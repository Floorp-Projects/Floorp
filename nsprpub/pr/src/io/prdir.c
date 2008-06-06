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
 *   Roy Yokoyama <yokoyama@netscape.com>
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
