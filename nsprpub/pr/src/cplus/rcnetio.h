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
** Subclass definitions for network I/O (ref: prio.h)
*/

#if defined(_RCNETIO_H)
#else
#define _RCNETIO_H

#include "rcbase.h"
#include "rcinrval.h"
#include "rcio.h"
#include "rcnetdb.h"

#include "prio.h"

class RCFileInfo;

/*
** Class: RCNetStreamIO (ref prio.h)
**
** Streamed (reliable) network I/O (e.g., TCP).
** This class hides (makes private) the functions that are not applicable
** to network I/O (i.e., those for file I/O).
*/

class PR_IMPLEMENT(RCNetStreamIO): public RCIO
{

public:
    RCNetStreamIO();
    virtual ~RCNetStreamIO();

    virtual RCIO*       Accept(RCNetAddr* addr, const RCInterval& timeout);
    virtual PRInt32     AcceptRead(
                            RCIO **nd, RCNetAddr **raddr, void *buf,
                            PRSize amount, const RCInterval& timeout);
    virtual PRInt64     Available();
    virtual PRStatus    Bind(const RCNetAddr& addr);
    virtual PRStatus    Connect(
                            const RCNetAddr& addr, const RCInterval& timeout);
    virtual PRStatus    GetLocalName(RCNetAddr *addr) const;
    virtual PRStatus    GetPeerName(RCNetAddr *addr) const;
    virtual PRStatus    GetSocketOption(PRSocketOptionData *data) const;
    virtual PRStatus    Listen(PRIntn backlog);
    virtual PRInt16     Poll(PRInt16 in_flags, PRInt16 *out_flags);
    virtual PRInt32     Read(void *buf, PRSize amount);
    virtual PRInt32     Recv(
                            void *buf, PRSize amount, PRIntn flags,
                            const RCInterval& timeout);
    virtual PRInt32     Recvfrom(
                            void *buf, PRSize amount, PRIntn flags,
                            RCNetAddr* addr, const RCInterval& timeout);
    virtual PRInt32     Send(
                            const void *buf, PRSize amount, PRIntn flags,
                            const RCInterval& timeout);
    virtual PRInt32     Sendto(
                            const void *buf, PRSize amount, PRIntn flags,
                            const RCNetAddr& addr,
                            const RCInterval& timeout);
    virtual PRStatus    SetSocketOption(const PRSocketOptionData *data);
    virtual PRStatus    Shutdown(ShutdownHow how);
    virtual PRInt32     TransmitFile(
                            RCIO *source, const void *headers,
                            PRSize hlen, RCIO::FileDisposition flags,
                            const RCInterval& timeout);
    virtual PRInt32     Write(const void *buf, PRSize amount);
    virtual PRInt32     Writev(
                            const PRIOVec *iov, PRSize size,
                            const RCInterval& timeout);

private:
    /* functions unavailable to this clients of this class */
    RCNetStreamIO(const RCNetStreamIO&);

    PRStatus    Close();
    PRStatus    Open(const char *name, PRIntn flags, PRIntn mode);
    PRStatus    FileInfo(RCFileInfo *info) const;
    PRStatus    Fsync();
    PRInt64     Seek(PRInt64 offset, RCIO::Whence how);

public:
    RCNetStreamIO(PRIntn protocol);
};  /* RCNetIO */

#endif /* defined(_RCNETIO_H) */

/* RCNetStreamIO.h */


