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

#define TFO_MAX_PACKET_SIZE_IPV4 1460
#define TFO_MAX_PACKET_SIZE_IPV6 1440
#define TFO_TLS_RECORD_HEADER_SIZE 22

/**
 *  For the TCP Fast Open it is necessary to send all data that can fit into the
 *  first packet on a single sendto function call. Consecutive calls will not
 *  have an effect. Therefore  TCPFastOpenLayer will collect some data before
 *  calling sendto. Necko and nss will call PR_Write multiple times with small
 *  amount of  data.
 *  TCPFastOpenLayer has 4 states:
 *    WAITING_FOR_CONNECT:
 *      This is before connect is call. A call of recv, send or getpeername will
 *      return PR_NOT_CONNECTED_ERROR. After connect is call the state transfers
 *      into COLLECT_DATA_FOR_FIRST_PACKET.
 *
 *    COLLECT_DATA_FOR_FIRST_PACKET:
 *      In this state all data received by send function calls will be stored in
 *      a buffer. If transaction do not have any more data ready to be sent or
 *      the buffer is full, TCPFastOpenFinish is call. TCPFastOpenFinish sends
 *      the collected data using sendto function and the state transfers to
 *      WAITING_FOR_CONNECTCONTINUE. If an error occurs during sendto, the error
 *      is reported by the TCPFastOpenFinish return values. nsSocketTransfer is
 *      the only caller of TCPFastOpenFinish; it knows how to interpreter these
 *      errors.
 *    WAITING_FOR_CONNECTCONTINUE:
 *      connectcontinue transfers from this state to CONNECTED. Any other
 *      function (e.g. send, recv) returns PR_WOULD_BLOCK_ERROR.
 *    CONNECTED:
 *      The size of mFirstPacketBuf is 1440/1460 (RFC7413 recomends that packet
 *      does exceeds these sizes). SendTo does not have to consume all buffered
 *      data and some data can be still in mFirstPacketBuf. Before sending any
 *      new data we need to send the remaining buffered data.
 **/

class TCPFastOpenSecret
{
public:
  TCPFastOpenSecret()
    : mState(WAITING_FOR_CONNECT)
    , mFirstPacketBufLen(0)
    , mCondition(0)
  {}

  enum {
    CONNECTED,
    WAITING_FOR_CONNECTCONTINUE,
    COLLECT_DATA_FOR_FIRST_PACKET,
    WAITING_FOR_CONNECT,
    SOCKET_ERROR_STATE
  } mState;
  PRNetAddr mAddr;
  char mFirstPacketBuf[1460];
  uint16_t mFirstPacketBufLen;
  PRErrorCode mCondition;
};

static PRStatus
TCPFastOpenConnect(PRFileDesc *fd, const PRNetAddr *addr,
                   PRIntervalTime timeout)
{
  MOZ_RELEASE_ASSERT(fd->identity == sTCPFastOpenLayerIdentity);
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  TCPFastOpenSecret *secret = reinterpret_cast<TCPFastOpenSecret *>(fd->secret);

  SOCKET_LOG(("TCPFastOpenConnect state=%d.\n", secret->mState));

  if (secret->mState != TCPFastOpenSecret::WAITING_FOR_CONNECT) {
    PR_SetError(PR_IS_CONNECTED_ERROR, 0);
    return PR_FAILURE;
  }

  // Remember the address. It will be used for sendto or connect later.
  memcpy(&secret->mAddr, addr, sizeof(secret->mAddr));
  secret->mState = TCPFastOpenSecret::COLLECT_DATA_FOR_FIRST_PACKET;

  return PR_SUCCESS;
}

static PRInt32
TCPFastOpenSend(PRFileDesc *fd, const void *buf, PRInt32 amount,
                PRIntn flags, PRIntervalTime timeout)
{
  MOZ_RELEASE_ASSERT(fd->identity == sTCPFastOpenLayerIdentity);
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  TCPFastOpenSecret *secret = reinterpret_cast<TCPFastOpenSecret *>(fd->secret);

  SOCKET_LOG(("TCPFastOpenSend state=%d.\n", secret->mState));

  switch(secret->mState) {
  case TCPFastOpenSecret::CONNECTED:
    // Before sending new data we need to drain the data collected during tfo.
    if (secret->mFirstPacketBufLen) {
      SOCKET_LOG(("TCPFastOpenSend - %d bytes to drain from "
                  "mFirstPacketBufLen.\n",
                  secret->mFirstPacketBufLen ));
      PRInt32 rv = (fd->lower->methods->send)(fd->lower,
                                              secret->mFirstPacketBuf,
                                              secret->mFirstPacketBufLen,
                                              0, // flags
                                              PR_INTERVAL_NO_WAIT);
      if (rv <= 0) {
        return rv;
      } else {
        secret->mFirstPacketBufLen -= rv;
        if (secret->mFirstPacketBufLen) {
          memmove(secret->mFirstPacketBuf,
                  secret->mFirstPacketBuf + rv,
                  secret->mFirstPacketBufLen);

          PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
          return PR_FAILURE;
        } // if we drained the buffer we can fall through this checks and call
          // send for the new data
      }
    }
    SOCKET_LOG(("TCPFastOpenSend sending new data.\n"));
    return (fd->lower->methods->send)(fd->lower, buf, amount, flags, timeout);
  case TCPFastOpenSecret::WAITING_FOR_CONNECTCONTINUE:
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    return -1;
  case TCPFastOpenSecret::COLLECT_DATA_FOR_FIRST_PACKET:
    {
      int32_t toSend =
        (secret->mAddr.raw.family == PR_AF_INET) ? TFO_MAX_PACKET_SIZE_IPV4
                                              : TFO_MAX_PACKET_SIZE_IPV6;
      MOZ_ASSERT(secret->mFirstPacketBufLen <= toSend);
      toSend -= secret->mFirstPacketBufLen;

      SOCKET_LOG(("TCPFastOpenSend: amount of data in the buffer=%d; the amount"
                  " of additional data that can be stored=%d.\n",
                  secret->mFirstPacketBufLen, toSend));

      if (!toSend) {
        PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
        return -1;
      }

      toSend = (toSend > amount) ? amount : toSend;
      memcpy(secret->mFirstPacketBuf + secret->mFirstPacketBufLen, buf, toSend);
      secret->mFirstPacketBufLen += toSend;
      return toSend;
    }
  case TCPFastOpenSecret::WAITING_FOR_CONNECT:
    PR_SetError(PR_NOT_CONNECTED_ERROR, 0);
    return -1;
  case TCPFastOpenSecret::SOCKET_ERROR_STATE:
    PR_SetError(secret->mCondition, 0);
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

    if (secret->mFirstPacketBufLen) {
      // TLS will not call write before receiving data from a server, therefore
      // we need to force sending buffered data even during recv call. Otherwise
      // It can come to a deadlock (clients waits for response, but the request
      // is sitting in mFirstPacketBufLen).
      SOCKET_LOG(("TCPFastOpenRevc - %d bytes to drain from mFirstPacketBuf.\n",
                  secret->mFirstPacketBufLen ));
      PRInt32 rv = (fd->lower->methods->send)(fd->lower,
                                              secret->mFirstPacketBuf,
                                              secret->mFirstPacketBufLen,
                                              0, // flags
                                              PR_INTERVAL_NO_WAIT);
      if (rv <= 0) {
        return rv;
      } else {
        secret->mFirstPacketBufLen -= rv;
        if (secret->mFirstPacketBufLen) {
          memmove(secret->mFirstPacketBuf,
                  secret->mFirstPacketBuf + rv,
                  secret->mFirstPacketBufLen);
        }
      }
    }
    rv = (fd->lower->methods->recv)(fd->lower, buf, amount, flags, timeout);
    break;
  case TCPFastOpenSecret::WAITING_FOR_CONNECTCONTINUE:
  case TCPFastOpenSecret::COLLECT_DATA_FOR_FIRST_PACKET:
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    break;
  case TCPFastOpenSecret::WAITING_FOR_CONNECT:
    PR_SetError(PR_NOT_CONNECTED_ERROR, 0);
    break;
  case TCPFastOpenSecret::SOCKET_ERROR_STATE:
    PR_SetError(secret->mCondition, 0);
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
  case TCPFastOpenSecret::COLLECT_DATA_FOR_FIRST_PACKET:
    PR_SetError(PR_NOT_CONNECTED_ERROR, 0);
    rv = PR_FAILURE;
    break;
  case TCPFastOpenSecret::WAITING_FOR_CONNECTCONTINUE:
    rv = (fd->lower->methods->connectcontinue)(fd->lower, out_flags);

    SOCKET_LOG(("TCPFastOpenConnectContinue result=%d.\n", rv));
    secret->mState = TCPFastOpenSecret::CONNECTED;
    break;
  case TCPFastOpenSecret::SOCKET_ERROR_STATE:
    PR_SetError(secret->mCondition, 0);
    rv = PR_FAILURE;
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

static PRInt16
TCPFastOpenPoll(PRFileDesc *fd, PRInt16 how_flags, PRInt16 *p_out_flags)
{
  MOZ_RELEASE_ASSERT(fd);
  MOZ_RELEASE_ASSERT(fd->identity == sTCPFastOpenLayerIdentity);

  TCPFastOpenSecret *secret = reinterpret_cast<TCPFastOpenSecret *>(fd->secret);
  if (secret->mFirstPacketBufLen) {
    how_flags |= PR_POLL_WRITE;
  }

  return fd->lower->methods->poll(fd->lower, how_flags, p_out_flags);
}

nsresult
AttachTCPFastOpenIOLayer(PRFileDesc *fd)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

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
    sTCPFastOpenLayerMethods.poll = TCPFastOpenPoll;
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
    PR_Free(layer); // PR_CreateIOLayerStub() uses PR_Malloc().
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void
TCPFastOpenFinish(PRFileDesc *fd, PRErrorCode &err,
                  bool &fastOpenNotSupported, uint8_t &tfoStatus)
{
  PRFileDesc *tfoFd = PR_GetIdentitiesLayer(fd, sTCPFastOpenLayerIdentity);
  MOZ_RELEASE_ASSERT(tfoFd);
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  TCPFastOpenSecret *secret = reinterpret_cast<TCPFastOpenSecret *>(tfoFd->secret);

  MOZ_ASSERT(secret->mState == TCPFastOpenSecret::COLLECT_DATA_FOR_FIRST_PACKET);

  fastOpenNotSupported = false;
  tfoStatus = TFO_NOT_TRIED;
  PRErrorCode result = 0;

  // If we do not have data to send with syn packet or nspr version does not
  // have sendto implemented we will call normal connect.
  // If sendto is not implemented it points to _PR_InvalidInt, therefore we
  // check if sendto != _PR_InvalidInt. _PR_InvalidInt is exposed so we use
  // reserved_fn_0 which also points to _PR_InvalidInt.
  if (!secret->mFirstPacketBufLen ||
      (tfoFd->lower->methods->sendto == (PRSendtoFN)tfoFd->lower->methods->reserved_fn_0)) {
    // Because of the way our nsHttpTransaction dispatch work, it can happened
    // that data has not been written into the socket.
    // In this case we can just call connect.
    PRInt32 rv = (tfoFd->lower->methods->connect)(tfoFd->lower, &secret->mAddr,
                                                  PR_INTERVAL_NO_WAIT);
    if (rv == PR_SUCCESS) {
      result = PR_IS_CONNECTED_ERROR;
    } else {
      result = PR_GetError();
    }
    if (tfoFd->lower->methods->sendto == (PRSendtoFN)tfoFd->lower->methods->reserved_fn_0) {
        // sendto is not implemented, it is equal to _PR_InvalidInt!
        // We will disable Fast Open.
        SOCKET_LOG(("TCPFastOpenFinish - sendto not implemented.\n"));
        fastOpenNotSupported = true;
    }
  } else {
    // We have some data ready in the buffer we will send it with the syn
    // packet.
    PRInt32 rv = (tfoFd->lower->methods->sendto)(tfoFd->lower,
                                                 secret->mFirstPacketBuf,
                                                 secret->mFirstPacketBufLen,
                                                 0, //flags
                                                 &secret->mAddr,
                                                 PR_INTERVAL_NO_WAIT);

    SOCKET_LOG(("TCPFastOpenFinish - sendto result=%d.\n", rv));
    if (rv > 0) {
      result = PR_IN_PROGRESS_ERROR;
      secret->mFirstPacketBufLen -= rv;
      if (secret->mFirstPacketBufLen) {
        memmove(secret->mFirstPacketBuf,
                secret->mFirstPacketBuf + rv,
                secret->mFirstPacketBufLen);
      }
      tfoStatus = TFO_DATA_SENT;
    } else {
      result = PR_GetError();
      SOCKET_LOG(("TCPFastOpenFinish - sendto error=%d.\n", result));

      if (result == PR_NOT_IMPLEMENTED_ERROR || // When a windows version does not support Fast Open it will return this error.
          result == PR_NOT_TCP_SOCKET_ERROR) { // SendTo will return PR_NOT_TCP_SOCKET_ERROR if TCP Fast Open is turned off on Linux.
        // We can call connect again.
        fastOpenNotSupported = true;
        rv = (tfoFd->lower->methods->connect)(tfoFd->lower, &secret->mAddr,
                                              PR_INTERVAL_NO_WAIT);

        if (rv == PR_SUCCESS) {
          result = PR_IS_CONNECTED_ERROR;
        } else {
          result = PR_GetError();
        }
      } else {
        tfoStatus = TFO_TRIED;
      }
    }
  }

  if (result == PR_IN_PROGRESS_ERROR) {
    secret->mState = TCPFastOpenSecret::WAITING_FOR_CONNECTCONTINUE;
  } else {
    // If the error is not PR_IN_PROGRESS_ERROR, change the state to CONNECT so
    // that recv/send can perform recv/send on the next lower layer and pick up
    // the real error. This is really important!
    // The result can also be PR_IS_CONNECTED_ERROR, that should change the
    // state to CONNECT anyway.
    secret->mState = TCPFastOpenSecret::CONNECTED;
  }
  err = result;
}

/* This function returns the size of the remaining free space in the
 * first_packet_buffer. This will be used by transactions with a tls layer. For
 * other transactions it is not necessary. The tls transactions make a tls
 * record before writing to this layer and if the record is too big the part
 * that does not have place in the mFirstPacketBuf will be saved on the tls
 * layer. During TFO we cannot send more than TFO_MAX_PACKET_SIZE_IPV4/6 bytes,
 * so if we have a big tls record, this record is encrypted with 0RTT key,
 * tls-early-data can be rejected and than we still need to send the rest of the
 * record.
 */
int32_t
TCPFastOpenGetBufferSizeLeft(PRFileDesc *fd)
{
  PRFileDesc *tfoFd = PR_GetIdentitiesLayer(fd, sTCPFastOpenLayerIdentity);
  MOZ_RELEASE_ASSERT(tfoFd);
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  TCPFastOpenSecret *secret = reinterpret_cast<TCPFastOpenSecret *>(tfoFd->secret);

  if (secret->mState != TCPFastOpenSecret::COLLECT_DATA_FOR_FIRST_PACKET) {
    return 0;
  }

  int32_t sizeLeft =
    (secret->mAddr.raw.family == PR_AF_INET) ? TFO_MAX_PACKET_SIZE_IPV4
                                             : TFO_MAX_PACKET_SIZE_IPV6;
  MOZ_ASSERT(secret->mFirstPacketBufLen <= sizeLeft);
  sizeLeft -= secret->mFirstPacketBufLen;

  SOCKET_LOG(("TCPFastOpenGetBufferSizeLeft=%d.\n", sizeLeft));

  return (sizeLeft > TFO_TLS_RECORD_HEADER_SIZE) ?
    sizeLeft - TFO_TLS_RECORD_HEADER_SIZE : 0;
}

bool
TCPFastOpenGetCurrentBufferSize(PRFileDesc *fd)
{
  PRFileDesc *tfoFd = PR_GetIdentitiesLayer(fd, sTCPFastOpenLayerIdentity);
  MOZ_RELEASE_ASSERT(tfoFd);
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  TCPFastOpenSecret *secret = reinterpret_cast<TCPFastOpenSecret *>(tfoFd->secret);

  return secret->mFirstPacketBufLen;
}

bool
TCPFastOpenFlushBuffer(PRFileDesc *fd)
{
  PRFileDesc *tfoFd = PR_GetIdentitiesLayer(fd, sTCPFastOpenLayerIdentity);
  MOZ_RELEASE_ASSERT(tfoFd);
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  TCPFastOpenSecret *secret = reinterpret_cast<TCPFastOpenSecret *>(tfoFd->secret);
  MOZ_ASSERT(secret->mState == TCPFastOpenSecret::CONNECTED);

  if (secret->mFirstPacketBufLen) {
    SOCKET_LOG(("TCPFastOpenFlushBuffer - %d bytes to drain from "
                "mFirstPacketBufLen.\n",
                secret->mFirstPacketBufLen ));
    PRInt32 rv = (tfoFd->lower->methods->send)(tfoFd->lower,
                                               secret->mFirstPacketBuf,
                                               secret->mFirstPacketBufLen,
                                               0, // flags
                                               PR_INTERVAL_NO_WAIT);
    if (rv <= 0) {
      PRErrorCode err = PR_GetError();
      if (err == PR_WOULD_BLOCK_ERROR) {
        // We still need to send this data.
        return true;
      } else {
        // There is an error, let nsSocketTransport pick it up properly.
        secret->mCondition = err;
        secret->mState = TCPFastOpenSecret::SOCKET_ERROR_STATE;
        return false;
      }
    }

    secret->mFirstPacketBufLen -= rv;
    if (secret->mFirstPacketBufLen) {
      memmove(secret->mFirstPacketBuf,
              secret->mFirstPacketBuf + rv,
              secret->mFirstPacketBufLen);
    }
  }
  return secret->mFirstPacketBufLen;
}

}
}
