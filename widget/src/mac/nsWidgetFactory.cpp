/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIButton.h"
#include "nsITextWidget.h"
#include "nsWidgetsCID.h"

#include "nsToolkit.h"
#include "nsWindow.h"
#include "nsMacWindow.h"
#include "nsAppShell.h"
#include "nsButton.h"
#include "nsTextWidget.h"
#include "nsLabel.h"
#include "nsFilePicker.h"
#include "nsScrollbar.h"

#if TARGET_CARBON
#include "nsMenuBarX.h"
#include "nsMenuX.h"
#include "nsMenuItemX.h"

#define nsMenuBar nsMenuBarX
#define nsMenu nsMenuX
#define nsMenuItem nsMenuItemX

#else
#include "nsMenuBar.h"
#include "nsMenu.h"
#include "nsMenuItem.h"
#endif

#include "nsClipboard.h"
#include "nsClipboardHelper.h"
#include "nsTransferable.h"
#include "nsHTMLFormatConverter.h"
#include "nsDragService.h"

#if USE_NATIVE_VERSION
# include "nsCheckButton.h"
#endif

#include "nsLookAndFeel.h"

#include "nsIComponentManager.h"

#include "nsSound.h"
#include "nsTimerMac.h"

#ifdef IBMBIDI
#include "nsBidiKeyboard.h"
#endif

#include "nsIGenericFactory.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsTimerImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(ChildWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsButton)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFilePicker)
#if USE_NATIVE_VERSION
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCheckButton)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsComboBox)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRadioButton)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsListBox)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTextAreaWidget)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHorizScrollbar)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsVertScrollbar)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTextWidget)	// used by Viewer?
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAppShell)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsToolkit)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLookAndFeel)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLabel)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMenuBar)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMenu)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMenuItem)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)
#ifdef IBMBIDI
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)
#endif

static nsModuleComponentInfo components[] =
{
	{	"Timer",
		NS_TIMER_CID,
		"@mozilla.org/timer;1",
		nsTimerImplConstructor },
	{	"nsWindow",
		NS_WINDOW_CID,
		"@mozilla.org/widgets/window/mac;1",
		nsMacWindowConstructor },
	{	"Popup nsWindow",
		NS_POPUP_CID,
		"@mozilla.org/widgets/popup/mac;1",
		nsMacWindowConstructor },
	{	"Child nsWindow",
		NS_CHILD_CID,
		"@mozilla.org/widgets/child_window/mac;1",
		ChildWindowConstructor },
	{	"Button",
		NS_BUTTON_CID,
		"@mozilla.org/widgets/button/mac;1",
		nsButtonConstructor },
	{	"File Picker",
		NS_FILEPICKER_CID,
		"@mozilla.org/filepicker;1",
		nsFilePickerConstructor },
	{	"Horiz Scrollbar",
		NS_HORZSCROLLBAR_CID,
		"@mozilla.org/widgets/horizscroll/mac;1",
		nsHorizScrollbarConstructor },
	{	"Vert Scrollbar",
		NS_VERTSCROLLBAR_CID,
		"@mozilla.org/widgets/vertscroll/mac;1",
		nsVertScrollbarConstructor },
	{ 	"Text Field",
		NS_TEXTFIELD_CID,
		"@mozilla.org/widgets/textwidget/mac;1",
		nsTextWidgetConstructor },
	{	"AppShell",
		NS_APPSHELL_CID,
		"@mozilla.org/widget/appshell/mac;1",
		nsAppShellConstructor },
	{	"Toolkit",
		NS_TOOLKIT_CID,
		"@mozilla.org/widget/toolkit/mac;1",
		nsToolkitConstructor },
	{	"Look And Feel",
		NS_LOOKANDFEEL_CID,
		"@mozilla.org/widget/lookandfeel/mac;1",
		nsLookAndFeelConstructor },
	{	"Label",
		NS_LABEL_CID,
		"@mozilla.org/widget/label/mac;1",
		nsLabelConstructor },
	{	"Menubar",
		NS_MENUBAR_CID,
		"@mozilla.org/widget/menubar/mac;1",
		nsMenuBarConstructor },
	{	"Menu",
		NS_MENU_CID,
		"@mozilla.org/widget/menu/mac;1",
		nsMenuConstructor },
	{	"MenuItem",
		NS_MENUITEM_CID,
		"@mozilla.org/widget/menuitem/mac;1",
		nsMenuItemConstructor },
	{ 	"Sound",
		NS_SOUND_CID,
		"@mozilla.org/sound;1",
		nsSoundConstructor },
	{	"Transferable",
		NS_TRANSFERABLE_CID,
		"@mozilla.org/widget/transferable;1",
		nsTransferableConstructor },
	{	"HTML Format Converter",
		NS_HTMLFORMATCONVERTER_CID,
		"@mozilla.org/widget/htmlformatconverter/mac;1",
		nsHTMLFormatConverterConstructor },
	{	"Clipboard",
		NS_CLIPBOARD_CID,
		"@mozilla.org/widget/clipboard;1",
		nsClipboardConstructor },
	{	"Clipboard Helper",
		NS_CLIPBOARDHELPER_CID,
		"@mozilla.org/widget/clipboardhelper;1",
		nsClipboardHelperConstructor },
	{	"Drag Service",
		NS_DRAGSERVICE_CID,
		"@mozilla.org/widget/dragservice;1",
		nsDragServiceConstructor },
#ifdef IBMBIDI
	{	"Gtk Bidi Keyboard",
		NS_BIDIKEYBOARD_CID,
		"@mozilla.org/widget/bidikeyboard;1",
		nsBidiKeyboardConstructor },
#endif // IBMBIDI
};

NS_IMPL_NSGETMODULE(nsWidgetModule, components)

