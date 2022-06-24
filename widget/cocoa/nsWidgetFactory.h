/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file contains forward declarations for classes defined in static
// components. The appropriate headers for those types cannot be included in
// the generated static component code directly.

#include "nsID.h"

namespace mozilla {
class OSXNotificationCenter;
}  // namespace mozilla

namespace mozilla::widget {
class ScreenManager;
}

class nsClipboardHelper;
class nsColorPicker;
class nsDeviceContextSpecX;
class nsDragService;
class nsFilePicker;
class nsHTMLFormatConverter;
class nsMacDockSupport;
class nsMacFinderProgress;
class nsMacSharingService;
class nsMacUserActivityUpdater;
class nsMacWebAppUtils;
class nsPrintDialogServiceX;
class nsPrintSettingsServiceX;
class nsPrinterListCUPS;
class nsSound;
class nsStandaloneNativeMenu;
class nsSystemStatusBarCocoa;
class nsTouchBarUpdater;
class nsTransferable;
class nsUserIdleServiceX;

nsresult nsAppShellConstructor(const nsIID&, void**);

void nsWidgetCocoaModuleCtor();
void nsWidgetCocoaModuleDtor();
