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
 *********************************************************************
 *
 * Memory-mapped files
 *
 *********************************************************************
 */

#include "primpl.h"

PR_IMPLEMENT(PRFileMap *) PR_CreateFileMap(
    PRFileDesc *fd,
    PRInt64 size,
    PRFileMapProtect prot)
{
    PRFileMap *fmap;

    PR_ASSERT(prot == PR_PROT_READONLY || prot == PR_PROT_READWRITE
            || prot == PR_PROT_WRITECOPY);
    fmap = PR_NEWZAP(PRFileMap);
    if (NULL == fmap) {
	PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
	return NULL;
    }
    fmap->fd = fd;
    fmap->prot = prot;
    if (_PR_MD_CREATE_FILE_MAP(fmap, size) == PR_SUCCESS) {
	return fmap;
    } else {
	PR_DELETE(fmap);
	return NULL;
    }
}

PR_IMPLEMENT(PRInt32) PR_GetMemMapAlignment(void)
{
    return _PR_MD_GET_MEM_MAP_ALIGNMENT();
}

PR_IMPLEMENT(void *) PR_MemMap(
    PRFileMap *fmap,
    PROffset64 offset,
    PRUint32 len)
{
    return _PR_MD_MEM_MAP(fmap, offset, len);
}

PR_IMPLEMENT(PRStatus) PR_MemUnmap(void *addr, PRUint32 len)
{
    return _PR_MD_MEM_UNMAP(addr, len);
}

PR_IMPLEMENT(PRStatus) PR_CloseFileMap(PRFileMap *fmap)
{
    return _PR_MD_CLOSE_FILE_MAP(fmap);
}
