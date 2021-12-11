/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** Class implementation for normal and special file I/O (ref: prio.h)
*/

#include "rcfileio.h"

#include <string.h>

RCFileIO::RCFileIO(): RCIO(RCIO::file) { }

RCFileIO::~RCFileIO() {
    if (NULL != fd) {
        (void)Close();
    }
}

PRInt64 RCFileIO::Available()
{
    return fd->methods->available(fd);
}

PRStatus RCFileIO::Close()
{
    PRStatus rv = fd->methods->close(fd);
    fd = NULL;
    return rv;
}

PRStatus RCFileIO::Delete(const char* filename) {
    return PR_Delete(filename);
}

PRStatus RCFileIO::FileInfo(RCFileInfo* info) const
{
    return fd->methods->fileInfo64(fd, &info->info);
}

PRStatus RCFileIO::FileInfo(const char *name, RCFileInfo* info)
{
    return PR_GetFileInfo64(name, &info->info);
}

PRStatus RCFileIO::Fsync()
{
    return fd->methods->fsync(fd);
}

PRStatus RCFileIO::Open(const char *filename, PRIntn flags, PRIntn mode)
{
    fd = PR_Open(filename, flags, mode);
    return (NULL == fd) ? PR_FAILURE : PR_SUCCESS;
}  /* RCFileIO::Open */

PRInt32 RCFileIO::Read(void *buf, PRSize amount)
{
    return fd->methods->read(fd, buf, amount);
}

PRInt64 RCFileIO::Seek(PRInt64 offset, RCIO::Whence how)
{
    PRSeekWhence whence;
    switch (how)
    {
        case RCFileIO::set: whence = PR_SEEK_SET; break;
        case RCFileIO::current: whence = PR_SEEK_CUR; break;
        case RCFileIO::end: whence = PR_SEEK_END; break;
        default: whence = (PRSeekWhence)-1;
    }
    return fd->methods->seek64(fd, offset, whence);
}  /* RCFileIO::Seek */

PRInt32 RCFileIO::Write(const void *buf, PRSize amount)
{
    return fd->methods->write(fd, buf, amount);
}

PRInt32 RCFileIO::Writev(
    const PRIOVec *iov, PRSize size, const RCInterval& timeout)
{
    return fd->methods->writev(fd, iov, size, timeout);
}

RCIO *RCFileIO::GetSpecialFile(RCFileIO::SpecialFile special)
{
    PRFileDesc* fd;
    PRSpecialFD which;
    RCFileIO* spec = NULL;

    switch (special)
    {
        case RCFileIO::input: which = PR_StandardInput; break;
        case RCFileIO::output: which = PR_StandardOutput; break;
        case RCFileIO::error: which = PR_StandardError; break;
        default: which = (PRSpecialFD)-1;
    }
    fd = PR_GetSpecialFD(which);
    if (NULL != fd)
    {
        spec = new RCFileIO();
        if (NULL != spec) {
            spec->fd = fd;
        }
    }
    return spec;
}  /* RCFileIO::GetSpecialFile */


/*
** The following methods have been made non-virtual and private. These
** default implementations are intended to NEVER be called. They
** are not valid for this type of I/O class (normal and special file).
*/
PRStatus RCFileIO::Connect(const RCNetAddr&, const RCInterval&)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus RCFileIO::GetLocalName(RCNetAddr*) const
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus RCFileIO::GetPeerName(RCNetAddr*) const
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus RCFileIO::GetSocketOption(PRSocketOptionData*) const
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus RCFileIO::Listen(PRIntn)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRInt16 RCFileIO::Poll(PRInt16, PRInt16*)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return 0;
}

PRInt32 RCFileIO::Recv(void*, PRSize, PRIntn, const RCInterval&)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRInt32 RCFileIO::Recvfrom(void*, PRSize, PRIntn, RCNetAddr*, const RCInterval&)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRInt32 RCFileIO::Send(
    const void*, PRSize, PRIntn, const RCInterval&)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRInt32 RCFileIO::Sendto(
    const void*, PRSize, PRIntn, const RCNetAddr&, const RCInterval&)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

RCIO* RCFileIO::Accept(RCNetAddr*, const RCInterval&)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return NULL;
}

PRStatus RCFileIO::Bind(const RCNetAddr&)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRInt32 RCFileIO::AcceptRead(
    RCIO**, RCNetAddr**, void*, PRSize, const RCInterval&)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRStatus RCFileIO::SetSocketOption(const PRSocketOptionData*)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus RCFileIO::Shutdown(RCIO::ShutdownHow)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRInt32 RCFileIO::TransmitFile(
    RCIO*, const void*, PRSize, RCIO::FileDisposition, const RCInterval&)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

/*
** Class implementation for file information object (ref: prio.h)
*/

RCFileInfo::~RCFileInfo() { }

RCFileInfo::RCFileInfo(const RCFileInfo& her): RCBase()
{
    info = her.info;    /* RCFileInfo::RCFileInfo */
}

RCTime RCFileInfo::CreationTime() const {
    return RCTime(info.creationTime);
}

RCTime RCFileInfo::ModifyTime() const {
    return RCTime(info.modifyTime);
}

RCFileInfo::FileType RCFileInfo::Type() const
{
    RCFileInfo::FileType type;
    switch (info.type)
    {
        case PR_FILE_FILE: type = RCFileInfo::file; break;
        case PR_FILE_DIRECTORY: type = RCFileInfo::directory; break;
        default: type = RCFileInfo::other;
    }
    return type;
}  /* RCFileInfo::Type */
