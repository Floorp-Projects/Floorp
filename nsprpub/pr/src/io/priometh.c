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

#include "primpl.h"

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
    (PRGetsockoptFN)_PR_InvalidStatus,    
    (PRSetsockoptFN)_PR_InvalidStatus,    
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus
};

PRIntn _PR_InvalidInt()
{
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}  /* _PR_InvalidInt */

PRInt16 _PR_InvalidInt16()
{
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}  /* _PR_InvalidInt */

PRInt64 _PR_InvalidInt64()
{
    PRInt64 rv;
    LL_I2L(rv, -1);
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return rv;
}  /* _PR_InvalidInt */

/*
 * An invalid method that returns PRStatus
 */

PRStatus _PR_InvalidStatus()
{
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}  /* _PR_InvalidDesc */

/*
 * An invalid method that returns a pointer
 */

PRFileDesc *_PR_InvalidDesc()
{
    PR_ASSERT(!"I/O method is invalid");
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

PR_IMPLEMENT(PRStatus) PR_GetSockOpt(
    PRFileDesc *fd, PRSockOption optname, void* optval, PRInt32* optlen)
{
#if defined(DEBUG)
    static PRBool warn = PR_TRUE;
    if (warn) warn = _PR_Obsolete("PR_GetSockOpt()", "PR_GetSocketOption()");
#endif
	return((fd->methods->getsockopt)(fd, optname, optval, optlen));
}

PR_IMPLEMENT(PRStatus) PR_SetSockOpt(
    PRFileDesc *fd, PRSockOption optname, const void* optval, PRInt32 optlen)
{
#if defined(DEBUG)
    static PRBool warn = PR_TRUE;
    if (warn) warn = _PR_Obsolete("PR_SetSockOpt()", "PR_SetSocketOption()");
#endif
	return((fd->methods->setsockopt)(fd, optname, optval, optlen));
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

/* priometh.c */
