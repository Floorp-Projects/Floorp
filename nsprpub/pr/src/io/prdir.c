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

PR_IMPLEMENT(PRDir*) PR_OpenDir(const char *name)
{
    PRDir *dir;
    PRStatus sts;

    dir = PR_NEW(PRDir);
    if (dir) {
        sts = _PR_MD_OPEN_DIR(&dir->md,name);
        if (sts != PR_SUCCESS) {
            PR_DELETE(dir);
            return NULL;
        }
    } else {
		PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
	}
    return dir;
}

PR_IMPLEMENT(PRDirEntry*) PR_ReadDir(PRDir *dir, PRDirFlags flags)
{
    /* _MD_READ_DIR return a char* to the name; allocation in machine-dependent code */
    char* name = _PR_MD_READ_DIR(&dir->md, flags);
    dir->d.name = name;
    return name ? &dir->d : NULL;
}

PR_IMPLEMENT(PRStatus) PR_CloseDir(PRDir *dir)
{
PRInt32 rv;

    if (dir) {
        rv = _PR_MD_CLOSE_DIR(&dir->md);
		PR_DELETE(dir);
		if (rv < 0) {
			return PR_FAILURE;
		} else
			return PR_SUCCESS;
    }
	return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus) PR_MkDir(const char *name, PRIntn mode)
{
PRInt32 rv;

	rv = _PR_MD_MKDIR(name, mode);
	if (rv < 0) {
		return PR_FAILURE;
	} else
		return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus) PR_RmDir(const char *name)
{
PRInt32 rv;

	rv = _PR_MD_RMDIR(name);
	if (rv < 0) {
		return PR_FAILURE;
	} else
		return PR_SUCCESS;
}

