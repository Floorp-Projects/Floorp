/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include "logging.h"
#include "nspr.h"
#include "prerror.h"
#include "prio.h"

#include "nsCOMPtr.h"
#include "nsASocketHandler.h"
#include "nsISocketTransportService.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOM.h"

#include "transportflow.h"
#include "transportlayerprsock.h"

namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")

nsresult TransportLayerPrsock::InitInternal() {
  // Get the transport service as a transport service
  nsresult rv;
  stservice_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

  if (!NS_SUCCEEDED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Couldn't get socket transport service");
    return rv;
  }

  return NS_OK;
}

void TransportLayerPrsock::Import(PRFileDesc *fd, nsresult *result) {
  if (state_ != TS_INIT) {
    *result = NS_ERROR_NOT_INITIALIZED;
    return;
  }

  MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Importing()");
  fd_ = fd;
  handler_ = new SocketHandler(this, fd);

  nsresult rv = stservice_->AttachSocket(fd_, handler_);
  if (!NS_SUCCEEDED(rv)) {
    *result = rv;
    return;
  }

  TL_SET_STATE(TS_OPEN);

  *result = NS_OK;
}

int TransportLayerPrsock::SendPacket(const unsigned char *data, size_t len) {
  MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "SendPacket(" << len << ")");
  if (state_ != TS_OPEN) {
    MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Can't send packet on closed interface");
    return TE_INTERNAL;
  }

  int32_t status;
  status = PR_Write(fd_, data, len);
  if (status >= 0) {
    MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Wrote " << len << " bytes");
    return status;
  }

  PRErrorCode err = PR_GetError();
  if (err == PR_WOULD_BLOCK_ERROR) {
    MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Write blocked");
    return TE_WOULDBLOCK;
  }


  MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Write error; channel closed");
  TL_SET_STATE(TS_ERROR);
  return TE_ERROR;
}

void TransportLayerPrsock::OnSocketReady(PRFileDesc *fd, int16_t outflags) {
  if (!(outflags & PR_POLL_READ)) {
    return;
  }

  MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "OnSocketReady(flags=" << outflags << ")");

  unsigned char buf[1600];
  int32_t rv = PR_Read(fd, buf, sizeof(buf));

  if (rv > 0) {
    // Successful read
    MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Read " << rv << " bytes");
    SignalPacketReceived(this, buf, rv);
  } else if (rv == 0) {
    MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Read 0 bytes; channel closed");
    TL_SET_STATE(TS_CLOSED);
  } else {
    PRErrorCode err = PR_GetError();

    if (err != PR_WOULD_BLOCK_ERROR) {
      MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Read error; channel closed");
      TL_SET_STATE(TS_ERROR);
    }
  }
}

NS_IMPL_ISUPPORTS0(TransportLayerPrsock::SocketHandler)
}  // close namespace
