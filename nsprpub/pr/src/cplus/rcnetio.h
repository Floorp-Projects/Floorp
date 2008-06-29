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


