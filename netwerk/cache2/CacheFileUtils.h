/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileUtils__h__
#define CacheFileUtils__h__

#include "nsError.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"

class nsILoadContextInfo;
class nsACString;

namespace mozilla {
namespace net {
namespace CacheFileUtils {

already_AddRefed<nsILoadContextInfo>
ParseKey(const nsCSubstring &aKey,
         nsCSubstring *aIdEnhance = nullptr,
         nsCSubstring *aURISpec = nullptr);

void
AppendKeyPrefix(nsILoadContextInfo *aInfo, nsACString &_retval);

void
AppendTagWithValue(nsACString & aTarget, char const aTag, nsCSubstring const & aValue);

nsresult
KeyMatchesLoadContextInfo(const nsACString &aKey,
                          nsILoadContextInfo *aInfo,
                          bool *_retval);

class ValidityPair {
public:
  ValidityPair(uint32_t aOffset, uint32_t aLen);

  ValidityPair& operator=(const ValidityPair& aOther);

  // Returns true when two pairs can be merged, i.e. they do overlap or the one
  // ends exactly where the other begins.
  bool CanBeMerged(const ValidityPair& aOther) const;

  // Returns true when aOffset is placed anywhere in the validity interval or
  // exactly after its end.
  bool IsInOrFollows(uint32_t aOffset) const;

  // Returns true when this pair has lower offset than the other pair. In case
  // both pairs have the same offset it returns true when this pair has a
  // shorter length.
  bool LessThan(const ValidityPair& aOther) const;

  // Merges two pair into one.
  void Merge(const ValidityPair& aOther);

  uint32_t Offset() const { return mOffset; }
  uint32_t Len() const    { return mLen; }

private:
  uint32_t mOffset;
  uint32_t mLen;
};

class ValidityMap {
public:
  // Prints pairs in the map into log.
  void Log() const;

  // Returns number of pairs in the map.
  uint32_t Length() const;

  // Adds a new pair to the map. It keeps the pairs ordered and merges pairs
  // when possible.
  void AddPair(uint32_t aOffset, uint32_t aLen);

  // Removes all pairs from the map.
  void Clear();

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  ValidityPair& operator[](uint32_t aIdx);

private:
  nsTArray<ValidityPair> mMap;
};

} // CacheFileUtils
} // net
} // mozilla

#endif
