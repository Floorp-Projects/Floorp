/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsIUrlDispatcher_h___
#define nsIUrlDispatcher_h___


#include "nsISupports.h"
#include "nsString.h"

class nsIPostData;

// Interface ID for nsILinkHandler
#define NS_IURLDISPATCHER_IID \
{0x4023b581, 0x309c, 0x11d3, {0xbd, 0xc3, 0x0, 0x50, 0x4, 0xa, 0x9b,0x44}} 

/**
 * Interface used for dispatching URL to the proper handler
 */
class nsIUrlDispatcher : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IURLDISPATCHER_IID)


  NS_IMETHOD HandleUrl(const PRUnichar * aCommand,  const PRUnichar * aURLSpec, 
                       nsIInputStream * aPostDataStream) = 0;
};

#define NS_DECL_IURLDISPATCHER \
  NS_IMETHOD  HandleUrl(const PRUnichar *, const PRUnichar *, nsIInputStream *); \


#endif /* nsIUrlDispatcher_h___ */
