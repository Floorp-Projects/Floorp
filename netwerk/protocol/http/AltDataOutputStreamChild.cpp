#include "mozilla/net/AltDataOutputStreamChild.h"
#include "mozilla/Unused.h"
#include "nsIInputStream.h"
#include "nsStreamUtils.h"

namespace mozilla {
namespace net {

NS_IMPL_ADDREF(AltDataOutputStreamChild)

NS_IMETHODIMP_(MozExternalRefCountType) AltDataOutputStreamChild::Release() {
  MOZ_ASSERT(0 != mRefCnt, "dup release");
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");
  --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "AltDataOutputStreamChild");

  if (mRefCnt == 1 && mIPCOpen) {
    // The only reference left is the IPDL one. After the parent replies back
    // with a DeleteSelf message, the child will call Send__delete__(this),
    // decrementing the ref count and triggering the destructor.
    SendDeleteSelf();
    return 1;
  }

  if (mRefCnt == 0) {
    mRefCnt = 1; /* stabilize */
    delete this;
    return 0;
  }
  return mRefCnt;
}

NS_INTERFACE_MAP_BEGIN(AltDataOutputStreamChild)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsIOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

AltDataOutputStreamChild::AltDataOutputStreamChild()
    : mIPCOpen(false), mError(NS_OK), mCallbackFlags(0) {
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");
}

void AltDataOutputStreamChild::AddIPDLReference() {
  MOZ_ASSERT(!mIPCOpen, "Attempt to retain more than one IPDL reference");
  mIPCOpen = true;
  AddRef();
}

void AltDataOutputStreamChild::ReleaseIPDLReference() {
  MOZ_ASSERT(mIPCOpen, "Attempt to release nonexistent IPDL reference");
  mIPCOpen = false;

  if (mCallback) {
    NotifyListener();
  }

  Release();
}

bool AltDataOutputStreamChild::WriteDataInChunks(
    const nsDependentCSubstring& data) {
  const uint32_t kChunkSize = 128 * 1024;
  uint32_t next = std::min(data.Length(), kChunkSize);
  for (uint32_t i = 0; i < data.Length();
       i = next, next = std::min(data.Length(), next + kChunkSize)) {
    nsCString chunk(Substring(data, i, kChunkSize));
    if (mIPCOpen && !SendWriteData(chunk)) {
      mIPCOpen = false;
      return false;
    }
  }
  return true;
}

NS_IMETHODIMP
AltDataOutputStreamChild::Close() { return CloseWithStatus(NS_OK); }

NS_IMETHODIMP
AltDataOutputStreamChild::Flush() {
  if (!mIPCOpen) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_FAILED(mError)) {
    return mError;
  }

  // This is a no-op
  return NS_OK;
}

NS_IMETHODIMP
AltDataOutputStreamChild::Write(const char* aBuf, uint32_t aCount,
                                uint32_t* _retval) {
  if (!mIPCOpen) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_FAILED(mError)) {
    return mError;
  }
  if (WriteDataInChunks(nsDependentCSubstring(aBuf, aCount))) {
    *_retval = aCount;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
AltDataOutputStreamChild::WriteFrom(nsIInputStream* aFromStream,
                                    uint32_t aCount, uint32_t* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
AltDataOutputStreamChild::WriteSegments(nsReadSegmentFun aReader,
                                        void* aClosure, uint32_t aCount,
                                        uint32_t* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
AltDataOutputStreamChild::IsNonBlocking(bool* _retval) {
  *_retval = false;
  return NS_OK;
}

mozilla::ipc::IPCResult AltDataOutputStreamChild::RecvError(
    const nsresult& err) {
  mError = err;
  return IPC_OK();
}

mozilla::ipc::IPCResult AltDataOutputStreamChild::RecvDeleteSelf() {
  PAltDataOutputStreamChild::Send__delete__(this);
  return IPC_OK();
}

// nsIAsyncOutputStream

NS_IMETHODIMP
AltDataOutputStreamChild::CloseWithStatus(nsresult aStatus) {
  if (!mIPCOpen) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_FAILED(mError)) {
    return mError;
  }
  Unused << SendClose(aStatus);

  return NS_OK;
}

NS_IMETHODIMP
AltDataOutputStreamChild::AsyncWait(nsIOutputStreamCallback* aCallback,
                                    uint32_t aFlags, uint32_t aRequestedCount,
                                    nsIEventTarget* aEventTarget) {
  mCallback = aCallback;
  mCallbackFlags = aFlags;
  mCallbackTarget = aEventTarget;

  if (!mCallback) {
    return NS_OK;
  }

  // The stream is blocking so it is writable at any time
  if (!mIPCOpen || !(aFlags & WAIT_CLOSURE_ONLY)) {
    NotifyListener();
  }

  return NS_OK;
}

void AltDataOutputStreamChild::NotifyListener() {
  MOZ_ASSERT(mCallback);

  if (!mCallbackTarget) {
    mCallbackTarget = GetMainThreadEventTarget();
  }

  nsCOMPtr<nsIOutputStreamCallback> asyncCallback =
      NS_NewOutputStreamReadyEvent(mCallback, mCallbackTarget);

  mCallback = nullptr;
  mCallbackTarget = nullptr;

  asyncCallback->OnOutputStreamReady(this);
}

}  // namespace net
}  // namespace mozilla
