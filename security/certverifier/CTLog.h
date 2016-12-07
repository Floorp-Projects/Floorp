/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTLog_h
#define CTLog_h

#include <stdint.h>

namespace mozilla { namespace ct {

// Signed integer sufficient to store the numeric ID of CT log operators
// as assigned at https://www.certificate-transparency.org/known-logs .
// The assigned IDs are 0-based positive integers, so you can use special
// values (such as -1) to indicate a "null" or unknown log ID.
typedef int16_t CTLogOperatorId;

// Current status of a CT log in regard to its inclusion in the
// Known Logs List such as https://www.certificate-transparency.org/known-logs
enum class CTLogStatus
{
  // Status unknown or unavailable.
  Unknown,
  // Included in the list of known logs.
  Included,
  // Previously included, but disqualified at some point of time.
  Disqualified,
};

} } // namespace mozilla::ct

#endif // CTLog_h
