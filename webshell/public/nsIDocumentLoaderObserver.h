/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef nsIDocumentLoaderObserver_h___
#define nsIDocumentLoaderObserver_h___

#include "nsISupports.h"

// Forward declarations... 
class nsIURL;
class nsIContentViewer;


/* f6b4f550-317c-11d2-bd8c-00805f8ae3f4 */
#define NS_IDOCUMENT_LOADER_OBSERVER_IID   \
{ 0xf6b4f550, 0x317c, 0x11d2, \
  {0xbd, 0x8c, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

/*
 *
 *
 */
class nsIDocumentLoaderObserver : public nsISupports 
{
public:
  NS_IMETHOD OnStartURLLoad(nsIURL* aURL, const char* aContentType, 
                            nsIContentViewer* aViewer) = 0;
  /**
   * Notify the observer that all connections are complete
   */
  NS_IMETHOD OnConnectionsComplete() = 0;
};


#endif /* nsIDocumentLoaderObserver_h___ */
