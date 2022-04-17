/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LoadTainting_h
#define mozilla_LoadTainting_h

#include <stdint.h>

namespace mozilla {

// Define an enumeration to reflect the concept of response tainting from the
// the fetch spec:
//
//  https://fetch.spec.whatwg.org/#concept-request-response-tainting
//
// Roughly the tainting means:
//
//  * Basic: the request resulted in a same-origin or non-http load
//  * CORS: the request resulted in a cross-origin load with CORS headers
//  * Opaque: the request resulted in a cross-origin load without CORS headers
//
// The enumeration is purposefully designed such that more restrictive tainting
// corresponds to a higher integral value.
enum class LoadTainting : uint8_t { Basic = 0, CORS = 1, Opaque = 2 };

}  // namespace mozilla

#endif  // mozilla_LoadTainting_h
