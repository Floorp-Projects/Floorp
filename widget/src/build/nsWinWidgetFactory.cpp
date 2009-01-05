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
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIdleServiceWin.h"
#include "nsLookAndFeel.h"
#include "nsNativeThemeWin.h"
#include "nsScreenManagerWin.h"
#include "nsSound.h"
#include "nsToolkit.h"
#include "nsWindow.h"

// Drag & Drop, Clipboard
#ifndef WINCE
#include "nsBidiKeyboard.h"
#include "nsClipboard.h"
#include "nsClipboardHelper.h"
#include "nsDragService.h"
#include "nsHTMLFormatConverter.h"
#include "nsTransferable.h"
#endif

#ifdef NS_PRINTING
#include "nsDeviceContextSpecWin.h"
#include "nsPrintOptionsWin.h"
#include "nsPrintSession.h"
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(ChildWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFilePicker)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLookAndFeel)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNativeThemeWin)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsToolkit)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerWin)

#ifndef WINCE

NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsIdleServiceWin)
#endif

#ifdef NS_PRINTING
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintOptionsWin, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrinterEnumeratorWin)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintSession, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecWin)
#endif

static const nsModuleComponentInfo components[] =
{
  { "nsWindow",
    NS_WINDOW_CID,
    "@mozilla.org/widgets/window/win;1",
    nsWindowConstructor },
  { "Child nsWindow",
    NS_CHILD_CID,
    "@mozilla.org/widgets/child_window/win;1",
    ChildWindowConstructor },
  { "File Picker",
    NS_FILEPICKER_CID,
    "@mozilla.org/filepicker;1",
    nsFilePickerConstructor },
  { "AppShell",
    NS_APPSHELL_CID,
    "@mozilla.org/widget/appshell/win;1",
    nsAppShellConstructor },
  { "Toolkit",
    NS_TOOLKIT_CID,
    "@mozilla.org/widget/toolkit/win;1",
    nsToolkitConstructor },
  { "nsScreenManagerWin",
    NS_SCREENMANAGER_CID,
    "@mozilla.org/gfx/screenmanager;1",
    nsScreenManagerWinConstructor },
  { "Look And Feel",
    NS_LOOKANDFEEL_CID,
    "@mozilla.org/widget/lookandfeel;1",
    nsLookAndFeelConstructor },
  { "Native Theme Renderer", 
    NS_THEMERENDERER_CID,
    "@mozilla.org/chrome/chrome-native-theme;1", 
    NS_NewNativeTheme
  },

#ifndef WINCE
  { "Clipboard",
    NS_CLIPBOARD_CID,
    "@mozilla.org/widget/clipboard;1",
    nsClipboardConstructor },
  { "Clipboard Helper",
    NS_CLIPBOARDHELPER_CID,
    "@mozilla.org/widget/clipboardhelper;1",
    nsClipboardHelperConstructor },
  { "Sound",
    NS_SOUND_CID,
    "@mozilla.org/sound;1",
    nsSoundConstructor },
  { "Drag Service",
    NS_DRAGSERVICE_CID,
    "@mozilla.org/widget/dragservice;1",
    nsDragServiceConstructor },
  { "Bidi Keyboard",
    NS_BIDIKEYBOARD_CID,
    "@mozilla.org/widget/bidikeyboard;1",
    nsBidiKeyboardConstructor },
  { "User Idle Service",
    NS_IDLE_SERVICE_CID,
    "@mozilla.org/widget/idleservice;1",
    nsIdleServiceWinConstructor },
  { "Transferable",
    NS_TRANSFERABLE_CID,
    "@mozilla.org/widget/transferable;1",
    nsTransferableConstructor },
  { "HTML Format Converter",
    NS_HTMLFORMATCONVERTER_CID,
    "@mozilla.org/widget/htmlformatconverter;1",
    nsHTMLFormatConverterConstructor },
#endif

#ifdef NS_PRINTING
  { "nsPrintOptionsWin",
    NS_PRINTSETTINGSSERVICE_CID,
    "@mozilla.org/gfx/printsettings-service;1",
    nsPrintOptionsWinConstructor },
  { "Win Printer Enumerator",
    NS_PRINTER_ENUMERATOR_CID,
    "@mozilla.org/gfx/printerenumerator;1",
    nsPrinterEnumeratorWinConstructor },
  { "Print Session",
    NS_PRINTSESSION_CID,
    "@mozilla.org/gfx/printsession;1",
    nsPrintSessionConstructor },
  { "nsDeviceContextSpecWin",
    NS_DEVICE_CONTEXT_SPEC_CID,
    "@mozilla.org/gfx/devicecontextspec;1",
    nsDeviceContextSpecWinConstructor },
#endif
};

NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(nsWidgetModule, components,
                                   nsAppShellInit, nsAppShellShutdown)
