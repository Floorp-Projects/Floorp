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
#include "nsIUrlWidget.h"
#include "nsIGenericFactory.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsCOMPtr.h"

#include <windows.h>

// {1802EE82-34A1-11d4-82EE-0050DA2DA771}
#define NS_IURLWIDGET_CID { 0x1802EE82, 0x34A1, 0x11d4, { 0x82, 0xEE, 0x00, 0x50, 0xDA, 0x2D, 0xA7, 0x71 } }

// Define this macro to turn on console debug output.                       
//#define DEBUG_URLWIDGET

// Implementation of the nsIUrlWidget interface.
class nsUrlWidget : public nsIUrlWidget {
public:

    nsUrlWidget();
    virtual ~nsUrlWidget();

private:
	
    // Declare all interface methods we must implement.
    NS_DECL_ISUPPORTS

// Simple initialization function.
NS_IMETHODIMP    
nsUrlWidget::Init()
{

   nsresult rv = NS_OK;

  return rv;
}

NS_IMETHODIMP
nsUrlWidget::SetURLToHiddenControl( char const *aURL, nsIDOMWindow *parent )
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
}; // End of nsUrlWidget class definition.

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


NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsUrlWidget, Init)

nsModuleComponentInfo components[] = {
  { NS_IURLWIDGET_CLASSNAME, 
    NS_IURLWIDGET_CID, 
    NS_IURLWIDGET_PROGID, 
	nsUrlWidgetConstructor },
};

NS_IMPL_NSGETMODULE( "nsUrlWidget", components )

