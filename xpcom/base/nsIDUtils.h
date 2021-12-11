/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCOM_BASE_NSIDUTILS_H_
#define XPCOM_BASE_NSIDUTILS_H_

#include "nsID.h"
#include "nsString.h"
#include "nsTString.h"

/**
 * Trims the brackets around the nsID string, since it wraps its ID with
 * brackets: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}.
 */
template <typename T>
class NSID_TrimBrackets : public nsTAutoString<T> {
 public:
  explicit NSID_TrimBrackets(const nsID& aID) {
    nsIDToCString toCString(aID);
    // The string from nsIDToCString::get() includes a null terminator.
    // Thus this trims 3 characters: `{`, `}`, and the null terminator.
    this->AssignASCII(toCString.get() + 1, NSID_LENGTH - 3);
  }
};

using NSID_TrimBracketsUTF16 = NSID_TrimBrackets<char16_t>;
using NSID_TrimBracketsASCII = NSID_TrimBrackets<char>;

#endif  // XPCOM_BASE_NSIDUTILS_H_
