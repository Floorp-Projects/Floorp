/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
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
#ifdef NECKO
#include "nsNeckoUtil.h"
#endif // NECKO
#include "nsIDOMHTMLInputElement.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShellWindow.h"
#include "nsIDOMEventReceiver.h"
#include "nsIURL.h"

/* Define Class IDs */

static NS_DEFINE_IID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);

const static PRInt32 kCancelButton = 0;
const static PRInt32 kOKButton = 1;

/* Define Interface IDs */
static NS_DEFINE_IID(kIAppShellServiceIID,       NS_IAPPSHELL_SERVICE_IID);

static NS_DEFINE_IID(kIDOMMouseListenerIID,   NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID,   NS_IDOMEVENTRECEIVER_IID);
#ifndef NECKO
static NS_DEFINE_IID(kINetSupportDialogIID,   NS_INETSUPPORTDIALOGSERVICE_IID);
#endif
static NS_DEFINE_IID(kIFactoryIID,         NS_IFACTORY_IID);
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

#ifdef NECKO
NS_IMETHODIMP nsNetSupportDialog::Alert(const PRUnichar *text)
#else
NS_IMETHODIMP nsNetSupportDialog::Alert( const nsString &aText )
#endif
{
	Init();
#ifdef NECKO
  nsAutoString aText(text);
#endif
	mMsg = &aText;
	nsString  url( "chrome://navigator/content/NetSupportAlert.xul") ;
	DoDialog( url );
	return NS_OK;
}

#ifdef NECKO
NS_IMETHODIMP nsNetSupportDialog::Confirm(const PRUnichar *text, PRBool *returnValue)
#else
NS_IMETHODIMP nsNetSupportDialog::Confirm( const nsString &aText, PRInt32* returnValue )
#endif
{
	Init();
#ifdef NECKO
  nsAutoString aText(text);
#endif
	mMsg = &aText;
	mReturnValue = returnValue;
	nsString  url( "chrome://navigator/content/NetSupportConfirm.xul") ; 
	DoDialog( url  );
	return NS_OK;	
}

#ifdef NECKO
NS_IMETHODIMP	nsNetSupportDialog::ConfirmCheck(const PRUnichar *text, 
                                               const PRUnichar *checkMsg, 
                                               PRBool *checkValue, 
                                               PRBool *returnValue)
#else
NS_IMETHODIMP	nsNetSupportDialog::ConfirmCheck( const nsString &aText, const nsString& aCheckMsg, PRInt32* returnValue, PRBool* checkValue )
#endif
{
	Init();
#ifdef NECKO
  nsAutoString aText(text);
  nsAutoString aCheckMsg(checkMsg);
#endif
	mMsg = &aText;
	mReturnValue = returnValue;
	mCheckValue = checkValue;
	mCheckMsg = &aCheckMsg;
	nsString  url( "chrome://navigator/content/NetSupportConfirmCheck.xul") ; 
	DoDialog( url  );
	return NS_OK;	
}

#ifdef NECKO
NS_IMETHODIMP nsNetSupportDialog::Prompt(const PRUnichar *text,
                                         const PRUnichar *defaultText, 
                                         PRUnichar **resultText,
                                         PRBool *returnValue)
#else
NS_IMETHODIMP nsNetSupportDialog::Prompt(	const nsString &aText, const nsString &aDefault,nsString &aResult, PRInt32* returnValue )
#endif
{
  Init();
#ifdef NECKO
  nsAutoString aText(text);
  nsAutoString aDefault(defaultText);
  nsAutoString aResult;
#endif
	mMsg = &aText;
	mDefault = &aDefault;
	mUser	= &aResult;
	mReturnValue = returnValue;
	nsString  url( "chrome://navigator/content/NetSupportPrompt.xul")  ;
	DoDialog( url );
#ifdef NECKO
  *resultText = aResult.ToNewUnicode();
#endif
	return NS_OK;	
}

#ifdef NECKO
NS_IMETHODIMP nsNetSupportDialog::PromptUsernameAndPassword(const PRUnichar *text,
                                                            PRUnichar **user,
                                                            PRUnichar **pwd,
                                                            PRBool *returnValue)
#else
NS_IMETHODIMP nsNetSupportDialog::PromptUserAndPassword(  const nsString &aText,
                                        nsString &aUser,
                                        nsString &aPassword,PRInt32* returnValue )
#endif
{
	Init();
#ifdef NECKO
  nsAutoString aText(text);
  nsAutoString aUser;
  nsAutoString aPassword;
#endif
	mMsg = &aText;
	mUser = &aUser;
	mPassword	= &aPassword;
	mReturnValue = returnValue;
	nsString  url( "chrome://navigator/content/NetSupportUserPassword.xul")  ;
	DoDialog( url );
#ifdef NECKO
  *user = aUser.ToNewUnicode();
  *pwd = aPassword.ToNewUnicode();
#endif
	return NS_OK;	
}

#ifdef NECKO
NS_IMETHODIMP nsNetSupportDialog::PromptPassword(const PRUnichar *text, 
                                                 PRUnichar **pwd, 
                                                 PRBool *returnValue)
#else
NS_IMETHODIMP nsNetSupportDialog::PromptPassword( 	const nsString &aText,
                                     	nsString &aPassword, PRInt32* returnValue )
#endif
{
 	Init();
#ifdef NECKO
  nsAutoString aText(text);
  nsAutoString aPassword;
#endif
	mMsg = &aText;
	mPassword	= &aPassword;
	mReturnValue = returnValue;
	nsString  url( "chrome://navigator/content/NetSupportPassword.xul")  ;
	DoDialog( url );
#ifdef NECKO
  *pwd = aPassword.ToNewUnicode();
#endif
 	return NS_OK;	
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
#ifndef NECKO
  result = NS_NewURL(&dialogURL, inXULURL );
#else
  result = NS_NewURI(&dialogURL, inXULURL );
#endif // NECKO
  if (!NS_SUCCEEDED (result) )
  {
    appShellService->Release();
    return result;
  }

  result = appShellService->CreateTopLevelWindow(nsnull, dialogURL, PR_TRUE,
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

#ifdef NECKO
  if ( aIID.Equals( nsIPrompt::GetIID() ) )
  {
    *aInstancePtr = (void*) ((nsIPrompt*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
#else
  if ( aIID.Equals( kINetSupportDialogIID ) )
  {
    *aInstancePtr = (void*) ((nsINetSupportDialogService*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
#endif
  else if (aIID.Equals(kIDOMMouseListenerIID))
  {
    NS_ADDREF_THIS(); // Increase reference count for caller
    *aInstancePtr = (void *)((nsIDOMMouseListener*)this);
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
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
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

