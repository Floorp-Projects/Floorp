/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*

  An nsIDocumentLoaderFactory that is used to load plugins

*/

#ifndef nsPluginDocLoaderFactory_h__
#define nsPluginDocLoaderFactory_h__

#include "nsIDocumentLoaderFactory.h"

class nsIChannel;
class nsIContentViewer;
class nsILoadGroup;
class nsIStreamListener;

/**
 * An nsIDocumentLoaderFactory that is used to load plugins
 */
class nsPluginDocLoaderFactory : public nsIDocumentLoaderFactory
{
protected:
  nsPluginDocLoaderFactory() { NS_INIT_ISUPPORTS(); }
  virtual ~nsPluginDocLoaderFactory() {}

public:
  static NS_METHOD
  Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

  NS_DECL_ISUPPORTS

  // nsIDocumentLoaderFactory
  NS_IMETHOD CreateInstance(const char* aCommand,
                            nsIChannel* aChannel,
                            nsILoadGroup* aLoadGroup,
                            const char* aContentType, 
                            nsISupports* aContainer,
                            nsISupports* aExtraInfo,
                            nsIStreamListener** aDocListener,
                            nsIContentViewer** aDocViewer);

  NS_IMETHOD CreateInstanceForDocument(nsISupports* aContainer,
                                       nsIDocument* aDocument,
                                       const char *aCommand,
                                       nsIContentViewer** aDocViewerResult);
};

/* dd1b8d10-1dd1-11b2-9852-e162b2c46000 */
#define NS_PLUGINDOCLOADERFACTORY_CID \
{ 0xdd1b8d10, 0x1dd1, 0x11b2, { 0x98, 0x52, 0xe1, 0x62, 0xb2, 0xc4, 0x60, 0x00 } }

#endif // nsPluginDocLoaderFactory_h__


