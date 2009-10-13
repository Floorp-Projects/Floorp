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
#include "nsIGenericFactory.h"

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

#include "nsAccelerometerX.h"
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
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAccelerometerX)
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

static const nsModuleComponentInfo gComponents[] =
{
  { "nsWindow",
    NS_WINDOW_CID,
    "@mozilla.org/widgets/window/mac;1",
    nsCocoaWindowConstructor },
  { "Popup nsWindow",
    NS_POPUP_CID,
    "@mozilla.org/widgets/popup/mac;1",
    nsCocoaWindowConstructor },
  { "Child nsWindow",
    NS_CHILD_CID,
    "@mozilla.org/widgets/childwindow/mac;1",
    nsChildViewConstructor },
  { "File Picker",
    NS_FILEPICKER_CID,
    "@mozilla.org/filepicker;1",
    nsFilePickerConstructor },
  { "AppShell",
    NS_APPSHELL_CID,
    "@mozilla.org/widget/appshell/mac;1",
    nsAppShellConstructor },
  { "Toolkit",
    NS_TOOLKIT_CID,
    "@mozilla.org/widget/toolkit/mac;1",
    nsToolkitConstructor },
  { "Look And Feel",
    NS_LOOKANDFEEL_CID,
    "@mozilla.org/widget/lookandfeel;1",
    nsLookAndFeelConstructor },
  { "Sound",
    NS_SOUND_CID,
    "@mozilla.org/sound;1",
    nsSoundConstructor },
  { "Accelerometer",
    NS_ACCELEROMETER_CID,
    NS_ACCELEROMETER_CONTRACTID,
    nsAccelerometerXConstructor },
  { "Transferable",
    NS_TRANSFERABLE_CID,
    "@mozilla.org/widget/transferable;1",
    nsTransferableConstructor },
  { "HTML Format Converter",
    NS_HTMLFORMATCONVERTER_CID,
    "@mozilla.org/widget/htmlformatconverter;1",
    nsHTMLFormatConverterConstructor },
  { "Clipboard",
    NS_CLIPBOARD_CID,
    "@mozilla.org/widget/clipboard;1",
    nsClipboardConstructor },
  { "Clipboard Helper",
    NS_CLIPBOARDHELPER_CID,
    "@mozilla.org/widget/clipboardhelper;1",
    nsClipboardHelperConstructor },
  { "Drag Service",
    NS_DRAGSERVICE_CID,
    "@mozilla.org/widget/dragservice;1",
    nsDragServiceConstructor },
  { "Cocoa Bidi Keyboard",
    NS_BIDIKEYBOARD_CID,
    "@mozilla.org/widget/bidikeyboard;1",
    nsBidiKeyboardConstructor },
  { "Native Theme Renderer", 
    NS_THEMERENDERER_CID,
    "@mozilla.org/chrome/chrome-native-theme;1",
    nsNativeThemeCocoaConstructor },
  { "nsScreenManager",
    NS_SCREENMANAGER_CID,
    "@mozilla.org/gfx/screenmanager;1",
    nsScreenManagerCocoaConstructor },
  { "nsDeviceContextSpec",
     NS_DEVICE_CONTEXT_SPEC_CID,
     "@mozilla.org/gfx/devicecontextspec;1",
     nsDeviceContextSpecXConstructor },
  { "Print Session",
    NS_PRINTSESSION_CID,
    "@mozilla.org/gfx/printsession;1",
    nsPrintSessionConstructor },
  { "PrintSettings Service",
     NS_PRINTSETTINGSSERVICE_CID,
     "@mozilla.org/gfx/printsettings-service;1",
     nsPrintOptionsXConstructor },
  { "Native Print Dialog",
    NS_PRINTDIALOGSERVICE_CID,
    NS_PRINTDIALOGSERVICE_CONTRACTID,
    nsPrintDialogServiceXConstructor },
  { "User Idle Service",
    NS_IDLE_SERVICE_CID,
    "@mozilla.org/widget/idleservice;1",
    nsIdleServiceXConstructor },
  { "Native Menu Service",
    NS_NATIVEMENUSERVICE_CID,
    "@mozilla.org/widget/nativemenuservice;1",
    nsNativeMenuServiceXConstructor },
};

NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(nsWidgetMacModule, gComponents,
                                   nsAppShellInit, nsAppShellShutdown)
