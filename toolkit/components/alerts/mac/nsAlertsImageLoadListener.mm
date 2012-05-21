/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAlertsImageLoadListener.h"
#include "nsObjCExceptions.h"

#ifdef DEBUG
#include "nsIRequest.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#endif

NS_IMPL_ISUPPORTS1(nsAlertsImageLoadListener, nsIStreamLoaderObserver)

nsAlertsImageLoadListener::nsAlertsImageLoadListener(const nsAString &aName,
                                                     const nsAString& aAlertTitle,
                                                     const nsAString& aAlertText,
                                                     const nsAString& aAlertCookie,
                                                     PRUint32 aAlertListenerKey) :
  mName(aName), mAlertTitle(aAlertTitle), mAlertText(aAlertText),
  mAlertCookie(aAlertCookie), mAlertListenerKey(aAlertListenerKey)
{
}

NS_IMETHODIMP
nsAlertsImageLoadListener::OnStreamComplete(nsIStreamLoader* aLoader,
                                            nsISupports* aContext,
                                            nsresult aStatus,
                                            PRUint32 aLength,
                                            const PRUint8* aResult)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

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

  [mozGrowlDelegate notifyWithName: mName
                             title: mAlertTitle
                       description: mAlertText
                          iconData: NS_FAILED(aStatus) ? [NSData data] :
                                      [NSData dataWithBytes: aResult
                                                     length: aLength]
                               key: mAlertListenerKey
                            cookie: mAlertCookie];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
