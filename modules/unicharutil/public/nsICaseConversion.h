/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef nsICaseConversion_h__
#define nsICaseConversion_h__


#include "nsISupports.h"
#include "nscore.h"

// {07D3D8E0-9614-11d2-B3AD-00805F8A6670}
#define NS_ICASECONVERSION_IID \
{ 0x7d3d8e0, 0x9614, 0x11d2, \
    { 0xb3, 0xad, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

class nsICaseConversion : public nsISupports {

public: 

  // Convert one Unicode character into upper case
  NS_IMETHOD ToUpper( PRUnichar aChar, PRUnichar* aReturn) = 0;

  // Convert one Unicode character into lower case
  NS_IMETHOD ToLower( PRUnichar aChar, PRUnichar* aReturn) = 0;

  // Convert one Unicode character into title case
  NS_IMETHOD ToTitle( PRUnichar aChar, PRUnichar* aReturn) = 0;

  // Convert an array of Unicode characters into upper case
  NS_IMETHOD ToUpper( const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen) = 0;

  // Convert an array of Unicode characters into lower case
  NS_IMETHOD ToLower( const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen) = 0;

  // Convert an array of Unicode characters into title case
  NS_IMETHOD ToTitle( const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen) = 0;
   
};

#endif  /* nsICaseConversion_h__ */
