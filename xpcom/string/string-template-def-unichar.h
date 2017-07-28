/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "nsString.h"

#define CharT                               char16_t
#define CharT_is_PRUnichar                  1
#define nsTAString_IncompatibleCharT        nsACString
#define nsTString_CharT                     nsString
#define nsTStringRepr_CharT                 nsStringRepr
#define nsTFixedString_CharT                nsFixedString
#define nsTAutoString_CharT                 nsAutoString
#define nsTSubstring_CharT                  nsAString
#define PrintfAppend_CharT                  PrintfAppend_nsAString
#define nsTSubstringTuple_CharT             nsSubstringTuple
#define nsTStringComparator_CharT           nsStringComparator
#define nsTDefaultStringComparator_CharT    nsDefaultStringComparator
#define nsTDependentString_CharT            nsDependentString
#define nsTDependentSubstring_CharT         nsDependentSubstring
#define nsTLiteralString_CharT              nsLiteralString
#define nsTXPIDLString_CharT                nsXPIDLString
#define nsTGetterCopies_CharT               nsGetterCopies
#define nsTPromiseFlatString_CharT          nsPromiseFlatString
#define TPromiseFlatString_CharT            PromiseFlatString
#define nsTSubstringSplitter_CharT          nsSubstringSplitter
