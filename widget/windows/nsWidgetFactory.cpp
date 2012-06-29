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
#include "nsFilePicker.h"
#include "mozilla/ModuleUtils.h"
#include "nsIServiceManager.h"
#include "nsIdleServiceWin.h"
#include "nsLookAndFeel.h"
#include "nsNativeThemeWin.h"
#include "nsScreenManagerWin.h"
#include "nsSound.h"
#include "nsWindow.h"
#include "WinMouseScrollHandler.h"
#include "WinTaskbar.h"
#include "JumpListBuilder.h"
#include "JumpListItem.h"
#include "GfxInfo.h"
#include "nsToolkit.h"

// Drag & Drop, Clipboard

#include "nsClipboardHelper.h"

#include "nsClipboard.h"
#include "nsBidiKeyboard.h"
#include "nsDragService.h"
#include "nsTransferable.h"
#include "nsHTMLFormatConverter.h"

#ifdef NS_PRINTING
#include "nsDeviceContextSpecWin.h"
#include "nsPrintOptionsWin.h"
#include "nsPrintSession.h"
#endif

using namespace mozilla::widget;

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(ChildWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFilePicker)
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

namespace mozilla {
namespace widget {
// This constructor should really be shared with all platforms.
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(GfxInfo, Init);
}
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)

#ifdef NS_PRINTING
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintOptionsWin, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrinterEnumeratorWin)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintSession, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecWin)
#endif

NS_DEFINE_NAMED_CID(NS_WINDOW_CID);
NS_DEFINE_NAMED_CID(NS_CHILD_CID);
NS_DEFINE_NAMED_CID(NS_FILEPICKER_CID);
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

NS_DEFINE_NAMED_CID(NS_DRAGSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_BIDIKEYBOARD_CID);

#ifdef NS_PRINTING
NS_DEFINE_NAMED_CID(NS_PRINTSETTINGSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_PRINTER_ENUMERATOR_CID);
NS_DEFINE_NAMED_CID(NS_PRINTSESSION_CID);
NS_DEFINE_NAMED_CID(NS_DEVICE_CONTEXT_SPEC_CID);
#endif


static const mozilla::Module::CIDEntry kWidgetCIDs[] = {
  { &kNS_WINDOW_CID, false, NULL, nsWindowConstructor },
  { &kNS_CHILD_CID, false, NULL, ChildWindowConstructor },
  { &kNS_FILEPICKER_CID, false, NULL, nsFilePickerConstructor },
  { &kNS_APPSHELL_CID, false, NULL, nsAppShellConstructor },
  { &kNS_SCREENMANAGER_CID, false, NULL, nsScreenManagerWinConstructor },
  { &kNS_GFXINFO_CID, false, NULL, GfxInfoConstructor },
  { &kNS_THEMERENDERER_CID, false, NULL, NS_NewNativeTheme },
  { &kNS_IDLE_SERVICE_CID, false, NULL, nsIdleServiceWinConstructor },
  { &kNS_CLIPBOARD_CID, false, NULL, nsClipboardConstructor },
  { &kNS_CLIPBOARDHELPER_CID, false, NULL, nsClipboardHelperConstructor },
  { &kNS_SOUND_CID, false, NULL, nsSoundConstructor },
  { &kNS_TRANSFERABLE_CID, false, NULL, nsTransferableConstructor },
  { &kNS_HTMLFORMATCONVERTER_CID, false, NULL, nsHTMLFormatConverterConstructor },
  { &kNS_WIN_TASKBAR_CID, false, NULL, WinTaskbarConstructor },
  { &kNS_WIN_JUMPLISTBUILDER_CID, false, NULL, JumpListBuilderConstructor },
  { &kNS_WIN_JUMPLISTITEM_CID, false, NULL, JumpListItemConstructor },
  { &kNS_WIN_JUMPLISTSEPARATOR_CID, false, NULL, JumpListSeparatorConstructor },
  { &kNS_WIN_JUMPLISTLINK_CID, false, NULL, JumpListLinkConstructor },
  { &kNS_WIN_JUMPLISTSHORTCUT_CID, false, NULL, JumpListShortcutConstructor },
  { &kNS_DRAGSERVICE_CID, false, NULL, nsDragServiceConstructor },
  { &kNS_BIDIKEYBOARD_CID, false, NULL, nsBidiKeyboardConstructor },
#ifdef NS_PRINTING
  { &kNS_PRINTSETTINGSSERVICE_CID, false, NULL, nsPrintOptionsWinConstructor },
  { &kNS_PRINTER_ENUMERATOR_CID, false, NULL, nsPrinterEnumeratorWinConstructor },
  { &kNS_PRINTSESSION_CID, false, NULL, nsPrintSessionConstructor },
  { &kNS_DEVICE_CONTEXT_SPEC_CID, false, NULL, nsDeviceContextSpecWinConstructor },
#endif
  { NULL }
};

static const mozilla::Module::ContractIDEntry kWidgetContracts[] = {
  { "@mozilla.org/widgets/window/win;1", &kNS_WINDOW_CID },
  { "@mozilla.org/widgets/child_window/win;1", &kNS_CHILD_CID },
  { "@mozilla.org/filepicker;1", &kNS_FILEPICKER_CID },
  { "@mozilla.org/widget/appshell/win;1", &kNS_APPSHELL_CID },
  { "@mozilla.org/gfx/screenmanager;1", &kNS_SCREENMANAGER_CID },
  { "@mozilla.org/gfx/info;1", &kNS_GFXINFO_CID },
  { "@mozilla.org/chrome/chrome-native-theme;1", &kNS_THEMERENDERER_CID },
  { "@mozilla.org/widget/idleservice;1", &kNS_IDLE_SERVICE_CID },
  { "@mozilla.org/widget/clipboard;1", &kNS_CLIPBOARD_CID },
  { "@mozilla.org/widget/clipboardhelper;1", &kNS_CLIPBOARDHELPER_CID },
  { "@mozilla.org/sound;1", &kNS_SOUND_CID },
  { "@mozilla.org/widget/transferable;1", &kNS_TRANSFERABLE_CID },
  { "@mozilla.org/widget/htmlformatconverter;1", &kNS_HTMLFORMATCONVERTER_CID },
  { "@mozilla.org/windows-taskbar;1", &kNS_WIN_TASKBAR_CID },
  { "@mozilla.org/windows-jumplistbuilder;1", &kNS_WIN_JUMPLISTBUILDER_CID },
  { "@mozilla.org/windows-jumplistitem;1", &kNS_WIN_JUMPLISTITEM_CID },
  { "@mozilla.org/windows-jumplistseparator;1", &kNS_WIN_JUMPLISTSEPARATOR_CID },
  { "@mozilla.org/windows-jumplistlink;1", &kNS_WIN_JUMPLISTLINK_CID },
  { "@mozilla.org/windows-jumplistshortcut;1", &kNS_WIN_JUMPLISTSHORTCUT_CID },
  { "@mozilla.org/widget/dragservice;1", &kNS_DRAGSERVICE_CID },
  { "@mozilla.org/widget/bidikeyboard;1", &kNS_BIDIKEYBOARD_CID },
#ifdef NS_PRINTING
  { "@mozilla.org/gfx/printsettings-service;1", &kNS_PRINTSETTINGSSERVICE_CID },
  { "@mozilla.org/gfx/printerenumerator;1", &kNS_PRINTER_ENUMERATOR_CID },
  { "@mozilla.org/gfx/printsession;1", &kNS_PRINTSESSION_CID },
  { "@mozilla.org/gfx/devicecontextspec;1", &kNS_DEVICE_CONTEXT_SPEC_CID },
#endif
  { NULL }
};

static void
nsWidgetWindowsModuleDtor()
{
  MouseScrollHandler::Shutdown();
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
  nsWidgetWindowsModuleDtor
};

NSMODULE_DEFN(nsWidgetModule) = &kWidgetModule;
