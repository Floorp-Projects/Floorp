/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TCPFastOpenLayer.h"
#include "nsSocketTransportService2.h"
#include "prmem.h"
#include "prio.h"

namespace mozilla {
namespace net {

static PRDescIdentity sTCPFastOpenLayerIdentity;
static PRIOMethods    sTCPFastOpenLayerMethods;
static PRIOMethods   *sTCPFastOpenLayerMethodsPtr = nullptr;

class TCPFastOpenSecret
{
public:
  TCPFastOpenSecret()
    : mState(WAITING_FOR_CONNECT)
    , mConnectResult(0)
    , mFastOpenNotSupported(false)
  {}

  enum {
    CONNECTED,
    WAITING_FOR_CONNECTCONTINUE,
    WAITING_FOR_FIRST_SEND,
    WAITING_FOR_CONNECT
  } mState;
  PRNetAddr mAddr;
  PRErrorCode mConnectResult;
  bool mFastOpenNotSupported;
};

static PRStatus
TCPFastOpenConnect(PRFileDesc *fd, const PRNetAddr *addr,
                   PRIntervalTime timeout)
{
  MOZ_RELEASE_ASSERT(fd->identity == sTCPFastOpenLayerIdentity);
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  TCPFastOpenSecret *secret = reinterpret_cast<TCPFastOpenSecret *>(fd->secret);

  SOCKET_LOG(("TCPFastOpenConnect state=%d.\n", secret->mState));

  if (secret->mState != TCPFastOpenSecret::WAITING_FOR_CONNECT) {
    PR_SetError(PR_IS_CONNECTED_ERROR, 0);
    return PR_FAILURE;
  }

  // Remember the address we will call PR_Sendto and for that we need
  // the address.
  memcpy(&secret->mAddr, addr, sizeof(secret->mAddr));
  secret->mState = TCPFastOpenSecret::WAITING_FOR_FIRST_SEND;

  return PR_SUCCESS;
}

static PRInt32
TCPFastOpenSend(PRFileDesc *fd, const void *buf, PRInt32 amount,
                PRIntn flags, PRIntervalTime timeout)
{
  MOZ_RELEASE_ASSERT(fd->identity == sTCPFastOpenLayerIdentity);
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  TCPFastOpenSecret *secret = reinterpret_cast<TCPFastOpenSecret *>(fd->secret);

  SOCKET_LOG(("TCPFastOpenSend state=%d.\n", secret->mState));

  switch(secret->mState) {
  case TCPFastOpenSecret::CONNECTED:
    return (fd->lower->methods->send)(fd->lower, buf, amount, flags, timeout);
  case TCPFastOpenSecret::WAITING_FOR_CONNECTCONTINUE:
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    return -1;
  case TCPFastOpenSecret::WAITING_FOR_FIRST_SEND:
    {
      PRInt32 rv = (fd->lower->methods->sendto)(fd->lower, buf, amount, flags,
                                                &secret->mAddr, timeout);

      SOCKET_LOG(("TCPFastOpenSend - sendto result=%d.\n", rv));
      if (rv > -1) {
        secret->mState = TCPFastOpenSecret::WAITING_FOR_CONNECTCONTINUE;
        secret->mConnectResult = PR_IN_PROGRESS_ERROR;
        return rv;
      }

      secret->mConnectResult = PR_GetError();
      SOCKET_LOG(("TCPFastOpenSend - sendto error=%d.\n",
                  secret->mConnectResult));

      if (secret->mConnectResult == PR_IS_CONNECTED_ERROR) {
        secret->mState = TCPFastOpenSecret::CONNECTED;

      } else if (secret->mConnectResult == PR_IN_PROGRESS_ERROR) {
        secret->mState = TCPFastOpenSecret::WAITING_FOR_CONNECTCONTINUE;

      } else if (secret->mConnectResult == PR_NOT_IMPLEMENTED_ERROR || // When a windows version does not support Fast Open it will return this error.
                 secret->mConnectResult == PR_NOT_TCP_SOCKET_ERROR) { // SendTo will return PR_NOT_TCP_SOCKET_ERROR if TCP Fast Open is turned off on Linux.
        // We can call connect again.
        secret->mFastOpenNotSupported = true;
        rv = (fd->lower->methods->connect)(fd->lower, &secret->mAddr, timeout);

        if (rv == PR_SUCCESS) {
          secret->mConnectResult = PR_IS_CONNECTED_ERROR;
          secret->mState = TCPFastOpenSecret::CONNECTED;

        } else {
          secret->mConnectResult = PR_GetError();
          secret->mState = TCPFastOpenSecret::WAITING_FOR_CONNECTCONTINUE;

        }
      }

      // Error will be picked up by TCPFastOpenConnectResult.
      PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
      return -1;
    }
  case TCPFastOpenSecret::WAITING_FOR_CONNECT:
    PR_SetError(PR_NOT_CONNECTED_ERROR, 0);
    return -1;
  }
  PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
  return PR_FAILURE;
}

static PRInt32
TCPFastOpenWrite(PRFileDesc *fd, const void *buf, PRInt32 amount)
{
  return TCPFastOpenSend(fd, buf, amount, 0, PR_INTERVAL_NO_WAIT);
}

static PRInt32
TCPFastOpenRecv(PRFileDesc *fd, void *buf, PRInt32 amount,
                PRIntn flags, PRIntervalTime timeout)
{
  MOZ_RELEASE_ASSERT(fd->identity == sTCPFastOpenLayerIdentity);

  TCPFastOpenSecret *secret = reinterpret_cast<TCPFastOpenSecret *>(fd->secret);

  PRInt32 rv = -1;
  switch(secret->mState) {
  case TCPFastOpenSecret::CONNECTED:
    rv = (fd->lower->methods->recv)(fd->lower, buf, amount, flags, timeout);
    break;
  case TCPFastOpenSecret::WAITING_FOR_CONNECTCONTINUE:
  case TCPFastOpenSecret::WAITING_FOR_FIRST_SEND:
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    break;
  case TCPFastOpenSecret::WAITING_FOR_CONNECT:
    PR_SetError(PR_NOT_CONNECTED_ERROR, 0);
  }
  return rv;
}

static PRInt32
TCPFastOpenRead(PRFileDesc *fd, void *buf, PRInt32 amount)
{
  return TCPFastOpenRecv(fd, buf, amount , 0, PR_INTERVAL_NO_WAIT);
}

static PRStatus
TCPFastOpenConnectContinue(PRFileDesc *fd, PRInt16 out_flags)
{
  MOZ_RELEASE_ASSERT(fd->identity == sTCPFastOpenLayerIdentity);

  TCPFastOpenSecret *secret = reinterpret_cast<TCPFastOpenSecret *>(fd->secret);

  PRStatus rv = PR_FAILURE;
  switch(secret->mState) {
  case TCPFastOpenSecret::CONNECTED:
    rv = PR_SUCCESS;
    break;
  case TCPFastOpenSecret::WAITING_FOR_CONNECT:
  case TCPFastOpenSecret::WAITING_FOR_FIRST_SEND:
    PR_SetError(PR_NOT_CONNECTED_ERROR, 0);
    rv = PR_FAILURE;
    break;
  case TCPFastOpenSecret::WAITING_FOR_CONNECTCONTINUE:
    rv = (fd->lower->methods->connectcontinue)(fd->lower, out_flags);

    SOCKET_LOG(("TCPFastOpenConnectContinue result=%d.\n", rv));
    if (rv == PR_SUCCESS) {
      secret->mState = TCPFastOpenSecret::CONNECTED;
    }
  }
  return rv;
}

static PRStatus
TCPFastOpenClose(PRFileDesc *fd)
{
  if (!fd) {
    return PR_FAILURE;
  }

  PRFileDesc* layer = PR_PopIOLayer(fd, PR_TOP_IO_LAYER);

  MOZ_RELEASE_ASSERT(layer &&
                     layer->identity == sTCPFastOpenLayerIdentity,
                     "TCP Fast Open Layer not on top of stack");

  TCPFastOpenSecret *secret = reinterpret_cast<TCPFastOpenSecret *>(layer->secret);
  layer->secret = nullptr;
  layer->dtor(layer);
  delete secret;
  return fd->methods->close(fd);
}

static PRStatus
TCPFastOpenGetpeername (PRFileDesc *fd, PRNetAddr *addr)
{
  MOZ_RELEASE_ASSERT(fd);
  MOZ_RELEASE_ASSERT(addr);

  MOZ_RELEASE_ASSERT(fd->identity == sTCPFastOpenLayerIdentity);

  TCPFastOpenSecret *secret = reinterpret_cast<TCPFastOpenSecret *>(fd->secret);
  if (secret->mState == TCPFastOpenSecret::WAITING_FOR_CONNECT) {
    PR_SetError(PR_NOT_CONNECTED_ERROR, 0);
    return PR_FAILURE;
  }

  memcpy(addr, &secret->mAddr, sizeof(secret->mAddr));
  return PR_SUCCESS;
}

nsresult
AttachTCPFastOpenIOLayer(PRFileDesc *fd)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  if (!sTCPFastOpenLayerMethodsPtr) {
    sTCPFastOpenLayerIdentity = PR_GetUniqueIdentity("TCPFastOpen Layer");
    sTCPFastOpenLayerMethods = *PR_GetDefaultIOMethods();
    sTCPFastOpenLayerMethods.connect = TCPFastOpenConnect;
    sTCPFastOpenLayerMethods.send = TCPFastOpenSend;
    sTCPFastOpenLayerMethods.write = TCPFastOpenWrite;
    sTCPFastOpenLayerMethods.recv = TCPFastOpenRecv;
    sTCPFastOpenLayerMethods.read = TCPFastOpenRead;
    sTCPFastOpenLayerMethods.connectcontinue = TCPFastOpenConnectContinue;
    sTCPFastOpenLayerMethods.close = TCPFastOpenClose;
    sTCPFastOpenLayerMethods.getpeername = TCPFastOpenGetpeername;
    sTCPFastOpenLayerMethodsPtr = &sTCPFastOpenLayerMethods;
  }

  PRFileDesc *layer = PR_CreateIOLayerStub(sTCPFastOpenLayerIdentity,
                                           sTCPFastOpenLayerMethodsPtr);

  if (!layer) {
    return NS_ERROR_FAILURE;
  }

  TCPFastOpenSecret *secret = new TCPFastOpenSecret();

  layer->secret = reinterpret_cast<PRFilePrivate *>(secret);

  PRStatus status = PR_PushIOLayer(fd, PR_NSPR_IO_LAYER, layer);

  if (status == PR_FAILURE) {
    delete secret;
    PR_DELETE(layer);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void
TCPFastOpenConnectResult(PRFileDesc * fd, PRErrorCode *err,
                         bool *fastOpenNotSupported)
{
  PRFileDesc *tfoFd = PR_GetIdentitiesLayer(fd, sTCPFastOpenLayerIdentity);
  MOZ_RELEASE_ASSERT(tfoFd);
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  TCPFastOpenSecret *secret = reinterpret_cast<TCPFastOpenSecret *>(tfoFd->secret);

  MOZ_ASSERT(secret->mState != TCPFastOpenSecret::WAITING_FOR_CONNECT);

  *fastOpenNotSupported = secret->mFastOpenNotSupported;

  if (secret->mState == TCPFastOpenSecret::WAITING_FOR_FIRST_SEND) {
    // Because of the way our HttpTransaction dispatch work, it can happened
    // that connect is not called.
    PRInt32 rv = (tfoFd->lower->methods->connect)(tfoFd->lower, &secret->mAddr,
                                               PR_INTERVAL_NO_WAIT);
    if (rv == PR_SUCCESS) {
      secret->mConnectResult = PR_IS_CONNECTED_ERROR;
      secret->mState = TCPFastOpenSecret::CONNECTED;
    } else {
      secret->mConnectResult = PR_GetError();
      secret->mState = TCPFastOpenSecret::WAITING_FOR_CONNECTCONTINUE;
    }
  }
  *err = secret->mConnectResult;
}

}
}
