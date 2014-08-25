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
#include "nsISeekableStream.h"
#include "nsISupportsPrimitives.h"
#include "nsCRT.h"
#include "prerror.h"
#include "plstr.h"
#include "nsIClassInfoImpl.h"
#include "mozilla/Attributes.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsIIPCSerializableInputStream.h"

using namespace mozilla::ipc;

//-----------------------------------------------------------------------------
// nsIStringInputStream implementation
//-----------------------------------------------------------------------------

class nsStringInputStream MOZ_FINAL
  : public nsIStringInputStream
  , public nsISeekableStream
  , public nsISupportsCString
  , public nsIIPCSerializableInputStream
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSISTRINGINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSCSTRING
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM

  nsStringInputStream()
  {
    Clear();
  }

private:
  ~nsStringInputStream()
  {
  }

  uint32_t Length() const
  {
    return mData.Length();
  }

  uint32_t LengthRemaining() const
  {
    return Length() - mOffset;
  }

  void Clear()
  {
    mData.SetIsVoid(true);
  }

  bool Closed()
  {
    return mData.IsVoid();
  }

  nsDependentCSubstring mData;
  uint32_t mOffset;
};

// This class needs to support threadsafe refcounting since people often
// allocate a string stream, and then read it from a background thread.
NS_IMPL_ADDREF(nsStringInputStream)
NS_IMPL_RELEASE(nsStringInputStream)

NS_IMPL_CLASSINFO(nsStringInputStream, nullptr, nsIClassInfo::THREADSAFE,
                  NS_STRINGINPUTSTREAM_CID)
NS_IMPL_QUERY_INTERFACE_CI(nsStringInputStream,
                           nsIStringInputStream,
                           nsIInputStream,
                           nsISupportsCString,
                           nsISeekableStream,
                           nsIIPCSerializableInputStream)
NS_IMPL_CI_INTERFACE_GETTER(nsStringInputStream,
                            nsIStringInputStream,
                            nsIInputStream,
                            nsISupportsCString,
                            nsISeekableStream)

/////////
// nsISupportsCString implementation
/////////

NS_IMETHODIMP
nsStringInputStream::GetType(uint16_t* aType)
{
  *aType = TYPE_CSTRING;
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::GetData(nsACString& data)
{
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
nsStringInputStream::SetData(const nsACString& aData)
{
  mData.Assign(aData);
  mOffset = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::ToString(char** aResult)
{
  // NOTE: This method may result in data loss, so we do not implement it.
  return NS_ERROR_NOT_IMPLEMENTED;
}

/////////
// nsIStringInputStream implementation
/////////

NS_IMETHODIMP
nsStringInputStream::SetData(const char* aData, int32_t aDataLen)
{
  if (NS_WARN_IF(!aData)) {
    return NS_ERROR_INVALID_ARG;
  }
  mData.Assign(aData, aDataLen);
  mOffset = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::AdoptData(char* aData, int32_t aDataLen)
{
  if (NS_WARN_IF(!aData)) {
    return NS_ERROR_INVALID_ARG;
  }
  mData.Adopt(aData, aDataLen);
  mOffset = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::ShareData(const char* aData, int32_t aDataLen)
{
  if (NS_WARN_IF(!aData)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aDataLen < 0) {
    aDataLen = strlen(aData);
  }

  mData.Rebind(aData, aDataLen);
  mOffset = 0;
  return NS_OK;
}

/////////
// nsIInputStream implementation
/////////

NS_IMETHODIMP
nsStringInputStream::Close()
{
  Clear();
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::Available(uint64_t* aLength)
{
  NS_ASSERTION(aLength, "null ptr");

  if (Closed()) {
    return NS_BASE_STREAM_CLOSED;
  }

  *aLength = LengthRemaining();
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::Read(char* aBuf, uint32_t aCount, uint32_t* aReadCount)
{
  NS_ASSERTION(aBuf, "null ptr");
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, aReadCount);
}

NS_IMETHODIMP
nsStringInputStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                  uint32_t aCount, uint32_t* aResult)
{
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
  nsresult rv = aWriter(this, aClosure, mData.BeginReading() + mOffset, 0,
                        aCount, aResult);
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(*aResult <= aCount,
                 "writer should not write more than we asked it to write");
    mOffset += *aResult;
  }

  // errors returned from the writer end here!
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::IsNonBlocking(bool* aNonBlocking)
{
  *aNonBlocking = true;
  return NS_OK;
}

/////////
// nsISeekableStream implementation
/////////

NS_IMETHODIMP
nsStringInputStream::Seek(int32_t aWhence, int64_t aOffset)
{
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
nsStringInputStream::Tell(int64_t* aOutWhere)
{
  if (Closed()) {
    return NS_BASE_STREAM_CLOSED;
  }

  *aOutWhere = mOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsStringInputStream::SetEOF()
{
  if (Closed()) {
    return NS_BASE_STREAM_CLOSED;
  }

  mOffset = Length();
  return NS_OK;
}

void
nsStringInputStream::Serialize(InputStreamParams& aParams,
                               FileDescriptorArray& /* aFDs */)
{
  StringInputStreamParams params;
  params.data() = PromiseFlatCString(mData);
  aParams = params;
}

bool
nsStringInputStream::Deserialize(const InputStreamParams& aParams,
                                 const FileDescriptorArray& /* aFDs */)
{
  if (aParams.type() != InputStreamParams::TStringInputStreamParams) {
    NS_ERROR("Received unknown parameters from the other process!");
    return false;
  }

  const StringInputStreamParams& params =
    aParams.get_StringInputStreamParams();

  if (NS_FAILED(SetData(params.data()))) {
    NS_WARNING("SetData failed!");
    return false;
  }

  return true;
}

nsresult
NS_NewByteInputStream(nsIInputStream** aStreamResult,
                      const char* aStringToRead, int32_t aLength,
                      nsAssignmentType aAssignment)
{
  NS_PRECONDITION(aStreamResult, "null out ptr");

  nsStringInputStream* stream = new nsStringInputStream();
  if (!stream) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(stream);

  nsresult rv;
  switch (aAssignment) {
    case NS_ASSIGNMENT_COPY:
      rv = stream->SetData(aStringToRead, aLength);
      break;
    case NS_ASSIGNMENT_DEPEND:
      rv = stream->ShareData(aStringToRead, aLength);
      break;
    case NS_ASSIGNMENT_ADOPT:
      rv = stream->AdoptData(const_cast<char*>(aStringToRead), aLength);
      break;
    default:
      NS_ERROR("invalid assignment type");
      rv = NS_ERROR_INVALID_ARG;
  }

  if (NS_FAILED(rv)) {
    NS_RELEASE(stream);
    return rv;
  }

  *aStreamResult = stream;
  return NS_OK;
}

nsresult
NS_NewStringInputStream(nsIInputStream** aStreamResult,
                        const nsAString& aStringToRead)
{
  NS_LossyConvertUTF16toASCII data(aStringToRead); // truncates high-order bytes
  return NS_NewCStringInputStream(aStreamResult, data);
}

nsresult
NS_NewCStringInputStream(nsIInputStream** aStreamResult,
                         const nsACString& aStringToRead)
{
  NS_PRECONDITION(aStreamResult, "null out ptr");

  nsStringInputStream* stream = new nsStringInputStream();
  if (!stream) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(stream);

  stream->SetData(aStringToRead);

  *aStreamResult = stream;
  return NS_OK;
}

// factory method for constructing a nsStringInputStream object
nsresult
nsStringInputStreamConstructor(nsISupports* aOuter, REFNSIID aIID,
                               void** aResult)
{
  *aResult = nullptr;

  if (NS_WARN_IF(aOuter)) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsStringInputStream* inst = new nsStringInputStream();
  if (!inst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(inst);
  nsresult rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}
