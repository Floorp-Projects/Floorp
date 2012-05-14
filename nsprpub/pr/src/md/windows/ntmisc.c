/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * ntmisc.c
 *
 */

#include "primpl.h"
#include <math.h>     /* for fabs() */
#include <windows.h>

char *_PR_MD_GET_ENV(const char *name)
{
    return getenv(name);
}

/*
** _PR_MD_PUT_ENV() -- add or change environment variable
**
**
*/
PRIntn _PR_MD_PUT_ENV(const char *name)
{
    return(putenv(name));
}


/*
 **************************************************************************
 **************************************************************************
 **
 **     Date and time routines
 **
 **************************************************************************
 **************************************************************************
 */

/*
 * The NSPR epoch (00:00:00 1 Jan 1970 UTC) in FILETIME.
 * We store the value in a PRTime variable for convenience.
 */
#ifdef __GNUC__
const PRTime _pr_filetime_offset = 116444736000000000LL;
const PRTime _pr_filetime_divisor = 10LL;
#else
const PRTime _pr_filetime_offset = 116444736000000000i64;
const PRTime _pr_filetime_divisor = 10i64;
#endif

#ifdef WINCE

#define FILETIME_TO_INT64(ft) \
  (((PRInt64)ft.dwHighDateTime) << 32 | (PRInt64)ft.dwLowDateTime)

static void
LowResTime(LPFILETIME lpft)
{
    GetCurrentFT(lpft);
}

typedef struct CalibrationData {
    long double freq;         /* The performance counter frequency */
    long double offset;       /* The low res 'epoch' */
    long double timer_offset; /* The high res 'epoch' */

    /* The last high res time that we returned since recalibrating */
    PRInt64 last;

    PRBool calibrated;

    CRITICAL_SECTION data_lock;
    CRITICAL_SECTION calibration_lock;
    PRInt64 granularity;
} CalibrationData;

static CalibrationData calibration;

typedef void (*GetSystemTimeAsFileTimeFcn)(LPFILETIME);
static GetSystemTimeAsFileTimeFcn ce6_GetSystemTimeAsFileTime = NULL;

static void
NowCalibrate(void)
{
    FILETIME ft, ftStart;
    LARGE_INTEGER liFreq, now;

    if (calibration.freq == 0.0) {
	if(!QueryPerformanceFrequency(&liFreq)) {
	    /* High-performance timer is unavailable */
	    calibration.freq = -1.0;
	} else {
	    calibration.freq = (long double) liFreq.QuadPart;
	}
    }
    if (calibration.freq > 0.0) {
	PRInt64 calibrationDelta = 0;
	/*
	 * By wrapping a timeBegin/EndPeriod pair of calls around this loop,
	 * the loop seems to take much less time (1 ms vs 15ms) on Vista. 
	 */
	timeBeginPeriod(1);
	LowResTime(&ftStart);
	do {
	    LowResTime(&ft);
	} while (memcmp(&ftStart,&ft, sizeof(ft)) == 0);
	timeEndPeriod(1);

	calibration.granularity = 
	    (FILETIME_TO_INT64(ft) - FILETIME_TO_INT64(ftStart))/10;

	QueryPerformanceCounter(&now);

	calibration.offset = (long double) FILETIME_TO_INT64(ft);
	calibration.timer_offset = (long double) now.QuadPart;
	/*
	 * The windows epoch is around 1600. The unix epoch is around 1970. 
	 * _pr_filetime_offset is the difference (in windows time units which
	 * are 10 times more highres than the JS time unit) 
	 */
	calibration.offset -= _pr_filetime_offset;
	calibration.offset *= 0.1;
	calibration.last = 0;

	calibration.calibrated = PR_TRUE;
    }
}

#define CALIBRATIONLOCK_SPINCOUNT 0
#define DATALOCK_SPINCOUNT 4096
#define LASTLOCK_SPINCOUNT 4096

void
_MD_InitTime(void)
{
    /* try for CE6 GetSystemTimeAsFileTime first */
    HANDLE h = GetModuleHandleW(L"coredll.dll");
    ce6_GetSystemTimeAsFileTime = (GetSystemTimeAsFileTimeFcn)
        GetProcAddressA(h, "GetSystemTimeAsFileTime");

    /* otherwise go the slow route */
    if (ce6_GetSystemTimeAsFileTime == NULL) {
        memset(&calibration, 0, sizeof(calibration));
        NowCalibrate();
        InitializeCriticalSection(&calibration.calibration_lock);
        InitializeCriticalSection(&calibration.data_lock);
    }
}

void
_MD_CleanupTime(void)
{
    if (ce6_GetSystemTimeAsFileTime == NULL) {
        DeleteCriticalSection(&calibration.calibration_lock);
        DeleteCriticalSection(&calibration.data_lock);
    }
}

#define MUTEX_SETSPINCOUNT(m, c)

/*
 *-----------------------------------------------------------------------
 *
 * PR_Now --
 *
 *     Returns the current time in microseconds since the epoch.
 *     The epoch is midnight January 1, 1970 GMT.
 *     The implementation is machine dependent.  This is the
 *     implementation for Windows.
 *     Cf. time_t time(time_t *tp)
 *
 *-----------------------------------------------------------------------
 */

PR_IMPLEMENT(PRTime)
PR_Now(void)
{
    long double lowresTime, highresTimerValue;
    FILETIME ft;
    LARGE_INTEGER now;
    PRBool calibrated = PR_FALSE;
    PRBool needsCalibration = PR_FALSE;
    PRInt64 returnedTime;
    long double cachedOffset = 0.0;

    if (ce6_GetSystemTimeAsFileTime) {
        union {
            FILETIME ft;
            PRTime prt;
        } currentTime;

        PR_ASSERT(sizeof(FILETIME) == sizeof(PRTime));

        ce6_GetSystemTimeAsFileTime(&currentTime.ft);

        /* written this way on purpose, since the second term becomes
         * a constant, and the entire expression is faster to execute.
         */
        return currentTime.prt/_pr_filetime_divisor -
            _pr_filetime_offset/_pr_filetime_divisor;
    }

    do {
	if (!calibration.calibrated || needsCalibration) {
	    EnterCriticalSection(&calibration.calibration_lock);
	    EnterCriticalSection(&calibration.data_lock);

	    /* Recalibrate only if no one else did before us */
	    if (calibration.offset == cachedOffset) {
		/*
		 * Since calibration can take a while, make any other
		 * threads immediately wait 
		 */
		MUTEX_SETSPINCOUNT(&calibration.data_lock, 0);

		NowCalibrate();

		calibrated = PR_TRUE;

		/* Restore spin count */
		MUTEX_SETSPINCOUNT(&calibration.data_lock, DATALOCK_SPINCOUNT);
	    }
	    LeaveCriticalSection(&calibration.data_lock);
	    LeaveCriticalSection(&calibration.calibration_lock);
	}

	/* Calculate a low resolution time */
	LowResTime(&ft);
	lowresTime =
            ((long double)(FILETIME_TO_INT64(ft) - _pr_filetime_offset)) * 0.1;

	if (calibration.freq > 0.0) {
	    long double highresTime, diff;
	    DWORD timeAdjustment, timeIncrement;
	    BOOL timeAdjustmentDisabled;

	    /* Default to 15.625 ms if the syscall fails */
	    long double skewThreshold = 15625.25;

	    /* Grab high resolution time */
	    QueryPerformanceCounter(&now);
	    highresTimerValue = (long double)now.QuadPart;

	    EnterCriticalSection(&calibration.data_lock);
	    highresTime = calibration.offset + 1000000L *
		(highresTimerValue-calibration.timer_offset)/calibration.freq;
	    cachedOffset = calibration.offset;

	    /* 
	     * On some dual processor/core systems, we might get an earlier 
	     * time so we cache the last time that we returned.
	     */
	    calibration.last = PR_MAX(calibration.last,(PRInt64)highresTime);
	    returnedTime = calibration.last;
	    LeaveCriticalSection(&calibration.data_lock);

	    /* Get an estimate of clock ticks per second from our own test */
	    skewThreshold = calibration.granularity;
	    /* Check for clock skew */
	    diff = lowresTime - highresTime;

	    /* 
	     * For some reason that I have not determined, the skew can be
	     * up to twice a kernel tick. This does not seem to happen by
	     * itself, but I have only seen it triggered by another program
	     * doing some kind of file I/O. The symptoms are a negative diff
	     * followed by an equally large positive diff. 
	     */
	    if (fabs(diff) > 2*skewThreshold) {
		if (calibrated) {
		    /*
		     * If we already calibrated once this instance, and the
		     * clock is still skewed, then either the processor(s) are
		     * wildly changing clockspeed or the system is so busy that
		     * we get switched out for long periods of time. In either
		     * case, it would be infeasible to make use of high
		     * resolution results for anything, so let's resort to old
		     * behavior for this call. It's possible that in the
		     * future, the user will want the high resolution timer, so
		     * we don't disable it entirely. 
		     */
		    returnedTime = (PRInt64)lowresTime;
		    needsCalibration = PR_FALSE;
		} else {
		    /*
		     * It is possible that when we recalibrate, we will return 
		     * a value less than what we have returned before; this is
		     * unavoidable. We cannot tell the different between a
		     * faulty QueryPerformanceCounter implementation and user
		     * changes to the operating system time. Since we must
		     * respect user changes to the operating system time, we
		     * cannot maintain the invariant that Date.now() never
		     * decreases; the old implementation has this behavior as
		     * well. 
		     */
		    needsCalibration = PR_TRUE;
		}
	    } else {
		/* No detectable clock skew */
		returnedTime = (PRInt64)highresTime;
		needsCalibration = PR_FALSE;
	    }
	} else {
	    /* No high resolution timer is available, so fall back */
	    returnedTime = (PRInt64)lowresTime;
	}
    } while (needsCalibration);

    return returnedTime;
}

#else

PR_IMPLEMENT(PRTime)
PR_Now(void)
{
    PRTime prt;
    FILETIME ft;
    SYSTEMTIME st;

    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);
    _PR_FileTimeToPRTime(&ft, &prt);
    return prt;       
}

#endif

/*
 ***********************************************************************
 ***********************************************************************
 *
 * Process creation routines
 *
 ***********************************************************************
 ***********************************************************************
 */

/*
 * Assemble the command line by concatenating the argv array.
 * On success, this function returns 0 and the resulting command
 * line is returned in *cmdLine.  On failure, it returns -1.
 */
static int assembleCmdLine(char *const *argv, char **cmdLine)
{
    char *const *arg;
    char *p, *q;
    size_t cmdLineSize;
    int numBackslashes;
    int i;
    int argNeedQuotes;

    /*
     * Find out how large the command line buffer should be.
     */
    cmdLineSize = 0;
    for (arg = argv; *arg; arg++) {
        /*
         * \ and " need to be escaped by a \.  In the worst case,
         * every character is a \ or ", so the string of length
         * may double.  If we quote an argument, that needs two ".
         * Finally, we need a space between arguments, and
         * a null byte at the end of command line.
         */
        cmdLineSize += 2 * strlen(*arg)  /* \ and " need to be escaped */
                + 2                      /* we quote every argument */
                + 1;                     /* space in between, or final null */
    }
    p = *cmdLine = PR_MALLOC((PRUint32) cmdLineSize);
    if (p == NULL) {
        return -1;
    }

    for (arg = argv; *arg; arg++) {
        /* Add a space to separates the arguments */
        if (arg != argv) {
            *p++ = ' '; 
        }
        q = *arg;
        numBackslashes = 0;
        argNeedQuotes = 0;

        /*
         * If the argument is empty or contains white space, it needs to
         * be quoted.
         */
        if (**arg == '\0' || strpbrk(*arg, " \f\n\r\t\v")) {
            argNeedQuotes = 1;
        }

        if (argNeedQuotes) {
            *p++ = '"';
        }
        while (*q) {
            if (*q == '\\') {
                numBackslashes++;
                q++;
            } else if (*q == '"') {
                if (numBackslashes) {
                    /*
                     * Double the backslashes since they are followed
                     * by a quote
                     */
                    for (i = 0; i < 2 * numBackslashes; i++) {
                        *p++ = '\\';
                    }
                    numBackslashes = 0;
                }
                /* To escape the quote */
                *p++ = '\\';
                *p++ = *q++;
            } else {
                if (numBackslashes) {
                    /*
                     * Backslashes are not followed by a quote, so
                     * don't need to double the backslashes.
                     */
                    for (i = 0; i < numBackslashes; i++) {
                        *p++ = '\\';
                    }
                    numBackslashes = 0;
                }
                *p++ = *q++;
            }
        }

        /* Now we are at the end of this argument */
        if (numBackslashes) {
            /*
             * Double the backslashes if we have a quote string
             * delimiter at the end.
             */
            if (argNeedQuotes) {
                numBackslashes *= 2;
            }
            for (i = 0; i < numBackslashes; i++) {
                *p++ = '\\';
            }
        }
        if (argNeedQuotes) {
            *p++ = '"';
        }
    } 

    *p = '\0';
    return 0;
}

/*
 * Assemble the environment block by concatenating the envp array
 * (preserving the terminating null byte in each array element)
 * and adding a null byte at the end.
 *
 * Returns 0 on success.  The resulting environment block is returned
 * in *envBlock.  Note that if envp is NULL, a NULL pointer is returned
 * in *envBlock.  Returns -1 on failure.
 */
static int assembleEnvBlock(char **envp, char **envBlock)
{
    char *p;
    char *q;
    char **env;
    char *curEnv;
    char *cwdStart, *cwdEnd;
    size_t envBlockSize;

    if (envp == NULL) {
        *envBlock = NULL;
        return 0;
    }

#ifdef WINCE
    {
        PRUnichar *wideCurEnv = mozce_GetEnvString();
        int len = WideCharToMultiByte(CP_ACP, 0, wideCurEnv, -1,
                                      NULL, 0, NULL, NULL);
        curEnv = (char *) PR_MALLOC(len * sizeof(char));
        WideCharToMultiByte(CP_ACP, 0, wideCurEnv, -1,
                            curEnv, len, NULL, NULL);
        free(wideCurEnv);
    }
#else
    curEnv = GetEnvironmentStrings();
#endif

    cwdStart = curEnv;
    while (*cwdStart) {
        if (cwdStart[0] == '=' && cwdStart[1] != '\0'
                && cwdStart[2] == ':' && cwdStart[3] == '=') {
            break;
        }
        cwdStart += strlen(cwdStart) + 1;
    }
    cwdEnd = cwdStart;
    if (*cwdEnd) {
        cwdEnd += strlen(cwdEnd) + 1;
        while (*cwdEnd) {
            if (cwdEnd[0] != '=' || cwdEnd[1] == '\0'
                    || cwdEnd[2] != ':' || cwdEnd[3] != '=') {
                break;
            }
            cwdEnd += strlen(cwdEnd) + 1;
        }
    }
    envBlockSize = cwdEnd - cwdStart;

    for (env = envp; *env; env++) {
        envBlockSize += strlen(*env) + 1;
    }
    envBlockSize++;

    p = *envBlock = PR_MALLOC((PRUint32) envBlockSize);
    if (p == NULL) {
#ifdef WINCE
        PR_Free(curEnv);
#else
        FreeEnvironmentStrings(curEnv);
#endif
        return -1;
    }

    q = cwdStart;
    while (q < cwdEnd) {
        *p++ = *q++;
    }
#ifdef WINCE
    PR_Free(curEnv);
#else
    FreeEnvironmentStrings(curEnv);
#endif

    for (env = envp; *env; env++) {
        q = *env;
        while (*q) {
            *p++ = *q++;
        }
        *p++ = '\0';
    }
    *p = '\0';
    return 0;
}

/*
 * For qsort.  We sort (case-insensitive) the environment strings
 * before generating the environment block.
 */
static int compare(const void *arg1, const void *arg2)
{
    return _stricmp(* (char**)arg1, * (char**)arg2);
}

PRProcess * _PR_CreateWindowsProcess(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr)
{
#ifdef WINCE
    STARTUPINFOW startupInfo;
    PRUnichar *wideCmdLine;
    PRUnichar *wideCwd;
    int len = 0;
#else
    STARTUPINFO startupInfo;
#endif
    DWORD creationFlags = 0;
    PROCESS_INFORMATION procInfo;
    BOOL retVal;
    char *cmdLine = NULL;
    char *envBlock = NULL;
    char **newEnvp = NULL;
    const char *cwd = NULL; /* current working directory */
    PRProcess *proc = NULL;
    PRBool hasFdInheritBuffer;

    proc = PR_NEW(PRProcess);
    if (!proc) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        goto errorExit;
    }

    if (assembleCmdLine(argv, &cmdLine) == -1) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        goto errorExit;
    }

#ifndef WINCE
    /*
     * If attr->fdInheritBuffer is not NULL, we need to insert
     * it into the envp array, so envp cannot be NULL.
     */
    hasFdInheritBuffer = (attr && attr->fdInheritBuffer);
    if ((envp == NULL) && hasFdInheritBuffer) {
        envp = environ;
    }

    if (envp != NULL) {
        int idx;
        int numEnv;
        PRBool found = PR_FALSE;

        numEnv = 0;
        while (envp[numEnv]) {
            numEnv++;
        }
        newEnvp = (char **) PR_MALLOC((numEnv + 2) * sizeof(char *));
        for (idx = 0; idx < numEnv; idx++) {
            newEnvp[idx] = envp[idx];
            if (hasFdInheritBuffer && !found
                    && !strncmp(newEnvp[idx], "NSPR_INHERIT_FDS=", 17)) {
                newEnvp[idx] = attr->fdInheritBuffer;
                found = PR_TRUE;
            }
        }
        if (hasFdInheritBuffer && !found) {
            newEnvp[idx++] = attr->fdInheritBuffer;
        }
        newEnvp[idx] = NULL;
        qsort((void *) newEnvp, (size_t) idx, sizeof(char *), compare);
    }
    if (assembleEnvBlock(newEnvp, &envBlock) == -1) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        goto errorExit;
    }

    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);

    if (attr) {
        PRBool redirected = PR_FALSE;

        /*
         * XXX the default value for stdin, stdout, and stderr
         * should probably be the console input and output, not
         * those of the parent process.
         */
        startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        if (attr->stdinFd) {
            startupInfo.hStdInput = (HANDLE) attr->stdinFd->secret->md.osfd;
            redirected = PR_TRUE;
        }
        if (attr->stdoutFd) {
            startupInfo.hStdOutput = (HANDLE) attr->stdoutFd->secret->md.osfd;
            redirected = PR_TRUE;
            /*
             * If stdout is redirected, we can assume that the process will
             * not write anything useful to the console windows, and therefore
             * automatically set the CREATE_NO_WINDOW flag.
             */
            creationFlags |= CREATE_NO_WINDOW;
        }
        if (attr->stderrFd) {
            startupInfo.hStdError = (HANDLE) attr->stderrFd->secret->md.osfd;
            redirected = PR_TRUE;
        }
        if (redirected) {
            startupInfo.dwFlags |= STARTF_USESTDHANDLES;
        }
        cwd = attr->currentDirectory;
    }
#endif

#ifdef WINCE
    len = MultiByteToWideChar(CP_ACP, 0, cmdLine, -1, NULL, 0);
    wideCmdLine = (PRUnichar *)PR_MALLOC(len * sizeof(PRUnichar));
    MultiByteToWideChar(CP_ACP, 0, cmdLine, -1, wideCmdLine, len);
    len = MultiByteToWideChar(CP_ACP, 0, cwd, -1, NULL, 0);
    wideCwd = PR_MALLOC(len * sizeof(PRUnichar));
    MultiByteToWideChar(CP_ACP, 0, cwd, -1, wideCwd, len);
    retVal = CreateProcessW(NULL,
                            wideCmdLine,
                            NULL,  /* security attributes for the new
                                    * process */
                            NULL,  /* security attributes for the primary
                                    * thread in the new process */
                            TRUE,  /* inherit handles */
                            creationFlags,
                            envBlock,  /* an environment block, consisting
                                        * of a null-terminated block of
                                        * null-terminated strings.  Each
                                        * string is in the form:
                                        *     name=value
                                        * XXX: usually NULL */
                            wideCwd,  /* current drive and directory */
                            &startupInfo,
                            &procInfo
                           );
    PR_Free(wideCmdLine);
    PR_Free(wideCwd);
#else
    retVal = CreateProcess(NULL,
                           cmdLine,
                           NULL,  /* security attributes for the new
                                   * process */
                           NULL,  /* security attributes for the primary
                                   * thread in the new process */
                           TRUE,  /* inherit handles */
                           creationFlags,
                           envBlock,  /* an environment block, consisting
                                       * of a null-terminated block of
                                       * null-terminated strings.  Each
                                       * string is in the form:
                                       *     name=value
                                       * XXX: usually NULL */
                           cwd,  /* current drive and directory */
                           &startupInfo,
                           &procInfo
                          );
#endif

    if (retVal == FALSE) {
        /* XXX what error code? */
        PR_SetError(PR_UNKNOWN_ERROR, GetLastError());
        goto errorExit;
    }

    CloseHandle(procInfo.hThread);
    proc->md.handle = procInfo.hProcess;
    proc->md.id = procInfo.dwProcessId;

    PR_DELETE(cmdLine);
    if (newEnvp) {
        PR_DELETE(newEnvp);
    }
    if (envBlock) {
        PR_DELETE(envBlock);
    }
    return proc;

errorExit:
    if (cmdLine) {
        PR_DELETE(cmdLine);
    }
    if (newEnvp) {
        PR_DELETE(newEnvp);
    }
    if (envBlock) {
        PR_DELETE(envBlock);
    }
    if (proc) {
        PR_DELETE(proc);
    }
    return NULL;
}  /* _PR_CreateWindowsProcess */

PRStatus _PR_DetachWindowsProcess(PRProcess *process)
{
    CloseHandle(process->md.handle);
    PR_DELETE(process);
    return PR_SUCCESS;
}

/*
 * XXX: This implementation is a temporary quick solution.
 * It can be called by native threads only (not by fibers).
 */
PRStatus _PR_WaitWindowsProcess(PRProcess *process,
    PRInt32 *exitCode)
{
    DWORD dwRetVal;

    dwRetVal = WaitForSingleObject(process->md.handle, INFINITE);
    if (dwRetVal == WAIT_FAILED) {
        PR_SetError(PR_UNKNOWN_ERROR, GetLastError());
        return PR_FAILURE;
    }
    PR_ASSERT(dwRetVal == WAIT_OBJECT_0);
    if (exitCode != NULL &&
            GetExitCodeProcess(process->md.handle, exitCode) == FALSE) {
        PR_SetError(PR_UNKNOWN_ERROR, GetLastError());
        return PR_FAILURE;
    }
    CloseHandle(process->md.handle);
    PR_DELETE(process);
    return PR_SUCCESS;
}

PRStatus _PR_KillWindowsProcess(PRProcess *process)
{
    /*
     * On Unix, if a process terminates normally, its exit code is
     * between 0 and 255.  So here on Windows, we use the exit code
     * 256 to indicate that the process is killed.
     */
    if (TerminateProcess(process->md.handle, 256)) {
	return PR_SUCCESS;
    }
    PR_SetError(PR_UNKNOWN_ERROR, GetLastError());
    return PR_FAILURE;
}

PRStatus _MD_WindowsGetHostName(char *name, PRUint32 namelen)
{
    PRIntn rv;
    PRInt32 syserror;

    rv = gethostname(name, (PRInt32) namelen);
    if (0 == rv) {
        return PR_SUCCESS;
    }
    syserror = WSAGetLastError();
    PR_ASSERT(WSANOTINITIALISED != syserror);
	_PR_MD_MAP_GETHOSTNAME_ERROR(syserror);
    return PR_FAILURE;
}

PRStatus _MD_WindowsGetSysInfo(PRSysInfo cmd, char *name, PRUint32 namelen)
{
	OSVERSIONINFO osvi;

	PR_ASSERT((cmd == PR_SI_SYSNAME) || (cmd == PR_SI_RELEASE));

	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	if (! GetVersionEx (&osvi) ) {
		_PR_MD_MAP_DEFAULT_ERROR(GetLastError());
    	return PR_FAILURE;
	}

	switch (osvi.dwPlatformId) {
		case VER_PLATFORM_WIN32_NT:
			if (PR_SI_SYSNAME == cmd)
				(void)PR_snprintf(name, namelen, "Windows_NT");
			else if (PR_SI_RELEASE == cmd)
				(void)PR_snprintf(name, namelen, "%d.%d",osvi.dwMajorVersion, 
            							osvi.dwMinorVersion);
			break;
		case VER_PLATFORM_WIN32_WINDOWS:
			if (PR_SI_SYSNAME == cmd) {
				if ((osvi.dwMajorVersion > 4) || 
					((osvi.dwMajorVersion == 4) && (osvi.dwMinorVersion > 0)))
					(void)PR_snprintf(name, namelen, "Windows_98");
				else
					(void)PR_snprintf(name, namelen, "Windows_95");
			} else if (PR_SI_RELEASE == cmd) {
				(void)PR_snprintf(name, namelen, "%d.%d",osvi.dwMajorVersion, 
            							osvi.dwMinorVersion);
			}
			break;
#ifdef VER_PLATFORM_WIN32_CE
    case VER_PLATFORM_WIN32_CE:
			if (PR_SI_SYSNAME == cmd)
				(void)PR_snprintf(name, namelen, "Windows_CE");
			else if (PR_SI_RELEASE == cmd)
				(void)PR_snprintf(name, namelen, "%d.%d",osvi.dwMajorVersion, 
            							osvi.dwMinorVersion);
			break;
#endif
   		default:
			if (PR_SI_SYSNAME == cmd)
				(void)PR_snprintf(name, namelen, "Windows_Unknown");
			else if (PR_SI_RELEASE == cmd)
				(void)PR_snprintf(name, namelen, "%d.%d",0,0);
			break;
	}
	return PR_SUCCESS;
}

PRStatus _MD_WindowsGetReleaseName(char *name, PRUint32 namelen)
{
	OSVERSIONINFO osvi;

	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	if (! GetVersionEx (&osvi) ) {
		_PR_MD_MAP_DEFAULT_ERROR(GetLastError());
    	return PR_FAILURE;
	}

	switch (osvi.dwPlatformId) {
		case VER_PLATFORM_WIN32_NT:
		case VER_PLATFORM_WIN32_WINDOWS:
			(void)PR_snprintf(name, namelen, "%d.%d",osvi.dwMajorVersion, 
            							osvi.dwMinorVersion);
			break;
   		default:
			(void)PR_snprintf(name, namelen, "%d.%d",0,0);
			break;
	}
	return PR_SUCCESS;
}

/*
 **********************************************************************
 *
 * Memory-mapped files
 *
 **********************************************************************
 */

PRStatus _MD_CreateFileMap(PRFileMap *fmap, PRInt64 size)
{
    DWORD dwHi, dwLo;
    DWORD flProtect;
    PROsfd osfd;

    osfd = ( fmap->fd == (PRFileDesc*)-1 )?  -1 : fmap->fd->secret->md.osfd;

    dwLo = (DWORD) (size & 0xffffffff);
    dwHi = (DWORD) (((PRUint64) size >> 32) & 0xffffffff);

    if (fmap->prot == PR_PROT_READONLY) {
        flProtect = PAGE_READONLY;
        fmap->md.dwAccess = FILE_MAP_READ;
    } else if (fmap->prot == PR_PROT_READWRITE) {
        flProtect = PAGE_READWRITE;
        fmap->md.dwAccess = FILE_MAP_WRITE;
    } else {
        PR_ASSERT(fmap->prot == PR_PROT_WRITECOPY);
#ifdef WINCE
        /* WINCE does not have FILE_MAP_COPY. */
        PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
        return PR_FAILURE;
#else
        flProtect = PAGE_WRITECOPY;
        fmap->md.dwAccess = FILE_MAP_COPY;
#endif
    }

    fmap->md.hFileMap = CreateFileMapping(
        (HANDLE) osfd,
        NULL,
        flProtect,
        dwHi,
        dwLo,
        NULL);

    if (fmap->md.hFileMap == NULL) {
        PR_SetError(PR_UNKNOWN_ERROR, GetLastError());
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

PRInt32 _MD_GetMemMapAlignment(void)
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwAllocationGranularity;
}

extern PRLogModuleInfo *_pr_shma_lm;

void * _MD_MemMap(
    PRFileMap *fmap,
    PROffset64 offset,
    PRUint32 len)
{
    DWORD dwHi, dwLo;
    void *addr;

    dwLo = (DWORD) (offset & 0xffffffff);
    dwHi = (DWORD) (((PRUint64) offset >> 32) & 0xffffffff);
    if ((addr = MapViewOfFile(fmap->md.hFileMap, fmap->md.dwAccess,
            dwHi, dwLo, len)) == NULL) {
        {
            LPVOID lpMsgBuf; 
            
            FormatMessage( 
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                NULL,
                GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                (LPTSTR) &lpMsgBuf,
                0,
                NULL 
            );
            PR_LOG( _pr_shma_lm, PR_LOG_DEBUG, ("md_memmap(): %s", lpMsgBuf ));
        }
        PR_SetError(PR_UNKNOWN_ERROR, GetLastError());
    }
    return addr;
}

PRStatus _MD_MemUnmap(void *addr, PRUint32 len)
{
    if (UnmapViewOfFile(addr)) {
        return PR_SUCCESS;
    } else {
        PR_SetError(PR_UNKNOWN_ERROR, GetLastError());
        return PR_FAILURE;
    }
}

PRStatus _MD_CloseFileMap(PRFileMap *fmap)
{
    CloseHandle(fmap->md.hFileMap);
    PR_DELETE(fmap);
    return PR_SUCCESS;
}

/*
 ***********************************************************************
 *
 * Atomic increment and decrement operations for x86 processors
 *
 * We don't use InterlockedIncrement and InterlockedDecrement
 * because on NT 3.51 and Win95, they return a number with
 * the same sign as the incremented/decremented result, rather
 * than the result itself.  On NT 4.0 these functions do return
 * the incremented/decremented result.
 *
 * The result is returned in the eax register by the inline
 * assembly code.  We disable the harmless "no return value"
 * warning (4035) for these two functions.
 *
 ***********************************************************************
 */

#if defined(_M_IX86) || defined(_X86_)

#pragma warning(disable: 4035)
PRInt32 _PR_MD_ATOMIC_INCREMENT(PRInt32 *val)
{    
#if defined(__GNUC__)
  PRInt32 result;
  asm volatile ("lock ; xadd %0, %1" 
                : "=r"(result), "=m"(*val)
                : "0"(1), "m"(*val));
  return result + 1;
#else
    __asm
    {
        mov ecx, val
        mov eax, 1
        lock xadd dword ptr [ecx], eax
        inc eax
    }
#endif /* __GNUC__ */
}
#pragma warning(default: 4035)

#pragma warning(disable: 4035)
PRInt32 _PR_MD_ATOMIC_DECREMENT(PRInt32 *val)
{
#if defined(__GNUC__)
  PRInt32 result;
  asm volatile ("lock ; xadd %0, %1" 
                : "=r"(result), "=m"(*val)
                : "0"(-1), "m"(*val));
  //asm volatile("lock ; xadd %0, %1" : "=m" (val), "=a" (result) : "-1" (1));
  return result - 1;
#else
    __asm
    {
        mov ecx, val
        mov eax, 0ffffffffh
        lock xadd dword ptr [ecx], eax
        dec eax
    }
#endif /* __GNUC__ */
}
#pragma warning(default: 4035)

#pragma warning(disable: 4035)
PRInt32 _PR_MD_ATOMIC_ADD(PRInt32 *intp, PRInt32 val)
{
#if defined(__GNUC__)
  PRInt32 result;
  //asm volatile("lock ; xadd %1, %0" : "=m" (intp), "=a" (result) : "1" (val));
  asm volatile ("lock ; xadd %0, %1" 
                : "=r"(result), "=m"(*intp)
                : "0"(val), "m"(*intp));
  return result + val;
#else
    __asm
    {
        mov ecx, intp
        mov eax, val
        mov edx, eax
        lock xadd dword ptr [ecx], eax
        add eax, edx
    }
#endif /* __GNUC__ */
}
#pragma warning(default: 4035)

#ifdef _PR_HAVE_ATOMIC_CAS

#pragma warning(disable: 4035)
void 
PR_StackPush(PRStack *stack, PRStackElem *stack_elem)
{
#if defined(__GNUC__)
  void **tos = (void **) stack;
  void *tmp;
  
 retry:
  if (*tos == (void *) -1)
    goto retry;
  
  __asm__("xchg %0,%1"
          : "=r" (tmp), "=m"(*tos)
          : "0" (-1), "m"(*tos));
  
  if (tmp == (void *) -1)
    goto retry;
  
  *(void **)stack_elem = tmp;
  __asm__("" : : : "memory");
  *tos = stack_elem;
#else
    __asm
    {
	mov ebx, stack
	mov ecx, stack_elem
retry:	mov eax,[ebx]
	cmp eax,-1
	je retry
	mov eax,-1
	xchg dword ptr [ebx], eax
	cmp eax,-1
	je  retry
	mov [ecx],eax
	mov [ebx],ecx
    }
#endif /* __GNUC__ */
}
#pragma warning(default: 4035)

#pragma warning(disable: 4035)
PRStackElem * 
PR_StackPop(PRStack *stack)
{
#if defined(__GNUC__)
  void **tos = (void **) stack;
  void *tmp;
  
 retry:
  if (*tos == (void *) -1)
    goto retry;
  
  __asm__("xchg %0,%1"
          : "=r" (tmp), "=m"(*tos)
          : "0" (-1), "m"(*tos));

  if (tmp == (void *) -1)
    goto retry;
  
  if (tmp != (void *) 0)
    {
      void *next = *(void **)tmp;
      *tos = next;
      *(void **)tmp = 0;
    }
  else
    *tos = tmp;
  
  return tmp;
#else
    __asm
    {
	mov ebx, stack
retry:	mov eax,[ebx]
	cmp eax,-1
	je retry
	mov eax,-1
	xchg dword ptr [ebx], eax
	cmp eax,-1
	je  retry
	cmp eax,0
	je  empty
	mov ecx,[eax]
	mov [ebx],ecx
	mov [eax],0
	jmp done
empty:
	mov [ebx],eax
done:	
	}
#endif /* __GNUC__ */
}
#pragma warning(default: 4035)

#endif /* _PR_HAVE_ATOMIC_CAS */

#endif /* x86 processors */
