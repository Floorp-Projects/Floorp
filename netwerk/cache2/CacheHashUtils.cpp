/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheHashUtils.h"

#include "plstr.h"

namespace mozilla {
namespace net {

/**
 *  CacheHashUtils::Hash(const char * key, uint32_t initval)
 *
 *  See http://burtleburtle.net/bob/hash/evahash.html for more information
 *  about this hash function.
 *
 *  This algorithm is used to check the data integrity.
 */

static inline void hashmix(uint32_t& a, uint32_t& b, uint32_t& c)
{
  a -= b; a -= c; a ^= (c>>13);
  b -= c; b -= a; b ^= (a<<8);
  c -= a; c -= b; c ^= (b>>13);
  a -= b; a -= c; a ^= (c>>12);
  b -= c; b -= a; b ^= (a<<16);
  c -= a; c -= b; c ^= (b>>5);
  a -= b; a -= c; a ^= (c>>3);
  b -= c; b -= a; b ^= (a<<10);
  c -= a; c -= b; c ^= (b>>15);
}

CacheHashUtils::Hash32_t
CacheHashUtils::Hash(const char *aData, uint32_t aSize, uint32_t aInitval)
{
  const uint8_t *k = reinterpret_cast<const uint8_t*>(aData);
  uint32_t a, b, c, len/*, length*/;

//  length = PL_strlen(key);
  /* Set up the internal state */
  len = aSize;
  a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
  c = aInitval;        /* variable initialization of internal state */

  /*---------------------------------------- handle most of the key */
  while (len >= 12)
  {
    a += k[0] + (uint32_t(k[1])<<8) + (uint32_t(k[2])<<16) + (uint32_t(k[3])<<24);
    b += k[4] + (uint32_t(k[5])<<8) + (uint32_t(k[6])<<16) + (uint32_t(k[7])<<24);
    c += k[8] + (uint32_t(k[9])<<8) + (uint32_t(k[10])<<16) + (uint32_t(k[11])<<24);
    hashmix(a, b, c);
    k += 12; len -= 12;
  }

  /*------------------------------------- handle the last 11 bytes */
  c += aSize;
  switch(len) {              /* all the case statements fall through */
    case 11: c += (uint32_t(k[10])<<24);
    case 10: c += (uint32_t(k[9])<<16);
    case 9 : c += (uint32_t(k[8])<<8);
    /* the low-order byte of c is reserved for the length */
    case 8 : b += (uint32_t(k[7])<<24);
    case 7 : b += (uint32_t(k[6])<<16);
    case 6 : b += (uint32_t(k[5])<<8);
    case 5 : b += k[4];
    case 4 : a += (uint32_t(k[3])<<24);
    case 3 : a += (uint32_t(k[2])<<16);
    case 2 : a += (uint32_t(k[1])<<8);
    case 1 : a += k[0];
    /* case 0: nothing left to add */
  }
  hashmix(a, b, c);

  return c;
}

CacheHashUtils::Hash16_t
CacheHashUtils::Hash16(const char *aData, uint32_t aSize, uint32_t aInitval)
{
  Hash32_t hash = Hash(aData, aSize, aInitval);
  return (hash & 0xFFFF);
}

} // net
} // mozilla

