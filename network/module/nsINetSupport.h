/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#ifndef __nsINetSupport_h
#define __nsINetSupport_h
#include "nsString.h"

// {4F065980-015B-11d2-AA0C-00805F8A7AC4}
NS_DECLARE_ID(kINetSupportIID,
  0x4f065980, 0x15b, 0x11d2, 0xaa, 0xc, 0x0, 0x80, 0x5f, 0x8a, 0x7a, 0xc4);

class nsINetSupport: public nsISupports {
public:
  NS_IMETHOD_(void) Alert(const nsString &aText) = 0;
  
  NS_IMETHOD_(PRBool) Confirm(const nsString &aText) = 0;

  NS_IMETHOD_(PRBool) Prompt(const nsString &aText,
                             const nsString &aDefault,
                             nsString &aResult) = 0;

  NS_IMETHOD_(PRBool) PasswordPrompt(const nsString &aText,
                                     nsString &aUser,
                                     nsString &aPassword) = 0;

  
};

#endif
