/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
 *   Brian Ryner <bryner@netscape.com>
 */

#ifndef nsSecureBrowserUIImpl_h_
#define nsSecureBrowserUIImpl_h_

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsIObserver.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindowInternal.h"
#include "nsIStringBundle.h"
#include "nsISecureBrowserUI.h"
#include "nsIDocShell.h"
#include "nsIPref.h"
#include "nsIWebProgressListener.h"
#include "nsIFormSubmitObserver.h"
#include "nsIURI.h"
#include "nsISecurityEventSink.h"
#include "nsWeakReference.h"

#define NS_SECURE_BROWSER_UI_CID \
{ 0xcc75499a, 0x1dd1, 0x11b2, {0x8a, 0x82, 0xca, 0x41, 0x0a, 0xc9, 0x07, 0xb8}}


class nsSecureBrowserUIImpl : public nsSecureBrowserUI,
                              public nsIWebProgressListener,
                              public nsIFormSubmitObserver,
                              public nsIObserver,
                              public nsSupportsWeakReference
{
public:
  
  nsSecureBrowserUIImpl();
  virtual ~nsSecureBrowserUIImpl();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSSECUREBROWSERUI
  
  // nsIObserver
  NS_DECL_NSIOBSERVER
  NS_IMETHOD Notify(nsIContent* formNode, nsIDOMWindowInternal* window,
                    nsIURI *actionURL, PRBool* cancelSubmit);
  
protected:
  
  nsCOMPtr<nsIDOMWindowInternal> mWindow;
  nsCOMPtr<nsIDOMElement> mSecurityButton;
  nsCOMPtr<nsIDocumentLoaderObserver> mOldWebShellObserver;
  nsCOMPtr<nsIPref> mPref;
  nsCOMPtr<nsIStringBundle> mStringBundle;
  nsCOMPtr<nsIURI> mCurrentURI;
  
  PRBool mMixContentAlertShown;
  PRInt32 mSecurityState;

  void GetBundleString(const PRUnichar* name, nsString &outString);
  
  nsresult CheckProtocolContextSwitch(nsISecurityEventSink* sink,
                                      nsIRequest* request, nsIChannel* aChannel);
  nsresult CheckMixedContext(nsISecurityEventSink* sink, nsIRequest* request,
                             nsIChannel* aChannel);
  nsresult CheckPost(nsIURI *actionURL, PRBool *okayToPost);
  nsresult IsURLHTTPS(nsIURI* aURL, PRBool *value);
  nsresult SetBrokenLockIcon(nsISecurityEventSink* sink, nsIRequest* request,
                             PRBool removeValue = PR_FALSE);
};


#endif /* nsSecureBrowserUIImpl_h_ */
