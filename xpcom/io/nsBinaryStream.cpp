/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains implementations of the nsIBinaryInputStream and
 * nsIBinaryOutputStream interfaces.  Together, these interfaces allows reading
 * and writing of primitive data types (integers, floating-point values,
 * booleans, etc.) to a stream in a binary, untagged, fixed-endianness format.
 * This might be used, for example, to implement network protocols or to
 * produce architecture-neutral binary disk files, i.e. ones that can be read
 * and written by both big-endian and little-endian platforms.  Output is
 * written in big-endian order (high-order byte first), as this is traditional
 * network order.
 *
 * @See nsIBinaryInputStream
 * @See nsIBinaryOutputStream
 */
#include <algorithm>
#include <string.h>

#include "nsBinaryStream.h"

#include "mozilla/Endian.h"
#include "mozilla/PodOperations.h"
#include "mozilla/UniquePtr.h"

#include "nsCRT.h"
#include "nsString.h"
#include "nsISerializable.h"
#include "nsIClassInfo.h"
#include "nsComponentManagerUtils.h"
#include "nsIURI.h" // for NS_IURI_IID

#include "jsfriendapi.h"

using mozilla::MakeUnique;
using mozilla::PodCopy;
using mozilla::UniquePtr;

NS_IMPL_ISUPPORTS(nsBinaryOutputStream,
                  nsIObjectOutputStream,
                  nsIBinaryOutputStream,
                  nsIOutputStream)

NS_IMETHODIMP
nsBinaryOutputStream::Flush()
{
  if (NS_WARN_IF(!mOutputStream)) {
    return NS_ERROR_UNEXPECTED;
  }
  return mOutputStream->Flush();
}

NS_IMETHODIMP
nsBinaryOutputStream::Close()
{
  if (NS_WARN_IF(!mOutputStream)) {
    return NS_ERROR_UNEXPECTED;
  }
  return mOutputStream->Close();
}

NS_IMETHODIMP
nsBinaryOutputStream::Write(const char* aBuf, uint32_t aCount,
                            uint32_t* aActualBytes)
{
  if (NS_WARN_IF(!mOutputStream)) {
    return NS_ERROR_UNEXPECTED;
  }
  return mOutputStream->Write(aBuf, aCount, aActualBytes);
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteFrom(nsIInputStream* aInStr, uint32_t aCount,
                                uint32_t* aResult)
{
  NS_NOTREACHED("WriteFrom");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                                    uint32_t aCount, uint32_t* aResult)
{
  NS_NOTREACHED("WriteSegments");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBinaryOutputStream::IsNonBlocking(bool* aNonBlocking)
{
  if (NS_WARN_IF(!mOutputStream)) {
    return NS_ERROR_UNEXPECTED;
  }
  return mOutputStream->IsNonBlocking(aNonBlocking);
}

nsresult
nsBinaryOutputStream::WriteFully(const char* aBuf, uint32_t aCount)
{
  if (NS_WARN_IF(!mOutputStream)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv;
  uint32_t bytesWritten;

  rv = mOutputStream->Write(aBuf, aCount, &bytesWritten);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (bytesWritten != aCount) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBinaryOutputStream::SetOutputStream(nsIOutputStream* aOutputStream)
{
  if (NS_WARN_IF(!aOutputStream)) {
    return NS_ERROR_INVALID_ARG;
  }
  mOutputStream = aOutputStream;
  mBufferAccess = do_QueryInterface(aOutputStream);
  return NS_OK;
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteBoolean(bool aBoolean)
{
  return Write8(aBoolean);
}

NS_IMETHODIMP
nsBinaryOutputStream::Write8(uint8_t aByte)
{
  return WriteFully((const char*)&aByte, sizeof(aByte));
}

NS_IMETHODIMP
nsBinaryOutputStream::Write16(uint16_t aNum)
{
  aNum = mozilla::NativeEndian::swapToBigEndian(aNum);
  return WriteFully((const char*)&aNum, sizeof(aNum));
}

NS_IMETHODIMP
nsBinaryOutputStream::Write32(uint32_t aNum)
{
  aNum = mozilla::NativeEndian::swapToBigEndian(aNum);
  return WriteFully((const char*)&aNum, sizeof(aNum));
}

NS_IMETHODIMP
nsBinaryOutputStream::Write64(uint64_t aNum)
{
  nsresult rv;
  uint32_t bytesWritten;

  aNum = mozilla::NativeEndian::swapToBigEndian(aNum);
  rv = Write(reinterpret_cast<char*>(&aNum), sizeof(aNum), &bytesWritten);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (bytesWritten != sizeof(aNum)) {
    return NS_ERROR_FAILURE;
  }
  return rv;
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteFloat(float aFloat)
{
  NS_ASSERTION(sizeof(float) == sizeof(uint32_t),
               "False assumption about sizeof(float)");
  return Write32(*reinterpret_cast<uint32_t*>(&aFloat));
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteDouble(double aDouble)
{
  NS_ASSERTION(sizeof(double) == sizeof(uint64_t),
               "False assumption about sizeof(double)");
  return Write64(*reinterpret_cast<uint64_t*>(&aDouble));
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteStringZ(const char* aString)
{
  uint32_t length;
  nsresult rv;

  length = strlen(aString);
  rv = Write32(length);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return WriteFully(aString, length);
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteWStringZ(const char16_t* aString)
{
  uint32_t length, byteCount;
  nsresult rv;

  length = NS_strlen(aString);
  rv = Write32(length);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (length == 0) {
    return NS_OK;
  }
  byteCount = length * sizeof(char16_t);

#ifdef IS_BIG_ENDIAN
  rv = WriteBytes(reinterpret_cast<const char*>(aString), byteCount);
#else
  // XXX use WriteSegments here to avoid copy!
  char16_t* copy;
  char16_t temp[64];
  if (length <= 64) {
    copy = temp;
  } else {
    copy = reinterpret_cast<char16_t*>(moz_malloc(byteCount));
    if (!copy) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  NS_ASSERTION((uintptr_t(aString) & 0x1) == 0, "aString not properly aligned");
  mozilla::NativeEndian::copyAndSwapToBigEndian(copy, aString, length);
  rv = WriteBytes(reinterpret_cast<const char*>(copy), byteCount);
  if (copy != temp) {
    moz_free(copy);
  }
#endif

  return rv;
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteUtf8Z(const char16_t* aString)
{
  return WriteStringZ(NS_ConvertUTF16toUTF8(aString).get());
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteBytes(const char* aString, uint32_t aLength)
{
  nsresult rv;
  uint32_t bytesWritten;

  rv = Write(aString, aLength, &bytesWritten);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (bytesWritten != aLength) {
    return NS_ERROR_FAILURE;
  }
  return rv;
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteByteArray(uint8_t* aBytes, uint32_t aLength)
{
  return WriteBytes(reinterpret_cast<char*>(aBytes), aLength);
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteObject(nsISupports* aObject, bool aIsStrongRef)
{
  return WriteCompoundObject(aObject, NS_GET_IID(nsISupports),
                             aIsStrongRef);
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteSingleRefObject(nsISupports* aObject)
{
  return WriteCompoundObject(aObject, NS_GET_IID(nsISupports),
                             true);
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteCompoundObject(nsISupports* aObject,
                                          const nsIID& aIID,
                                          bool aIsStrongRef)
{
  nsCOMPtr<nsIClassInfo> classInfo = do_QueryInterface(aObject);
  nsCOMPtr<nsISerializable> serializable = do_QueryInterface(aObject);

  // Can't deal with weak refs
  if (NS_WARN_IF(!aIsStrongRef)) {
    return NS_ERROR_UNEXPECTED;
  }
  if (NS_WARN_IF(!classInfo) || NS_WARN_IF(!serializable)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCID cid;
  nsresult rv = classInfo->GetClassIDNoAlloc(&cid);
  if (NS_SUCCEEDED(rv)) {
    rv = WriteID(cid);
  } else {
    nsCID* cidptr = nullptr;
    rv = classInfo->GetClassID(&cidptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = WriteID(*cidptr);

    NS_Free(cidptr);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = WriteID(aIID);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return serializable->Write(this);
}

NS_IMETHODIMP
nsBinaryOutputStream::WriteID(const nsIID& aIID)
{
  nsresult rv = Write32(aIID.m0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = Write16(aIID.m1);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = Write16(aIID.m2);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (int i = 0; i < 8; ++i) {
    rv = Write8(aIID.m3[i]);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP_(char*)
nsBinaryOutputStream::GetBuffer(uint32_t aLength, uint32_t aAlignMask)
{
  if (mBufferAccess) {
    return mBufferAccess->GetBuffer(aLength, aAlignMask);
  }
  return nullptr;
}

NS_IMETHODIMP_(void)
nsBinaryOutputStream::PutBuffer(char* aBuffer, uint32_t aLength)
{
  if (mBufferAccess) {
    mBufferAccess->PutBuffer(aBuffer, aLength);
  }
}

NS_IMPL_ISUPPORTS(nsBinaryInputStream,
                  nsIObjectInputStream,
                  nsIBinaryInputStream,
                  nsIInputStream)

NS_IMETHODIMP
nsBinaryInputStream::Available(uint64_t* aResult)
{
  if (NS_WARN_IF(!mInputStream)) {
    return NS_ERROR_UNEXPECTED;
  }
  return mInputStream->Available(aResult);
}

NS_IMETHODIMP
nsBinaryInputStream::Read(char* aBuffer, uint32_t aCount, uint32_t* aNumRead)
{
  if (NS_WARN_IF(!mInputStream)) {
    return NS_ERROR_UNEXPECTED;
  }

  // mInputStream might give us short reads, so deal with that.
  uint32_t totalRead = 0;

  uint32_t bytesRead;
  do {
    nsresult rv = mInputStream->Read(aBuffer, aCount, &bytesRead);
    if (rv == NS_BASE_STREAM_WOULD_BLOCK && totalRead != 0) {
      // We already read some data.  Return it.
      break;
    }

    if (NS_FAILED(rv)) {
      return rv;
    }

    totalRead += bytesRead;
    aBuffer += bytesRead;
    aCount -= bytesRead;
  } while (aCount != 0 && bytesRead != 0);

  *aNumRead = totalRead;

  return NS_OK;
}


// when forwarding ReadSegments to mInputStream, we need to make sure
// 'this' is being passed to the writer each time. To do this, we need
// a thunking function which keeps the real input stream around.

// the closure wrapper
struct MOZ_STACK_CLASS ReadSegmentsClosure
{
  nsCOMPtr<nsIInputStream> mRealInputStream;
  void* mRealClosure;
  nsWriteSegmentFun mRealWriter;
  nsresult mRealResult;
  uint32_t mBytesRead;  // to properly implement aToOffset
};

// the thunking function
static NS_METHOD
ReadSegmentForwardingThunk(nsIInputStream* aStream,
                           void* aClosure,
                           const char* aFromSegment,
                           uint32_t aToOffset,
                           uint32_t aCount,
                           uint32_t* aWriteCount)
{
  ReadSegmentsClosure* thunkClosure =
    reinterpret_cast<ReadSegmentsClosure*>(aClosure);

  NS_ASSERTION(NS_SUCCEEDED(thunkClosure->mRealResult),
               "How did this get to be a failure status?");

  thunkClosure->mRealResult =
    thunkClosure->mRealWriter(thunkClosure->mRealInputStream,
                              thunkClosure->mRealClosure,
                              aFromSegment,
                              thunkClosure->mBytesRead + aToOffset,
                              aCount, aWriteCount);

  return thunkClosure->mRealResult;
}


NS_IMETHODIMP
nsBinaryInputStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                  uint32_t aCount, uint32_t* aResult)
{
  if (NS_WARN_IF(!mInputStream)) {
    return NS_ERROR_UNEXPECTED;
  }

  ReadSegmentsClosure thunkClosure = { this, aClosure, aWriter, NS_OK, 0 };

  // mInputStream might give us short reads, so deal with that.
  uint32_t bytesRead;
  do {
    nsresult rv = mInputStream->ReadSegments(ReadSegmentForwardingThunk,
                                             &thunkClosure,
                                             aCount, &bytesRead);

    if (rv == NS_BASE_STREAM_WOULD_BLOCK && thunkClosure.mBytesRead != 0) {
      // We already read some data.  Return it.
      break;
    }

    if (NS_FAILED(rv)) {
      return rv;
    }

    thunkClosure.mBytesRead += bytesRead;
    aCount -= bytesRead;
  } while (aCount != 0 && bytesRead != 0 &&
           NS_SUCCEEDED(thunkClosure.mRealResult));

  *aResult = thunkClosure.mBytesRead;

  return NS_OK;
}

NS_IMETHODIMP
nsBinaryInputStream::IsNonBlocking(bool* aNonBlocking)
{
  if (NS_WARN_IF(!mInputStream)) {
    return NS_ERROR_UNEXPECTED;
  }
  return mInputStream->IsNonBlocking(aNonBlocking);
}

NS_IMETHODIMP
nsBinaryInputStream::Close()
{
  if (NS_WARN_IF(!mInputStream)) {
    return NS_ERROR_UNEXPECTED;
  }
  return mInputStream->Close();
}

NS_IMETHODIMP
nsBinaryInputStream::SetInputStream(nsIInputStream* aInputStream)
{
  if (NS_WARN_IF(!aInputStream)) {
    return NS_ERROR_INVALID_ARG;
  }
  mInputStream = aInputStream;
  mBufferAccess = do_QueryInterface(aInputStream);
  return NS_OK;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadBoolean(bool* aBoolean)
{
  uint8_t byteResult;
  nsresult rv = Read8(&byteResult);
  if (NS_FAILED(rv)) {
    return rv;
  }
  *aBoolean = !!byteResult;
  return rv;
}

NS_IMETHODIMP
nsBinaryInputStream::Read8(uint8_t* aByte)
{
  nsresult rv;
  uint32_t bytesRead;

  rv = Read(reinterpret_cast<char*>(aByte), sizeof(*aByte), &bytesRead);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (bytesRead != 1) {
    return NS_ERROR_FAILURE;
  }
  return rv;
}

NS_IMETHODIMP
nsBinaryInputStream::Read16(uint16_t* aNum)
{
  uint32_t bytesRead;
  nsresult rv = Read(reinterpret_cast<char*>(aNum), sizeof(*aNum), &bytesRead);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (bytesRead != sizeof(*aNum)) {
    return NS_ERROR_FAILURE;
  }
  *aNum = mozilla::NativeEndian::swapFromBigEndian(*aNum);
  return rv;
}

NS_IMETHODIMP
nsBinaryInputStream::Read32(uint32_t* aNum)
{
  uint32_t bytesRead;
  nsresult rv = Read(reinterpret_cast<char*>(aNum), sizeof(*aNum), &bytesRead);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (bytesRead != sizeof(*aNum)) {
    return NS_ERROR_FAILURE;
  }
  *aNum = mozilla::NativeEndian::swapFromBigEndian(*aNum);
  return rv;
}

NS_IMETHODIMP
nsBinaryInputStream::Read64(uint64_t* aNum)
{
  uint32_t bytesRead;
  nsresult rv = Read(reinterpret_cast<char*>(aNum), sizeof(*aNum), &bytesRead);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (bytesRead != sizeof(*aNum)) {
    return NS_ERROR_FAILURE;
  }
  *aNum = mozilla::NativeEndian::swapFromBigEndian(*aNum);
  return rv;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadFloat(float* aFloat)
{
  NS_ASSERTION(sizeof(float) == sizeof(uint32_t),
               "False assumption about sizeof(float)");
  return Read32(reinterpret_cast<uint32_t*>(aFloat));
}

NS_IMETHODIMP
nsBinaryInputStream::ReadDouble(double* aDouble)
{
  NS_ASSERTION(sizeof(double) == sizeof(uint64_t),
               "False assumption about sizeof(double)");
  return Read64(reinterpret_cast<uint64_t*>(aDouble));
}

static NS_METHOD
WriteSegmentToCString(nsIInputStream* aStream,
                      void* aClosure,
                      const char* aFromSegment,
                      uint32_t aToOffset,
                      uint32_t aCount,
                      uint32_t* aWriteCount)
{
  nsACString* outString = static_cast<nsACString*>(aClosure);

  outString->Append(aFromSegment, aCount);

  *aWriteCount = aCount;

  return NS_OK;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadCString(nsACString& aString)
{
  nsresult rv;
  uint32_t length, bytesRead;

  rv = Read32(&length);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aString.Truncate();
  rv = ReadSegments(WriteSegmentToCString, &aString, length, &bytesRead);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (bytesRead != length) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


// sometimes, WriteSegmentToString will be handed an odd-number of
// bytes, which means we only have half of the last char16_t
struct WriteStringClosure
{
  char16_t* mWriteCursor;
  bool mHasCarryoverByte;
  char mCarryoverByte;
};

// there are a few cases we have to account for here:
// * even length buffer, no carryover - easy, just append
// * odd length buffer, no carryover - the last byte needs to be saved
//                                     for carryover
// * odd length buffer, with carryover - first byte needs to be used
//                              with the carryover byte, and
//                              the rest of the even length
//                              buffer is appended as normal
// * even length buffer, with carryover - the first byte needs to be
//                              used with the previous carryover byte.
//                              this gives you an odd length buffer,
//                              so you have to save the last byte for
//                              the next carryover


// same version of the above, but with correct casting and endian swapping
static NS_METHOD
WriteSegmentToString(nsIInputStream* aStream,
                     void* aClosure,
                     const char* aFromSegment,
                     uint32_t aToOffset,
                     uint32_t aCount,
                     uint32_t* aWriteCount)
{
  NS_PRECONDITION(aCount > 0, "Why are we being told to write 0 bytes?");
  NS_PRECONDITION(sizeof(char16_t) == 2, "We can't handle other sizes!");

  WriteStringClosure* closure = static_cast<WriteStringClosure*>(aClosure);
  char16_t* cursor = closure->mWriteCursor;

  // we're always going to consume the whole buffer no matter what
  // happens, so take care of that right now.. that allows us to
  // tweak aCount later. Do NOT move this!
  *aWriteCount = aCount;

  // if the last Write had an odd-number of bytes read, then
  if (closure->mHasCarryoverByte) {
    // re-create the two-byte sequence we want to work with
    char bytes[2] = { closure->mCarryoverByte, *aFromSegment };
    *cursor = *(char16_t*)bytes;
    // Now the little endianness dance
    mozilla::NativeEndian::swapToBigEndianInPlace(cursor, 1);
    ++cursor;

    // now skip past the first byte of the buffer.. code from here
    // can assume normal operations, but should not assume aCount
    // is relative to the ORIGINAL buffer
    ++aFromSegment;
    --aCount;

    closure->mHasCarryoverByte = false;
  }

  // this array is possibly unaligned... be careful how we access it!
  const char16_t* unicodeSegment =
    reinterpret_cast<const char16_t*>(aFromSegment);

  // calculate number of full characters in segment (aCount could be odd!)
  uint32_t segmentLength = aCount / sizeof(char16_t);

  // copy all data into our aligned buffer.  byte swap if necessary.
  // cursor may be unaligned, so we cannot use copyAndSwapToBigEndian directly
  memcpy(cursor, unicodeSegment, segmentLength * sizeof(char16_t));
  char16_t* end = cursor + segmentLength;
  mozilla::NativeEndian::swapToBigEndianInPlace(cursor, segmentLength);
  closure->mWriteCursor = end;

  // remember this is the modifed aCount and aFromSegment,
  // so that will take into account the fact that we might have
  // skipped the first byte in the buffer
  if (aCount % sizeof(char16_t) != 0) {
    // we must have had a carryover byte, that we'll need the next
    // time around
    closure->mCarryoverByte = aFromSegment[aCount - 1];
    closure->mHasCarryoverByte = true;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsBinaryInputStream::ReadString(nsAString& aString)
{
  nsresult rv;
  uint32_t length, bytesRead;

  rv = Read32(&length);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (length == 0) {
    aString.Truncate();
    return NS_OK;
  }

  // pre-allocate output buffer, and get direct access to buffer...
  if (!aString.SetLength(length, mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsAString::iterator start;
  aString.BeginWriting(start);

  WriteStringClosure closure;
  closure.mWriteCursor = start.get();
  closure.mHasCarryoverByte = false;

  rv = ReadSegments(WriteSegmentToString, &closure,
                    length * sizeof(char16_t), &bytesRead);
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ASSERTION(!closure.mHasCarryoverByte, "some strange stream corruption!");

  if (bytesRead != length * sizeof(char16_t)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadBytes(uint32_t aLength, char** aResult)
{
  nsresult rv;
  uint32_t bytesRead;
  char* s;

  s = reinterpret_cast<char*>(moz_malloc(aLength));
  if (!s) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = Read(s, aLength, &bytesRead);
  if (NS_FAILED(rv)) {
    moz_free(s);
    return rv;
  }
  if (bytesRead != aLength) {
    moz_free(s);
    return NS_ERROR_FAILURE;
  }

  *aResult = s;
  return NS_OK;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadByteArray(uint32_t aLength, uint8_t** aResult)
{
  return ReadBytes(aLength, reinterpret_cast<char**>(aResult));
}

NS_IMETHODIMP
nsBinaryInputStream::ReadArrayBuffer(uint32_t aLength,
                                     JS::Handle<JS::Value> aBuffer,
                                     JSContext* aCx, uint32_t* aReadLength)
{
  if (!aBuffer.isObject()) {
    return NS_ERROR_FAILURE;
  }
  JS::RootedObject buffer(aCx, &aBuffer.toObject());
  if (!JS_IsArrayBufferObject(buffer)) {
    return NS_ERROR_FAILURE;
  }

  uint32_t bufferLength = JS_GetArrayBufferByteLength(buffer);
  if (bufferLength < aLength) {
    return NS_ERROR_FAILURE;
  }

  uint32_t bufSize = std::min<uint32_t>(aLength, 4096);
  UniquePtr<char[]> buf = MakeUnique<char[]>(bufSize);

  uint32_t pos = 0;
  *aReadLength = 0;
  do {
    // Read data into temporary buffer.
    uint32_t bytesRead;
    uint32_t amount = std::min(aLength - pos, bufSize);
    nsresult rv = Read(buf.get(), amount, &bytesRead);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    MOZ_ASSERT(bytesRead <= amount);

    if (bytesRead == 0) {
      break;
    }

    // Copy data into actual buffer.

    JS::AutoCheckCannotGC nogc;
    if (bufferLength != JS_GetArrayBufferByteLength(buffer)) {
      return NS_ERROR_FAILURE;
    }

    char* data = reinterpret_cast<char*>(JS_GetArrayBufferData(buffer, nogc));
    if (!data) {
      return NS_ERROR_FAILURE;
    }

    *aReadLength += bytesRead;
    PodCopy(data + pos, buf.get(), bytesRead);

    pos += bytesRead;
  } while (pos < aLength);

  return NS_OK;
}

NS_IMETHODIMP
nsBinaryInputStream::ReadObject(bool aIsStrongRef, nsISupports** aObject)
{
  nsCID cid;
  nsIID iid;
  nsresult rv = ReadID(&cid);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = ReadID(&iid);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // HACK: Intercept old (pre-gecko6) nsIURI IID, and replace with
  // the updated IID, so that we're QI'ing to an actual interface.
  // (As soon as we drop support for upgrading from pre-gecko6, we can
  // remove this chunk.)
  static const nsIID oldURIiid = {
    0x7a22cc0, 0xce5, 0x11d3,
    { 0x93, 0x31, 0x0, 0x10, 0x4b, 0xa0, 0xfd, 0x40 }
  };

  // hackaround for bug 670542
  static const nsIID oldURIiid2 = {
    0xd6d04c36, 0x0fa4, 0x4db3,
    { 0xbe, 0x05, 0x4a, 0x18, 0x39, 0x71, 0x03, 0xe2 }
  };

  // hackaround for bug 682031
  static const nsIID oldURIiid3 = {
    0x12120b20, 0x0929, 0x40e9,
    { 0x88, 0xcf, 0x6e, 0x08, 0x76, 0x6e, 0x8b, 0x23 }
  };

  if (iid.Equals(oldURIiid) ||
      iid.Equals(oldURIiid2) ||
      iid.Equals(oldURIiid3)) {
    const nsIID newURIiid = NS_IURI_IID;
    iid = newURIiid;
  }
  // END HACK

  nsCOMPtr<nsISupports> object = do_CreateInstance(cid, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsISerializable> serializable = do_QueryInterface(object);
  if (NS_WARN_IF(!serializable)) {
    return NS_ERROR_UNEXPECTED;
  }

  rv = serializable->Read(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return object->QueryInterface(iid, reinterpret_cast<void**>(aObject));
}

NS_IMETHODIMP
nsBinaryInputStream::ReadID(nsID* aResult)
{
  nsresult rv = Read32(&aResult->m0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = Read16(&aResult->m1);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = Read16(&aResult->m2);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (int i = 0; i < 8; ++i) {
    rv = Read8(&aResult->m3[i]);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP_(char*)
nsBinaryInputStream::GetBuffer(uint32_t aLength, uint32_t aAlignMask)
{
  if (mBufferAccess) {
    return mBufferAccess->GetBuffer(aLength, aAlignMask);
  }
  return nullptr;
}

NS_IMETHODIMP_(void)
nsBinaryInputStream::PutBuffer(char* aBuffer, uint32_t aLength)
{
  if (mBufferAccess) {
    mBufferAccess->PutBuffer(aBuffer, aLength);
  }
}
