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

#include "nsToolkit.h"
#include "nsIWidget.h"
#include "nsIFileWidget.h"
#include "nsWindow.h"

/**
 * Native Motif FileSelector wrapper
 */

class nsFileWidget : public nsWindow
{
  public:
                            nsFileWidget(nsISupports *aOuter); 
    virtual                 ~nsFileWidget();
    NS_IMETHOD QueryObject(REFNSIID aIID, void** aInstancePtr);

    void Create(nsIWidget *aParent,
                const nsRect &aRect,
                EVENT_CALLBACK aHandleEventFunction,
                nsIDeviceContext *aContext = nsnull,
                nsIToolkit *aToolkit = nsnull,
                nsWidgetInitData *aInitData = nsnull);

    void Create(nsNativeWindow aParent,
                const nsRect &aRect,
                EVENT_CALLBACK aHandleEventFunction,
                nsIDeviceContext *aContext = nsnull,
                nsIToolkit *aToolkit = nsnull,
                nsWidgetInitData *aInitData = nsnull);

    // nsIWidget interface
  
    virtual void            Create( nsIWidget *aParent,
                                    nsString& aTitle,
                                    nsMode aMode,
                                    nsIDeviceContext *aContext = nsnull,
                                    nsIToolkit *aToolkit = nsnull,
                                    void *aInitData = nsnull);

    // nsIFileWidget part
    virtual void            Show(PRBool bState);
    virtual void            GetFile(nsString& aFile);
    virtual void            SetFilterList(PRUint32 aNumberOfFilters,
                                          const nsString aTitles[],
                                          const nsString aFilters[]);
    virtual void            OnOk();
    virtual void            OnCancel();
  
  protected:
     PRBool                 mIOwnEventLoop;
     PRBool                 mWasCancelled;
     nsString               mTitle;
     nsMode                 mMode;
     nsString               mFile;
     PRUint32               mNumberOfFilters;  
     const nsString*        mTitles;
     const nsString*        mFilters;

     void GetFilterListArray(nsString& aFilterList);

  private:

    // this should not be public
    static PRInt32 GetOuterOffset() {
      return offsetof(nsFileWidget,mAggWidget);
    }


    // Aggregator class and instance variable used to aggregate in the
    // nsIFileWidget interface to nsFileWidget w/o using multiple
    // inheritance.
    class AggFileWidget : public nsIFileWidget {
    public:
      AggFileWidget();
      virtual ~AggFileWidget();
  
      AGGRRGATE_METHOD_DEF
  
      // nsIFileWidget
      virtual void            Create( nsIWidget *aParent,
                                      nsString& aTitle,
                                      nsMode aMode,
                                      nsIDeviceContext *aContext = nsnull,
                                      nsIToolkit *aToolkit = nsnull,
                                      void *aInitData = nsnull);

      virtual void            GetFile(nsString& aFile);
      virtual void            SetFilterList(PRUint32 aNumberOfFilters,
                                            const nsString aTitles[],
                                            const nsString aFilters[]);

      virtual PRBool          Show();
      virtual void            OnOk();
      virtual void            OnCancel();
    };
    AggFileWidget mAggWidget;


};

#endif // nsFileWidget_h__
