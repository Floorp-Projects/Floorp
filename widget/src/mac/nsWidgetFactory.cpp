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
 */

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
#include "nsFileSpecWithUIImpl.h"
#include "nsScrollbar.h"
#include "nsMenuBar.h"
#include "nsMenu.h"
#include "nsMenuItem.h"

#include "nsClipboard.h"
#include "nsTransferable.h"
#include "nsXIFFormatConverter.h"
#include "nsDragService.h"

#if USE_NATIVE_VERSION
# include "nsTextAreaWidget.h"
# include "nsListBox.h"
# include "nsComboBox.h"
# include "nsRadioButton.h"
# include "nsCheckButton.h"
#endif

#include "nsLookAndFeel.h"

#include "nsIComponentManager.h"

#include "nsSound.h"
#include "nsTimerMac.h"

// NOTE the following does not match MAC_STATIC actually used below in this file!
#define MACSTATIC

static NS_DEFINE_CID(kCWindow,        NS_WINDOW_CID);
static NS_DEFINE_IID(kCTimer,         NS_TIMER_CID);
static NS_DEFINE_CID(kCPopUp,         NS_POPUP_CID);
static NS_DEFINE_CID(kCChild,         NS_CHILD_CID);
static NS_DEFINE_CID(kCButton,        NS_BUTTON_CID);
static NS_DEFINE_CID(kCCheckButton,   NS_CHECKBUTTON_CID);
static NS_DEFINE_CID(kCFilePicker,    NS_FILEPICKER_CID);
static NS_DEFINE_CID(kCCombobox,      NS_COMBOBOX_CID);
static NS_DEFINE_CID(kCListbox,       NS_LISTBOX_CID);
static NS_DEFINE_CID(kCRadioButton,   NS_RADIOBUTTON_CID);
static NS_DEFINE_CID(kCHorzScrollbar, NS_HORZSCROLLBAR_CID);
static NS_DEFINE_CID(kCVertScrollbar, NS_VERTSCROLLBAR_CID);
static NS_DEFINE_CID(kCTextArea,      NS_TEXTAREA_CID);
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
static NS_DEFINE_CID(kCTransferable,  NS_TRANSFERABLE_CID);
static NS_DEFINE_CID(kCXIFFormatConverter,  NS_XIFFORMATCONVERTER_CID);
static NS_DEFINE_CID(kCDragService,   NS_DRAGSERVICE_CID);

// Sound services (just Beep for now)
static NS_DEFINE_CID(kCSound,   NS_SOUND_CID);
static NS_DEFINE_CID(kCFileSpecWithUI,   NS_FILESPECWITHUI_CID);

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
    ~nsWidgetFactory();
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
        inst = (nsISupports*)(nsBaseWidget*)new nsMacWindow();
    }
    else if (mClassID.Equals(kCPopUp)) {
        inst = (nsISupports*)(nsBaseWidget*)new nsMacWindow();
    }
    else if (mClassID.Equals(kCChild)) {
        inst = (nsISupports*)(nsBaseWidget*)new ChildWindow();
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
    else if (mClassID.Equals(kCCombobox)) {
        inst = (nsISupports*)(nsBaseWidget*)(nsWindow*)new nsComboBox();
    }
    else if (mClassID.Equals(kCRadioButton)) {
        inst = (nsISupports*)(nsBaseWidget*)(nsWindow*)new nsRadioButton();
    }
    else if (mClassID.Equals(kCListbox)) {
       inst = (nsISupports*)(nsBaseWidget*)(nsWindow*)new nsListBox();
    }
    else if (mClassID.Equals(kCTextArea)) {
        inst = (nsISupports*)(nsBaseWidget*)(nsWindow*)new nsTextAreaWidget();
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
        inst = (nsISupports*)new nsAppShell();
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
    else if (mClassID.Equals(kCFileSpecWithUI))
    	inst = (nsISupports*) (nsIFileSpecWithUI *) new nsFileSpecWithUIImpl;
    else if (mClassID.Equals(kCTransferable))
        inst = (nsISupports*)new nsTransferable();
    else if (mClassID.Equals(kCXIFFormatConverter))
        inst = (nsISupports*)new nsXIFFormatConverter();
    else if (mClassID.Equals(kCClipboard))
        inst = (nsISupports*)new nsClipboard();
    else if (mClassID.Equals(kCDragService))
        inst = (nsISupports*)NS_STATIC_CAST(nsIDragService*, new nsDragService());

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
                        const char *aProgID,
                        nsIFactory **aFactory)
#else
extern "C" NS_WIDGET nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
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
