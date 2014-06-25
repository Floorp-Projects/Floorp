/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* nsStringFwd.h --- forward declarations for string classes */

#ifndef nsStringFwd_h___
#define nsStringFwd_h___

#include "nscore.h"

#ifndef MOZILLA_INTERNAL_API
#error Internal string headers are not available from external-linkage code.
#endif

/**
 * double-byte (char16_t) string types
 */

class nsAString;
class nsSubstringTuple;
class nsString;
class nsAutoString;
class nsDependentString;
class nsDependentSubstring;
class nsPromiseFlatString;
class nsStringComparator;
class nsDefaultStringComparator;
class nsXPIDLString;


/**
 * single-byte (char) string types
 */

class nsACString;
class nsCSubstringTuple;
class nsCString;
class nsAutoCString;
class nsDependentCString;
class nsDependentCSubstring;
class nsPromiseFlatCString;
class nsCStringComparator;
class nsDefaultCStringComparator;
class nsXPIDLCString;


/**
 * typedefs for backwards compatibility
 */

typedef nsAString             nsSubstring;
typedef nsACString            nsCSubstring;

typedef nsString              nsAFlatString;
typedef nsSubstring           nsASingleFragmentString;

typedef nsCString             nsAFlatCString;
typedef nsCSubstring          nsASingleFragmentCString;


#endif /* !defined(nsStringFwd_h___) */
