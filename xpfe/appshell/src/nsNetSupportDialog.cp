/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#include "nsIWebShell.h"
#include "nsIXULWindowCallbacks.h"
#include "nsIDOMXULDocument.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIDocumentViewer.h"
#include "nsCOMPtr.h"
#include "nsIPresContext.h"
#include "nsIDOMElement.h"
#include "nsIAppShellService.h"
#include "nsIServiceManager.h"
#include "nsAppShellCIDs.h"
#include "nsIURL.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsNetSupportDialog.h"

/* Define Class IDs */

static NS_DEFINE_IID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);



/* Define Interface IDs */
static NS_DEFINE_IID(kIAppShellServiceIID,       NS_IAPPSHELL_SERVICE_IID);


// Copy and paste
#define APP_DEBUG 1
static nsresult setAttribute( nsIWebShell *shell,
                              const char *id,
                              const char *name,
                              const nsString &value ) {
    nsresult rv = NS_OK;
  
    nsCOMPtr<nsIContentViewer> cv;
    rv = shell->GetContentViewer(getter_AddRefs(cv));
    if ( cv ) {
        // Up-cast.
        nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
        if ( docv ) {
            // Get the document from the doc viewer.
            nsCOMPtr<nsIDocument> doc;
            rv = docv->GetDocument(*getter_AddRefs(doc));
            if ( doc ) {
                // Up-cast.
                nsCOMPtr<nsIDOMXULDocument> xulDoc( do_QueryInterface(doc) );
                if ( xulDoc ) {
                    // Find specified element.
                    nsCOMPtr<nsIDOMElement> elem;
                    rv = xulDoc->GetElementById( id, getter_AddRefs(elem) );
                    if ( elem ) {
                        // Set the text attribute.
                        rv = elem->SetAttribute( name, value );
                        if ( APP_DEBUG ) {
                            char *p = value.ToNewCString();
                            //printf( "Set %s %s=\"%s\", rv=0x%08X\n", id, name, p, (int)rv );
                            delete [] p;
                        }
                        if ( rv != NS_OK ) {
                            if (APP_DEBUG) printf("SetAttribute failed, rv=0x%X\n",(int)rv);
                        }
                    } else {
                        if (APP_DEBUG) printf("GetElementByID failed, rv=0x%X\n",(int)rv);
                    }
                } else {
                    if (APP_DEBUG) printf("Upcast to nsIDOMXULDocument failed\n");
                }
            } else {
                if (APP_DEBUG) printf("GetDocument failed, rv=0x%X\n",(int)rv);
            }
        } else {
            if (APP_DEBUG) printf("Upcast to nsIDocumentViewer failed\n");
        }
    } else {
        if (APP_DEBUG) printf("GetContentViewer failed, rv=0x%X\n",(int)rv);
    }
    return rv;
}

static nsresult GetInputFieldValue( nsIWebShell *shell,
                              const char *id,
                               nsString &value ) {
    nsresult rv = NS_OK;
  
    nsCOMPtr<nsIContentViewer> cv;
    rv = shell->GetContentViewer(getter_AddRefs(cv));
    if ( cv ) {
        // Up-cast.
        nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
        if ( docv ) {
            // Get the document from the doc viewer.
            nsCOMPtr<nsIDocument> doc;
            rv = docv->GetDocument(*getter_AddRefs(doc));
            if ( doc ) {
                // Up-cast.
                nsCOMPtr<nsIDOMXULDocument> xulDoc( do_QueryInterface(doc) );
                if ( xulDoc ) {
                    // Find specified element.
                    nsCOMPtr<nsIDOMElement> elem;
                    rv = xulDoc->GetElementById( id, getter_AddRefs(elem) );
                    if ( elem ) {
                    	 nsCOMPtr<nsIDOMHTMLInputElement>  element( do_QueryInterface( elem ) );
     					 if ( element ){
       						 nsString str;
      						 element->GetValue(value);
        				 
        				}else
        				{
        				 if (APP_DEBUG) printf(" Get  nsIDOMHTMLInputElement failed, rv=0x%X\n",(int)rv);
        				}

                   } else {
                        if (APP_DEBUG) printf("GetElementByID failed, rv=0x%X\n",(int)rv);
                    }
                } else {
                    if (APP_DEBUG) printf("Upcast to nsIDOMXULDocument failed\n");
                }
            } else {
                if (APP_DEBUG) printf("GetDocument failed, rv=0x%X\n",(int)rv);
            }
        } else {
            if (APP_DEBUG) printf("Upcast to nsIDocumentViewer failed\n");
        }
    } else {
        if (APP_DEBUG) printf("GetContentViewer failed, rv=0x%X\n",(int)rv);
    }
    return rv;
}

nsNetSupportDialog::nsNetSupportDialog()
{
	Init();
}

void nsNetSupportDialog::Init()
{
	mDefault = NULL;
	mUser = NULL;
	mPassword = NULL;
	mMsg = NULL;
}

void nsNetSupportDialog::Alert( const nsString &aText )
{
	Init();
	mMsg = &aText;
	nsString  url( "resource:/res/samples/NetSupportAlert.xul") ;
	PRBool wasOked = false;
	DoDialog( url, wasOked );
}

PRBool nsNetSupportDialog::Confirm( const nsString &aText )
{
	Init();
	mMsg = &aText;
	nsString  url( "resource:/res/samples/NetSupportConfirm.xul") ; 
	PRBool wasOked = false;
	DoDialog( url, wasOked );
	return wasOked;
}

PRBool nsNetSupportDialog::Prompt(	const nsString &aText, const nsString &aDefault,nsString &aResult )
{
    Init();
	mMsg = &aText;
	mDefault = &aDefault;
	mUser	= &aResult;
	nsString  url( "resource:/res/samples/NetSupportPrompt.xul")  ;
	PRBool wasOked = false;
	DoDialog( url, wasOked );
	return wasOked;
}

PRBool nsNetSupportDialog::PromptUserAndPassword(  const nsString &aText,
                                        nsString &aUser,
                                        nsString &aPassword )
{
	Init();
	mMsg = &aText;
	mUser = &aUser;
	mPassword	= &aPassword;
	nsString  url( "resource:/res/samples/NetSupportUserPassword.xul")  ;
	PRBool wasOked = false;
	DoDialog( url, wasOked );
	return wasOked;
}

PRBool nsNetSupportDialog::PromptPassword( 	const nsString &aText,
                                     	nsString &aPassword )
 {
 	Init();
	mMsg = &aText;
	mPassword	= &aPassword;
	nsString  url( "resource:/res/samples/NetSupportPassword.xul")  ;
	PRBool wasOked = false;
	DoDialog( url, wasOked );
 	return wasOked;
 }



nsresult nsNetSupportDialog::ConstructBeforeJavaScript(nsIWebShell *aWebShell)
{
	
	 if ( aWebShell == NULL )
	 	return NS_ERROR_INVALID_ARG;
	 mWebShell = aWebShell;
	 mWebShell->AddRef();
	
	 if ( mMsg )
	 	setAttribute( aWebShell, "NetDialog:Message", "text", *mMsg );
	 
	
	return NS_OK;
}

nsresult nsNetSupportDialog::ConstructAfterJavaScript(nsIWebShell *aWebShell)
{
	
	return NS_OK;
}

nsresult nsNetSupportDialog::DoDialog(  nsString& inXULURL, PRBool &wasOKed )
{
	nsresult result;
   	// Create the Application Shell instance...
   	nsIAppShellService* appShellService = nsnull;
	if ( !NS_SUCCEEDED( 
  		result = nsServiceManager::GetService(kAppShellServiceCID, kIAppShellServiceIID, (nsISupports**)&appShellService) )
  	 )
  	{
  	 	return result;
  	}
  	

   if ( !NS_SUCCEEDED (result = appShellService->Initialize() ) )
   {
   		appShellService->Release();
  		return result;
  	}
  	
	
	
	nsIURL* dialogURL;
 	if (!NS_SUCCEEDED (result = NS_NewURL(&dialogURL, inXULURL ) ) )
 	{
 		appShellService->Release();
 		return result;
 	}
 	
 	nsIWebShellWindow* dialogWindow = NULL;
 	nsString controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";
 	result = appShellService->CreateDialogWindow( nsnull, dialogURL, controllerCID, dialogWindow, nsnull, this, 300,  150);

	// Run the dialog
	
	//  Get Results
	
	// cleanup
	appShellService->Release();
	dialogURL->Release();
//	dialogWindow->Release();
 	return NS_OK;	
}
#if 0
// Event Handlers which should be called using XPConnect eventually
NS_IMETHODIMP nsNetSupportDialog::OnOK()
{
	if ( mUser )
		GetInputFieldValue( mWebShell,"User" ,*mUser);
	if ( mPassword )
		GetInputFieldValue( mWebShell,"Password" ,*mPassword);
	// Fill in NetLib struct
	
	// Cleanup
	mWebShell->Release();
}

NS_IMETHODIMP nsNetSupportDialog::OnCancel()
{

	mWebShell->Release();
}
#endif
// COM Fluff
NS_IMPL_ADDREF(nsNetSupportDialog)
NS_IMPL_RELEASE(nsNetSupportDialog)


NS_IMETHODIMP nsNetSupportDialog::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL)
  {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;
/*
  if ( aIID.Equals( GetIID() ) ) {
    *aInstancePtr = (void*) ((nsIXULWindowCallbacks*)this);
    AddRef();
    return NS_OK;
  }
 */
  return NS_NOINTERFACE;
}
