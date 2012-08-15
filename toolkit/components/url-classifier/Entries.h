/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Url Classifier code
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
 *   Gian-Carlo Pascutto <gpascutto@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef SBEntries_h__
#define SBEntries_h__

#include "nsTArray.h"
#include "nsString.h"
#include "nsICryptoHash.h"
#include "nsNetUtil.h"
#include "prlog.h"

extern PRLogModuleInfo *gUrlClassifierDbServiceLog;
#if defined(PR_LOGGING)
#define LOG(args) PR_LOG(gUrlClassifierDbServiceLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gUrlClassifierDbServiceLog, 4)
#else
#define LOG(args)
#define LOG_ENABLED() (false)
#endif

#if DEBUG
#include "plbase64.h"
#endif

namespace mozilla {
namespace safebrowsing {

#define PREFIX_SIZE   4
#define COMPLETE_SIZE 32

template <uint32 S, class Comparator>
struct SafebrowsingHash
{
  static const uint32 sHashSize = S;
  typedef SafebrowsingHash<S, Comparator> self_type;
  uint8 buf[S];

  nsresult FromPlaintext(const nsACString& aPlainText, nsICryptoHash* aHash) {
    // From the protocol doc:
    // Each entry in the chunk is composed
    // of the SHA 256 hash of a suffix/prefix expression.

    nsresult rv = aHash->Init(nsICryptoHash::SHA256);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aHash->Update
      (reinterpret_cast<const uint8*>(aPlainText.BeginReading()),
       aPlainText.Length());
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString hashed;
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
    uint32 len = ((sHashSize + 2) / 3) * 4;
    aStr.SetCapacity(len + 1);
    PL_Base64Encode((char*)buf, sHashSize, aStr.BeginWriting());
    aStr.BeginWriting()[len] = '\0';
  }
#endif
  PRUint32 ToUint32() const {
    PRUint32 res = 0;
    memcpy(&res, buf, NS_MIN<size_t>(4, S));
    return res;
  }
  void FromUint32(PRUint32 aHash) {
    memcpy(buf, &aHash, NS_MIN<size_t>(4, S));
  }
};

class PrefixComparator {
public:
  static int Compare(const PRUint8* a, const PRUint8* b) {
    return *((uint32*)a) - *((uint32*)b);
  }
};
typedef SafebrowsingHash<PREFIX_SIZE, PrefixComparator> Prefix;
typedef nsTArray<Prefix> PrefixArray;

class CompletionComparator {
public:
  static int Compare(const PRUint8* a, const PRUint8* b) {
    return memcmp(a, b, COMPLETE_SIZE);
  }
};
typedef SafebrowsingHash<COMPLETE_SIZE, CompletionComparator> Completion;
typedef nsTArray<Completion> CompletionArray;

struct AddPrefix {
  Prefix prefix;
  uint32 addChunk;

  AddPrefix() : addChunk(0) {}

  uint32 Chunk() const { return addChunk; }
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
  union {
    Prefix prefix;
    Completion complete;
  } hash;
  uint32 addChunk;

  AddComplete() : addChunk(0) {}

  uint32 Chunk() const { return addChunk; }
  const Prefix &PrefixHash() const { return hash.prefix; }
  const Completion &CompleteHash() const { return hash.complete; }

  template<class T>
  int Compare(const T& other) const {
    int cmp = hash.complete.Compare(other.CompleteHash());
    if (cmp != 0) {
      return cmp;
    }
    return addChunk - other.addChunk;
  }
};

struct SubPrefix {
  Prefix prefix;
  uint32 addChunk;
  uint32 subChunk;

  SubPrefix(): addChunk(0), subChunk(0) {}

  uint32 Chunk() const { return subChunk; }
  uint32 AddChunk() const { return addChunk; }
  const Prefix &PrefixHash() const { return prefix; }

  template<class T>
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
    int cmp = prefix.Compare(aOther.PrefixHash());
    if (cmp != 0)
      return cmp;
    return addChunk - aOther.addChunk;
  }
};

struct SubComplete {
  union {
    Prefix prefix;
    Completion complete;
  } hash;
  uint32 addChunk;
  uint32 subChunk;

  SubComplete() : addChunk(0), subChunk(0) {}

  uint32 Chunk() const { return subChunk; }
  uint32 AddChunk() const { return addChunk; }
  const Prefix &PrefixHash() const { return hash.prefix; }
  const Completion &CompleteHash() const { return hash.complete; }

  int Compare(const SubComplete& aOther) const {
    int cmp = hash.complete.Compare(aOther.hash.complete);
    if (cmp != 0)
      return cmp;
    if (addChunk != aOther.addChunk)
      return addChunk - aOther.addChunk;
    return subChunk - aOther.subChunk;
  }
};

typedef nsTArray<AddPrefix>   AddPrefixArray;
typedef nsTArray<AddComplete> AddCompleteArray;
typedef nsTArray<SubPrefix>   SubPrefixArray;
typedef nsTArray<SubComplete> SubCompleteArray;

/**
 * Compares chunks by their add chunk, then their prefix.
 */
template<class T>
class EntryCompare {
public:
  typedef T elem_type;
  static int Compare(const void* e1, const void* e2, void* data) {
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
template<class T>
void
EntrySort(nsTArray<T>& aArray)
{
  NS_QuickSort(aArray.Elements(), aArray.Length(), sizeof(T),
               EntryCompare<T>::Compare, 0);
}

template<class T>
nsresult
ReadTArray(nsIInputStream* aStream, nsTArray<T>* aArray, PRUint32 aNumElements)
{
  if (!aArray->SetLength(aNumElements))
    return NS_ERROR_OUT_OF_MEMORY;

  void *buffer = aArray->Elements();
  nsresult rv = NS_ReadInputStreamToBuffer(aStream, &buffer,
                                           (aNumElements * sizeof(T)));
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

template<class T>
nsresult
WriteTArray(nsIOutputStream* aStream, nsTArray<T>& aArray)
{
  PRUint32 written;
  return aStream->Write(reinterpret_cast<char*>(aArray.Elements()),
                        aArray.Length() * sizeof(T),
                        &written);
}

}
}
#endif
