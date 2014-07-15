/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsdefs.h"
#include "nsWidgetsCID.h"
#include "nsAppShell.h"
#include "nsAppShellSingleton.h"
#include "mozilla/ModuleUtils.h"
#include "nsIServiceManager.h"
#include "nsIdleServiceWin.h"
#include "nsLookAndFeel.h"
#include "nsScreenManagerWin.h"
#include "nsSound.h"
#include "WinMouseScrollHandler.h"
#include "KeyboardLayout.h"
#include "GfxInfo.h"
#include "nsToolkit.h"

// Modules that switch out based on the environment
#include "nsXULAppAPI.h"
// Desktop
#include "nsFilePicker.h" // needs to be included before other shobjidl.h includes
#include "nsColorPicker.h"
#include "nsNativeThemeWin.h"
#include "nsWindow.h"
// Content processes
#include "nsFilePickerProxy.h"
// Metro
#ifdef MOZ_METRO
#include "winrt/MetroAppShell.h"
#include "winrt/MetroWidget.h"
#include "winrt/nsMetroFilePicker.h"
#include "winrt/nsWinMetroUtils.h"
#endif

// Drag & Drop, Clipboard
#include "nsClipboardHelper.h"
#include "nsClipboard.h"
#include "nsBidiKeyboard.h"
#include "nsDragService.h"
#include "nsTransferable.h"
#include "nsHTMLFormatConverter.h"

#include "WinTaskbar.h"
#include "JumpListBuilder.h"
#include "JumpListItem.h"

#ifdef NS_PRINTING
#include "nsDeviceContextSpecWin.h"
#include "nsPrintOptionsWin.h"
#include "nsPrintSession.h"
#endif

#include "mozilla/Module.h"

using namespace mozilla;
using namespace mozilla::widget;

static nsresult
WindowConstructor(nsISupports *aOuter, REFNSIID aIID,
                  void **aResult)
{
  *aResult = nullptr;
  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }
  nsCOMPtr<nsIWidget> widget;

  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Metro) {
#ifdef MOZ_METRO
    widget = new MetroWidget;
#else
    NS_RUNTIMEABORT("build does not support metro.");
#endif
  } else {
    widget = new nsWindow;
  }

  return widget->QueryInterface(aIID, aResult);
}

static nsresult
ChildWindowConstructor(nsISupports *aOuter, REFNSIID aIID,
                       void **aResult)
{
  *aResult = nullptr;
  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }
  nsCOMPtr<nsIWidget> widget;

  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Metro) {
    return NS_NOINTERFACE;
  } else {
    widget = new ChildWindow;
  }

  return widget->QueryInterface(aIID, aResult);
}

static nsresult
FilePickerConstructor(nsISupports *aOuter, REFNSIID aIID,
                      void **aResult)
{
  *aResult = nullptr;
  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }
  nsCOMPtr<nsIFilePicker> picker;

  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Metro) {
#ifdef MOZ_METRO
    picker = new nsMetroFilePicker;
#else
    NS_RUNTIMEABORT("build does not support metro.");
#endif
  } else {
    picker = new nsFilePicker;
  }
  return picker->QueryInterface(aIID, aResult);
}

static nsresult
ColorPickerConstructor(nsISupports *aOuter, REFNSIID aIID,
                       void **aResult)
{
  *aResult = nullptr;
  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }
  nsCOMPtr<nsIColorPicker> picker;

  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Metro) {
#ifdef MOZ_METRO
    // TODO
    // picker = new nsMetroColorPicker;
    NS_ERROR("metro color picker isn't implemented currently");
    return NS_ERROR_NO_INTERFACE;
#else
    NS_RUNTIMEABORT("build does not support metro.");
#endif
  } else {
    picker = new nsColorPicker;
  }
  return picker->QueryInterface(aIID, aResult);
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerWin)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIdleServiceWin, nsIdleServiceWin::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)
NS_GENERIC_FACTORY_CONSTRUCTOR(WinTaskbar)
NS_GENERIC_FACTORY_CONSTRUCTOR(JumpListBuilder)
NS_GENERIC_FACTORY_CONSTRUCTOR(JumpListItem)
NS_GENERIC_FACTORY_CONSTRUCTOR(JumpListSeparator)
NS_GENERIC_FACTORY_CONSTRUCTOR(JumpListLink)
NS_GENERIC_FACTORY_CONSTRUCTOR(JumpListShortcut)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)
#ifdef MOZ_METRO
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWinMetroUtils)
#endif
#ifdef NS_PRINTING
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintOptionsWin, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrinterEnumeratorWin)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintSession, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecWin)
#endif

namespace mozilla {
namespace widget {
// This constructor should really be shared with all platforms.
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(GfxInfo, Init)
}
}

NS_DEFINE_NAMED_CID(NS_WINDOW_CID);
NS_DEFINE_NAMED_CID(NS_CHILD_CID);
NS_DEFINE_NAMED_CID(NS_FILEPICKER_CID);
NS_DEFINE_NAMED_CID(NS_COLORPICKER_CID);
NS_DEFINE_NAMED_CID(NS_APPSHELL_CID);
NS_DEFINE_NAMED_CID(NS_SCREENMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_GFXINFO_CID);
NS_DEFINE_NAMED_CID(NS_THEMERENDERER_CID);
NS_DEFINE_NAMED_CID(NS_IDLE_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARD_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARDHELPER_CID);
NS_DEFINE_NAMED_CID(NS_SOUND_CID);
NS_DEFINE_NAMED_CID(NS_TRANSFERABLE_CID);
NS_DEFINE_NAMED_CID(NS_HTMLFORMATCONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_WIN_TASKBAR_CID);
NS_DEFINE_NAMED_CID(NS_WIN_JUMPLISTBUILDER_CID);
NS_DEFINE_NAMED_CID(NS_WIN_JUMPLISTITEM_CID);
NS_DEFINE_NAMED_CID(NS_WIN_JUMPLISTSEPARATOR_CID);
NS_DEFINE_NAMED_CID(NS_WIN_JUMPLISTLINK_CID);
NS_DEFINE_NAMED_CID(NS_WIN_JUMPLISTSHORTCUT_CID);
#ifdef MOZ_METRO
NS_DEFINE_NAMED_CID(NS_WIN_METROUTILS_CID);
#endif
NS_DEFINE_NAMED_CID(NS_DRAGSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_BIDIKEYBOARD_CID);
#ifdef NS_PRINTING
NS_DEFINE_NAMED_CID(NS_PRINTSETTINGSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_PRINTER_ENUMERATOR_CID);
NS_DEFINE_NAMED_CID(NS_PRINTSESSION_CID);
NS_DEFINE_NAMED_CID(NS_DEVICE_CONTEXT_SPEC_CID);
#endif


static const mozilla::Module::CIDEntry kWidgetCIDs[] = {
  { &kNS_WINDOW_CID, false, nullptr, WindowConstructor },
  { &kNS_CHILD_CID, false, nullptr, ChildWindowConstructor },
  { &kNS_FILEPICKER_CID, false, nullptr, FilePickerConstructor, Module::MAIN_PROCESS_ONLY },
  { &kNS_COLORPICKER_CID, false, nullptr, ColorPickerConstructor, Module::MAIN_PROCESS_ONLY },
  { &kNS_APPSHELL_CID, false, nullptr, nsAppShellConstructor },
  { &kNS_SCREENMANAGER_CID, false, nullptr, nsScreenManagerWinConstructor,
    Module::MAIN_PROCESS_ONLY },
  { &kNS_GFXINFO_CID, false, nullptr, GfxInfoConstructor },
  { &kNS_THEMERENDERER_CID, false, nullptr, NS_NewNativeTheme },
  { &kNS_IDLE_SERVICE_CID, false, nullptr, nsIdleServiceWinConstructor },
  { &kNS_CLIPBOARD_CID, false, nullptr, nsClipboardConstructor, Module::MAIN_PROCESS_ONLY },
  { &kNS_CLIPBOARDHELPER_CID, false, nullptr, nsClipboardHelperConstructor },
  { &kNS_SOUND_CID, false, nullptr, nsSoundConstructor, Module::MAIN_PROCESS_ONLY },
  { &kNS_TRANSFERABLE_CID, false, nullptr, nsTransferableConstructor },
  { &kNS_HTMLFORMATCONVERTER_CID, false, nullptr, nsHTMLFormatConverterConstructor },
  { &kNS_WIN_TASKBAR_CID, false, nullptr, WinTaskbarConstructor },
  { &kNS_WIN_JUMPLISTBUILDER_CID, false, nullptr, JumpListBuilderConstructor },
  { &kNS_WIN_JUMPLISTITEM_CID, false, nullptr, JumpListItemConstructor },
  { &kNS_WIN_JUMPLISTSEPARATOR_CID, false, nullptr, JumpListSeparatorConstructor },
  { &kNS_WIN_JUMPLISTLINK_CID, false, nullptr, JumpListLinkConstructor },
  { &kNS_WIN_JUMPLISTSHORTCUT_CID, false, nullptr, JumpListShortcutConstructor },
  { &kNS_DRAGSERVICE_CID, false, nullptr, nsDragServiceConstructor, Module::MAIN_PROCESS_ONLY },
  { &kNS_BIDIKEYBOARD_CID, false, nullptr, nsBidiKeyboardConstructor },
#ifdef MOZ_METRO
  { &kNS_WIN_METROUTILS_CID, false, nullptr, nsWinMetroUtilsConstructor },
#endif
#ifdef NS_PRINTING
  { &kNS_PRINTSETTINGSSERVICE_CID, false, nullptr, nsPrintOptionsWinConstructor },
  { &kNS_PRINTER_ENUMERATOR_CID, false, nullptr, nsPrinterEnumeratorWinConstructor,
    Module::MAIN_PROCESS_ONLY },
  { &kNS_PRINTSESSION_CID, false, nullptr, nsPrintSessionConstructor,
    Module::MAIN_PROCESS_ONLY },
  { &kNS_DEVICE_CONTEXT_SPEC_CID, false, nullptr, nsDeviceContextSpecWinConstructor,
    Module::MAIN_PROCESS_ONLY },
#endif
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kWidgetContracts[] = {
  { "@mozilla.org/widgets/window/win;1", &kNS_WINDOW_CID },
  { "@mozilla.org/widgets/child_window/win;1", &kNS_CHILD_CID },
  { "@mozilla.org/filepicker;1", &kNS_FILEPICKER_CID, Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/colorpicker;1", &kNS_COLORPICKER_CID, Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/widget/appshell/win;1", &kNS_APPSHELL_CID },
  { "@mozilla.org/gfx/screenmanager;1", &kNS_SCREENMANAGER_CID, Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/gfx/info;1", &kNS_GFXINFO_CID },
  { "@mozilla.org/chrome/chrome-native-theme;1", &kNS_THEMERENDERER_CID },
  { "@mozilla.org/widget/idleservice;1", &kNS_IDLE_SERVICE_CID },
  { "@mozilla.org/widget/clipboard;1", &kNS_CLIPBOARD_CID, Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/widget/clipboardhelper;1", &kNS_CLIPBOARDHELPER_CID },
  { "@mozilla.org/sound;1", &kNS_SOUND_CID, Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/widget/transferable;1", &kNS_TRANSFERABLE_CID },
  { "@mozilla.org/widget/htmlformatconverter;1", &kNS_HTMLFORMATCONVERTER_CID },
  { "@mozilla.org/windows-taskbar;1", &kNS_WIN_TASKBAR_CID },
  { "@mozilla.org/windows-jumplistbuilder;1", &kNS_WIN_JUMPLISTBUILDER_CID },
  { "@mozilla.org/windows-jumplistitem;1", &kNS_WIN_JUMPLISTITEM_CID },
  { "@mozilla.org/windows-jumplistseparator;1", &kNS_WIN_JUMPLISTSEPARATOR_CID },
  { "@mozilla.org/windows-jumplistlink;1", &kNS_WIN_JUMPLISTLINK_CID },
  { "@mozilla.org/windows-jumplistshortcut;1", &kNS_WIN_JUMPLISTSHORTCUT_CID },
  { "@mozilla.org/widget/dragservice;1", &kNS_DRAGSERVICE_CID, Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/widget/bidikeyboard;1", &kNS_BIDIKEYBOARD_CID },
#ifdef MOZ_METRO
  { "@mozilla.org/windows-metroutils;1", &kNS_WIN_METROUTILS_CID },
#endif
#ifdef NS_PRINTING
  { "@mozilla.org/gfx/printsettings-service;1", &kNS_PRINTSETTINGSSERVICE_CID },
  { "@mozilla.org/gfx/printerenumerator;1", &kNS_PRINTER_ENUMERATOR_CID,
    Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/gfx/printsession;1", &kNS_PRINTSESSION_CID,
    Module::MAIN_PROCESS_ONLY },
  { "@mozilla.org/gfx/devicecontextspec;1", &kNS_DEVICE_CONTEXT_SPEC_CID,
    Module::MAIN_PROCESS_ONLY },
#endif
  { nullptr }
};

static void
nsWidgetWindowsModuleDtor()
{
  KeyboardLayout::Shutdown();
  MouseScrollHandler::Shutdown();
  nsLookAndFeel::Shutdown();
  nsToolkit::Shutdown();
  nsAppShellShutdown();
}

static const mozilla::Module kWidgetModule = {
  mozilla::Module::kVersion,
  kWidgetCIDs,
  kWidgetContracts,
  nullptr,
  nullptr,
  nsAppShellInit,
  nsWidgetWindowsModuleDtor
};

NSMODULE_DEFN(nsWidgetModule) = &kWidgetModule;
