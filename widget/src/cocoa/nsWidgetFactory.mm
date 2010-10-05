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
#include "nsIComponentManager.h"
#include "mozilla/ModuleUtils.h"

#include "nsWidgetsCID.h"

#include "nsToolkit.h"
#include "nsChildView.h"
#include "nsCocoaWindow.h"
#include "nsAppShell.h"
#include "nsAppShellSingleton.h"
#include "nsFilePicker.h"

#include "nsClipboard.h"
#include "nsClipboardHelper.h"
#include "nsTransferable.h"
#include "nsHTMLFormatConverter.h"
#include "nsDragService.h"

#include "nsLookAndFeel.h"

#include "nsSound.h"
#include "nsIdleServiceX.h"

#include "nsScreenManagerCocoa.h"
#include "nsDeviceContextSpecX.h"
#include "nsPrintOptionsX.h"
#include "nsPrintDialogX.h"
#include "nsPrintSession.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCocoaWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsChildView)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFilePicker)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsToolkit)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLookAndFeel)
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
NS_GENERIC_FACTORY_CONSTRUCTOR(nsIdleServiceX)

#include "nsMenuBarX.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNativeMenuServiceX)

#include "nsBidiKeyboard.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)

#include "nsNativeThemeCocoa.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNativeThemeCocoa)

#include "nsMacDockSupport.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacDockSupport)

#include "nsStandaloneNativeMenu.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStandaloneNativeMenu)

#include "GfxInfo.h"
namespace mozilla {
namespace widget {
NS_GENERIC_FACTORY_CONSTRUCTOR(GfxInfo)
}
}


NS_DEFINE_NAMED_CID(NS_WINDOW_CID);
NS_DEFINE_NAMED_CID(NS_POPUP_CID);
NS_DEFINE_NAMED_CID(NS_CHILD_CID);
NS_DEFINE_NAMED_CID(NS_FILEPICKER_CID);
NS_DEFINE_NAMED_CID(NS_APPSHELL_CID);
NS_DEFINE_NAMED_CID(NS_TOOLKIT_CID);
NS_DEFINE_NAMED_CID(NS_LOOKANDFEEL_CID);
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
NS_DEFINE_NAMED_CID(NS_NATIVEMENUSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_MACDOCKSUPPORT_CID);
NS_DEFINE_NAMED_CID(NS_STANDALONENATIVEMENU_CID);
NS_DEFINE_NAMED_CID(NS_GFXINFO_CID);


static const mozilla::Module::CIDEntry kWidgetCIDs[] = {
  { &kNS_WINDOW_CID, false, NULL, nsCocoaWindowConstructor },
  { &kNS_POPUP_CID, false, NULL, nsCocoaWindowConstructor },
  { &kNS_CHILD_CID, false, NULL, nsChildViewConstructor },
  { &kNS_FILEPICKER_CID, false, NULL, nsFilePickerConstructor },
  { &kNS_APPSHELL_CID, false, NULL, nsAppShellConstructor },
  { &kNS_TOOLKIT_CID, false, NULL, nsToolkitConstructor },
  { &kNS_LOOKANDFEEL_CID, false, NULL, nsLookAndFeelConstructor },
  { &kNS_SOUND_CID, false, NULL, nsSoundConstructor },
  { &kNS_TRANSFERABLE_CID, false, NULL, nsTransferableConstructor },
  { &kNS_HTMLFORMATCONVERTER_CID, false, NULL, nsHTMLFormatConverterConstructor },
  { &kNS_CLIPBOARD_CID, false, NULL, nsClipboardConstructor },
  { &kNS_CLIPBOARDHELPER_CID, false, NULL, nsClipboardHelperConstructor },
  { &kNS_DRAGSERVICE_CID, false, NULL, nsDragServiceConstructor },
  { &kNS_BIDIKEYBOARD_CID, false, NULL, nsBidiKeyboardConstructor },
  { &kNS_THEMERENDERER_CID, false, NULL, nsNativeThemeCocoaConstructor },
  { &kNS_SCREENMANAGER_CID, false, NULL, nsScreenManagerCocoaConstructor },
  { &kNS_DEVICE_CONTEXT_SPEC_CID, false, NULL, nsDeviceContextSpecXConstructor },
  { &kNS_PRINTSESSION_CID, false, NULL, nsPrintSessionConstructor },
  { &kNS_PRINTSETTINGSSERVICE_CID, false, NULL, nsPrintOptionsXConstructor },
  { &kNS_PRINTDIALOGSERVICE_CID, false, NULL, nsPrintDialogServiceXConstructor },
  { &kNS_IDLE_SERVICE_CID, false, NULL, nsIdleServiceXConstructor },
  { &kNS_NATIVEMENUSERVICE_CID, false, NULL, nsNativeMenuServiceXConstructor },
  { &kNS_MACDOCKSUPPORT_CID, false, NULL, nsMacDockSupportConstructor },
  { &kNS_STANDALONENATIVEMENU_CID, false, NULL, nsStandaloneNativeMenuConstructor },
  { &kNS_GFXINFO_CID, false, NULL, mozilla::widget::GfxInfoConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kWidgetContracts[] = {
  { "@mozilla.org/widgets/window/mac;1", &kNS_WINDOW_CID },
  { "@mozilla.org/widgets/popup/mac;1", &kNS_POPUP_CID },
  { "@mozilla.org/widgets/childwindow/mac;1", &kNS_CHILD_CID },
  { "@mozilla.org/filepicker;1", &kNS_FILEPICKER_CID },
  { "@mozilla.org/widget/appshell/mac;1", &kNS_APPSHELL_CID },
  { "@mozilla.org/widget/toolkit/mac;1", &kNS_TOOLKIT_CID },
  { "@mozilla.org/widget/lookandfeel;1", &kNS_LOOKANDFEEL_CID },
  { "@mozilla.org/sound;1", &kNS_SOUND_CID },
  { "@mozilla.org/widget/transferable;1", &kNS_TRANSFERABLE_CID },
  { "@mozilla.org/widget/htmlformatconverter;1", &kNS_HTMLFORMATCONVERTER_CID },
  { "@mozilla.org/widget/clipboard;1", &kNS_CLIPBOARD_CID },
  { "@mozilla.org/widget/clipboardhelper;1", &kNS_CLIPBOARDHELPER_CID },
  { "@mozilla.org/widget/dragservice;1", &kNS_DRAGSERVICE_CID },
  { "@mozilla.org/widget/bidikeyboard;1", &kNS_BIDIKEYBOARD_CID },
  { "@mozilla.org/chrome/chrome-native-theme;1", &kNS_THEMERENDERER_CID },
  { "@mozilla.org/gfx/screenmanager;1", &kNS_SCREENMANAGER_CID },
  { "@mozilla.org/gfx/devicecontextspec;1", &kNS_DEVICE_CONTEXT_SPEC_CID },
  { "@mozilla.org/gfx/printsession;1", &kNS_PRINTSESSION_CID },
  { "@mozilla.org/gfx/printsettings-service;1", &kNS_PRINTSETTINGSSERVICE_CID },
  { NS_PRINTDIALOGSERVICE_CONTRACTID, &kNS_PRINTDIALOGSERVICE_CID },
  { "@mozilla.org/widget/idleservice;1", &kNS_IDLE_SERVICE_CID },
  { "@mozilla.org/widget/nativemenuservice;1", &kNS_NATIVEMENUSERVICE_CID },
  { "@mozilla.org/widget/macdocksupport;1", &kNS_MACDOCKSUPPORT_CID },
  { "@mozilla.org/widget/standalonenativemenu;1", &kNS_STANDALONENATIVEMENU_CID },
  { "@mozilla.org/gfx/info;1", &kNS_GFXINFO_CID },
  { NULL }
};

static const mozilla::Module kWidgetModule = {
  mozilla::Module::kVersion,
  kWidgetCIDs,
  kWidgetContracts,
  NULL,
  NULL,
  nsAppShellInit,
  nsAppShellShutdown
};

NSMODULE_DEFN(nsWidgetMacModule) = &kWidgetModule;
