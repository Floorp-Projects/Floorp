/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** Subclass implementation for streamed network I/O (ref: prio.h)
*/

#include "rcnetio.h"

#include <private/pprio.h>

RCNetStreamIO::~RCNetStreamIO()
    { PRStatus rv = (fd->methods->close)(fd); fd = NULL; }

RCNetStreamIO::RCNetStreamIO(): RCIO(RCIO::tcp)
    { fd = PR_NewTCPSocket(); }

RCNetStreamIO::RCNetStreamIO(PRIntn protocol): RCIO(RCIO::tcp)
    { fd = PR_Socket(PR_AF_INET, PR_SOCK_STREAM, protocol); }

RCIO* RCNetStreamIO::Accept(RCNetAddr* addr, const RCInterval& timeout)
{
    PRNetAddr peer;
    RCNetStreamIO* rcio = NULL;
    PRFileDesc* newfd = fd->methods->accept(fd, &peer, timeout);
    if (NULL != newfd)
    {
        rcio = new RCNetStreamIO();
        if (NULL != rcio)
        {
            *addr = &peer;
            rcio->fd = newfd;
        }
        else
            (void)(newfd->methods->close)(newfd);
    }
    return rcio;
}  /* RCNetStreamIO::Accept */

PRInt32 RCNetStreamIO::AcceptRead(
    RCIO **nd, RCNetAddr **raddr, void *buf,
    PRSize amount, const RCInterval& timeout)
{   
    PRNetAddr *from;
    PRFileDesc *accepted;
    PRInt32 rv = (fd->methods->acceptread)(
        fd, &accepted, &from, buf, amount, timeout);
    if (rv >= 0)
    {
        RCNetStreamIO *ns = new RCNetStreamIO();
        if (NULL != *nd) ns->fd = accepted;
        else {PR_Close(accepted); rv = -1; }
        *nd = ns;
    }
    return rv;
}  /* RCNetStreamIO::AcceptRead */

PRInt64 RCNetStreamIO::Available()
    { return (fd->methods->available64)(fd); }

PRStatus RCNetStreamIO::Bind(const RCNetAddr& addr)
    { return (fd->methods->bind)(fd, addr); }

PRStatus RCNetStreamIO::Connect(const RCNetAddr& addr, const RCInterval& timeout)
    { return (fd->methods->connect)(fd, addr, timeout); }

PRStatus RCNetStreamIO::GetLocalName(RCNetAddr *addr) const
{
    PRNetAddr local;
    PRStatus rv = (fd->methods->getsockname)(fd, &local);
    if (PR_SUCCESS == rv) *addr = &local;
    return rv;
}  /* RCNetStreamIO::GetLocalName */

PRStatus RCNetStreamIO::GetPeerName(RCNetAddr *addr) const
{
    PRNetAddr peer;
    PRStatus rv = (fd->methods->getpeername)(fd, &peer);
    if (PR_SUCCESS == rv) *addr = &peer;
    return rv;
}  /* RCNetStreamIO::GetPeerName */

PRStatus RCNetStreamIO::GetSocketOption(PRSocketOptionData *data) const
    { return (fd->methods->getsocketoption)(fd, data); }

PRStatus RCNetStreamIO::Listen(PRIntn backlog)
    { return (fd->methods->listen)(fd, backlog); }

PRInt16 RCNetStreamIO::Poll(PRInt16 in_flags, PRInt16 *out_flags)
    { return (fd->methods->poll)(fd, in_flags, out_flags); }

PRInt32 RCNetStreamIO::Read(void *buf, PRSize amount)
    { return (fd->methods->read)(fd, buf, amount); }

PRInt32 RCNetStreamIO::Recv(
    void *buf, PRSize amount, PRIntn flags, const RCInterval& timeout)
    { return (fd->methods->recv)(fd, buf, amount, flags, timeout); }

PRInt32 RCNetStreamIO::Recvfrom(
    void *buf, PRSize amount, PRIntn flags,
    RCNetAddr* addr, const RCInterval& timeout)
{
    PRNetAddr peer;
    PRInt32 rv = (fd->methods->recvfrom)(
        fd, buf, amount, flags, &peer, timeout);
    if (-1 != rv) *addr = &peer;
    return rv;
}  /* RCNetStreamIO::Recvfrom */

PRInt32 RCNetStreamIO::Send(
    const void *buf, PRSize amount, PRIntn flags, const RCInterval& timeout)
    { return (fd->methods->send)(fd, buf, amount, flags, timeout); }

PRInt32 RCNetStreamIO::Sendto(
    const void *buf, PRSize amount, PRIntn flags,
    const RCNetAddr& addr, const RCInterval& timeout)
    { return (fd->methods->sendto)(fd, buf, amount, flags, addr, timeout); }

PRStatus RCNetStreamIO::SetSocketOption(const PRSocketOptionData *data)
    { return (fd->methods->setsocketoption)(fd, data); }

PRStatus RCNetStreamIO::Shutdown(RCIO::ShutdownHow how)
    { return (fd->methods->shutdown)(fd, (PRIntn)how); }

PRInt32 RCNetStreamIO::TransmitFile(
    RCIO *source, const void *headers, PRSize hlen,
    RCIO::FileDisposition flags, const RCInterval& timeout)
{
    RCNetStreamIO *src = (RCNetStreamIO*)source;
    return (fd->methods->transmitfile)(
        fd, src->fd, headers, hlen, (PRTransmitFileFlags)flags, timeout); }

PRInt32 RCNetStreamIO::Write(const void *buf, PRSize amount)
    { return (fd->methods->write)(fd, buf, amount); }

PRInt32 RCNetStreamIO::Writev(
    const PRIOVec *iov, PRSize size, const RCInterval& timeout)
    { return (fd->methods->writev)(fd, iov, size, timeout); }
    
/*
** Invalid functions
*/

PRStatus RCNetStreamIO::Close()
    { PR_SetError(PR_INVALID_METHOD_ERROR, 0); return PR_FAILURE; }

PRStatus RCNetStreamIO::FileInfo(RCFileInfo*) const
    { PR_SetError(PR_INVALID_METHOD_ERROR, 0); return PR_FAILURE; }

PRStatus RCNetStreamIO::Fsync()
    { return (fd->methods->fsync)(fd); }

PRStatus RCNetStreamIO::Open(const char*, PRIntn, PRIntn)
    { PR_SetError(PR_INVALID_METHOD_ERROR, 0); return PR_FAILURE; }

PRInt64 RCNetStreamIO::Seek(PRInt64, RCIO::Whence)
    { PR_SetError(PR_INVALID_METHOD_ERROR, 0); return PR_FAILURE; }

/* RCNetStreamIO.cpp */


