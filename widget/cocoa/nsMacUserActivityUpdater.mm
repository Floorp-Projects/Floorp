/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nsMacUserActivityUpdater.h"

#include "nsCocoaUtils.h"
#include "nsIBaseWindow.h"
#include "gfxPlatform.h"

NS_IMPL_ISUPPORTS(nsMacUserActivityUpdater, nsIMacUserActivityUpdater)

NS_IMETHODIMP
nsMacUserActivityUpdater::UpdateLocation(const nsAString& aPageUrl,
                                         const nsAString& aPageTitle,
                                         nsIBaseWindow* aWindow) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;
  if (gfxPlatform::IsHeadless()) {
    // In headless mode, Handoff will fail since there is no Cocoa window.
    return NS_OK;
  }

  BaseWindow* cocoaWin = nsMacUserActivityUpdater::GetCocoaWindow(aWindow);
  if (!cocoaWin) {
    return NS_ERROR_FAILURE;
  }

  NSURL* pageUrl = nsCocoaUtils::ToNSURL(aPageUrl);
  if (!pageUrl || (![pageUrl.scheme isEqualToString:@"https"] &&
                   ![pageUrl.scheme isEqualToString:@"http"])) {
    [cocoaWin.userActivity invalidate];
    return NS_OK;
  }

  NSString* pageTitle = nsCocoaUtils::ToNSString(aPageTitle);
  if (!pageTitle) {
    pageTitle = pageUrl.absoluteString;
  }

  NSUserActivity* userActivity = [[NSUserActivity alloc]
      initWithActivityType:NSUserActivityTypeBrowsingWeb];
  userActivity.webpageURL = pageUrl;
  userActivity.title = pageTitle;
  cocoaWin.userActivity = userActivity;
  [userActivity release];

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

BaseWindow* nsMacUserActivityUpdater::GetCocoaWindow(nsIBaseWindow* aWindow) {
  nsCOMPtr<nsIWidget> widget = nullptr;
  aWindow->GetMainWidget(getter_AddRefs(widget));
  if (!widget) {
    return nil;
  }
  BaseWindow* cocoaWin = (BaseWindow*)widget->GetNativeData(NS_NATIVE_WINDOW);
  if (!cocoaWin) {
    return nil;
  }
  return cocoaWin;
}
