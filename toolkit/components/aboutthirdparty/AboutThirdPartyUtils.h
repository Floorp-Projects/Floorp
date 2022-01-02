/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __AboutThirdPartyUtils_h__
#define __AboutThirdPartyUtils_h__

#include "nsString.h"

namespace mozilla {

// Define a custom case-insensitive string comparator wrapping
// nsCaseInsensitiveStringComparator to sort items alphabetically because
// nsCaseInsensitiveStringComparator sorts items by the length first.
int32_t CompareIgnoreCase(const nsAString& aStr1, const nsAString& aStr2);

// Mimicking the logic in msi!PackGUID to convert a GUID string to
// a packed GUID used as registry keys.
bool MsiPackGuid(const nsAString& aGuid, nsAString& aPacked);

// Mimicking the validation logic for a path in msi!_GetComponentPath
//
// Accecpted patterns and conversions:
//   C:\path  --> C:\path
//   C?\path  --> C:\path
//   C:\?path --> C:\path
//
// msi!_GetComponentPath also checks the existence by calling
// RegOpenKeyExW or GetFileAttributesExW, but we don't need it.
bool CorrectMsiComponentPath(nsAString& aPath);

}  // namespace mozilla

#endif  // __AboutThirdPartyUtils_h__
