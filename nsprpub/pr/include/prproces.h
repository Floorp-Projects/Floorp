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

PR_EXTERN(PRProcessAttr *) PR_NewProcessAttr(void);

PR_EXTERN(void) PR_ResetProcessAttr(PRProcessAttr *attr);

PR_EXTERN(void) PR_DestroyProcessAttr(PRProcessAttr *attr);

PR_EXTERN(void) PR_ProcessAttrSetStdioRedirect(
    PRProcessAttr *attr,
    PRSpecialFD stdioFd,
    PRFileDesc *redirectFd
);

/*
 * OBSOLETE -- use PR_ProcessAttrSetStdioRedirect instead.
 */
PR_EXTERN(void) PR_SetStdioRedirect(
    PRProcessAttr *attr,
    PRSpecialFD stdioFd,
    PRFileDesc *redirectFd
);

PR_EXTERN(PRStatus) PR_ProcessAttrSetCurrentDirectory(
    PRProcessAttr *attr,
    const char *dir
);

PR_EXTERN(PRStatus) PR_ProcessAttrSetInheritableFD(
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

PR_EXTERN(PRProcess*) PR_CreateProcess(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr);

PR_EXTERN(PRStatus) PR_CreateProcessDetached(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr);

PR_EXTERN(PRStatus) PR_DetachProcess(PRProcess *process);

PR_EXTERN(PRStatus) PR_WaitProcess(PRProcess *process, PRInt32 *exitCode);

PR_EXTERN(PRStatus) PR_KillProcess(PRProcess *process);

PR_END_EXTERN_C

#endif /* prproces_h___ */
