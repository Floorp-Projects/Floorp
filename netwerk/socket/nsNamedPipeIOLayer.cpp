/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <utility>
#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "mozilla/Move.h"
#include "mozilla/net/DNS.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "nsINamedPipeService.h"
#include "nsISupportsImpl.h"
#include "nsIThread.h"
#include "nsNamedPipeIOLayer.h"
#include "nsNetCID.h"
#include "nspr.h"
#include "nsServiceManagerUtils.h"
#include "nsSocketTransportService2.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "private/pprio.h"

namespace mozilla {
namespace net {

static mozilla::LazyLogModule gNamedPipeLog("NamedPipeWin");
#define LOG_NPIO_DEBUG(...) MOZ_LOG(gNamedPipeLog, mozilla::LogLevel::Debug, \
                                    (__VA_ARGS__))
#define LOG_NPIO_ERROR(...) MOZ_LOG(gNamedPipeLog, mozilla::LogLevel::Error, \
                                    (__VA_ARGS__))

PRDescIdentity nsNamedPipeLayerIdentity;
static PRIOMethods nsNamedPipeLayerMethods;

class NamedPipeInfo final : public nsINamedPipeDataObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINAMEDPIPEDATAOBSERVER

  explicit NamedPipeInfo();

  nsresult Connect(const nsACString& aPath);
  nsresult Disconnect();

  /**
   * Both blocking/non-blocking mode are supported in this class.
   * The default mode is non-blocking mode, however, the client may change its
   * mode to blocking mode during hand-shaking (e.g. nsSOCKSSocketInfo).
   *
   * In non-blocking mode, |Read| and |Write| should be called by clients only
   * when |GetPollFlags| reports data availability. That is, the client calls
   * |GetPollFlags| with |PR_POLL_READ| and/or |PR_POLL_WRITE| set, and
   * according to the flags that set, |GetPollFlags| will check buffers status
   * and decide corresponding actions:
   *
   * -------------------------------------------------------------------
   * |               | data in buffer          | empty buffer          |
   * |---------------+-------------------------+-----------------------|
   * | PR_POLL_READ  | out: PR_POLL_READ       | DoRead/DoReadContinue |
   * |---------------+-------------------------+-----------------------|
   * | PR_POLL_WRITE | DoWrite/DoWriteContinue | out: PR_POLL_WRITE    |
   * ------------------------------------------+------------------------
   *
   * |DoRead| and |DoWrite| initiate read/write operations asynchronously, and
   * the |DoReadContinue| and |DoWriteContinue| are used to check the amount
   * of the data are read/written to/from buffers.
   *
   * The output parameter and the return value of |GetPollFlags| are identical
   * because we don't rely on the low-level select function to wait for data
   * availability, we instead use nsNamedPipeService to poll I/O completeness.
   *
   * When client get |PR_POLL_READ| or |PR_POLL_WRITE| from |GetPollFlags|,
   * they are able to use |Read| or |Write| to access the data in the buffer,
   * and this is supposed to be very fast because no network traffic is involved.
   *
   * In blocking mode, the flow is quite similar to non-blocking mode, but
   * |DoReadContinue| and |DoWriteContinue| are never been used since the
   * operations are done synchronously, which could lead to slow responses.
   */
  int32_t Read(void* aBuffer, int32_t aSize);
  int32_t Write(const void* aBuffer, int32_t aSize);

  // Like Read, but doesn't remove data in internal buffer.
  uint32_t Peek(void* aBuffer, int32_t aSize);

  // Number of bytes available to read in internal buffer.
  int32_t Available() const;

  // Flush write buffer
  //
  // @return whether the buffer has been flushed
  bool Sync(uint32_t aTimeout);
  void SetNonblocking(bool nonblocking);

  bool IsConnected() const;
  bool IsNonblocking() const;
  HANDLE GetHandle() const;

  // Initiate and check current status for read/write operations.
  int16_t GetPollFlags(int16_t aInFlags, int16_t* aOutFlags);

private:
  virtual ~NamedPipeInfo();

  /**
   * DoRead/DoWrite starts a read/write call synchronously or asynchronously
   * depending on |mNonblocking|. In blocking mode, they return when the action
   * has been done and in non-blocking mode it returns the number of bytes that
   * were read/written if the operation is done immediately. If it takes some
   * time to finish the operation, zero is returned and
   * DoReadContinue/DoWriteContinue must be called to get async I/O result.
   */
  int32_t DoRead();
  int32_t DoReadContinue();
  int32_t DoWrite();
  int32_t DoWriteContinue();

  /**
   * There was a write size limitation of named pipe,
   * see https://support.microsoft.com/en-us/kb/119218 for more information.
   * The limitation no longer exists, so feel free to change the value.
   */
  static const uint32_t kBufferSize = 65536;

  nsCOMPtr<nsINamedPipeService> mNamedPipeService;

  HANDLE mPipe; // the handle to the named pipe.
  OVERLAPPED mReadOverlapped; // used for asynchronous read operations.
  OVERLAPPED mWriteOverlapped;  // used for asynchronous write operations.

  uint8_t mReadBuffer[kBufferSize]; // octets read from pipe.

  /**
   * These indicates the [begin, end) position of the data in the buffer.
   */
  DWORD mReadBegin;
  DWORD mReadEnd;

  bool mHasPendingRead; // previous asynchronous read is not finished yet.

  uint8_t mWriteBuffer[kBufferSize]; // octets to be written to pipe.

  /**
   * These indicates the [begin, end) position of the data in the buffer.
   */
  DWORD mWriteBegin; // how many bytes are already written.
  DWORD mWriteEnd; // valid amount of data in the buffer.

  bool mHasPendingWrite; // previous asynchronous write is not finished yet.

  /**
   * current blocking mode is non-blocking or not, accessed only in socket
   * thread.
   */
  bool mNonblocking;

  Atomic<DWORD> mErrorCode; // error code from Named Pipe Service.
};

NS_IMPL_ISUPPORTS(NamedPipeInfo,
                  nsINamedPipeDataObserver)

NamedPipeInfo::NamedPipeInfo()
  : mNamedPipeService(do_GetService(NS_NAMEDPIPESERVICE_CONTRACTID))
  , mPipe(INVALID_HANDLE_VALUE)
  , mReadBegin(0)
  , mReadEnd(0)
  , mHasPendingRead(false)
  , mWriteBegin(0)
  , mWriteEnd(0)
  , mHasPendingWrite(false)
  , mNonblocking(true)
  , mErrorCode(0)
{
  MOZ_ASSERT(mNamedPipeService);

  ZeroMemory(&mReadOverlapped, sizeof(OVERLAPPED));
  ZeroMemory(&mWriteOverlapped, sizeof(OVERLAPPED));
}

NamedPipeInfo::~NamedPipeInfo()
{
  MOZ_ASSERT(!mPipe);
}

// nsINamedPipeDataObserver

NS_IMETHODIMP
NamedPipeInfo::OnDataAvailable(uint32_t aBytesTransferred,
                               void* aOverlapped)
{
  DebugOnly<bool> isOnPipeServiceThread;
  MOZ_ASSERT(NS_SUCCEEDED(mNamedPipeService->IsOnCurrentThread(&isOnPipeServiceThread)) &&
             isOnPipeServiceThread);

  if (aOverlapped == &mReadOverlapped) {
    LOG_NPIO_DEBUG("[%s] %p read %d bytes", __func__, this, aBytesTransferred);
  } else if (aOverlapped == &mWriteOverlapped) {
    LOG_NPIO_DEBUG("[%s] %p write %d bytes", __func__, this, aBytesTransferred);
  } else {
    MOZ_ASSERT(false, "invalid callback");
    mErrorCode = ERROR_INVALID_DATA;
    return NS_ERROR_FAILURE;
  }

  mErrorCode = ERROR_SUCCESS;

  // dispatch an empty event to trigger STS thread
  gSocketTransportService->Dispatch(NS_NewRunnableFunction([]{}),
                                    NS_DISPATCH_NORMAL);

  return NS_OK;
}

NS_IMETHODIMP
NamedPipeInfo::OnError(uint32_t aError,
                       void* aOverlapped)
{
  DebugOnly<bool> isOnPipeServiceThread;
  MOZ_ASSERT(NS_SUCCEEDED(mNamedPipeService->IsOnCurrentThread(&isOnPipeServiceThread)) &&
             isOnPipeServiceThread);

  LOG_NPIO_ERROR("[%s] error code=%d", __func__, aError);
  mErrorCode = aError;

  // dispatch an empty event to trigger STS thread
  gSocketTransportService->Dispatch(NS_NewRunnableFunction([]{}),
                                    NS_DISPATCH_NORMAL);

  return NS_OK;
}

// Named pipe operations

nsresult
NamedPipeInfo::Connect(const nsACString& aPath)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  HANDLE pipe;
  nsAutoCString path(aPath);

  pipe = CreateFileA(path.get(),
                     GENERIC_READ | GENERIC_WRITE,
                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                     nullptr,
                     OPEN_EXISTING,
                     FILE_FLAG_OVERLAPPED,
                     nullptr);

  if (pipe == INVALID_HANDLE_VALUE) {
    LOG_NPIO_ERROR("[%p] CreateFile error (%d)", this, GetLastError());
    return NS_ERROR_FAILURE;
  }

  DWORD pipeMode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(pipe, &pipeMode, nullptr, nullptr)) {
    LOG_NPIO_ERROR("[%p] SetNamedPipeHandleState error (%d)",
                   this,
                   GetLastError());
    CloseHandle(pipe);
    return NS_ERROR_FAILURE;
  }

  nsresult rv = mNamedPipeService->AddDataObserver(pipe, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    CloseHandle(pipe);
    return rv;
  }

  HANDLE readEvent = CreateEventA(nullptr, TRUE, TRUE, "NamedPipeRead");
  if (NS_WARN_IF(!readEvent || readEvent == INVALID_HANDLE_VALUE)) {
    CloseHandle(pipe);
    return NS_ERROR_FAILURE;
  }

  HANDLE writeEvent = CreateEventA(nullptr, TRUE, TRUE, "NamedPipeWrite");
  if (NS_WARN_IF(!writeEvent || writeEvent == INVALID_HANDLE_VALUE)) {
    CloseHandle(pipe);
    CloseHandle(readEvent);
    return NS_ERROR_FAILURE;
  }

  mPipe = pipe;
  mReadOverlapped.hEvent = readEvent;
  mWriteOverlapped.hEvent = writeEvent;
  return NS_OK;
}

nsresult
NamedPipeInfo::Disconnect()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  nsresult rv = mNamedPipeService->RemoveDataObserver(mPipe, this);
  NS_WARN_IF(NS_FAILED(rv));
  mPipe = nullptr;

  if (mReadOverlapped.hEvent &&
      mReadOverlapped.hEvent != INVALID_HANDLE_VALUE) {
    CloseHandle(mReadOverlapped.hEvent);
    mReadOverlapped.hEvent = nullptr;
  }

  if (mWriteOverlapped.hEvent &&
      mWriteOverlapped.hEvent != INVALID_HANDLE_VALUE) {
    CloseHandle(mWriteOverlapped.hEvent);
    mWriteOverlapped.hEvent = nullptr;
  }

  return NS_OK;
}

int32_t
NamedPipeInfo::Read(void* aBuffer, int32_t aSize)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  int32_t bytesRead = Peek(aBuffer, aSize);

  if (bytesRead > 0) {
    mReadBegin += bytesRead;
  }

  return bytesRead;
}

int32_t
NamedPipeInfo::Write(const void* aBuffer, int32_t aSize)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mWriteBegin <= mWriteEnd);

  if (!IsConnected()) {
    // pipe unconnected
    PR_SetError(PR_NOT_CONNECTED_ERROR, 0);
    return -1;
  }

  if (mWriteBegin == mWriteEnd) {
    mWriteBegin = mWriteEnd = 0;
  }

  int32_t bytesToWrite = std::min<int32_t>(aSize,
                                           sizeof(mWriteBuffer) - mWriteEnd);
  MOZ_ASSERT(bytesToWrite >= 0);

  if (bytesToWrite == 0) {
    PR_SetError(IsNonblocking() ? PR_WOULD_BLOCK_ERROR
                                : PR_IO_PENDING_ERROR,
                0);
    return -1;
  }

  memcpy(&mWriteBuffer[mWriteEnd], aBuffer, bytesToWrite);
  mWriteEnd += bytesToWrite;

  /**
   * Triggers internal write operation by calling |GetPollFlags|.
   * This is required for callers that use blocking I/O because they don't call
   * |GetPollFlags| to write data, but this also works for non-blocking I/O.
   */
  int16_t outFlag;
  GetPollFlags(PR_POLL_WRITE, &outFlag);

  return bytesToWrite;
}

uint32_t
NamedPipeInfo::Peek(void* aBuffer, int32_t aSize)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mReadBegin <= mReadEnd);

  if (!IsConnected()) {
    // pipe unconnected
    PR_SetError(PR_NOT_CONNECTED_ERROR, 0);
    return -1;
  }

  /**
   * If there's nothing in the read buffer, try to trigger internal read
   * operation by calling |GetPollFlags|. This is required for callers that
   * use blocking I/O because they don't call |GetPollFlags| to read data,
   * but this also works for non-blocking I/O.
   */
  if (!Available()) {
    int16_t outFlag;
    GetPollFlags(PR_POLL_READ, &outFlag);

    if (!(outFlag & PR_POLL_READ)) {
      PR_SetError(IsNonblocking() ? PR_WOULD_BLOCK_ERROR
                                  : PR_IO_PENDING_ERROR,
                  0);
      return -1;
    }
  }

  // Available() can't return more than what fits to the buffer at the read offset.
  int32_t bytesRead = std::min<int32_t>(aSize, Available());
  MOZ_ASSERT(bytesRead >= 0);
  MOZ_ASSERT(mReadBegin + bytesRead <= mReadEnd);
  memcpy(aBuffer, &mReadBuffer[mReadBegin], bytesRead);
  return bytesRead;
}

int32_t
NamedPipeInfo::Available() const
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mReadBegin <= mReadEnd);
  MOZ_ASSERT(mReadEnd - mReadBegin <= 0x7FFFFFFF); // no more than int32_max
  return mReadEnd - mReadBegin;
}

bool
NamedPipeInfo::Sync(uint32_t aTimeout)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  if (!mHasPendingWrite) {
    return true;
  }
  return WaitForSingleObject(mWriteOverlapped.hEvent, aTimeout) == WAIT_OBJECT_0;
}

void
NamedPipeInfo::SetNonblocking(bool nonblocking)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  mNonblocking = nonblocking;
}

bool
NamedPipeInfo::IsConnected() const
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  return mPipe && mPipe != INVALID_HANDLE_VALUE;
}

bool
NamedPipeInfo::IsNonblocking() const
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  return mNonblocking;
}

HANDLE
NamedPipeInfo::GetHandle() const
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  return mPipe;
}


int16_t
NamedPipeInfo::GetPollFlags(int16_t aInFlags, int16_t* aOutFlags)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  *aOutFlags = 0;

  if (aInFlags & PR_POLL_READ) {
    int32_t bytesToRead = 0;
    if (mReadBegin < mReadEnd) { // data in buffer and is ready to be read
      bytesToRead = Available();
    } else if (mHasPendingRead) { // nonblocking I/O and has pending task
      bytesToRead = DoReadContinue();
    } else { // read bufer is empty.
      bytesToRead = DoRead();
    }

    if (bytesToRead > 0) {
      *aOutFlags |= PR_POLL_READ;
    } else if (bytesToRead < 0) {
      *aOutFlags |= PR_POLL_ERR;
    }
  }

  if (aInFlags & PR_POLL_WRITE) {
    int32_t bytesWritten = 0;
    if (mHasPendingWrite) { // nonblocking I/O and has pending task.
      bytesWritten = DoWriteContinue();
    } else if (mWriteBegin < mWriteEnd) { // data in buffer, ready to write
      bytesWritten = DoWrite();
    } else { // write buffer is empty.
      *aOutFlags |= PR_POLL_WRITE;
    }

    if (bytesWritten < 0) {
      *aOutFlags |= PR_POLL_ERR;
    } else if (bytesWritten &&
               !mHasPendingWrite &&
               mWriteBegin == mWriteEnd) {
      *aOutFlags |= PR_POLL_WRITE;
    }
  }

  return *aOutFlags;
}

// @return: data has been read and is available
int32_t
NamedPipeInfo::DoRead()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(!mHasPendingRead);
  MOZ_ASSERT(mReadBegin == mReadEnd); // the buffer should be empty

  mReadBegin = 0;
  mReadEnd = 0;

  BOOL success = ReadFile(mPipe,
                          mReadBuffer,
                          sizeof(mReadBuffer),
                          &mReadEnd,
                          IsNonblocking() ? &mReadOverlapped : nullptr);

  if (success) {
    LOG_NPIO_DEBUG("[%s][%p] %d bytes read", __func__, this, mReadEnd);
    return mReadEnd;
  }

  switch (GetLastError()) {
    case ERROR_MORE_DATA:   // has more data to read
      mHasPendingRead = true;
      return DoReadContinue();

    case ERROR_IO_PENDING:  // read is pending
      mHasPendingRead = true;
      break;

    default:
      LOG_NPIO_ERROR("[%s] ReadFile failed (%d)", __func__, GetLastError());
      Disconnect();
      PR_SetError(PR_IO_ERROR, 0);
      return -1;
  }

  return 0;
}

int32_t
NamedPipeInfo::DoReadContinue()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mHasPendingRead);
  MOZ_ASSERT(mReadBegin == 0 && mReadEnd == 0);

  BOOL success;
  success = GetOverlappedResult(mPipe,
                                &mReadOverlapped,
                                &mReadEnd,
                                FALSE);
  if (success) {
    mHasPendingRead = false;
    if (mReadEnd == 0) {
      Disconnect();
      PR_SetError(PR_NOT_CONNECTED_ERROR, 0);
      return -1;
    }

    LOG_NPIO_DEBUG("[%s][%p] %d bytes read", __func__, this, mReadEnd);
    return mReadEnd;
  }

  switch (GetLastError()) {
    case ERROR_MORE_DATA:
      mHasPendingRead = false;
      LOG_NPIO_DEBUG("[%s][%p] %d bytes read", __func__, this, mReadEnd);
      return mReadEnd;
    case ERROR_IO_INCOMPLETE: // still in progress
      break;
    default:
      LOG_NPIO_ERROR("[%s]: GetOverlappedResult failed (%d)",
                     __func__,
                     GetLastError());
      Disconnect();
      PR_SetError(PR_IO_ERROR, 0);
      return -1;
  }

  return 0;
}

int32_t
NamedPipeInfo::DoWrite()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(!mHasPendingWrite);
  MOZ_ASSERT(mWriteBegin < mWriteEnd);

  DWORD bytesWritten = 0;
  BOOL success = WriteFile(mPipe,
                           &mWriteBuffer[mWriteBegin],
                           mWriteEnd - mWriteBegin,
                           &bytesWritten,
                           IsNonblocking() ? &mWriteOverlapped : nullptr);

  if (success) {
    mWriteBegin += bytesWritten;
    LOG_NPIO_DEBUG("[%s][%p] %d bytes written", __func__, this, bytesWritten);
    return bytesWritten;
  }

  if (GetLastError() != ERROR_IO_PENDING) {
    LOG_NPIO_ERROR("[%s] WriteFile failed (%d)", __func__, GetLastError());
    Disconnect();
    PR_SetError(PR_IO_ERROR, 0);
    return -1;
  }

  mHasPendingWrite = true;

  return 0;
}

int32_t
NamedPipeInfo::DoWriteContinue()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mHasPendingWrite);

  DWORD bytesWritten = 0;
  BOOL success = GetOverlappedResult(mPipe,
                                     &mWriteOverlapped,
                                     &bytesWritten,
                                     FALSE);

  if (!success) {
    if (GetLastError() == ERROR_IO_INCOMPLETE) {
      // still in progress
      return 0;
    }

    LOG_NPIO_ERROR("[%s] GetOverlappedResult failed (%d)",
                   __func__,
                   GetLastError());
    Disconnect();
    PR_SetError(PR_IO_ERROR, 0);
    return -1;
  }

  mHasPendingWrite = false;
  mWriteBegin += bytesWritten;
  LOG_NPIO_DEBUG("[%s][%p] %d bytes written", __func__, this, bytesWritten);
  return bytesWritten;
}

static inline NamedPipeInfo*
GetNamedPipeInfo(PRFileDesc* aFd)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_DIAGNOSTIC_ASSERT(aFd);
  MOZ_DIAGNOSTIC_ASSERT(aFd->secret);
  MOZ_DIAGNOSTIC_ASSERT(PR_GetLayersIdentity(aFd) == nsNamedPipeLayerIdentity);

  if (!aFd ||
      !aFd->secret ||
      PR_GetLayersIdentity(aFd) != nsNamedPipeLayerIdentity) {
    LOG_NPIO_ERROR("cannot get named pipe info");
    return nullptr;
  }

  return reinterpret_cast<NamedPipeInfo*>(aFd->secret);
}

static PRStatus
nsNamedPipeConnect(PRFileDesc* aFd,
                   const PRNetAddr* aAddr,
                   PRIntervalTime aTimeout)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  NamedPipeInfo* info = GetNamedPipeInfo(aFd);
  if (!info) {
    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
    return PR_FAILURE;
  }

  if (NS_WARN_IF(NS_FAILED(info->Connect(
      nsDependentCString(aAddr->local.path))))) {
    return PR_FAILURE;
  }

  return PR_SUCCESS;
}

static PRStatus
nsNamedPipeConnectContinue(PRFileDesc* aFd, PRInt16 aOutFlags)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  return PR_SUCCESS;
}

static PRStatus
nsNamedPipeClose(PRFileDesc* aFd)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  if (aFd->secret && PR_GetLayersIdentity(aFd) == nsNamedPipeLayerIdentity) {
    RefPtr<NamedPipeInfo> info = dont_AddRef(GetNamedPipeInfo(aFd));
    info->Disconnect();
    aFd->secret = nullptr;
    aFd->identity = PR_INVALID_IO_LAYER;
  }

  MOZ_ASSERT(!aFd->lower);
  PR_DELETE(aFd);

  return PR_SUCCESS;
}

static PRInt32
nsNamedPipeSend(PRFileDesc* aFd,
                const void* aBuffer,
                PRInt32 aAmount,
                PRIntn aFlags,
                PRIntervalTime aTimeout)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  Unused << aFlags;
  Unused << aTimeout;

  NamedPipeInfo* info = GetNamedPipeInfo(aFd);
  if (!info) {
    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
    return -1;
  }
  return info->Write(aBuffer, aAmount);
}

static PRInt32
nsNamedPipeRecv(PRFileDesc* aFd,
                void* aBuffer,
                PRInt32 aAmount,
                PRIntn aFlags,
                PRIntervalTime aTimeout)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  Unused << aTimeout;

  NamedPipeInfo* info = GetNamedPipeInfo(aFd);
  if (!info) {
    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
    return -1;
  }

  if (aFlags) {
    if (aFlags != PR_MSG_PEEK) {
      PR_SetError(PR_UNKNOWN_ERROR, 0);
      return -1;
    }
    return info->Peek(aBuffer, aAmount);
  }

  return info->Read(aBuffer, aAmount);
}

static inline PRInt32
nsNamedPipeRead(PRFileDesc* aFd, void* aBuffer, PRInt32 aAmount)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  NamedPipeInfo* info = GetNamedPipeInfo(aFd);
  if (!info) {
    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
    return -1;
  }
  return info->Read(aBuffer, aAmount);
}

static inline PRInt32
nsNamedPipeWrite(PRFileDesc* aFd, const void* aBuffer, PRInt32 aAmount)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  NamedPipeInfo* info = GetNamedPipeInfo(aFd);
  if (!info) {
    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
    return -1;
  }
  return info->Write(aBuffer, aAmount);
}

static PRInt32
nsNamedPipeAvailable(PRFileDesc* aFd)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  NamedPipeInfo* info = GetNamedPipeInfo(aFd);
  if (!info) {
    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
    return -1;
  }
  return static_cast<PRInt32>(info->Available());
}

static PRInt64
nsNamedPipeAvailable64(PRFileDesc* aFd)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  NamedPipeInfo* info = GetNamedPipeInfo(aFd);
  if (!info) {
    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
    return -1;
  }
  return static_cast<PRInt64>(info->Available());
}

static PRStatus
nsNamedPipeSync(PRFileDesc* aFd)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  NamedPipeInfo* info = GetNamedPipeInfo(aFd);
  if (!info) {
    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
    return PR_FAILURE;
  }
  return info->Sync(0) ? PR_SUCCESS : PR_FAILURE;
}

static PRInt16
nsNamedPipePoll(PRFileDesc* aFd, PRInt16 aInFlags, PRInt16* aOutFlags)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  NamedPipeInfo* info = GetNamedPipeInfo(aFd);
  if (!info) {
    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
    return 0;
  }
  return info->GetPollFlags(aInFlags, aOutFlags);
}

// FIXME: remove socket option functions?
static PRStatus
nsNamedPipeGetSocketOption(PRFileDesc* aFd, PRSocketOptionData* aData)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  MOZ_ASSERT(aFd);
  MOZ_ASSERT(aData);

  switch (aData->option) {
    case PR_SockOpt_Nonblocking:
      aData->value.non_blocking = GetNamedPipeInfo(aFd)->IsNonblocking()
                                  ? PR_TRUE
                                  : PR_FALSE;
      break;
    case PR_SockOpt_Keepalive:
      aData->value.keep_alive = PR_TRUE;
      break;
    case PR_SockOpt_NoDelay:
      aData->value.no_delay = PR_TRUE;
      break;
    default:
      PR_SetError(PR_INVALID_METHOD_ERROR, 0);
      return PR_FAILURE;
  }

  return PR_SUCCESS;
}

static PRStatus
nsNamedPipeSetSocketOption(PRFileDesc* aFd, const PRSocketOptionData* aData)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  MOZ_ASSERT(aFd);
  MOZ_ASSERT(aData);

  switch (aData->option) {
    case PR_SockOpt_Nonblocking:
      GetNamedPipeInfo(aFd)->SetNonblocking(aData->value.non_blocking);
      break;
    case PR_SockOpt_Keepalive:
    case PR_SockOpt_NoDelay:
      break;
    default:
      PR_SetError(PR_INVALID_METHOD_ERROR, 0);
      return PR_FAILURE;
  }

  return PR_SUCCESS;
}

static void
Initialize()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  static bool initialized = false;
  if (initialized) {
    return;
  }

  nsNamedPipeLayerIdentity = PR_GetUniqueIdentity("Named Pipe layer");
  nsNamedPipeLayerMethods = *PR_GetDefaultIOMethods();
  nsNamedPipeLayerMethods.close = nsNamedPipeClose;
  nsNamedPipeLayerMethods.read = nsNamedPipeRead;
  nsNamedPipeLayerMethods.write = nsNamedPipeWrite;
  nsNamedPipeLayerMethods.available = nsNamedPipeAvailable;
  nsNamedPipeLayerMethods.available64 = nsNamedPipeAvailable64;
  nsNamedPipeLayerMethods.fsync = nsNamedPipeSync;
  nsNamedPipeLayerMethods.connect = nsNamedPipeConnect;
  nsNamedPipeLayerMethods.recv = nsNamedPipeRecv;
  nsNamedPipeLayerMethods.send = nsNamedPipeSend;
  nsNamedPipeLayerMethods.poll = nsNamedPipePoll;
  nsNamedPipeLayerMethods.getsocketoption = nsNamedPipeGetSocketOption;
  nsNamedPipeLayerMethods.setsocketoption = nsNamedPipeSetSocketOption;
  nsNamedPipeLayerMethods.connectcontinue = nsNamedPipeConnectContinue;

  initialized = true;
}

bool
IsNamedPipePath(const nsACString& aPath)
{
  return StringBeginsWith(aPath, NS_LITERAL_CSTRING("\\\\.\\pipe\\"));
}

PRFileDesc*
CreateNamedPipeLayer()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  Initialize();

  PRFileDesc* layer = PR_CreateIOLayerStub(nsNamedPipeLayerIdentity,
                                           &nsNamedPipeLayerMethods);
  if (NS_WARN_IF(!layer)) {
    LOG_NPIO_ERROR("CreateNamedPipeLayer() failed.");
    return nullptr;
  }

  RefPtr<NamedPipeInfo> info = new NamedPipeInfo();
  layer->secret = reinterpret_cast<PRFilePrivate*>(info.forget().take());

  return layer;
}

} // namespace net
} // namespace mozilla
