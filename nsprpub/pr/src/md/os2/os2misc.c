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
 *   Davide Bresolin <davide@teamos2.it>
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
 * os2misc.c
 *
 */
#include <string.h>
#include "primpl.h"

extern int   _CRT_init(void);
extern void  _CRT_term(void);
extern void __ctordtorInit(int flag);
extern void __ctordtorTerm(int flag);

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
    int cmdLineSize;

    /*
     * Find out how large the command line buffer should be.
     */
    cmdLineSize = 1; /* final null */
    for (arg = argv+1; *arg; arg++) {
        cmdLineSize += strlen(*arg) + 1; /* space in between */
    }
    *cmdLine = PR_MALLOC(cmdLineSize);
    if (*cmdLine == NULL) {
        return -1;
    }

    (*cmdLine)[0] = '\0';

    for (arg = argv+1; *arg; arg++) {
        if (arg > argv +1) {
            strcat(*cmdLine, " ");
        }
        strcat(*cmdLine, *arg);
    } 
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
    PRProcess *proc = NULL;
    char *cmdLine = NULL;
    char **newEnvp = NULL;
    char *envBlock = NULL;
   
    STARTDATA startData = {0};
    APIRET    rc;
    ULONG     ulAppType = 0;
    PID       pid = 0;
    char     *pEnvWPS = NULL;
    char     *pszComSpec;
    char      pszEXEName[CCHMAXPATH] = "";
    char      pszFormatString[CCHMAXPATH];
    char      pszObjectBuffer[CCHMAXPATH];
    char     *pszFormatResult = NULL;

    /*
     * Variables for DosExecPgm
     */
    char szFailed[CCHMAXPATH];
    char *pszCmdLine = NULL;
    RESULTCODES procInfo;
    HFILE hStdIn  = 0,
          hStdOut = 0,
          hStdErr = 0;
    HFILE hStdInSave  = -1,
          hStdOutSave = -1,
          hStdErrSave = -1;

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
  
    rc = DosQueryAppType(path, &ulAppType);
    if (rc != NO_ERROR) {
       char *pszDot = strrchr(path, '.');
       if (pszDot) {
          /* If it is a CMD file, launch the users command processor */
          if (!stricmp(pszDot, ".cmd")) {
             rc = DosScanEnv("COMSPEC", &pszComSpec);
             if (!rc) {
                strcpy(pszFormatString, "/C %s %s");
                strcpy(pszEXEName, pszComSpec);
                ulAppType = FAPPTYP_WINDOWCOMPAT;
             }
          }
       }
    }
    if (ulAppType == 0) {
       PR_SetError(PR_UNKNOWN_ERROR, 0);
       goto errorExit;
    }
 
    if ((ulAppType & FAPPTYP_WINDOWAPI) == FAPPTYP_WINDOWAPI) {
        startData.SessionType = SSF_TYPE_PM;
    }
    else if (ulAppType & FAPPTYP_WINDOWCOMPAT) {
        startData.SessionType = SSF_TYPE_WINDOWABLEVIO;
    }
    else {
        startData.SessionType = SSF_TYPE_DEFAULT;
    }
 
    if (ulAppType & (FAPPTYP_WINDOWSPROT31 | FAPPTYP_WINDOWSPROT | FAPPTYP_WINDOWSREAL))
    {
        strcpy(pszEXEName, "WINOS2.COM");
        startData.SessionType = PROG_31_STDSEAMLESSVDM;
        strcpy(pszFormatString, "/3 %s %s");
    }
 
    startData.InheritOpt = SSF_INHERTOPT_SHELL;
 
    if (pszEXEName[0]) {
        pszFormatResult = PR_MALLOC(strlen(pszFormatString)+strlen(path)+strlen(cmdLine));
        sprintf(pszFormatResult, pszFormatString, path, cmdLine);
        startData.PgmInputs = pszFormatResult;
    } else {
        strcpy(pszEXEName, path);
        startData.PgmInputs = cmdLine;
    }
    startData.PgmName = pszEXEName;
 
    startData.Length = sizeof(startData);
    startData.Related = SSF_RELATED_INDEPENDENT;
    startData.ObjectBuffer = pszObjectBuffer;
    startData.ObjectBuffLen = CCHMAXPATH;
    startData.Environment = envBlock;
 
    if (attr) {
        /* On OS/2, there is really no way to pass file handles for stdin,
         * stdout, and stderr to a new process.  Instead, we can make it
         * a child process and make the given file handles a copy of our
         * stdin, stdout, and stderr.  The child process then inherits
         * ours, and we set ours back.  Twisted and gross I know. If you
         * know a better way, please use it.
         */
        if (attr->stdinFd) {
            hStdIn = 0;
            DosDupHandle(hStdIn, &hStdInSave);
            DosDupHandle((HFILE) attr->stdinFd->secret->md.osfd, &hStdIn);
        }

        if (attr->stdoutFd) {
            hStdOut = 1;
            DosDupHandle(hStdOut, &hStdOutSave);
            DosDupHandle((HFILE) attr->stdoutFd->secret->md.osfd, &hStdOut);
        }

        if (attr->stderrFd) {
            hStdErr = 2;
            DosDupHandle(hStdErr, &hStdErrSave);
            DosDupHandle((HFILE) attr->stderrFd->secret->md.osfd, &hStdErr);
        }
        /*
         * Build up the Command Line for DosExecPgm
         */
        pszCmdLine = PR_MALLOC(strlen(pszEXEName) +
                               strlen(startData.PgmInputs) + 3);
        sprintf(pszCmdLine, "%s%c%s%c", pszEXEName, '\0',
                startData.PgmInputs, '\0');
        rc = DosExecPgm(szFailed,
                        CCHMAXPATH,
                        EXEC_ASYNCRESULT,
                        pszCmdLine,
                        envBlock,
                        &procInfo,
                        pszEXEName);
        PR_DELETE(pszCmdLine);

        /* Restore our old values.  Hope this works */
        if (hStdInSave != -1) {
            DosDupHandle(hStdInSave, &hStdIn);
            DosClose(hStdInSave);
        }

        if (hStdOutSave != -1) {
            DosDupHandle(hStdOutSave, &hStdOut);
            DosClose(hStdOutSave);
        }

        if (hStdErrSave != -1) {
            DosDupHandle(hStdErrSave, &hStdErr);
            DosClose(hStdErrSave);
        }

        if (rc != NO_ERROR) {
            /* XXX what error code? */
            PR_SetError(PR_UNKNOWN_ERROR, rc);
            goto errorExit;
        }

        proc->md.pid = procInfo.codeTerminate;
    } else {	
        /*
         * If no STDIN/STDOUT redirection is not needed, use DosStartSession
         * to create a new, independent session
         */
        rc = DosStartSession(&startData, &ulAppType, &pid);

        if ((rc != NO_ERROR) && (rc != ERROR_SMG_START_IN_BACKGROUND)) {
            PR_SetError(PR_UNKNOWN_ERROR, rc);
            goto errorExit;
        }
 
        proc->md.pid = pid;
    }

    if (pszFormatResult) {
        PR_DELETE(pszFormatResult);
    }

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
}  /* _PR_CreateOS2Process */

PRStatus _PR_DetachOS2Process(PRProcess *process)
{
    /* On OS/2, a process is either created as a child or not. 
     * You can't 'detach' it later on.
     */
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

void
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
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

PRInt32 _MD_GetMemMapAlignment(void)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return -1;
}

void * _MD_MemMap(
    PRFileMap *fmap,
    PROffset64 offset,
    PRUint32 len)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
}

PRStatus _MD_MemUnmap(void *addr, PRUint32 len)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

PRStatus _MD_CloseFileMap(PRFileMap *fmap)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

