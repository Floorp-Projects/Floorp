/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LoadTainting_h
#define mozilla_LoadTainting_h

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
//
// NOTE: Checking the tainting is not currently adequate.  You *must* still
//       check the final URL and CORS mode on the channel.
//
// These values are currently only set on the channel LoadInfo when the request
// was initiated through fetch() or when a service worker interception occurs.
// In the future we should set the tainting value within necko so that it is
// consistently applied.  Once that is done consumers can replace checks against
// the final URL and CORS mode with checks against tainting.
enum class LoadTainting : uint8_t
{
  Basic = 0,
  CORS = 1,
  Opaque = 2
};

} // namespace mozilla

#endif // mozilla_LoadTainting_h
