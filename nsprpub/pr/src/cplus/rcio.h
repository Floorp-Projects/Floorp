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


