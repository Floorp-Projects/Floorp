/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAlertsImageLoadListener_h_
#define nsAlertsImageLoadListener_h_

#import "mozGrowlDelegate.h"

#include "nsIStreamLoader.h"
#include "nsStringAPI.h"
#include "mozilla/Attributes.h"

class nsAlertsImageLoadListener MOZ_FINAL : public nsIStreamLoaderObserver
{
public:
  nsAlertsImageLoadListener(const nsAString &aName,
                            const nsAString& aAlertTitle,
                            const nsAString& aAlertText,
                            const nsAString& aAlertCookie,
                            PRUint32 aAlertListenerKey);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER
private:
  nsString mName;
  nsString mAlertTitle;
  nsString mAlertText;
  nsString mAlertCookie;
  PRUint32 mAlertListenerKey;
};

#endif // nsAlertsImageLoadListener_h_
