/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  John C. Griggs <johng@corel.com>
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Zack Rusin <zack@kde.org>
 *   Lars Knoll <knoll@kde.org>
 *   John C. Griggs <johng@corel.com>
 *   Dan Rosen <dr@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"
#include "nsIComponentRegistrar.h"
#include "nsComponentManagerUtils.h"
#include "nsAutoPtr.h"

#include "nsWindow.h"
#include "nsAppShell.h"
#include "nsToolkit.h"
#include "nsLookAndFeel.h"
#include "nsTransferable.h"
#include "nsClipboard.h"
#include "nsClipboardHelper.h"
#include "nsHTMLFormatConverter.h"
#include "nsDragService.h"
#include "nsScrollbar.h"
#include "nsFilePicker.h"
#include "nsSound.h"

#include "nsGUIEvent.h"
#include "nsQtEventDispatcher.h"
#include "nsIRenderingContext.h"
#include "nsIServiceManager.h"
#include "nsGfxCIID.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

#include "nsBidiKeyboard.h"

static NS_DEFINE_CID(kNativeScrollCID, NS_NATIVESCROLLBAR_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(ChildWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(PopupWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAppShell)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsToolkit)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLookAndFeel)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNativeScrollbar)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFilePicker)

static const nsModuleComponentInfo components[] =
{
    { "Qt nsWindow",
      NS_WINDOW_CID,
      "@mozilla.org/widgets/window/qt;1",
      nsWindowConstructor },
    { "Qt Child nsWindow",
      NS_CHILD_CID,
      "@mozilla.org/widgets/child_window/qt;1",
      ChildWindowConstructor },
    { "Qt Popup nsWindow",
      NS_POPUP_CID,
      "@mozilla.org/widgets/popup_window/qt;1",
      PopupWindowConstructor },
    { "Qt Native Scrollbar",
      NS_NATIVESCROLLBAR_CID,
      "@mozilla.org/widget/nativescrollbar/qt;1",
      nsNativeScrollbarConstructor},
    { "Qt AppShell",
      NS_APPSHELL_CID,
      "@mozilla.org/widget/appshell/qt;1",
      nsAppShellConstructor },
    { "Qt Toolkit",
      NS_TOOLKIT_CID,
      "@mozilla.org/widget/toolkit/qt;1",
      nsToolkitConstructor },
    { "Qt Look And Feel",
      NS_LOOKANDFEEL_CID,
      "@mozilla.org/widget/lookandfeel/qt;1",
      nsLookAndFeelConstructor },
    { "Transferrable",
      NS_TRANSFERABLE_CID,
      "@mozilla.org/widget/transferable;1",
      nsTransferableConstructor },
    { "Qt Clipboard",
      NS_CLIPBOARD_CID,
      "@mozilla.org/widget/clipboard;1",
      nsClipboardConstructor },
    { "Clipboard Helper",
      NS_CLIPBOARDHELPER_CID,
      "@mozilla.org/widget/clipboardhelper;1",
      nsClipboardHelperConstructor },
    { "HTML Format Converter",
      NS_HTMLFORMATCONVERTER_CID,
      "@mozilla.org/widget/htmlformatconverter/qt;1",
      nsHTMLFormatConverterConstructor },
    { "Qt Drag Service",
      NS_DRAGSERVICE_CID,
      "@mozilla.org/widget/dragservice;1",
      nsDragServiceConstructor },
    { "Qt Bidi Keyboard",
      NS_BIDIKEYBOARD_CID,
      "@mozilla.org/widget/bidikeyboard;1",
      nsBidiKeyboardConstructor },
    { "Qt Sound",
      NS_SOUND_CID,
      "@mozilla.org/sound;1",
      nsSoundConstructor },
    { "Qt File Picker",
      NS_FILEPICKER_CID,
      "@mozilla.org/filepicker;1",
      nsFilePickerConstructor }
};

NS_IMPL_NSGETMODULE(nsWidgetQtModule,components)
