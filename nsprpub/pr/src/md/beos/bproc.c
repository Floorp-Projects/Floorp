/* -*- Mode: C++; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http:// www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 */

#include "primpl.h"

PRProcess*
_MD_create_process (const char *path, char *const *argv,
		    char *const *envp, const PRProcessAttr *attr)
{
    return NULL;
}

PRStatus
_MD_detach_process (PRProcess *process)
{
    return PR_NOT_IMPLEMENTED_ERROR;
}

PRStatus
_MD_wait_process (PRProcess *process, PRInt32 *exitCode)
{
    return PR_NOT_IMPLEMENTED_ERROR;
}

PRStatus
_MD_kill_process (PRProcess *process)
{
    return PR_NOT_IMPLEMENTED_ERROR;
}
