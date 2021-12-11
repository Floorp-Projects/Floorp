/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IDNBlocklistUtils_h__
#define IDNBlocklistUtils_h__

#include <utility>
#include "nsTArray.h"

namespace mozilla {
namespace net {

// A blocklist range is defined as all of the characters between:
// { firstCharacterInRange, lastCharacterInRange }
using BlocklistRange = std::pair<char16_t, char16_t>;

// Used to perform a binary search of the needle in the sorted array of pairs
class BlocklistPairToCharComparator {
 public:
  bool Equals(const BlocklistRange& pair, char16_t needle) const {
    // If the needle is between pair.first and pair.second it
    // is part of the range.
    return pair.first <= needle && needle <= pair.second;
  }

  bool LessThan(const BlocklistRange& pair, char16_t needle) const {
    // The needle has to be larger than the second value,
    // otherwise it may be equal.
    return pair.second < needle;
  }
};

// Used to sort the array of pairs
class BlocklistEntryComparator {
 public:
  bool Equals(const BlocklistRange& a, const BlocklistRange& b) const {
    return a.first == b.first && a.second == b.second;
  }

  bool LessThan(const BlocklistRange& a, const BlocklistRange& b) const {
    return a.first < b.first;
  }
};

// Returns true if the char can be found in the blocklist
inline bool CharInBlocklist(char16_t aChar,
                            const nsTArray<BlocklistRange>& aBlocklist) {
  return aBlocklist.ContainsSorted(aChar, BlocklistPairToCharComparator());
}

// Initializes the blocklist based on the statically defined list and the
// values of the following preferences:
//     - network.IDN.extra_allowed_chars
//     - network.IDN.extra_blocked_chars
void InitializeBlocklist(nsTArray<BlocklistRange>& aBlocklist);

void RemoveCharFromBlocklist(char16_t aChar,
                             nsTArray<BlocklistRange>& aBlocklist);

}  // namespace net
}  // namespace mozilla

#endif  // IDNBlocklistUtils_h__
