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


