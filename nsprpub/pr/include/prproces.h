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

#ifndef prproces_h___
#define prproces_h___

#include "prtypes.h"
#include "prio.h"

PR_BEGIN_EXTERN_C

/************************************************************************/
/*****************************PROCESS OPERATIONS*************************/
/************************************************************************/

typedef struct PRProcess PRProcess;
typedef struct PRProcessAttr PRProcessAttr;

NSPR_API(PRProcessAttr *) PR_NewProcessAttr(void);

NSPR_API(void) PR_ResetProcessAttr(PRProcessAttr *attr);

NSPR_API(void) PR_DestroyProcessAttr(PRProcessAttr *attr);

NSPR_API(void) PR_ProcessAttrSetStdioRedirect(
    PRProcessAttr *attr,
    PRSpecialFD stdioFd,
    PRFileDesc *redirectFd
);

/*
 * OBSOLETE -- use PR_ProcessAttrSetStdioRedirect instead.
 */
NSPR_API(void) PR_SetStdioRedirect(
    PRProcessAttr *attr,
    PRSpecialFD stdioFd,
    PRFileDesc *redirectFd
);

NSPR_API(PRStatus) PR_ProcessAttrSetCurrentDirectory(
    PRProcessAttr *attr,
    const char *dir
);

NSPR_API(PRStatus) PR_ProcessAttrSetInheritableFD(
    PRProcessAttr *attr,
    PRFileDesc *fd,
    const char *name
);

/*
** Create a new process
**
** Create a new process executing the file specified as 'path' and with
** the supplied arguments and environment.
**
** This function may fail because of illegal access (permissions),
** invalid arguments or insufficient resources.
**
** A process may be created such that the creator can later synchronize its
** termination using PR_WaitProcess(). 
*/

NSPR_API(PRProcess*) PR_CreateProcess(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr);

NSPR_API(PRStatus) PR_CreateProcessDetached(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr);

NSPR_API(PRStatus) PR_DetachProcess(PRProcess *process);

NSPR_API(PRStatus) PR_WaitProcess(PRProcess *process, PRInt32 *exitCode);

NSPR_API(PRStatus) PR_KillProcess(PRProcess *process);

PR_END_EXTERN_C

#endif /* prproces_h___ */
