/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "primpl.h"

#include <string.h>

/*****************************************************************************/
/************************** Invalid I/O method object ************************/
/*****************************************************************************/
PRIOMethods _pr_faulty_methods = {
    (PRDescType)0,
    (PRCloseFN)_PR_InvalidStatus,
    (PRReadFN)_PR_InvalidInt,
    (PRWriteFN)_PR_InvalidInt,
    (PRAvailableFN)_PR_InvalidInt,
    (PRAvailable64FN)_PR_InvalidInt64,
    (PRFsyncFN)_PR_InvalidStatus,
    (PRSeekFN)_PR_InvalidInt,
    (PRSeek64FN)_PR_InvalidInt64,
    (PRFileInfoFN)_PR_InvalidStatus,
    (PRFileInfo64FN)_PR_InvalidStatus,
    (PRWritevFN)_PR_InvalidInt,
    (PRConnectFN)_PR_InvalidStatus,
    (PRAcceptFN)_PR_InvalidDesc,
    (PRBindFN)_PR_InvalidStatus,
    (PRListenFN)_PR_InvalidStatus,
    (PRShutdownFN)_PR_InvalidStatus,
    (PRRecvFN)_PR_InvalidInt,
    (PRSendFN)_PR_InvalidInt,
    (PRRecvfromFN)_PR_InvalidInt,
    (PRSendtoFN)_PR_InvalidInt,
    (PRPollFN)_PR_InvalidInt16,
    (PRAcceptreadFN)_PR_InvalidInt,
    (PRTransmitfileFN)_PR_InvalidInt,
    (PRGetsocknameFN)_PR_InvalidStatus,
    (PRGetpeernameFN)_PR_InvalidStatus,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus,
    (PRSendfileFN)_PR_InvalidInt,
    (PRConnectcontinueFN)_PR_InvalidStatus,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt
};

PRIntn _PR_InvalidInt(void)
{
    PR_NOT_REACHED("I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}  /* _PR_InvalidInt */

PRInt16 _PR_InvalidInt16(void)
{
    PR_NOT_REACHED("I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}  /* _PR_InvalidInt */

PRInt64 _PR_InvalidInt64(void)
{
    PRInt64 rv;
    LL_I2L(rv, -1);
    PR_NOT_REACHED("I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return rv;
}  /* _PR_InvalidInt */

/*
 * An invalid method that returns PRStatus
 */

PRStatus _PR_InvalidStatus(void)
{
    PR_NOT_REACHED("I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}  /* _PR_InvalidDesc */

/*
 * An invalid method that returns a pointer
 */

PRFileDesc *_PR_InvalidDesc(void)
{
    PR_NOT_REACHED("I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return NULL;
}  /* _PR_InvalidDesc */

PR_IMPLEMENT(PRDescType) PR_GetDescType(PRFileDesc *file)
{
    return file->methods->file_type;
}

PR_IMPLEMENT(PRStatus) PR_Close(PRFileDesc *fd)
{
    return (fd->methods->close)(fd);
}

PR_IMPLEMENT(PRInt32) PR_Read(PRFileDesc *fd, void *buf, PRInt32 amount)
{
    return((fd->methods->read)(fd,buf,amount));
}

PR_IMPLEMENT(PRInt32) PR_Write(PRFileDesc *fd, const void *buf, PRInt32 amount)
{
    return((fd->methods->write)(fd,buf,amount));
}

PR_IMPLEMENT(PRInt32) PR_Seek(PRFileDesc *fd, PRInt32 offset, PRSeekWhence whence)
{
    return((fd->methods->seek)(fd, offset, whence));
}

PR_IMPLEMENT(PRInt64) PR_Seek64(PRFileDesc *fd, PRInt64 offset, PRSeekWhence whence)
{
    return((fd->methods->seek64)(fd, offset, whence));
}

PR_IMPLEMENT(PRInt32) PR_Available(PRFileDesc *fd)
{
    return((fd->methods->available)(fd));
}

PR_IMPLEMENT(PRInt64) PR_Available64(PRFileDesc *fd)
{
    return((fd->methods->available64)(fd));
}

PR_IMPLEMENT(PRStatus) PR_GetOpenFileInfo(PRFileDesc *fd, PRFileInfo *info)
{
    return((fd->methods->fileInfo)(fd, info));
}

PR_IMPLEMENT(PRStatus) PR_GetOpenFileInfo64(PRFileDesc *fd, PRFileInfo64 *info)
{
    return((fd->methods->fileInfo64)(fd, info));
}

PR_IMPLEMENT(PRStatus) PR_Sync(PRFileDesc *fd)
{
    return((fd->methods->fsync)(fd));
}

PR_IMPLEMENT(PRStatus) PR_Connect(
    PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
    return((fd->methods->connect)(fd,addr,timeout));
}

PR_IMPLEMENT(PRStatus) PR_ConnectContinue(
    PRFileDesc *fd, PRInt16 out_flags)
{
    return((fd->methods->connectcontinue)(fd,out_flags));
}

PR_IMPLEMENT(PRFileDesc*) PR_Accept(PRFileDesc *fd, PRNetAddr *addr,
                                    PRIntervalTime timeout)
{
    return((fd->methods->accept)(fd,addr,timeout));
}

PR_IMPLEMENT(PRStatus) PR_Bind(PRFileDesc *fd, const PRNetAddr *addr)
{
    return((fd->methods->bind)(fd,addr));
}

PR_IMPLEMENT(PRStatus) PR_Shutdown(PRFileDesc *fd, PRShutdownHow how)
{
    return((fd->methods->shutdown)(fd,how));
}

PR_IMPLEMENT(PRStatus) PR_Listen(PRFileDesc *fd, PRIntn backlog)
{
    return((fd->methods->listen)(fd,backlog));
}

PR_IMPLEMENT(PRInt32) PR_Recv(PRFileDesc *fd, void *buf, PRInt32 amount,
                              PRIntn flags, PRIntervalTime timeout)
{
    return((fd->methods->recv)(fd,buf,amount,flags,timeout));
}

PR_IMPLEMENT(PRInt32) PR_Send(PRFileDesc *fd, const void *buf, PRInt32 amount,
                              PRIntn flags, PRIntervalTime timeout)
{
    return((fd->methods->send)(fd,buf,amount,flags,timeout));
}

PR_IMPLEMENT(PRInt32) PR_Writev(PRFileDesc *fd, const PRIOVec *iov,
                                PRInt32 iov_size, PRIntervalTime timeout)
{
    if (iov_size > PR_MAX_IOVECTOR_SIZE)
    {
        PR_SetError(PR_BUFFER_OVERFLOW_ERROR, 0);
        return -1;
    }
    return((fd->methods->writev)(fd,iov,iov_size,timeout));
}

PR_IMPLEMENT(PRInt32) PR_RecvFrom(PRFileDesc *fd, void *buf, PRInt32 amount,
                                  PRIntn flags, PRNetAddr *addr, PRIntervalTime timeout)
{
    return((fd->methods->recvfrom)(fd,buf,amount,flags,addr,timeout));
}

PR_IMPLEMENT(PRInt32) PR_SendTo(
    PRFileDesc *fd, const void *buf, PRInt32 amount,
    PRIntn flags, const PRNetAddr *addr, PRIntervalTime timeout)
{
    return((fd->methods->sendto)(fd,buf,amount,flags,addr,timeout));
}

PR_IMPLEMENT(PRInt32) PR_TransmitFile(
    PRFileDesc *sd, PRFileDesc *fd, const void *hdr, PRInt32 hlen,
    PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    return((sd->methods->transmitfile)(sd,fd,hdr,hlen,flags,timeout));
}

PR_IMPLEMENT(PRInt32) PR_AcceptRead(
    PRFileDesc *sd, PRFileDesc **nd, PRNetAddr **raddr,
    void *buf, PRInt32 amount, PRIntervalTime timeout)
{
    return((sd->methods->acceptread)(sd, nd, raddr, buf, amount,timeout));
}

PR_IMPLEMENT(PRStatus) PR_GetSockName(PRFileDesc *fd, PRNetAddr *addr)
{
    return((fd->methods->getsockname)(fd,addr));
}

PR_IMPLEMENT(PRStatus) PR_GetPeerName(PRFileDesc *fd, PRNetAddr *addr)
{
    return((fd->methods->getpeername)(fd,addr));
}

PR_IMPLEMENT(PRStatus) PR_GetSocketOption(
    PRFileDesc *fd, PRSocketOptionData *data)
{
    return((fd->methods->getsocketoption)(fd, data));
}

PR_IMPLEMENT(PRStatus) PR_SetSocketOption(
    PRFileDesc *fd, const PRSocketOptionData *data)
{
    return((fd->methods->setsocketoption)(fd, data));
}

PR_IMPLEMENT(PRInt32) PR_SendFile(
    PRFileDesc *sd, PRSendFileData *sfd,
    PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    return((sd->methods->sendfile)(sd,sfd,flags,timeout));
}

PR_IMPLEMENT(PRInt32) PR_EmulateAcceptRead(
    PRFileDesc *sd, PRFileDesc **nd, PRNetAddr **raddr,
    void *buf, PRInt32 amount, PRIntervalTime timeout)
{
    PRInt32 rv = -1;
    PRNetAddr remote;
    PRFileDesc *accepted = NULL;

    /*
    ** The timeout does not apply to the accept portion of the
    ** operation - it waits indefinitely.
    */
    accepted = PR_Accept(sd, &remote, PR_INTERVAL_NO_TIMEOUT);
    if (NULL == accepted) {
        return rv;
    }

    rv = PR_Recv(accepted, buf, amount, 0, timeout);
    if (rv >= 0)
    {
        /* copy the new info out where caller can see it */
#define AMASK ((PRPtrdiff)7)  /* mask for alignment of PRNetAddr */
        PRPtrdiff aligned = (PRPtrdiff)buf + amount + AMASK;
        *raddr = (PRNetAddr*)(aligned & ~AMASK);
        memcpy(*raddr, &remote, PR_NETADDR_SIZE(&remote));
        *nd = accepted;
        return rv;
    }

    PR_Close(accepted);
    return rv;
}

/*
 * PR_EmulateSendFile
 *
 *    Send file sfd->fd across socket sd. If header/trailer are specified
 *    they are sent before and after the file, respectively.
 *
 *    PR_TRANSMITFILE_CLOSE_SOCKET flag - close socket after sending file
 *
 *    return number of bytes sent or -1 on error
 *
 */

#if defined(XP_UNIX) || defined(WIN32)

/*
 * An implementation based on memory-mapped files
 */

#define SENDFILE_MMAP_CHUNK (256 * 1024)

PR_IMPLEMENT(PRInt32) PR_EmulateSendFile(
    PRFileDesc *sd, PRSendFileData *sfd,
    PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    PRInt32 rv, count = 0;
    PRInt32 len, file_bytes, index = 0;
    PRFileInfo info;
    PRIOVec iov[3];
    PRFileMap *mapHandle = NULL;
    void *addr = (void*)0; /* initialized to some arbitrary value. Keeps compiler warnings down. */
    PRUint32 file_mmap_offset, alignment;
    PRInt64 zero64;
    PROffset64 file_mmap_offset64;
    PRUint32 addr_offset, mmap_len;

    /* Get file size */
    if (PR_SUCCESS != PR_GetOpenFileInfo(sfd->fd, &info)) {
        count = -1;
        goto done;
    }
    if (sfd->file_nbytes &&
        (info.size < (sfd->file_offset + sfd->file_nbytes))) {
        /*
         * there are fewer bytes in file to send than specified
         */
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        count = -1;
        goto done;
    }
    if (sfd->file_nbytes) {
        file_bytes = sfd->file_nbytes;
    }
    else {
        file_bytes = info.size - sfd->file_offset;
    }

    alignment = PR_GetMemMapAlignment();

    /* number of initial bytes to skip in mmap'd segment */
    addr_offset = sfd->file_offset % alignment;

    /* find previous mmap alignment boundary */
    file_mmap_offset = sfd->file_offset - addr_offset;

    /*
     * If the file is large, mmap and send the file in chunks so as
     * to not consume too much virtual address space
     */
    mmap_len = PR_MIN(file_bytes + addr_offset, SENDFILE_MMAP_CHUNK);
    len = mmap_len - addr_offset;

    /*
     * Map in (part of) file. Take care of zero-length files.
     */
    if (len) {
        LL_I2L(zero64, 0);
        mapHandle = PR_CreateFileMap(sfd->fd, zero64, PR_PROT_READONLY);
        if (!mapHandle) {
            count = -1;
            goto done;
        }
        LL_I2L(file_mmap_offset64, file_mmap_offset);
        addr = PR_MemMap(mapHandle, file_mmap_offset64, mmap_len);
        if (!addr) {
            count = -1;
            goto done;
        }
    }
    /*
     * send headers first, followed by the file
     */
    if (sfd->hlen) {
        iov[index].iov_base = (char *) sfd->header;
        iov[index].iov_len = sfd->hlen;
        index++;
    }
    if (len) {
        iov[index].iov_base = (char*)addr + addr_offset;
        iov[index].iov_len = len;
        index++;
    }
    if ((file_bytes == len) && (sfd->tlen)) {
        /*
         * all file data is mapped in; send the trailer too
         */
        iov[index].iov_base = (char *) sfd->trailer;
        iov[index].iov_len = sfd->tlen;
        index++;
    }
    rv = PR_Writev(sd, iov, index, timeout);
    if (len) {
        PR_MemUnmap(addr, mmap_len);
    }
    if (rv < 0) {
        count = -1;
        goto done;
    }

    PR_ASSERT(rv == sfd->hlen + len + ((len == file_bytes) ? sfd->tlen : 0));

    file_bytes -= len;
    count += rv;
    if (!file_bytes) {  /* header, file and trailer are sent */
        goto done;
    }

    /*
     * send remaining bytes of the file, if any
     */
    len = PR_MIN(file_bytes, SENDFILE_MMAP_CHUNK);
    while (len > 0) {
        /*
         * Map in (part of) file
         */
        file_mmap_offset = sfd->file_offset + count - sfd->hlen;
        PR_ASSERT((file_mmap_offset % alignment) == 0);

        LL_I2L(file_mmap_offset64, file_mmap_offset);
        addr = PR_MemMap(mapHandle, file_mmap_offset64, len);
        if (!addr) {
            count = -1;
            goto done;
        }
        rv = PR_Send(sd, addr, len, 0, timeout);
        PR_MemUnmap(addr, len);
        if (rv < 0) {
            count = -1;
            goto done;
        }

        PR_ASSERT(rv == len);
        file_bytes -= rv;
        count += rv;
        len = PR_MIN(file_bytes, SENDFILE_MMAP_CHUNK);
    }
    PR_ASSERT(0 == file_bytes);
    if (sfd->tlen) {
        rv = PR_Send(sd, sfd->trailer, sfd->tlen, 0, timeout);
        if (rv >= 0) {
            PR_ASSERT(rv == sfd->tlen);
            count += rv;
        } else {
            count = -1;
        }
    }
done:
    if (mapHandle) {
        PR_CloseFileMap(mapHandle);
    }
    if ((count >= 0) && (flags & PR_TRANSMITFILE_CLOSE_SOCKET)) {
        PR_Close(sd);
    }
    return count;
}

#else

PR_IMPLEMENT(PRInt32) PR_EmulateSendFile(
    PRFileDesc *sd, PRSendFileData *sfd,
    PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    PRInt32 rv, count = 0;
    PRInt32 rlen;
    const void * buffer;
    PRInt32 buflen;
    PRInt32 sendbytes, readbytes;
    char *buf;

#define _SENDFILE_BUFSIZE   (16 * 1024)

    buf = (char*)PR_MALLOC(_SENDFILE_BUFSIZE);
    if (buf == NULL) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return -1;
    }

    /*
     * send header first
     */
    buflen = sfd->hlen;
    buffer = sfd->header;
    while (buflen) {
        rv = PR_Send(sd, buffer, buflen, 0, timeout);
        if (rv < 0) {
            /* PR_Send() has invoked PR_SetError(). */
            rv = -1;
            goto done;
        } else {
            count += rv;
            buffer = (const void*) ((const char*)buffer + rv);
            buflen -= rv;
        }
    }

    /*
     * send file next
     */
    if (PR_Seek(sfd->fd, sfd->file_offset, PR_SEEK_SET) < 0) {
        rv = -1;
        goto done;
    }
    sendbytes = sfd->file_nbytes;
    if (sendbytes == 0) {
        /* send entire file */
        while ((rlen = PR_Read(sfd->fd, buf, _SENDFILE_BUFSIZE)) > 0) {
            while (rlen) {
                char *bufptr = buf;

                rv =  PR_Send(sd, bufptr, rlen, 0, timeout);
                if (rv < 0) {
                    /* PR_Send() has invoked PR_SetError(). */
                    rv = -1;
                    goto done;
                } else {
                    count += rv;
                    bufptr = ((char*)bufptr + rv);
                    rlen -= rv;
                }
            }
        }
        if (rlen < 0) {
            /* PR_Read() has invoked PR_SetError(). */
            rv = -1;
            goto done;
        }
    } else {
        readbytes = PR_MIN(sendbytes, _SENDFILE_BUFSIZE);
        while (readbytes && ((rlen = PR_Read(sfd->fd, buf, readbytes)) > 0)) {
            while (rlen) {
                char *bufptr = buf;

                rv =  PR_Send(sd, bufptr, rlen, 0, timeout);
                if (rv < 0) {
                    /* PR_Send() has invoked PR_SetError(). */
                    rv = -1;
                    goto done;
                } else {
                    count += rv;
                    sendbytes -= rv;
                    bufptr = ((char*)bufptr + rv);
                    rlen -= rv;
                }
            }
            readbytes = PR_MIN(sendbytes, _SENDFILE_BUFSIZE);
        }
        if (rlen < 0) {
            /* PR_Read() has invoked PR_SetError(). */
            rv = -1;
            goto done;
        } else if (sendbytes != 0) {
            /*
             * there are fewer bytes in file to send than specified
             */
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            rv = -1;
            goto done;
        }
    }

    /*
     * send trailer last
     */
    buflen = sfd->tlen;
    buffer = sfd->trailer;
    while (buflen) {
        rv =  PR_Send(sd, buffer, buflen, 0, timeout);
        if (rv < 0) {
            /* PR_Send() has invoked PR_SetError(). */
            rv = -1;
            goto done;
        } else {
            count += rv;
            buffer = (const void*) ((const char*)buffer + rv);
            buflen -= rv;
        }
    }
    rv = count;

done:
    if (buf) {
        PR_DELETE(buf);
    }
    if ((rv >= 0) && (flags & PR_TRANSMITFILE_CLOSE_SOCKET)) {
        PR_Close(sd);
    }
    return rv;
}

#endif

/* priometh.c */
