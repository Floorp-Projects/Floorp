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

NS_GENERIC_FACTORY_CONSTRUCTOR(nsFilePicker)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsColorPicker)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecX)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrinterListCUPS)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintSettingsServiceX, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintDialogServiceX, Init)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsUserIdleServiceX, nsUserIdleServiceX::GetInstance)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(ScreenManager, ScreenManager::GetAddRefedSingleton)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(OSXNotificationCenter, Init)

#include "nsMacDockSupport.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacDockSupport)

#include "nsMacFinderProgress.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacFinderProgress)

#include "nsMacSharingService.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacSharingService)

#include "nsMacUserActivityUpdater.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacUserActivityUpdater)

#include "nsMacWebAppUtils.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacWebAppUtils)

#include "nsStandaloneNativeMenu.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStandaloneNativeMenu)

#include "nsSystemStatusBarCocoa.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSystemStatusBarCocoa)

#include "nsTouchBarUpdater.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTouchBarUpdater)

NS_DEFINE_NAMED_CID(NS_FILEPICKER_CID);
NS_DEFINE_NAMED_CID(NS_COLORPICKER_CID);
NS_DEFINE_NAMED_CID(NS_APPSHELL_CID);
NS_DEFINE_NAMED_CID(NS_SOUND_CID);
NS_DEFINE_NAMED_CID(NS_TRANSFERABLE_CID);
NS_DEFINE_NAMED_CID(NS_HTMLFORMATCONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARDHELPER_CID);
NS_DEFINE_NAMED_CID(NS_DRAGSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_SCREENMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_DEVICE_CONTEXT_SPEC_CID);
NS_DEFINE_NAMED_CID(NS_PRINTER_LIST_CID);
NS_DEFINE_NAMED_CID(NS_PRINTSETTINGSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_PRINTDIALOGSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_IDLE_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_SYSTEMALERTSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_MACDOCKSUPPORT_CID);
NS_DEFINE_NAMED_CID(NS_MACFINDERPROGRESS_CID);
NS_DEFINE_NAMED_CID(NS_MACSHARINGSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_MACUSERACTIVITYUPDATER_CID);
NS_DEFINE_NAMED_CID(NS_MACWEBAPPUTILS_CID);
NS_DEFINE_NAMED_CID(NS_STANDALONENATIVEMENU_CID);
NS_DEFINE_NAMED_CID(NS_SYSTEMSTATUSBAR_CID);
NS_DEFINE_NAMED_CID(NS_TOUCHBARUPDATER_CID);

static const mozilla::Module::CIDEntry kWidgetCIDs[] = {
    {&kNS_FILEPICKER_CID, false, NULL, nsFilePickerConstructor, mozilla::Module::MAIN_PROCESS_ONLY},
    {&kNS_COLORPICKER_CID, false, NULL, nsColorPickerConstructor,
     mozilla::Module::MAIN_PROCESS_ONLY},
    {&kNS_APPSHELL_CID, false, NULL, nsAppShellConstructor,
     mozilla::Module::ALLOW_IN_GPU_RDD_VR_SOCKET_AND_UTILITY_PROCESS},
    {&kNS_SOUND_CID, false, NULL, nsSoundConstructor, mozilla::Module::MAIN_PROCESS_ONLY},
    {&kNS_TRANSFERABLE_CID, false, NULL, nsTransferableConstructor},
    {&kNS_HTMLFORMATCONVERTER_CID, false, NULL, nsHTMLFormatConverterConstructor},
    {&kNS_CLIPBOARDHELPER_CID, false, NULL, nsClipboardHelperConstructor},
    {&kNS_DRAGSERVICE_CID, false, NULL, nsDragServiceConstructor,
     mozilla::Module::MAIN_PROCESS_ONLY},
    {&kNS_SCREENMANAGER_CID, false, NULL, ScreenManagerConstructor,
     mozilla::Module::MAIN_PROCESS_ONLY},
    {&kNS_DEVICE_CONTEXT_SPEC_CID, false, NULL, nsDeviceContextSpecXConstructor,
     mozilla::Module::MAIN_PROCESS_ONLY},
    {&kNS_PRINTER_LIST_CID, false, NULL, nsPrinterListCUPSConstructor,
     mozilla::Module::MAIN_PROCESS_ONLY},
    {&kNS_PRINTSETTINGSSERVICE_CID, false, NULL, nsPrintSettingsServiceXConstructor},
    {&kNS_PRINTDIALOGSERVICE_CID, false, NULL, nsPrintDialogServiceXConstructor,
     mozilla::Module::MAIN_PROCESS_ONLY},
    {&kNS_IDLE_SERVICE_CID, false, NULL, nsUserIdleServiceXConstructor},
    {&kNS_SYSTEMALERTSSERVICE_CID, false, NULL, OSXNotificationCenterConstructor},
    {&kNS_MACDOCKSUPPORT_CID, false, NULL, nsMacDockSupportConstructor},
    {&kNS_MACFINDERPROGRESS_CID, false, NULL, nsMacFinderProgressConstructor},
    {&kNS_MACSHARINGSERVICE_CID, false, NULL, nsMacSharingServiceConstructor},
    {&kNS_MACUSERACTIVITYUPDATER_CID, false, NULL, nsMacUserActivityUpdaterConstructor},
    {&kNS_MACWEBAPPUTILS_CID, false, NULL, nsMacWebAppUtilsConstructor},
    {&kNS_STANDALONENATIVEMENU_CID, false, NULL, nsStandaloneNativeMenuConstructor},
    {&kNS_SYSTEMSTATUSBAR_CID, false, NULL, nsSystemStatusBarCocoaConstructor},
    {&kNS_TOUCHBARUPDATER_CID, false, NULL, nsTouchBarUpdaterConstructor},
    {NULL}};

static const mozilla::Module::ContractIDEntry kWidgetContracts[] = {
    {"@mozilla.org/filepicker;1", &kNS_FILEPICKER_CID, mozilla::Module::MAIN_PROCESS_ONLY},
    {"@mozilla.org/colorpicker;1", &kNS_COLORPICKER_CID, mozilla::Module::MAIN_PROCESS_ONLY},
    {"@mozilla.org/widget/appshell/mac;1", &kNS_APPSHELL_CID,
     mozilla::Module::ALLOW_IN_GPU_RDD_VR_SOCKET_AND_UTILITY_PROCESS},
    {"@mozilla.org/sound;1", &kNS_SOUND_CID, mozilla::Module::MAIN_PROCESS_ONLY},
    {"@mozilla.org/widget/transferable;1", &kNS_TRANSFERABLE_CID},
    {"@mozilla.org/widget/htmlformatconverter;1", &kNS_HTMLFORMATCONVERTER_CID},
    {"@mozilla.org/widget/clipboardhelper;1", &kNS_CLIPBOARDHELPER_CID},
    {"@mozilla.org/widget/dragservice;1", &kNS_DRAGSERVICE_CID, mozilla::Module::MAIN_PROCESS_ONLY},
    {"@mozilla.org/gfx/screenmanager;1", &kNS_SCREENMANAGER_CID,
     mozilla::Module::MAIN_PROCESS_ONLY},
    {"@mozilla.org/gfx/devicecontextspec;1", &kNS_DEVICE_CONTEXT_SPEC_CID,
     mozilla::Module::MAIN_PROCESS_ONLY},
    {"@mozilla.org/gfx/printerlist;1", &kNS_PRINTER_LIST_CID, mozilla::Module::MAIN_PROCESS_ONLY},
    {"@mozilla.org/gfx/printsettings-service;1", &kNS_PRINTSETTINGSSERVICE_CID},
    {NS_PRINTDIALOGSERVICE_CONTRACTID, &kNS_PRINTDIALOGSERVICE_CID,
     mozilla::Module::MAIN_PROCESS_ONLY},
    {"@mozilla.org/widget/useridleservice;1", &kNS_IDLE_SERVICE_CID},
    {"@mozilla.org/system-alerts-service;1", &kNS_SYSTEMALERTSSERVICE_CID},
    {"@mozilla.org/widget/macdocksupport;1", &kNS_MACDOCKSUPPORT_CID},
    {"@mozilla.org/widget/macfinderprogress;1", &kNS_MACFINDERPROGRESS_CID},
    {"@mozilla.org/widget/macsharingservice;1", &kNS_MACSHARINGSERVICE_CID},
    {"@mozilla.org/widget/macuseractivityupdater;1", &kNS_MACUSERACTIVITYUPDATER_CID},
    {"@mozilla.org/widget/mac-web-app-utils;1", &kNS_MACWEBAPPUTILS_CID},
    {"@mozilla.org/widget/standalonenativemenu;1", &kNS_STANDALONENATIVEMENU_CID},
    {"@mozilla.org/widget/systemstatusbar;1", &kNS_SYSTEMSTATUSBAR_CID},
    {"@mozilla.org/widget/touchbarupdater;1", &kNS_TOUCHBARUPDATER_CID},
    {NULL}};

static void nsWidgetCocoaModuleDtor() {
  // Shutdown all XP level widget classes.
  WidgetUtils::Shutdown();

  NativeKeyBindings::Shutdown();
  nsLookAndFeel::Shutdown();
  nsToolkit::Shutdown();
  nsAppShellShutdown();
}

extern const mozilla::Module kWidgetModule = {
    mozilla::Module::kVersion,
    kWidgetCIDs,
    kWidgetContracts,
    NULL,
    NULL,
    nsAppShellInit,
    nsWidgetCocoaModuleDtor,
    mozilla::Module::ALLOW_IN_GPU_RDD_VR_SOCKET_AND_UTILITY_PROCESS};
