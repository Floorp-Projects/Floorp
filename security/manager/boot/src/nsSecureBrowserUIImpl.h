/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
 *   Brian Ryner <bryner@brianryner.com>
 *   Kai Engert <kaie@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsSecureBrowserUIImpl_h_
#define nsSecureBrowserUIImpl_h_

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsIObserver.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIStringBundle.h"
#include "nsISecureBrowserUI.h"
#include "nsIDocShell.h"
#include "nsIWebProgressListener.h"
#include "nsIFormSubmitObserver.h"
#include "nsIURI.h"
#include "nsISecurityEventSink.h"
#include "nsWeakReference.h"
#include "nsISSLStatusProvider.h"
#include "pldhash.h"

class nsITransportSecurityInfo;
class nsISecurityWarningDialogs;
class nsIChannel;

#define NS_SECURE_BROWSER_UI_CID \
{ 0xcc75499a, 0x1dd1, 0x11b2, {0x8a, 0x82, 0xca, 0x41, 0x0a, 0xc9, 0x07, 0xb8}}


class nsSecureBrowserUIImpl : public nsISecureBrowserUI,
                              public nsIWebProgressListener,
                              public nsIFormSubmitObserver,
                              public nsIObserver,
                              public nsSupportsWeakReference,
                              public nsISSLStatusProvider
{
public:
  
  nsSecureBrowserUIImpl();
  virtual ~nsSecureBrowserUIImpl();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSISECUREBROWSERUI
  
  // nsIObserver
  NS_DECL_NSIOBSERVER
  NS_DECL_NSISSLSTATUSPROVIDER

  NS_IMETHOD Notify(nsIDOMHTMLFormElement* formNode, nsIDOMWindowInternal* window,
                    nsIURI *actionURL, PRBool* cancelSubmit);
  
protected:
  
  nsCOMPtr<nsIDOMWindow> mWindow;
  nsCOMPtr<nsIStringBundle> mStringBundle;
  nsCOMPtr<nsIURI> mCurrentURI;
  nsCOMPtr<nsISecurityEventSink> mToplevelEventSink;
  
  enum lockIconState {
    lis_no_security,
    lis_broken_security,
    lis_mixed_security,
    lis_low_security,
    lis_high_security
  };

  lockIconState mPreviousSecurityState;

  void ResetStateTracking();
  PRUint32 mNewToplevelSecurityState;
  PRPackedBool mNewToplevelSecurityStateKnown;
  PRPackedBool mIsViewSource;

  nsXPIDLString mInfoTooltip;
  PRInt32 mDocumentRequestsInProgress;
  PRInt32 mSubRequestsInProgress;
  PRInt32 mSubRequestsHighSecurity;
  PRInt32 mSubRequestsLowSecurity;
  PRInt32 mSubRequestsBrokenSecurity;
  PRInt32 mSubRequestsNoSecurity;

  nsresult UpdateSecurityState(nsIRequest* aRequest);
  nsresult EvaluateAndUpdateSecurityState(nsIRequest *aRequest);
  void UpdateSubrequestMembers(nsIRequest *aRequest);

  void ObtainEventSink(nsIChannel *channel);
  
  nsCOMPtr<nsISupports> mSSLStatus;

  void GetBundleString(const PRUnichar* name, nsAString &outString);
  
  nsresult CheckPost(nsIURI *formURI, nsIURI *actionURL, PRBool *okayToPost);
  nsresult IsURLHTTPS(nsIURI* aURL, PRBool *value);
  nsresult IsURLJavaScript(nsIURI* aURL, PRBool *value);

  PRBool ConfirmEnteringSecure();
  PRBool ConfirmEnteringWeak();
  PRBool ConfirmLeavingSecure();
  PRBool ConfirmMixedMode();
  PRBool ConfirmPostToInsecure();
  PRBool ConfirmPostToInsecureFromSecure();

  // Support functions
  nsresult GetNSSDialogs(nsISecurityWarningDialogs **);

  PLDHashTable mTransferringRequests;
};


#endif /* nsSecureBrowserUIImpl_h_ */
