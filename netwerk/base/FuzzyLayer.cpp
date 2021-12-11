/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzyLayer.h"
#include "nsTHashMap.h"
#include "nsDeque.h"
#include "nsIRunnable.h"
#include "nsSocketTransportService2.h"
#include "nsThreadUtils.h"

#include "prmem.h"
#include "prio.h"
#include "mozilla/Logging.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace net {

LazyLogModule gFuzzingLog("nsFuzzingNecko");

#define FUZZING_LOG(args) \
  MOZ_LOG(mozilla::net::gFuzzingLog, mozilla::LogLevel::Verbose, args)

// Mutex for modifying our hash tables
Mutex gConnRecvMutex("ConnectRecvMutex");

// This structure will be created by addNetworkFuzzingBuffer below
// and then added to the gNetworkFuzzingBuffers structure.
//
// It is assigned on connect and associated with the socket it belongs to.
typedef struct {
  const uint8_t* buf;
  size_t size;
  bool allowRead;
  bool allowUnused;
} NetworkFuzzingBuffer;

// This holds all connections we have currently open.
static nsTHashMap<nsPtrHashKey<PRFileDesc>, NetworkFuzzingBuffer*>
    gConnectedNetworkFuzzingBuffers;

// This holds all buffers for connections we can still open.
static nsDeque<NetworkFuzzingBuffer> gNetworkFuzzingBuffers;

// This is `true` once all connections are closed and either there are
// no buffers left to be used or all remaining buffers are marked optional.
// Used by `signalNetworkFuzzingDone` to tell the main thread if it needs
// to spin-wait for `gFuzzingConnClosed`.
static Atomic<bool> fuzzingNoWaitRequired(false);

// Used to memorize if the main thread has indicated that it is done with
// its iteration and we don't expect more connections now.
static Atomic<bool> fuzzingMainSignaledDone(false);

/*
 * The flag `gFuzzingConnClosed` is set by `FuzzyClose` when all connections
 * are closed *and* there are no more buffers in `gNetworkFuzzingBuffers` that
 * must be used. The main thread spins until this becomes true to synchronize
 * the fuzzing iteration between the main thread and the socket thread, if
 * the prior call to `signalNetworkFuzzingDone` returned `false`.
 */
Atomic<bool> gFuzzingConnClosed(true);

void addNetworkFuzzingBuffer(const uint8_t* data, size_t size, bool readFirst,
                             bool useIsOptional) {
  if (size > INT32_MAX) {
    MOZ_CRASH("Unsupported buffer size");
  }

  MutexAutoLock lock(gConnRecvMutex);

  NetworkFuzzingBuffer* buf = new NetworkFuzzingBuffer();
  buf->buf = data;
  buf->size = size;
  buf->allowRead = readFirst;
  buf->allowUnused = useIsOptional;

  gNetworkFuzzingBuffers.Push(buf);

  fuzzingMainSignaledDone = false;
  fuzzingNoWaitRequired = false;
}

/*
 * This method should be called by fuzzing from the main thread to signal to
 * the layer code that a fuzzing iteration is done. As a result, we can throw
 * away any optional buffers and signal back once all connections have been
 * closed. The main thread should synchronize on all connections being closed
 * after the actual request/action is complete.
 */
bool signalNetworkFuzzingDone() {
  FUZZING_LOG(("[signalNetworkFuzzingDone] Called."));
  MutexAutoLock lock(gConnRecvMutex);
  bool rv = false;

  if (fuzzingNoWaitRequired) {
    FUZZING_LOG(("[signalNetworkFuzzingDone] Purging remaining buffers."));
    // Easy case, we already have no connections and non-optional buffers left.
    gNetworkFuzzingBuffers.Erase();
    gFuzzingConnClosed = true;
    rv = true;
  } else {
    // We still have either connections left open or non-optional buffers left.
    // In this case, FuzzyClose will handle the tear-down and signaling.
    fuzzingMainSignaledDone = true;
  }

  return rv;
}

static PRDescIdentity sFuzzyLayerIdentity;
static PRIOMethods sFuzzyLayerMethods;
static PRIOMethods* sFuzzyLayerMethodsPtr = nullptr;

static PRInt16 PR_CALLBACK FuzzyPoll(PRFileDesc* fd, PRInt16 in_flags,
                                     PRInt16* out_flags) {
  *out_flags = 0;

  FUZZING_LOG(("[FuzzyPoll] Called with in_flags=%X.", in_flags));

  NetworkFuzzingBuffer* fuzzBuf = gConnectedNetworkFuzzingBuffers.Get(fd);

  if (in_flags & PR_POLL_READ && fuzzBuf && fuzzBuf->allowRead) {
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

  MutexAutoLock lock(gConnRecvMutex);

  NetworkFuzzingBuffer* buf = gNetworkFuzzingBuffers.PopFront();
  if (!buf) {
    FUZZING_LOG(("[FuzzyConnect] Denying additional connection."));
    return PR_FAILURE;
  }

  gConnectedNetworkFuzzingBuffers.InsertOrUpdate(fd, buf);
  fuzzingNoWaitRequired = false;

  FUZZING_LOG(("[FuzzyConnect] Successfully opened connection: %p", fd));

  gFuzzingConnClosed = false;

  return PR_SUCCESS;
}

static PRInt32 FuzzySend(PRFileDesc* fd, const void* buf, PRInt32 amount,
                         PRIntn flags, PRIntervalTime timeout) {
  MOZ_RELEASE_ASSERT(fd->identity == sFuzzyLayerIdentity);
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  MutexAutoLock lock(gConnRecvMutex);

  NetworkFuzzingBuffer* fuzzBuf = gConnectedNetworkFuzzingBuffers.Get(fd);
  if (!fuzzBuf) {
    FUZZING_LOG(("[FuzzySend] Write on socket that is not connected."));
    amount = 0;
  }

  // Allow reading once the implementation has written at least some data
  if (fuzzBuf && !fuzzBuf->allowRead) {
    FUZZING_LOG(("[FuzzySend] Write received, allowing further reads."));
    fuzzBuf->allowRead = true;
  }

  FUZZING_LOG(("[FuzzySend] Sent %" PRIx32 " bytes of data.", amount));

  return amount;
}

static PRInt32 FuzzyWrite(PRFileDesc* fd, const void* buf, PRInt32 amount) {
  return FuzzySend(fd, buf, amount, 0, PR_INTERVAL_NO_WAIT);
}

static PRInt32 FuzzyRecv(PRFileDesc* fd, void* buf, PRInt32 amount,
                         PRIntn flags, PRIntervalTime timeout) {
  MOZ_RELEASE_ASSERT(fd->identity == sFuzzyLayerIdentity);

  MutexAutoLock lock(gConnRecvMutex);

  NetworkFuzzingBuffer* fuzzBuf = gConnectedNetworkFuzzingBuffers.Get(fd);
  if (!fuzzBuf) {
    FUZZING_LOG(("[FuzzyRecv] Denying read, connection is closed."));
    return 0;
  }

  // As long as we haven't written anything, act as if no data was there yet
  if (!fuzzBuf->allowRead) {
    FUZZING_LOG(("[FuzzyRecv] Denying read, nothing written before."));
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    return -1;
  }

  if (gFuzzingConnClosed) {
    FUZZING_LOG(("[FuzzyRecv] Denying read, connection is closed."));
    return 0;
  }

  // No data left, act as if the connection was closed.
  if (!fuzzBuf->size) {
    FUZZING_LOG(("[FuzzyRecv] Read failed, no data left."));
    return 0;
  }

  // Use the remains of fuzzing buffer, if too little is left
  if (fuzzBuf->size < (PRUint32)amount) amount = fuzzBuf->size;

  // Consume buffer, copy data
  memcpy(buf, fuzzBuf->buf, amount);

  if (!(flags & PR_MSG_PEEK)) {
    fuzzBuf->buf += amount;
    fuzzBuf->size -= amount;
  }

  FUZZING_LOG(("[FuzzyRecv] Read %" PRIx32 " bytes of data.", amount));

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

  MutexAutoLock lock(gConnRecvMutex);

  NetworkFuzzingBuffer* fuzzBuf = nullptr;
  if (gConnectedNetworkFuzzingBuffers.Remove(fd, &fuzzBuf)) {
    FUZZING_LOG(("[FuzzyClose] Received close on socket %p", fd));
    delete fuzzBuf;
  } else {
    /* Happens when close is called on a non-connected socket */
    FUZZING_LOG(("[FuzzyClose] Received close on unknown socket %p.", fd));
  }

  PRStatus ret = fd->methods->close(fd);

  if (!gConnectedNetworkFuzzingBuffers.Count()) {
    // At this point, all connections are closed, but we might still have
    // unused network buffers that were not marked as optional.
    bool haveRemainingUnusedBuffers = false;
    for (size_t i = 0; i < gNetworkFuzzingBuffers.GetSize(); ++i) {
      NetworkFuzzingBuffer* buf = gNetworkFuzzingBuffers.ObjectAt(i);

      if (!buf->allowUnused) {
        haveRemainingUnusedBuffers = true;
        break;
      }
    }

    if (haveRemainingUnusedBuffers) {
      FUZZING_LOG(
          ("[FuzzyClose] All connections closed, waiting for remaining "
           "connections."));
    } else if (!fuzzingMainSignaledDone) {
      // We have no connections left, but the main thread hasn't signaled us
      // yet. For now, that means once the main thread signals us, we can tell
      // it immediately that it won't have to wait for closing connections.
      FUZZING_LOG(
          ("[FuzzyClose] All connections closed, waiting for main thread."));
      fuzzingNoWaitRequired = true;
    } else {
      // No connections left and main thread is already done. Perform cleanup
      // and then signal the main thread to continue.
      FUZZING_LOG(("[FuzzyClose] All connections closed, cleaning up."));

      gNetworkFuzzingBuffers.Erase();
      gFuzzingConnClosed = true;

      // We need to dispatch this so the main thread is guaranteed to wake up
      nsCOMPtr<nsIRunnable> r(new mozilla::Runnable("Dummy"));
      NS_DispatchToMainThread(r.forget());
    }
  } else {
    FUZZING_LOG(("[FuzzyClose] Connection closed."));
  }

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
