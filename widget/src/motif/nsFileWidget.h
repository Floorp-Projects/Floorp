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

//#include "nsIWidget.h"
#include "nsIFileWidget.h"
#include "nsWindow.h"

/**
 * Native Motif FileSelector wrapper
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

    NS_IMETHOD Create(nsIWidget *aParent,
                const nsRect &aRect,
                EVENT_CALLBACK aHandleEventFunction,
                nsIDeviceContext *aContext = nsnull,
                nsIAppShell *aAppShell = nsnull,
                nsIToolkit *aToolkit = nsnull,
                nsWidgetInitData *aInitData = nsnull);

    NS_IMETHOD Create(nsNativeWidget aParent,
                const nsRect &aRect,
                EVENT_CALLBACK aHandleEventFunction,
                nsIDeviceContext *aContext = nsnull,
                nsIAppShell *aAppShell = nsnull,
                nsIToolkit *aToolkit = nsnull,
                nsWidgetInitData *aInitData = nsnull);

    // nsIWidget interface
  
    NS_IMETHOD            Create( nsIWidget *aParent,
                                    const nsString& aTitle,
                                    nsFileDlgMode aMode,
                                    nsIDeviceContext *aContext = nsnull,
                                    nsIAppShell *aAppShell = nsnull,
                                    nsIToolkit *aToolkit = nsnull,
                                    void *aInitData = nsnull);

    // nsIFileWidget part
    virtual PRBool               Show();
    virtual nsFileDlgResults     GetFile(class nsIWidget *, const class nsString &, class nsFileSpec &);
    virtual nsFileDlgResults     GetFolder(class nsIWidget *, const class nsString &, class nsFileSpec &);
    virtual nsFileDlgResults     PutFile(class nsIWidget *, const class nsString &, class nsFileSpec &);
    NS_IMETHOD                   GetFile(nsFileSpec& aFile);
    NS_IMETHOD                   SetDefaultString(const nsString& aString);
    NS_IMETHOD                   SetDisplayDirectory(const nsFileSpec& aDirectory);
    NS_IMETHOD                   GetDisplayDirectory(nsFileSpec& aDirectory);
    NS_IMETHOD                   SetFilterList(PRUint32 aNumberOfFilters,
                                        const nsString aTitles[],
                                        const nsString aFilters[]);
    NS_IMETHOD                   GetSelectedType(PRInt16& theType);
    NS_IMETHOD                   OnOk();
    NS_IMETHOD                   OnCancel();

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
     XtAppContext           mAppContext;

     void GetFilterListArray(nsString& aFilterList);

  
};

#endif // nsFileWidget_h__
