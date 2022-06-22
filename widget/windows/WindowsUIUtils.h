/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_WindowsUIUtils_h__
#define mozilla_widget_WindowsUIUtils_h__

#include "nsIWindowsUIUtils.h"
#include "nsString.h"
#include "nsColor.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"

using SharePromise =
    mozilla::MozPromise<bool, nsresult, /* IsExclusive */ true>;

namespace mozilla {
enum class ColorScheme : uint8_t;
}

class WindowsUIUtils final : public nsIWindowsUIUtils {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWINDOWSUIUTILS

  WindowsUIUtils();

  static RefPtr<SharePromise> Share(nsAutoString aTitle, nsAutoString aText,
                                    nsAutoString aUrl);

  static void UpdateInTabletMode();
  static bool GetInTabletMode();

  // Gets the system accent color, or one of the darker / lighter variants
  // (darker = -1/2/3, lighter=+1/2/3, values outside of that range are
  // disallowed).
  static mozilla::Maybe<nscolor> GetAccentColor(int aTone = 0);
  static mozilla::Maybe<nscolor> GetSystemColor(mozilla::ColorScheme, int);

  // Use LookAndFeel for a cached getter.
  static bool ComputeOverlayScrollbars();
  static double ComputeTextScaleFactor();

 protected:
  ~WindowsUIUtils();
};

#endif  // mozilla_widget_WindowsUIUtils_h__
