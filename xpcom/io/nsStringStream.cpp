/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Based on original code from nsIStringStream.cpp
 */

#include "ipc/IPCMessageUtils.h"

#include "nsStringStream.h"
#include "nsStreamUtils.h"
#include "nsReadableUtils.h"
#include "nsICloneableInputStream.h"
#include "nsISeekableStream.h"
#include "nsISupportsPrimitives.h"
#include "nsCRT.h"
#include "prerror.h"
#include "plstr.h"
#include "nsIClassInfoImpl.h"
#include "mozilla/Attributes.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/StreamBufferSource.h"
#include "nsIIPCSerializableInputStream.h"
#include "XPCOMModule.h"

using namespace mozilla::ipc;
using mozilla::fallible;
using mozilla::MakeRefPtr;
using mozilla::MallocSizeOf;
using mozilla::ReentrantMonitorAutoEnter;
using mozilla::Span;
using mozilla::StreamBufferSource;

//-----------------------------------------------------------------------------
// StreamBufferSource implementations
//-----------------------------------------------------------------------------

class nsTArraySource final : public StreamBufferSource {
 public:
  explicit nsTArraySource(nsTArray<uint8_t>&& aArray)
      : mArray(std::move(aArray)) {}

  Span<const char> Data() override {
    return Span{reinterpret_cast<const char*>(mArray.Elements()),
                mArray.Length()};
  }

  bool Owning() override { return true; }

  size_t SizeOfExcludingThisEvenIfShared(MallocSizeOf aMallocSizeOf) override {
    return mArray.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

  nsTArray<uint8_t> mArray;
};

class nsCStringSource final : public StreamBufferSource {
 public:
  nsCStringSource() = default;

  Span<const char> Data() override { return mString; }

  nsresult GetData(nsACString& aString) override {
    if (!aString.Assign(mString, mozilla::fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
  }

  bool Owning() override { return true; }

  size_t SizeOfExcludingThisIfUnshared(MallocSizeOf aMallocSizeOf) override {
    return mString.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  size_t SizeOfExcludingThisEvenIfShared(MallocSizeOf aMallocSizeOf) override {
    return mString.SizeOfExcludingThisEvenIfShared(aMallocSizeOf);
  }

  nsCString mString;
};

class nsBorrowedSource final : public StreamBufferSource {
 public:
  explicit nsBorrowedSource(Span<const char> aBuffer) : mBuffer(aBuffer) {}

  Span<const char> Data() override { return mBuffer; }

  bool Owning() override { return false; }

  size_t SizeOfExcludingThisEvenIfShared(MallocSizeOf aMallocSizeOf) override {
    return 0;
  }

  Span<const char> mBuffer;
};

//-----------------------------------------------------------------------------
// nsIStringInputStream implementation
//-----------------------------------------------------------------------------

class nsStringInputStream final : public nsIStringInputStream,
                                  public nsISeekableStream,
                                  public nsISupportsCString,
                                  public nsIIPCSerializableInputStream,
                                  public nsICloneableInputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSISTRINGINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSITELLABLESTREAM
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSCSTRING
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAM

  nsStringInputStream() = default;

  nsresult Init(nsCString&& aString);

  nsresult Init(nsTArray<uint8_t>&& aArray);

 private:
  ~nsStringInputStream() = default;

  size_t Length() const MOZ_REQUIRES(mMon) {
    return mSource ? mSource->Data().Length() : 0;
  }

  size_t LengthRemaining() const MOZ_REQUIRES(mMon) {
    return Length() - mOffset;
  }

  void Clear() MOZ_REQUIRES(mMon) { mSource = nullptr; }

  bool Closed() MOZ_REQUIRES(mMon) { return !mSource; }

  RefPtr<StreamBufferSource> mSource MOZ_GUARDED_BY(mMon);
  size_t mOffset MOZ_GUARDED_BY(mMon) = 0;

  mutable mozilla::ReentrantMonitor mMon{"nsStringInputStream"};
};

nsresult nsStringInputStream::Init(nsCString&& aString) {
  auto source = MakeRefPtr<nsCStringSource>();
  if (!source->mString.Assign(std::move(aString), fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return SetDataSource(source);
}

nsresult nsStringInputStream::Init(nsTArray<uint8_t>&& aArray) {
  auto source = MakeRefPtr<nsTArraySource>(std::move(aArray));
  return SetDataSource(source);
}

// This class needs to support threadsafe refcounting since people often
// allocate a string stream, and then read it from a background thread.
NS_IMPL_ADDREF(nsStringInputStream)
NS_IMPL_RELEASE(nsStringInputStream)

NS_IMPL_CLASSINFO(nsStringInputStream, nullptr, nsIClassInfo::THREADSAFE,
                  NS_STRINGINPUTSTREAM_CID)
NS_IMPL_QUERY_INTERFACE_CI(nsStringInputStream, nsIStringInputStream,
                           nsIInputStream, nsISupportsCString,
                           nsISeekableStream, nsITellableStream,
                           nsIIPCSerializableInputStream,
                           nsICloneableInputStream)
NS_IMPL_CI_INTERFACE_GETTER(nsStringInputStream, nsIStringInputStream,
                            nsIInputStream, nsISupportsCString,
                            nsISeekableStream, nsITellableStream,
                            nsICloneableInputStream)

/////////
// nsISupportsCString implementation
/////////

NS_IMETHODIMP
nsStringInputStream::GetType(uint16_t* aType) {
  *aType = TYPE_CSTRING;
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::GetData(nsACString& data) {
  ReentrantMonitorAutoEnter lock(mMon);

  // The stream doesn't have any data when it is closed.  We could fake it
  // and return an empty string here, but it seems better to keep this return
  // value consistent with the behavior of the other 'getter' methods.
  if (NS_WARN_IF(Closed())) {
    return NS_BASE_STREAM_CLOSED;
  }

  return mSource->GetData(data);
}

NS_IMETHODIMP
nsStringInputStream::SetData(const nsACString& aData) {
  auto source = MakeRefPtr<nsCStringSource>();
  if (!source->mString.Assign(aData, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return SetDataSource(source);
}

NS_IMETHODIMP
nsStringInputStream::ToString(char** aResult) {
  // NOTE: This method may result in data loss, so we do not implement it.
  return NS_ERROR_NOT_IMPLEMENTED;
}

/////////
// nsIStringInputStream implementation
/////////

NS_IMETHODIMP
nsStringInputStream::SetData(const char* aData, int32_t aDataLen) {
  if (NS_WARN_IF(!aData)) {
    return NS_ERROR_INVALID_ARG;
  }

  auto source = MakeRefPtr<nsCStringSource>();
  if (NS_WARN_IF(!source->mString.Assign(aData, aDataLen, fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return SetDataSource(source);
}

NS_IMETHODIMP
nsStringInputStream::SetUTF8Data(const nsACString& aData) {
  return nsStringInputStream::SetData(aData);
}

NS_IMETHODIMP
nsStringInputStream::AdoptData(char* aData, int32_t aDataLen) {
  if (NS_WARN_IF(!aData)) {
    return NS_ERROR_INVALID_ARG;
  }

  auto source = MakeRefPtr<nsCStringSource>();
  source->mString.Adopt(aData, aDataLen);
  return SetDataSource(source);
}

NS_IMETHODIMP
nsStringInputStream::ShareData(const char* aData, int32_t aDataLen) {
  if (NS_WARN_IF(!aData)) {
    return NS_ERROR_INVALID_ARG;
  }

  size_t length = aDataLen < 0 ? strlen(aData) : size_t(aDataLen);
  auto source = MakeRefPtr<nsBorrowedSource>(Span{aData, length});
  return SetDataSource(source);
}

NS_IMETHODIMP
nsStringInputStream::SetDataSource(StreamBufferSource* aSource) {
  ReentrantMonitorAutoEnter lock(mMon);

  if (NS_WARN_IF(!aSource)) {
    return NS_ERROR_INVALID_ARG;
  }

  mSource = aSource;
  mOffset = 0;
  return NS_OK;
}

NS_IMETHODIMP_(size_t)
nsStringInputStream::SizeOfIncludingThisIfUnshared(MallocSizeOf aMallocSizeOf) {
  ReentrantMonitorAutoEnter lock(mMon);

  size_t n = aMallocSizeOf(this);
  if (mSource) {
    n += mSource->SizeOfIncludingThisIfUnshared(aMallocSizeOf);
  }
  return n;
}

NS_IMETHODIMP_(size_t)
nsStringInputStream::SizeOfIncludingThisEvenIfShared(
    MallocSizeOf aMallocSizeOf) {
  ReentrantMonitorAutoEnter lock(mMon);

  size_t n = aMallocSizeOf(this);
  if (mSource) {
    n += mSource->SizeOfIncludingThisEvenIfShared(aMallocSizeOf);
  }
  return n;
}

/////////
// nsIInputStream implementation
/////////

NS_IMETHODIMP
nsStringInputStream::Close() {
  ReentrantMonitorAutoEnter lock(mMon);

  Clear();
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::Available(uint64_t* aLength) {
  ReentrantMonitorAutoEnter lock(mMon);

  NS_ASSERTION(aLength, "null ptr");

  if (Closed()) {
    return NS_BASE_STREAM_CLOSED;
  }

  *aLength = LengthRemaining();
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::Read(char* aBuf, uint32_t aCount, uint32_t* aReadCount) {
  NS_ASSERTION(aBuf, "null ptr");
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, aReadCount);
}

NS_IMETHODIMP
nsStringInputStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                  uint32_t aCount, uint32_t* aResult) {
  ReentrantMonitorAutoEnter lock(mMon);

  NS_ASSERTION(aResult, "null ptr");
  NS_ASSERTION(Length() >= mOffset, "bad stream state");

  if (Closed()) {
    return NS_BASE_STREAM_CLOSED;
  }

  // We may be at end-of-file
  size_t maxCount = LengthRemaining();
  if (maxCount == 0) {
    *aResult = 0;
    return NS_OK;
  }

  if (aCount > maxCount) {
    aCount = maxCount;
  }

  RefPtr<StreamBufferSource> source = mSource;
  size_t offset = mOffset;

  nsresult rv = aWriter(this, aClosure, source->Data().Elements() + offset, 0,
                        aCount, aResult);

  if (Closed()) {
    NS_WARNING("nsStringInputStream was closed during ReadSegments");
    return NS_OK;
  }

  MOZ_RELEASE_ASSERT(mSource == source, "String was replaced!");
  MOZ_RELEASE_ASSERT(mOffset == offset, "Nested read operation!");

  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(*aResult <= aCount,
                 "writer should not write more than we asked it to write");
    mOffset = offset + *aResult;
  }

  // errors returned from the writer end here!
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::IsNonBlocking(bool* aNonBlocking) {
  *aNonBlocking = true;
  return NS_OK;
}

/////////
// nsISeekableStream implementation
/////////

NS_IMETHODIMP
nsStringInputStream::Seek(int32_t aWhence, int64_t aOffset) {
  ReentrantMonitorAutoEnter lock(mMon);

  if (Closed()) {
    return NS_BASE_STREAM_CLOSED;
  }

  // Compute new stream position.  The given offset may be a negative value.

  int64_t newPos = aOffset;
  switch (aWhence) {
    case NS_SEEK_SET:
      break;
    case NS_SEEK_CUR:
      newPos += (int64_t)mOffset;
      break;
    case NS_SEEK_END:
      newPos += (int64_t)Length();
      break;
    default:
      NS_ERROR("invalid aWhence");
      return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(newPos < 0) || NS_WARN_IF(newPos > (int64_t)Length())) {
    return NS_ERROR_INVALID_ARG;
  }

  mOffset = (size_t)newPos;
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::SetEOF() {
  ReentrantMonitorAutoEnter lock(mMon);

  if (Closed()) {
    return NS_BASE_STREAM_CLOSED;
  }

  mOffset = Length();
  return NS_OK;
}

/////////
// nsITellableStream implementation
/////////

NS_IMETHODIMP
nsStringInputStream::Tell(int64_t* aOutWhere) {
  ReentrantMonitorAutoEnter lock(mMon);

  if (Closed()) {
    return NS_BASE_STREAM_CLOSED;
  }

  *aOutWhere = (int64_t)mOffset;
  return NS_OK;
}

/////////
// nsIIPCSerializableInputStream implementation
/////////

void nsStringInputStream::SerializedComplexity(uint32_t aMaxSize,
                                               uint32_t* aSizeUsed,
                                               uint32_t* aPipes,
                                               uint32_t* aTransferables) {
  ReentrantMonitorAutoEnter lock(mMon);

  if (Length() >= aMaxSize) {
    *aPipes = 1;
  } else {
    *aSizeUsed = Length();
  }
}

void nsStringInputStream::Serialize(InputStreamParams& aParams,
                                    uint32_t aMaxSize, uint32_t* aSizeUsed) {
  ReentrantMonitorAutoEnter lock(mMon);

  MOZ_DIAGNOSTIC_ASSERT(!Closed(), "cannot send a closed stream!");
  MOZ_ASSERT(aSizeUsed);
  *aSizeUsed = 0;

  if (Length() >= aMaxSize) {
    // If the input stream is non-owning (i.e. it was initialized with
    // `ShareData`), create a new owning source so that it doesn't go away while
    // async copying.
    if (!mSource->Owning()) {
      auto source = MakeRefPtr<nsCStringSource>();
      source->mString.Assign(nsDependentCSubstring{mSource->Data()});
      mSource = source;
    }

    InputStreamHelper::SerializeInputStreamAsPipe(this, aParams);
    return;
  }

  *aSizeUsed = Length();

  StringInputStreamParams params;
  mSource->GetData(params.data());
  aParams = params;
}

bool nsStringInputStream::Deserialize(const InputStreamParams& aParams) {
  if (aParams.type() != InputStreamParams::TStringInputStreamParams) {
    NS_ERROR("Received unknown parameters from the other process!");
    return false;
  }

  const StringInputStreamParams& params = aParams.get_StringInputStreamParams();

  if (NS_FAILED(SetData(params.data()))) {
    NS_WARNING("SetData failed!");
    return false;
  }

  return true;
}

/////////
// nsICloneableInputStream implementation
/////////

NS_IMETHODIMP
nsStringInputStream::GetCloneable(bool* aCloneableOut) {
  *aCloneableOut = true;
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::Clone(nsIInputStream** aCloneOut) {
  ReentrantMonitorAutoEnter lock(mMon);

  RefPtr<nsStringInputStream> ref = new nsStringInputStream();
  // Nothing else can access this yet, but suppress static analysis warnings
  ReentrantMonitorAutoEnter reflock(ref->mMon);
  if (mSource && !mSource->Owning()) {
    auto data = mSource->Data();
    nsresult rv = ref->SetData(data.Elements(), data.Length());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    ref->mSource = mSource;
  }

  // mOffset is overwritten by SetData().
  ref->mOffset = mOffset;

  ref.forget(aCloneOut);
  return NS_OK;
}

nsresult NS_NewByteInputStream(nsIInputStream** aStreamResult,
                               mozilla::Span<const char> aStringToRead,
                               nsAssignmentType aAssignment) {
  MOZ_ASSERT(aStreamResult, "null out ptr");

  RefPtr<nsStringInputStream> stream = new nsStringInputStream();

  nsresult rv;
  switch (aAssignment) {
    case NS_ASSIGNMENT_COPY:
      rv = stream->SetData(aStringToRead.Elements(), aStringToRead.Length());
      break;
    case NS_ASSIGNMENT_DEPEND:
      rv = stream->ShareData(aStringToRead.Elements(), aStringToRead.Length());
      break;
    case NS_ASSIGNMENT_ADOPT:
      rv = stream->AdoptData(const_cast<char*>(aStringToRead.Elements()),
                             aStringToRead.Length());
      break;
    default:
      NS_ERROR("invalid assignment type");
      rv = NS_ERROR_INVALID_ARG;
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  stream.forget(aStreamResult);
  return NS_OK;
}

nsresult NS_NewByteInputStream(nsIInputStream** aStreamResult,
                               nsTArray<uint8_t>&& aArray) {
  MOZ_ASSERT(aStreamResult, "null out ptr");

  RefPtr<nsStringInputStream> stream = new nsStringInputStream();

  nsresult rv = stream->Init(std::move(aArray));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  stream.forget(aStreamResult);
  return NS_OK;
}

extern nsresult NS_NewByteInputStream(nsIInputStream** aStreamResult,
                                      mozilla::StreamBufferSource* aSource) {
  MOZ_ASSERT(aStreamResult, "null out ptr");

  RefPtr<nsStringInputStream> stream = new nsStringInputStream();

  nsresult rv = stream->SetDataSource(aSource);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  stream.forget(aStreamResult);
  return NS_OK;
}

nsresult NS_NewCStringInputStream(nsIInputStream** aStreamResult,
                                  const nsACString& aStringToRead) {
  MOZ_ASSERT(aStreamResult, "null out ptr");

  RefPtr<nsStringInputStream> stream = new nsStringInputStream();

  nsresult rv = stream->SetData(aStringToRead);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  stream.forget(aStreamResult);
  return NS_OK;
}

nsresult NS_NewCStringInputStream(nsIInputStream** aStreamResult,
                                  nsCString&& aStringToRead) {
  MOZ_ASSERT(aStreamResult, "null out ptr");

  RefPtr<nsStringInputStream> stream = new nsStringInputStream();

  nsresult rv = stream->Init(std::move(aStringToRead));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  stream.forget(aStreamResult);
  return NS_OK;
}

// factory method for constructing a nsStringInputStream object
nsresult nsStringInputStreamConstructor(REFNSIID aIID, void** aResult) {
  *aResult = nullptr;

  RefPtr<nsStringInputStream> inst = new nsStringInputStream();
  return inst->QueryInterface(aIID, aResult);
}
