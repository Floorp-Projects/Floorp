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
** Base class definitions for I/O (ref: prio.h)
**
** This class is a virtual base class. Construction must be done by a
** subclass, but the I/O operations can be done on a RCIO object reference.
*/

#if defined(_RCIO_H)
#else
#define _RCIO_H

#include "rcbase.h"
#include "rcnetdb.h"
#include "rcinrval.h"

#include "prio.h"

class RCFileInfo;

class PR_IMPLEMENT(RCIO): public RCBase
{
public:
    typedef enum {
        open = PR_TRANSMITFILE_KEEP_OPEN,   /* socket is left open after file
                                             * is transmitted. */
        close = PR_TRANSMITFILE_CLOSE_SOCKET/* socket is closed after file
                                             * is transmitted. */
    } FileDisposition;

    typedef enum {
        set = PR_SEEK_SET,                  /* Set to value specified */
        current = PR_SEEK_CUR,              /* Seek relative to current position */
        end = PR_SEEK_END                   /* seek past end of current eof */
    } Whence;

    typedef enum {
        recv = PR_SHUTDOWN_RCV,             /* receives will be disallowed */
        send = PR_SHUTDOWN_SEND,            /* sends will be disallowed */
        both = PR_SHUTDOWN_BOTH             /* sends & receives will be disallowed */
    } ShutdownHow;

public:
    virtual ~RCIO();

    virtual RCIO*       Accept(RCNetAddr* addr, const RCInterval& timeout) = 0;
    virtual PRInt32     AcceptRead(
                            RCIO **nd, RCNetAddr **raddr, void *buf,
                            PRSize amount, const RCInterval& timeout) = 0;
    virtual PRInt64     Available() = 0;
    virtual PRStatus    Bind(const RCNetAddr& addr) = 0;
    virtual PRStatus    Close() = 0;
    virtual PRStatus    Connect(
                            const RCNetAddr& addr,
                            const RCInterval& timeout) = 0;
    virtual PRStatus    FileInfo(RCFileInfo *info) const = 0;
    virtual PRStatus    Fsync() = 0;
    virtual PRStatus    GetLocalName(RCNetAddr *addr) const = 0;
    virtual PRStatus    GetPeerName(RCNetAddr *addr) const = 0;
    virtual PRStatus    GetSocketOption(PRSocketOptionData *data) const = 0;
    virtual PRStatus    Listen(PRIntn backlog) = 0;
    virtual PRStatus    Open(const char *name, PRIntn flags, PRIntn mode) = 0;
    virtual PRInt16     Poll(PRInt16 in_flags, PRInt16 *out_flags) = 0;
    virtual PRInt32     Read(void *buf, PRSize amount) = 0;
    virtual PRInt32     Recv(
                            void *buf, PRSize amount, PRIntn flags,
                            const RCInterval& timeout) = 0;
    virtual PRInt32     Recvfrom(
                            void *buf, PRSize amount, PRIntn flags,
                            RCNetAddr* addr, const RCInterval& timeout) = 0;
    virtual PRInt64     Seek(PRInt64 offset, Whence how) = 0;
    virtual PRInt32     Send(
                            const void *buf, PRSize amount, PRIntn flags,
                            const RCInterval& timeout) = 0;
    virtual PRInt32     Sendto(
                            const void *buf, PRSize amount, PRIntn flags,
                            const RCNetAddr& addr,
                            const RCInterval& timeout) = 0;
    virtual PRStatus    SetSocketOption(const PRSocketOptionData *data) = 0;
    virtual PRStatus    Shutdown(ShutdownHow how) = 0;
    virtual PRInt32     TransmitFile(
                            RCIO *source, const void *headers,
                            PRSize hlen, RCIO::FileDisposition flags,
                            const RCInterval& timeout) = 0;
    virtual PRInt32     Write(const void *buf, PRSize amount) = 0;
    virtual PRInt32     Writev(
                            const PRIOVec *iov, PRSize size,
                            const RCInterval& timeout) = 0;

protected:
    typedef enum {
        file = PR_DESC_FILE,
        tcp = PR_DESC_SOCKET_TCP,
        udp = PR_DESC_SOCKET_UDP,
        layered = PR_DESC_LAYERED} RCIOType;

    RCIO(RCIOType);

    PRFileDesc *fd;  /* where the real code hides */

private:
    /* no default construction and no copies allowed */
    RCIO();
    RCIO(const RCIO&);

};  /* RCIO */

#endif /* defined(_RCIO_H) */

/* RCIO.h */


