/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

PR_IMPLEMENT(PRStatus) PR_MakeDir(const char *name, PRIntn mode)
{
PRInt32 rv;

	if (!_pr_initialized) _PR_ImplicitInitialization();
	rv = _PR_MD_MAKE_DIR(name, mode);
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

#ifdef MOZ_UNICODE
/*
 *  UTF16 Interface
 */
PR_IMPLEMENT(PRDirUTF16*) PR_OpenDirUTF16(const PRUnichar *name)
{ 
    PRDirUTF16 *dir;
    PRStatus sts;

    dir = PR_NEW(PRDirUTF16);
    if (dir) {
        sts = _PR_MD_OPEN_DIR_UTF16(&dir->md,name);
        if (sts != PR_SUCCESS) {
            PR_DELETE(dir);
            return NULL;
        }
    } else {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }
    return dir;
}  
 
PR_IMPLEMENT(PRDirEntryUTF16*) PR_ReadDirUTF16(PRDirUTF16 *dir, PRDirFlags flags)
{ 
    /*
     * _MD_READ_DIR_UTF16 return a PRUnichar* to the name; allocation in
     * machine-dependent code
     */
    PRUnichar* name = _PR_MD_READ_DIR_UTF16(&dir->md, flags);
    dir->d.name = name;
    return name ? &dir->d : NULL;
} 
 
PR_IMPLEMENT(PRStatus) PR_CloseDirUTF16(PRDirUTF16 *dir)
{ 
    PRInt32 rv; 

    if (dir) {
        rv = _PR_MD_CLOSE_DIR_UTF16(&dir->md);
        PR_DELETE(dir);
        if (rv < 0)
	    return PR_FAILURE;
        else
	    return PR_SUCCESS;
    } 
    return PR_SUCCESS;
}

#endif /* MOZ_UNICODE */
