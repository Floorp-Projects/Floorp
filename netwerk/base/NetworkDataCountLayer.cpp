/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NetworkDataCountLayer.h"
#include "nsSocketTransportService2.h"
#include "prmem.h"
#include "prio.h"

namespace mozilla {
namespace net {

static PRDescIdentity sNetworkDataCountLayerIdentity;
static PRIOMethods sNetworkDataCountLayerMethods;
static PRIOMethods* sNetworkDataCountLayerMethodsPtr = nullptr;

class NetworkDataCountSecret {
 public:
  NetworkDataCountSecret() = default;

  uint64_t mSentBytes = 0;
  uint64_t mReceivedBytes = 0;
};

static PRInt32 NetworkDataCountSend(PRFileDesc* fd, const void* buf,
                                    PRInt32 amount, PRIntn flags,
                                    PRIntervalTime timeout) {
  MOZ_RELEASE_ASSERT(fd->identity == sNetworkDataCountLayerIdentity);

  NetworkDataCountSecret* secret =
      reinterpret_cast<NetworkDataCountSecret*>(fd->secret);

  PRInt32 rv =
      (fd->lower->methods->send)(fd->lower, buf, amount, flags, timeout);
  if (rv > 0) {
    secret->mSentBytes += rv;
  }
  return rv;
}

static PRInt32 NetworkDataCountWrite(PRFileDesc* fd, const void* buf,
                                     PRInt32 amount) {
  return NetworkDataCountSend(fd, buf, amount, 0, PR_INTERVAL_NO_WAIT);
}

static PRInt32 NetworkDataCountRecv(PRFileDesc* fd, void* buf, PRInt32 amount,
                                    PRIntn flags, PRIntervalTime timeout) {
  MOZ_RELEASE_ASSERT(fd->identity == sNetworkDataCountLayerIdentity);

  NetworkDataCountSecret* secret =
      reinterpret_cast<NetworkDataCountSecret*>(fd->secret);

  PRInt32 rv =
      (fd->lower->methods->recv)(fd->lower, buf, amount, flags, timeout);
  if (rv > 0) {
    secret->mReceivedBytes += rv;
  }
  return rv;
}

static PRInt32 NetworkDataCountRead(PRFileDesc* fd, void* buf, PRInt32 amount) {
  return NetworkDataCountRecv(fd, buf, amount, 0, PR_INTERVAL_NO_WAIT);
}

static PRStatus NetworkDataCountClose(PRFileDesc* fd) {
  if (!fd) {
    return PR_FAILURE;
  }

  PRFileDesc* layer = PR_PopIOLayer(fd, PR_TOP_IO_LAYER);

  MOZ_RELEASE_ASSERT(layer && layer->identity == sNetworkDataCountLayerIdentity,
                     "NetworkDataCount Layer not on top of stack");

  NetworkDataCountSecret* secret =
      reinterpret_cast<NetworkDataCountSecret*>(layer->secret);
  layer->secret = nullptr;
  layer->dtor(layer);
  delete secret;
  return fd->methods->close(fd);
}

nsresult AttachNetworkDataCountLayer(PRFileDesc* fd) {
  if (!sNetworkDataCountLayerMethodsPtr) {
    sNetworkDataCountLayerIdentity =
        PR_GetUniqueIdentity("NetworkDataCount Layer");
    sNetworkDataCountLayerMethods = *PR_GetDefaultIOMethods();
    sNetworkDataCountLayerMethods.send = NetworkDataCountSend;
    sNetworkDataCountLayerMethods.write = NetworkDataCountWrite;
    sNetworkDataCountLayerMethods.recv = NetworkDataCountRecv;
    sNetworkDataCountLayerMethods.read = NetworkDataCountRead;
    sNetworkDataCountLayerMethods.close = NetworkDataCountClose;
    sNetworkDataCountLayerMethodsPtr = &sNetworkDataCountLayerMethods;
  }

  PRFileDesc* layer = PR_CreateIOLayerStub(sNetworkDataCountLayerIdentity,
                                           sNetworkDataCountLayerMethodsPtr);

  if (!layer) {
    return NS_ERROR_FAILURE;
  }

  NetworkDataCountSecret* secret = new NetworkDataCountSecret();

  layer->secret = reinterpret_cast<PRFilePrivate*>(secret);

  PRStatus status = PR_PushIOLayer(fd, PR_NSPR_IO_LAYER, layer);

  if (status == PR_FAILURE) {
    delete secret;
    PR_Free(layer);  // PR_CreateIOLayerStub() uses PR_Malloc().
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void NetworkDataCountSent(PRFileDesc* fd, uint64_t& sentBytes) {
  PRFileDesc* ndcFd = PR_GetIdentitiesLayer(fd, sNetworkDataCountLayerIdentity);
  MOZ_RELEASE_ASSERT(ndcFd);

  NetworkDataCountSecret* secret =
      reinterpret_cast<NetworkDataCountSecret*>(ndcFd->secret);
  sentBytes = secret->mSentBytes;
}

void NetworkDataCountReceived(PRFileDesc* fd, uint64_t& receivedBytes) {
  PRFileDesc* ndcFd = PR_GetIdentitiesLayer(fd, sNetworkDataCountLayerIdentity);
  MOZ_RELEASE_ASSERT(ndcFd);

  NetworkDataCountSecret* secret =
      reinterpret_cast<NetworkDataCountSecret*>(ndcFd->secret);
  receivedBytes = secret->mReceivedBytes;
}

}  // namespace net
}  // namespace mozilla
