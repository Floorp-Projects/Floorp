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
#include "nsWidgetsCID.h"

#include "nsToolkit.h"
#include "nsChildView.h"
#include "nsCocoaWindow.h"
#include "nsAppShellCocoa.h"
#include "nsFilePicker.h"

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

#ifdef XP_MACOSX

#include "nsIGenericFactory.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsTimerImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCocoaWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsChildView)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFilePicker)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAppShellCocoa)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsToolkit)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLookAndFeel)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMenuBar)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMenu)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMenuItem)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsFileSpecWithUIImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
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
		nsCocoaWindowConstructor },
	{	"Popup nsWindow",
		NS_POPUP_CID,
		"@mozilla.org/widgets/popup/mac;1",
		nsCocoaWindowConstructor },
	{	"Child nsWindow",
		NS_CHILD_CID,
		"@mozilla.org/widgets/childwindow/mac;1",
		nsChildViewConstructor },
	{	"File Picker",
		NS_FILEPICKER_CID,
		"@mozilla.org/filepicker;1",
		nsFilePickerConstructor },
	{	"AppShell",
		NS_APPSHELL_CID,
		"@mozilla.org/widget/appshell/mac;1",
		nsAppShellCocoaConstructor },
	{	"Toolkit",
		NS_TOOLKIT_CID,
		"@mozilla.org/widget/toolkit/mac;1",
		nsToolkitConstructor },
	{	"Look And Feel",
		NS_LOOKANDFEEL_CID,
		"@mozilla.org/widget/lookandfeel/mac;1",
		nsLookAndFeelConstructor },
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
		{ "Gtk Bidi Keyboard",
		NS_BIDIKEYBOARD_CID,
		"@mozilla.org/widget/bidikeyboard;1",
		nsBidiKeyboardConstructor },
#endif // IBMBIDI
};

NS_IMPL_NSGETMODULE(nsWidgetModule, components)

#else

// NOTE the following does not match MAC_STATIC actually used below in this file!
#define MACSTATIC

static NS_DEFINE_CID(kCWindow,        NS_WINDOW_CID);
static NS_DEFINE_IID(kCTimer,         NS_TIMER_CID);
static NS_DEFINE_CID(kCPopUp,         NS_POPUP_CID);
static NS_DEFINE_CID(kCChild,         NS_CHILD_CID);
static NS_DEFINE_CID(kCButton,        NS_BUTTON_CID);
static NS_DEFINE_CID(kCCheckButton,   NS_CHECKBUTTON_CID);
static NS_DEFINE_CID(kCFilePicker,    NS_FILEPICKER_CID);
static NS_DEFINE_CID(kCHorzScrollbar, NS_HORZSCROLLBAR_CID);
static NS_DEFINE_CID(kCVertScrollbar, NS_VERTSCROLLBAR_CID);
static NS_DEFINE_CID(kCTextField,     NS_TEXTFIELD_CID);
static NS_DEFINE_CID(kCAppShell,      NS_APPSHELL_CID);
static NS_DEFINE_CID(kCToolkit,       NS_TOOLKIT_CID);
static NS_DEFINE_CID(kCLookAndFeel,   NS_LOOKANDFEEL_CID);
static NS_DEFINE_CID(kCLabel,         NS_LABEL_CID);
static NS_DEFINE_CID(kCMenuBar,       NS_MENUBAR_CID);
static NS_DEFINE_CID(kCMenu,          NS_MENU_CID);
static NS_DEFINE_CID(kCMenuItem,      NS_MENUITEM_CID);
static NS_DEFINE_CID(kCPopUpMenu,     NS_POPUPMENU_CID);

// Drag and Drop/Clipboard
static NS_DEFINE_CID(kCDataFlavor,    NS_DATAFLAVOR_CID);
static NS_DEFINE_CID(kCClipboard,     NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCClipboardHelper,  NS_CLIPBOARDHELPER_CID);
static NS_DEFINE_CID(kCTransferable,  NS_TRANSFERABLE_CID);
static NS_DEFINE_CID(kCHTMLFormatConverter,  NS_HTMLFORMATCONVERTER_CID);
static NS_DEFINE_CID(kCDragService,   NS_DRAGSERVICE_CID);

// Sound services (just Beep for now)
static NS_DEFINE_CID(kCSound,   NS_SOUND_CID);

#ifdef IBMBIDI
static NS_DEFINE_CID(kCBidiKeyboard,   NS_BIDIKEYBOARD_CID);
#endif
//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
class nsWidgetFactory : public nsIFactory
{
public:

    NS_DECL_ISUPPORTS

    // nsIFactory methods
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              const nsIID &aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

    nsWidgetFactory(const nsCID &aClass);
    virtual ~nsWidgetFactory();
private:
  nsCID mClassID;

};

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsWidgetFactory::nsWidgetFactory(const nsCID &aClass)
{
 NS_INIT_REFCNT();
 mClassID = aClass;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsWidgetFactory::~nsWidgetFactory()
{
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
NS_IMPL_ISUPPORTS1(nsWidgetFactory, nsIFactory)

//-------------------------------------------------------------------------
//
// ***IMPORTANT***
// On all platforms, we are assuming in places that the implementation of |nsIWidget|
// is really |nsWindow| and then calling methods specific to nsWindow to finish the job.
// This is by design and the assumption is safe because an nsIWidget can only be created through
// our Widget factory where all our widgets, including the XP widgets, inherit from nsWindow.
// A similar warning is in nsWindow.cpp.
//-------------------------------------------------------------------------
nsresult nsWidgetFactory::CreateInstance(nsISupports *aOuter,
                                          const nsIID &aIID,
                                          void **aResult)
{
    if (aResult == NULL) {
        return NS_ERROR_NULL_POINTER;
    }
    *aResult = NULL;
    if (nsnull != aOuter) {
        return NS_ERROR_NO_AGGREGATION;
    }

    nsISupports *inst = nsnull;

    if (mClassID.Equals(kCTimer)) {
        inst = (nsISupports*)new nsTimerImpl();
    }
    else if (mClassID.Equals(kCWindow)) {
        inst = (nsISupports*)(nsBaseWidget*)new nsCocoaWindow();
    }
    else if (mClassID.Equals(kCPopUp)) {
        inst = (nsISupports*)(nsBaseWidget*)new nsCocoaWindow();
    }
    else if (mClassID.Equals(kCChild)) {
        inst = (nsISupports*)(nsBaseWidget*)new nsChildView();
    }
    else if (mClassID.Equals(kCButton)) {
        inst = (nsISupports*)(nsBaseWidget*)(nsWindow*)new nsButton();
    }
    else if (mClassID.Equals(kCFilePicker)) {
       inst = (nsISupports*)(nsBaseFilePicker*)new nsFilePicker();
    }
#if USE_NATIVE_VERSION
    else if (mClassID.Equals(kCCheckButton)) {
        inst = (nsISupports*)(nsBaseWidget*)(nsWindow*)new nsCheckButton();
    }
#endif
    else if (mClassID.Equals(kCHorzScrollbar)) {
        inst = (nsISupports*)(nsBaseWidget*)(nsWindow*)new nsScrollbar(PR_FALSE);
    }
    else if (mClassID.Equals(kCVertScrollbar)) {
        inst = (nsISupports*)(nsBaseWidget*)(nsWindow*)new nsScrollbar(PR_TRUE);
    }
   else if (mClassID.Equals(kCTextField)) {
        inst = (nsISupports*)(nsBaseWidget*)(nsWindow*)new nsTextWidget();
    }
    else if (mClassID.Equals(kCAppShell)) {
        inst = (nsISupports*)new nsAppShellCocoa();
    }
    else if (mClassID.Equals(kCToolkit)) {
        inst = (nsISupports*)new nsToolkit();
    }
    else if (mClassID.Equals(kCLookAndFeel)) {
        inst = (nsISupports*)new nsLookAndFeel();
    }
    else if (mClassID.Equals(kCLabel)) {
        inst = (nsISupports*)(nsBaseWidget*)(nsWindow*)new nsLabel();
    }
    else if (mClassID.Equals(kCMenuBar)) {
        inst = (nsISupports*)(nsIMenuBar*) new nsMenuBar();
    }
    else if (mClassID.Equals(kCMenu)) {
        inst = (nsISupports*)(nsIMenu*) new nsMenu();
    }
    else if (mClassID.Equals(kCMenuItem)) {
        inst = (nsISupports*)(nsIMenuItem*) new nsMenuItem();
    }
    else if (mClassID.Equals(kCPopUpMenu)) {
 //       inst = (nsISupports*)new nsPopUpMenu();
					NS_NOTYETIMPLEMENTED("nsPopUpMenu");
    }
    else if (mClassID.Equals(kCSound)) {
        inst = (nsISupports*)(nsISound*) new nsSound();
    }
    else if (mClassID.Equals(kCTransferable))
        inst = (nsISupports*)new nsTransferable();
    else if (mClassID.Equals(kCHTMLFormatConverter))
        inst = (nsISupports*)new nsHTMLFormatConverter();
    else if (mClassID.Equals(kCClipboard))
        inst = (nsISupports*)new nsClipboard();
    else if (mClassID.Equals(kCClipboardHelper))
        inst = (nsISupports*)new nsClipboardHelper();
    else if (mClassID.Equals(kCDragService))
        inst = (nsISupports*)NS_STATIC_CAST(nsIDragService*, new nsDragService());
#ifdef IBMBIDI
    else if (mClassID.Equals(kCBidiKeyboard))
        inst = (nsISupports*)(nsIBidiKeyboard*) new nsBidiKeyboard();
#endif // IBMBIDI

    if (inst == NULL) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

		NS_ADDREF(inst);
    nsresult res = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);

    return res;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsresult nsWidgetFactory::LockFactory(PRBool aLock)
{
    // Not implemented in simplest case.
    return NS_OK;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
// return the proper factory to the caller
#if defined(XP_MAC) && defined(MAC_STATIC)
extern "C" NS_WIDGET nsresult
NSGetFactory_WIDGET_DLL(nsISupports* serviceMgr,
                        const nsCID &aClass,
                        const char *aClassName,
                        const char *aContractID,
                        nsIFactory **aFactory)
#else
extern "C" NS_WIDGET nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aContractID,
             nsIFactory **aFactory)
#endif
{
    if (nsnull == aFactory) {
        return NS_ERROR_NULL_POINTER;
    }

    *aFactory = new nsWidgetFactory(aClass);

    if (nsnull == aFactory) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return (*aFactory)->QueryInterface(NS_GET_IID(nsIFactory), (void**)aFactory);
}

#endif
