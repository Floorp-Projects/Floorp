/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Christopher Blizzard.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John C. Griggs <johng@corel.com>
 *   Dan Rosen <dr@netscape.com>
 *   Paul Ashford <arougthopher@lizardland.net>
 *   Fredrik Holmqvist <thesuckiestemail@yahoo.se>
 *   Sergei Dolgov <sergei_d@fi.tartu.ee>
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

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"

#include "nsWindow.h"
#include "nsPopupWindow.h"
#include "nsChildView.h"
#include "nsSound.h"
#include "nsToolkit.h"
#include "nsAppShell.h"
#include "nsAppShellSingleton.h"
#include "nsLookAndFeel.h"
#include "nsFilePicker.h"
#include "nsBidiKeyboard.h"
#include "nsScreenManagerBeOS.h" 
// Printing:
// aka    nsDeviceContextSpecBeOS.h 
#include "nsDeviceContextSpecB.h"
#include "nsPrintOptionsBeOS.h"
#include "nsPrintSession.h"

// Drag & Drop, Clipboard
#include "nsTransferable.h"
#include "nsClipboard.h"
#include "nsClipboardHelper.h"
#include "nsHTMLFormatConverter.h"
#include "nsDragService.h"


NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPopupWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsChildView)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsToolkit)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLookAndFeel)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFilePicker)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerBeOS)
 
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecBeOS) 
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintOptionsBeOS, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintSession, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrinterEnumeratorBeOS)

static const nsModuleComponentInfo components[] =
{
  { "BeOS nsWindow",
    NS_WINDOW_CID,
    "@mozilla.org/widgets/window/beos;1",
    nsWindowConstructor },
  { "BeOS Popup nsWindow",
    NS_POPUP_CID,
    "@mozilla.org/widgets/popup/beos;1",
    nsPopupWindowConstructor },
  { "BeOS Child nsWindow",
    NS_CHILD_CID,
    "@mozilla.org/widgets/view/beos;1",
    nsChildViewConstructor },
  { "BeOS AppShell",
    NS_APPSHELL_CID,
    "@mozilla.org/widget/appshell/beos;1",
    nsAppShellConstructor },
  { "BeOS Toolkit",
    NS_TOOLKIT_CID,
    "@mozilla.org/widget/toolkit/beos;1",
    nsToolkitConstructor },
  { "BeOS Look And Feel",
    NS_LOOKANDFEEL_CID,
    "@mozilla.org/widget/lookandfeel;1",
    nsLookAndFeelConstructor },
  { "Transferrable",
    NS_TRANSFERABLE_CID,
    "@mozilla.org/widget/transferable;1",
    nsTransferableConstructor },
  { "BeOS Clipboard",
    NS_CLIPBOARD_CID,
    "@mozilla.org/widget/clipboard;1",
    nsClipboardConstructor },
  { "Clipboard Helper",
    NS_CLIPBOARDHELPER_CID,
    "@mozilla.org/widget/clipboardhelper;1",
    nsClipboardHelperConstructor },
  { "HTML Format Converter",
    NS_HTMLFORMATCONVERTER_CID,
    "@mozilla.org/widget/htmlformatconverter;1",
    nsHTMLFormatConverterConstructor },
  { "BeOS Sound",
    NS_SOUND_CID,
    "@mozilla.org/sound;1",
    nsSoundConstructor },
  { "BeOS Drag Service",
    NS_DRAGSERVICE_CID,
    "@mozilla.org/widget/dragservice;1",
    nsDragServiceConstructor },
  { "BeOS Bidi Keyboard",
    NS_BIDIKEYBOARD_CID,
    "@mozilla.org/widget/bidikeyboard;1",
    nsBidiKeyboardConstructor },
  { "BeOS File Picker",
    NS_FILEPICKER_CID,
    "@mozilla.org/filepicker;1",
   nsFilePickerConstructor },
  { "BeOS Screen Manager", 
    NS_SCREENMANAGER_CID, 
    "@mozilla.org/gfx/screenmanager;1", 
    nsScreenManagerBeOSConstructor },
  { "BeOS Device Context Spec", 
    NS_DEVICE_CONTEXT_SPEC_CID, 
    //    "@mozilla.org/gfx/device_context_spec/beos;1", 
    "@mozilla.org/gfx/devicecontextspec;1", 
    nsDeviceContextSpecBeOSConstructor }, 
  { "BeOS Printer Enumerator",
    NS_PRINTER_ENUMERATOR_CID,
    //    "@mozilla.org/gfx/printer_enumerator/beos;1",
    "@mozilla.org/gfx/printerenumerator;1",
    nsPrinterEnumeratorBeOSConstructor },
  { "BeOS PrintSettings Service",
    NS_PRINTSETTINGSSERVICE_CID,
    "@mozilla.org/gfx/printsettings-service;1",
    nsPrintOptionsBeOSConstructor },
  { "Print Session",
    NS_PRINTSESSION_CID,
    "@mozilla.org/gfx/printsession;1",
    nsPrintSessionConstructor }
};

NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(nsWidgetBeOSModule,
                              components,
                              nsAppShellInit, nsAppShellShutdown)
