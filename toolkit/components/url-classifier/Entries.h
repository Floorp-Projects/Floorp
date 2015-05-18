//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This header file defines the storage types of the actual safebrowsing
// chunk data, which may be either 32-bit hashes or complete 256-bit hashes.
// Chunk numbers are represented in ChunkSet.h.

#ifndef SBEntries_h__
#define SBEntries_h__

#include "nsTArray.h"
#include "nsString.h"
#include "nsICryptoHash.h"
#include "nsNetUtil.h"

#if DEBUG
#include "plbase64.h"
#endif

namespace mozilla {
namespace safebrowsing {

#define PREFIX_SIZE   4
#define COMPLETE_SIZE 32

// This is the struct that contains 4-byte hash prefixes.
template <uint32_t S, class Comparator>
struct SafebrowsingHash
{
  static const uint32_t sHashSize = S;
  typedef SafebrowsingHash<S, Comparator> self_type;
  uint8_t buf[S];

  nsresult FromPlaintext(const nsACString& aPlainText, nsICryptoHash* aHash) {
    // From the protocol doc:
    // Each entry in the chunk is composed
    // of the SHA 256 hash of a suffix/prefix expression.

    nsresult rv = aHash->Init(nsICryptoHash::SHA256);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aHash->Update
      (reinterpret_cast<const uint8_t*>(aPlainText.BeginReading()),
       aPlainText.Length());
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString hashed;
    rv = aHash->Finish(false, hashed);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(hashed.Length() >= sHashSize,
                 "not enough characters in the hash");

    memcpy(buf, hashed.BeginReading(), sHashSize);

    return NS_OK;
  }

  void Assign(const nsACString& aStr) {
    NS_ASSERTION(aStr.Length() >= sHashSize,
                 "string must be at least sHashSize characters long");
    memcpy(buf, aStr.BeginReading(), sHashSize);
  }

  int Compare(const self_type& aOther) const {
    return Comparator::Compare(buf, aOther.buf);
  }

  bool operator==(const self_type& aOther) const {
    return Comparator::Compare(buf, aOther.buf) == 0;
  }

  bool operator!=(const self_type& aOther) const {
    return Comparator::Compare(buf, aOther.buf) != 0;
  }

  bool operator<(const self_type& aOther) const {
    return Comparator::Compare(buf, aOther.buf) < 0;
  }

#ifdef DEBUG
  void ToString(nsACString& aStr) const {
    uint32_t len = ((sHashSize + 2) / 3) * 4;
    aStr.SetCapacity(len + 1);
    PL_Base64Encode((char*)buf, sHashSize, aStr.BeginWriting());
    aStr.BeginWriting()[len] = '\0';
  }

  void ToHexString(nsACString& aStr) const {
    static const char* const lut = "0123456789ABCDEF";
    // 32 bytes is the longest hash
    size_t len = 32;

    aStr.SetCapacity(2 * len);
    for (size_t i = 0; i < len; ++i) {
      const char c = static_cast<const char>(buf[i]);
      aStr.Append(lut[(c >> 4) & 0x0F]);
      aStr.Append(lut[c & 15]);
    }
  }
#endif
  uint32_t ToUint32() const {
      return *((uint32_t*)buf);
  }
  void FromUint32(uint32_t aHash) {
      *((uint32_t*)buf) = aHash;
  }
};

class PrefixComparator {
public:
  static int Compare(const uint8_t* a, const uint8_t* b) {
      uint32_t first = *((uint32_t*)a);
      uint32_t second = *((uint32_t*)b);
      if (first > second) {
          return 1;
      } else if (first == second) {
          return 0;
      } else {
          return -1;
      }
  }
};
// Use this for 4-byte hashes
typedef SafebrowsingHash<PREFIX_SIZE, PrefixComparator> Prefix;
typedef nsTArray<Prefix> PrefixArray;

class CompletionComparator {
public:
  static int Compare(const uint8_t* a, const uint8_t* b) {
    return memcmp(a, b, COMPLETE_SIZE);
  }
};
// Use this for 32-byte hashes
typedef SafebrowsingHash<COMPLETE_SIZE, CompletionComparator> Completion;
typedef nsTArray<Completion> CompletionArray;

struct AddPrefix {
  // The truncated hash.
  Prefix prefix;
  // The chunk number to which it belongs.
  uint32_t addChunk;

  AddPrefix() : addChunk(0) {}

  // Returns the chunk number.
  uint32_t Chunk() const { return addChunk; }
  const Prefix &PrefixHash() const { return prefix; }

  template<class T>
  int Compare(const T& other) const {
    int cmp = prefix.Compare(other.PrefixHash());
    if (cmp != 0) {
      return cmp;
    }
    return addChunk - other.addChunk;
  }
};

struct AddComplete {
  Completion complete;
  uint32_t addChunk;

  AddComplete() : addChunk(0) {}

  uint32_t Chunk() const { return addChunk; }
  // The 4-byte prefix of the sha256 hash.
  uint32_t ToUint32() const { return complete.ToUint32(); }
  // The 32-byte sha256 hash.
  const Completion &CompleteHash() const { return complete; }

  template<class T>
  int Compare(const T& other) const {
    int cmp = complete.Compare(other.CompleteHash());
    if (cmp != 0) {
      return cmp;
    }
    return addChunk - other.addChunk;
  }
};

struct SubPrefix {
  // The hash to subtract.
  Prefix prefix;
  // The chunk number of the add chunk to which the hash belonged.
  uint32_t addChunk;
  // The chunk number of this sub chunk.
  uint32_t subChunk;

  SubPrefix(): addChunk(0), subChunk(0) {}

  uint32_t Chunk() const { return subChunk; }
  uint32_t AddChunk() const { return addChunk; }
  const Prefix &PrefixHash() const { return prefix; }

  template<class T>
  // Returns 0 if and only if the chunks are the same in every way.
  int Compare(const T& aOther) const {
    int cmp = prefix.Compare(aOther.PrefixHash());
    if (cmp != 0)
      return cmp;
    if (addChunk != aOther.addChunk)
      return addChunk - aOther.addChunk;
    return subChunk - aOther.subChunk;
  }

  template<class T>
  int CompareAlt(const T& aOther) const {
    Prefix other;
    other.FromUint32(aOther.ToUint32());
    int cmp = prefix.Compare(other);
    if (cmp != 0)
      return cmp;
    return addChunk - aOther.addChunk;
  }
};

struct SubComplete {
  Completion complete;
  uint32_t addChunk;
  uint32_t subChunk;

  SubComplete() : addChunk(0), subChunk(0) {}

  uint32_t Chunk() const { return subChunk; }
  uint32_t AddChunk() const { return addChunk; }
  const Completion &CompleteHash() const { return complete; }
  // The 4-byte prefix of the sha256 hash.
  uint32_t ToUint32() const { return complete.ToUint32(); }

  int Compare(const SubComplete& aOther) const {
    int cmp = complete.Compare(aOther.complete);
    if (cmp != 0)
      return cmp;
    if (addChunk != aOther.addChunk)
      return addChunk - aOther.addChunk;
    return subChunk - aOther.subChunk;
  }
};

typedef FallibleTArray<AddPrefix>   AddPrefixArray;
typedef FallibleTArray<AddComplete> AddCompleteArray;
typedef FallibleTArray<SubPrefix>   SubPrefixArray;
typedef FallibleTArray<SubComplete> SubCompleteArray;

/**
 * Compares chunks by their add chunk, then their prefix.
 */
template<class T>
class EntryCompare {
public:
  typedef T elem_type;
  static int Compare(const void* e1, const void* e2) {
    const elem_type* a = static_cast<const elem_type*>(e1);
    const elem_type* b = static_cast<const elem_type*>(e2);
    return a->Compare(*b);
  }
};

/**
 * Sort an array of store entries.  nsTArray::Sort uses Equal/LessThan
 * to sort, this does a single Compare so it's a bit quicker over the
 * large sorts we do.
 */
template<class T, class Alloc>
void
EntrySort(nsTArray_Impl<T, Alloc>& aArray)
{
  qsort(aArray.Elements(), aArray.Length(), sizeof(T),
        EntryCompare<T>::Compare);
}

template<class T, class Alloc>
nsresult
ReadTArray(nsIInputStream* aStream, nsTArray_Impl<T, Alloc>* aArray, uint32_t aNumElements)
{
  aArray->SetLength(aNumElements);

  void *buffer = aArray->Elements();
  nsresult rv = NS_ReadInputStreamToBuffer(aStream, &buffer,
                                           (aNumElements * sizeof(T)));
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

template<class T>
nsresult
ReadTArray(nsIInputStream* aStream, FallibleTArray<T>* aArray, uint32_t aNumElements)
{
  if (!aArray->SetLength(aNumElements, fallible))
    return NS_ERROR_OUT_OF_MEMORY;

  void *buffer = aArray->Elements();
  nsresult rv = NS_ReadInputStreamToBuffer(aStream, &buffer,
                                           (aNumElements * sizeof(T)));
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

template<class T, class Alloc>
nsresult
WriteTArray(nsIOutputStream* aStream, nsTArray_Impl<T, Alloc>& aArray)
{
  uint32_t written;
  return aStream->Write(reinterpret_cast<char*>(aArray.Elements()),
                        aArray.Length() * sizeof(T),
                        &written);
}

} // namespace safebrowsing
} // namespace mozilla
#endif // SBEntries_h__
