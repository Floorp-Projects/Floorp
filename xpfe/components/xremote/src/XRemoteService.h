/*
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
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by Christopher Blizzard
 * are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 */

#include "nsIXRemoteService.h"
#include "nsHashtable.h"
#include "nsIDOMWindow.h"
#include "nsIWidget.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"

// {3dfe7324-1dd2-11b2-9ff2-8853f91e8a20}

#define NS_XREMOTESERVICE_CID \
  { 0x3dfe7324, 0x1dd2, 0x11b2, \
  { 0x9f, 0xf2, 0x88, 0x53, 0xf9, 0x1e, 0x8a, 0x20 } }

class XRemoteService : public nsIXRemoteService, public nsIObserver {
 public:
  XRemoteService();
  virtual ~XRemoteService();

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_XREMOTESERVICE_CID);

  NS_DECL_ISUPPORTS

  NS_DECL_NSIXREMOTESERVICE
  NS_DECL_NSIOBSERVER

 private:

  // create and destroy the proxy window
  void CreateProxyWindow();
  void DestroyProxyWindow();

  // this builds a response for any parsing
  char *BuildResponse(const char *aError, const char *aMessage);

  // find the last argument in an argument string
  void FindLastInList(nsCString &aString, nsCString &retString,
                      PRUint32 *aIndexRet);

  // find the second argument through the last argument in the string
  void FindRestInList(nsCString &aString, nsCString &retString,
		      PRUint32 *aIndexRet);

  // short cut for opening chrome windows
  nsresult OpenChromeWindow(nsIDOMWindow *aParent,
			    const char *aUrl,
			    const char *aFeatures,
			    nsISupports *aArguments,
			    nsIDOMWindow **_retval);

  // get the primary browser chrome location
  nsresult GetBrowserLocation(char **_retval);
  nsresult GetMailLocation(char **_retval);
  nsresult GetComposeLocation(const char **_retval);

  // returns true if the URL may be loaded.
  PRBool   MayOpenURL(const nsCString &aURL);

  // remote command handlers
  nsresult OpenURL(nsCString &aArgument,
		   nsIDOMWindowInternal *aParent,
		   PRBool aOpenBrowser);

  nsresult OpenURLDialog(nsIDOMWindowInternal *aParent);

  // handle xfe commands
  nsresult XfeDoCommand(nsCString &aArgument,
			nsIDOMWindowInternal *aParent);

  // find the most recent window of a certain type
  nsresult FindWindow(const PRUnichar *aType,
		      nsIDOMWindowInternal **_retval);

  // Save the profile name
  void GetProfileName(nsACString &aProfile);

  // hidden window for proxy requests
  nsCOMPtr<nsIWidget> mProxyWindow;
  
  // native window to internal dom window map
  nsHashtable mWindowList;
  // internal dom window to native window map
  nsHashtable mBrowserList;

  // the number of non-proxy windows that are set up for X Remote
  PRUint32 mNumWindows;

  // have we been started up from the main loop yet?
  PRBool   mRunning;

  // Name of our program
  nsCString mProgram;
};
