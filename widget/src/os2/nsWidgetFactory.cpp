/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * ***** END LICENSE BLOCK *****
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by            Description of modification
 * 03/23/2000       IBM Corp.              Added support for directory picker dialog.
 * 03/24/2000       IBM Corp.              Updated based on nsWinWidgetFactory.cpp.
 * 05/31/2000       IBM Corp.              Enabled timer stuff
 * 06/30/2000       sobotka@axess.com      Added nsFilePicker
 * 03/11/2001       achimha@innotek.de     converted to XPCOM module
 * 03/20/2001       achimha@innotek.de     Added class for embedded module init
 * 12/16/2001       pavlov@netscape.com    Removed timer stuff
 */

#include "mozilla/ModuleUtils.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"

// class definition headers
#include "nsAppShell.h"
#include "nsAppShellSingleton.h"
#include "nsBidiKeyboard.h"
#include "nsWindow.h"
#include "nsDragService.h"
#include "nsILocalFile.h"
#include "nsFilePicker.h"
#include "nsLookAndFeel.h"
#include "nsSound.h"
#include "nsToolkit.h"

// Drag & Drop, Clipboard
#include "nsClipboard.h"
#include "nsClipboardHelper.h"
#include "nsTransferable.h"
#include "nsHTMLFormatConverter.h"

#include "nsScreenManagerOS2.h"
#include "nsRwsService.h"

// Printing
#include "nsDeviceContextSpecOS2.h"
#include "nsPrintOptionsOS2.h"
#include "nsPrintSession.h"
#include "nsIdleServiceOS2.h"

// objects that just require generic constructors
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFilePicker)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsChildWindow)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsLookAndFeel,
                                         nsLookAndFeel::GetAddRefedInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsToolkit)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecOS2)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintOptionsOS2, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrinterEnumeratorOS2)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintSession, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerOS2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsIdleServiceOS2)

// component definition, will be exported using XPCOM
NS_DEFINE_NAMED_CID(NS_APPSHELL_CID);
NS_DEFINE_NAMED_CID(NS_BIDIKEYBOARD_CID);
NS_DEFINE_NAMED_CID(NS_CHILD_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARD_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARDHELPER_CID);
NS_DEFINE_NAMED_CID(NS_DRAGSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_FILEPICKER_CID);
NS_DEFINE_NAMED_CID(NS_LOOKANDFEEL_CID);
NS_DEFINE_NAMED_CID(NS_SOUND_CID);
NS_DEFINE_NAMED_CID(NS_TOOLKIT_CID);
NS_DEFINE_NAMED_CID(NS_WINDOW_CID);
NS_DEFINE_NAMED_CID(NS_TRANSFERABLE_CID);
NS_DEFINE_NAMED_CID(NS_HTMLFORMATCONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_SCREENMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_DEVICE_CONTEXT_SPEC_CID);
NS_DEFINE_NAMED_CID(NS_PRINTSETTINGSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_PRINTSESSION_CID);
NS_DEFINE_NAMED_CID(NS_PRINTER_ENUMERATOR_CID);
NS_DEFINE_NAMED_CID(NS_RWSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_IDLE_SERVICE_CID);


static const mozilla::Module::CIDEntry kWidgetCIDs[] = {
    { &kNS_APPSHELL_CID, false, NULL, nsAppShellConstructor },
    { &kNS_BIDIKEYBOARD_CID, false, NULL, nsBidiKeyboardConstructor },
    { &kNS_CHILD_CID, false, NULL, nsChildWindowConstructor },
    { &kNS_CLIPBOARD_CID, false, NULL, nsClipboardConstructor },
    { &kNS_CLIPBOARDHELPER_CID, false, NULL, nsClipboardHelperConstructor },
    { &kNS_DRAGSERVICE_CID, false, NULL, nsDragServiceConstructor },
    { &kNS_FILEPICKER_CID, false, NULL, nsFilePickerConstructor },
    { &kNS_LOOKANDFEEL_CID, false, NULL, nsLookAndFeelConstructor },
    { &kNS_SOUND_CID, false, NULL, nsSoundConstructor },
    { &kNS_TOOLKIT_CID, false, NULL, nsToolkitConstructor },
    { &kNS_WINDOW_CID, false, NULL, nsWindowConstructor },
    { &kNS_TRANSFERABLE_CID, false, NULL, nsTransferableConstructor },
    { &kNS_HTMLFORMATCONVERTER_CID, false, NULL, nsHTMLFormatConverterConstructor },
    { &kNS_SCREENMANAGER_CID, false, NULL, nsScreenManagerOS2Constructor },
    { &kNS_DEVICE_CONTEXT_SPEC_CID, false, NULL, nsDeviceContextSpecOS2Constructor },
    { &kNS_PRINTSETTINGSSERVICE_CID, false, NULL, nsPrintOptionsOS2Constructor },
    { &kNS_PRINTSESSION_CID, false, NULL, nsPrintSessionConstructor },
    { &kNS_PRINTER_ENUMERATOR_CID, false, NULL, nsPrinterEnumeratorOS2Constructor },
    { &kNS_RWSSERVICE_CID, false, NULL, nsRwsServiceConstructor },
    { &kNS_IDLE_SERVICE_CID, false, NULL, nsIdleServiceOS2Constructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kWidgetContracts[] = {
  { "@mozilla.org/widget/appshell/os2;1", &kNS_APPSHELL_CID },
  { "@mozilla.org/widget/bidikeyboard;1", &kNS_BIDIKEYBOARD_CID },
  { "@mozilla.org/widget/child_window/os2;1", &kNS_CHILD_CID },
  { "@mozilla.org/widget/clipboard;1", &kNS_CLIPBOARD_CID },
  { "@mozilla.org/widget/clipboardhelper;1", &kNS_CLIPBOARDHELPER_CID },
  { "@mozilla.org/widget/dragservice;1", &kNS_DRAGSERVICE_CID },
  { "@mozilla.org/filepicker;1", &kNS_FILEPICKER_CID },
  { "@mozilla.org/widget/lookandfeel;1", &kNS_LOOKANDFEEL_CID },
  { "@mozilla.org/sound;1", &kNS_SOUND_CID },
  { "@mozilla.org/widget/toolkit/os2;1", &kNS_TOOLKIT_CID },
  { "@mozilla.org/widget/window/os2;1", &kNS_WINDOW_CID },
  { "@mozilla.org/widget/transferable;1", &kNS_TRANSFERABLE_CID },
  { "@mozilla.org/widget/htmlformatconverter;1", &kNS_HTMLFORMATCONVERTER_CID },
  { "@mozilla.org/gfx/screenmanager;1", &kNS_SCREENMANAGER_CID },
  { "@mozilla.org/gfx/devicecontextspec;1", &kNS_DEVICE_CONTEXT_SPEC_CID },
  { "@mozilla.org/gfx/printsettings-service;1", &kNS_PRINTSETTINGSSERVICE_CID },
  { "@mozilla.org/gfx/printsession;1", &kNS_PRINTSESSION_CID },
  { "@mozilla.org/gfx/printerenumerator;1", &kNS_PRINTER_ENUMERATOR_CID },
  { NS_RWSSERVICE_CONTRACTID, &kNS_RWSSERVICE_CID },
  { "@mozilla.org/widget/idleservice;1", &kNS_IDLE_SERVICE_CID },
  { NULL }
};

static void
nsWidgetOS2ModuleDtor()
{
  nsLookAndFeel::Shutdown();
  nsWindow::ReleaseGlobals();
  nsFilePicker::ReleaseGlobals();
  nsAppShellShutdown();
}

static const mozilla::Module kWidgetModule = {
  mozilla::Module::kVersion,
  kWidgetCIDs,
  kWidgetContracts,
  NULL,
  NULL,
  nsAppShellInit,
  nsWidgetOS2ModuleDtor
};

NSMODULE_DEFN(nsWidgetOS2Module) = &kWidgetModule;
