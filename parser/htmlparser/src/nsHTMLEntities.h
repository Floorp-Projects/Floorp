/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsHTMLEntities_h___
#define nsHTMLEntities_h___

#include "nsString.h"

class nsHTMLEntities {
public:

  static nsresult AddRefTable(void);
  static void ReleaseTable(void);

/**
 * Translate an entity string into it's unicode value. This call
 * returns -1 if the entity cannot be mapped. Note that the string
 * passed in must NOT have the leading "&" nor the trailing ";"
 * in it.
 */
  static int32_t EntityToUnicode(const nsAString& aEntity);
  static int32_t EntityToUnicode(const nsCString& aEntity);

/**
 * Translate a unicode value into an entity string. This call
 * returns null if the entity cannot be mapped. 
 * Note that the string returned DOES NOT have the leading "&" nor 
 * the trailing ";" in it.
 */
  static const char* UnicodeToEntity(int32_t aUnicode);
};


#endif /* nsHTMLEntities_h___ */
