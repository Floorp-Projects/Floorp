/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * ntmisc.c
 *
 */

#include "primpl.h"

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

#include <sys/timeb.h>

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
    PRTime prt;
    FILETIME ft;

    GetSystemTimeAsFileTime(&ft);
    _PR_FileTimeToPRTime(&ft, &prt);
    return prt;       
}

/*
 * The following code works around a bug in NT (Netscape Bugsplat
 * Defect ID 47942).
 *
 * In Windows NT 3.51 and 4.0, if the local time zone does not practice
 * daylight savings time, e.g., Arizona, Taiwan, and Japan, the global
 * variables that _ftime() and localtime() depend on have the wrong
 * default values:
 *     _tzname[0]  "PST"
 *     _tzname[1]  "PDT"
 *     _daylight   1
 *     _timezone   28800
 *
 * So at startup time, we need to invoke _PR_Win32InitTimeZone(), which
 * on NT sets these global variables to the correct values (obtained by
 * calling GetTimeZoneInformation().
 */

#include <time.h>     /* for _tzname, _daylight, _timezone */

void
_PR_Win32InitTimeZone(void)
{
    OSVERSIONINFO version;
    TIME_ZONE_INFORMATION tzinfo;

    version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (GetVersionEx(&version) != FALSE) {
        /* Only Windows NT needs this hack */
        if (version.dwPlatformId != VER_PLATFORM_WIN32_NT) {
            return;
        }
    }

    if (GetTimeZoneInformation(&tzinfo) == 0xffffffff) {
        return;  /* not much we can do if this failed */
    }
 
    /* 
     * I feel nervous about modifying these globals.  I hope that no
     * other thread is reading or modifying these globals simultaneously
     * during nspr initialization.
     *
     * I am assuming that _tzname[0] and _tzname[1] point to static buffers
     * and that the buffers are at least 32 byte long.  My experiments show
     * this is true, but of course this is undocumented.  --wtc
     *
     * Convert time zone names from WCHAR to CHAR and copy them to
     * the static buffers pointed to by _tzname[0] and _tzname[1].
     * Ignore conversion errors, because it is _timezone and _daylight
     * that _ftime() and localtime() really depend on.
     */

    WideCharToMultiByte(CP_ACP, 0, tzinfo.StandardName, -1, _tzname[0],
            32, NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, tzinfo.DaylightName, -1, _tzname[1],
            32, NULL, NULL);

    /* _timezone is in seconds.  tzinfo.Bias is in minutes. */

    _timezone = tzinfo.Bias * 60;
    _daylight = tzinfo.DaylightBias ? 1 : 0;
    return;
}

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
    int cmdLineSize;
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
    p = *cmdLine = PR_MALLOC(cmdLineSize);
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

        /* If the argument contains white space, it needs to be quoted. */
        if (strpbrk(*arg, " \f\n\r\t\v")) {
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
    int envBlockSize;

    if (envp == NULL) {
        *envBlock = NULL;
        return 0;
    }

    curEnv = GetEnvironmentStrings();

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

    p = *envBlock = PR_MALLOC(envBlockSize);
    if (p == NULL) {
        FreeEnvironmentStrings(curEnv);
        return -1;
    }

    q = cwdStart;
    while (q < cwdEnd) {
        *p++ = *q++;
    }
    FreeEnvironmentStrings(curEnv);

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
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION procInfo;
    BOOL retVal;
    char *cmdLine = NULL;
    char *envBlock = NULL;
    char **newEnvp = NULL;
    const char *cwd = NULL; /* current working directory */
    PRProcess *proc = NULL;

    proc = PR_NEW(PRProcess);
    if (!proc) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        goto errorExit;
    }

    if (assembleCmdLine(argv, &cmdLine) == -1) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        goto errorExit;
    }

    /*
     * If attr->fdInheritBuffer is not NULL, we need to insert
     * it into the envp array, so envp cannot be NULL.
     */
    if ((envp == NULL) && attr && attr->fdInheritBuffer) {
        envp = environ;
    }

    if (envp != NULL) {
        int idx;
        int numEnv;
        int newEnvpSize;

        numEnv = 0;
        while (envp[numEnv]) {
            numEnv++;
        }
        newEnvpSize = numEnv + 1;  /* terminating null pointer */
        if (attr && attr->fdInheritBuffer) {
            newEnvpSize++;
        }
        newEnvp = (char **) PR_MALLOC(newEnvpSize * sizeof(char *));
        for (idx = 0; idx < numEnv; idx++) {
            newEnvp[idx] = envp[idx];
        }
        if (attr && attr->fdInheritBuffer) {
            newEnvp[idx++] = attr->fdInheritBuffer;
        }
        newEnvp[idx] = NULL;
        qsort((void *) newEnvp, (size_t) (newEnvpSize - 1),
                sizeof(char *), compare);
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

    retVal = CreateProcess(NULL,
                           cmdLine,
                           NULL,  /* security attributes for the new
                                   * process */
                           NULL,  /* security attributes for the primary
                                   * thread in the new process */
                           TRUE,  /* inherit handles */
                           0,     /* creation flags */
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
    PRUint32    osfd;

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
        flProtect = PAGE_WRITECOPY;
        fmap->md.dwAccess = FILE_MAP_COPY;
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

#include "prlog.h"
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
  
  __asm__("lock xchg %0,%1"
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
  
  __asm__("lock xchg %0,%1"
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
