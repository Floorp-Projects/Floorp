/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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

#ifndef nsFileWidget_h__
#define nsFileWidget_h__

#include "nsToolkit.h"
#include "nsIWidget.h"
#include "nsIFileWidget.h"
#include "nsWindow.h"
#include <Navigation.h>

#define	kMaxTypeListCount	10
#define kMaxTypesPerFilter	9

/**
 * Native Mac FileSelector wrapper
 */

class nsFileWidget : public nsWindow, public nsIFileWidget
{
  public:
                            nsFileWidget(); 
    virtual                 ~nsFileWidget();
	
	// nsISupports
	NS_IMETHOD_(nsrefcnt) AddRef();
	NS_IMETHOD_(nsrefcnt) Release();
	NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);


    // nsIWidget interface
    NS_IMETHOD              Create(nsIWidget *aParent,
                                   const nsRect &aRect,
                                   EVENT_CALLBACK aHandleEventFunction,
                                   nsIDeviceContext *aContext,
                                   nsIAppShell *aAppShell = nsnull,
                                   nsIToolkit *aToolkit = nsnull,
                                   nsWidgetInitData *aInitData = nsnull);
    NS_IMETHOD              Create(nsNativeWidget aNativeParent,
                                   const nsRect &aRect,
                                   EVENT_CALLBACK aHandleEventFunction,
                                   nsIDeviceContext *aContext,
                                   nsIAppShell *aAppShell = nsnull,
                                   nsIToolkit *aToolkit = nsnull,
                                   nsWidgetInitData *aInitData = nsnull);

    // nsIFileWidget interface
		NS_IMETHOD					   Create(nsIWidget *aParent,
							                      const nsString& aTitle,
							                      nsFileDlgMode aMode,
							                      nsIDeviceContext *aContext = nsnull,
							                      nsIAppShell *aAppShell = nsnull,
							                      nsIToolkit *aToolkit = nsnull,
							                      void *aInitData = nsnull);

    virtual PRBool      	Show();
    NS_IMETHOD            GetFile(nsFileSpec& aFile);
    NS_IMETHOD            SetDefaultString(const nsString& aString);
    NS_IMETHOD           	SetFilterList(PRUint32 aNumberOfFilters,
                                          const nsString aTitles[],
                                          const nsString aFilters[]);
   
    NS_IMETHOD            GetDisplayDirectory(nsFileSpec& aDirectory);
    NS_IMETHOD            SetDisplayDirectory(const nsFileSpec& aDirectory);

	  virtual nsFileDlgResults GetFile(
	    nsIWidget        * aParent,
	    const nsString   & promptString,    // Window title for the dialog
	    nsFileSpec       & theFileSpec);     // Populate with initial path for file dialog
	    
	  virtual nsFileDlgResults GetFolder(
	    nsIWidget        * aParent,
	    const nsString   & promptString,    // Window title for the dialog
	    nsFileSpec       & theFileSpec);     // Populate with initial path for file dialog 
	    
	  virtual nsFileDlgResults PutFile(
	    nsIWidget        * aParent,
	    const nsString   & promptString,    // Window title for the dialog
	    nsFileSpec       & theFileSpec);     // Populate with initial path for file dialog 

    NS_IMETHOD            GetSelectedType(PRInt16& theType);

  protected:
    NS_IMETHOD            OnOk();
    NS_IMETHOD            OnCancel();

		// actual implementations of get/put dialogs using NavServices
	PRBool PutFile ( Str255 & inTitle, Str255 & inDefaultName, FSSpec* outFileSpec ) ;
	PRBool GetFile ( Str255 & inTitle, FSSpec* outFileSpec ) ;
	PRBool GetFolder ( Str255 & inTitle, FSSpec* outFileSpec ) ;
  
  protected:
     PRBool                 mIOwnEventLoop;
     PRBool                 mWasCancelled;
     nsString               mTitle;
     nsFileDlgMode          mMode;
     nsString               mFile;
     PRUint32               mNumberOfFilters;  
     const nsString*        mTitles;
     const nsString*        mFilters;
     nsString               mDefault;
     nsFileSpec             mDisplayDirectory;
     nsFileSpec				mFileSpec;
     nsFileDlgResults		mSelectResult;

     void GetFilterListArray(nsString& aFilterList);
     
     NavTypeListPtr			mTypeLists[kMaxTypeListCount];
     PRInt16				mSelectedType;
          
};

#endif // nsFileWidget_h__
