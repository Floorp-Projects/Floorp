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
#include "nsIIPCSerializableInputStream.h"
#include "XPCOMModule.h"

using namespace mozilla::ipc;
using mozilla::fallible;
using mozilla::MallocSizeOf;
using mozilla::Maybe;
using mozilla::ReentrantMonitorAutoEnter;
using mozilla::Some;

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

  nsStringInputStream() : mOffset(0), mMon("nsStringInputStream") { Clear(); }

  nsresult Init(nsCString&& aString);

  nsresult Init(nsTArray<uint8_t>&& aArray);

 private:
  ~nsStringInputStream() = default;

  template <typename M>
  void SerializeInternal(InputStreamParams& aParams, bool aDelayedStart,
                         uint32_t aMaxSize, uint32_t* aSizeUsed, M* aManager);

  uint32_t Length() const { return mData.Length(); }

  uint32_t LengthRemaining() const { return Length() - mOffset; }

  void Clear() { mData.SetIsVoid(true); }

  bool Closed() { return mData.IsVoid(); }

  nsDependentCSubstring mData;
  uint32_t mOffset;

  // If we were initialized from an nsTArray, we store its data here.
  Maybe<nsTArray<uint8_t>> mArray;

  mozilla::ReentrantMonitor mMon;
};

nsresult nsStringInputStream::Init(nsCString&& aString) {
  ReentrantMonitorAutoEnter lock(mMon);

  mArray.reset();
  if (!mData.Assign(std::move(aString), fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mOffset = 0;
  return NS_OK;
}

nsresult nsStringInputStream::Init(nsTArray<uint8_t>&& aArray) {
  ReentrantMonitorAutoEnter lock(mMon);

  mArray.reset();
  mArray.emplace(std::move(aArray));
  mOffset = 0;

  if (mArray->IsEmpty()) {
    // Not sure it's safe to Rebind() with a null pointer.  Pretty
    // sure it's not, in fact.
    mData.Truncate();
  } else {
    mData.Rebind(reinterpret_cast<const char*>(mArray->Elements()),
                 mArray->Length());
  }
  return NS_OK;
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

  data.Assign(mData);
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::SetData(const nsACString& aData) {
  ReentrantMonitorAutoEnter lock(mMon);

  mArray.reset();
  if (NS_WARN_IF(!mData.Assign(aData, fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mOffset = 0;
  return NS_OK;
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
  ReentrantMonitorAutoEnter lock(mMon);

  if (NS_WARN_IF(!aData)) {
    return NS_ERROR_INVALID_ARG;
  }

  mArray.reset();
  if (NS_WARN_IF(!mData.Assign(aData, aDataLen, fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mOffset = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::SetUTF8Data(const nsACString& aData) {
  return nsStringInputStream::SetData(aData);
}

NS_IMETHODIMP
nsStringInputStream::AdoptData(char* aData, int32_t aDataLen) {
  ReentrantMonitorAutoEnter lock(mMon);

  if (NS_WARN_IF(!aData)) {
    return NS_ERROR_INVALID_ARG;
  }
  mArray.reset();
  mData.Adopt(aData, aDataLen);
  mOffset = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::ShareData(const char* aData, int32_t aDataLen) {
  ReentrantMonitorAutoEnter lock(mMon);

  if (NS_WARN_IF(!aData)) {
    return NS_ERROR_INVALID_ARG;
  }

  mArray.reset();
  if (aDataLen < 0) {
    aDataLen = strlen(aData);
  }

  mData.Rebind(aData, aDataLen);
  mOffset = 0;
  return NS_OK;
}

NS_IMETHODIMP_(size_t)
nsStringInputStream::SizeOfIncludingThisIfUnshared(MallocSizeOf aMallocSizeOf) {
  ReentrantMonitorAutoEnter lock(mMon);

  size_t n = aMallocSizeOf(this);
  n += mData.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  return n;
}

NS_IMETHODIMP_(size_t)
nsStringInputStream::SizeOfIncludingThisEvenIfShared(
    MallocSizeOf aMallocSizeOf) {
  ReentrantMonitorAutoEnter lock(mMon);

  size_t n = aMallocSizeOf(this);
  n += mData.SizeOfExcludingThisEvenIfShared(aMallocSizeOf);
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
  uint32_t maxCount = LengthRemaining();
  if (maxCount == 0) {
    *aResult = 0;
    return NS_OK;
  }

  if (aCount > maxCount) {
    aCount = maxCount;
  }

  nsDependentCSubstring tempData;
  tempData.SetIsVoid(true);
  if (mData.GetDataFlags() & nsACString::DataFlags::OWNED) {
    tempData.Assign(std::move(mData));
    mData.Rebind(tempData.BeginReading(), tempData.EndReading());
  }

  nsresult rv = aWriter(this, aClosure, mData.BeginReading() + mOffset, 0,
                        aCount, aResult);

  if (!mData.IsVoid() && !tempData.IsVoid()) {
    MOZ_DIAGNOSTIC_ASSERT(mData == tempData, "String was replaced!");
    mData.SetIsVoid(true);
    mData.Assign(std::move(tempData));
  }

  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(*aResult <= aCount,
                 "writer should not write more than we asked it to write");
    mOffset += *aResult;
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
      newPos += mOffset;
      break;
    case NS_SEEK_END:
      newPos += Length();
      break;
    default:
      NS_ERROR("invalid aWhence");
      return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(newPos < 0) || NS_WARN_IF(newPos > Length())) {
    return NS_ERROR_INVALID_ARG;
  }

  mOffset = (uint32_t)newPos;
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

  *aOutWhere = mOffset;
  return NS_OK;
}

/////////
// nsIIPCSerializableInputStream implementation
/////////

void nsStringInputStream::Serialize(
    InputStreamParams& aParams, FileDescriptorArray& /* aFDs */,
    bool aDelayedStart, uint32_t aMaxSize, uint32_t* aSizeUsed,
    mozilla::ipc::ParentToChildStreamActorManager* aManager) {
  SerializeInternal(aParams, aDelayedStart, aMaxSize, aSizeUsed, aManager);
}

void nsStringInputStream::Serialize(
    InputStreamParams& aParams, FileDescriptorArray& /* aFDs */,
    bool aDelayedStart, uint32_t aMaxSize, uint32_t* aSizeUsed,
    mozilla::ipc::ChildToParentStreamActorManager* aManager) {
  SerializeInternal(aParams, aDelayedStart, aMaxSize, aSizeUsed, aManager);
}

template <typename M>
void nsStringInputStream::SerializeInternal(InputStreamParams& aParams,
                                            bool aDelayedStart,
                                            uint32_t aMaxSize,
                                            uint32_t* aSizeUsed, M* aManager) {
  ReentrantMonitorAutoEnter lock(mMon);

  MOZ_ASSERT(aSizeUsed);
  *aSizeUsed = 0;

  if (Length() >= aMaxSize) {
    InputStreamHelper::SerializeInputStreamAsPipe(this, aParams, aDelayedStart,
                                                  aManager);
    return;
  }

  *aSizeUsed = Length();

  StringInputStreamParams params;
  params.data() = PromiseFlatCString(mData);
  aParams = params;
}

bool nsStringInputStream::Deserialize(const InputStreamParams& aParams,
                                      const FileDescriptorArray& /* aFDs */) {
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
  nsresult rv = ref->SetData(mData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
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
nsresult nsStringInputStreamConstructor(nsISupports* aOuter, REFNSIID aIID,
                                        void** aResult) {
  *aResult = nullptr;

  if (NS_WARN_IF(aOuter)) {
    return NS_ERROR_NO_AGGREGATION;
  }

  RefPtr<nsStringInputStream> inst = new nsStringInputStream();
  return inst->QueryInterface(aIID, aResult);
}
