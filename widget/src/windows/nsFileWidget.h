/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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

#ifndef nsFileWidget_h__
#define nsFileWidget_h__

#include "nsObject.h"
#include "nsToolkit.h"
#include "nsIWidget.h"
#include "nsIFileWidget.h"

/**
 * Native Win32 FileSelector wrapper
 */

class nsFileWidget : public nsIFileWidget, public nsObject
{
  public:
                            nsFileWidget(nsISupports *aOuter); 
    virtual                 ~nsFileWidget();

    // nsISupports. Forward to the nsObject base class
    BASE_SUPPORT

    virtual nsresult        QueryObject(const nsIID& aIID, void** aInstancePtr);
    PRBool                  OnPaint();

    // nsIWidget interface
  
    virtual void            Create( nsIWidget *aParent,
                                    nsString& aTitle,
                                    nsMode aMode,
                                    nsIDeviceContext *aContext = nsnull,
                                    nsIToolkit *aToolkit = nsnull,
                                    void *aInitData = nsnull);

    // nsIFileWidget part
    virtual PRBool          Show();
    virtual void            GetFile(nsString& aFile);
    virtual void            SetFilterList(PRUint32 aNumberOfFilters,const nsString aTitles[],const nsString aFilters[]);
  
  protected:

     HWND                   mWnd;
     nsString               mTitle;
     nsMode                 mMode;
     nsString               mFile;
     PRUint32               mNumberOfFilters;  
     const nsString*        mTitles;
     const nsString*        mFilters;

     void GetFilterListArray(nsString& aFilterList);
};

#endif // nsFileWidget_h__
