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

#include "primpl.h"
#include <sys/timeb.h>


/* 
** Create Process.
*/
PRProcess * _PR_CreateWindowsProcess(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr)
{
    PR_ASSERT(!"Not implemented");
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
}

PRStatus _PR_DetachWindowsProcess(PRProcess *process)
{
    PR_ASSERT(!"Not implemented");
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

PRStatus _PR_WaitWindowsProcess(PRProcess *process,
    PRInt32 *exitCode)
{
    PR_ASSERT(!"Not implemented");
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

PRStatus _PR_KillWindowsProcess(PRProcess *process)
{
    PR_ASSERT(!"Not implemented");
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

