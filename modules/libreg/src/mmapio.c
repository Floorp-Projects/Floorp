/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator.
 * 
 * The Initial Developer of the Original Code is James L. Nance.
 * Portions created by James L. Nance are Copyright (C) 1999, James
 * L. Nance.  All Rights Reserved.
 * 
 * Contributor(s):  James L. Nance <jim_nance@yahoo.com>
 */
#include <string.h>
#include "mmapio.h"
#include "prmem.h"
#include "prlog.h"

struct MmioFileStruct
{
    PRFileDesc *fd;
    PRFileMap  *fileMap;
    PRUint32   fsize; /* The size of the file */
    PRUint32   msize; /* The size of the mmap()ed area */
    PRInt32    pos;   /* Our logical position for doing I/O */
    char       *addr; /* The base address of our mapping */
    PRBool     needSeek; /* Do we need to seek to pos before doing a write() */
};

PRStatus mmio_FileSeek(MmioFile *mmio, PRInt32 offset, PRSeekWhence whence)
{
    mmio->needSeek = PR_TRUE;

    switch(whence) {
        case PR_SEEK_SET:
            mmio->pos = offset;
            break;
        case PR_SEEK_END:
            mmio->pos = mmio->fsize + offset;
            break;
        case PR_SEEK_CUR:
            mmio->pos = mmio->pos + offset;
            break;
        default:
            return PR_FAILURE;
    }

    if(mmio->pos<0) {
        mmio->pos = 0;
    }

    return PR_SUCCESS;
}

PRInt32  mmio_FileRead(MmioFile *mmio, char *dest, PRInt32 count)
{
    static PRFileMapProtect prot = PR_PROT_READONLY;
    static PRInt64 fsize_l;

    /* First see if we are going to try and read past the end of the file
     * and shorten count if we are.
    */
    if(mmio->pos+count > mmio->fsize) {
        count = mmio->fsize - mmio->pos;
    }

    if(count<1) {
        return 0;
    }

    /* Check to see if we need to remap for this read */
    if(mmio->pos+count > mmio->msize) {
        if(mmio->addr && mmio->msize) {
            PR_ASSERT(mmio->fileMap);
            PR_MemUnmap(mmio->addr, mmio->msize);
            PR_CloseFileMap(mmio->fileMap);
            mmio->addr  = NULL;
            mmio->msize = 0;
        }

        LL_UI2L(fsize_l, mmio->fsize);
        mmio->fileMap = PR_CreateFileMap(mmio->fd, fsize_l, prot);

        if(!mmio->fileMap) {
            return -1;
        }

        mmio->addr = PR_MemMap(mmio->fileMap, 0, fsize_l);

        if(!mmio->addr) {
            return -1;
        }

        mmio->msize = mmio->fsize;
    }

    memcpy(dest, mmio->addr+mmio->pos, count);

    mmio->pos += count;
    mmio->needSeek = PR_TRUE;

    return count;
}

PRInt32  mmio_FileWrite(MmioFile *mmio, const char *src, PRInt32 count)
{
    PRInt32 wcode;

    if(mmio->needSeek) {
        PR_Seek(mmio->fd, mmio->pos, PR_SEEK_SET);
        mmio->needSeek = PR_FALSE;
    }

    /* If this system does not keep mmap() and write() synchronized, we can
    ** force it to by doing an munmap() when we do a write.  This will
    ** obviously slow things down but fortunatly we do not do that many
    ** writes from within mozilla.  Platforms which need this may want to
    ** use the new USE_BUFFERED_REGISTRY_IO code instead of this code though.
    */
#if MMAP_MISSES_WRITES
    if(mmio->addr && mmio->msize) {
	PR_ASSERT(mmio->fileMap);
	PR_MemUnmap(mmio->addr, mmio->msize);
	PR_CloseFileMap(mmio->fileMap);
	mmio->addr  = NULL;
	mmio->msize = 0;
    }
#endif

    wcode = PR_Write(mmio->fd, src, count);

    if(wcode>0) {
        mmio->pos += wcode;
        if(mmio->pos>mmio->fsize) {
            mmio->fsize=mmio->pos;
        }
    }

    return wcode;
}

PRInt32  mmio_FileTell(MmioFile *mmio)
{
    return mmio->pos;
}

PRStatus mmio_FileClose(MmioFile *mmio)
{
    if(mmio->addr && mmio->msize) {
        PR_ASSERT(mmio->fileMap);
        PR_MemUnmap(mmio->addr, mmio->msize);
        PR_CloseFileMap(mmio->fileMap);
    }

    PR_Close(mmio->fd);

    memset(mmio, 0, sizeof(*mmio)); /* Catch people who try to keep using it */

    PR_Free(mmio);

    return PR_SUCCESS;
}

MmioFile *mmio_FileOpen(char *path, PRIntn flags, PRIntn mode)
{
    PRFileDesc *fd = PR_Open(path, flags, mode);
    PRFileInfo info;
    MmioFile   *mmio;

    if(!fd) {
        return NULL;
    }

    mmio = PR_MALLOC(sizeof(MmioFile));

    if(!mmio || PR_FAILURE==PR_GetOpenFileInfo(fd, &info)) {
        PR_Close(fd);
        return NULL;
    }

    mmio->fd = fd;
    mmio->fileMap = NULL;
    mmio->fsize = info.size;
    mmio->msize = 0;
    mmio->pos   = 0;
    mmio->addr  = NULL;
    mmio->needSeek = PR_FALSE;

    return mmio;
}
