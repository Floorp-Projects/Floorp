/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ctype.h>
#include "nsAString.h"
#include "plstr.h"


  // define nsStringComparator
#include "string-template-def-unichar.h"
#include "nsTStringComparator.cpp"
#include "string-template-undef.h"

  // define nsCStringComparator
#include "string-template-def-char.h"
#include "nsTStringComparator.cpp"
#include "string-template-undef.h"


int
nsCaseInsensitiveCStringComparator::operator()( const char_type* lhs,
                                                const char_type* rhs,
                                                uint32_t lLength,
                                                uint32_t rLength ) const
  {
    if (lLength != rLength)
      return (lLength > rLength) ? 1 : -1;
    int32_t result=int32_t(PL_strncasecmp(lhs, rhs, lLength));
    //Egads. PL_strncasecmp is returning *very* negative numbers.
    //Some folks expect -1,0,1, so let's temper its enthusiasm.
    if (result<0) 
      result=-1;
    return result;
  }
