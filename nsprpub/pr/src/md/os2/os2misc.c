/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * os2misc.c
 *
 */
#include <string.h>
#include "primpl.h"

char *
_PR_MD_GET_ENV(const char *name)
{
    return getenv(name);
}

PRIntn
_PR_MD_PUT_ENV(const char *name)
{
    return putenv(name);
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
 *     implementation for OS/2.
 *     Cf. time_t time(time_t *tp)
 *
 *-----------------------------------------------------------------------
 */

PR_IMPLEMENT(PRTime)
PR_Now(void)
{
    PRInt64 s, ms, ms2us, s2us;
    struct timeb b;

    ftime(&b);
    LL_I2L(ms2us, PR_USEC_PER_MSEC);
    LL_I2L(s2us, PR_USEC_PER_SEC);
    LL_I2L(s, b.time);
    LL_I2L(ms, b.millitm);
    LL_MUL(ms, ms, ms2us);
    LL_MUL(s, s, s2us);
    LL_ADD(s, s, ms);
    return s;       
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
         * Finally, we need a space between arguments, a null between
         * the EXE name and the arguments, and 2 nulls at the end 
         * of command line.
         */
        cmdLineSize += 2 * strlen(*arg)  /* \ and " need to be escaped */
                + 2                      /* we quote every argument */
                + 4;                     /* space in between, or final nulls */
    }
    p = *cmdLine = PR_MALLOC(cmdLineSize);
    if (p == NULL) {
        return -1;
    }

    for (arg = argv; *arg; arg++) {
        /* Add a space to separates the arguments */
        if (arg > argv + 1) {
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
        if(arg == argv)
           *p++ = '\0';
    } 

    /* Add 2 nulls at the end */
    *p++ = '\0';
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

    PPIB ppib = NULL;
    PTIB ptib = NULL;

    if (envp == NULL) {
        *envBlock = NULL;
        return 0;
    }

    if(DosGetInfoBlocks(&ptib, &ppib) != NO_ERROR)
       return -1;

    curEnv = ppib->pib_pchenv;

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
        return -1;
    }

    q = cwdStart;
    while (q < cwdEnd) {
        *p++ = *q++;
    }

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
    return stricmp(* (char**)arg1, * (char**)arg2);
}
PRProcess * _PR_CreateOS2Process(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr)
{
   char szFailed[CCHMAXPATH];
   RESULTCODES procInfo;
   APIRET retVal;
   char *cmdLine = NULL;
   char *envBlock = NULL;
   char **newEnvp;
   PRProcess *proc = NULL;
   HFILE hStdIn  = 0,
           hStdOut = 0, 
           hStdErr = 0;


   proc = PR_NEW(PRProcess);
   if (!proc) {
       PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
       goto errorExit;
   }

   if (assembleCmdLine(argv, &cmdLine) == -1) {
       PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
       goto errorExit;
   }

   if (envp == NULL) {
       newEnvp = NULL;
   } else {
       int i;
       int numEnv = 0;
       while (envp[numEnv]) {
           numEnv++;
       }
       newEnvp = (char **) PR_MALLOC((numEnv+1) * sizeof(char *));
       for (i = 0; i <= numEnv; i++) {
           newEnvp[i] = envp[i];
       }
       qsort((void *) newEnvp, (size_t) numEnv, sizeof(char *), compare);
   }
   if (assembleEnvBlock(newEnvp, &envBlock) == -1) {
       PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
       goto errorExit;
   }

   if (attr) {
      /* On OS/2, there is really no way to pass file handles for stdin, stdout, 
       * and stderr to a new process.  Instead, we can make it a child process       
       * and make the given file handles a copy of our stdin, stdout, and stderr.
       * The child process then inherits ours, and we set ours back.  Twisted
       * and gross I know. If you know a better way, please use it.
       */
       if (attr->stdinFd) {
            hStdIn = (HFILE) attr->stdinFd->secret->md.osfd;
            DosDupHandle(0, &hStdIn);
        }
        if (attr->stdoutFd) {
            hStdOut = (HFILE) attr->stdoutFd->secret->md.osfd;
            DosDupHandle(1, &hStdOut);
        }
        if (attr->stderrFd) {
            hStdErr = (HFILE) attr->stderrFd->secret->md.osfd;
            DosDupHandle(2, &hStdErr);
        }
   }

   retVal = DosExecPgm(szFailed, 
                       CCHMAXPATH, 
                       EXEC_ASYNCRESULT, 
                       cmdLine, 
                       envBlock, 
                       &procInfo,
                       argv[0]);

   /* Restore our old values.  Hope this works */
   if(hStdIn){
      hStdIn = 0;
      DosDupHandle(0, &hStdIn);      
   }
   if(hStdOut){
      hStdOut = 1;
      DosDupHandle(1, &hStdOut);      
   }
   if(hStdErr){
      hStdErr = 1;
      DosDupHandle(0, &hStdErr);      
   }

   if (retVal != NO_ERROR) {
       /* XXX what error code? */
       PR_SetError(PR_UNKNOWN_ERROR, retVal);
       goto errorExit;
   }

   proc->md.pid = procInfo.codeTerminate;

   PR_DELETE(cmdLine);
   if (envBlock) {
       PR_DELETE(envBlock);
   }
   return proc;

errorExit:
   if (cmdLine) {
       PR_DELETE(cmdLine);
   }
   if (envBlock) {
       PR_DELETE(envBlock);
   }
   if (proc) {
       PR_DELETE(proc);
   }
   return NULL;
    
}  /* _PR_CreateWindowsProcess */

PRStatus _PR_DetachOS2Process(PRProcess *process)
{
    /* This is basically what they did on Windows (CloseHandle)
     * but I don't think it will do much on OS/2. A process is
     * either created as a child or not.  You can't 'detach' it
     * later on.
     */
    DosClose(process->md.pid);
    PR_DELETE(process);
    return PR_SUCCESS;
}

/*
 * XXX: This will currently only work on a child process.
 */
PRStatus _PR_WaitOS2Process(PRProcess *process,
    PRInt32 *exitCode)
{
    ULONG ulRetVal;
    RESULTCODES results;
    PID pidEnded = 0;

    ulRetVal = DosWaitChild(DCWA_PROCESS, DCWW_WAIT, 
                            &results,
                            &pidEnded, process->md.pid);

    if (ulRetVal != NO_ERROR) {
       printf("\nDosWaitChild rc = %lu\n", ulRetVal);
        PR_SetError(PR_UNKNOWN_ERROR, ulRetVal);
        return PR_FAILURE;
    }
    PR_DELETE(process);
    return PR_SUCCESS;
}

PRStatus _PR_KillOS2Process(PRProcess *process)
{
   ULONG ulRetVal;
    if ((ulRetVal = DosKillProcess(DKP_PROCESS, process->md.pid)) == NO_ERROR) {
	return PR_SUCCESS;
    }
    PR_SetError(PR_UNKNOWN_ERROR, ulRetVal);
    return PR_FAILURE;
}

PRStatus _MD_OS2GetHostName(char *name, PRUint32 namelen)
{
    PRIntn rv;

    rv = gethostname(name, (PRInt32) namelen);
    if (0 == rv) {
        return PR_SUCCESS;
    }
	_PR_MD_MAP_GETHOSTNAME_ERROR(sock_errno());
    return PR_FAILURE;
}

PR_IMPLEMENT(void)
_PR_MD_WAKEUP_CPUS( void )
{
    return;
}    


/*
 **********************************************************************
 *
 * Memory-mapped files are not supported on OS/2 (or Win16).
 *
 **********************************************************************
 */

PRStatus _MD_CreateFileMap(PRFileMap *fmap, PRInt64 size)
{
    PR_ASSERT(!"Not implemented");
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

void * _MD_MemMap(
    PRFileMap *fmap,
    PRInt64 offset,
    PRUint32 len)
{
    PR_ASSERT(!"Not implemented");
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
}

PRStatus _MD_MemUnmap(void *addr, PRUint32 len)
{
    PR_ASSERT(!"Not implemented");
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

PRStatus _MD_CloseFileMap(PRFileMap *fmap)
{
    PR_ASSERT(!"Not implemented");
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

