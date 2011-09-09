/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Rosen <dr@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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
#include "nsToolkit.h"
#include "nsWindow.h"
#include "WinTaskbar.h"
#include "JumpListBuilder.h"
#include "JumpListItem.h"
#include "GfxInfo.h"

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
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsLookAndFeel,
                                         nsLookAndFeel::GetAddRefedInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsToolkit)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerWin)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsIdleServiceWin)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
NS_GENERIC_FACTORY_CONSTRUCTOR(WinTaskbar)
NS_GENERIC_FACTORY_CONSTRUCTOR(JumpListBuilder)
NS_GENERIC_FACTORY_CONSTRUCTOR(JumpListItem)
NS_GENERIC_FACTORY_CONSTRUCTOR(JumpListSeparator)
NS_GENERIC_FACTORY_CONSTRUCTOR(JumpListLink)
NS_GENERIC_FACTORY_CONSTRUCTOR(JumpListShortcut)
#endif

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
NS_DEFINE_NAMED_CID(NS_TOOLKIT_CID);
NS_DEFINE_NAMED_CID(NS_SCREENMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_GFXINFO_CID);
NS_DEFINE_NAMED_CID(NS_LOOKANDFEEL_CID);
NS_DEFINE_NAMED_CID(NS_THEMERENDERER_CID);
NS_DEFINE_NAMED_CID(NS_IDLE_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARD_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARDHELPER_CID);
NS_DEFINE_NAMED_CID(NS_SOUND_CID);
NS_DEFINE_NAMED_CID(NS_TRANSFERABLE_CID);
NS_DEFINE_NAMED_CID(NS_HTMLFORMATCONVERTER_CID);

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
NS_DEFINE_NAMED_CID(NS_WIN_TASKBAR_CID);
NS_DEFINE_NAMED_CID(NS_WIN_JUMPLISTBUILDER_CID);
NS_DEFINE_NAMED_CID(NS_WIN_JUMPLISTITEM_CID);
NS_DEFINE_NAMED_CID(NS_WIN_JUMPLISTSEPARATOR_CID);
NS_DEFINE_NAMED_CID(NS_WIN_JUMPLISTLINK_CID);
NS_DEFINE_NAMED_CID(NS_WIN_JUMPLISTSHORTCUT_CID);
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
  { &kNS_WINDOW_CID, false, NULL, nsWindowConstructor },
  { &kNS_CHILD_CID, false, NULL, ChildWindowConstructor },
  { &kNS_FILEPICKER_CID, false, NULL, nsFilePickerConstructor },
  { &kNS_APPSHELL_CID, false, NULL, nsAppShellConstructor },
  { &kNS_TOOLKIT_CID, false, NULL, nsToolkitConstructor },
  { &kNS_SCREENMANAGER_CID, false, NULL, nsScreenManagerWinConstructor },
  { &kNS_GFXINFO_CID, false, NULL, GfxInfoConstructor },
  { &kNS_LOOKANDFEEL_CID, false, NULL, nsLookAndFeelConstructor },
  { &kNS_THEMERENDERER_CID, false, NULL, NS_NewNativeTheme },
  { &kNS_IDLE_SERVICE_CID, false, NULL, nsIdleServiceWinConstructor },
  { &kNS_CLIPBOARD_CID, false, NULL, nsClipboardConstructor },
  { &kNS_CLIPBOARDHELPER_CID, false, NULL, nsClipboardHelperConstructor },
  { &kNS_SOUND_CID, false, NULL, nsSoundConstructor },
  { &kNS_TRANSFERABLE_CID, false, NULL, nsTransferableConstructor },
  { &kNS_HTMLFORMATCONVERTER_CID, false, NULL, nsHTMLFormatConverterConstructor },
#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
  { &kNS_WIN_TASKBAR_CID, false, NULL, WinTaskbarConstructor },
  { &kNS_WIN_JUMPLISTBUILDER_CID, false, NULL, JumpListBuilderConstructor },
  { &kNS_WIN_JUMPLISTITEM_CID, false, NULL, JumpListItemConstructor },
  { &kNS_WIN_JUMPLISTSEPARATOR_CID, false, NULL, JumpListSeparatorConstructor },
  { &kNS_WIN_JUMPLISTLINK_CID, false, NULL, JumpListLinkConstructor },
  { &kNS_WIN_JUMPLISTSHORTCUT_CID, false, NULL, JumpListShortcutConstructor },
#endif
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
  { "@mozilla.org/widget/toolkit/win;1", &kNS_TOOLKIT_CID },
  { "@mozilla.org/gfx/screenmanager;1", &kNS_SCREENMANAGER_CID },
  { "@mozilla.org/gfx/info;1", &kNS_GFXINFO_CID },
  { "@mozilla.org/widget/lookandfeel;1", &kNS_LOOKANDFEEL_CID },
  { "@mozilla.org/chrome/chrome-native-theme;1", &kNS_THEMERENDERER_CID },
  { "@mozilla.org/widget/idleservice;1", &kNS_IDLE_SERVICE_CID },
  { "@mozilla.org/widget/clipboard;1", &kNS_CLIPBOARD_CID },
  { "@mozilla.org/widget/clipboardhelper;1", &kNS_CLIPBOARDHELPER_CID },
  { "@mozilla.org/sound;1", &kNS_SOUND_CID },
  { "@mozilla.org/widget/transferable;1", &kNS_TRANSFERABLE_CID },
  { "@mozilla.org/widget/htmlformatconverter;1", &kNS_HTMLFORMATCONVERTER_CID },
#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
  { "@mozilla.org/windows-taskbar;1", &kNS_WIN_TASKBAR_CID },
  { "@mozilla.org/windows-jumplistbuilder;1", &kNS_WIN_JUMPLISTBUILDER_CID },
  { "@mozilla.org/windows-jumplistitem;1", &kNS_WIN_JUMPLISTITEM_CID },
  { "@mozilla.org/windows-jumplistseparator;1", &kNS_WIN_JUMPLISTSEPARATOR_CID },
  { "@mozilla.org/windows-jumplistlink;1", &kNS_WIN_JUMPLISTLINK_CID },
  { "@mozilla.org/windows-jumplistshortcut;1", &kNS_WIN_JUMPLISTSHORTCUT_CID },
#endif
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
  nsLookAndFeel::Shutdown();
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
