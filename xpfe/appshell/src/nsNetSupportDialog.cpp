/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsNetSupportDialog.h"
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
#include "nsNetUtil.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShellWindow.h"
#include "nsIDOMEventReceiver.h"
#include "nsIURL.h"
#include "nsICommonDialogs.h"
#include "nsIWindowMediator.h"
#include "nsIDOMWindow.h"
/* Define Class IDs */

static NS_DEFINE_IID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);

const static PRInt32 kCancelButton = 0;
const static PRInt32 kOKButton = 1;

/* Define Interface IDs */
static NS_DEFINE_IID(kIAppShellServiceIID,       NS_IAPPSHELL_SERVICE_IID);

static NS_DEFINE_IID(kIDOMMouseListenerIID,   NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID,   NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIFactoryIID,         NS_IFACTORY_IID);
static NS_DEFINE_IID(kISupportsIID,         NS_ISUPPORTS_IID);

static NS_DEFINE_CID( kCommonDialogsCID,          NS_CommonDialog_CID);
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);
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
                          //char *p = value.ToNewCString();
                            //printf( "Set %s %s=\"%s\", rv=0x%08X\n", id, name, p, (int)rv );
                          //delete [] p;
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

static nsresult GetCheckboxValue( nsIWebShell *shell,
                              const char *id,
                               PRBool &value ) {
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
       			
      						 element->GetChecked(&value);
        				 
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


static nsresult SetCheckboxValue( nsIWebShell *shell, 
                              const char *id, 
                               PRBool value ) { 
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

             element->SetChecked(value); 

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

static nsresult findDOMNode( nsIWebShell *shell,
                              const char *id,
                              nsIDOMElement **node )
  {
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
                    rv = xulDoc->GetElementById( id, node );
                    
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
	NS_INIT_REFCNT();
	Init();
}

nsNetSupportDialog::~nsNetSupportDialog()
{
  // just making sure I understand what I'm doing...
  NS_ASSERTION( !mWebShellWindow, "webshell window still exists in ~nsNetSupportDialog" );

	NS_IF_RELEASE( mWebShell );
	NS_IF_RELEASE( mWebShellWindow );
	NS_IF_RELEASE( mOKButton );
	NS_IF_RELEASE( mCancelButton );
}

void nsNetSupportDialog::Init()
{
	mDefault = NULL;
	mUser = NULL;
	mPassword = NULL;
	mMsg = NULL;
	mReturnValue = NULL;
	mOKButton = NULL;
	mCancelButton = NULL;
	mCheckValue = NULL;
 	mCheckMsg = NULL;
  mWebShell = NULL;
  mWebShellWindow = NULL;
}

NS_IMETHODIMP nsNetSupportDialog::Alert(const PRUnichar *text)
{
	 nsresult rv;
	 NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
	 if ( NS_SUCCEEDED ( rv ) )
	 {
	 	nsCOMPtr< nsIDOMWindow> window;
	 	windowMediator->GetMostRecentWindow( NULL, getter_AddRefs( window ) );
	 	nsCOMPtr<nsICommonDialogs> dialogService;
	 	rv = nsComponentManager::CreateInstance( kCommonDialogsCID,0, nsICommonDialogs::GetIID(),
                                                      (void**)&dialogService );
        if( NS_SUCCEEDED ( rv ) )
        	rv = dialogService->Alert( window, NULL, text );
	 
	 }
	 return rv;
#if 0
	Init();
  nsAutoString aText(text);
	mMsg = &aText;
	nsString  url( "chrome://navigator/content/NetSupportAlert.xul") ;
	DoDialog( url );
	return NS_OK;
#endif
}

NS_IMETHODIMP nsNetSupportDialog::Confirm(const PRUnichar *text, PRBool *returnValue)
{

 nsresult rv;
	 NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
	 if ( NS_SUCCEEDED ( rv ) )
	 {
	 	nsCOMPtr< nsIDOMWindow> window;
	 	windowMediator->GetMostRecentWindow( NULL, getter_AddRefs( window ) );
	 	nsCOMPtr<nsICommonDialogs> dialogService;
	 	rv = nsComponentManager::CreateInstance( kCommonDialogsCID,0, nsICommonDialogs::GetIID(),
                                                      (void**)&dialogService );
        if( NS_SUCCEEDED ( rv ) )
        	rv = dialogService->Confirm( window, NULL, text, returnValue );
	 
	 }
	 return rv;
#if 0
	Init();
  nsAutoString aText(text);
	mMsg = &aText;
	mReturnValue = returnValue;
	nsString  url( "chrome://navigator/content/NetSupportConfirm.xul") ; 
	DoDialog( url  );
	return NS_OK;
#endif
}

NS_IMETHODIMP	nsNetSupportDialog::ConfirmCheck(const PRUnichar *text, 
                                               const PRUnichar *checkMsg, 
                                               PRBool *checkValue, 
                                               PRBool *returnValue)
{
 nsresult rv;
	 NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
	 if ( NS_SUCCEEDED ( rv ) )
	 {
	 	nsCOMPtr< nsIDOMWindow> window;
	 	windowMediator->GetMostRecentWindow( NULL, getter_AddRefs( window ) );
	 	nsCOMPtr<nsICommonDialogs> dialogService;
	 	rv = nsComponentManager::CreateInstance( kCommonDialogsCID,0, nsICommonDialogs::GetIID(),
                                                      (void**)&dialogService );
        if( NS_SUCCEEDED ( rv ) )
        	rv = dialogService->ConfirmCheck( window,NULL, text, checkMsg, checkValue, returnValue );
	 
	 }
	 return rv;
#if 0
	Init();
  nsAutoString aText(text);
  nsAutoString aCheckMsg(checkMsg);
	mMsg = &aText;
	mReturnValue = returnValue;
	mCheckValue = checkValue;
	mCheckMsg = &aCheckMsg;
	nsString  url( "chrome://navigator/content/NetSupportConfirmCheck.xul") ; 
	DoDialog( url  );
	return NS_OK;
#endif
}

NS_IMETHODIMP	nsNetSupportDialog::UniversalDialog
	(const PRUnichar *inTitleMessage,
	const PRUnichar *inDialogTitle, /* e.g., alert, confirm, prompt, prompt password */
	const PRUnichar *inMsg, /* main message for dialog */
	const PRUnichar *inCheckboxMsg, /* message for checkbox */
	const PRUnichar *inButton0Text, /* text for first button */
	const PRUnichar *inButton1Text, /* text for second button */
	const PRUnichar *inButton2Text, /* text for third button */
	const PRUnichar *inButton3Text, /* text for fourth button */
	const PRUnichar *inEditfield1Msg, /*message for first edit field */
	const PRUnichar *inEditfield2Msg, /* message for second edit field */
	PRUnichar **inoutEditfield1Value, /* initial and final value for first edit field */
	PRUnichar **inoutEditfield2Value, /* initial and final value for second edit field */
	const PRUnichar *inIConURL, /* url of icon to be displayed in dialog */
		/* examples are
		   "chrome://global/skin/question-icon.gif" for question mark,
		   "chrome://global/skin/alert-icon.gif" for exclamation mark
		*/
	PRBool *inoutCheckboxState, /* initial and final state of check box */
	PRInt32 inNumberButtons, /* total number of buttons (0 to 4) */
	PRInt32 inNumberEditfields, /* total number of edit fields (0 to 2) */
	PRInt32 inEditField1Password, /* is first edit field a password field */
	PRInt32 *outButtonPressed) /* number of button that was pressed (0 to 3) */
{
 nsresult rv;
	 NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
	 if ( NS_SUCCEEDED ( rv ) )
	 {
	 	nsCOMPtr< nsIDOMWindow> window;
	 	windowMediator->GetMostRecentWindow( NULL, getter_AddRefs( window ) );
	 	nsCOMPtr<nsICommonDialogs> dialogService;
	 	rv = nsComponentManager::CreateInstance( kCommonDialogsCID,0, nsICommonDialogs::GetIID(),
                                                      (void**)&dialogService );

        if( NS_SUCCEEDED ( rv ) )
        	rv = dialogService->UniversalDialog(
                        window, inTitleMessage, inDialogTitle, inMsg, inCheckboxMsg,
                        inButton0Text, inButton1Text, inButton2Text, inButton3Text,
                        inEditfield1Msg, inEditfield2Msg, inoutEditfield1Value,
                        inoutEditfield2Value, inIConURL, inoutCheckboxState, inNumberButtons,
                        inNumberEditfields, inEditField1Password, outButtonPressed);
	 }
	 return rv;
}

NS_IMETHODIMP nsNetSupportDialog::Prompt(const PRUnichar *text,
                                         const PRUnichar *defaultText, 
                                         PRUnichar **resultText,
                                         PRBool *returnValue)
{
 nsresult rv;
	 NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
	 if ( NS_SUCCEEDED ( rv ) )
	 {
	 	nsCOMPtr< nsIDOMWindow> window;
	 	windowMediator->GetMostRecentWindow( NULL, getter_AddRefs( window ) );
	 	nsCOMPtr<nsICommonDialogs> dialogService;
	 	rv = nsComponentManager::CreateInstance( kCommonDialogsCID,0, nsICommonDialogs::GetIID(),
                                                      (void**)&dialogService );
        if( NS_SUCCEEDED ( rv ) )
        	rv = dialogService->Prompt( window, NULL, text, defaultText, resultText, returnValue );
	 
	 }
	 return rv;
#if 0
  Init();
  nsAutoString aText(text);
  nsAutoString aDefault(defaultText);
  nsAutoString aResult;
	mMsg = &aText;
	mDefault = &aDefault;
	mUser	= &aResult;
	mReturnValue = returnValue;
	nsString  url( "chrome://navigator/content/NetSupportPrompt.xul")  ;
	DoDialog( url );
  *resultText = aResult.ToNewUnicode();
	return NS_OK;	
#endif
}

NS_IMETHODIMP nsNetSupportDialog::PromptUsernameAndPassword(const PRUnichar *text,
                                                            PRUnichar **user,
                                                            PRUnichar **pwd,
                                                            PRBool *returnValue)
{
	 nsresult rv;
	 NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
	 if ( NS_SUCCEEDED ( rv ) )
	 {
	 	nsCOMPtr< nsIDOMWindow> window;
	 	windowMediator->GetMostRecentWindow( NULL, getter_AddRefs( window ) );
	 	nsCOMPtr<nsICommonDialogs> dialogService;
	 	rv = nsComponentManager::CreateInstance( kCommonDialogsCID,0, nsICommonDialogs::GetIID(),
                                                      (void**)&dialogService );
        if( NS_SUCCEEDED ( rv ) )
        	rv = dialogService->PromptUsernameAndPassword( window, NULL,text, user, pwd, returnValue );
	 
	 }
	 return rv;
#if 0
	Init();
  nsAutoString aText(text);
  nsAutoString aUser;
  nsAutoString aPassword;
	mMsg = &aText;
	mUser = &aUser;
	mPassword	= &aPassword;
	mReturnValue = returnValue;
	nsString  url( "chrome://navigator/content/NetSupportUserPassword.xul")  ;
	DoDialog( url );
  *user = aUser.ToNewUnicode();
  *pwd = aPassword.ToNewUnicode();
	return NS_OK;
#endif	
}

NS_IMETHODIMP nsNetSupportDialog::PromptPassword(const PRUnichar *text,
						 const PRUnichar *windowTitle,
                                                 PRUnichar **pwd, 
                                                 PRBool *returnValue)
{
	 nsresult rv;
	 NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
	 if ( NS_SUCCEEDED ( rv ) )
	 {
	 	nsCOMPtr< nsIDOMWindow> window;
	 	windowMediator->GetMostRecentWindow( NULL, getter_AddRefs( window ) );
	 	nsCOMPtr<nsICommonDialogs> dialogService;
	 	rv = nsComponentManager::CreateInstance( kCommonDialogsCID,0, nsICommonDialogs::GetIID(),
                                                      (void**)&dialogService );
        if( NS_SUCCEEDED ( rv ) )
        	rv = dialogService->PromptPassword( window, windowTitle,text, pwd, returnValue );
	 
	 }
	 return rv;
#if 0
 	Init();
  nsAutoString aText(text);
  nsAutoString aPassword;
	mMsg = &aText;
	mPassword	= &aPassword;
	mReturnValue = returnValue;
	nsString  url( "chrome://navigator/content/NetSupportPassword.xul")  ;
	DoDialog( url );
  *pwd = aPassword.ToNewUnicode();
 	return NS_OK;	
#endif
}

nsresult nsNetSupportDialog::Select(const PRUnichar *inDialogTitle, const PRUnichar *inMsg, PRUint32 inCount, const char **inList, PRInt32 *outSelection, PRBool *_retval)
{
	 nsresult rv;
	 NS_WITH_SERVICE(nsIAppShellService, appshellservice, kAppShellServiceCID, &rv);
      if(NS_FAILED(rv))
          return rv;
      nsCOMPtr<nsIWebShellWindow> webshellwindow;
      appshellservice->GetHiddenWindow(getter_AddRefs( webshellwindow ) );
     nsCOMPtr<nsIPrompt> prompter(do_QueryInterface( webshellwindow ));
     PRInt32 selectedIndex;
     rv = prompter->Select( inDialogTitle, inMsg, inCount, inList, outSelection,_retval );
     *outSelection = selectedIndex;
     return rv;
}

nsresult nsNetSupportDialog::ConstructBeforeJavaScript(nsIWebShell *aWebShell)
{
	
	 if ( aWebShell == NULL )
	 	return NS_ERROR_INVALID_ARG;
	 mWebShell = aWebShell;
	 mWebShell->AddRef();
	
   if ( mMsg )
     setAttribute( aWebShell, "NetDialog:Message", "text", *mMsg );
   if( mCheckMsg )
     setAttribute( aWebShell, "NetDialog:CheckMessage", "text", *mCheckMsg );
	 	
	// Hook up the event listeners
	 findDOMNode( mWebShell,"OKButton", &mOKButton );
	 findDOMNode( mWebShell,"CancelButton", &mCancelButton );
	 if ( mOKButton )
	 	 	AddMouseEventListener( mOKButton );
	 if ( mCancelButton )
			AddMouseEventListener( mCancelButton );
	 if ( mCheckValue ) 
			SetCheckboxValue( mWebShell, "checkbox", *mCheckValue ); 
	return NS_OK;
}

nsresult nsNetSupportDialog::ConstructAfterJavaScript(nsIWebShell *aWebShell)
{
	return NS_OK;
}

nsresult nsNetSupportDialog::DoDialog(  nsString& inXULURL  )
{
  nsresult result;
  nsIWebShellWindow *dialogWindow;

 	// Create the Application Shell instance...
  NS_WITH_SERVICE(nsIAppShellService, appShellService, kAppShellServiceCID, &result);

  if ( !NS_SUCCEEDED ( result ) )
    return result;

  nsIURI* dialogURL;
  result = NS_NewURI(&dialogURL, inXULURL );
  if ( !NS_SUCCEEDED (result) )
    return result;

  result = appShellService->CreateTopLevelWindow(nsnull, dialogURL,
                              PR_TRUE, PR_TRUE,
                              NS_CHROME_ALL_CHROME | NS_CHROME_OPEN_AS_DIALOG,
                              this, 300, 200, &dialogWindow);
  mWebShellWindow = dialogWindow;

  if (NS_SUCCEEDED(result))
    appShellService->RunModalDialog(&dialogWindow, nsnull, dialogURL,
                       NS_CHROME_ALL_CHROME | NS_CHROME_OPEN_AS_DIALOG,
                       this, 300, 200);

  // cleanup
  if ( mOKButton )
    RemoveEventListener( mOKButton );
  if ( mCancelButton )
    RemoveEventListener( mCancelButton );
  dialogURL->Release();
  NS_RELEASE( mWebShellWindow );

  return NS_OK;	
}

// Event Handlers which should be called using XPConnect eventually
void nsNetSupportDialog::OnOK()
{
	if ( mUser )
		GetInputFieldValue( mWebShell,"User" ,*mUser);
	if ( mPassword )
		GetInputFieldValue( mWebShell,"Password" ,*mPassword);
	// Fill in NetLib struct
  if ( mReturnValue ) 
    *mReturnValue = kOKButton;
	if ( mCheckValue )
		GetCheckboxValue( mWebShell, "checkbox", *mCheckValue );
	// Cleanup

  NS_ASSERTION(mWebShellWindow, "missing webshell window in NetSupportDalog::OnOK");
  if (mWebShellWindow)
	  mWebShellWindow->Close();
}

void nsNetSupportDialog::OnCancel()
{
	*mReturnValue = kCancelButton;
	if ( mCheckValue ) 
		GetCheckboxValue( mWebShell, "checkbox", *mCheckValue ); 
  NS_ASSERTION(mWebShellWindow, "missing webshell window in NetSupportDalog::OnCancel");
  if (mWebShellWindow)
  	mWebShellWindow->Close();
}

nsresult nsNetSupportDialog::MouseClick(nsIDOMEvent* aMouseEvent)
{

    nsIDOMNode * node;
    aMouseEvent->GetTarget(&node);
    if (node == mOKButton)
    {
     	OnOK();
    } else if ( node == mCancelButton )
    {
    	OnCancel();
    }
    
    NS_RELEASE(node);
	return NS_OK;
}

void nsNetSupportDialog::AddMouseEventListener(nsIDOMNode * aNode)
{
  nsIDOMEventReceiver * receiver;

  NS_PRECONDITION(nsnull != aNode, "adding event listener to null node");

  if ( NS_SUCCEEDED(aNode->QueryInterface(kIDOMEventReceiverIID, (void**) &receiver) ) )
  {
    receiver->AddEventListenerByIID((nsIDOMMouseListener*)this, kIDOMMouseListenerIID);
    NS_RELEASE(receiver);
  }
}

//-----------------------------------------------------------------
void nsNetSupportDialog::RemoveEventListener(nsIDOMNode * aNode)
{
  nsIDOMEventReceiver * receiver;

  NS_PRECONDITION(nsnull != aNode, "removing event listener from null node");

  if (NS_OK == aNode->QueryInterface(kIDOMEventReceiverIID, (void**) &receiver)) {
    receiver->RemoveEventListenerByIID(this, kIDOMMouseListenerIID);
    NS_RELEASE(receiver);
   
  }
}

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

  if ( aIID.Equals( nsIPrompt::GetIID() ) )
  {
    *aInstancePtr = (void*) ((nsIPrompt*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(kIDOMMouseListenerIID))
  {
    NS_ADDREF_THIS(); // Increase reference count for caller
    *aInstancePtr = (void *)((nsIDOMMouseListener*)this);
    return NS_OK;
  }
  else if (aIID.Equals(kISupportsIID))
  {
    NS_ADDREF_THIS(); // Increase reference count for caller
    *aInstancePtr = (void *)((nsISupports*)(nsIPrompt*)this);
    return NS_OK;
  }
  
  return NS_NOINTERFACE;
}


//----------------------------------------------------------------------

// Factory code for creating nsGlobalHistory

class nsNetSupportDialogFactory : public nsIFactory
{
public:
  nsNetSupportDialogFactory();
  NS_DECL_ISUPPORTS

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);
  
  NS_IMETHOD LockFactory(PRBool aLock);  
protected:
  virtual ~nsNetSupportDialogFactory();
};


nsNetSupportDialogFactory::nsNetSupportDialogFactory()
{
  NS_INIT_REFCNT();
}

nsresult
nsNetSupportDialogFactory::LockFactory(PRBool lock)
{

  return NS_OK;
}

nsNetSupportDialogFactory::~nsNetSupportDialogFactory()
{
}

NS_IMPL_ISUPPORTS(nsNetSupportDialogFactory, kIFactoryIID);


nsresult nsNetSupportDialogFactory::CreateInstance(nsISupports *aOuter,
                                  const nsIID &aIID,
                                  void **aResult)
{
  nsresult rv;
  nsNetSupportDialog* inst;

  if (aResult == NULL)
  {
  	return NS_ERROR_NULL_POINTER;
  }
  *aResult = NULL;
  if (nsnull != aOuter)
  {
  	rv = NS_ERROR_NO_AGGREGATION;
    goto done;
  }

  NS_NEWXPCOM(inst, nsNetSupportDialog);
  if (inst == NULL)
  {
  	rv = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

done:
  return rv;
}


nsresult NS_NewNetSupportDialogFactory(nsIFactory** aFactory)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsNetSupportDialogFactory();
  if (nsnull == inst)
  {
  	rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else
  {
    NS_ADDREF(inst);
  }
  *aFactory = inst;
  return rv;
}

