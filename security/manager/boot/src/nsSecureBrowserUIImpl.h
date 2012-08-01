/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSecureBrowserUIImpl_h_
#define nsSecureBrowserUIImpl_h_

#include "mozilla/ReentrantMonitor.h"
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
#include "nsIAssociatedContentSecurity.h"
#include "pldhash.h"
#include "nsINetUtil.h"

class nsISSLStatus;
class nsITransportSecurityInfo;
class nsISecurityWarningDialogs;
class nsIChannel;
class nsIInterfaceRequestor;

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

  NS_IMETHOD Notify(nsIDOMHTMLFormElement* formNode, nsIDOMWindow* window,
                    nsIURI *actionURL, bool* cancelSubmit);
  NS_IMETHOD NotifyInvalidSubmit(nsIDOMHTMLFormElement* formNode,
                                 nsIArray* invalidElements) { return NS_OK; };
  
protected:
  mozilla::ReentrantMonitor mReentrantMonitor;
  
  nsWeakPtr mWindow;
  nsCOMPtr<nsINetUtil> mIOService;
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

  lockIconState mNotifiedSecurityState;
  bool mNotifiedToplevelIsEV;

  void ResetStateTracking();
  PRUint32 mNewToplevelSecurityState;
  bool mNewToplevelIsEV;
  bool mNewToplevelSecurityStateKnown;
  bool mIsViewSource;

  nsXPIDLString mInfoTooltip;
  PRInt32 mDocumentRequestsInProgress;
  PRInt32 mSubRequestsHighSecurity;
  PRInt32 mSubRequestsLowSecurity;
  PRInt32 mSubRequestsBrokenSecurity;
  PRInt32 mSubRequestsNoSecurity;
  bool mRestoreSubrequests;
  bool mOnLocationChangeSeen;
#ifdef DEBUG
  /* related to mReentrantMonitor */
  PRInt32 mOnStateLocationChangeReentranceDetection;
#endif

  static already_AddRefed<nsISupports> ExtractSecurityInfo(nsIRequest* aRequest);
  static nsresult MapInternalToExternalState(PRUint32* aState, lockIconState lock, bool ev);
  nsresult UpdateSecurityState(nsIRequest* aRequest, bool withNewLocation,
                               bool withUpdateStatus, bool withUpdateTooltip);
  bool UpdateMyFlags(bool &showWarning, lockIconState &warnSecurityState);
  nsresult TellTheWorld(bool showWarning, 
                        lockIconState warnSecurityState, 
                        nsIRequest* aRequest);

  nsresult EvaluateAndUpdateSecurityState(nsIRequest* aRequest, nsISupports *info,
                                          bool withNewLocation);
  void UpdateSubrequestMembers(nsISupports *securityInfo);

  void ObtainEventSink(nsIChannel *channel, 
                       nsCOMPtr<nsISecurityEventSink> &sink);

  nsCOMPtr<nsISSLStatus> mSSLStatus;
  nsCOMPtr<nsISupports> mCurrentToplevelSecurityInfo;

  void GetBundleString(const PRUnichar* name, nsAString &outString);
  
  nsresult CheckPost(nsIURI *formURI, nsIURI *actionURL, bool *okayToPost);
  nsresult IsURLHTTPS(nsIURI* aURL, bool *value);
  nsresult IsURLJavaScript(nsIURI* aURL, bool *value);

  bool ConfirmEnteringSecure();
  bool ConfirmEnteringWeak();
  bool ConfirmLeavingSecure();
  bool ConfirmMixedMode();
  bool ConfirmPostToInsecure();
  bool ConfirmPostToInsecureFromSecure();

  bool GetNSSDialogs(nsCOMPtr<nsISecurityWarningDialogs> & dialogs,
                     nsCOMPtr<nsIInterfaceRequestor> & window);

  PLDHashTable mTransferringRequests;
};


#endif /* nsSecureBrowserUIImpl_h_ */
