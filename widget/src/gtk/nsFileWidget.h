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
#include "nsWidgetsCID.h"
#include "nsIWidget.h"
#include "nsIFileWidget.h"

#include <gtk/gtk.h>

/**
 * Native GTK FileSelector wrapper
 */

class nsFileWidget : public nsIFileWidget 
{
  public:
    nsFileWidget(); 
    virtual ~nsFileWidget();

    NS_DECL_ISUPPORTS

    PRBool		OnPaint();

    // nsIWidget interface
  
    NS_IMETHOD		Create(nsIWidget *aParent,
                               nsString& aTitle,
			       nsMode aMode,
			       nsIDeviceContext *aContext = nsnull,
			       nsIAppShell *aAppShell = nsnull,
			       nsIToolkit *aToolkit = nsnull,
			       void *aInitData = nsnull);

    // nsIFileWidget part
    virtual PRBool	Show();
    NS_IMETHOD		GetFile(nsString& aFile);
    NS_IMETHOD GetFile(nsFileSpec& aFile);
    NS_IMETHOD		SetDefaultString(nsString& aFile);
    NS_IMETHOD		SetFilterList(PRUint32 aNumberOfFilters,
				      const nsString aTitles[],
				      const nsString aFilters[]);

    NS_IMETHOD GetDisplayDirectory(nsString& aDirectory);
    NS_IMETHOD SetDisplayDirectory(nsString& aDirectory);
  
  protected:
    GtkWidget		*mWidget;
    nsString		mTitle;
    nsMode		  mMode;
    PRUint32		mNumberOfFilters;  
    const nsString*	mTitles;
    const nsString*	mFilters;
    nsString		mDefault;
    nsString    mDisplayDirectory;
};

#endif // nsFileWidget_h__
