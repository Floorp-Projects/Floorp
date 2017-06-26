/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    }
	PR_DELETE(fmap);
	return NULL;
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

PR_IMPLEMENT(PRStatus) PR_SyncMemMap(
    PRFileDesc *fd,
    void *addr,
    PRUint32 len)
{
  return _PR_MD_SYNC_MEM_MAP(fd, addr, len);
}
