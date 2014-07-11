/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "mozilla/ModuleUtils.h"

#include "nsWidgetsCID.h"

#include "nsChildView.h"
#include "nsCocoaWindow.h"
#include "nsAppShell.h"
#include "nsAppShellSingleton.h"
#include "nsFilePicker.h"
#include "nsColorPicker.h"

#include "nsClipboard.h"
#include "nsClipboardHelper.h"
#include "nsTransferable.h"
#include "nsHTMLFormatConverter.h"
#include "nsDragService.h"
#include "nsToolkit.h"

#include "nsLookAndFeel.h"

#include "nsSound.h"
#include "nsIdleServiceX.h"
#include "NativeKeyBindings.h"
#include "OSXNotificationCenter.h"

#include "nsScreenManagerCocoa.h"
#include "nsDeviceContextSpecX.h"
#include "nsPrintOptionsX.h"
#include "nsPrintDialogX.h"
#include "nsPrintSession.h"
#include "nsToolkitCompsCID.h"

#include "mozilla/Module.h"

using namespace mozilla;
using namespace mozilla::widget;

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCocoaWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsChildView)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFilePicker)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsColorPicker)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerCocoa)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecX)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintOptionsX, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintDialogServiceX, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintSession, Init)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIdleServiceX, nsIdleServiceX::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(OSXNotificationCenter, Init)

#include "nsMenuBarX.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNativeMenuServiceX)

#include "nsBidiKeyboard.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)

#include "nsNativeThemeCocoa.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNativeThemeCocoa)

#include "nsMacDockSupport.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacDockSupport)

#include "nsMacWebAppUtils.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacWebAppUtils)

#include "nsStandaloneNativeMenu.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStandaloneNativeMenu)

#include "nsSystemStatusBarCocoa.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSystemStatusBarCocoa)

#include "GfxInfo.h"
namespace mozilla {
namespace widget {
// This constructor should really be shared with all platforms.
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(GfxInfo, Init)
}
}

NS_DEFINE_NAMED_CID(NS_WINDOW_CID);
NS_DEFINE_NAMED_CID(NS_POPUP_CID);
NS_DEFINE_NAMED_CID(NS_CHILD_CID);
NS_DEFINE_NAMED_CID(NS_FILEPICKER_CID);
NS_DEFINE_NAMED_CID(NS_COLORPICKER_CID);
NS_DEFINE_NAMED_CID(NS_APPSHELL_CID);
NS_DEFINE_NAMED_CID(NS_SOUND_CID);
NS_DEFINE_NAMED_CID(NS_TRANSFERABLE_CID);
NS_DEFINE_NAMED_CID(NS_HTMLFORMATCONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARD_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARDHELPER_CID);
NS_DEFINE_NAMED_CID(NS_DRAGSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_BIDIKEYBOARD_CID);
NS_DEFINE_NAMED_CID(NS_THEMERENDERER_CID);
NS_DEFINE_NAMED_CID(NS_SCREENMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_DEVICE_CONTEXT_SPEC_CID);
NS_DEFINE_NAMED_CID(NS_PRINTSESSION_CID);
NS_DEFINE_NAMED_CID(NS_PRINTSETTINGSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_PRINTDIALOGSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_IDLE_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_SYSTEMALERTSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_NATIVEMENUSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_MACDOCKSUPPORT_CID);
NS_DEFINE_NAMED_CID(NS_MACWEBAPPUTILS_CID);
NS_DEFINE_NAMED_CID(NS_STANDALONENATIVEMENU_CID);
NS_DEFINE_NAMED_CID(NS_MACSYSTEMSTATUSBAR_CID);
NS_DEFINE_NAMED_CID(NS_GFXINFO_CID);

static const mozilla::Module::CIDEntry kWidgetCIDs[] = {
  { &kNS_WINDOW_CID, false, NULL, nsCocoaWindowConstructor },
  { &kNS_POPUP_CID, false, NULL, nsCocoaWindowConstructor },
  { &kNS_CHILD_CID, false, NULL, nsChildViewConstructor },
  { &kNS_FILEPICKER_CID, false, NULL, nsFilePickerConstructor,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { &kNS_COLORPICKER_CID, false, NULL, nsColorPickerConstructor,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { &kNS_APPSHELL_CID, false, NULL, nsAppShellConstructor },
  { &kNS_SOUND_CID, false, NULL, nsSoundConstructor,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { &kNS_TRANSFERABLE_CID, false, NULL, nsTransferableConstructor },
  { &kNS_HTMLFORMATCONVERTER_CID, false, NULL, nsHTMLFormatConverterConstructor },
  { &kNS_CLIPBOARD_CID, false, NULL, nsClipboardConstructor,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { &kNS_CLIPBOARDHELPER_CID, false, NULL, nsClipboardHelperConstructor },
  { &kNS_DRAGSERVICE_CID, false, NULL, nsDragServiceConstructor,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { &kNS_BIDIKEYBOARD_CID, false, NULL, nsBidiKeyboardConstructor,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { &kNS_THEMERENDERER_CID, false, NULL, nsNativeThemeCocoaConstructor },
  { &kNS_SCREENMANAGER_CID, false, NULL, nsScreenManagerCocoaConstructor,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { &kNS_DEVICE_CONTEXT_SPEC_CID, false, NULL, nsDeviceContextSpecXConstructor,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { &kNS_PRINTSESSION_CID, false, NULL, nsPrintSessionConstructor,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { &kNS_PRINTSETTINGSSERVICE_CID, false, NULL, nsPrintOptionsXConstructor },
  { &kNS_PRINTDIALOGSERVICE_CID, false, NULL, nsPrintDialogServiceXConstructor,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { &kNS_IDLE_SERVICE_CID, false, NULL, nsIdleServiceXConstructor },
  { &kNS_SYSTEMALERTSSERVICE_CID, false, NULL, OSXNotificationCenterConstructor },
  { &kNS_NATIVEMENUSERVICE_CID, false, NULL, nsNativeMenuServiceXConstructor },
  { &kNS_MACDOCKSUPPORT_CID, false, NULL, nsMacDockSupportConstructor },
  { &kNS_MACWEBAPPUTILS_CID, false, NULL, nsMacWebAppUtilsConstructor },
  { &kNS_STANDALONENATIVEMENU_CID, false, NULL, nsStandaloneNativeMenuConstructor },
  { &kNS_MACSYSTEMSTATUSBAR_CID, false, NULL, nsSystemStatusBarCocoaConstructor },
  { &kNS_GFXINFO_CID, false, NULL, mozilla::widget::GfxInfoConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kWidgetContracts[] = {
  { "@mozilla.org/widgets/window/mac;1", &kNS_WINDOW_CID },
  { "@mozilla.org/widgets/popup/mac;1", &kNS_POPUP_CID },
  { "@mozilla.org/widgets/childwindow/mac;1", &kNS_CHILD_CID },
  { "@mozilla.org/filepicker;1", &kNS_FILEPICKER_CID,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/colorpicker;1", &kNS_COLORPICKER_CID,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/widget/appshell/mac;1", &kNS_APPSHELL_CID },
  { "@mozilla.org/sound;1", &kNS_SOUND_CID,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/widget/transferable;1", &kNS_TRANSFERABLE_CID },
  { "@mozilla.org/widget/htmlformatconverter;1", &kNS_HTMLFORMATCONVERTER_CID },
  { "@mozilla.org/widget/clipboard;1", &kNS_CLIPBOARD_CID,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/widget/clipboardhelper;1", &kNS_CLIPBOARDHELPER_CID },
  { "@mozilla.org/widget/dragservice;1", &kNS_DRAGSERVICE_CID,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/widget/bidikeyboard;1", &kNS_BIDIKEYBOARD_CID,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/chrome/chrome-native-theme;1", &kNS_THEMERENDERER_CID },
  { "@mozilla.org/gfx/screenmanager;1", &kNS_SCREENMANAGER_CID,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/gfx/devicecontextspec;1", &kNS_DEVICE_CONTEXT_SPEC_CID,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/gfx/printsession;1", &kNS_PRINTSESSION_CID,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/gfx/printsettings-service;1", &kNS_PRINTSETTINGSSERVICE_CID },
  { NS_PRINTDIALOGSERVICE_CONTRACTID, &kNS_PRINTDIALOGSERVICE_CID,
    mozilla::Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/widget/idleservice;1", &kNS_IDLE_SERVICE_CID },
  { "@mozilla.org/system-alerts-service;1", &kNS_SYSTEMALERTSSERVICE_CID },
  { "@mozilla.org/widget/nativemenuservice;1", &kNS_NATIVEMENUSERVICE_CID },
  { "@mozilla.org/widget/macdocksupport;1", &kNS_MACDOCKSUPPORT_CID },
  { "@mozilla.org/widget/mac-web-app-utils;1", &kNS_MACWEBAPPUTILS_CID },
  { "@mozilla.org/widget/standalonenativemenu;1", &kNS_STANDALONENATIVEMENU_CID },
  { "@mozilla.org/widget/macsystemstatusbar;1", &kNS_MACSYSTEMSTATUSBAR_CID },
  { "@mozilla.org/gfx/info;1", &kNS_GFXINFO_CID },
  { NULL }
};

static void
nsWidgetCocoaModuleDtor()
{
  NativeKeyBindings::Shutdown();
  nsLookAndFeel::Shutdown();
  nsToolkit::Shutdown();
  nsAppShellShutdown();
}

static const mozilla::Module kWidgetModule = {
  mozilla::Module::kVersion,
  kWidgetCIDs,
  kWidgetContracts,
  NULL,
  NULL,
  nsAppShellInit,
  nsWidgetCocoaModuleDtor
};

NSMODULE_DEFN(nsWidgetMacModule) = &kWidgetModule;
