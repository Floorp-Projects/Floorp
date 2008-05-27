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
 * Christopher Blizzard <blizzard@mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsISuiteRemoteService.h"

#include "nsCOMPtr.h"
#include "nsString.h"

class nsIDOMWindowInternal;

// {3dfe7324-1dd2-11b2-9ff2-8853f91e8a20}

#define NS_XREMOTESERVICE_CID \
  { 0x3dfe7324, 0x1dd2, 0x11b2, \
  { 0x9f, 0xf2, 0x88, 0x53, 0xf9, 0x1e, 0x8a, 0x20 } }

class XRemoteService : public nsISuiteRemoteService
{
 public:
  XRemoteService();
  virtual ~XRemoteService();

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_XREMOTESERVICE_CID)

  NS_DECL_ISUPPORTS

  NS_DECL_NSISUITEREMOTESERVICE

 private:
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
  nsresult GetCalendarLocation(char **_retval);

  // returns true if the URL may be loaded.
  PRBool   MayOpenURL(const nsCString &aURL);

  // remote command handlers
  nsresult OpenURL(nsCString &aArgument,
                   nsIDOMWindow* aParent,
                   PRBool aOpenBrowser);

  // handle xfe commands
  nsresult XfeDoCommand(nsCString &aArgument,
                        nsIDOMWindow* aParent);

  // find the most recent window of a certain type
  nsresult FindWindow(const PRUnichar *aType,
                      nsIDOMWindowInternal **_retval);
};
