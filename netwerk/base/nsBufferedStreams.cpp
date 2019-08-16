/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ipc/IPCMessageUtils.h"

#include "nsBufferedStreams.h"
#include "nsStreamUtils.h"
#include "nsNetCID.h"
#include "nsIClassInfoImpl.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include <algorithm>

#ifdef DEBUG_brendan
#  define METERING
#endif

#ifdef METERING
#  include <stdio.h>
#  define METER(x) x
#  define MAX_BIG_SEEKS 20

static struct {
  uint32_t mSeeksWithinBuffer;
  uint32_t mSeeksOutsideBuffer;
  uint32_t mBufferReadUponSeek;
  uint32_t mBufferUnreadUponSeek;
  uint32_t mBytesReadFromBuffer;
  uint32_t mBigSeekIndex;
  struct {
    int64_t mOldOffset;
    int64_t mNewOffset;
  } mBigSeek[MAX_BIG_SEEKS];
} bufstats;
#else
#  define METER(x) /* nothing */
#endif

using namespace mozilla::ipc;
using mozilla::DebugOnly;
using mozilla::Maybe;
using mozilla::MutexAutoLock;
using mozilla::Nothing;
using mozilla::Some;

////////////////////////////////////////////////////////////////////////////////
// nsBufferedStream

nsBufferedStream::nsBufferedStream()
    : mBufferSize(0),
      mBuffer(nullptr),
      mBufferStartOffset(0),
      mCursor(0),
      mFillPoint(0),
      mStream(nullptr),
      mBufferDisabled(false),
      mEOF(false),
      mGetBufferCount(0) {}

nsBufferedStream::~nsBufferedStream() { Close(); }

NS_IMPL_ISUPPORTS(nsBufferedStream, nsITellableStream, nsISeekableStream)

nsresult nsBufferedStream::Init(nsISupports* aStream, uint32_t bufferSize) {
  NS_ASSERTION(aStream, "need to supply a stream");
  NS_ASSERTION(mStream == nullptr, "already inited");
  mStream = aStream;  // we keep a reference until nsBufferedStream::Close
  mBufferSize = bufferSize;
  mBufferStartOffset = 0;
  mCursor = 0;
  mBuffer = new (mozilla::fallible) char[bufferSize];
  if (mBuffer == nullptr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult nsBufferedStream::Close() {
  // Drop the reference from nsBufferedStream::Init()
  mStream = nullptr;
  if (mBuffer) {
    delete[] mBuffer;
    mBuffer = nullptr;
    mBufferSize = 0;
    mBufferStartOffset = 0;
    mCursor = 0;
    mFillPoint = 0;
  }
#ifdef METERING
  {
    static FILE* tfp;
    if (!tfp) {
      tfp = fopen("/tmp/bufstats", "w");
      if (tfp) {
        setvbuf(tfp, nullptr, _IOLBF, 0);
      }
    }
    if (tfp) {
      fprintf(tfp, "seeks within buffer:    %u\n", bufstats.mSeeksWithinBuffer);
      fprintf(tfp, "seeks outside buffer:   %u\n",
              bufstats.mSeeksOutsideBuffer);
      fprintf(tfp, "buffer read on seek:    %u\n",
              bufstats.mBufferReadUponSeek);
      fprintf(tfp, "buffer unread on seek:  %u\n",
              bufstats.mBufferUnreadUponSeek);
      fprintf(tfp, "bytes read from buffer: %u\n",
              bufstats.mBytesReadFromBuffer);
      for (uint32_t i = 0; i < bufstats.mBigSeekIndex; i++) {
        fprintf(tfp, "bigseek[%u] = {old: %u, new: %u}\n", i,
                bufstats.mBigSeek[i].mOldOffset,
                bufstats.mBigSeek[i].mNewOffset);
      }
    }
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsBufferedStream::Seek(int32_t whence, int64_t offset) {
  if (mStream == nullptr) {
    return NS_BASE_STREAM_CLOSED;
  }

  // If the underlying stream isn't a random access store, then fail early.
  // We could possibly succeed for the case where the seek position denotes
  // something that happens to be read into the buffer, but that would make
  // the failure data-dependent.
  nsresult rv;
  nsCOMPtr<nsISeekableStream> ras = do_QueryInterface(mStream, &rv);
  if (NS_FAILED(rv)) {
#ifdef DEBUG
    NS_WARNING("mStream doesn't QI to nsISeekableStream");
#endif
    return rv;
  }

  int64_t absPos = 0;
  switch (whence) {
    case nsISeekableStream::NS_SEEK_SET:
      absPos = offset;
      break;
    case nsISeekableStream::NS_SEEK_CUR:
      absPos = mBufferStartOffset;
      absPos += mCursor;
      absPos += offset;
      break;
    case nsISeekableStream::NS_SEEK_END:
      absPos = -1;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("bogus seek whence parameter");
      return NS_ERROR_UNEXPECTED;
  }

  // Let mCursor point into the existing buffer if the new position is
  // between the current cursor and the mFillPoint "fencepost" -- the
  // client may never get around to a Read or Write after this Seek.
  // Read and Write worry about flushing and filling in that event.
  // But if we're at EOF, make sure to pass the seek through to the
  // underlying stream, because it may have auto-closed itself and
  // needs to reopen.
  uint32_t offsetInBuffer = uint32_t(absPos - mBufferStartOffset);
  if (offsetInBuffer <= mFillPoint && !mEOF) {
    METER(bufstats.mSeeksWithinBuffer++);
    mCursor = offsetInBuffer;
    return NS_OK;
  }

  METER(bufstats.mSeeksOutsideBuffer++);
  METER(bufstats.mBufferReadUponSeek += mCursor);
  METER(bufstats.mBufferUnreadUponSeek += mFillPoint - mCursor);
  rv = Flush();
  if (NS_FAILED(rv)) {
#ifdef DEBUG
    NS_WARNING(
        "(debug) Flush returned error within nsBufferedStream::Seek, so we "
        "exit early.");
#endif
    return rv;
  }

  rv = ras->Seek(whence, offset);
  if (NS_FAILED(rv)) {
#ifdef DEBUG
    NS_WARNING(
        "(debug) Error: ras->Seek() returned error within "
        "nsBufferedStream::Seek, so we exit early.");
#endif
    return rv;
  }

  mEOF = false;

  // Recompute whether the offset we're seeking to is in our buffer.
  // Note that we need to recompute because Flush() might have
  // changed mBufferStartOffset.
  offsetInBuffer = uint32_t(absPos - mBufferStartOffset);
  if (offsetInBuffer <= mFillPoint) {
    // It's safe to just set mCursor to offsetInBuffer.  In particular, we
    // want to avoid calling Fill() here since we already have the data that
    // was seeked to and calling Fill() might auto-close our underlying
    // stream in some cases.
    mCursor = offsetInBuffer;
    return NS_OK;
  }

  METER(if (bufstats.mBigSeekIndex < MAX_BIG_SEEKS)
            bufstats.mBigSeek[bufstats.mBigSeekIndex]
                .mOldOffset = mBufferStartOffset + int64_t(mCursor));
  const int64_t minus1 = -1;
  if (absPos == minus1) {
    // then we had the SEEK_END case, above
    int64_t tellPos;
    rv = ras->Tell(&tellPos);
    mBufferStartOffset = tellPos;
    if (NS_FAILED(rv)) {
      return rv;
    }
  } else {
    mBufferStartOffset = absPos;
  }
  METER(if (bufstats.mBigSeekIndex < MAX_BIG_SEEKS)
            bufstats.mBigSeek[bufstats.mBigSeekIndex++]
                .mNewOffset = mBufferStartOffset);

  mFillPoint = mCursor = 0;
  return Fill();
}

NS_IMETHODIMP
nsBufferedStream::Tell(int64_t* result) {
  if (mStream == nullptr) {
    return NS_BASE_STREAM_CLOSED;
  }

  int64_t result64 = mBufferStartOffset;
  result64 += mCursor;
  *result = result64;
  return NS_OK;
}

NS_IMETHODIMP
nsBufferedStream::SetEOF() {
  if (mStream == nullptr) {
    return NS_BASE_STREAM_CLOSED;
  }

  nsresult rv;
  nsCOMPtr<nsISeekableStream> ras = do_QueryInterface(mStream, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = ras->SetEOF();
  if (NS_SUCCEEDED(rv)) {
    mEOF = true;
  }

  return rv;
}

nsresult nsBufferedStream::GetData(nsISupports** aResult) {
  nsCOMPtr<nsISupports> stream(mStream);
  stream.forget(aResult);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsBufferedInputStream

NS_IMPL_ADDREF_INHERITED(nsBufferedInputStream, nsBufferedStream)
NS_IMPL_RELEASE_INHERITED(nsBufferedInputStream, nsBufferedStream)

NS_IMPL_CLASSINFO(nsBufferedInputStream, nullptr, nsIClassInfo::THREADSAFE,
                  NS_BUFFEREDINPUTSTREAM_CID)

NS_INTERFACE_MAP_BEGIN(nsBufferedInputStream)
  // Unfortunately there isn't a macro that combines ambiguous and conditional,
  // and as far as I can tell, no other class would need such a macro.
  if (mIsAsyncInputStream && aIID.Equals(NS_GET_IID(nsIInputStream))) {
    foundInterface =
        static_cast<nsIInputStream*>(static_cast<nsIAsyncInputStream*>(this));
  } else if (!mIsAsyncInputStream && aIID.Equals(NS_GET_IID(nsIInputStream))) {
    foundInterface = static_cast<nsIInputStream*>(
        static_cast<nsIBufferedInputStream*>(this));
  } else
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIBufferedInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIBufferedInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIStreamBufferAccess)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIIPCSerializableInputStream,
                                     mIsIPCSerializable)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAsyncInputStream, mIsAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIInputStreamCallback,
                                     mIsAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsICloneableInputStream,
                                     mIsCloneableInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIInputStreamLength, mIsInputStreamLength)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAsyncInputStreamLength,
                                     mIsAsyncInputStreamLength)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIInputStreamLengthCallback,
                                     mIsAsyncInputStreamLength)
  NS_IMPL_QUERY_CLASSINFO(nsBufferedInputStream)
NS_INTERFACE_MAP_END_INHERITING(nsBufferedStream)

NS_IMPL_CI_INTERFACE_GETTER(nsBufferedInputStream, nsIInputStream,
                            nsIBufferedInputStream, nsISeekableStream,
                            nsITellableStream, nsIStreamBufferAccess)

nsBufferedInputStream::nsBufferedInputStream()
    : nsBufferedStream(),
      mMutex("nsBufferedInputStream::mMutex"),
      mIsIPCSerializable(true),
      mIsAsyncInputStream(false),
      mIsCloneableInputStream(false),
      mIsInputStreamLength(false),
      mIsAsyncInputStreamLength(false) {}

nsresult nsBufferedInputStream::Create(nsISupports* aOuter, REFNSIID aIID,
                                       void** aResult) {
  NS_ENSURE_NO_AGGREGATION(aOuter);

  RefPtr<nsBufferedInputStream> stream = new nsBufferedInputStream();
  return stream->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsBufferedInputStream::Init(nsIInputStream* stream, uint32_t bufferSize) {
  nsresult rv = nsBufferedStream::Init(stream, bufferSize);
  NS_ENSURE_SUCCESS(rv, rv);

  {
    nsCOMPtr<nsIIPCSerializableInputStream> stream = do_QueryInterface(mStream);
    mIsIPCSerializable = !!stream;
  }

  {
    nsCOMPtr<nsIAsyncInputStream> stream = do_QueryInterface(mStream);
    mIsAsyncInputStream = !!stream;
  }

  {
    nsCOMPtr<nsICloneableInputStream> stream = do_QueryInterface(mStream);
    mIsCloneableInputStream = !!stream;
  }

  {
    nsCOMPtr<nsIInputStreamLength> stream = do_QueryInterface(mStream);
    mIsInputStreamLength = !!stream;
  }

  {
    nsCOMPtr<nsIAsyncInputStreamLength> stream = do_QueryInterface(mStream);
    mIsAsyncInputStreamLength = !!stream;
  }

  return NS_OK;
}

already_AddRefed<nsIInputStream> nsBufferedInputStream::GetInputStream() {
  // A non-null mStream implies Init() has been called.
  MOZ_ASSERT(mStream);

  nsIInputStream* out = nullptr;
  DebugOnly<nsresult> rv = QueryInterface(NS_GET_IID(nsIInputStream),
                                          reinterpret_cast<void**>(&out));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(out);

  return already_AddRefed<nsIInputStream>(out);
}

NS_IMETHODIMP
nsBufferedInputStream::Close() {
  nsresult rv1 = NS_OK, rv2;
  if (mStream) {
    rv1 = Source()->Close();
#ifdef DEBUG
    if (NS_FAILED(rv1)) {
      NS_WARNING(
          "(debug) Error: Source()->Close() returned error (rv1) in "
          "bsBuffedInputStream::Close().");
    };
#endif
  }

  rv2 = nsBufferedStream::Close();

#ifdef DEBUG
  if (NS_FAILED(rv2)) {
    NS_WARNING(
        "(debug) Error: nsBufferedStream::Close() returned error (rv2) within "
        "nsBufferedInputStream::Close().");
  };
#endif

  if (NS_FAILED(rv1)) {
    return rv1;
  }
  return rv2;
}

NS_IMETHODIMP
nsBufferedInputStream::Available(uint64_t* result) {
  *result = 0;

  if (!mStream) {
    return NS_OK;
  }

  uint64_t avail = mFillPoint - mCursor;

  uint64_t tmp;
  nsresult rv = Source()->Available(&tmp);
  if (NS_SUCCEEDED(rv)) {
    avail += tmp;
  }

  if (avail) {
    *result = avail;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
nsBufferedInputStream::Read(char* buf, uint32_t count, uint32_t* result) {
  if (mBufferDisabled) {
    if (!mStream) {
      *result = 0;
      return NS_OK;
    }
    nsresult rv = Source()->Read(buf, count, result);
    if (NS_SUCCEEDED(rv)) {
      mBufferStartOffset += *result;  // so nsBufferedStream::Tell works
      if (*result == 0) {
        mEOF = true;
      }
    }
    return rv;
  }

  return ReadSegments(NS_CopySegmentToBuffer, buf, count, result);
}

NS_IMETHODIMP
nsBufferedInputStream::ReadSegments(nsWriteSegmentFun writer, void* closure,
                                    uint32_t count, uint32_t* result) {
  *result = 0;

  if (!mStream) {
    return NS_OK;
  }

  nsresult rv = NS_OK;
  while (count > 0) {
    uint32_t amt = std::min(count, mFillPoint - mCursor);
    if (amt > 0) {
      uint32_t read = 0;
      rv = writer(static_cast<nsIBufferedInputStream*>(this), closure,
                  mBuffer + mCursor, *result, amt, &read);
      if (NS_FAILED(rv)) {
        // errors returned from the writer end here!
        rv = NS_OK;
        break;
      }
      *result += read;
      count -= read;
      mCursor += read;
    } else {
      rv = Fill();
      if (NS_FAILED(rv) || mFillPoint == mCursor) {
        break;
      }
    }
  }
  return (*result > 0) ? NS_OK : rv;
}

NS_IMETHODIMP
nsBufferedInputStream::IsNonBlocking(bool* aNonBlocking) {
  if (mStream) {
    return Source()->IsNonBlocking(aNonBlocking);
  }
  return NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
nsBufferedInputStream::Fill() {
  if (mBufferDisabled) {
    return NS_OK;
  }
  NS_ENSURE_TRUE(mStream, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  int32_t rem = int32_t(mFillPoint - mCursor);
  if (rem > 0) {
    // slide the remainder down to the start of the buffer
    // |<------------->|<--rem-->|<--->|
    // b               c         f     s
    memcpy(mBuffer, mBuffer + mCursor, rem);
  }
  mBufferStartOffset += mCursor;
  mFillPoint = rem;
  mCursor = 0;

  uint32_t amt;
  rv = Source()->Read(mBuffer + mFillPoint, mBufferSize - mFillPoint, &amt);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (amt == 0) {
    mEOF = true;
  }

  mFillPoint += amt;
  return NS_OK;
}

NS_IMETHODIMP_(char*)
nsBufferedInputStream::GetBuffer(uint32_t aLength, uint32_t aAlignMask) {
  NS_ASSERTION(mGetBufferCount == 0, "nested GetBuffer!");
  if (mGetBufferCount != 0) {
    return nullptr;
  }

  if (mBufferDisabled) {
    return nullptr;
  }

  char* buf = mBuffer + mCursor;
  uint32_t rem = mFillPoint - mCursor;
  if (rem == 0) {
    if (NS_FAILED(Fill())) {
      return nullptr;
    }
    buf = mBuffer + mCursor;
    rem = mFillPoint - mCursor;
  }

  uint32_t mod = (NS_PTR_TO_INT32(buf) & aAlignMask);
  if (mod) {
    uint32_t pad = aAlignMask + 1 - mod;
    if (pad > rem) {
      return nullptr;
    }

    memset(buf, 0, pad);
    mCursor += pad;
    buf += pad;
    rem -= pad;
  }

  if (aLength > rem) {
    return nullptr;
  }
  mGetBufferCount++;
  return buf;
}

NS_IMETHODIMP_(void)
nsBufferedInputStream::PutBuffer(char* aBuffer, uint32_t aLength) {
  NS_ASSERTION(mGetBufferCount == 1, "stray PutBuffer!");
  if (--mGetBufferCount != 0) {
    return;
  }

  NS_ASSERTION(mCursor + aLength <= mFillPoint, "PutBuffer botch");
  mCursor += aLength;
}

NS_IMETHODIMP
nsBufferedInputStream::DisableBuffering() {
  NS_ASSERTION(!mBufferDisabled, "redundant call to DisableBuffering!");
  NS_ASSERTION(mGetBufferCount == 0,
               "DisableBuffer call between GetBuffer and PutBuffer!");
  if (mGetBufferCount != 0) {
    return NS_ERROR_UNEXPECTED;
  }

  // Empty the buffer so nsBufferedStream::Tell works.
  mBufferStartOffset += mCursor;
  mFillPoint = mCursor = 0;
  mBufferDisabled = true;
  return NS_OK;
}

NS_IMETHODIMP
nsBufferedInputStream::EnableBuffering() {
  NS_ASSERTION(mBufferDisabled, "gratuitous call to EnableBuffering!");
  mBufferDisabled = false;
  return NS_OK;
}

NS_IMETHODIMP
nsBufferedInputStream::GetUnbufferedStream(nsISupports** aStream) {
  // Empty the buffer so subsequent i/o trumps any buffered data.
  mBufferStartOffset += mCursor;
  mFillPoint = mCursor = 0;

  nsCOMPtr<nsISupports> stream = mStream;
  stream.forget(aStream);
  return NS_OK;
}

void nsBufferedInputStream::Serialize(InputStreamParams& aParams,
                                      FileDescriptorArray& aFileDescriptors,
                                      bool aDelayedStart, uint32_t aMaxSize,
                                      uint32_t* aSizeUsed,
                                      mozilla::dom::ContentChild* aManager) {
  SerializeInternal(aParams, aFileDescriptors, aDelayedStart, aMaxSize,
                    aSizeUsed, aManager);
}

void nsBufferedInputStream::Serialize(InputStreamParams& aParams,
                                      FileDescriptorArray& aFileDescriptors,
                                      bool aDelayedStart, uint32_t aMaxSize,
                                      uint32_t* aSizeUsed,
                                      PBackgroundChild* aManager) {
  SerializeInternal(aParams, aFileDescriptors, aDelayedStart, aMaxSize,
                    aSizeUsed, aManager);
}

void nsBufferedInputStream::Serialize(InputStreamParams& aParams,
                                      FileDescriptorArray& aFileDescriptors,
                                      bool aDelayedStart, uint32_t aMaxSize,
                                      uint32_t* aSizeUsed,
                                      mozilla::dom::ContentParent* aManager) {
  SerializeInternal(aParams, aFileDescriptors, aDelayedStart, aMaxSize,
                    aSizeUsed, aManager);
}

void nsBufferedInputStream::Serialize(InputStreamParams& aParams,
                                      FileDescriptorArray& aFileDescriptors,
                                      bool aDelayedStart, uint32_t aMaxSize,
                                      uint32_t* aSizeUsed,
                                      PBackgroundParent* aManager) {
  SerializeInternal(aParams, aFileDescriptors, aDelayedStart, aMaxSize,
                    aSizeUsed, aManager);
}

template <typename M>
void nsBufferedInputStream::SerializeInternal(
    InputStreamParams& aParams, FileDescriptorArray& aFileDescriptors,
    bool aDelayedStart, uint32_t aMaxSize, uint32_t* aSizeUsed, M* aManager) {
  MOZ_ASSERT(aSizeUsed);
  *aSizeUsed = 0;

  BufferedInputStreamParams params;

  if (mStream) {
    nsCOMPtr<nsIInputStream> stream = do_QueryInterface(mStream);
    MOZ_ASSERT(stream);

    InputStreamParams wrappedParams;
    InputStreamHelper::SerializeInputStream(stream, wrappedParams,
                                            aFileDescriptors, aDelayedStart,
                                            aMaxSize, aSizeUsed, aManager);

    params.optionalStream().emplace(wrappedParams);
  }

  params.bufferSize() = mBufferSize;

  aParams = params;
}

bool nsBufferedInputStream::Deserialize(
    const InputStreamParams& aParams,
    const FileDescriptorArray& aFileDescriptors) {
  if (aParams.type() != InputStreamParams::TBufferedInputStreamParams) {
    NS_ERROR("Received unknown parameters from the other process!");
    return false;
  }

  const BufferedInputStreamParams& params =
      aParams.get_BufferedInputStreamParams();
  const Maybe<InputStreamParams>& wrappedParams = params.optionalStream();

  nsCOMPtr<nsIInputStream> stream;
  if (wrappedParams.isSome()) {
    stream = InputStreamHelper::DeserializeInputStream(wrappedParams.ref(),
                                                       aFileDescriptors);
    if (!stream) {
      NS_WARNING("Failed to deserialize wrapped stream!");
      return false;
    }
  }

  nsresult rv = Init(stream, params.bufferSize());
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

NS_IMETHODIMP
nsBufferedInputStream::CloseWithStatus(nsresult aStatus) { return Close(); }

NS_IMETHODIMP
nsBufferedInputStream::AsyncWait(nsIInputStreamCallback* aCallback,
                                 uint32_t aFlags, uint32_t aRequestedCount,
                                 nsIEventTarget* aEventTarget) {
  nsCOMPtr<nsIAsyncInputStream> stream = do_QueryInterface(mStream);
  if (!stream) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIInputStreamCallback> callback = aCallback ? this : nullptr;
  {
    MutexAutoLock lock(mMutex);

    if (mAsyncWaitCallback && aCallback) {
      return NS_ERROR_FAILURE;
    }

    mAsyncWaitCallback = aCallback;
  }

  return stream->AsyncWait(callback, aFlags, aRequestedCount, aEventTarget);
}

NS_IMETHODIMP
nsBufferedInputStream::OnInputStreamReady(nsIAsyncInputStream* aStream) {
  nsCOMPtr<nsIInputStreamCallback> callback;
  {
    MutexAutoLock lock(mMutex);

    // We have been canceled in the meanwhile.
    if (!mAsyncWaitCallback) {
      return NS_OK;
    }

    callback.swap(mAsyncWaitCallback);
  }

  MOZ_ASSERT(callback);
  return callback->OnInputStreamReady(this);
}

NS_IMETHODIMP
nsBufferedInputStream::GetData(nsIInputStream** aResult) {
  nsCOMPtr<nsISupports> stream;
  nsBufferedStream::GetData(getter_AddRefs(stream));
  nsCOMPtr<nsIInputStream> inputStream = do_QueryInterface(stream);
  inputStream.forget(aResult);
  return NS_OK;
}

// nsICloneableInputStream interface

NS_IMETHODIMP
nsBufferedInputStream::GetCloneable(bool* aCloneable) {
  *aCloneable = false;

  // If we don't have the buffer, the inputStream has been already closed.
  // If mBufferStartOffset is not 0, the stream has been seeked or read.
  // In both case the cloning is not supported.
  if (!mBuffer || mBufferStartOffset) {
    return NS_OK;
  }

  nsCOMPtr<nsICloneableInputStream> stream = do_QueryInterface(mStream);

  // GetCloneable is infallible.
  NS_ENSURE_TRUE(stream, NS_OK);

  return stream->GetCloneable(aCloneable);
}

NS_IMETHODIMP
nsBufferedInputStream::Clone(nsIInputStream** aResult) {
  if (!mBuffer || mBufferStartOffset) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsICloneableInputStream> stream = do_QueryInterface(mStream);
  NS_ENSURE_TRUE(stream, NS_ERROR_FAILURE);

  nsCOMPtr<nsIInputStream> clonedStream;
  nsresult rv = stream->Clone(getter_AddRefs(clonedStream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIBufferedInputStream> bis = new nsBufferedInputStream();
  rv = bis->Init(clonedStream, mBufferSize);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult =
      static_cast<nsBufferedInputStream*>(bis.get())->GetInputStream().take();

  return NS_OK;
}

// nsIInputStreamLength

NS_IMETHODIMP
nsBufferedInputStream::Length(int64_t* aLength) {
  nsCOMPtr<nsIInputStreamLength> stream = do_QueryInterface(mStream);
  NS_ENSURE_TRUE(stream, NS_ERROR_FAILURE);

  return stream->Length(aLength);
}

// nsIAsyncInputStreamLength

NS_IMETHODIMP
nsBufferedInputStream::AsyncLengthWait(nsIInputStreamLengthCallback* aCallback,
                                       nsIEventTarget* aEventTarget) {
  nsCOMPtr<nsIAsyncInputStreamLength> stream = do_QueryInterface(mStream);
  if (!stream) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIInputStreamLengthCallback> callback = aCallback ? this : nullptr;
  {
    MutexAutoLock lock(mMutex);
    mAsyncInputStreamLengthCallback = aCallback;
  }

  MOZ_ASSERT(stream);
  return stream->AsyncLengthWait(callback, aEventTarget);
}

// nsIInputStreamLengthCallback

NS_IMETHODIMP
nsBufferedInputStream::OnInputStreamLengthReady(
    nsIAsyncInputStreamLength* aStream, int64_t aLength) {
  nsCOMPtr<nsIInputStreamLengthCallback> callback;
  {
    MutexAutoLock lock(mMutex);
    // We have been canceled in the meanwhile.
    if (!mAsyncInputStreamLengthCallback) {
      return NS_OK;
    }

    callback.swap(mAsyncInputStreamLengthCallback);
  }

  MOZ_ASSERT(callback);
  return callback->OnInputStreamLengthReady(this, aLength);
}

////////////////////////////////////////////////////////////////////////////////
// nsBufferedOutputStream

NS_IMPL_ADDREF_INHERITED(nsBufferedOutputStream, nsBufferedStream)
NS_IMPL_RELEASE_INHERITED(nsBufferedOutputStream, nsBufferedStream)
// This QI uses NS_INTERFACE_MAP_ENTRY_CONDITIONAL to check for
// non-nullness of mSafeStream.
NS_INTERFACE_MAP_BEGIN(nsBufferedOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsIOutputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISafeOutputStream, mSafeStream)
  NS_INTERFACE_MAP_ENTRY(nsIBufferedOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsIStreamBufferAccess)
NS_INTERFACE_MAP_END_INHERITING(nsBufferedStream)

nsresult nsBufferedOutputStream::Create(nsISupports* aOuter, REFNSIID aIID,
                                        void** aResult) {
  NS_ENSURE_NO_AGGREGATION(aOuter);

  RefPtr<nsBufferedOutputStream> stream = new nsBufferedOutputStream();
  return stream->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsBufferedOutputStream::Init(nsIOutputStream* stream, uint32_t bufferSize) {
  // QI stream to an nsISafeOutputStream, to see if we should support it
  mSafeStream = do_QueryInterface(stream);

  return nsBufferedStream::Init(stream, bufferSize);
}

NS_IMETHODIMP
nsBufferedOutputStream::Close() {
  nsresult rv1, rv2 = NS_OK, rv3;

  rv1 = Flush();

#ifdef DEBUG
  if (NS_FAILED(rv1)) {
    NS_WARNING(
        "(debug) Flush() inside nsBufferedOutputStream::Close() returned error "
        "(rv1).");
  }
#endif

  // If we fail to Flush all the data, then we close anyway and drop the
  // remaining data in the buffer. We do this because it's what Unix does
  // for fclose and close. However, we report the error from Flush anyway.
  if (mStream) {
    rv2 = Sink()->Close();
#ifdef DEBUG
    if (NS_FAILED(rv2)) {
      NS_WARNING(
          "(debug) Sink->Close() inside nsBufferedOutputStream::Close() "
          "returned error (rv2).");
    }
#endif
  }
  rv3 = nsBufferedStream::Close();

#ifdef DEBUG
  if (NS_FAILED(rv3)) {
    NS_WARNING(
        "(debug) nsBufferedStream:Close() inside "
        "nsBufferedOutputStream::Close() returned error (rv3).");
  }
#endif

  if (NS_FAILED(rv1)) {
    return rv1;
  }
  if (NS_FAILED(rv2)) {
    return rv2;
  }
  return rv3;
}

NS_IMETHODIMP
nsBufferedOutputStream::Write(const char* buf, uint32_t count,
                              uint32_t* result) {
  nsresult rv = NS_OK;
  uint32_t written = 0;
  *result = 0;
  if (!mStream) {
    // We special case this situtaion.
    // We should catch the failure, NS_BASE_STREAM_CLOSED ASAP, here.
    // If we don't, eventually Flush() is called in the while loop below
    // after so many writes.
    // However, Flush() returns NS_OK when mStream is null (!!),
    // and we don't get a meaningful error, NS_BASE_STREAM_CLOSED,
    // soon enough when we use buffered output.
#ifdef DEBUG
    NS_WARNING(
        "(info) nsBufferedOutputStream::Write returns NS_BASE_STREAM_CLOSED "
        "immediately (mStream==null).");
#endif
    return NS_BASE_STREAM_CLOSED;
  }

  while (count > 0) {
    uint32_t amt = std::min(count, mBufferSize - mCursor);
    if (amt > 0) {
      memcpy(mBuffer + mCursor, buf + written, amt);
      written += amt;
      count -= amt;
      mCursor += amt;
      if (mFillPoint < mCursor) mFillPoint = mCursor;
    } else {
      NS_ASSERTION(mFillPoint, "loop in nsBufferedOutputStream::Write!");
      rv = Flush();
      if (NS_FAILED(rv)) {
#ifdef DEBUG
        NS_WARNING(
            "(debug) Flush() returned error in nsBufferedOutputStream::Write.");
#endif
        break;
      }
    }
  }
  *result = written;
  return (written > 0) ? NS_OK : rv;
}

NS_IMETHODIMP
nsBufferedOutputStream::Flush() {
  nsresult rv;
  uint32_t amt;
  if (!mStream) {
    // Stream already cancelled/flushed; probably because of previous error.
    return NS_OK;
  }
  // optimize : some code within C-C needs to call Seek -> Flush() often.
  if (mFillPoint == 0) {
    return NS_OK;
  }
  rv = Sink()->Write(mBuffer, mFillPoint, &amt);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mBufferStartOffset += amt;
  if (amt == mFillPoint) {
    mFillPoint = mCursor = 0;
    return NS_OK;  // flushed everything
  }

  // slide the remainder down to the start of the buffer
  // |<-------------->|<---|----->|
  // b                a    c      s
  uint32_t rem = mFillPoint - amt;
  memmove(mBuffer, mBuffer + amt, rem);
  mFillPoint = mCursor = rem;
  return NS_ERROR_FAILURE;  // didn't flush all
}

// nsISafeOutputStream
NS_IMETHODIMP
nsBufferedOutputStream::Finish() {
  // flush the stream, to write out any buffered data...
  nsresult rv1 = nsBufferedOutputStream::Flush();
  nsresult rv2 = NS_OK, rv3;

  if (NS_FAILED(rv1)) {
    NS_WARNING(
        "(debug) nsBufferedOutputStream::Flush() failed in "
        "nsBufferedOutputStream::Finish()! Possible dataloss.");

    rv2 = Sink()->Close();
    if (NS_FAILED(rv2)) {
      NS_WARNING(
          "(debug) Sink()->Close() failed in nsBufferedOutputStream::Finish()! "
          "Possible dataloss.");
    }
  } else {
    rv2 = mSafeStream->Finish();
    if (NS_FAILED(rv2)) {
      NS_WARNING(
          "(debug) mSafeStream->Finish() failed within "
          "nsBufferedOutputStream::Flush()! Possible dataloss.");
    }
  }

  // ... and close the buffered stream, so any further attempts to flush/close
  // the buffered stream won't cause errors.
  rv3 = nsBufferedStream::Close();

  // We want to return the errors precisely from Finish()
  // and mimick the existing error handling in
  // nsBufferedOutputStream::Close() as reference.

  if (NS_FAILED(rv1)) {
    return rv1;
  }
  if (NS_FAILED(rv2)) {
    return rv2;
  }
  return rv3;
}

static nsresult nsReadFromInputStream(nsIOutputStream* outStr, void* closure,
                                      char* toRawSegment, uint32_t offset,
                                      uint32_t count, uint32_t* readCount) {
  nsIInputStream* fromStream = (nsIInputStream*)closure;
  return fromStream->Read(toRawSegment, count, readCount);
}

NS_IMETHODIMP
nsBufferedOutputStream::WriteFrom(nsIInputStream* inStr, uint32_t count,
                                  uint32_t* _retval) {
  return WriteSegments(nsReadFromInputStream, inStr, count, _retval);
}

NS_IMETHODIMP
nsBufferedOutputStream::WriteSegments(nsReadSegmentFun reader, void* closure,
                                      uint32_t count, uint32_t* _retval) {
  *_retval = 0;
  nsresult rv;
  while (count > 0) {
    uint32_t left = std::min(count, mBufferSize - mCursor);
    if (left == 0) {
      rv = Flush();
      if (NS_FAILED(rv)) {
        return (*_retval > 0) ? NS_OK : rv;
      }

      continue;
    }

    uint32_t read = 0;
    rv = reader(this, closure, mBuffer + mCursor, *_retval, left, &read);

    if (NS_FAILED(rv)) {  // If we have read some data, return ok
      return (*_retval > 0) ? NS_OK : rv;
    }
    mCursor += read;
    *_retval += read;
    count -= read;
    mFillPoint = std::max(mFillPoint, mCursor);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBufferedOutputStream::IsNonBlocking(bool* aNonBlocking) {
  if (mStream) {
    return Sink()->IsNonBlocking(aNonBlocking);
  }
  return NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP_(char*)
nsBufferedOutputStream::GetBuffer(uint32_t aLength, uint32_t aAlignMask) {
  NS_ASSERTION(mGetBufferCount == 0, "nested GetBuffer!");
  if (mGetBufferCount != 0) {
    return nullptr;
  }

  if (mBufferDisabled) {
    return nullptr;
  }

  char* buf = mBuffer + mCursor;
  uint32_t rem = mBufferSize - mCursor;
  if (rem == 0) {
    if (NS_FAILED(Flush())) {
      return nullptr;
    }
    buf = mBuffer + mCursor;
    rem = mBufferSize - mCursor;
  }

  uint32_t mod = (NS_PTR_TO_INT32(buf) & aAlignMask);
  if (mod) {
    uint32_t pad = aAlignMask + 1 - mod;
    if (pad > rem) {
      return nullptr;
    }

    memset(buf, 0, pad);
    mCursor += pad;
    buf += pad;
    rem -= pad;
  }

  if (aLength > rem) {
    return nullptr;
  }
  mGetBufferCount++;
  return buf;
}

NS_IMETHODIMP_(void)
nsBufferedOutputStream::PutBuffer(char* aBuffer, uint32_t aLength) {
  NS_ASSERTION(mGetBufferCount == 1, "stray PutBuffer!");
  if (--mGetBufferCount != 0) {
    return;
  }

  NS_ASSERTION(mCursor + aLength <= mBufferSize, "PutBuffer botch");
  mCursor += aLength;
  if (mFillPoint < mCursor) {
    mFillPoint = mCursor;
  }
}

NS_IMETHODIMP
nsBufferedOutputStream::DisableBuffering() {
  NS_ASSERTION(!mBufferDisabled, "redundant call to DisableBuffering!");
  NS_ASSERTION(mGetBufferCount == 0,
               "DisableBuffer call between GetBuffer and PutBuffer!");
  if (mGetBufferCount != 0) {
    return NS_ERROR_UNEXPECTED;
  }

  // Empty the buffer so nsBufferedStream::Tell works.
  nsresult rv = Flush();
  if (NS_FAILED(rv)) {
    return rv;
  }

  mBufferDisabled = true;
  return NS_OK;
}

NS_IMETHODIMP
nsBufferedOutputStream::EnableBuffering() {
  NS_ASSERTION(mBufferDisabled, "gratuitous call to EnableBuffering!");
  mBufferDisabled = false;
  return NS_OK;
}

NS_IMETHODIMP
nsBufferedOutputStream::GetUnbufferedStream(nsISupports** aStream) {
  // Empty the buffer so subsequent i/o trumps any buffered data.
  if (mFillPoint) {
    nsresult rv = Flush();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  nsCOMPtr<nsISupports> stream = mStream;
  stream.forget(aStream);
  return NS_OK;
}

NS_IMETHODIMP
nsBufferedOutputStream::GetData(nsIOutputStream** aResult) {
  nsCOMPtr<nsISupports> stream;
  nsBufferedStream::GetData(getter_AddRefs(stream));
  nsCOMPtr<nsIOutputStream> outputStream = do_QueryInterface(stream);
  outputStream.forget(aResult);
  return NS_OK;
}
#undef METER

////////////////////////////////////////////////////////////////////////////////
