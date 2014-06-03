/* //device/libs/cutils/logprint.c
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#define _GNU_SOURCE /* for asprintf */

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>

#include <log/logd.h>
#include <log/logprint.h>

#ifdef _MSC_VER
#include <nspr/prprf.h>
#define snprintf PR_snprintf
#define inline
/* We don't want to indent large blocks because it causes unnecessary merge
 * conflicts */
#define UNINDENTED_BLOCK_START {
#define UNINDENTED_BLOCK_END }
#else
#define UNINDENTED_BLOCK_START
#define UNINDENTED_BLOCK_END
#endif

#ifdef WIN32
static char *
strsep(char **stringp, const char *delim)
{
    char* res = *stringp;
    while (**stringp) {
        const char *c;
        for (c = delim; *c; c++) {
            if (**stringp == *c) {
                **stringp++ = 0;
                return res;
            }
        }
    }
    return res;
}
#endif

typedef struct FilterInfo_t {
    char *mTag;
    android_LogPriority mPri;
    struct FilterInfo_t *p_next;
} FilterInfo;

struct AndroidLogFormat_t {
    android_LogPriority global_pri;
    FilterInfo *filters;
    AndroidLogPrintFormat format;
};

static FilterInfo * filterinfo_new(const char * tag, android_LogPriority pri)
{
    FilterInfo *p_ret;

    p_ret = (FilterInfo *)calloc(1, sizeof(FilterInfo));
    p_ret->mTag = strdup(tag);
    p_ret->mPri = pri;

    return p_ret;
}

static void filterinfo_free(FilterInfo *p_info)
{
    if (p_info == NULL) {
        return;
    }

    free(p_info->mTag);
    p_info->mTag = NULL;
}

/*
 * Note: also accepts 0-9 priorities
 * returns ANDROID_LOG_UNKNOWN if the character is unrecognized
 */
static android_LogPriority filterCharToPri (char c)
{
    android_LogPriority pri;

    c = tolower(c);

    if (c >= '0' && c <= '9') {
        if (c >= ('0'+ANDROID_LOG_SILENT)) {
            pri = ANDROID_LOG_VERBOSE;
        } else {
            pri = (android_LogPriority)(c - '0');
        }
    } else if (c == 'v') {
        pri = ANDROID_LOG_VERBOSE;
    } else if (c == 'd') {
        pri = ANDROID_LOG_DEBUG;
    } else if (c == 'i') {
        pri = ANDROID_LOG_INFO;
    } else if (c == 'w') {
        pri = ANDROID_LOG_WARN;
    } else if (c == 'e') {
        pri = ANDROID_LOG_ERROR;
    } else if (c == 'f') {
        pri = ANDROID_LOG_FATAL;
    } else if (c == 's') {
        pri = ANDROID_LOG_SILENT;
    } else if (c == '*') {
        pri = ANDROID_LOG_DEFAULT;
    } else {
        pri = ANDROID_LOG_UNKNOWN;
    }

    return pri;
}

static char filterPriToChar (android_LogPriority pri)
{
    switch (pri) {
        case ANDROID_LOG_VERBOSE:       return 'V';
        case ANDROID_LOG_DEBUG:         return 'D';
        case ANDROID_LOG_INFO:          return 'I';
        case ANDROID_LOG_WARN:          return 'W';
        case ANDROID_LOG_ERROR:         return 'E';
        case ANDROID_LOG_FATAL:         return 'F';
        case ANDROID_LOG_SILENT:        return 'S';

        case ANDROID_LOG_DEFAULT:
        case ANDROID_LOG_UNKNOWN:
        default:                        return '?';
    }
}

static android_LogPriority filterPriForTag(
        AndroidLogFormat *p_format, const char *tag)
{
    FilterInfo *p_curFilter;

    for (p_curFilter = p_format->filters
            ; p_curFilter != NULL
            ; p_curFilter = p_curFilter->p_next
    ) {
        if (0 == strcmp(tag, p_curFilter->mTag)) {
            if (p_curFilter->mPri == ANDROID_LOG_DEFAULT) {
                return p_format->global_pri;
            } else {
                return p_curFilter->mPri;
            }
        }
    }

    return p_format->global_pri;
}

/** for debugging */
static void dumpFilters(AndroidLogFormat *p_format)
{
    FilterInfo *p_fi;

    for (p_fi = p_format->filters ; p_fi != NULL ; p_fi = p_fi->p_next) {
        char cPri = filterPriToChar(p_fi->mPri);
        if (p_fi->mPri == ANDROID_LOG_DEFAULT) {
            cPri = filterPriToChar(p_format->global_pri);
        }
        fprintf(stderr,"%s:%c\n", p_fi->mTag, cPri);
    }

    fprintf(stderr,"*:%c\n", filterPriToChar(p_format->global_pri));

}

/**
 * returns 1 if this log line should be printed based on its priority
 * and tag, and 0 if it should not
 */
int android_log_shouldPrintLine (
        AndroidLogFormat *p_format, const char *tag, android_LogPriority pri)
{
    return pri >= filterPriForTag(p_format, tag);
}

AndroidLogFormat *android_log_format_new()
{
    AndroidLogFormat *p_ret;

    p_ret = calloc(1, sizeof(AndroidLogFormat));

    p_ret->global_pri = ANDROID_LOG_VERBOSE;
    p_ret->format = FORMAT_BRIEF;

    return p_ret;
}

void android_log_format_free(AndroidLogFormat *p_format)
{
    FilterInfo *p_info, *p_info_old;

    p_info = p_format->filters;

    while (p_info != NULL) {
        p_info_old = p_info;
        p_info = p_info->p_next;

        free(p_info_old);
    }

    free(p_format);
}



void android_log_setPrintFormat(AndroidLogFormat *p_format,
        AndroidLogPrintFormat format)
{
    p_format->format=format;
}

/**
 * Returns FORMAT_OFF on invalid string
 */
AndroidLogPrintFormat android_log_formatFromString(const char * formatString)
{
    static AndroidLogPrintFormat format;

    if (strcmp(formatString, "brief") == 0) format = FORMAT_BRIEF;
    else if (strcmp(formatString, "process") == 0) format = FORMAT_PROCESS;
    else if (strcmp(formatString, "tag") == 0) format = FORMAT_TAG;
    else if (strcmp(formatString, "thread") == 0) format = FORMAT_THREAD;
    else if (strcmp(formatString, "raw") == 0) format = FORMAT_RAW;
    else if (strcmp(formatString, "time") == 0) format = FORMAT_TIME;
    else if (strcmp(formatString, "threadtime") == 0) format = FORMAT_THREADTIME;
    else if (strcmp(formatString, "long") == 0) format = FORMAT_LONG;
    else format = FORMAT_OFF;

    return format;
}

/**
 * filterExpression: a single filter expression
 * eg "AT:d"
 *
 * returns 0 on success and -1 on invalid expression
 *
 * Assumes single threaded execution
 */

int android_log_addFilterRule(AndroidLogFormat *p_format,
        const char *filterExpression)
{
    size_t i=0;
    size_t tagNameLength;
    android_LogPriority pri = ANDROID_LOG_DEFAULT;

    tagNameLength = strcspn(filterExpression, ":");

    if (tagNameLength == 0) {
        goto error;
    }

    if(filterExpression[tagNameLength] == ':') {
        pri = filterCharToPri(filterExpression[tagNameLength+1]);

        if (pri == ANDROID_LOG_UNKNOWN) {
            goto error;
        }
    }

    if(0 == strncmp("*", filterExpression, tagNameLength)) {
        // This filter expression refers to the global filter
        // The default level for this is DEBUG if the priority
        // is unspecified
        if (pri == ANDROID_LOG_DEFAULT) {
            pri = ANDROID_LOG_DEBUG;
        }

        p_format->global_pri = pri;
    } else {
        // for filter expressions that don't refer to the global
        // filter, the default is verbose if the priority is unspecified
        if (pri == ANDROID_LOG_DEFAULT) {
            pri = ANDROID_LOG_VERBOSE;
        }

        UNINDENTED_BLOCK_START
        char *tagName;

// Presently HAVE_STRNDUP is never defined, so the second case is always taken
// Darwin doesn't have strnup, everything else does
#ifdef HAVE_STRNDUP
        tagName = strndup(filterExpression, tagNameLength);
#else
        //a few extra bytes copied...
        tagName = strdup(filterExpression);
        tagName[tagNameLength] = '\0';
#endif /*HAVE_STRNDUP*/

        UNINDENTED_BLOCK_START
        FilterInfo *p_fi = filterinfo_new(tagName, pri);
        free(tagName);

        p_fi->p_next = p_format->filters;
        p_format->filters = p_fi;
        UNINDENTED_BLOCK_END
        UNINDENTED_BLOCK_END
    }

    return 0;
error:
    return -1;
}


/**
 * filterString: a comma/whitespace-separated set of filter expressions
 *
 * eg "AT:d *:i"
 *
 * returns 0 on success and -1 on invalid expression
 *
 * Assumes single threaded execution
 *
 */

int android_log_addFilterString(AndroidLogFormat *p_format,
        const char *filterString)
{
    char *filterStringCopy = strdup (filterString);
    char *p_cur = filterStringCopy;
    char *p_ret;
    int err;

    // Yes, I'm using strsep
    while (NULL != (p_ret = strsep(&p_cur, " \t,"))) {
        // ignore whitespace-only entries
        if(p_ret[0] != '\0') {
            err = android_log_addFilterRule(p_format, p_ret);

            if (err < 0) {
                goto error;
            }
        }
    }

    free (filterStringCopy);
    return 0;
error:
    free (filterStringCopy);
    return -1;
}

static inline char * strip_end(char *str)
{
    char *end = str + strlen(str) - 1;

    while (end >= str && isspace(*end))
        *end-- = '\0';
    return str;
}

/**
 * Splits a wire-format buffer into an AndroidLogEntry
 * entry allocated by caller. Pointers will point directly into buf
 *
 * Returns 0 on success and -1 on invalid wire format (entry will be
 * in unspecified state)
 */
int android_log_processLogBuffer(struct logger_entry *buf,
                                 AndroidLogEntry *entry)
{
    entry->tv_sec = buf->sec;
    entry->tv_nsec = buf->nsec;
    entry->pid = buf->pid;
    entry->tid = buf->tid;

    /*
     * format: <priority:1><tag:N>\0<message:N>\0
     *
     * tag str
     *   starts at buf->msg+1
     * msg
     *   starts at buf->msg+1+len(tag)+1
     *
     * The message may have been truncated by the kernel log driver.
     * When that happens, we must null-terminate the message ourselves.
     */
    if (buf->len < 3) {
        // An well-formed entry must consist of at least a priority
        // and two null characters
        fprintf(stderr, "+++ LOG: entry too small\n");
        return -1;
    }

    UNINDENTED_BLOCK_START
    int msgStart = -1;
    int msgEnd = -1;

    int i;
    for (i = 1; i < buf->len; i++) {
        if (buf->msg[i] == '\0') {
            if (msgStart == -1) {
                msgStart = i + 1;
            } else {
                msgEnd = i;
                break;
            }
        }
    }

    if (msgStart == -1) {
        fprintf(stderr, "+++ LOG: malformed log message\n");
        return -1;
    }
    if (msgEnd == -1) {
        // incoming message not null-terminated; force it
        msgEnd = buf->len - 1;
        buf->msg[msgEnd] = '\0';
    }

    entry->priority = buf->msg[0];
    entry->tag = buf->msg + 1;
    entry->message = buf->msg + msgStart;
    entry->messageLen = msgEnd - msgStart;

    return 0;
    UNINDENTED_BLOCK_END
}

/*
 * Extract a 4-byte value from a byte stream.
 */
static inline uint32_t get4LE(const uint8_t* src)
{
    return src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
}

/*
 * Extract an 8-byte value from a byte stream.
 */
static inline uint64_t get8LE(const uint8_t* src)
{
    uint32_t low, high;

    low = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
    high = src[4] | (src[5] << 8) | (src[6] << 16) | (src[7] << 24);
    return ((long long) high << 32) | (long long) low;
}


/*
 * Recursively convert binary log data to printable form.
 *
 * This needs to be recursive because you can have lists of lists.
 *
 * If we run out of room, we stop processing immediately.  It's important
 * for us to check for space on every output element to avoid producing
 * garbled output.
 *
 * Returns 0 on success, 1 on buffer full, -1 on failure.
 */
static int android_log_printBinaryEvent(const unsigned char** pEventData,
    size_t* pEventDataLen, char** pOutBuf, size_t* pOutBufLen)
{
    const unsigned char* eventData = *pEventData;
    size_t eventDataLen = *pEventDataLen;
    char* outBuf = *pOutBuf;
    size_t outBufLen = *pOutBufLen;
    unsigned char type;
    size_t outCount;
    int result = 0;

    if (eventDataLen < 1)
        return -1;
    type = *eventData++;
    eventDataLen--;

    //fprintf(stderr, "--- type=%d (rem len=%d)\n", type, eventDataLen);

    switch (type) {
    case EVENT_TYPE_INT:
        /* 32-bit signed int */
        {
            int ival;

            if (eventDataLen < 4)
                return -1;
            ival = get4LE(eventData);
            eventData += 4;
            eventDataLen -= 4;

            outCount = snprintf(outBuf, outBufLen, "%d", ival);
            if (outCount < outBufLen) {
                outBuf += outCount;
                outBufLen -= outCount;
            } else {
                /* halt output */
                goto no_room;
            }
        }
        break;
    case EVENT_TYPE_LONG:
        /* 64-bit signed long */
        {
            long long lval;

            if (eventDataLen < 8)
                return -1;
            lval = get8LE(eventData);
            eventData += 8;
            eventDataLen -= 8;

            outCount = snprintf(outBuf, outBufLen, "%lld", lval);
            if (outCount < outBufLen) {
                outBuf += outCount;
                outBufLen -= outCount;
            } else {
                /* halt output */
                goto no_room;
            }
        }
        break;
    case EVENT_TYPE_STRING:
        /* UTF-8 chars, not NULL-terminated */
        {
            unsigned int strLen;

            if (eventDataLen < 4)
                return -1;
            strLen = get4LE(eventData);
            eventData += 4;
            eventDataLen -= 4;

            if (eventDataLen < strLen)
                return -1;

            if (strLen < outBufLen) {
                memcpy(outBuf, eventData, strLen);
                outBuf += strLen;
                outBufLen -= strLen;
            } else if (outBufLen > 0) {
                /* copy what we can */
                memcpy(outBuf, eventData, outBufLen);
                outBuf += outBufLen;
                outBufLen -= outBufLen;
                goto no_room;
            }
            eventData += strLen;
            eventDataLen -= strLen;
            break;
        }
    case EVENT_TYPE_LIST:
        /* N items, all different types */
        {
            unsigned char count;
            int i;

            if (eventDataLen < 1)
                return -1;

            count = *eventData++;
            eventDataLen--;

            if (outBufLen > 0) {
                *outBuf++ = '[';
                outBufLen--;
            } else {
                goto no_room;
            }

            for (i = 0; i < count; i++) {
                result = android_log_printBinaryEvent(&eventData, &eventDataLen,
                        &outBuf, &outBufLen);
                if (result != 0)
                    goto bail;

                if (i < count-1) {
                    if (outBufLen > 0) {
                        *outBuf++ = ',';
                        outBufLen--;
                    } else {
                        goto no_room;
                    }
                }
            }

            if (outBufLen > 0) {
                *outBuf++ = ']';
                outBufLen--;
            } else {
                goto no_room;
            }
        }
        break;
    default:
        fprintf(stderr, "Unknown binary event type %d\n", type);
        return -1;
    }

bail:
    *pEventData = eventData;
    *pEventDataLen = eventDataLen;
    *pOutBuf = outBuf;
    *pOutBufLen = outBufLen;
    return result;

no_room:
    result = 1;
    goto bail;
}

/**
 * Convert a binary log entry to ASCII form.
 *
 * For convenience we mimic the processLogBuffer API.  There is no
 * pre-defined output length for the binary data, since we're free to format
 * it however we choose, which means we can't really use a fixed-size buffer
 * here.
 */
int android_log_processBinaryLogBuffer(struct logger_entry *buf,
    AndroidLogEntry *entry, const EventTagMap* map, char* messageBuf,
    int messageBufLen)
{
    size_t inCount;
    unsigned int tagIndex;
    const unsigned char* eventData;

    entry->tv_sec = buf->sec;
    entry->tv_nsec = buf->nsec;
    entry->priority = ANDROID_LOG_INFO;
    entry->pid = buf->pid;
    entry->tid = buf->tid;

    /*
     * Pull the tag out.
     */
    eventData = (const unsigned char*) buf->msg;
    inCount = buf->len;
    if (inCount < 4)
        return -1;
    tagIndex = get4LE(eventData);
    eventData += 4;
    inCount -= 4;

    entry->tag = NULL;

    /*
     * If we don't have a map, or didn't find the tag number in the map,
     * stuff a generated tag value into the start of the output buffer and
     * shift the buffer pointers down.
     */
    if (entry->tag == NULL) {
        int tagLen;

        tagLen = snprintf(messageBuf, messageBufLen, "[%d]", tagIndex);
        entry->tag = messageBuf;
        messageBuf += tagLen+1;
        messageBufLen -= tagLen+1;
    }

    /*
     * Format the event log data into the buffer.
     */
    UNINDENTED_BLOCK_START
    char* outBuf = messageBuf;
    size_t outRemaining = messageBufLen-1;      /* leave one for nul byte */
    int result;
    result = android_log_printBinaryEvent(&eventData, &inCount, &outBuf,
                &outRemaining);
    if (result < 0) {
        fprintf(stderr, "Binary log entry conversion failed\n");
        return -1;
    } else if (result == 1) {
        if (outBuf > messageBuf) {
            /* leave an indicator */
            *(outBuf-1) = '!';
        } else {
            /* no room to output anything at all */
            *outBuf++ = '!';
            outRemaining--;
        }
        /* pretend we ate all the data */
        inCount = 0;
    }

    /* eat the silly terminating '\n' */
    if (inCount == 1 && *eventData == '\n') {
        eventData++;
        inCount--;
    }

    if (inCount != 0) {
        fprintf(stderr,
            "Warning: leftover binary log data (%zu bytes)\n", inCount);
    }

    /*
     * Terminate the buffer.  The NUL byte does not count as part of
     * entry->messageLen.
     */
    *outBuf = '\0';
    entry->messageLen = outBuf - messageBuf;
    assert(entry->messageLen == (messageBufLen-1) - outRemaining);

    entry->message = messageBuf;

    return 0;
    UNINDENTED_BLOCK_END
}

/**
 * Formats a log message into a buffer
 *
 * Uses defaultBuffer if it can, otherwise malloc()'s a new buffer
 * If return value != defaultBuffer, caller must call free()
 * Returns NULL on malloc error
 */

char *android_log_formatLogLine (
    AndroidLogFormat *p_format,
    char *defaultBuffer,
    size_t defaultBufferSize,
    const AndroidLogEntry *entry,
    size_t *p_outLength)
{
#if defined(HAVE_LOCALTIME_R)
    struct tm tmBuf;
#endif
    struct tm* ptm;
    char timeBuf[32];
    char headerBuf[128];
    char prefixBuf[128], suffixBuf[128];
    char priChar;
    int prefixSuffixIsHeaderFooter = 0;
    char * ret = NULL;

    priChar = filterPriToChar(entry->priority);

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
    ptm = localtime_r(&(entry->tv_sec), &tmBuf);
#else
    ptm = localtime(&(entry->tv_sec));
#endif
    //strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", ptm);
    strftime(timeBuf, sizeof(timeBuf), "%m-%d %H:%M:%S", ptm);

    /*
     * Construct a buffer containing the log header and log message.
     */
    UNINDENTED_BLOCK_START
    size_t prefixLen, suffixLen;

    switch (p_format->format) {
        case FORMAT_TAG:
            prefixLen = snprintf(prefixBuf, sizeof(prefixBuf),
                "%c/%-8s: ", priChar, entry->tag);
            strcpy(suffixBuf, "\n"); suffixLen = 1;
            break;
        case FORMAT_PROCESS:
            prefixLen = snprintf(prefixBuf, sizeof(prefixBuf),
                "%c(%5d) ", priChar, entry->pid);
            suffixLen = snprintf(suffixBuf, sizeof(suffixBuf),
                "  (%s)\n", entry->tag);
            break;
        case FORMAT_THREAD:
            prefixLen = snprintf(prefixBuf, sizeof(prefixBuf),
                "%c(%5d:%5d) ", priChar, entry->pid, entry->tid);
            strcpy(suffixBuf, "\n");
            suffixLen = 1;
            break;
        case FORMAT_RAW:
            prefixBuf[0] = 0;
            prefixLen = 0;
            strcpy(suffixBuf, "\n");
            suffixLen = 1;
            break;
        case FORMAT_TIME:
            prefixLen = snprintf(prefixBuf, sizeof(prefixBuf),
                "%s.%03ld %c/%-8s(%5d): ", timeBuf, entry->tv_nsec / 1000000,
                priChar, entry->tag, entry->pid);
            strcpy(suffixBuf, "\n");
            suffixLen = 1;
            break;
        case FORMAT_THREADTIME:
            prefixLen = snprintf(prefixBuf, sizeof(prefixBuf),
                "%s.%03ld %5d %5d %c %-8s: ", timeBuf, entry->tv_nsec / 1000000,
                entry->pid, entry->tid, priChar, entry->tag);
            strcpy(suffixBuf, "\n");
            suffixLen = 1;
            break;
        case FORMAT_LONG:
            prefixLen = snprintf(prefixBuf, sizeof(prefixBuf),
                "[ %s.%03ld %5d:%5d %c/%-8s ]\n",
                timeBuf, entry->tv_nsec / 1000000, entry->pid,
                entry->tid, priChar, entry->tag);
            strcpy(suffixBuf, "\n\n");
            suffixLen = 2;
            prefixSuffixIsHeaderFooter = 1;
            break;
        case FORMAT_BRIEF:
        default:
            prefixLen = snprintf(prefixBuf, sizeof(prefixBuf),
                "%c/%-8s(%5d): ", priChar, entry->tag, entry->pid);
            strcpy(suffixBuf, "\n");
            suffixLen = 1;
            break;
    }
    /* snprintf has a weird return value.   It returns what would have been
     * written given a large enough buffer.  In the case that the prefix is
     * longer then our buffer(128), it messes up the calculations below
     * possibly causing heap corruption.  To avoid this we double check and
     * set the length at the maximum (size minus null byte)
     */
    if(prefixLen >= sizeof(prefixBuf))
        prefixLen = sizeof(prefixBuf) - 1;
    if(suffixLen >= sizeof(suffixBuf))
        suffixLen = sizeof(suffixBuf) - 1;

    /* the following code is tragically unreadable */

    UNINDENTED_BLOCK_START
    size_t numLines;
    size_t i;
    char *p;
    size_t bufferSize;
    const char *pm;

    if (prefixSuffixIsHeaderFooter) {
        // we're just wrapping message with a header/footer
        numLines = 1;
    } else {
        pm = entry->message;
        numLines = 0;

        // The line-end finding here must match the line-end finding
        // in for ( ... numLines...) loop below
        while (pm < (entry->message + entry->messageLen)) {
            if (*pm++ == '\n') numLines++;
        }
        // plus one line for anything not newline-terminated at the end
        if (pm > entry->message && *(pm-1) != '\n') numLines++;
    }

    // this is an upper bound--newlines in message may be counted
    // extraneously
    bufferSize = (numLines * (prefixLen + suffixLen)) + entry->messageLen + 1;

    if (defaultBufferSize >= bufferSize) {
        ret = defaultBuffer;
    } else {
        ret = (char *)malloc(bufferSize);

        if (ret == NULL) {
            return ret;
        }
    }

    ret[0] = '\0';       /* to start strcat off */

    p = ret;
    pm = entry->message;

    if (prefixSuffixIsHeaderFooter) {
        strcat(p, prefixBuf);
        p += prefixLen;
        strncat(p, entry->message, entry->messageLen);
        p += entry->messageLen;
        strcat(p, suffixBuf);
        p += suffixLen;
    } else {
        while(pm < (entry->message + entry->messageLen)) {
            const char *lineStart;
            size_t lineLen;
            lineStart = pm;

            // Find the next end-of-line in message
            while (pm < (entry->message + entry->messageLen)
                    && *pm != '\n') pm++;
            lineLen = pm - lineStart;

            strcat(p, prefixBuf);
            p += prefixLen;
            strncat(p, lineStart, lineLen);
            p += lineLen;
            strcat(p, suffixBuf);
            p += suffixLen;

            if (*pm == '\n') pm++;
        }
    }

    if (p_outLength != NULL) {
        *p_outLength = p - ret;
    }

    return ret;
    UNINDENTED_BLOCK_END
    UNINDENTED_BLOCK_END
}

/**
 * Either print or do not print log line, based on filter
 *
 * Returns count bytes written
 */

int android_log_printLogLine(
    AndroidLogFormat *p_format,
    int fd,
    const AndroidLogEntry *entry)
{
    int ret;
    char defaultBuffer[512];
    char *outBuffer = NULL;
    size_t totalLen;

    outBuffer = android_log_formatLogLine(p_format, defaultBuffer,
            sizeof(defaultBuffer), entry, &totalLen);

    if (!outBuffer)
        return -1;

    do {
        ret = write(fd, outBuffer, totalLen);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0) {
        fprintf(stderr, "+++ LOG: write failed (errno=%d)\n", errno);
        ret = 0;
        goto done;
    }

    if (((size_t)ret) < totalLen) {
        fprintf(stderr, "+++ LOG: write partial (%d of %d)\n", ret,
                (int)totalLen);
        goto done;
    }

done:
    if (outBuffer != defaultBuffer) {
        free(outBuffer);
    }

    return ret;
}



void logprint_run_tests()
{
#if 0

    fprintf(stderr, "tests disabled\n");

#else

    int err;
    const char *tag;
    AndroidLogFormat *p_format;

    p_format = android_log_format_new();

    fprintf(stderr, "running tests\n");

    tag = "random";

    android_log_addFilterRule(p_format,"*:i");

    assert (ANDROID_LOG_INFO == filterPriForTag(p_format, "random"));
    assert(android_log_shouldPrintLine(p_format, tag, ANDROID_LOG_DEBUG) == 0);
    android_log_addFilterRule(p_format, "*");
    assert (ANDROID_LOG_DEBUG == filterPriForTag(p_format, "random"));
    assert(android_log_shouldPrintLine(p_format, tag, ANDROID_LOG_DEBUG) > 0);
    android_log_addFilterRule(p_format, "*:v");
    assert (ANDROID_LOG_VERBOSE == filterPriForTag(p_format, "random"));
    assert(android_log_shouldPrintLine(p_format, tag, ANDROID_LOG_DEBUG) > 0);
    android_log_addFilterRule(p_format, "*:i");
    assert (ANDROID_LOG_INFO == filterPriForTag(p_format, "random"));
    assert(android_log_shouldPrintLine(p_format, tag, ANDROID_LOG_DEBUG) == 0);

    android_log_addFilterRule(p_format, "random");
    assert (ANDROID_LOG_VERBOSE == filterPriForTag(p_format, "random"));
    assert(android_log_shouldPrintLine(p_format, tag, ANDROID_LOG_DEBUG) > 0);
    android_log_addFilterRule(p_format, "random:v");
    assert (ANDROID_LOG_VERBOSE == filterPriForTag(p_format, "random"));
    assert(android_log_shouldPrintLine(p_format, tag, ANDROID_LOG_DEBUG) > 0);
    android_log_addFilterRule(p_format, "random:d");
    assert (ANDROID_LOG_DEBUG == filterPriForTag(p_format, "random"));
    assert(android_log_shouldPrintLine(p_format, tag, ANDROID_LOG_DEBUG) > 0);
    android_log_addFilterRule(p_format, "random:w");
    assert (ANDROID_LOG_WARN == filterPriForTag(p_format, "random"));
    assert(android_log_shouldPrintLine(p_format, tag, ANDROID_LOG_DEBUG) == 0);

    android_log_addFilterRule(p_format, "crap:*");
    assert (ANDROID_LOG_VERBOSE== filterPriForTag(p_format, "crap"));
    assert(android_log_shouldPrintLine(p_format, "crap", ANDROID_LOG_VERBOSE) > 0);

    // invalid expression
    err = android_log_addFilterRule(p_format, "random:z");
    assert (err < 0);
    assert (ANDROID_LOG_WARN == filterPriForTag(p_format, "random"));
    assert(android_log_shouldPrintLine(p_format, tag, ANDROID_LOG_DEBUG) == 0);

    // Issue #550946
    err = android_log_addFilterString(p_format, " ");
    assert(err == 0);
    assert(ANDROID_LOG_WARN == filterPriForTag(p_format, "random"));

    // note trailing space
    err = android_log_addFilterString(p_format, "*:s random:d ");
    assert(err == 0);
    assert(ANDROID_LOG_DEBUG == filterPriForTag(p_format, "random"));

    err = android_log_addFilterString(p_format, "*:s random:z");
    assert(err < 0);


#if 0
    char *ret;
    char defaultBuffer[512];

    ret = android_log_formatLogLine(p_format,
        defaultBuffer, sizeof(defaultBuffer), 0, ANDROID_LOG_ERROR, 123,
        123, 123, "random", "nofile", strlen("Hello"), "Hello", NULL);
#endif


    fprintf(stderr, "tests complete\n");
#endif
}

#undef UNINDENTED_BLOCK_START
#undef UNINDENTED_BLOCK_END
