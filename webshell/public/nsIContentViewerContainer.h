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
#ifndef nsIContentViewerContainer_h___
#define nsIContentViewerContainer_h___

#include "nsweb.h"
#include "prtypes.h"
#include "nsISupports.h"

class nsIContentViewer;
class nsIURL;

#define NS_ICONTENT_VIEWER_CONTAINER_IID \
 { 0xa6cf9055, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/*
 * The primary container interface for objects that can contain
 * implementations of nsIContentViewer.
 */
class nsIContentViewerContainer : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICONTENT_VIEWER_CONTAINER_IID; return iid; }

  NS_IMETHOD QueryCapability(const nsIID &aIID, void** aResult) = 0;

  NS_IMETHOD Embed(nsIContentViewer* aDocViewer, 
                   const char* aCommand,
                   nsISupports* aExtraInfo) = 0;

  NS_IMETHOD GetContentViewer(nsIContentViewer** aResult) = 0;

  NS_IMETHOD HandleUnknownContentType( nsIURL* aURL,
                                       const char *aContentType,
                                       const char *aCommand ) = 0;
};

#endif /* nsIContentViewerContainer_h___ */
