/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * Intercepts log messages intended for the Android log device.
 * When running in the context of the simulator, the messages are
 * passed on to the underlying (fake) log device.  When not in the
 * simulator, messages are printed to stderr.
 */
#include <log/logd.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#ifdef HAVE_PTHREADS
#include <pthread.h>
#endif

#ifdef _MSC_VER
#include <io.h>
#include <process.h>
#if _MSC_VER < 1900
#include <nspr/prprf.h>
#define snprintf PR_snprintf
#endif

/* We don't want to indent large blocks because it causes unnecessary merge
 * conflicts */
#define UNINDENTED_BLOCK_START {
#define UNINDENTED_BLOCK_END }
#else
#define UNINDENTED_BLOCK_START
#define UNINDENTED_BLOCK_END
#endif

#ifdef _MSC_VER
#include <io.h>
#include <process.h>

/* We don't want to indent large blocks because it causes unnecessary merge
 * conflicts */
#define UNINDENTED_BLOCK_START {
#define UNINDENTED_BLOCK_END }
#else
#define UNINDENTED_BLOCK_START
#define UNINDENTED_BLOCK_END
#endif

#define kMaxTagLen  16      /* from the long-dead utils/Log.cpp */

#define kTagSetSize 16      /* arbitrary */

#if 0
#define TRACE(...) printf("fake_log_device: " __VA_ARGS__)
#else
#define TRACE(...) ((void)0)
#endif

/* from the long-dead utils/Log.cpp */
typedef enum {
    FORMAT_OFF = 0,
    FORMAT_BRIEF,
    FORMAT_PROCESS,
    FORMAT_TAG,
    FORMAT_THREAD,
    FORMAT_RAW,
    FORMAT_TIME,
    FORMAT_THREADTIME,
    FORMAT_LONG
} LogFormat;


/*
 * Log driver state.
 */
typedef struct LogState {
    /* the fake fd that's seen by the user */
    int     fakeFd;

    /* a printable name for this fake device */
    char   *debugName;

    /* nonzero if this is a binary log */
    int     isBinary;

    /* global minimum priority */
    int     globalMinPriority;

    /* output format */
    LogFormat outputFormat;

    /* tags and priorities */
    struct {
        char    tag[kMaxTagLen];
        int     minPriority;
    } tagSet[kTagSetSize];
} LogState;


#ifdef HAVE_PTHREADS
/*
 * Locking.  Since we're emulating a device, we need to be prepared
 * to have multiple callers at the same time.  This lock is used
 * to both protect the fd list and to prevent LogStates from being
 * freed out from under a user.
 */
static pthread_mutex_t fakeLogDeviceLock = PTHREAD_MUTEX_INITIALIZER;

static void lock()
{
    pthread_mutex_lock(&fakeLogDeviceLock);
}

static void unlock()
{
    pthread_mutex_unlock(&fakeLogDeviceLock);
}
#else   // !HAVE_PTHREADS
#define lock() ((void)0)
#define unlock() ((void)0)
#endif  // !HAVE_PTHREADS


/*
 * File descriptor management.
 */
#define FAKE_FD_BASE 10000
#define MAX_OPEN_LOGS 16
static LogState *openLogTable[MAX_OPEN_LOGS];

/*
 * Allocate an fd and associate a new LogState with it.
 * The fd is available via the fakeFd field of the return value.
 */
static LogState *createLogState()
{
    size_t i;

    for (i = 0; i < sizeof(openLogTable); i++) {
        if (openLogTable[i] == NULL) {
            openLogTable[i] = calloc(1, sizeof(LogState));
            openLogTable[i]->fakeFd = FAKE_FD_BASE + i;
            return openLogTable[i];
        }
    }
    return NULL;
}

/*
 * Translate an fd to a LogState.
 */
static LogState *fdToLogState(int fd)
{
    if (fd >= FAKE_FD_BASE && fd < FAKE_FD_BASE + MAX_OPEN_LOGS) {
        return openLogTable[fd - FAKE_FD_BASE];
    }
    return NULL;
}

/*
 * Unregister the fake fd and free the memory it pointed to.
 */
static void deleteFakeFd(int fd)
{
    LogState *ls;

    lock();

    ls = fdToLogState(fd);
    if (ls != NULL) {
        openLogTable[fd - FAKE_FD_BASE] = NULL;
        free(ls->debugName);
        free(ls);
    }

    unlock();
}

/*
 * Configure logging based on ANDROID_LOG_TAGS environment variable.  We
 * need to parse a string that looks like
 *
 *   *:v jdwp:d dalvikvm:d dalvikvm-gc:i dalvikvmi:i
 *
 * The tag (or '*' for the global level) comes first, followed by a colon
 * and a letter indicating the minimum priority level we're expected to log.
 * This can be used to reveal or conceal logs with specific tags.
 *
 * We also want to check ANDROID_PRINTF_LOG to determine how the output
 * will look.
 */
static void configureInitialState(const char* pathName, LogState* logState)
{
    static const int kDevLogLen = sizeof("/dev/log/") - 1;

    logState->debugName = strdup(pathName);

    /* identify binary logs */
    if (strcmp(pathName + kDevLogLen, "events") == 0) {
        logState->isBinary = 1;
    }

    /* global min priority defaults to "info" level */
    logState->globalMinPriority = ANDROID_LOG_INFO;

    /*
     * This is based on the the long-dead utils/Log.cpp code.
     */
    UNINDENTED_BLOCK_START
    const char* tags = getenv("ANDROID_LOG_TAGS");
    TRACE("Found ANDROID_LOG_TAGS='%s'\n", tags);
    if (tags != NULL) {
        int entry = 0;

        while (*tags != '\0') {
            char tagName[kMaxTagLen];
            int i, minPrio;

            while (isspace(*tags))
                tags++;

            i = 0;
            while (*tags != '\0' && !isspace(*tags) && *tags != ':' &&
                i < kMaxTagLen)
            {
                tagName[i++] = *tags++;
            }
            if (i == kMaxTagLen) {
                TRACE("ERROR: env tag too long (%d chars max)\n", kMaxTagLen-1);
                return;
            }
            tagName[i] = '\0';

            /* default priority, if there's no ":" part; also zero out '*' */
            minPrio = ANDROID_LOG_VERBOSE;
            if (tagName[0] == '*' && tagName[1] == '\0') {
                minPrio = ANDROID_LOG_DEBUG;
                tagName[0] = '\0';
            }

            if (*tags == ':') {
                tags++;
                if (*tags >= '0' && *tags <= '9') {
                    if (*tags >= ('0' + ANDROID_LOG_SILENT))
                        minPrio = ANDROID_LOG_VERBOSE;
                    else
                        minPrio = *tags - '\0';
                } else {
                    switch (*tags) {
                    case 'v':   minPrio = ANDROID_LOG_VERBOSE;  break;
                    case 'd':   minPrio = ANDROID_LOG_DEBUG;    break;
                    case 'i':   minPrio = ANDROID_LOG_INFO;     break;
                    case 'w':   minPrio = ANDROID_LOG_WARN;     break;
                    case 'e':   minPrio = ANDROID_LOG_ERROR;    break;
                    case 'f':   minPrio = ANDROID_LOG_FATAL;    break;
                    case 's':   minPrio = ANDROID_LOG_SILENT;   break;
                    default:    minPrio = ANDROID_LOG_DEFAULT;  break;
                    }
                }

                tags++;
                if (*tags != '\0' && !isspace(*tags)) {
                    TRACE("ERROR: garbage in tag env; expected whitespace\n");
                    TRACE("       env='%s'\n", tags);
                    return;
                }
            }

            if (tagName[0] == 0) {
                logState->globalMinPriority = minPrio;
                TRACE("+++ global min prio %d\n", logState->globalMinPriority);
            } else {
                logState->tagSet[entry].minPriority = minPrio;
                strcpy(logState->tagSet[entry].tag, tagName);
                TRACE("+++ entry %d: %s:%d\n",
                    entry,
                    logState->tagSet[entry].tag,
                    logState->tagSet[entry].minPriority);
                entry++;
            }
        }
    }
    UNINDENTED_BLOCK_END

    /*
     * Taken from the long-dead utils/Log.cpp
     */
    UNINDENTED_BLOCK_START
    const char* fstr = getenv("ANDROID_PRINTF_LOG");
    LogFormat format;
    if (fstr == NULL) {
        format = FORMAT_BRIEF;
    } else {
        if (strcmp(fstr, "brief") == 0)
            format = FORMAT_BRIEF;
        else if (strcmp(fstr, "process") == 0)
            format = FORMAT_PROCESS;
        else if (strcmp(fstr, "tag") == 0)
            format = FORMAT_PROCESS;
        else if (strcmp(fstr, "thread") == 0)
            format = FORMAT_PROCESS;
        else if (strcmp(fstr, "raw") == 0)
            format = FORMAT_PROCESS;
        else if (strcmp(fstr, "time") == 0)
            format = FORMAT_PROCESS;
        else if (strcmp(fstr, "long") == 0)
            format = FORMAT_PROCESS;
        else
            format = (LogFormat) atoi(fstr);        // really?!
    }

    logState->outputFormat = format;
    UNINDENTED_BLOCK_END
}

/*
 * Return a human-readable string for the priority level.  Always returns
 * a valid string.
 */
static const char* getPriorityString(int priority)
{
    /* the first character of each string should be unique */
    static const char* priorityStrings[] = {
        "Verbose", "Debug", "Info", "Warn", "Error", "Assert"
    };
    int idx;

    idx = (int) priority - (int) ANDROID_LOG_VERBOSE;
    if (idx < 0 ||
        idx >= (int) (sizeof(priorityStrings) / sizeof(priorityStrings[0])))
        return "?unknown?";
    return priorityStrings[idx];
}

#ifndef HAVE_WRITEV
/*
 * Some platforms like WIN32 do not have writev().
 * Make up something to replace it.
 */
static ssize_t fake_writev(int fd, const struct iovec *iov, int iovcnt) {
    int result = 0;
    const struct iovec* end = iov + iovcnt;
    for (; iov < end; iov++) {
        int w = write(fd, iov->iov_base, iov->iov_len);
        if (w != iov->iov_len) {
            if (w < 0)
                return w;
            return result + w;
        }
        result += w;
    }
    return result;
}

#define writev fake_writev
#endif


/*
 * Write a filtered log message to stderr.
 *
 * Log format parsing taken from the long-dead utils/Log.cpp.
 */
static void showLog(LogState *state,
        int logPrio, const char* tag, const char* msg)
{
#if defined(HAVE_LOCALTIME_R)
    struct tm tmBuf;
#endif
    struct tm* ptm;
    char timeBuf[32];
    char prefixBuf[128], suffixBuf[128];
    char priChar;
    time_t when;
#ifdef _MSC_VER
    int pid, tid;
#else
    pid_t pid, tid;
#endif

    TRACE("LOG %d: %s %s", logPrio, tag, msg);

    priChar = getPriorityString(logPrio)[0];
    when = time(NULL);
    pid = tid = getpid();       // find gettid()?

    /*
     * Get the current date/time in pretty form
     *
     * It's often useful when examining a log with "less" to jump to
     * a specific point in the file by searching for the date/time stamp.
     * For this reason it's very annoying to have regexp meta characters
     * in the time stamp.  Don't use forward slashes, parenthesis,
     * brackets, asterisks, or other special chars here.
     */
#if defined(HAVE_LOCALTIME_R)
    ptm = localtime_r(&when, &tmBuf);
#else
    ptm = localtime(&when);
#endif
    //strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", ptm);
    strftime(timeBuf, sizeof(timeBuf), "%m-%d %H:%M:%S", ptm);

    /*
     * Construct a buffer containing the log header and log message.
     */
    UNINDENTED_BLOCK_START
    size_t prefixLen, suffixLen;

    switch (state->outputFormat) {
    case FORMAT_TAG:
        prefixLen = snprintf(prefixBuf, sizeof(prefixBuf),
            "%c/%-8s: ", priChar, tag);
        strcpy(suffixBuf, "\n"); suffixLen = 1;
        break;
    case FORMAT_PROCESS:
        prefixLen = snprintf(prefixBuf, sizeof(prefixBuf),
            "%c(%5d) ", priChar, pid);
        suffixLen = snprintf(suffixBuf, sizeof(suffixBuf),
            "  (%s)\n", tag);
        break;
    case FORMAT_THREAD:
        prefixLen = snprintf(prefixBuf, sizeof(prefixBuf),
            "%c(%5d:%5d) ", priChar, pid, tid);
        strcpy(suffixBuf, "\n"); suffixLen = 1;
        break;
    case FORMAT_RAW:
        prefixBuf[0] = 0; prefixLen = 0;
        strcpy(suffixBuf, "\n"); suffixLen = 1;
        break;
    case FORMAT_TIME:
        prefixLen = snprintf(prefixBuf, sizeof(prefixBuf),
            "%s %-8s\n\t", timeBuf, tag);
        strcpy(suffixBuf, "\n"); suffixLen = 1;
        break;
    case FORMAT_THREADTIME:
        prefixLen = snprintf(prefixBuf, sizeof(prefixBuf),
            "%s %5d %5d %c %-8s \n\t", timeBuf, pid, tid, priChar, tag);
        strcpy(suffixBuf, "\n"); suffixLen = 1;
        break;
    case FORMAT_LONG:
        prefixLen = snprintf(prefixBuf, sizeof(prefixBuf),
            "[ %s %5d:%5d %c/%-8s ]\n",
            timeBuf, pid, tid, priChar, tag);
        strcpy(suffixBuf, "\n\n"); suffixLen = 2;
        break;
    default:
        prefixLen = snprintf(prefixBuf, sizeof(prefixBuf),
            "%c/%-8s(%5d): ", priChar, tag, pid);
        strcpy(suffixBuf, "\n"); suffixLen = 1;
        break;
     }

    /*
     * Figure out how many lines there will be.
     */
    UNINDENTED_BLOCK_START
    const char* end = msg + strlen(msg);
    size_t numLines = 0;
    const char* p = msg;
    while (p < end) {
        if (*p++ == '\n') numLines++;
    }
    if (p > msg && *(p-1) != '\n') numLines++;

    /*
     * Create an array of iovecs large enough to write all of
     * the lines with a prefix and a suffix.
     */
    UNINDENTED_BLOCK_START
    #define INLINE_VECS 6
    const size_t MAX_LINES   = ((size_t)~0)/(3*sizeof(struct iovec*));
    struct iovec stackVec[INLINE_VECS];
    struct iovec* vec = stackVec;
    size_t numVecs;

    if (numLines > MAX_LINES)
        numLines = MAX_LINES;

    numVecs = numLines*3;  // 3 iovecs per line.
    if (numVecs > INLINE_VECS) {
        vec = (struct iovec*)malloc(sizeof(struct iovec)*numVecs);
        if (vec == NULL) {
            msg = "LOG: write failed, no memory";
            numVecs = 3;
            numLines = 1;
            vec = stackVec;
        }
    }

    /*
     * Fill in the iovec pointers.
     */
    p = msg;
    UNINDENTED_BLOCK_START
    struct iovec* v = vec;
    int totalLen = 0;
    while (numLines > 0 && p < end) {
        if (prefixLen > 0) {
            v->iov_base = prefixBuf;
            v->iov_len = prefixLen;
            totalLen += prefixLen;
            v++;
        }
        UNINDENTED_BLOCK_START
        const char* start = p;
        while (p < end && *p != '\n') p++;
        if ((p-start) > 0) {
            v->iov_base = (void*)start;
            v->iov_len = p-start;
            totalLen += p-start;
            v++;
        }
        if (*p == '\n') p++;
        if (suffixLen > 0) {
            v->iov_base = suffixBuf;
            v->iov_len = suffixLen;
            totalLen += suffixLen;
            v++;
        }
        numLines -= 1;
        UNINDENTED_BLOCK_END
    }
    
    /*
     * Write the entire message to the log file with a single writev() call.
     * We need to use this rather than a collection of printf()s on a FILE*
     * because of multi-threading and multi-process issues.
     *
     * If the file was not opened with O_APPEND, this will produce interleaved
     * output when called on the same file from multiple processes.
     *
     * If the file descriptor is actually a network socket, the writev()
     * call may return with a partial write.  Putting the writev() call in
     * a loop can result in interleaved data.  This can be alleviated
     * somewhat by wrapping the writev call in the Mutex.
     */

    for(;;) {
        int cc = writev(fileno(stderr), vec, v-vec);

        if (cc == totalLen) break;
        
        if (cc < 0) {
            if(errno == EINTR) continue;
            
                /* can't really log the failure; for now, throw out a stderr */
            fprintf(stderr, "+++ LOG: write failed (errno=%d)\n", errno);
            break;
        } else {
                /* shouldn't happen when writing to file or tty */
            fprintf(stderr, "+++ LOG: write partial (%d of %d)\n", cc, totalLen);
            break;
        }
    }

    /* if we allocated storage for the iovecs, free it */
    if (vec != stackVec)
        free(vec);
    UNINDENTED_BLOCK_END
    UNINDENTED_BLOCK_END
    UNINDENTED_BLOCK_END
    UNINDENTED_BLOCK_END
}


/*
 * Receive a log message.  We happen to know that "vector" has three parts:
 *
 *  priority (1 byte)
 *  tag (N bytes -- null-terminated ASCII string)
 *  message (N bytes -- null-terminated ASCII string)
 */
static ssize_t logWritev(int fd, const struct iovec* vector, int count)
{
    LogState* state;

    /* Make sure that no-one frees the LogState while we're using it.
     * Also guarantees that only one thread is in showLog() at a given
     * time (if it matters).
     */
    lock();

    state = fdToLogState(fd);
    if (state == NULL) {
        errno = EBADF;
        goto error;
    }

    if (state->isBinary) {
        TRACE("%s: ignoring binary log\n", state->debugName);
        goto bail;
    }

    if (count != 3) {
        TRACE("%s: writevLog with count=%d not expected\n",
            state->debugName, count);
        goto error;
    }

    /* pull out the three fields */
    UNINDENTED_BLOCK_START
    int logPrio = *(const char*)vector[0].iov_base;
    const char* tag = (const char*) vector[1].iov_base;
    const char* msg = (const char*) vector[2].iov_base;

    /* see if this log tag is configured */
    int i;
    int minPrio = state->globalMinPriority;
    for (i = 0; i < kTagSetSize; i++) {
        if (state->tagSet[i].minPriority == ANDROID_LOG_UNKNOWN)
            break;      /* reached end of configured values */

        if (strcmp(state->tagSet[i].tag, tag) == 0) {
            //TRACE("MATCH tag '%s'\n", tag);
            minPrio = state->tagSet[i].minPriority;
            break;
        }
    }

    if (logPrio >= minPrio) {
        showLog(state, logPrio, tag, msg);
    } else {
        //TRACE("+++ NOLOG(%d): %s %s", logPrio, tag, msg);
    }
    UNINDENTED_BLOCK_END

bail:
    unlock();
    return vector[0].iov_len + vector[1].iov_len + vector[2].iov_len;
error:
    unlock();
    return -1;
}

/*
 * Free up our state and close the fake descriptor.
 */
static int logClose(int fd)
{
    deleteFakeFd(fd);
    return 0;
}

/*
 * Open a log output device and return a fake fd.
 */
static int logOpen(const char* pathName, int flags)
{
    LogState *logState;
    int fd = -1;

    lock();

    logState = createLogState();
    if (logState != NULL) {
        configureInitialState(pathName, logState);
        fd = logState->fakeFd;
    } else  {
        errno = ENFILE;
    }

    unlock();

    return fd;
}


/*
 * Runtime redirection.  If this binary is running in the simulator,
 * just pass log messages to the emulated device.  If it's running
 * outside of the simulator, write the log messages to stderr.
 */

static int (*redirectOpen)(const char *pathName, int flags) = NULL;
static int (*redirectClose)(int fd) = NULL;
static ssize_t (*redirectWritev)(int fd, const struct iovec* vector, int count)
        = NULL;

static void setRedirects()
{
    const char *ws;

    /* Wrapsim sets this environment variable on children that it's
     * created using its LD_PRELOAD wrapper.
     */
    ws = getenv("ANDROID_WRAPSIM");
    if (ws != NULL && strcmp(ws, "1") == 0) {
        /* We're running inside wrapsim, so we can just write to the device. */
        redirectOpen = (int (*)(const char *pathName, int flags))open;
        redirectClose = close;
        redirectWritev = writev;
    } else {
        /* There's no device to delegate to; handle the logging ourselves. */
        redirectOpen = logOpen;
        redirectClose = logClose;
        redirectWritev = logWritev;
    }
}

int fakeLogOpen(const char *pathName, int flags)
{
    if (redirectOpen == NULL) {
        setRedirects();
    }
    return redirectOpen(pathName, flags);
}

int fakeLogClose(int fd)
{
    /* Assume that open() was called first. */
    return redirectClose(fd);
}

ssize_t fakeLogWritev(int fd, const struct iovec* vector, int count)
{
    /* Assume that open() was called first. */
    return redirectWritev(fd, vector, count);
}

#undef UNINDENTED_BLOCK_START
#undef UNINDENTED_BLOCK_END
