/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIButton.h"
#include "nsITextWidget.h"
#include "nsWidgetsCID.h"

#include "nsToolkit.h"
#include "nsWindow.h"
#include "nsMacWindow.h"
#include "nsDialog.h"
#include "nsAppShell.h"
#include "nsButton.h"
#include "nsRadioButton.h"
#include "nsCheckButton.h"
#include "nsTextWidget.h"
#include "nsLabel.h"
#include "nsFileWidget.h"
#include "nsScrollbar.h"
#include "nsMenuBar.h"
#include "nsMenu.h"
#include "nsMenuItem.h"
#include "nsImageButton.h"
#include "nsMenuButton.h"

#include "nsClipboard.h"
#include "nsTransferable.h"
#include "nsXIFFormatConverter.h"
#include "nsDataFlavor.h"

#include "nsTextAreaWidget.h"
#include "nsListBox.h"
#include "nsComboBox.h"
#include "nsLookAndFeel.h"

#define MACSTATIC

static NS_DEFINE_IID(kCWindow,        NS_WINDOW_CID);
static NS_DEFINE_IID(kCChild,         NS_CHILD_CID);
static NS_DEFINE_IID(kCButton,        NS_BUTTON_CID);
static NS_DEFINE_IID(kCCheckButton,   NS_CHECKBUTTON_CID);
static NS_DEFINE_IID(kCCombobox,      NS_COMBOBOX_CID);
static NS_DEFINE_IID(kCFileOpen,      NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kCListbox,       NS_LISTBOX_CID);
static NS_DEFINE_IID(kCRadioButton,   NS_RADIOBUTTON_CID);
static NS_DEFINE_IID(kCHorzScrollbar, NS_HORZSCROLLBAR_CID);
static NS_DEFINE_IID(kCVertScrollbar, NS_VERTSCROLLBAR_CID);
static NS_DEFINE_IID(kCTextArea,      NS_TEXTAREA_CID);
static NS_DEFINE_IID(kCTextField,     NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kCTabWidget,     NS_TABWIDGET_CID);
static NS_DEFINE_IID(kCTooltipWidget, NS_TOOLTIPWIDGET_CID);
static NS_DEFINE_IID(kCAppShell,      NS_APPSHELL_CID);
static NS_DEFINE_IID(kCToolkit,       NS_TOOLKIT_CID);
static NS_DEFINE_IID(kCLookAndFeel,   NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kCDialog,        NS_DIALOG_CID);
static NS_DEFINE_IID(kCLabel,         NS_LABEL_CID);
static NS_DEFINE_IID(kCMenuBar,       NS_MENUBAR_CID);
static NS_DEFINE_IID(kCMenu,          NS_MENU_CID);
static NS_DEFINE_IID(kCMenuItem,      NS_MENUITEM_CID);
static NS_DEFINE_IID(kCImageButton,   NS_IMAGEBUTTON_CID);
static NS_DEFINE_IID(kCPopUpMenu,     NS_POPUPMENU_CID);
static NS_DEFINE_IID(kCMenuButton,     NS_MENUBUTTON_CID);

static NS_DEFINE_IID(kISupportsIID,   NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,    NS_IFACTORY_IID);

static NS_DEFINE_IID(kCDataFlavor,    NS_DATAFLAVOR_CID);
static NS_DEFINE_IID(kCClipboard,     NS_CLIPBOARD_CID);
static NS_DEFINE_IID(kCGenericTransferable,  NS_GENERICTRANSFERABLE_CID);
static NS_DEFINE_IID(kCXIFFormatConverter,  NS_XIFFORMATCONVERTER_CID);


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
nsresult nsWidgetFactory::QueryInterface(const nsIID &aIID,   
                                         void **aInstancePtr)   
{   
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(kIFactoryIID)) {
    *aInstancePtr = (void*)(nsWidgetFactory*)this;
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsWidgetFactory*)this;
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}   

NS_IMPL_ADDREF(nsWidgetFactory)
NS_IMPL_RELEASE(nsWidgetFactory)

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
    if (mClassID.Equals(kCWindow)) {
        inst = (nsISupports*)new nsMacWindow();
    }
    else if (mClassID.Equals(kCChild)) {
        inst = (nsISupports*)new ChildWindow();
    }
    else if (mClassID.Equals(kCButton)) {
        inst = (nsISupports*)(nsWindow*)new nsButton();
    }
    else if (mClassID.Equals(kCCheckButton)) {
        inst = (nsISupports*)(nsWindow*)new nsCheckButton();
    }
    else if (mClassID.Equals(kCCombobox)) {
        inst = (nsISupports*)(nsWindow*)new nsComboBox();
    }
    else if (mClassID.Equals(kCRadioButton)) {
        inst = (nsISupports*)(nsWindow*)new nsRadioButton();
    }
    else if (mClassID.Equals(kCFileOpen)) {
       inst = (nsISupports*)(nsWindow*)new nsFileWidget();
    }
    else if (mClassID.Equals(kCListbox)) {
       inst = (nsISupports*)(nsWindow*)new nsListBox();
    }
    else if (mClassID.Equals(kCHorzScrollbar)) {
        inst = (nsISupports*)(nsWindow*)new nsScrollbar(PR_FALSE);
    }
    else if (mClassID.Equals(kCVertScrollbar)) {
        inst = (nsISupports*)(nsWindow*)new nsScrollbar(PR_TRUE);
    }
    else if (mClassID.Equals(kCTextArea)) {
        inst = (nsISupports*)(nsWindow*)new nsTextAreaWidget();
    }
    else if (mClassID.Equals(kCTextField)) {
        inst = (nsISupports*)(nsWindow*)new nsTextWidget();
    }
    else if (mClassID.Equals(kCTabWidget)) {
//        inst = (nsISupports*)(nsWindow*)new nsTabWidget();
					NS_NOTYETIMPLEMENTED("nsTabWidget");
    }
    else if (mClassID.Equals(kCTooltipWidget)) {
 //       inst = (nsISupports*)(nsWindow*)new nsTooltipWidget();
					NS_NOTYETIMPLEMENTED("nsTooltipWidget");
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
    else if (mClassID.Equals(kCDialog)) {
        inst = (nsISupports*)(nsWindow*)new nsDialog();
    }
    else if (mClassID.Equals(kCLabel)) {
        inst = (nsISupports*)(nsWindow*)new nsLabel();
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
    else if (mClassID.Equals(kCImageButton)) {
        inst = (nsISupports*)(nsWindow*)new nsImageButton();
    }
    else if (mClassID.Equals(kCMenuButton)) {
        inst = (nsISupports*)(nsWindow*)new nsMenuButton();
    }
    else if (mClassID.Equals(kCPopUpMenu)) {
 //       inst = (nsISupports*)new nsPopUpMenu();
					NS_NOTYETIMPLEMENTED("nsPopUpMenu");
    }
    else if (mClassID.Equals(kCDataFlavor)) {
        inst = (nsISupports*)new nsDataFlavor();
    }
    else if (mClassID.Equals(kCGenericTransferable)) {
        inst = (nsISupports*)new nsTransferable();
    }
    else if (mClassID.Equals(kCXIFFormatConverter)) {
        inst = (nsISupports*)new nsXIFFormatConverter();
    }
    else if (mClassID.Equals(kCClipboard)) {
        inst = (nsISupports*)new nsClipboard();
    }
  
    if (inst == NULL) {  
        return NS_ERROR_OUT_OF_MEMORY;  
    }  

    nsresult res = inst->QueryInterface(aIID, aResult);

    if (res != NS_OK) {  
        // We didn't get the right interface, so clean up  
        delete inst;  
    }

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

    return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}
