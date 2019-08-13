/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzyLayer.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"

#include "prmem.h"
#include "prio.h"
#include "mozilla/Logging.h"

namespace mozilla {
namespace net {

LazyLogModule gFuzzingLog("nsFuzzingNecko");

#define FUZZING_LOG(args) \
  MOZ_LOG(mozilla::net::gFuzzingLog, mozilla::LogLevel::Verbose, args)

// These will be set by setNetworkFuzzingBuffer below
static Atomic<const uint8_t*> gFuzzingBuf(NULL);
static Atomic<size_t> gFuzzingSize(0);

/*
 * These globals control the behavior of our virtual socket:
 *
 *   If `gFuzzingAllowRead` is false, any reading on the socket will be denied.
 *   This is to ensure that a write has to happen first on a new connection.
 *
 *   If `gFuzzingAllowNewConn` is false, any calls to `FuzzyConnect` will fail.
 *   We use this to control that only a single connection is opened per buffer.
 *
 *   The flag `gFuzzingConnClosed` is set by `FuzzyClose` when the connection
 *   is closed. The main thread spins until this becomes true to synchronize
 *   the fuzzing iteration between the main thread and the socket thread.
 *
 */
Atomic<bool> gFuzzingConnClosed(true);
Atomic<bool> gFuzzingAllowNewConn(false);
Atomic<bool> gFuzzingAllowRead(false);

void setNetworkFuzzingBuffer(const uint8_t* data, size_t size) {
  if (size > INT32_MAX) {
    MOZ_CRASH("Unsupported buffer size");
  }
  gFuzzingBuf = data;
  gFuzzingSize = size;
  gFuzzingAllowNewConn = true;
  gFuzzingAllowRead = false;
}

static PRDescIdentity sFuzzyLayerIdentity;
static PRIOMethods sFuzzyLayerMethods;
static PRIOMethods* sFuzzyLayerMethodsPtr = nullptr;

static PRInt16 PR_CALLBACK FuzzyPoll(PRFileDesc* fd, PRInt16 in_flags,
                                     PRInt16* out_flags) {
  *out_flags = 0;

  if (in_flags & PR_POLL_READ && gFuzzingAllowRead) {
    *out_flags = PR_POLL_READ;
    return PR_POLL_READ;
  }

  if (in_flags & PR_POLL_WRITE) {
    *out_flags = PR_POLL_WRITE;
    return PR_POLL_WRITE;
  }

  return in_flags;
}

static PRStatus FuzzyConnect(PRFileDesc* fd, const PRNetAddr* addr,
                             PRIntervalTime timeout) {
  MOZ_RELEASE_ASSERT(fd->identity == sFuzzyLayerIdentity);
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (!gFuzzingAllowNewConn) {
    FUZZING_LOG(("[FuzzyConnect] Denying additional connection."));
    return PR_FAILURE;
  }

  FUZZING_LOG(("[FuzzyConnect] Successfully opened connection."));

  gFuzzingAllowNewConn = false;
  gFuzzingConnClosed = false;

  return PR_SUCCESS;
}

static PRInt32 FuzzySend(PRFileDesc* fd, const void* buf, PRInt32 amount,
                         PRIntn flags, PRIntervalTime timeout) {
  MOZ_RELEASE_ASSERT(fd->identity == sFuzzyLayerIdentity);
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  // Allow reading once the implementation has written at least some data
  if (!gFuzzingAllowRead) {
    FUZZING_LOG(("[FuzzySend] Write received, allowing further reads."));
    gFuzzingAllowRead = true;
  }

  return amount;
}

static PRInt32 FuzzyWrite(PRFileDesc* fd, const void* buf, PRInt32 amount) {
  return FuzzySend(fd, buf, amount, 0, PR_INTERVAL_NO_WAIT);
}

static PRInt32 FuzzyRecv(PRFileDesc* fd, void* buf, PRInt32 amount,
                         PRIntn flags, PRIntervalTime timeout) {
  MOZ_RELEASE_ASSERT(fd->identity == sFuzzyLayerIdentity);

  // As long as we haven't written anything, act as if no data was there yet
  if (!gFuzzingAllowRead) {
    FUZZING_LOG(("[FuzzyRecv] Denying read, nothing written before."));
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    return -1;
  }

  // No data left, act as if the connection was closed.
  if (!gFuzzingSize) return 0;

  // Use the remains of fuzzing buffer, if too little is left
  if (gFuzzingSize < (PRUint32)amount) amount = gFuzzingSize;

  // Consume buffer, copy data
  memcpy(buf, gFuzzingBuf, amount);

  if (!(flags & PR_MSG_PEEK)) {
    gFuzzingBuf += amount;
    gFuzzingSize -= amount;
  }

  return amount;
}

static PRInt32 FuzzyRead(PRFileDesc* fd, void* buf, PRInt32 amount) {
  return FuzzyRecv(fd, buf, amount, 0, PR_INTERVAL_NO_WAIT);
}

static PRStatus FuzzyClose(PRFileDesc* fd) {
  if (!fd) {
    return PR_FAILURE;
  }
  PRFileDesc* layer = PR_PopIOLayer(fd, PR_TOP_IO_LAYER);
  MOZ_RELEASE_ASSERT(layer && layer->identity == sFuzzyLayerIdentity,
                     "Fuzzy Layer not on top of stack");

  layer->dtor(layer);

  PRStatus ret = fd->methods->close(fd);

  // Mark connection as closed, but still don't allow new connections
  gFuzzingConnClosed = true;

  FUZZING_LOG(("[FuzzyClose] Connection closed."));

  // We need to dispatch this so the main thread is guaranteed to wake up
  nsCOMPtr<nsIRunnable> r(new mozilla::Runnable("Dummy"));
  NS_DispatchToMainThread(r.forget());

  return ret;
}

nsresult AttachFuzzyIOLayer(PRFileDesc* fd) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (!sFuzzyLayerMethodsPtr) {
    sFuzzyLayerIdentity = PR_GetUniqueIdentity("Fuzzy Layer");
    sFuzzyLayerMethods = *PR_GetDefaultIOMethods();
    sFuzzyLayerMethods.connect = FuzzyConnect;
    sFuzzyLayerMethods.send = FuzzySend;
    sFuzzyLayerMethods.write = FuzzyWrite;
    sFuzzyLayerMethods.recv = FuzzyRecv;
    sFuzzyLayerMethods.read = FuzzyRead;
    sFuzzyLayerMethods.close = FuzzyClose;
    sFuzzyLayerMethods.poll = FuzzyPoll;
    sFuzzyLayerMethodsPtr = &sFuzzyLayerMethods;
  }

  PRFileDesc* layer =
      PR_CreateIOLayerStub(sFuzzyLayerIdentity, sFuzzyLayerMethodsPtr);

  if (!layer) {
    return NS_ERROR_FAILURE;
  }

  PRStatus status = PR_PushIOLayer(fd, PR_TOP_IO_LAYER, layer);

  if (status == PR_FAILURE) {
    PR_Free(layer);  // PR_CreateIOLayerStub() uses PR_Malloc().
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
