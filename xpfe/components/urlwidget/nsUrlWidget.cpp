/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *  Bill Law		<law@netscape.com>
 *  Jesse Burris	<jburris@mmxi.com>
 */

// Filename: nsIUrlWidget.cpp

#include "nsIDocShell.h"
#include "nsUrlWidget.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsCOMPtr.h"

#include <windows.h>

// Define this macro to turn on console debug output.                       
//#define DEBUG_URLWIDGET

// Implementation of the nsIUrlWidget interface.
// Simple initialization function.
nsresult    
nsUrlWidget::Init()
{

   nsresult rv = NS_OK;

  return rv;
}

NS_IMETHODIMP
nsUrlWidget::SetURLToHiddenControl( char const *aURL, nsIDOMWindowInternal *parent )
{
	nsresult rv = NS_OK;
	HWND	hEdit=NULL; // Handle to the hidden editbox control.
	HWND	hMainFrame=NULL;  // Handle to main frame window where our
							//editbox is attached.

    static const LONG editControlID = 12345;

    nsCOMPtr<nsIScriptGlobalObject> ppScriptGlobalObj( do_QueryInterface(parent) );
    if (!ppScriptGlobalObj)
    {
        return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIDocShell> ppDocShell;
    ppScriptGlobalObj->GetDocShell(getter_AddRefs(ppDocShell));
    if (!ppDocShell)
    {
        return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIBaseWindow> ppBaseWindow;
    ppDocShell->QueryInterface(NS_GET_IID(nsIBaseWindow), (void **)&ppBaseWindow);
    if (ppBaseWindow != NULL)
    {
        nsCOMPtr<nsIWidget> ppWidget;
        ppBaseWindow->GetMainWidget(getter_AddRefs(ppWidget));
        hMainFrame = (HWND)ppWidget->GetNativeData(NS_NATIVE_WIDGET);
    }

	if (!hMainFrame)
	{
		return NS_ERROR_FAILURE;
	}

    // See if edit control has been created already.
    hEdit = GetDlgItem( hMainFrame, 12345 );

    if ( !hEdit ) {
        ULONG visibility = 0;
        // Set this to WS_VISIBLE to debug.
        //visibility = WS_VISIBLE;

        hEdit = ::CreateWindow("Edit",
                "",
                WS_CHILD | WS_BORDER | visibility,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                200,
                15,
                hMainFrame,
                (HMENU)editControlID,
                NULL,
                NULL);
    }

	// OK.  If we have an editbox created, and a url, post it.
	if ((aURL != NULL) && (hEdit != NULL))
	{
        #ifdef DEBUG_URLWIDGET
        printf( "nsUrlWidget; window=0x%08X, url=[%s]\n", (int)hEdit, aURL );
        #endif
		::SendMessage(hEdit, WM_SETTEXT, (WPARAM)0, (LPARAM)aURL);
	}

    return rv;
}

// Use standard implementation of nsISupports stuff.
NS_IMPL_ISUPPORTS1( nsUrlWidget, nsIUrlWidget );

nsUrlWidget::nsUrlWidget() {
  NS_INIT_ISUPPORTS();
#ifdef DEBUG_URLWIDGET
printf( "nsUrlWidget ctor called\n" );
#endif
}


nsUrlWidget::~nsUrlWidget() {
#ifdef DEBUG_URLWIDGET
printf( "nsUrlWidget dtor called\n" );
#endif
}
