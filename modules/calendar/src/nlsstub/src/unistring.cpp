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

#include "unistring.h"


UnicodeString::UnicodeString()
{
}

UnicodeString::~UnicodeString()
{
}

UnicodeString::UnicodeString(const UnicodeString& aUnicodeString)
{
}

UnicodeString::UnicodeString(const char * aString)
{
}

PRInt32  UnicodeString::hashCode() const
{
  return 0;
}

TextOffset UnicodeString::indexOf(const UnicodeString& aUnicodeString, TextOffset aFromOffset, PRUint32 aForLength) const
{
  return 0;
}

TextOffset UnicodeString::indexOf(PRUnichar aUnichar, TextOffset aFromOffset, PRUint32 aForLength) const
{
  return 0;
}


UnicodeString& UnicodeString::extractBetween(TextOffset aStart, TextOffset aLimit, UnicodeString& aExtractInto) const
{
  UnicodeString u;
  return (u);
}


PRInt32 UnicodeString::compareIgnoreCase(const UnicodeString& aUnicodeString) const
{
  return 0;
}

PRInt32 UnicodeString::compareIgnoreCase(const PRUnichar* aUnichar, PRInt32 aLength) const
{
  return 0;
}

PRInt32 UnicodeString::compareIgnoreCase(const PRUnichar* aUnichar) const
{
  return 0;
}

PRInt32 UnicodeString::compareIgnoreCase(const char*	aChar, const char* aEncoding) const
{
  return 0;
}

PRInt32 UnicodeString::compareIgnoreCase(const char*	aChar) const
{
  return 0;
}

UnicodeString& UnicodeString::toUpper()
{
  UnicodeString u;
  return (u);
}

UnicodeString& UnicodeString::toUpper(const Locale& aLocale)
{
  UnicodeString u;
  return (u);
}

char* UnicodeString::toCString(const char* aEncoding) const
{
  return ((char *)nsnull);
}

UnicodeString& UnicodeString::trim(UnicodeString& aUnicodeString) const
{
  UnicodeString u;
  return (u);
}

void UnicodeString::trim()
{
  return;
}

UnicodeString& UnicodeString::remove()
{
  UnicodeString u;
  return (u);
}

UnicodeString& UnicodeString::remove(TextOffset aOffset,PRInt32 aLength)
{
  UnicodeString u;
  return (u);
}

UnicodeString& UnicodeString::insert(TextOffset aThisOffset, const UnicodeString& aUnicodeString)
{
  UnicodeString u;
  return (u);
}

PRBool UnicodeString::startsWith(const UnicodeString& aUnicodeString) const
{
  return (PR_TRUE);  
}

PRBool UnicodeString::endsWith(const UnicodeString& aUnicodeString) const
{
  return (PR_TRUE);  
}

UnicodeString& UnicodeString::removeBetween(TextOffset aStart, TextOffset aLimit)
{
  UnicodeString u;
  return (u);
}

PRInt8 UnicodeString::compare(const UnicodeString& aUnicodeString) const
{
  return 0;
}

PRInt8 UnicodeString::compare(TextOffset aOffset, PRInt32 aThisLength, const UnicodeString& aUnicodeString, TextOffset aStringOffset, PRInt32 aLength) const
{
  return 0;
}

PRInt8 UnicodeString::compare(const PRUnichar* aUnichar) const
{
  return 0;
}

PRInt8 UnicodeString::compare(const PRUnichar* aUnichar, PRInt32 aLength) const
{
  return 0;
}

PRInt8 UnicodeString::compare(const char* aChar) const
{
  return 0;
}

UnicodeString& UnicodeString::extract(TextOffset aOffset,PRInt32 aLength, UnicodeString& aExtractInto) const
{
  UnicodeString u;
  return (u);
}

void UnicodeString::extract(TextOffset aOffset, PRInt32 aLength, PRUnichar*aExtractInto) const
{
  return;
}

void UnicodeString::extract(TextOffset aOffset, PRInt32 aLength, char* aExtractInto) const
{
  return;
}

PRUnichar UnicodeString::operator[](TextOffset	aOffset) const
{
  PRUnichar p;
  return (p);
}

PRUnichar& UnicodeString::operator[](TextOffset	aOffset)
{
  PRUnichar p;
  return (p);
}

UnicodeString&  UnicodeString::operator+=(const UnicodeString& aUnicodeString)
{
  UnicodeString u;
  return (u);
}

UnicodeString&  UnicodeString::operator+=(PRUnichar aUnichar)
{
  UnicodeString u;
  return (u);
}

PRBool          UnicodeString::operator==(const UnicodeString& aUnicodeString) const
{
  return (PR_TRUE);
}

PRBool          UnicodeString::operator!=(const UnicodeString& aUnicodeString) const
{
  return (PR_TRUE);
}

UnicodeString&  UnicodeString::operator=(const UnicodeString& aUnicodeString)
{
  UnicodeString u;
  return (u);
}

