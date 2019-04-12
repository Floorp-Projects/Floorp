/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nsTouchBar.h"
#include "nsITouchBarInput.h"
#include "nsTouchBarUpdater.h"

#include "nsCocoaWindow.h"
#include "nsIArray.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"

#if !defined(MAC_OS_X_VERSION_10_12_2) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12_2
@interface BaseWindow (NSTouchBarProvider)
@property(strong) NSTouchBar* touchBar;
@end
#endif

NS_IMPL_ISUPPORTS(nsTouchBarUpdater, nsITouchBarUpdater);

NS_IMETHODIMP
nsTouchBarUpdater::UpdateTouchBarInputs(nsIBaseWindow* aWindow,
                                        const nsTArray<RefPtr<nsITouchBarInput>>& aInputs) {
  nsCOMPtr<nsIWidget> widget = nullptr;
  aWindow->GetMainWidget(getter_AddRefs(widget));
  if (!widget) {
    return NS_ERROR_FAILURE;
  }
  BaseWindow* cocoaWin = (BaseWindow*)widget->GetNativeData(NS_NATIVE_WINDOW);
  if (!cocoaWin) {
    return NS_ERROR_FAILURE;
  }

  if ([cocoaWin respondsToSelector:@selector(touchBar)]) {
    size_t itemCount = aInputs.Length();
    for (size_t i = 0; i < itemCount; ++i) {
      nsCOMPtr<nsITouchBarInput> input(aInputs.ElementAt(i));
      if (!input) {
        continue;
      }

      TouchBarInput* convertedInput = [[TouchBarInput alloc] initWithXPCOM:input];
      [(nsTouchBar*)cocoaWin.touchBar updateItem:convertedInput];
    }
  }

  return NS_OK;
}
