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
#include "nsAppShell.h"
#include "nsButton.h"
#include "nsRadioButton.h"
#include "nsCheckButton.h"
#include "nsTextWidget.h"
#include "nsFileWidget.h"
#include "nsScrollbar.h"

//#include "nsTextAreaWidget.h"
//#include "nsListBox.h"
//#include "nsComboBox.h"
#include "nsLookAndFeel.h"

#define MACSTATIC

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
static NS_DEFINE_IID(kCToolkit,       NS_TOOLKIT_CID);
static NS_DEFINE_IID(kCLookAndFeelCID, NS_LOOKANDFEEL_CID);


static NS_DEFINE_IID(kIWidget,        NS_IWIDGET_IID);
static NS_DEFINE_IID(kIWindow,        NS_IWINDOW_IID);	//¥¥¥
static NS_DEFINE_IID(kIAppShellIID,   NS_IAPPSHELL_IID);
static NS_DEFINE_IID(kIButton,        NS_IBUTTON_IID);
static NS_DEFINE_IID(kICheckButton,   NS_ICHECKBUTTON_IID);
static NS_DEFINE_IID(kIScrollbar,     NS_ISCROLLBAR_IID);
//static NS_DEFINE_IID(kIFileWidget,    NS_IFILEWIDGET_IID);
//static NS_DEFINE_IID(kIListBox,       NS_ILISTBOX_IID);

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

    *aResult = NULL;  
  
    if (nsnull != aOuter && !aIID.Equals(kISupportsIID)) {
        // aggregation with IID != nsISupports
        return NS_ERROR_ILLEGAL_VALUE;
    }

    nsWindow *inst = nsnull;
    if (aIID.Equals(kCWindow)) 
    	{
      inst = new nsMacWindow();
    	}
	 	else if (aIID.Equals(kIWindow)) 
    	{
     	inst = new nsMacWindow();
    	}
	 	else if (aIID.Equals(kIWidget)) 
    	{
     	//¥¥¥inst = new nsWindow();
     	inst = new ChildWindow();
    	}
    else if (mClassID.Equals(kCAppShellCID)) 
			{
			nsAppShell *appInst = new nsAppShell();
			if (appInst == NULL) 
				{  
				return NS_ERROR_OUT_OF_MEMORY;  
				}  
			nsresult res = appInst->QueryInterface(aIID, aResult);
			if (res != NS_OK) 
				{
				delete appInst;
				}
			return res;
			}
    else if (mClassID.Equals(kCToolkit)) {
        inst = (nsWindow*)new nsToolkit();
    }
    else if ( mClassID.Equals(kCButtonCID)) {
        inst = new nsButton();
    }
    else if (mClassID.Equals(kCChild)) {
        inst = new ChildWindow();
    }
    else if ( mClassID.Equals(kCRadioButtonCID )) {
        inst = new nsRadioButton();
    }
    else if ( mClassID.Equals(kCCheckButtonCID)) {
        inst = new nsCheckButton();
    }
    else if (mClassID.Equals(kCTextWidgetCID)) {
        inst = new nsTextWidget();
    }
    else if (mClassID.Equals(kCFileWidgetCID)) {
        inst = new nsFileWidget();
    }
    else if (mClassID.Equals(kCVertScrollbarCID)) {
        inst = new nsScrollbar(PR_TRUE);
    }
    else if (mClassID.Equals(kCHorzScrollbarCID)) {
        inst = new nsScrollbar(PR_FALSE);
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
   
    
    
#ifdef NOTNOW
    else if (mClassID.Equals(kCVertScrollbarCID)) {
        inst = new nsScrollbar(, PR_TRUE);
    }
    else if (mClassID.Equals(kCHorzScrollbarCID)) {
        inst = new nsScrollbar(, PR_FALSE);
    }
    else if (aIID.Equals(kIScrollbar)) {
        inst = nsnull;
        fprintf(stderr, "------ NOT CreatingkIScrollbar Scrollbar\n");
    }
    else if (mClassID.Equals(kCTextAreaWidgetCID)) {
        inst = new nsTextAreaWidget();
    }
    else if (mClassID.Equals(kCListBoxCID)) {
        inst = new nsListBox();
    }
    else if (mClassID.Equals(kCComboBoxCID)) {
        inst = new nsComboBox();
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
#endif

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
#ifdef MACSTATIC
extern "C" NS_WIDGET nsresult NSGetFactory_WIDGET_DLL(const nsCID &aClass, nsIFactory **aFactory)
#else
extern "C" NS_WIDGET nsresult NSGetFactory(const nsCID &aClass, nsIFactory **aFactory)
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


