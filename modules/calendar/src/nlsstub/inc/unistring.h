/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef unistring_h__
#define unistring_h__

#include "nscore.h"
#include "nsString.h"
#include "nspr.h"
#include "limits.h"

#include "ptypes.h"

class Locale;

class NS_NLS UnicodeString 
{

public:
  UnicodeString();
  ~UnicodeString();

  UnicodeString(const UnicodeString& aUnicodeString);
  UnicodeString(const char * aString);

  PRUint32 size() { return mLength; }
  PRInt32  hashCode() const;

  TextOffset indexOf(const UnicodeString& aUnicodeString, TextOffset aFromOffset = 0, PRUint32 aForLength = -1) const;
  TextOffset indexOf(PRUnichar aUnichar, TextOffset aFromOffset = 0, PRUint32 aForLength = -1) const;

  UnicodeString& extractBetween(TextOffset aStart, TextOffset aLimit, UnicodeString& aExtractInto) const;

  PRInt32 compareIgnoreCase(const UnicodeString& aUnicodeString) const;
  PRInt32 compareIgnoreCase(const PRUnichar* aUnichar, PRInt32 aLength) const;
  PRInt32 compareIgnoreCase(const PRUnichar* aUnichar) const;
  PRInt32 compareIgnoreCase(const char*	aChar, const char* aEncoding) const;
  PRInt32 compareIgnoreCase(const char*	aChar) const;
  UnicodeString& toUpper();
  UnicodeString& toUpper(const Locale& aLocale);

  char* toCString(const char* aEncoding) const;

  UnicodeString& trim(UnicodeString& aUnicodeString) const;
  void trim();
  UnicodeString& remove();
  UnicodeString& remove(TextOffset aOffset,PRInt32 aLength = LONG_MAX);
  UnicodeString& insert(TextOffset aThisOffset, const UnicodeString& aUnicodeString);
  PRBool startsWith(const UnicodeString& aUnicodeString) const;
  PRBool endsWith(const UnicodeString& aUnicodeString) const;
  UnicodeString& removeBetween(TextOffset aStart = 0, TextOffset aLimit = LONG_MAX);


  PRInt8 compare(const UnicodeString& aUnicodeString) const;
  PRInt8 compare(TextOffset aOffset, PRInt32 aThisLength, const UnicodeString& aUnicodeString, TextOffset aStringOffset, PRInt32 aLength) const;
  PRInt8 compare(const PRUnichar* aUnichar) const;
  PRInt8 compare(const PRUnichar* aUnichar, PRInt32 aLength) const;
  PRInt8 compare(const char* aChar) const;

  UnicodeString& extract(TextOffset aOffset,PRInt32 aLength, UnicodeString& aExtractInto) const;
  void extract(TextOffset aOffset, PRInt32 aLength, PRUnichar*aExtractInto) const;
  void extract(TextOffset aOffset, PRInt32 aLength, char* aExtractInto) const;


public:
  PRUnichar		  operator[](TextOffset	aOffset) const;
  PRUnichar&	  operator[](TextOffset	aOffset);
  UnicodeString&  operator+=(const UnicodeString& aUnicodeString);
  UnicodeString&  operator+=(PRUnichar aUnichar);
  PRBool          operator==(const UnicodeString& aUnicodeString) const;
  PRBool          operator!=(const UnicodeString& aUnicodeString) const;
  UnicodeString&  operator=(const UnicodeString& aUnicodeString);

public:
  PRUint32 mLength;

private:
  nsString mString;
};

#endif
