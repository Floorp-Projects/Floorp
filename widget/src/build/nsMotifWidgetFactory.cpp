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
#include "nsAppShell.h"
#include "nsButton.h"
#include "nsScrollbar.h"
#include "nsCheckButton.h"
#include "nsRadioButton.h"
#include "nsTextWidget.h"
#include "nsTextAreaWidget.h"
#include "nsFileWidget.h"
#include "nsListBox.h"
#include "nsComboBox.h"
#include "nsLookAndFeel.h"
#include "nsDialog.h"
#include "nsLabel.h"
#include "nsMenuBar.h"
#include "nsMenu.h"
#include "nsMenuItem.h"
#include "nsPopUpMenu.h"

#include "nsImageButton.h"
#include "nsMenuButton.h"
#include "nsToolbar.h"
#include "nsToolbarManager.h"
#include "nsToolbarItemHolder.h"



static NS_DEFINE_IID(kCWindow,        NS_WINDOW_CID);
static NS_DEFINE_IID(kCChild,         NS_CHILD_CID);
static NS_DEFINE_IID(kCAppShellCID,      NS_APPSHELL_CID);
static NS_DEFINE_IID(kCHorzScrollbarCID, NS_HORZSCROLLBAR_CID);
static NS_DEFINE_IID(kCVertScrollbarCID, NS_VERTSCROLLBAR_CID);
static NS_DEFINE_IID(kCCheckButtonCID, NS_CHECKBUTTON_CID);
static NS_DEFINE_IID(kCRadioButtonCID, NS_RADIOBUTTON_CID);
static NS_DEFINE_IID(kCTextWidgetCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kCTextAreaWidgetCID, NS_TEXTAREA_CID);
static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kCButtonCID,     NS_BUTTON_CID);
static NS_DEFINE_IID(kCListBoxCID,    NS_LISTBOX_CID);
static NS_DEFINE_IID(kCComboBoxCID,    NS_COMBOBOX_CID);
static NS_DEFINE_IID(kCLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kCDialog,        NS_DIALOG_CID);
static NS_DEFINE_IID(kCLabel,         NS_LABEL_CID);


static NS_DEFINE_IID(kIWidget,        NS_IWIDGET_IID);
static NS_DEFINE_IID(kIAppShellIID,   NS_IAPPSHELL_IID);
static NS_DEFINE_IID(kIButton,        NS_IBUTTON_IID);
static NS_DEFINE_IID(kICheckButton,   NS_ICHECKBUTTON_IID);
static NS_DEFINE_IID(kIScrollbar,     NS_ISCROLLBAR_IID);
static NS_DEFINE_IID(kIFileWidget,    NS_IFILEWIDGET_IID);
static NS_DEFINE_IID(kIListBox,       NS_ILISTBOX_IID);

static NS_DEFINE_IID(kCMenuBar,       NS_MENUBAR_CID);
static NS_DEFINE_IID(kCMenu,          NS_MENU_CID);
static NS_DEFINE_IID(kCMenuItem,      NS_MENUITEM_CID);
static NS_DEFINE_IID(kCPopUpMenu,     NS_POPUPMENU_CID);

static NS_DEFINE_IID(kCImageButton,   NS_IMAGEBUTTON_CID);
static NS_DEFINE_IID(kCToolBar,       NS_TOOLBAR_CID);
static NS_DEFINE_IID(kCToolBarManager,  NS_TOOLBARMANAGER_CID);
static NS_DEFINE_IID(kCToolBarItemHolder,  NS_TOOLBARITEMHOLDER_CID);
static NS_DEFINE_IID(kCMenuButton,     NS_MENUBUTTON_CID);

static NS_DEFINE_IID(kISupportsIID,   NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,    NS_IFACTORY_IID);


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



nsWidgetFactory::nsWidgetFactory(const nsCID &aClass)   
{   
 mClassID = aClass;
}   

nsWidgetFactory::~nsWidgetFactory()   
{   
}   

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

nsresult nsWidgetFactory::CreateInstance(nsISupports *aOuter,  
                                          const nsIID &aIID,  
                                          void **aResult)  
{  
    if (aResult == NULL) {  
        return NS_ERROR_NULL_POINTER;  
    }  

    if (nsnull != aOuter)
      return NS_ERROR_NO_AGGREGATION;

    *aResult = NULL;  
  
    nsISupports *inst = nsnull;
    if (aIID.Equals(kCWindow)) {
        inst = (nsISupports *)(nsIWidget *)new nsWindow();
    }
    else if ( mClassID.Equals(kCCheckButtonCID)) {
        inst = (nsISupports *)(nsIWidget *)new nsCheckButton();
    }
    else if ( mClassID.Equals(kCButtonCID)) {
        inst = (nsISupports *)(nsIWidget *)new nsButton();
    }
    else if (mClassID.Equals(kCVertScrollbarCID)) {
        inst = (nsISupports *)(nsIWidget *)new nsScrollbar(PR_TRUE);
    }
    else if (mClassID.Equals(kCHorzScrollbarCID)) {
        inst = (nsISupports *)(nsIWidget *)new nsScrollbar(PR_FALSE);
    }
    else if (mClassID.Equals(kCTextWidgetCID)) {
        inst = (nsISupports *)(nsIWidget *)new nsTextWidget();
    }
    else if (mClassID.Equals(kCTextAreaWidgetCID)) {
        inst = (nsISupports *)(nsIWidget *)new nsTextAreaWidget();
    }
    else if ( mClassID.Equals(kCRadioButtonCID)) {
        inst = (nsISupports *)(nsIWidget *)new nsRadioButton();
    }
    else if (mClassID.Equals(kCListBoxCID)) {
        inst = (nsISupports *)(nsIWidget *)new nsListBox();
    }
    else if (mClassID.Equals(kCComboBoxCID)) {
        inst = (nsISupports *)(nsIWidget *)new nsComboBox();
    }
    else if (mClassID.Equals(kCFileWidgetCID)) {
        inst = (nsISupports *)(nsIWidget *)new nsFileWidget();
    }
    else if (aIID.Equals(kIWidget)) {
        inst = (nsISupports *)(nsIWidget *)new nsWindow();
    }
    else if (mClassID.Equals(kCChild)) {
        inst = (nsISupports *)(nsIWidget *)new ChildWindow();
    }
    else if (mClassID.Equals(kCDialog)) {
        inst = (nsISupports *)(nsIWidget *)new nsDialog();
    }
    else if (mClassID.Equals(kCLabel)) {
        inst = (nsISupports *)(nsIWidget *)new nsLabel();
    }
    else if (mClassID.Equals(kCMenuBar)) {
        inst = (nsISupports*)new nsMenuBar();
    }
    else if (mClassID.Equals(kCMenu)) {
        inst = (nsISupports*)new nsMenu();
    }
    else if (mClassID.Equals(kCMenuItem)) {
        inst = (nsISupports*)new nsMenuItem();
    }
    else if (mClassID.Equals(kCImageButton)) {
        inst = (nsISupports*)(nsWindow*)new nsImageButton();
    }
    else if (mClassID.Equals(kCMenuButton)) {
        inst = (nsISupports*)(nsWindow*)new nsMenuButton();
    }
    else if (mClassID.Equals(kCToolBar)) {
       inst = (nsISupports*)(nsWindow*)new nsToolbar();
    }
    else if (mClassID.Equals(kCToolBarManager)) {
        inst = (nsISupports*)(nsWindow*)new nsToolbarManager();
    }
    else if (mClassID.Equals(kCToolBarItemHolder)) {
        inst = (nsISupports*)(nsIToolbarItemHolder *) new nsToolbarItemHolder();
    }

    else if (mClassID.Equals(kCPopUpMenu)) {
        inst = (nsISupports*)new nsPopUpMenu();
    }
    else if (mClassID.Equals(kCLookAndFeelCID)) {
        nsLookAndFeel *laf = new nsLookAndFeel();
        if (laf == NULL) {  
            return NS_ERROR_OUT_OF_MEMORY;  
        }  
        nsresult res = laf->QueryInterface(aIID, aResult);
        if (res != NS_OK) {
            delete laf;
        }
        return res;
    }
    else if (mClassID.Equals(kCAppShellCID)) {
        nsAppShell *appInst = new nsAppShell();
        if (appInst == NULL) {  
            return NS_ERROR_OUT_OF_MEMORY;  
        }  
        nsresult res = appInst->QueryInterface(aIID, aResult);
        if (res != NS_OK) {
            delete appInst;
        }
        return res;
    }

    if (inst == NULL) {
        return NS_ERROR_OUT_OF_MEMORY;  
    }
        
    nsresult res = inst->QueryInterface(aIID, aResult);

    if (res != NS_OK) {
        delete inst;         
    }
        
    return res;
}  

nsresult nsWidgetFactory::LockFactory(PRBool aLock)  
{  
    // Not implemented in simplest case.  
    return NS_OK;
}  

// return the proper factory to the caller
extern "C" NS_WIDGET nsresult NSGetFactory(const nsCID &aClass, nsISupports* servMgr, nsIFactory **aFactory)
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


