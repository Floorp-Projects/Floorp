/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SeekableStreamWrapper.h"

#include "mozilla/InputStreamLengthHelper.h"
#include "nsStreamUtils.h"

namespace mozilla {

/* static */
nsresult SeekableStreamWrapper::MaybeWrap(
    already_AddRefed<nsIInputStream> aOriginal, nsIInputStream** aResult) {
  // First, check if the original stream is already seekable.
  nsCOMPtr<nsIInputStream> original(aOriginal);
  if (nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(original)) {
    original.forget(aResult);
    return NS_OK;
  }

  // If the original stream was not seekable, clone it. This may create a
  // replacement stream using `nsPipe` if the original stream was not cloneable.
  nsCOMPtr<nsIInputStream> clone;
  nsCOMPtr<nsIInputStream> replacement;
  nsresult rv = NS_CloneInputStream(original, getter_AddRefs(clone),
                                    getter_AddRefs(replacement));
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (replacement) {
    original = replacement.forget();
  }

  // Our replacement or original must be cloneable if `NS_CloneInputStream`
  // succeeded.
  nsCOMPtr<nsICloneableInputStream> cloneable = do_QueryInterface(original);
  MOZ_DIAGNOSTIC_ASSERT(cloneable && cloneable->GetCloneable(),
                        "Result of NS_CloneInputStream must be cloneable");

  nsCOMPtr<nsIAsyncInputStream> stream =
      new SeekableStreamWrapper(cloneable, clone);
  stream.forget(aResult);
  return NS_OK;
}

template <typename T>
nsCOMPtr<T> SeekableStreamWrapper::StreamAs() {
  MutexAutoLock lock(mMutex);
  if constexpr (std::is_same_v<T, nsIInputStream>) {
    return nsCOMPtr<T>(mCurrent);
  } else {
    return nsCOMPtr<T>(do_QueryInterface(mCurrent));
  }
}

// nsIInputStream

NS_IMETHODIMP
SeekableStreamWrapper::Close() {
  auto stream = StreamAs<nsIInputStream>();
  NS_ENSURE_STATE(stream);
  nsresult rv1 = stream->Close();

  nsresult rv2 = NS_ERROR_NOT_IMPLEMENTED;
  if (nsCOMPtr<nsIInputStream> original = do_QueryInterface(mOriginal)) {
    rv2 = original->Close();
  }

  return NS_FAILED(rv1) ? rv1 : rv2;
}

NS_IMETHODIMP
SeekableStreamWrapper::Available(uint64_t* aLength) {
  auto stream = StreamAs<nsIInputStream>();
  NS_ENSURE_STATE(stream);
  return stream->Available(aLength);
}

NS_IMETHODIMP
SeekableStreamWrapper::Read(char* aBuffer, uint32_t aCount,
                            uint32_t* aReadCount) {
  auto stream = StreamAs<nsIInputStream>();
  NS_ENSURE_STATE(stream);
  return stream->Read(aBuffer, aCount, aReadCount);
}

namespace {
struct WriterClosure {
  nsIInputStream* mStream;
  nsWriteSegmentFun mInnerFun;
  void* mInnerClosure;
};

static nsresult WriterCb(nsIInputStream* aInStream, void* aClosure,
                         const char* aFromSegment, uint32_t aToOffset,
                         uint32_t aCount, uint32_t* aWriteCount) {
  auto* closure = static_cast<WriterClosure*>(aClosure);
  return (closure->mInnerFun)(closure->mStream, closure->mInnerClosure,
                              aFromSegment, aToOffset, aCount, aWriteCount);
}
}  // namespace

NS_IMETHODIMP
SeekableStreamWrapper::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                    uint32_t aCount, uint32_t* aResult) {
  auto stream = StreamAs<nsIInputStream>();
  NS_ENSURE_STATE(stream);
  WriterClosure innerClosure{static_cast<nsIAsyncInputStream*>(this), aWriter,
                             aClosure};
  return stream->ReadSegments(WriterCb, &innerClosure, aCount, aResult);
}

NS_IMETHODIMP
SeekableStreamWrapper::IsNonBlocking(bool* aNonBlocking) {
  auto stream = StreamAs<nsIInputStream>();
  NS_ENSURE_STATE(stream);
  return stream->IsNonBlocking(aNonBlocking);
}

// nsICloneableInputStream

NS_IMETHODIMP
SeekableStreamWrapper::GetCloneable(bool* aCloneable) {
  auto stream = StreamAs<nsICloneableInputStream>();
  NS_ENSURE_STATE(stream);
  return stream->GetCloneable(aCloneable);
}

NS_IMETHODIMP
SeekableStreamWrapper::Clone(nsIInputStream** aResult) {
  NS_ENSURE_STATE(mOriginal);
  NS_ENSURE_STATE(mOriginal->GetCloneable());

  nsCOMPtr<nsIInputStream> originalClone;
  nsresult rv = mOriginal->Clone(getter_AddRefs(originalClone));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsICloneableInputStream> newOriginal =
      do_QueryInterface(originalClone);
  if (!newOriginal || !newOriginal->GetCloneable()) {
    return NS_ERROR_FAILURE;
  }

  auto cloneable = StreamAs<nsICloneableInputStream>();
  if (!cloneable || !cloneable->GetCloneable()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIInputStream> newClone;
  rv = cloneable->Clone(getter_AddRefs(newClone));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIAsyncInputStream> result =
      new SeekableStreamWrapper(newOriginal, newClone);
  result.forget(aResult);
  return NS_OK;
}

// nsIAsyncInputStream

NS_IMETHODIMP
SeekableStreamWrapper::CloseWithStatus(nsresult aStatus) {
  auto stream = StreamAs<nsIAsyncInputStream>();
  NS_ENSURE_STATE(stream);

  nsresult rv1 = stream->CloseWithStatus(aStatus);
  nsresult rv2 = NS_ERROR_NOT_IMPLEMENTED;
  if (nsCOMPtr<nsIAsyncInputStream> original = do_QueryInterface(mOriginal)) {
    rv2 = original->CloseWithStatus(aStatus);
  }
  return NS_FAILED(rv1) ? rv1 : rv2;
}

class AsyncWaitCallback final : public nsIInputStreamCallback {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAMCALLBACK

 public:
  AsyncWaitCallback(nsIInputStreamCallback* aCallback,
                    nsIAsyncInputStream* aStream)
      : mCallback(aCallback), mStream(aStream) {}

 private:
  ~AsyncWaitCallback() = default;

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIAsyncInputStream> mStream;
};

NS_IMPL_ISUPPORTS(AsyncWaitCallback, nsIInputStreamCallback)

NS_IMETHODIMP
AsyncWaitCallback::OnInputStreamReady(nsIAsyncInputStream*) {
  return mCallback->OnInputStreamReady(mStream);
}

NS_IMETHODIMP
SeekableStreamWrapper::AsyncWait(nsIInputStreamCallback* aCallback,
                                 uint32_t aFlags, uint32_t aRequestedCount,
                                 nsIEventTarget* aEventTarget) {
  auto stream = StreamAs<nsIAsyncInputStream>();
  NS_ENSURE_TRUE(stream, NS_ERROR_NOT_IMPLEMENTED);

  nsCOMPtr<nsIInputStreamCallback> callback;
  if (aCallback) {
    callback = new AsyncWaitCallback(aCallback, this);
  }
  return stream->AsyncWait(callback, aFlags, aRequestedCount, aEventTarget);
}

// nsITellableStream

NS_IMETHODIMP
SeekableStreamWrapper::Tell(int64_t* aResult) {
  auto stream = StreamAs<nsITellableStream>();
  NS_ENSURE_STATE(stream);
  return stream->Tell(aResult);
}

// nsISeekableStream

NS_IMETHODIMP
SeekableStreamWrapper::Seek(int32_t aWhence, int64_t aOffset) {
  // Check if our original stream has already been closed, in which case we
  // can't seek anymore.
  nsCOMPtr<nsIInputStream> original = do_QueryInterface(mOriginal);
  uint64_t ignored = 0;
  nsresult rv = original->Available(&ignored);
  if (NS_FAILED(rv)) {
    return rv;
  }

  int64_t offset = aOffset;
  nsCOMPtr<nsIInputStream> origin;
  switch (aWhence) {
    // With NS_SEEK_SET, offset relative to our original stream.
    case NS_SEEK_SET:
      break;

    // With NS_SEEK_CUR, if the offset is positive, perform it relative to our
    // current stream, otherwise perform it relative to the original stream,
    // offset by the result of `Tell`-ing our current stream.
    case NS_SEEK_CUR: {
      auto current = StreamAs<nsIInputStream>();
      if (offset >= 0) {
        origin = current.forget();
        break;
      }

      nsCOMPtr<nsITellableStream> tellable = do_QueryInterface(current);
      if (!tellable) {
        return NS_ERROR_NOT_IMPLEMENTED;
      }

      int64_t cur = 0;
      rv = tellable->Tell(&cur);
      if (NS_FAILED(rv)) {
        return rv;
      }
      offset += cur;
      break;
    }

    // We have no way of knowing what the end of the stream is, so we can't
    // implement this right now.
    case NS_SEEK_END:
      return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (offset < 0) {
    NS_WARNING("cannot seek before beginning of stream");
    return NS_ERROR_INVALID_ARG;
  }

  // If we aren't starting from our existing stream, clone a new stream from
  // `mOriginal`.
  if (!origin) {
    rv = mOriginal->Clone(getter_AddRefs(origin));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  MOZ_ASSERT(origin);

  // If we have an offset from our origin to read to, use `ReadSegments` with
  // `NS_DiscardSegment` to read to that point in the stream.
  if (offset > 0) {
    uint64_t available = 0;
    rv = origin->Available(&available);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (available < uint64_t(offset)) {
      // The current implementation of Seek cannot handle seeking into
      // unavailable data, whether that's because it's past the end of the input
      // stream, or the data isn't available yet.
      NS_WARNING("cannot seek past the end of the currently avaliable data");
      return NS_ERROR_INVALID_ARG;
    }

    // Discard segments from the input stream.
    uint32_t written = 0;
    rv = origin->ReadSegments(NS_DiscardSegment, nullptr, offset, &written);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (written != offset) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }
  }

  // Actually swap out our `nsIInputStream`.
  nsCOMPtr<nsIInputStream> toClose;
  {
    MutexAutoLock lock(mMutex);
    if (mCurrent != origin) {
      toClose = mCurrent.forget();
      mCurrent = origin.forget();
    }
    MOZ_ASSERT(mCurrent);
  }
  if (toClose) {
    toClose->Close();
  }

  return NS_OK;
}

NS_IMETHODIMP
SeekableStreamWrapper::SetEOF() { return NS_ERROR_NOT_IMPLEMENTED; }

// nsIInputStreamLength

NS_IMETHODIMP
SeekableStreamWrapper::Length(int64_t* aLength) {
  auto stream = StreamAs<nsIInputStreamLength>();
  NS_ENSURE_STATE(stream);
  return stream->Length(aLength);
}

// nsIAsyncInputStreamLength

class AsyncLengthCallback final : public nsIInputStreamLengthCallback {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAMLENGTHCALLBACK

 public:
  AsyncLengthCallback(nsIInputStreamLengthCallback* aCallback,
                      nsIAsyncInputStreamLength* aStream)
      : mCallback(aCallback), mStream(aStream) {}

 private:
  ~AsyncLengthCallback() = default;

  nsCOMPtr<nsIInputStreamLengthCallback> mCallback;
  nsCOMPtr<nsIAsyncInputStreamLength> mStream;
};

NS_IMPL_ISUPPORTS(AsyncLengthCallback, nsIInputStreamLengthCallback)

NS_IMETHODIMP
AsyncLengthCallback::OnInputStreamLengthReady(nsIAsyncInputStreamLength*,
                                              int64_t aLength) {
  return mCallback->OnInputStreamLengthReady(mStream, aLength);
}

NS_IMETHODIMP
SeekableStreamWrapper::AsyncLengthWait(nsIInputStreamLengthCallback* aCallback,
                                       nsIEventTarget* aEventTarget) {
  auto stream = StreamAs<nsIAsyncInputStreamLength>();
  NS_ENSURE_TRUE(stream, NS_ERROR_NOT_IMPLEMENTED);

  nsCOMPtr<nsIInputStreamLengthCallback> callback;
  if (aCallback) {
    callback = new AsyncLengthCallback(aCallback, this);
  }
  return stream->AsyncLengthWait(callback, aEventTarget);
}

// nsIIPCSerializableInputStream

void SeekableStreamWrapper::Serialize(
    mozilla::ipc::InputStreamParams& aParams,
    FileDescriptorArray& aFileDescriptors, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed,
    mozilla::ipc::ParentToChildStreamActorManager* aManager) {
  SerializeInternal(aParams, aFileDescriptors, aDelayedStart, aMaxSize,
                    aSizeUsed, aManager);
}

void SeekableStreamWrapper::Serialize(
    mozilla::ipc::InputStreamParams& aParams,
    FileDescriptorArray& aFileDescriptors, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed,
    mozilla::ipc::ChildToParentStreamActorManager* aManager) {
  SerializeInternal(aParams, aFileDescriptors, aDelayedStart, aMaxSize,
                    aSizeUsed, aManager);
}

template <typename M>
void SeekableStreamWrapper::SerializeInternal(
    mozilla::ipc::InputStreamParams& aParams,
    FileDescriptorArray& aFileDescriptors, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed, M* aManager) {
  // Stream serialization methods generally do not respect the current seek
  // offset when serializing, as is seen in nsStringStream::Serialize. As
  // `StreamAs` may be nullptr and requires acquiring the mutex, instead
  // serialize `mOriginal` into our pipe.
  //
  // Don't serialize context about this being a SeekableStreamWrapper, as the
  // specific interfaces a stream implements, such as nsISeekableStream, are not
  // preserved over IPC.
  MOZ_ASSERT(IsIPCSerializableInputStream());
  nsCOMPtr<nsIInputStream> original = do_QueryInterface(mOriginal);
  mozilla::ipc::InputStreamHelper::SerializeInputStream(
      original, aParams, aFileDescriptors, aDelayedStart, aMaxSize, aSizeUsed,
      aManager);
}

bool SeekableStreamWrapper::Deserialize(
    const mozilla::ipc::InputStreamParams& aParams,
    const FileDescriptorArray& aFileDescriptors) {
  MOZ_CRASH("This method should never be called!");
  return false;
}

// nsIBufferedInputStream

NS_IMETHODIMP
SeekableStreamWrapper::Init(nsIInputStream*, uint32_t) {
  MOZ_CRASH(
      "SeekableStreamWrapper should never be initialized with "
      "nsIBufferedInputStream::Init!\n");
}

NS_IMETHODIMP
SeekableStreamWrapper::GetData(nsIInputStream** aResult) {
  auto stream = StreamAs<nsIBufferedInputStream>();
  NS_ENSURE_STATE(stream);
  return stream->GetData(aResult);
}

// nsISupports

bool SeekableStreamWrapper::IsAsyncInputStream() {
  nsCOMPtr<nsIAsyncInputStream> stream(do_QueryInterface(mOriginal));
  return stream;
}
bool SeekableStreamWrapper::IsInputStreamLength() {
  nsCOMPtr<nsIInputStreamLength> stream(do_QueryInterface(mOriginal));
  return stream;
}
bool SeekableStreamWrapper::IsAsyncInputStreamLength() {
  nsCOMPtr<nsIAsyncInputStreamLength> stream(do_QueryInterface(mOriginal));
  return stream;
}
bool SeekableStreamWrapper::IsIPCSerializableInputStream() {
  nsCOMPtr<nsIIPCSerializableInputStream> stream(do_QueryInterface(mOriginal));
  return stream;
}
bool SeekableStreamWrapper::IsBufferedInputStream() {
  nsCOMPtr<nsIBufferedInputStream> stream(do_QueryInterface(mOriginal));
  return stream;
}

NS_IMPL_ADDREF(SeekableStreamWrapper)
NS_IMPL_RELEASE(SeekableStreamWrapper)
NS_INTERFACE_MAP_BEGIN(SeekableStreamWrapper)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIInputStream, nsIAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY(nsITellableStream)
  NS_INTERFACE_MAP_ENTRY(nsISeekableStream)
  NS_INTERFACE_MAP_ENTRY(nsICloneableInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAsyncInputStream, IsAsyncInputStream())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIInputStreamLength,
                                     IsInputStreamLength())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAsyncInputStreamLength,
                                     IsAsyncInputStreamLength())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIIPCSerializableInputStream,
                                     IsIPCSerializableInputStream())
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIBufferedInputStream,
                                     IsBufferedInputStream())
NS_INTERFACE_MAP_END

}  // namespace mozilla
