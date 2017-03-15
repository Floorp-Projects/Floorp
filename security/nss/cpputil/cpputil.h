/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef cpputil_h__
#define cpputil_h__

static unsigned char* toUcharPtr(const uint8_t* v) {
  return const_cast<unsigned char*>(static_cast<const unsigned char*>(v));
}

#endif  // cpputil_h__
