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

PR_IMPLEMENT(void *) PR_MemMap(
    PRFileMap *fmap,
    PRInt64 offset,
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
