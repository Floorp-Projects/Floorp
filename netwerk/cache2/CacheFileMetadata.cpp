/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheFileMetadata.h"

#include "CacheFileIOManager.h"
#include "nsICacheEntry.h"
#include "CacheHashUtils.h"
#include "CacheFileChunk.h"
#include "CacheFileUtils.h"
#include "nsILoadContextInfo.h"
#include "../cache/nsCacheUtils.h"
#include "nsIFile.h"
#include "mozilla/Telemetry.h"
#include "mozilla/DebugOnly.h"
#include "prnetdb.h"


namespace mozilla {
namespace net {

#define kMinMetadataRead 1024  // TODO find optimal value from telemetry
#define kAlignSize       4096

#define kCacheEntryVersion 1

NS_IMPL_ISUPPORTS1(CacheFileMetadata, CacheFileIOListener)

CacheFileMetadata::CacheFileMetadata(CacheFileHandle *aHandle, const nsACString &aKey)
  : CacheMemoryConsumer(NORMAL)
  , mHandle(aHandle)
  , mHashArray(nullptr)
  , mHashArraySize(0)
  , mHashCount(0)
  , mOffset(-1)
  , mBuf(nullptr)
  , mBufSize(0)
  , mWriteBuf(nullptr)
  , mElementsSize(0)
  , mIsDirty(false)
  , mAnonymous(false)
  , mInBrowser(false)
  , mAppId(nsILoadContextInfo::NO_APP_ID)
{
  LOG(("CacheFileMetadata::CacheFileMetadata() [this=%p, handle=%p, key=%s]",
       this, aHandle, PromiseFlatCString(aKey).get()));

  MOZ_COUNT_CTOR(CacheFileMetadata);
  memset(&mMetaHdr, 0, sizeof(CacheFileMetadataHeader));
  mMetaHdr.mVersion = kCacheEntryVersion;
  mMetaHdr.mExpirationTime = nsICacheEntry::NO_EXPIRATION_TIME;
  mKey = aKey;

  DebugOnly<nsresult> rv;
  rv = ParseKey(aKey);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

CacheFileMetadata::CacheFileMetadata(bool aMemoryOnly, const nsACString &aKey)
  : CacheMemoryConsumer(aMemoryOnly ? MEMORY_ONLY : NORMAL)
  , mHandle(nullptr)
  , mHashArray(nullptr)
  , mHashArraySize(0)
  , mHashCount(0)
  , mOffset(0)
  , mBuf(nullptr)
  , mBufSize(0)
  , mWriteBuf(nullptr)
  , mElementsSize(0)
  , mIsDirty(true)
  , mAnonymous(false)
  , mInBrowser(false)
  , mAppId(nsILoadContextInfo::NO_APP_ID)
{
  LOG(("CacheFileMetadata::CacheFileMetadata() [this=%p, key=%s]",
       this, PromiseFlatCString(aKey).get()));

  MOZ_COUNT_CTOR(CacheFileMetadata);
  memset(&mMetaHdr, 0, sizeof(CacheFileMetadataHeader));
  mMetaHdr.mVersion = kCacheEntryVersion;
  mMetaHdr.mExpirationTime = nsICacheEntry::NO_EXPIRATION_TIME;
  mMetaHdr.mFetchCount = 1;
  mKey = aKey;
  mMetaHdr.mKeySize = mKey.Length();

  DebugOnly<nsresult> rv;
  rv = ParseKey(aKey);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

CacheFileMetadata::CacheFileMetadata()
  : CacheMemoryConsumer(DONT_REPORT /* This is a helper class */)
  , mHandle(nullptr)
  , mHashArray(nullptr)
  , mHashArraySize(0)
  , mHashCount(0)
  , mOffset(0)
  , mBuf(nullptr)
  , mBufSize(0)
  , mWriteBuf(nullptr)
  , mElementsSize(0)
  , mIsDirty(false)
  , mAnonymous(false)
  , mInBrowser(false)
  , mAppId(nsILoadContextInfo::NO_APP_ID)
{
  LOG(("CacheFileMetadata::CacheFileMetadata() [this=%p]", this));

  MOZ_COUNT_CTOR(CacheFileMetadata);
  memset(&mMetaHdr, 0, sizeof(CacheFileMetadataHeader));
}

CacheFileMetadata::~CacheFileMetadata()
{
  LOG(("CacheFileMetadata::~CacheFileMetadata() [this=%p]", this));

  MOZ_COUNT_DTOR(CacheFileMetadata);
  MOZ_ASSERT(!mListener);

  if (mHashArray) {
    free(mHashArray);
    mHashArray = nullptr;
    mHashArraySize = 0;
  }

  if (mBuf) {
    free(mBuf);
    mBuf = nullptr;
    mBufSize = 0;
  }
}

void
CacheFileMetadata::SetHandle(CacheFileHandle *aHandle)
{
  LOG(("CacheFileMetadata::SetHandle() [this=%p, handle=%p]", this, aHandle));

  MOZ_ASSERT(!mHandle);

  mHandle = aHandle;
}

nsresult
CacheFileMetadata::GetKey(nsACString &_retval)
{
  _retval = mKey;
  return NS_OK;
}

nsresult
CacheFileMetadata::ReadMetadata(CacheFileMetadataListener *aListener)
{
  LOG(("CacheFileMetadata::ReadMetadata() [this=%p, listener=%p]", this, aListener));

  MOZ_ASSERT(!mListener);
  MOZ_ASSERT(!mHashArray);
  MOZ_ASSERT(!mBuf);
  MOZ_ASSERT(!mWriteBuf);

  nsresult rv;

  int64_t size = mHandle->FileSize();
  MOZ_ASSERT(size != -1);

  if (size == 0) {
    // this is a new entry
    LOG(("CacheFileMetadata::ReadMetadata() - Filesize == 0, creating empty "
         "metadata. [this=%p]", this));

    InitEmptyMetadata();
    aListener->OnMetadataRead(NS_OK);
    return NS_OK;
  }

  if (size < int64_t(sizeof(CacheFileMetadataHeader) + 2*sizeof(uint32_t))) {
    // there must be at least checksum, header and offset
    LOG(("CacheFileMetadata::ReadMetadata() - File is corrupted, creating "
         "empty metadata. [this=%p, filesize=%lld]", this, size));

    InitEmptyMetadata();
    aListener->OnMetadataRead(NS_OK);
    return NS_OK;
  }

  // round offset to 4k blocks
  int64_t offset = (size / kAlignSize) * kAlignSize;

  if (size - offset < kMinMetadataRead && offset > kAlignSize)
    offset -= kAlignSize;

  mBufSize = size - offset;
  mBuf = static_cast<char *>(moz_xmalloc(mBufSize));

  DoMemoryReport(MemoryUsage());

  LOG(("CacheFileMetadata::ReadMetadata() - Reading metadata from disk, trying "
       "offset=%lld, filesize=%lld [this=%p]", offset, size, this));

  mListener = aListener;
  rv = CacheFileIOManager::Read(mHandle, offset, mBuf, mBufSize, this);
  if (NS_FAILED(rv)) {
    LOG(("CacheFileMetadata::ReadMetadata() - CacheFileIOManager::Read() failed"
         " synchronously, creating empty metadata. [this=%p, rv=0x%08x]",
         this, rv));

    mListener = nullptr;
    InitEmptyMetadata();
    aListener->OnMetadataRead(NS_OK);
    return NS_OK;
  }

  return NS_OK;
}

nsresult
CacheFileMetadata::WriteMetadata(uint32_t aOffset,
                                 CacheFileMetadataListener *aListener)
{
  LOG(("CacheFileMetadata::WriteMetadata() [this=%p, offset=%d, listener=%p]",
       this, aOffset, aListener));

  MOZ_ASSERT(!mListener);
  MOZ_ASSERT(!mWriteBuf);

  nsresult rv;

  mIsDirty = false;

  mWriteBuf = static_cast<char *>(moz_xmalloc(sizeof(uint32_t) +
                mHashCount * sizeof(CacheHash::Hash16_t) +
                sizeof(CacheFileMetadataHeader) + mKey.Length() + 1 +
                mElementsSize + sizeof(uint32_t)));

  char *p = mWriteBuf + sizeof(uint32_t);
  memcpy(p, mHashArray, mHashCount * sizeof(CacheHash::Hash16_t));
  p += mHashCount * sizeof(CacheHash::Hash16_t);
  mMetaHdr.WriteToBuf(p);
  p += sizeof(CacheFileMetadataHeader);
  memcpy(p, mKey.get(), mKey.Length());
  p += mKey.Length();
  *p = 0;
  p++;
  memcpy(p, mBuf, mElementsSize);
  p += mElementsSize;

  CacheHash::Hash32_t hash;
  hash = CacheHash::Hash(mWriteBuf + sizeof(uint32_t),
                         p - mWriteBuf - sizeof(uint32_t));
  NetworkEndian::writeUint32(mWriteBuf, hash);

  NetworkEndian::writeUint32(p, aOffset);
  p += sizeof(uint32_t);

  char * writeBuffer;
  if (aListener) {
    mListener = aListener;
    writeBuffer = mWriteBuf;
  } else {
    // We are not going to pass |this| as callback to CacheFileIOManager::Write
    // so we must allocate a new buffer that will be released automatically when
    // write is finished.  This is actually better than to let
    // CacheFileMetadata::OnDataWritten do the job, since when dispatching the
    // result from some reason fails during shutdown, we would unnecessarily leak
    // both this object and the buffer.
    writeBuffer = static_cast<char *>(moz_xmalloc(p - mWriteBuf));
    memcpy(mWriteBuf, writeBuffer, p - mWriteBuf);
  }

  rv = CacheFileIOManager::Write(mHandle, aOffset, writeBuffer, p - mWriteBuf,
                                 true, aListener ? this : nullptr);
  if (NS_FAILED(rv)) {
    LOG(("CacheFileMetadata::WriteMetadata() - CacheFileIOManager::Write() "
         "failed synchronously. [this=%p, rv=0x%08x]", this, rv));

    mListener = nullptr;
    if (writeBuffer != mWriteBuf) {
      free(writeBuffer);
    }
    free(mWriteBuf);
    mWriteBuf = nullptr;
    NS_ENSURE_SUCCESS(rv, rv);
  }

  DoMemoryReport(MemoryUsage());

  return NS_OK;
}

nsresult
CacheFileMetadata::SyncReadMetadata(nsIFile *aFile)
{
  LOG(("CacheFileMetadata::SyncReadMetadata() [this=%p]", this));

  MOZ_ASSERT(!mListener);
  MOZ_ASSERT(!mHandle);
  MOZ_ASSERT(!mHashArray);
  MOZ_ASSERT(!mBuf);
  MOZ_ASSERT(!mWriteBuf);
  MOZ_ASSERT(mKey.IsEmpty());

  nsresult rv;

  int64_t fileSize;
  rv = aFile->GetFileSize(&fileSize);
  NS_ENSURE_SUCCESS(rv, rv);

  PRFileDesc *fd;
  rv = aFile->OpenNSPRFileDesc(PR_RDONLY, 0600, &fd);
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t offset = PR_Seek64(fd, fileSize - sizeof(uint32_t), PR_SEEK_SET);
  if (offset == -1) {
    PR_Close(fd);
    return NS_ERROR_FAILURE;
  }

  uint32_t metaOffset;
  int32_t bytesRead = PR_Read(fd, &metaOffset, sizeof(uint32_t));
  if (bytesRead != sizeof(uint32_t)) {
    PR_Close(fd);
    return NS_ERROR_FAILURE;
  }

  metaOffset = NetworkEndian::readUint32(&metaOffset);
  if (metaOffset > fileSize) {
    PR_Close(fd);
    return NS_ERROR_FAILURE;
  }

  mBufSize = fileSize - metaOffset;
  mBuf = static_cast<char *>(moz_xmalloc(mBufSize));

  DoMemoryReport(MemoryUsage());

  offset = PR_Seek64(fd, metaOffset, PR_SEEK_SET);
  if (offset == -1) {
    PR_Close(fd);
    return NS_ERROR_FAILURE;
  }

  bytesRead = PR_Read(fd, mBuf, mBufSize);
  PR_Close(fd);
  if (bytesRead != static_cast<int32_t>(mBufSize)) {
    return NS_ERROR_FAILURE;
  }

  rv = ParseMetadata(metaOffset, 0, false);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

const char *
CacheFileMetadata::GetElement(const char *aKey)
{
  const char *data = mBuf;
  const char *limit = mBuf + mElementsSize;

  while (data < limit) {
    // Point to the value part
    const char *value = data + strlen(data) + 1;
    MOZ_ASSERT(value < limit, "Metadata elements corrupted");
    if (strcmp(data, aKey) == 0) {
      LOG(("CacheFileMetadata::GetElement() - Key found [this=%p, key=%s]",
           this, aKey));
      return value;
    }

    // Skip value part
    data = value + strlen(value) + 1;
  }
  MOZ_ASSERT(data == limit, "Metadata elements corrupted");
  LOG(("CacheFileMetadata::GetElement() - Key not found [this=%p, key=%s]",
       this, aKey));
  return nullptr;
}

nsresult
CacheFileMetadata::SetElement(const char *aKey, const char *aValue)
{
  LOG(("CacheFileMetadata::SetElement() [this=%p, key=%s, value=%p]",
       this, aKey, aValue));

  MarkDirty();

  const uint32_t keySize = strlen(aKey) + 1;
  char *pos = const_cast<char *>(GetElement(aKey));

  if (!aValue) {
    // No value means remove the key/value pair completely, if existing
    if (pos) {
      uint32_t oldValueSize = strlen(pos) + 1;
      uint32_t offset = pos - mBuf;
      uint32_t remainder = mElementsSize - (offset + oldValueSize);

      memmove(pos - keySize, pos + oldValueSize, remainder);
      mElementsSize -= keySize + oldValueSize;
    }
    return NS_OK;
  }

  const uint32_t valueSize = strlen(aValue) + 1;
  uint32_t newSize = mElementsSize + valueSize;
  if (pos) {
    const uint32_t oldValueSize = strlen(pos) + 1;
    const uint32_t offset = pos - mBuf;
    const uint32_t remainder = mElementsSize - (offset + oldValueSize);

    // Update the value in place
    newSize -= oldValueSize;
    EnsureBuffer(newSize);

    // Move the remainder to the right place
    pos = mBuf + offset;
    memmove(pos + valueSize, pos + oldValueSize, remainder);
  } else {
    // allocate new meta data element
    newSize += keySize;
    EnsureBuffer(newSize);

    // Add after last element
    pos = mBuf + mElementsSize;
    memcpy(pos, aKey, keySize);
    pos += keySize;
  }

  // Update value
  memcpy(pos, aValue, valueSize);
  mElementsSize = newSize;

  return NS_OK;
}

CacheHash::Hash16_t
CacheFileMetadata::GetHash(uint32_t aIndex)
{
  MOZ_ASSERT(aIndex < mHashCount);
  return NetworkEndian::readUint16(&mHashArray[aIndex]);
}

nsresult
CacheFileMetadata::SetHash(uint32_t aIndex, CacheHash::Hash16_t aHash)
{
  LOG(("CacheFileMetadata::SetHash() [this=%p, idx=%d, hash=%x]",
       this, aIndex, aHash));

  MarkDirty();

  MOZ_ASSERT(aIndex <= mHashCount);

  if (aIndex > mHashCount) {
    return NS_ERROR_INVALID_ARG;
  } else if (aIndex == mHashCount) {
    if ((aIndex + 1) * sizeof(CacheHash::Hash16_t) > mHashArraySize) {
      // reallocate hash array buffer
      if (mHashArraySize == 0)
        mHashArraySize = 32 * sizeof(CacheHash::Hash16_t);
      else
        mHashArraySize *= 2;
      mHashArray = static_cast<CacheHash::Hash16_t *>(
                     moz_xrealloc(mHashArray, mHashArraySize));
    }

    mHashCount++;
  }

  NetworkEndian::writeUint16(&mHashArray[aIndex], aHash);

  DoMemoryReport(MemoryUsage());

  return NS_OK;
}

nsresult
CacheFileMetadata::SetExpirationTime(uint32_t aExpirationTime)
{
  LOG(("CacheFileMetadata::SetExpirationTime() [this=%p, expirationTime=%d]",
       this, aExpirationTime));

  MarkDirty();
  mMetaHdr.mExpirationTime = aExpirationTime;
  return NS_OK;
}

nsresult
CacheFileMetadata::GetExpirationTime(uint32_t *_retval)
{
  *_retval = mMetaHdr.mExpirationTime;
  return NS_OK;
}

nsresult
CacheFileMetadata::SetLastModified(uint32_t aLastModified)
{
  LOG(("CacheFileMetadata::SetLastModified() [this=%p, lastModified=%d]",
       this, aLastModified));

  MarkDirty();
  mMetaHdr.mLastModified = aLastModified;
  return NS_OK;
}

nsresult
CacheFileMetadata::GetLastModified(uint32_t *_retval)
{
  *_retval = mMetaHdr.mLastModified;
  return NS_OK;
}

nsresult
CacheFileMetadata::SetFrecency(uint32_t aFrecency)
{
  LOG(("CacheFileMetadata::SetFrecency() [this=%p, frecency=%f]",
       this, (double)aFrecency));

  MarkDirty();
  mMetaHdr.mFrecency = aFrecency;
  return NS_OK;
}

nsresult
CacheFileMetadata::GetFrecency(uint32_t *_retval)
{
  *_retval = mMetaHdr.mFrecency;
  return NS_OK;
}

nsresult
CacheFileMetadata::GetLastFetched(uint32_t *_retval)
{
  *_retval = mMetaHdr.mLastFetched;
  return NS_OK;
}

nsresult
CacheFileMetadata::GetFetchCount(uint32_t *_retval)
{
  *_retval = mMetaHdr.mFetchCount;
  return NS_OK;
}

nsresult
CacheFileMetadata::OnFileOpened(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_CRASH("CacheFileMetadata::OnFileOpened should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileMetadata::OnDataWritten(CacheFileHandle *aHandle, const char *aBuf,
                                 nsresult aResult)
{
  LOG(("CacheFileMetadata::OnDataWritten() [this=%p, handle=%p, result=0x%08x]",
       this, aHandle, aResult));

  MOZ_ASSERT(mListener);
  MOZ_ASSERT(mWriteBuf);

  free(mWriteBuf);
  mWriteBuf = nullptr;

  nsCOMPtr<CacheFileMetadataListener> listener;

  mListener.swap(listener);
  listener->OnMetadataWritten(aResult);

  DoMemoryReport(MemoryUsage());

  return NS_OK;
}

nsresult
CacheFileMetadata::OnDataRead(CacheFileHandle *aHandle, char *aBuf,
                              nsresult aResult)
{
  LOG(("CacheFileMetadata::OnDataRead() [this=%p, handle=%p, result=0x%08x]",
       this, aHandle, aResult));

  MOZ_ASSERT(mListener);

  nsresult rv, retval;
  nsCOMPtr<CacheFileMetadataListener> listener;

  if (NS_FAILED(aResult)) {
    LOG(("CacheFileMetadata::OnDataRead() - CacheFileIOManager::Read() failed"
         ", creating empty metadata. [this=%p, rv=0x%08x]", this, aResult));

    InitEmptyMetadata();
    retval = NS_OK;

    mListener.swap(listener);
    listener->OnMetadataRead(retval);
    return NS_OK;
  }

  // check whether we have read all necessary data
  uint32_t realOffset = NetworkEndian::readUint32(mBuf + mBufSize -
                                                  sizeof(uint32_t));

  int64_t size = mHandle->FileSize();
  MOZ_ASSERT(size != -1);

  if (realOffset >= size) {
    LOG(("CacheFileMetadata::OnDataRead() - Invalid realOffset, creating "
         "empty metadata. [this=%p, realOffset=%d, size=%lld]", this,
         realOffset, size));

    InitEmptyMetadata();
    retval = NS_OK;

    mListener.swap(listener);
    listener->OnMetadataRead(retval);
    return NS_OK;
  }

  uint32_t usedOffset = size - mBufSize;

  if (realOffset < usedOffset) {
    uint32_t missing = usedOffset - realOffset;
    // we need to read more data
    mBuf = static_cast<char *>(moz_xrealloc(mBuf, mBufSize + missing));
    memmove(mBuf + missing, mBuf, mBufSize);
    mBufSize += missing;

    DoMemoryReport(MemoryUsage());

    LOG(("CacheFileMetadata::OnDataRead() - We need to read %d more bytes to "
         "have full metadata. [this=%p]", missing, this));

    rv = CacheFileIOManager::Read(mHandle, realOffset, mBuf, missing, this);
    if (NS_FAILED(rv)) {
      LOG(("CacheFileMetadata::OnDataRead() - CacheFileIOManager::Read() "
           "failed synchronously, creating empty metadata. [this=%p, "
           "rv=0x%08x]", this, rv));

      InitEmptyMetadata();
      retval = NS_OK;

      mListener.swap(listener);
      listener->OnMetadataRead(retval);
      return NS_OK;
    }

    return NS_OK;
  }

  // We have all data according to offset information at the end of the entry.
  // Try to parse it.
  rv = ParseMetadata(realOffset, realOffset - usedOffset, true);
  if (NS_FAILED(rv)) {
    LOG(("CacheFileMetadata::OnDataRead() - Error parsing metadata, creating "
         "empty metadata. [this=%p]", this));
    InitEmptyMetadata();
    retval = NS_OK;
  }
  else {
    retval = NS_OK;
  }

  mListener.swap(listener);
  listener->OnMetadataRead(retval);

  return NS_OK;
}

nsresult
CacheFileMetadata::OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_CRASH("CacheFileMetadata::OnFileDoomed should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileMetadata::OnEOFSet(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_CRASH("CacheFileMetadata::OnEOFSet should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileMetadata::OnFileRenamed(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_CRASH("CacheFileMetadata::OnFileRenamed should not be called!");
  return NS_ERROR_UNEXPECTED;
}

void
CacheFileMetadata::InitEmptyMetadata()
{
  if (mBuf) {
    free(mBuf);
    mBuf = nullptr;
    mBufSize = 0;
  }
  mOffset = 0;
  mMetaHdr.mVersion = kCacheEntryVersion;
  mMetaHdr.mFetchCount = 1;
  mMetaHdr.mExpirationTime = nsICacheEntry::NO_EXPIRATION_TIME;
  mMetaHdr.mKeySize = mKey.Length();

  DoMemoryReport(MemoryUsage());
}

nsresult
CacheFileMetadata::ParseMetadata(uint32_t aMetaOffset, uint32_t aBufOffset,
                                 bool aHaveKey)
{
  LOG(("CacheFileMetadata::ParseMetadata() [this=%p, metaOffset=%d, "
       "bufOffset=%d, haveKey=%u]", this, aMetaOffset, aBufOffset, aHaveKey));

  nsresult rv;

  uint32_t metaposOffset = mBufSize - sizeof(uint32_t);
  uint32_t hashesOffset = aBufOffset + sizeof(uint32_t);
  uint32_t hashCount = aMetaOffset / kChunkSize;
  if (aMetaOffset % kChunkSize)
    hashCount++;
  uint32_t hashesLen = hashCount * sizeof(CacheHash::Hash16_t);
  uint32_t hdrOffset = hashesOffset + hashesLen;
  uint32_t keyOffset = hdrOffset + sizeof(CacheFileMetadataHeader);

  LOG(("CacheFileMetadata::ParseMetadata() [this=%p]\n  metaposOffset=%d\n  "
       "hashesOffset=%d\n  hashCount=%d\n  hashesLen=%d\n  hdfOffset=%d\n  "
       "keyOffset=%d\n", this, metaposOffset, hashesOffset, hashCount,
       hashesLen,hdrOffset, keyOffset));

  if (keyOffset > metaposOffset) {
    LOG(("CacheFileMetadata::ParseMetadata() - Wrong keyOffset! [this=%p]",
         this));
    return NS_ERROR_FILE_CORRUPTED;
  }

  uint32_t elementsOffset = reinterpret_cast<CacheFileMetadataHeader *>(
                              mBuf + hdrOffset)->mKeySize + keyOffset + 1;

  if (elementsOffset > metaposOffset) {
    LOG(("CacheFileMetadata::ParseMetadata() - Wrong elementsOffset %d "
         "[this=%p]", elementsOffset, this));
    return NS_ERROR_FILE_CORRUPTED;
  }

  // check that key ends with \0
  if (mBuf[elementsOffset - 1] != 0) {
    LOG(("CacheFileMetadata::ParseMetadata() - Elements not null terminated. "
         "[this=%p]", this));
    return NS_ERROR_FILE_CORRUPTED;
  }

  uint32_t keySize = reinterpret_cast<CacheFileMetadataHeader *>(
                       mBuf + hdrOffset)->mKeySize;

  if (!aHaveKey) {
    // get the key form metadata
    mKey.Assign(mBuf + keyOffset, keySize);

    rv = ParseKey(mKey);
    if (NS_FAILED(rv))
      return rv;
  }
  else {
    if (keySize != mKey.Length()) {
      LOG(("CacheFileMetadata::ParseMetadata() - Key collision (1), key=%s "
           "[this=%p]", nsCString(mBuf + keyOffset, keySize).get(), this));
      return NS_ERROR_FILE_CORRUPTED;
    }

    if (memcmp(mKey.get(), mBuf + keyOffset, mKey.Length()) != 0) {
      LOG(("CacheFileMetadata::ParseMetadata() - Key collision (2), key=%s "
           "[this=%p]", nsCString(mBuf + keyOffset, keySize).get(), this));
      return NS_ERROR_FILE_CORRUPTED;
    }
  }

  // check metadata hash (data from hashesOffset to metaposOffset)
  CacheHash::Hash32_t hashComputed, hashExpected;
  hashComputed = CacheHash::Hash(mBuf + hashesOffset,
                                 metaposOffset - hashesOffset);
  hashExpected = NetworkEndian::readUint32(mBuf + aBufOffset);

  if (hashComputed != hashExpected) {
    LOG(("CacheFileMetadata::ParseMetadata() - Metadata hash mismatch! Hash of "
         "the metadata is %x, hash in file is %x [this=%p]", hashComputed,
         hashExpected, this));
    return NS_ERROR_FILE_CORRUPTED;
  }

  // check elements
  rv = CheckElements(mBuf + elementsOffset, metaposOffset - elementsOffset);
  if (NS_FAILED(rv))
    return rv;

  mHashArraySize = hashesLen;
  mHashCount = hashCount;
  if (mHashArraySize) {
    mHashArray = static_cast<CacheHash::Hash16_t *>(
                   moz_xmalloc(mHashArraySize));
    memcpy(mHashArray, mBuf + hashesOffset, mHashArraySize);
  }

  mMetaHdr.ReadFromBuf(mBuf + hdrOffset);

  if (mMetaHdr.mVersion != kCacheEntryVersion) {
    LOG(("CacheFileMetadata::ParseMetadata() - Not a version we understand to. "
         "[version=0x%x, this=%p]", mMetaHdr.mVersion, this));
    return NS_ERROR_UNEXPECTED;
  }


  mMetaHdr.mFetchCount++;
  MarkDirty();

  mElementsSize = metaposOffset - elementsOffset;
  memmove(mBuf, mBuf + elementsOffset, mElementsSize);
  mOffset = aMetaOffset;

  // TODO: shrink memory if buffer is too big

  DoMemoryReport(MemoryUsage());

  return NS_OK;
}

nsresult
CacheFileMetadata::CheckElements(const char *aBuf, uint32_t aSize)
{
  if (aSize) {
    // Check if the metadata ends with a zero byte.
    if (aBuf[aSize - 1] != 0) {
      NS_ERROR("Metadata elements are not null terminated");
      LOG(("CacheFileMetadata::CheckElements() - Elements are not null "
           "terminated. [this=%p]", this));
      return NS_ERROR_FILE_CORRUPTED;
    }
    // Check that there are an even number of zero bytes
    // to match the pattern { key \0 value \0 }
    bool odd = false;
    for (uint32_t i = 0; i < aSize; i++) {
      if (aBuf[i] == 0)
        odd = !odd;
    }
    if (odd) {
      NS_ERROR("Metadata elements are malformed");
      LOG(("CacheFileMetadata::CheckElements() - Elements are malformed. "
           "[this=%p]", this));
      return NS_ERROR_FILE_CORRUPTED;
    }
  }
  return NS_OK;
}

void
CacheFileMetadata::EnsureBuffer(uint32_t aSize)
{
  if (mBufSize < aSize) {
    mBufSize = aSize;
    mBuf = static_cast<char *>(moz_xrealloc(mBuf, mBufSize));
  }

  DoMemoryReport(MemoryUsage());
}

nsresult
CacheFileMetadata::ParseKey(const nsACString &aKey)
{
  nsCOMPtr<nsILoadContextInfo> info = CacheFileUtils::ParseKey(aKey);
  NS_ENSURE_TRUE(info, NS_ERROR_FAILURE);

  mAnonymous =  info->IsAnonymous();
  mAppId = info->AppId();
  mInBrowser = info->IsInBrowserElement();

  return NS_OK;
}

// Memory reporting

size_t
CacheFileMetadata::SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  size_t n = 0;
  // mHandle reported via CacheFileIOManager.
  n += mKey.SizeOfExcludingThisIfUnshared(mallocSizeOf);
  n += mallocSizeOf(mHashArray);
  n += mallocSizeOf(mBuf);
  n += mallocSizeOf(mWriteBuf);
  // mListener is usually the owning CacheFile.

  return n;
}

size_t
CacheFileMetadata::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  return mallocSizeOf(this) + SizeOfExcludingThis(mallocSizeOf);
}

} // net
} // mozilla
