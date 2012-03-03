//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// Originally based on Chrome sources:
// Copyright (c) 2010 The Chromium Authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include "HashStore.h"
#include "nsAutoPtr.h"
#include "nsICryptoHash.h"
#include "nsISeekableStream.h"
#include "nsIStreamConverterService.h"
#include "nsNetUtil.h"
#include "nsCheckSummedOutputStream.h"
#include "prlog.h"
#include "zlib.h"

// Main store for SafeBrowsing protocol data. We store
// known add/sub chunks, prefixes and completions in memory
// during an update, and serialize to disk.
// We do not store the add prefixes, those are retrieved by
// decompressing the PrefixSet cache whenever we need to apply
// an update.
//
// byte slicing: Many of the 4-byte values stored here are strongly
// correlated in the upper bytes, and uncorrelated in the lower
// bytes. Because zlib/DEFLATE requires match lengths of at least
// 3 to achieve good compression, and we don't get those if only
// the upper 16-bits are correlated, it is worthwhile to slice 32-bit
// values into 4 1-byte slices and compress the slices individually.
// The slices corresponding to MSBs will compress very well, and the
// slice corresponding to LSB almost nothing. Because of this, we
// only apply DEFLATE to the 3 most significant bytes, and store the
// LSB uncompressed.
//
// byte sliced (numValues) data format:
//    uint32 compressed-size
//    compressed-size bytes    zlib DEFLATE data
//        0...numValues        byte MSB of 4-byte numValues data
//    uint32 compressed-size
//    compressed-size bytes    zlib DEFLATE data
//        0...numValues        byte 2nd byte of 4-byte numValues data
//    uint32 compressed-size
//    compressed-size bytes    zlib DEFLATE data
//        0...numValues        byte 3rd byte of 4-byte numValues data
//    0...numValues            byte LSB of 4-byte numValues data
//
// Store data format:
//    uint32 magic
//    uint32 version
//    uint32 numAddChunks
//    uint32 numSubChunks
//    uint32 numAddPrefixes
//    uint32 numSubPrefixes
//    uint32 numAddCompletes
//    uint32 numSubCompletes
//    0...numAddChunks               uint32 addChunk
//    0...numSubChunks               uint32 subChunk
//    byte sliced (numAddPrefixes)   uint32 add chunk of AddPrefixes
//    byte sliced (numSubPrefixes)   uint32 sub chunk of SubPrefixes
//    byte sliced (numSubPrefixes)   uint32 add chunk of SubPrefixes
//    byte sliced (numSubPrefixes)   uint32 SubPrefixes
//    0...numAddCompletes           32-byte Completions
//    0...numSubCompletes           32-byte Completions
//    16-byte MD5 of all preceding data

// NSPR_LOG_MODULES=UrlClassifierDbService:5
extern PRLogModuleInfo *gUrlClassifierDbServiceLog;
#if defined(PR_LOGGING)
#define LOG(args) PR_LOG(gUrlClassifierDbServiceLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gUrlClassifierDbServiceLog, 4)
#else
#define LOG(args)
#define LOG_ENABLED() (PR_FALSE)
#endif

namespace mozilla {
namespace safebrowsing {

const uint32 STORE_MAGIC = 0x1231af3b;
const uint32 CURRENT_VERSION = 2;

void
TableUpdate::NewAddPrefix(PRUint32 aAddChunk, const Prefix& aHash)
{
  AddPrefix *add = mAddPrefixes.AppendElement();
  add->addChunk = aAddChunk;
  add->prefix = aHash;
}

void
TableUpdate::NewSubPrefix(PRUint32 aAddChunk, const Prefix& aHash, PRUint32 aSubChunk)
{
  SubPrefix *sub = mSubPrefixes.AppendElement();
  sub->addChunk = aAddChunk;
  sub->prefix = aHash;
  sub->subChunk = aSubChunk;
}

void
TableUpdate::NewAddComplete(PRUint32 aAddChunk, const Completion& aHash)
{
  AddComplete *add = mAddCompletes.AppendElement();
  add->addChunk = aAddChunk;
  add->hash.complete = aHash;
}

void
TableUpdate::NewSubComplete(PRUint32 aAddChunk, const Completion& aHash, PRUint32 aSubChunk)
{
  SubComplete *sub = mSubCompletes.AppendElement();
  sub->addChunk = aAddChunk;
  sub->hash.complete = aHash;
  sub->subChunk = aSubChunk;
}


HashStore::HashStore(const nsACString& aTableName, nsIFile* aStoreDir)
  : mTableName(aTableName)
  , mStoreDirectory(aStoreDir)
  , mInUpdate(false)
{
}

HashStore::~HashStore()
{
}

nsresult
HashStore::Reset()
{
  LOG(("HashStore resetting"));

  nsCOMPtr<nsIFile> storeFile;
  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(storeFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = storeFile->AppendNative(mTableName + NS_LITERAL_CSTRING(".sbstore"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = storeFile->Remove(PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  Clear();

  return NS_OK;
}

nsresult
HashStore::CheckChecksum(nsIFile* aStoreFile)
{
  // Check for file corruption by
  // comparing the stored checksum to actual checksum of data
  nsCAutoString hash;
  nsCAutoString compareHash;
  char *data;
  PRUint32 read;

  PRInt64 fileSize;
  nsresult rv = aStoreFile->GetFileSize(&fileSize);
  NS_ENSURE_SUCCESS(rv, rv);

  if (fileSize < 0) {
    return NS_ERROR_FAILURE;
  }

  rv = CalculateChecksum(hash, true);
  NS_ENSURE_SUCCESS(rv, rv);

  compareHash.GetMutableData(&data, hash.Length());

  nsCOMPtr<nsISeekableStream> seekIn = do_QueryInterface(mInputStream);
  rv = seekIn->Seek(nsISeekableStream::NS_SEEK_SET, fileSize-hash.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mInputStream->Read(data, hash.Length(), &read);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(read == hash.Length(), "Could not read hash bytes");

  if (!hash.Equals(compareHash)) {
    NS_WARNING("Safebrowing file failed checksum.");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
HashStore::Open()
{
  nsCOMPtr<nsIFile> storeFile;
  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(storeFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = storeFile->AppendNative(mTableName + NS_LITERAL_CSTRING(".sbstore"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> origStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(origStream), storeFile,
                                  PR_RDONLY);

  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND) {
    Reset();
    return rv;
  }

  if (rv == NS_ERROR_FILE_NOT_FOUND) {
    Clear();
    UpdateHeader();
    return NS_OK;
  }

  PRInt64 fileSize;
  rv = storeFile->GetFileSize(&fileSize);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewBufferedInputStream(getter_AddRefs(mInputStream), origStream,
                                 fileSize);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CheckChecksum(storeFile);
  if (NS_FAILED(rv)) {
    Reset();
    return rv;
  }

  rv = ReadHeader();
  if (NS_FAILED(rv)) {
    Reset();
    return rv;
  }

  rv = SanityCheck(storeFile);
  if (NS_FAILED(rv)) {
    NS_WARNING("Safebrowsing file failed sanity check. probably out of date.");
    Reset();
    return rv;
  }

  rv = ReadChunkNumbers();
  if (NS_FAILED(rv)) {
    Reset();
    return rv;
  }

  return NS_OK;
}

void
HashStore::Clear()
{
  mAddChunks.Clear();
  mSubChunks.Clear();
  mAddExpirations.Clear();
  mSubExpirations.Clear();
  mAddPrefixes.Clear();
  mSubPrefixes.Clear();
  mAddCompletes.Clear();
  mSubCompletes.Clear();
}

nsresult
HashStore::ReadEntireStore()
{
  Clear();

  nsresult rv = ReadHeader();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReadChunkNumbers();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReadHashes();
  if (NS_FAILED(rv)) {
    // we are the only one reading this so it's up to us to detect corruption
    Reset();
  }

  return rv;
}

nsresult
HashStore::ReadHeader()
{
  if (!mInputStream) {
    Clear();
    UpdateHeader();
    return NS_OK;
  }

  nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mInputStream);
  nsresult rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  void *buffer = &mHeader;
  rv = NS_ReadInputStreamToBuffer(mInputStream,
                                  &buffer,
                                  sizeof(Header));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
HashStore::SanityCheck(nsIFile *storeFile)
{
  if (mHeader.magic != STORE_MAGIC || mHeader.version != CURRENT_VERSION) {
    NS_WARNING("Unexpected header data in the store.");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
HashStore::CalculateChecksum(nsCAutoString& aChecksum, bool aChecksumPresent)
{
  aChecksum.Truncate();

  nsCOMPtr<nsIFile> storeFile;
  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(storeFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = storeFile->AppendNative(mTableName + NS_LITERAL_CSTRING(".sbstore"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> hashStream;

  rv = NS_NewLocalFileInputStream(getter_AddRefs(hashStream), storeFile,
                                  PR_RDONLY);

  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND) {
    Reset();
    return rv;
  }

  PRInt64 fileSize;
  rv = storeFile->GetFileSize(&fileSize);
  NS_ENSURE_SUCCESS(rv, rv);

  if (fileSize < 0) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsICryptoHash> hash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Size of MD5 hash in bytes
  const uint32 CHECKSUM_SIZE = 16;

  rv = hash->Init(nsICryptoHash::MD5);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aChecksumPresent) {
    // Hash entire file
    rv = hash->UpdateFromStream(hashStream, PR_UINT32_MAX);
  } else {
    // Hash everything but last checksum bytes
    rv = hash->UpdateFromStream(hashStream, fileSize-CHECKSUM_SIZE);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = hash->Finish(PR_FALSE, aChecksum);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
HashStore::UpdateHeader()
{
  mHeader.magic = STORE_MAGIC;
  mHeader.version = CURRENT_VERSION;

  mHeader.numAddChunks = mAddChunks.Length();
  mHeader.numSubChunks = mSubChunks.Length();
  mHeader.numAddPrefixes = mAddPrefixes.Length();
  mHeader.numSubPrefixes = mSubPrefixes.Length();
  mHeader.numAddCompletes = mAddCompletes.Length();
  mHeader.numSubCompletes = mSubCompletes.Length();
}

nsresult
HashStore::ReadChunkNumbers()
{
  if (!mInputStream) {
    LOG(("Clearing."));
    Clear();
    return NS_OK;
  }

  nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mInputStream);
  nsresult rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET,
                               sizeof(Header));

  rv = mAddChunks.Read(mInputStream, mHeader.numAddChunks);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(mAddChunks.Length() == mHeader.numAddChunks, "Read the right amount of add chunks.");

  rv = mSubChunks.Read(mInputStream, mHeader.numSubChunks);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(mSubChunks.Length() == mHeader.numSubChunks, "Read the right amount of sub chunks.");

  return NS_OK;
}

nsresult
HashStore::ReadHashes()
{
  if (!mInputStream) {
    return NS_OK;
  }

  nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mInputStream);

  uint32 offset = sizeof(Header);
  offset += (mHeader.numAddChunks + mHeader.numSubChunks) * sizeof(uint32);
  nsresult rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, offset);

  rv = ReadAddPrefixes();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReadSubPrefixes();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReadTArray(mInputStream, &mAddCompletes, mHeader.numAddCompletes);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReadTArray(mInputStream, &mSubCompletes, mHeader.numSubCompletes);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
HashStore::BeginUpdate()
{
  mInUpdate = true;

  nsresult rv = ReadEntireStore();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

template<class T>
static nsresult
Merge(ChunkSet* aStoreChunks,
      nsTArray<T>* aStorePrefixes,
      ChunkSet& aUpdateChunks,
      nsTArray<T>& aUpdatePrefixes)
{
  EntrySort(aUpdatePrefixes);

  T* updateIter = aUpdatePrefixes.Elements();
  T* updateEnd = aUpdatePrefixes.Elements() + aUpdatePrefixes.Length();

  T* storeIter = aStorePrefixes->Elements();
  T* storeEnd = aStorePrefixes->Elements() + aStorePrefixes->Length();

  // use a separate array so we can keep the iterators valid
  // if the nsTArray grows
  nsTArray<T> adds;

  for (; updateIter != updateEnd; updateIter++) {
    // XXX: binary search for insertion point might be faster in common
    // case?
    while (storeIter < storeEnd && (storeIter->Compare(*updateIter) < 0)) {
      // skip forward to matching element (or not...)
      storeIter++;
    }
    // no match, add
    if (storeIter == storeEnd
        || storeIter->Compare(*updateIter) != 0) {
      if (!adds.AppendElement(*updateIter))
        return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // chunks can be empty, but we should still report we have them
  // to make the chunkranges continuous
  aStoreChunks->Merge(aUpdateChunks);

  aStorePrefixes->AppendElements(adds);
  EntrySort(*aStorePrefixes);

  return NS_OK;
}

nsresult
HashStore::ApplyUpdate(TableUpdate &update)
{
  nsresult rv = mAddExpirations.Merge(update.AddExpirations());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mSubExpirations.Merge(update.SubExpirations());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Expire();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Merge(&mAddChunks, &mAddPrefixes,
             update.AddChunks(), update.AddPrefixes());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Merge(&mAddChunks, &mAddCompletes,
             update.AddChunks(), update.AddCompletes());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Merge(&mSubChunks, &mSubPrefixes,
             update.SubChunks(), update.SubPrefixes());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Merge(&mSubChunks, &mSubCompletes,
             update.SubChunks(), update.SubCompletes());
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
HashStore::Rebuild()
{
  NS_ASSERTION(mInUpdate, "Must be in update to rebuild.");

  nsresult rv = ProcessSubs();
  NS_ENSURE_SUCCESS(rv, rv);

  UpdateHeader();

  return NS_OK;
}

template<class T>
static void
ExpireEntries(nsTArray<T>* aEntries, ChunkSet& aExpirations)
{
  T* addIter = aEntries->Elements();
  T* end = aEntries->Elements() + aEntries->Length();

  for (T *iter = addIter; iter != end; iter++) {
    if (!aExpirations.Has(iter->Chunk())) {
      *addIter = *iter;
      addIter++;
    }
  }

  aEntries->SetLength(addIter - aEntries->Elements());
}

nsresult
HashStore::Expire()
{
  ExpireEntries(&mAddPrefixes, mAddExpirations);
  ExpireEntries(&mAddCompletes, mAddExpirations);
  ExpireEntries(&mSubPrefixes, mSubExpirations);
  ExpireEntries(&mSubCompletes, mSubExpirations);

  mAddChunks.Remove(mAddExpirations);
  mSubChunks.Remove(mSubExpirations);

  mAddExpirations.Clear();
  mSubExpirations.Clear();

  return NS_OK;
}

template<class T>
nsresult DeflateWriteTArray(nsIOutputStream* aStream, nsTArray<T>& aIn)
{
  uLongf insize = aIn.Length() * sizeof(T);
  uLongf outsize = compressBound(insize);
  nsTArray<char> outBuff;
  outBuff.SetLength(outsize);

  int zerr = compress(reinterpret_cast<Bytef*>(outBuff.Elements()),
                      &outsize,
                      reinterpret_cast<const Bytef*>(aIn.Elements()),
                      insize);
  if (zerr != Z_OK) {
    return NS_ERROR_FAILURE;
  }
  LOG(("DeflateWriteTArray: %d in %d out", insize, outsize));

  outBuff.TruncateLength(outsize);

  // Length of compressed data stream
  PRUint32 dataLen = outBuff.Length();
  PRUint32 written;
  nsresult rv = aStream->Write(reinterpret_cast<char*>(&dataLen), sizeof(dataLen), &written);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(written == sizeof(dataLen), "Error writing deflate length");

  // Store to stream
  rv = WriteTArray(aStream, outBuff);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

template<class T>
nsresult InflateReadTArray(nsIInputStream* aStream, nsTArray<T>* aOut,
                           PRUint32 aExpectedSize)
{

  PRUint32 inLen;
  PRUint32 read;
  nsresult rv = aStream->Read(reinterpret_cast<char*>(&inLen), sizeof(inLen), &read);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(read == sizeof(inLen), "Error reading inflate length");

  nsTArray<char> inBuff;
  inBuff.SetLength(inLen);

  rv = ReadTArray(aStream, &inBuff, inLen);
  NS_ENSURE_SUCCESS(rv, rv);

  uLongf insize = inLen;
  uLongf outsize = aExpectedSize * sizeof(T);
  aOut->SetLength(aExpectedSize);

  int zerr = uncompress(reinterpret_cast<Bytef*>(aOut->Elements()),
                        &outsize,
                        reinterpret_cast<const Bytef*>(inBuff.Elements()),
                        insize);
  if (zerr != Z_OK) {
    return NS_ERROR_FAILURE;
  }
  LOG(("InflateReadTArray: %d in %d out", insize, outsize));

  NS_ASSERTION(outsize == aExpectedSize * sizeof(T), "Decompression size mismatch");

  return NS_OK;
}

static nsresult
ByteSliceWrite(nsIOutputStream* aOut, nsTArray<PRUint32>& aData)
{
  nsTArray<PRUint8> slice1;
  nsTArray<PRUint8> slice2;
  nsTArray<PRUint8> slice3;
  nsTArray<PRUint8> slice4;
  PRUint32 count = aData.Length();

  slice1.SetCapacity(count);
  slice2.SetCapacity(count);
  slice3.SetCapacity(count);
  slice4.SetCapacity(count);

  for (PRUint32 i = 0; i < count; i++) {
    slice1.AppendElement( aData[i] >> 24);
    slice2.AppendElement((aData[i] >> 16) & 0xFF);
    slice3.AppendElement((aData[i] >>  8) & 0xFF);
    slice4.AppendElement( aData[i]        & 0xFF);
  }

  nsresult rv = DeflateWriteTArray(aOut, slice1);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = DeflateWriteTArray(aOut, slice2);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = DeflateWriteTArray(aOut, slice3);
  NS_ENSURE_SUCCESS(rv, rv);
  // The LSB slice is generally uncompressible, don't bother
  // compressing it.
  rv = WriteTArray(aOut, slice4);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

static nsresult
ByteSliceRead(nsIInputStream* aInStream, nsTArray<PRUint32>* aData, PRUint32 count)
{
  nsTArray<PRUint8> slice1;
  nsTArray<PRUint8> slice2;
  nsTArray<PRUint8> slice3;
  nsTArray<PRUint8> slice4;

  nsresult rv = InflateReadTArray(aInStream, &slice1, count);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = InflateReadTArray(aInStream, &slice2, count);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = InflateReadTArray(aInStream, &slice3, count);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReadTArray(aInStream, &slice4, count);
  NS_ENSURE_SUCCESS(rv, rv);

  aData->SetCapacity(count);

  for (uint32 i = 0; i < count; i++) {
    aData->AppendElement((slice1[i] << 24) | (slice2[i] << 16)
                         | (slice3[i] << 8) | (slice4[i]));
  }

  return NS_OK;
}

nsresult
HashStore::ReadAddPrefixes()
{
  nsTArray<PRUint32> chunks;
  PRUint32 count = mHeader.numAddPrefixes;

  nsresult rv = ByteSliceRead(mInputStream, &chunks, count);
  NS_ENSURE_SUCCESS(rv, rv);

  mAddPrefixes.SetCapacity(count);
  for (PRUint32 i = 0; i < count; i++) {
    AddPrefix *add = mAddPrefixes.AppendElement();
    add->prefix.FromUint32(0);
    add->addChunk = chunks[i];
  }

  return NS_OK;
}

nsresult
HashStore::ReadSubPrefixes()
{
  nsTArray<PRUint32> addchunks;
  nsTArray<PRUint32> subchunks;
  nsTArray<PRUint32> prefixes;
  PRUint32 count = mHeader.numSubPrefixes;

  nsresult rv = ByteSliceRead(mInputStream, &addchunks, count);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ByteSliceRead(mInputStream, &subchunks, count);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ByteSliceRead(mInputStream, &prefixes, count);
  NS_ENSURE_SUCCESS(rv, rv);

  mSubPrefixes.SetCapacity(count);
  for (uint32 i = 0; i < count; i++) {
    SubPrefix *sub = mSubPrefixes.AppendElement();
    sub->addChunk = addchunks[i];
    sub->prefix.FromUint32(prefixes[i]);
    sub->subChunk = subchunks[i];
  }

  return NS_OK;
}

// Split up PrefixArray back into the constituents
nsresult
HashStore::WriteAddPrefixes(nsIOutputStream* aOut)
{
  nsTArray<PRUint32> chunks;
  PRUint32 count = mAddPrefixes.Length();
  chunks.SetCapacity(count);

  for (uint32 i = 0; i < count; i++) {
    chunks.AppendElement(mAddPrefixes[i].Chunk());
  }

  nsresult rv = ByteSliceWrite(aOut, chunks);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
HashStore::WriteSubPrefixes(nsIOutputStream* aOut)
{
  nsTArray<PRUint32> addchunks;
  nsTArray<PRUint32> subchunks;
  nsTArray<PRUint32> prefixes;
  PRUint32 count = mSubPrefixes.Length();
  addchunks.SetCapacity(count);
  subchunks.SetCapacity(count);
  prefixes.SetCapacity(count);

  for (uint32 i = 0; i < count; i++) {
    addchunks.AppendElement(mSubPrefixes[i].AddChunk());
    prefixes.AppendElement(mSubPrefixes[i].PrefixHash().ToUint32());
    subchunks.AppendElement(mSubPrefixes[i].Chunk());
  }

  nsresult rv = ByteSliceWrite(aOut, addchunks);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ByteSliceWrite(aOut, subchunks);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ByteSliceWrite(aOut, prefixes);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
HashStore::WriteFile()
{
  nsCOMPtr<nsIFile> storeFile;
  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(storeFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = storeFile->AppendNative(mTableName + NS_LITERAL_CSTRING(".sbstore"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Need to close the inputstream here *before* rewriting its file.
  // Windows will fail with an access violation if we don't.
  if (mInputStream) {
    rv = mInputStream->Close();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIOutputStream> out;
  rv = NS_NewCheckSummedOutputStream(getter_AddRefs(out), storeFile,
                                     PR_WRONLY | PR_TRUNCATE | PR_CREATE_FILE);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 written;
  rv = out->Write(reinterpret_cast<char*>(&mHeader), sizeof(mHeader), &written);
  NS_ENSURE_SUCCESS(rv, rv);

  // Write chunk numbers...
  rv = mAddChunks.Write(out);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mSubChunks.Write(out);
  NS_ENSURE_SUCCESS(rv, rv);

  // Write hashes..
  rv = WriteAddPrefixes(out);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = WriteSubPrefixes(out);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = WriteTArray(out, mAddCompletes);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = WriteTArray(out, mSubCompletes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISafeOutputStream> safeOut = do_QueryInterface(out, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = safeOut->Finish();
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 fileSize;
  rv = storeFile->GetFileSize(&fileSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // Reopen the file now that we've rewritten it.
  nsCOMPtr<nsIInputStream> origStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(origStream), storeFile,
                                  PR_RDONLY);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewBufferedInputStream(getter_AddRefs(mInputStream), origStream,
                                 fileSize);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
HashStore::FinishUpdate()
{
  // Drop add/sub data, it's only used during updates.
  mAddPrefixes.Clear();
  mSubPrefixes.Clear();
  mAddCompletes.Clear();
  mSubCompletes.Clear();

  return NS_OK;
}

template <class T>
static void
Erase(nsTArray<T>* array, T* iterStart, T* iterEnd)
{
  uint32 start = iterStart - array->Elements();
  uint32 count = iterEnd - iterStart;

  if (count > 0) {
    array->RemoveElementsAt(start, count);
  }
}

// Find items matching between |subs| and |adds|, and remove them,
// recording the item from |adds| in |adds_removed|.  To minimize
// copies, the inputs are processing in parallel, so |subs| and |adds|
// should be compatibly ordered (either by SBAddPrefixLess or
// SBAddPrefixHashLess).
//
// |predAS| provides add < sub, |predSA| provides sub < add, for the
// tightest compare appropriate (see calls in SBProcessSubs).
template<class TSub, class TAdd>
static void
KnockoutSubs(nsTArray<TSub>* aSubs, nsTArray<TAdd>* aAdds)
{
  // Keep a pair of output iterators for writing kept items.  Due to
  // deletions, these may lag the main iterators.  Using erase() on
  // individual items would result in O(N^2) copies.  Using a list
  // would work around that, at double or triple the memory cost.
  TAdd* addOut = aAdds->Elements();
  TAdd* addIter = aAdds->Elements();

  TSub* subOut = aSubs->Elements();
  TSub* subIter = aSubs->Elements();

  TAdd* addEnd = addIter + aAdds->Length();
  TSub* subEnd = subIter + aSubs->Length();

  while (addIter != addEnd && subIter != subEnd) {
    // additer compare, so it compares on add chunk
    int32 cmp = addIter->Compare(*subIter);
    if (cmp > 0) {
      // If |*sub_iter| < |*add_iter|, retain the sub.
      *subOut = *subIter;
      ++subOut;
      ++subIter;
    } else if (cmp < 0) {
      // If |*add_iter| < |*sub_iter|, retain the add.
      *addOut = *addIter;
      ++addOut;
      ++addIter;
    } else {
      // Drop equal items
      ++addIter;
      ++subIter;
    }
  }

  Erase(aAdds, addOut, addIter);
  Erase(aSubs, subOut, subIter);
}

// Remove items in |removes| from |fullHashes|.  |fullHashes| and
// |removes| should be ordered by SBAddPrefix component.
template <class T>
static void
RemoveMatchingPrefixes(const SubPrefixArray& aSubs, nsTArray<T>* aFullHashes)
{
  // Where to store kept items.
  T* out = aFullHashes->Elements();
  T* hashIter = out;
  T* hashEnd = aFullHashes->Elements() + aFullHashes->Length();

  SubPrefix const * removeIter = aSubs.Elements();
  SubPrefix const * removeEnd = aSubs.Elements() + aSubs.Length();

  while (hashIter != hashEnd && removeIter != removeEnd) {
    int32 cmp = removeIter->CompareAlt(*hashIter);
    if (cmp > 0) {
      // Keep items less than |*removeIter|.
      *out = *hashIter;
      ++out;
      ++hashIter;
    } else if (cmp < 0) {
      // No hit for |*removeIter|, bump it forward.
      ++removeIter;
    } else {
      // Drop equal items, there may be multiple hits.
      do {
        ++hashIter;
      } while (hashIter != hashEnd &&
               !(removeIter->CompareAlt(*hashIter) < 0));
      ++removeIter;
    }
  }
  Erase(aFullHashes, out, hashIter);
}

nsresult
HashStore::ProcessSubs()
{
  EntrySort(mAddPrefixes);
  EntrySort(mSubPrefixes);
  EntrySort(mAddCompletes);
  EntrySort(mSubCompletes);

  KnockoutSubs(&mSubPrefixes, &mAddPrefixes);

  RemoveMatchingPrefixes(mSubPrefixes, &mAddCompletes);
  RemoveMatchingPrefixes(mSubPrefixes, &mSubCompletes);

  KnockoutSubs(&mSubCompletes, &mAddCompletes);

  // Clean up temporary subs used for knocking out completes
  ChunkSet dummyChunks;
  dummyChunks.Set(0);
  ExpireEntries(&mSubPrefixes, dummyChunks);
  ExpireEntries(&mSubCompletes, dummyChunks);
  mSubChunks.Remove(dummyChunks);

  return NS_OK;
}

nsresult
HashStore::AugmentAdds(const nsTArray<PRUint32>& aPrefixes)
{
  uint32 cnt = aPrefixes.Length();
  if (cnt != mAddPrefixes.Length()) {
    LOG(("Amount of prefixes in cache not consistent with store (%d vs %d)",
         aPrefixes.Length(), mAddPrefixes.Length()));
    return NS_ERROR_FAILURE;
  }
  for (uint32 i = 0; i < cnt; i++) {
    mAddPrefixes[i].prefix.FromUint32(aPrefixes[i]);
  }
  return NS_OK;
}

}
}
