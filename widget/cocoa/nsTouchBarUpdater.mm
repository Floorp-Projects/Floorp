/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nsTouchBar.h"
#include "nsTouchBarInput.h"
#include "nsTouchBarUpdater.h"

#include "nsIBaseWindow.h"
#include "nsIWidget.h"

// defined in nsCocoaWindow.mm.
extern BOOL sTouchBarIsInitialized;

NS_IMPL_ISUPPORTS(nsTouchBarUpdater, nsITouchBarUpdater);

NS_IMETHODIMP
nsTouchBarUpdater::UpdateTouchBarInputs(
    nsIBaseWindow* aWindow, const nsTArray<RefPtr<nsITouchBarInput>>& aInputs) {
  if (!sTouchBarIsInitialized || !aWindow) {
    return NS_OK;
  }

  BaseWindow* cocoaWin = nsTouchBarUpdater::GetCocoaWindow(aWindow);
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

      NSTouchBarItemIdentifier newIdentifier =
          [TouchBarInput nativeIdentifierWithXPCOM:input];
      // We don't support updating the Share scrubber since it's a special
      // Apple-made component that behaves differently from the other inputs.
      if ([newIdentifier
              isEqualToString:[TouchBarInput
                                  nativeIdentifierWithType:@"scrubber"
                                                   withKey:@"share"]]) {
        continue;
      }

      TouchBarInput* convertedInput =
          [[TouchBarInput alloc] initWithXPCOM:input];
      [(nsTouchBar*)cocoaWin.touchBar updateItem:convertedInput];
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTouchBarUpdater::ShowPopover(nsIBaseWindow* aWindow,
                               nsITouchBarInput* aPopover, bool aShowing) {
  if (!sTouchBarIsInitialized || !aPopover || !aWindow) {
    return NS_OK;
  }

  BaseWindow* cocoaWin = nsTouchBarUpdater::GetCocoaWindow(aWindow);
  if (!cocoaWin) {
    return NS_ERROR_FAILURE;
  }

  if ([cocoaWin respondsToSelector:@selector(touchBar)]) {
    // We don't need to completely reinitialize the popover. We only need its
    // identifier to look it up in [nsTouchBar mappedLayoutItems].
    NSTouchBarItemIdentifier popoverIdentifier =
        [TouchBarInput nativeIdentifierWithXPCOM:aPopover];

    TouchBarInput* popoverItem =
        [[(nsTouchBar*)cocoaWin.touchBar mappedLayoutItems]
            objectForKey:popoverIdentifier];

    [(nsTouchBar*)cocoaWin.touchBar showPopover:popoverItem showing:aShowing];
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTouchBarUpdater::EnterCustomizeMode() {
  [NSApp toggleTouchBarCustomizationPalette:(id)this];
  return NS_OK;
}

NS_IMETHODIMP
nsTouchBarUpdater::IsTouchBarInitialized(bool* aResult) {
  *aResult = sTouchBarIsInitialized;
  return NS_OK;
}

BaseWindow* nsTouchBarUpdater::GetCocoaWindow(nsIBaseWindow* aWindow) {
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

// NOTE: This method is for internal unit tests only.
NS_IMETHODIMP
nsTouchBarUpdater::SetTouchBarInitialized(bool aIsInitialized) {
  sTouchBarIsInitialized = aIsInitialized;
  return NS_OK;
}
