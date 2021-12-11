/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <string.h>
#include <random>
#include <tuple>

#include "asn1_mutators.h"

using namespace std;

static tuple<uint8_t *, size_t> ParseItem(uint8_t *Data, size_t MaxLength) {
  // Short form. Bit 8 has value "0" and bits 7-1 give the length.
  if ((Data[1] & 0x80) == 0) {
    size_t length = min(static_cast<size_t>(Data[1]), MaxLength - 2);
    return make_tuple(&Data[2], length);
  }

  // Constructed, indefinite length. Read until {0x00, 0x00}.
  if (Data[1] == 0x80) {
    void *offset = memmem(&Data[2], MaxLength - 2, "\0", 2);
    size_t length = offset ? (static_cast<uint8_t *>(offset) - &Data[2]) + 2
                           : MaxLength - 2;
    return make_tuple(&Data[2], length);
  }

  // Long form. Two to 127 octets. Bit 8 of first octet has value "1"
  // and bits 7-1 give the number of additional length octets.
  size_t octets = min(static_cast<size_t>(Data[1] & 0x7f), MaxLength - 2);

  // Handle lengths bigger than 32 bits.
  if (octets > 4) {
    // Ignore any further children, assign remaining length.
    return make_tuple(&Data[2] + octets, MaxLength - 2 - octets);
  }

  // Parse the length.
  size_t length = 0;
  for (size_t j = 0; j < octets; j++) {
    length = (length << 8) | Data[2 + j];
  }

  length = min(length, MaxLength - 2 - octets);
  return make_tuple(&Data[2] + octets, length);
}

static vector<uint8_t *> ParseItems(uint8_t *Data, size_t Size) {
  vector<uint8_t *> items;
  vector<size_t> lengths;

  // The first item is always the whole corpus.
  items.push_back(Data);
  lengths.push_back(Size);

  // Can't use iterators here because the `items` vector is modified inside the
  // loop. That's safe as long as we always check `items.size()` before every
  // iteration, and only call `.push_back()` to append new items we found.
  // Items are accessed through `items.at()`, we hold no references.
  for (size_t i = 0; i < items.size(); i++) {
    uint8_t *item = items.at(i);
    size_t remaining = lengths.at(i);

    // Empty or primitive items have no children.
    if (remaining == 0 || (0x20 & item[0]) == 0) {
      continue;
    }

    while (remaining > 2) {
      uint8_t *content;
      size_t length;

      tie(content, length) = ParseItem(item, remaining);

      if (length > 0) {
        // Record the item.
        items.push_back(content);

        // Record the length for further parsing.
        lengths.push_back(length);
      }

      // Reduce number of bytes left in current item.
      remaining -= length + (content - item);

      // Skip the item we just parsed.
      item = content + length;
    }
  }

  return items;
}

size_t ASN1MutatorFlipConstructed(uint8_t *Data, size_t Size, size_t MaxSize,
                                  unsigned int Seed) {
  auto items = ParseItems(Data, Size);

  std::mt19937 rng(Seed);
  std::uniform_int_distribution<size_t> dist(0, items.size() - 1);
  uint8_t *item = items.at(dist(rng));

  // Flip "constructed" type bit.
  item[0] ^= 0x20;

  return Size;
}

size_t ASN1MutatorChangeType(uint8_t *Data, size_t Size, size_t MaxSize,
                             unsigned int Seed) {
  auto items = ParseItems(Data, Size);

  std::mt19937 rng(Seed);
  std::uniform_int_distribution<size_t> dist(0, items.size() - 1);
  uint8_t *item = items.at(dist(rng));

  // Change type to a random int [0, 30].
  static std::uniform_int_distribution<size_t> tdist(0, 30);
  item[0] = tdist(rng);

  return Size;
}
