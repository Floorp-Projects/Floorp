/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "nsString.h"

#ifndef nsAString_h___
#define nsAString_h___

#ifndef nsStringFwd_h___
#include "nsStringFwd.h"
#endif

#ifndef nsStringIterator_h___
#include "nsStringIterator.h"
#endif

#include <string.h>
#include <stdarg.h>

#include "mozilla/fallible.h"

#define kNotFound -1

  // declare nsAString
#include "string-template-def-unichar.h"
#include "nsTSubstring.h"
#include "string-template-undef.h"

  // declare nsACString
#include "string-template-def-char.h"
#include "nsTSubstring.h"
#include "string-template-undef.h"


  /**
   * ASCII case-insensitive comparator.  (for Unicode case-insensitive
   * comparision, see nsUnicharUtils.h)
   */
class nsCaseInsensitiveCStringComparator
    : public nsCStringComparator
  {
    public:
      nsCaseInsensitiveCStringComparator() {}
      typedef char char_type;

      virtual int operator()( const char_type*, const char_type*, uint32_t, uint32_t ) const;
  };

class nsCaseInsensitiveCStringArrayComparator
  {
    public:
      template<class A, class B>
      bool Equals(const A& a, const B& b) const {
        return a.Equals(b, nsCaseInsensitiveCStringComparator());
      }
  };

  // included here for backwards compatibility
#ifndef nsSubstringTuple_h___
#include "nsSubstringTuple.h"
#endif

#endif // !defined(nsAString_h___)
