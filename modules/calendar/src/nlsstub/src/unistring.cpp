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
#include "nsCRT.h"


UnicodeString::UnicodeString()
{
  mLength = mString.Length();
}

UnicodeString::~UnicodeString()
{
}

UnicodeString::UnicodeString(const UnicodeString& aUnicodeString)
{
  mString = aUnicodeString.mString;
  mLength = mString.Length();
}

UnicodeString::UnicodeString(const char * aString)
{
  mString = aString;
  mLength = mString.Length();
}

PRInt32  UnicodeString::hashCode() const
{
  nsCRT::HashValue(mString.GetUnicode());
  return 0;
}

TextOffset UnicodeString::indexOf(const UnicodeString& aUnicodeString, TextOffset aFromOffset, PRUint32 aForLength) const
{
  return (mString.FindCharInSet((nsString&)(aUnicodeString.mString), aFromOffset));
}

TextOffset UnicodeString::indexOf(PRUnichar aUnichar, TextOffset aFromOffset, PRUint32 aForLength) const
{
  return (mString.Find(aUnichar, aFromOffset));
}


UnicodeString& UnicodeString::extractBetween(TextOffset aStart, TextOffset aLimit, UnicodeString& aExtractInto) const
{
  nsString a = mString;
  nsString b;
  a.Mid(b,aStart,aLimit);
  aExtractInto.mString = b;
  return (aExtractInto);
}


PRInt32 UnicodeString::compareIgnoreCase(const UnicodeString& aUnicodeString) const
{
  return (mString.Compare(aUnicodeString.mString,PR_TRUE));
}

PRInt32 UnicodeString::compareIgnoreCase(const PRUnichar* aUnichar, PRInt32 aLength) const
{
  return (mString.Compare(aUnichar,PR_TRUE),aLength);
}

PRInt32 UnicodeString::compareIgnoreCase(const PRUnichar* aUnichar) const
{
  return (mString.Compare(aUnichar,PR_TRUE));
}

PRInt32 UnicodeString::compareIgnoreCase(const char*	aChar, const char* aEncoding) const
{
  return (mString.Compare(aChar,PR_TRUE));
}

PRInt32 UnicodeString::compareIgnoreCase(const char*	aChar) const
{
  return (mString.Compare(aChar,PR_TRUE));
}

UnicodeString& UnicodeString::toUpper()
{
  
  mString.ToUpperCase();
  return (*this);
}

UnicodeString& UnicodeString::toUpper(const Locale& aLocale)
{
  mString.ToUpperCase();
  return (*this);
}

char* UnicodeString::toCString(const char* aEncoding) const
{
  return (mString.ToNewCString());
}

UnicodeString& UnicodeString::trim(UnicodeString& aUnicodeString) const
{
  aUnicodeString.mString.CompressWhitespace();
  return (aUnicodeString);
}

void UnicodeString::trim()
{
  mString.CompressWhitespace();
  mLength = mString.Length();
  return;
}

UnicodeString& UnicodeString::remove()
{
  mString.Truncate();
  mLength = mString.Length();
  return (*this);
}

UnicodeString& UnicodeString::remove(TextOffset aOffset,PRInt32 aLength)
{
  mString.Cut(aOffset,aLength);  
  mLength = mString.Length();
  return (*this);
}

UnicodeString& UnicodeString::insert(TextOffset aThisOffset, const UnicodeString& aUnicodeString)
{
  mString.Insert(*(aUnicodeString.mString),aThisOffset);
  mLength = mString.Length();
  return (*this);
}

PRBool UnicodeString::startsWith(const UnicodeString& aUnicodeString) const
{
  if (nsCRT::strncmp(aUnicodeString.mString.GetUnicode(),mString.GetUnicode(),aUnicodeString.mString.Length()) == 0)
    return PR_TRUE;

  return PR_FALSE;
}

PRBool UnicodeString::endsWith(const UnicodeString& aUnicodeString) const
{
  PRUint32 offset = mString.Length() - aUnicodeString.mString.Length();
  PRBool b = PR_FALSE;

  if (offset < 0)
    return PR_FALSE;

  char * str1 = mString.ToNewCString();
  char * str2 = aUnicodeString.mString.ToNewCString();

  if (nsCRT::strncasecmp((char *)(str1+offset),str2,aUnicodeString.mString.Length()) == 0)
    b = PR_TRUE;
  
  delete str1;
  delete str2;

  return (b);  
}

UnicodeString& UnicodeString::removeBetween(TextOffset aStart, TextOffset aLimit)
{
  mString.Cut(aStart, aLimit);
  mLength = mString.Length();
  return (*this);
}

PRInt8 UnicodeString::compare(const UnicodeString& aUnicodeString) const
{
  return (mString.Equals(aUnicodeString.mString));
}

PRInt8 UnicodeString::compare(TextOffset aOffset, 
                              PRInt32 aThisLength, 
                              const UnicodeString& aUnicodeString, 
                              TextOffset aStringOffset, 
                              PRInt32 aLength) const
{

  nsString s1 = mString,s2 = aUnicodeString.mString;

  s1.Mid(s1,aOffset,aThisLength);
  s2.Mid(s2,aStringOffset,aLength);

  return (s1.Equals(s2));
}

PRInt8 UnicodeString::compare(const PRUnichar* aUnichar) const
{
  return (mString.Compare(aUnichar));
}

PRInt8 UnicodeString::compare(const PRUnichar* aUnichar, PRInt32 aLength) const
{
  return (mString.Compare(aUnichar,PR_FALSE,aLength));
}

PRInt8 UnicodeString::compare(const char* aChar) const
{
  return (mString.Compare(aChar));
}

UnicodeString& UnicodeString::extract(TextOffset aOffset,PRInt32 aLength, UnicodeString& aExtractInto) const
{
  nsString str = mString;
  str.Mid(aExtractInto.mString,aOffset,aLength);
  aExtractInto.mLength = aExtractInto.mString.Length();
  return (aExtractInto);
}

void UnicodeString::extract(TextOffset aOffset, PRInt32 aLength, PRUnichar*aExtractInto) const
{
  PRUint32 aExtractLength = 0;
  nsString str = mString;

  if(((PRInt32)aOffset)<((PRInt32)mLength)) 
  {
    aLength=(PRInt32)(((PRInt32)(aOffset+aLength)<=((PRInt32)mLength)) ? aLength : mLength-aOffset);

    PRUnichar* from = (PRUnichar*)(str.GetUnicode() + aOffset);
    PRUnichar* end =  (PRUnichar*)(str.GetUnicode() + aOffset + aLength);

    while (from < end) 
    {
      PRUnichar ch = *from;

      aExtractInto[aExtractLength++]=ch;
      aExtractInto[aExtractLength]=0;

      from++;
    }
  }
  else 
    aLength=0;

  return ;
}

void UnicodeString::extract(TextOffset aOffset, PRInt32 aLength, char* aExtractInto) const
{
  nsString str = mString;
  char * p = str.ToNewCString();

  nsCRT::memcpy(aExtractInto,p+aOffset,aLength);
  delete p;

  return;
}

PRUnichar UnicodeString::operator[](TextOffset	aOffset) const
{
  return(mString[aOffset]);
}

PRUnichar& UnicodeString::operator[](TextOffset	aOffset)
{
  return(mString[aOffset]);
}

UnicodeString&  UnicodeString::operator+=(const UnicodeString& aUnicodeString)
{
  mString.Append(aUnicodeString.mString);
  return (*this);
}

UnicodeString&  UnicodeString::operator+=(PRUnichar aUnichar)
{
  mString.Append(aUnichar);
  return (*this);
}

PRBool          UnicodeString::operator==(const UnicodeString& aUnicodeString) const
{
  return (mString == (aUnicodeString.mString));
}

PRBool          UnicodeString::operator!=(const UnicodeString& aUnicodeString) const
{
  return (mString != (aUnicodeString.mString));
}

UnicodeString&  UnicodeString::operator=(const UnicodeString& aUnicodeString)
{
  mString = aUnicodeString.mString;
  mLength = mString.Length();
  return (*this);
}

