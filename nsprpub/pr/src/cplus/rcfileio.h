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
** Class definitions for normal and special file I/O (ref: prio.h)
*/

#if defined(_RCFILEIO_H)
#else
#define _RCFILEIO_H

#include "rcio.h"
#include "rctime.h"

/*
** One would normally create a concrete class, such as RCFileIO, but then
** pass around more generic references, ie., RCIO.
**
** This subclass of RCIO hides (makes private) the methods that are not
** applicable to normal files.
*/

class RCFileInfo;

class PR_IMPLEMENT(RCFileIO): public RCIO
{
public:
    RCFileIO();
    virtual ~RCFileIO();

    virtual PRInt64     Available();
    virtual PRStatus    Close();
    static  PRStatus    Delete(const char *name);
    virtual PRStatus    FileInfo(RCFileInfo* info) const;
    static  PRStatus    FileInfo(const char *name, RCFileInfo* info);
    virtual PRStatus    Fsync();
    virtual PRStatus    Open(const char *name, PRIntn flags, PRIntn mode);
    virtual PRInt32     Read(void *buf, PRSize amount);
    virtual PRInt64     Seek(PRInt64 offset, RCIO::Whence how);
    virtual PRInt32     Write(const void *buf, PRSize amount);
    virtual PRInt32     Writev(
                            const PRIOVec *iov, PRSize size,
                            const RCInterval& timeout);

private:

    /* These methods made private are unavailable for this object */
    RCFileIO(const RCFileIO&);
    void operator=(const RCFileIO&);

    RCIO*       Accept(RCNetAddr* addr, const RCInterval& timeout);
    PRInt32     AcceptRead(
                    RCIO **newfd, RCNetAddr **address, void *buffer,
                    PRSize amount, const RCInterval& timeout);
    PRStatus    Bind(const RCNetAddr& addr);
    PRStatus    Connect(const RCNetAddr& addr, const RCInterval& timeout);
    PRStatus    GetLocalName(RCNetAddr *addr) const;
    PRStatus    GetPeerName(RCNetAddr *addr) const;
    PRStatus    GetSocketOption(PRSocketOptionData *data) const;
    PRStatus    Listen(PRIntn backlog);
    PRInt16     Poll(PRInt16 in_flags, PRInt16 *out_flags);
    PRInt32     Recv(
                    void *buf, PRSize amount, PRIntn flags,
                    const RCInterval& timeout);
    PRInt32     Recvfrom(
                    void *buf, PRSize amount, PRIntn flags,
                    RCNetAddr* addr, const RCInterval& timeout);
    PRInt32     Send(
                    const void *buf, PRSize amount, PRIntn flags,
                    const RCInterval& timeout);
    PRInt32     Sendto(
                    const void *buf, PRSize amount, PRIntn flags,
                    const RCNetAddr& addr,
                    const RCInterval& timeout);
    PRStatus    SetSocketOption(const PRSocketOptionData *data);
    PRStatus    Shutdown(RCIO::ShutdownHow how);
    PRInt32     TransmitFile(
                    RCIO *source, const void *headers,
                    PRSize hlen, RCIO::FileDisposition flags,
                    const RCInterval& timeout);
public:

    /*
    ** The following function return a valid normal file object,
    ** Such objects can be used for scanned input and console output.
    */
    typedef enum {
        input = PR_StandardInput,
        output = PR_StandardOutput,
        error = PR_StandardError
    } SpecialFile;

    static RCIO *GetSpecialFile(RCFileIO::SpecialFile special);

};  /* RCFileIO */

class PR_IMPLEMENT(RCFileInfo): public RCBase
{
public:
    typedef enum {
        file = PR_FILE_FILE,
        directory = PR_FILE_DIRECTORY,
        other = PR_FILE_OTHER
    } FileType;

public:
    RCFileInfo();
    RCFileInfo(const RCFileInfo&);

    virtual ~RCFileInfo();

    PRInt64 Size() const;
    RCTime CreationTime() const;
    RCTime ModifyTime() const;
    RCFileInfo::FileType Type() const;

friend PRStatus RCFileIO::FileInfo(RCFileInfo*) const;
friend PRStatus RCFileIO::FileInfo(const char *name, RCFileInfo*);

private:
    PRFileInfo64 info;
};  /* RCFileInfo */

inline RCFileInfo::RCFileInfo(): RCBase() { }
inline PRInt64 RCFileInfo::Size() const { return info.size; }

#endif /* defined(_RCFILEIO_H) */
