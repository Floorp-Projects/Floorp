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

#ifndef nsIStreamObject_h___
#define nsIStreamObject_h___

#include "nscore.h"
#include "nsxpfc.h"
#include "nsISupports.h"
#include "nsIWebViewerContainer.h"
#include "nsIURL.h"
#include "nsIStreamListener.h"

// 38acdab0-4914-11d2-924a-00805f8a7ab6
#define NS_ISTREAM_OBJECT_IID      \
 { 0x38acdab0, 0x4914, 0x11d2, \
   {0x92, 0x4a, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

// Application Shell Interface
class nsIStreamObject : public nsISupports 
{
public:

  /**
   * Initialize the StreamObject
   * @result The result of the initialization, NS_OK if no errors
   */
  NS_IMETHOD Init() = 0;

};

#endif /* nsIStreamObject_h___ */
