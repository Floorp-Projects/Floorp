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

/* Interface Headers */
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIButton.h"
#include "nsILabel.h"
#include "nsICheckButton.h"
#include "nsIRadioButton.h"
#include "nsITextWidget.h"
#include "nsITextAreaWidget.h"
#include "nsIListBox.h"
#include "nsIFileWidget.h"
#include "nsFileSpecWithUIImpl.h"
#include "nsIComboBox.h"
#include "nsISound.h"

#include "nsWidgetsCID.h"

/* Implmentation Headers */
#include "nsToolkit.h"
#include "nsWindow.h"
#include "nsAppShell.h"
#include "nsLookAndFeel.h"
#include "nsButton.h"
#include "nsCheckButton.h"
#include "nsRadioButton.h"
#include "nsMenuBar.h"
#include "nsMenu.h"
#include "nsMenuItem.h"
#include "nsTextWidget.h"
#include "nsTextAreaWidget.h"
#include "nsScrollbar.h"
#include "nsListBox.h"
#include "nsFileWidget.h"
#include "nsComboBox.h"
#include "nsLabel.h"
#include "nsClipboard.h"
#include "nsTransferable.h"
#include "nsXIFFormatConverter.h"

#include "nsPhWidgetLog.h"

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
static NS_DEFINE_IID(kCAppShell,      NS_APPSHELL_CID);
static NS_DEFINE_IID(kCToolkit,       NS_TOOLKIT_CID);
static NS_DEFINE_IID(kCLookAndFeel,   NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kCLabel,         NS_LABEL_CID);
static NS_DEFINE_IID(kCMenuBar,       NS_MENUBAR_CID);
static NS_DEFINE_IID(kCMenu,          NS_MENU_CID);
static NS_DEFINE_IID(kCMenuItem,      NS_MENUITEM_CID);
static NS_DEFINE_IID(kCImageButton,   NS_IMAGEBUTTON_CID);
static NS_DEFINE_IID(kCPopUpMenu,     NS_POPUPMENU_CID);
static NS_DEFINE_IID(kCMenuButton,    NS_MENUBUTTON_CID);

// Drag & Drop, Clipboard
static NS_DEFINE_IID(kCDataObj,       NS_DATAOBJ_CID);
static NS_DEFINE_IID(kCClipboard,     NS_CLIPBOARD_CID);
static NS_DEFINE_IID(kCTransferable,  NS_TRANSFERABLE_CID);
static NS_DEFINE_IID(kCXIFFormatConverter,  NS_XIFFORMATCONVERTER_CID);
//static NS_DEFINE_IID(kCDragService,   NS_DRAGSERVICE_CID);
//static NS_DEFINE_IID(kCFileListTransferable, NS_FILELISTTRANSFERABLE_CID);

static NS_DEFINE_IID(kISupportsIID,   NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,    NS_IFACTORY_IID);
static NS_DEFINE_CID(kCSound,   	  NS_SOUND_CID);

static NS_DEFINE_CID(kCFileSpecWithUI,   NS_FILESPECWITHUI_CID);


class nsWidgetFactory : public nsIFactory
{   
public:   
    // nsISupports methods
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

NS_IMPL_ADDREF(nsWidgetFactory)
NS_IMPL_RELEASE(nsWidgetFactory)


nsWidgetFactory::nsWidgetFactory(const nsCID &aClass)   
{   
  NS_INIT_REFCNT();
  mClassID = aClass;
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidgetFactory::nsWidgetFactory Constructor Called\n"));

}   

nsWidgetFactory::~nsWidgetFactory()   
{   
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidgetFactory::~nsWidgetFactory Destructor Called\n"));

  NS_ASSERTION(mRefCnt == 0, "Reference count not zero in destructor");
}   

nsresult nsWidgetFactory::QueryInterface(const nsIID &aIID,   
                                         void **aResult)   
{   
  PR_LOG(PhWidLog, PR_LOG_ERROR,( "nsWidgetFactory::QueryInterface mRefCnt=<%d>\n", mRefCnt));

  if (NULL == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = NULL;

  if (aIID.Equals(kISupportsIID)) {
    *aResult = (void *)(nsISupports *)this;
  } else if (aIID.Equals(kIFactoryIID)) {
    *aResult = (void *)(nsIFactory *)this;
  }

  if (*aResult == NULL) {
    return NS_NOINTERFACE;
  }

  NS_ADDREF_THIS();
  return NS_OK;
}   


nsresult nsWidgetFactory::CreateInstance( nsISupports *aOuter,  
                                          const nsIID &aIID,  
                                          void **aResult)  
{  
PR_LOG(PhWidLog, PR_LOG_ERROR,( "nsWidgetFactory::CreateInstance 1 mRefCnt=<%d>\n", mRefCnt));

    if (aResult == NULL) {  
        return NS_ERROR_NULL_POINTER;  
    }  

    *aResult = NULL;  

    if (nsnull != aOuter)
      return NS_ERROR_NO_AGGREGATION;
  
    nsISupports *inst = nsnull;
    if (mClassID.Equals(kCWindow)) {
	  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidgetFactory::CreateInstance of nsWindow\n"));
      inst = (nsISupports *)new nsWindow();
    }
    else if (mClassID.Equals(kCChild)) {
      PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidgetFactory::CreateInstance of nsChildWindow\n"));
      inst = (nsISupports *)new ChildWindow();
    }
    else if (mClassID.Equals(kCButton)) {
	  PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsButton\n" ));
      inst = (nsISupports *)(nsWidget *)new nsButton();
    }
    else if (mClassID.Equals(kCTextField)) {
    PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidgetFactory::CreateInstance of TextField\n"));
      inst = (nsISupports *) (nsWidget *) new nsTextWidget();
    }
    else if (mClassID.Equals(kCHorzScrollbar)) {
	  PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of HorzScrollBar\n" ));
      inst = (nsISupports *) (nsWidget *) new nsScrollbar( PR_FALSE );
    }
    else if (mClassID.Equals(kCVertScrollbar)) {
	  PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of VertScrollBar\n" ));
      inst = (nsISupports *) (nsWidget *) new nsScrollbar( PR_TRUE );
    }
    else if (mClassID.Equals(kCCheckButton)) {
	  PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsCheckButton\n" ));
      inst = (nsISupports *)(nsWidget *)new nsCheckButton();
    }
    else if (mClassID.Equals(kCRadioButton)) {
	  PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsRadioButton\n" ));
      inst = (nsISupports *)(nsWidget *)new nsRadioButton();
    }
    else if (mClassID.Equals(kCTextArea)) {
      inst = (nsISupports *) (nsWidget *) new nsTextAreaWidget();
	  PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of TextArea\n" ));
    }
    else if (mClassID.Equals(kCListbox)) {
      inst = (nsISupports *) (nsWidget *) new nsListBox();
	  PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsListBox\n" ));
	}
    else if (mClassID.Equals(kCFileOpen)) {
      inst = (nsISupports *) new nsFileWidget();
	  PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsFileWidget\n" ));
    }
    else if (mClassID.Equals(kCCombobox)) {
      inst = (nsISupports *) (nsWidget *) new nsComboBox();
 	  PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsComboBox\n" ));
    }
    else if (mClassID.Equals(kCSound)) {
    	nsISound* aSound = nsnull;
    	NS_NewSound(&aSound);
        inst = (nsISupports*) aSound;
 	    PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsSound\n" ));

    }
	
#if 1
/* Widget we don't support */
    else if (mClassID.Equals(kCPopUpMenu)) {
	  PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of PopUpMenu\n" ));
    }
#endif

    else if (mClassID.Equals(kCAppShell)) {
    PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsAppShell\n" ));
        inst = (nsISupports*)new nsAppShell();
    }
    else if (mClassID.Equals(kCToolkit)) {
    PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsToolkit\n" ));
        inst = (nsISupports*)new nsToolkit();
    }
    else if (mClassID.Equals(kCLookAndFeel)) {
    PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsLookAndFeel\n" ));
        inst = (nsISupports*)new nsLookAndFeel();
    }
    else if (mClassID.Equals(kCMenuBar)) {
    PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsMenuBar\n" ));
        inst = (nsISupports*)(nsIMenuBar *)new nsMenuBar();
    }
    else if (mClassID.Equals(kCMenu)) {
    PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsMenu\n" ));
        inst = (nsISupports*)(nsIMenu *)new nsMenu();
    }
    else if (mClassID.Equals(kCMenuItem)) {
    PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsMenuItem\n" ));
        inst = (nsISupports*)(nsIMenuItem *)new nsMenuItem();
    }
    else if (mClassID.Equals(kCLabel)) {
      PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsLabel\n" ));
      inst = (nsISupports*)(nsILabel *)new nsLabel();
    }
    else if (mClassID.Equals(kCClipboard)) {
        PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidgetFactory::CreateInstance of nsClipboard\n"));
        inst = (nsISupports*)(nsBaseClipboard *)new nsClipboard();
    }
    else if (mClassID.Equals(kCXIFFormatConverter)) {
        PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsXIFFormatConverter\n" ));
        inst = (nsISupports*)new nsXIFFormatConverter();
    }
    else if (mClassID.Equals(kCFileSpecWithUI))
    {
        PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsFileSpecWithUIImpl\n" ));
    	inst = (nsISupports*) (nsIFileSpecWithUI *) new nsFileSpecWithUIImpl;
    }
    else if (mClassID.Equals(kCTransferable)) {
        PR_LOG(PhWidLog, PR_LOG_DEBUG,( "nsWidgetFactory::CreateInstance of nsTransferable\n" ));
        inst = (nsISupports*)new nsTransferable();
    }
												
    if (inst == NULL) {  
        PR_LOG(PhWidLog, PR_LOG_ERROR,( "nsWidgetFactory::CreateInstance Failed to create widget.\n" ));
        return NS_ERROR_OUT_OF_MEMORY;  
    }  

PR_LOG(PhWidLog, PR_LOG_ERROR,( "nsWidgetFactory::CreateInstance 2 mRefCnt=<%d>\n", mRefCnt));

    nsresult res = inst->QueryInterface(aIID, aResult);

    if (res != NS_OK) {  
        PR_LOG(PhWidLog, PR_LOG_ERROR,( "nsWidgetFactory::CreateInstance  query interface barfed.\n" ));
        // We didn't get the right interface, so clean up  
        delete inst;  
    }

    return res;

}  

nsresult nsWidgetFactory::LockFactory(PRBool aLock)  
{  
  PR_LOG(PhWidLog, PR_LOG_ERROR,( "nsWidgetFactory::Lock Factory mRefCnt=<%d> - Not Implemented\n", mRefCnt));

    // Not implemented in simplest case.  
    return NS_OK;
}  

// return the proper factory to the caller
extern "C" NS_WIDGET nsresult 
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
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


