/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupports.h"
#include "mozilla/Components.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/WidgetUtils.h"

#include "nsWidgetsCID.h"

#include "nsChildView.h"
#include "nsAppShell.h"
#include "nsAppShellSingleton.h"
#include "nsFilePicker.h"
#include "nsColorPicker.h"

#include "nsClipboard.h"
#include "nsClipboardHelper.h"
#include "HeadlessClipboard.h"
#include "gfxPlatform.h"
#include "nsTransferable.h"
#include "nsHTMLFormatConverter.h"
#include "nsDragService.h"
#include "nsToolkit.h"

#include "nsLookAndFeel.h"

#include "nsSound.h"
#include "nsUserIdleServiceX.h"
#include "NativeKeyBindings.h"
#include "OSXNotificationCenter.h"

#include "nsDeviceContextSpecX.h"
#include "nsPrinterListCUPS.h"
#include "nsPrintSettingsServiceX.h"
#include "nsPrintDialogX.h"
#include "nsToolkitCompsCID.h"

#include "mozilla/widget/ScreenManager.h"

using namespace mozilla;
using namespace mozilla::widget;

NS_IMPL_COMPONENT_FACTORY(nsIClipboard) {
  nsCOMPtr<nsIClipboard> inst;
  if (gfxPlatform::IsHeadless()) {
    inst = new HeadlessClipboard();
  } else {
    inst = new nsClipboard();
  }

  return inst.forget();
}

#define MAKE_GENERIC_CTOR(class_, iface_)    \
  NS_IMPL_COMPONENT_FACTORY(class_) {        \
    RefPtr inst = new class_();              \
    return inst.forget().downcast<iface_>(); \
  }

#define MAKE_GENERIC_CTOR_INIT(class_, iface_, init_) \
  NS_IMPL_COMPONENT_FACTORY(class_) {                 \
    RefPtr inst = new class_();                       \
    if (NS_SUCCEEDED(inst->init_())) {                \
      return inst.forget().downcast<iface_>();        \
    }                                                 \
    return nullptr;                                   \
  }

#define MAKE_GENERIC_SINGLETON_CTOR(iface_, func_) \
  NS_IMPL_COMPONENT_FACTORY(iface_) { return func_(); }

MAKE_GENERIC_CTOR(nsFilePicker, nsIFilePicker)
MAKE_GENERIC_CTOR(nsColorPicker, nsIColorPicker)
MAKE_GENERIC_CTOR(nsSound, nsISound)
MAKE_GENERIC_CTOR(nsTransferable, nsITransferable)
MAKE_GENERIC_CTOR(nsHTMLFormatConverter, nsIFormatConverter)
MAKE_GENERIC_CTOR(nsClipboardHelper, nsIClipboardHelper)
MAKE_GENERIC_CTOR(nsDragService, nsIDragService)
MAKE_GENERIC_CTOR(nsDeviceContextSpecX, nsIDeviceContextSpec)
MAKE_GENERIC_CTOR(nsPrinterListCUPS, nsIPrinterList)
MAKE_GENERIC_CTOR_INIT(nsPrintSettingsServiceX, nsIPrintSettingsService, Init)
MAKE_GENERIC_CTOR_INIT(nsPrintDialogServiceX, nsIPrintDialogService, Init)
MAKE_GENERIC_SINGLETON_CTOR(nsUserIdleServiceX, nsUserIdleServiceX::GetInstance)
MAKE_GENERIC_SINGLETON_CTOR(ScreenManager, ScreenManager::GetAddRefedSingleton)
MAKE_GENERIC_CTOR_INIT(OSXNotificationCenter, nsIAlertsService, Init)

#include "nsMacDockSupport.h"
MAKE_GENERIC_CTOR(nsMacDockSupport, nsIMacDockSupport)

#include "nsMacFinderProgress.h"
MAKE_GENERIC_CTOR(nsMacFinderProgress, nsIMacFinderProgress)

#include "nsMacSharingService.h"
MAKE_GENERIC_CTOR(nsMacSharingService, nsIMacSharingService)

#include "nsMacUserActivityUpdater.h"
MAKE_GENERIC_CTOR(nsMacUserActivityUpdater, nsIMacUserActivityUpdater)

#include "nsMacWebAppUtils.h"
MAKE_GENERIC_CTOR(nsMacWebAppUtils, nsIMacWebAppUtils)

#include "nsStandaloneNativeMenu.h"
MAKE_GENERIC_CTOR(nsStandaloneNativeMenu, nsIStandaloneNativeMenu)

#include "nsSystemStatusBarCocoa.h"
MAKE_GENERIC_CTOR(nsSystemStatusBarCocoa, nsISystemStatusBar)

#include "nsTouchBarUpdater.h"
MAKE_GENERIC_CTOR(nsTouchBarUpdater, nsITouchBarUpdater)

void nsWidgetCocoaModuleCtor() { nsAppShellInit(); }

void nsWidgetCocoaModuleDtor() {
  // Shutdown all XP level widget classes.
  WidgetUtils::Shutdown();

  NativeKeyBindings::Shutdown();
  nsLookAndFeel::Shutdown();
  nsToolkit::Shutdown();
  nsAppShellShutdown();
}
