/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Dan Rosen <dr@netscape.com>
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

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"

//#include "nsWidgetDefs.h"

// class definition headers
#include "nsAppShell.h"
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

#include "nsFrameWindow.h" // OS/2 only

// objects that just require generic constructors
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFilePicker)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAppShell)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFrameWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLookAndFeel)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsToolkit)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)

// component definition, will be exported using XPCOM
static const nsModuleComponentInfo components[] =
{
  { "OS/2 AppShell",
    NS_APPSHELL_CID,
    "@mozilla.org/widget/appshell/os2;1",
    nsAppShellConstructor },
  { "OS/2 Bidi Keyboard",
    NS_BIDIKEYBOARD_CID,
    "@mozilla.org/widget/bidikeyboard;1",
    nsBidiKeyboardConstructor },
  { "OS/2 Child Window",
    NS_CHILD_CID,
    "@mozilla.org/widget/child_window/os2;1",
    nsWindowConstructor },
  { "OS/2 Clipboard",
    NS_CLIPBOARD_CID,
    "@mozilla.org/widget/clipboard;1",
    nsClipboardConstructor },
  { "Clipboard Helper",
    NS_CLIPBOARDHELPER_CID,
    "@mozilla.org/widget/clipboardhelper;1",
    nsClipboardHelperConstructor },
  { "OS/2 Drag Service",
    NS_DRAGSERVICE_CID,
    "@mozilla.org/widget/dragservice;1",
    nsDragServiceConstructor },
  { "OS/2 File Picker",
    NS_FILEPICKER_CID,
    "@mozilla.org/filepicker;1",
    nsFilePickerConstructor },
  { "OS/2 Look And Feel",
    NS_LOOKANDFEEL_CID,
    "@mozilla.org/widget/lookandfeel;1",
    nsLookAndFeelConstructor },
  { "OS/2 Sound",
    NS_SOUND_CID,
    "@mozilla.org/sound;1",
    nsSoundConstructor },
  { "OS/2 Toolkit",
    NS_TOOLKIT_CID,
    "@mozilla.org/widget/toolkit/os2;1",
    nsToolkitConstructor },
  { "OS/2 Frame Window",
    NS_WINDOW_CID,
    "@mozilla.org/widget/window/os2;1",
    nsFrameWindowConstructor },
  { "OS/2 Transferable",
    NS_TRANSFERABLE_CID,
    "@mozilla.org/widget/transferable;1",
    nsTransferableConstructor },
  { "OS/2 HTML Format Converter",
    NS_HTMLFORMATCONVERTER_CID,
    "@mozilla.org/widget/htmlformatconverter;1",
    nsHTMLFormatConverterConstructor }
};

PR_STATIC_CALLBACK(void)
nsWidgetOS2ModuleDtor(nsIModule *self)
{
  nsWindow::ReleaseGlobals();
  nsFilePicker::ReleaseGlobals();
}

NS_IMPL_NSGETMODULE_WITH_DTOR(nsWidgetOS2Module,
                              components,
                              nsWidgetOS2ModuleDtor)
