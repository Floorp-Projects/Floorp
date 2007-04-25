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
 * The Original Code is Growl implementation of nsIAlertsService.
 *
 * The Initial Developer of the Original Code is
 *   Shawn Wilsher <me@shawnwilsher.com>.
 * Portions created by the Initial Developer are Copyright (C) 2006-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsAlertsImageLoadListener.h"

#ifdef DEBUG
#include "nsIRequest.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#endif

NS_IMPL_ISUPPORTS1(nsAlertsImageLoadListener, nsIStreamLoaderObserver)

nsAlertsImageLoadListener::nsAlertsImageLoadListener(const nsAString& aAlertTitle,
                                                     const nsAString& aAlertText,
                                                     PRBool aAlertClickable,
                                                     const nsAString& aAlertCookie,
                                                     PRUint32 aAlertListenerKey) :
  mAlertTitle(aAlertTitle), mAlertText(aAlertText),
  mAlertClickable(aAlertClickable), mAlertCookie(aAlertCookie),
  mAlertListenerKey(aAlertListenerKey)
{
}

NS_IMETHODIMP
nsAlertsImageLoadListener::OnStreamComplete(nsIStreamLoader* aLoader,
                                            nsISupports* aContext,
                                            nsresult aStatus,
                                            PRUint32 aLength,
                                            const PRUint8* aResult)
{
#ifdef DEBUG
  // print a load error on bad status
  nsCOMPtr<nsIRequest> request;
  aLoader->GetRequest(getter_AddRefs(request));
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);

  if (NS_FAILED(aStatus)) {
    if (channel) {
      nsCOMPtr<nsIURI> uri;
      channel->GetURI(getter_AddRefs(uri));
      if (uri) {
        nsCAutoString uriSpec;
        uri->GetSpec(uriSpec);
        printf("Failed to load %s\n", uriSpec.get());
      }
    }
  }
#endif

  [mozGrowlDelegate notifyWithTitle: mAlertTitle
                        description: mAlertText
                           iconData: NS_FAILED(aStatus) ? [NSData data] :
                                       [NSData dataWithBytes: aResult
                                                      length: aLength]
                                key: mAlertListenerKey
                             cookie: mAlertCookie];

  return NS_OK;
}
