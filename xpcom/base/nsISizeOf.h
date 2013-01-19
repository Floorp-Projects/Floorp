/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsISizeOf_h___
#define nsISizeOf_h___

#include "nsISupports.h"

#define NS_ISIZEOF_IID \
  {0x61d05579, 0xd7ec, 0x485c, \
    { 0xa4, 0x0c, 0x31, 0xc7, 0x9a, 0x5c, 0xf9, 0xf3 }}

class nsISizeOf : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISIZEOF_IID)

  /**
   * Measures the size of the things pointed to by the object.
   */
  virtual size_t SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const = 0;

  /**
   * Like SizeOfExcludingThis, but also includes the size of the object itself.
   */
  virtual size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsISizeOf, NS_ISIZEOF_IID)

#endif /* nsISizeOf_h___ */
