/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsURILoader_h__
#define nsURILoader_h__

#include "nsCURILoader.h"
#include "nsISupportsUtils.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"

class nsVoidArray;

class nsURILoader : public nsIURILoader
{
public:
  NS_DECL_NSIURILOADER
  NS_DECL_ISUPPORTS

  nsURILoader();
  virtual ~nsURILoader();

protected:
  // we shouldn't need to have an owning ref count on registered
  // content listeners because they are supposed to unregister themselves
  // when they go away.
  nsVoidArray * m_listeners;

  // prepare the load cookie for the window context
  nsresult SetupLoadCookie(nsISupports * aWindowContext, nsIInterfaceRequestor ** aLoadCookie);

  // a small helper function
  PRBool ShouldHandleContent(nsIURIContentListener * aCntListener, 
                             const char * aContentType,
                             nsURILoadCommand aCommand,
                             const char * aWindowTarget,
                             char ** aContentTypeToUse);

  NS_IMETHOD GetTarget(nsIChannel * aChannel, const char * aWindowTarget, 
                       nsISupports * aWindowContext,
                       nsISupports ** aRetargetedWindowContext);
};

#endif /* nsURILoader_h__ */
