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
#include "nsIDOMHTMLInputElement.h"
#include "nsIWebShellWindow.H"
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
static NS_DEFINE_IID(kINetSupportDialogIID,   NS_INETSUPPORTDIALOGSERVICE_IID);
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
}

NS_IMETHODIMP nsNetSupportDialog::Alert( const nsString &aText )
{
	Init();
	mMsg = &aText;
	nsString  url( "resource:/res/samples/NetSupportAlert.xul") ;
	DoDialog( url );
	return NS_OK;
}

NS_IMETHODIMP nsNetSupportDialog::Confirm( const nsString &aText, PRInt32* returnValue )
{
	Init();
	mMsg = &aText;
	mReturnValue = returnValue;
	nsString  url( "resource:/res/samples/NetSupportConfirm.xul") ; 
	DoDialog( url  );
	return NS_OK;	
}

NS_IMETHODIMP nsNetSupportDialog::Prompt(	const nsString &aText, const nsString &aDefault,nsString &aResult, PRInt32* returnValue )
{
    Init();
	mMsg = &aText;
	mDefault = &aDefault;
	mUser	= &aResult;
	mReturnValue = returnValue;
	nsString  url( "resource:/res/samples/NetSupportPrompt.xul")  ;
	DoDialog( url );
	return NS_OK;	
}

NS_IMETHODIMP nsNetSupportDialog::PromptUserAndPassword(  const nsString &aText,
                                        nsString &aUser,
                                        nsString &aPassword,PRInt32* returnValue )
                                       
{
	Init();
	mMsg = &aText;
	mUser = &aUser;
	mPassword	= &aPassword;
	mReturnValue = returnValue;
	nsString  url( "resource:/res/samples/NetSupportUserPassword.xul")  ;
	DoDialog( url );
	return NS_OK;	
}

NS_IMETHODIMP nsNetSupportDialog::PromptPassword( 	const nsString &aText,
                                     	nsString &aPassword, PRInt32* returnValue )
{
 	Init();
	mMsg = &aText;
	mPassword	= &aPassword;
	mReturnValue = returnValue;
	nsString  url( "resource:/res/samples/NetSupportPassword.xul")  ;
	DoDialog( url );
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
	
	// Hook up the event listeners
	 findDOMNode( mWebShell,"OKButton", &mOKButton );
	 findDOMNode( mWebShell,"CancelButton", &mCancelButton );
	 
	 AddMouseEventListener( mOKButton );
	 AddMouseEventListener( mCancelButton );
	return NS_OK;
}

nsresult nsNetSupportDialog::ConstructAfterJavaScript(nsIWebShell *aWebShell)
{
	return NS_OK;
}

nsresult nsNetSupportDialog::DoDialog(  nsString& inXULURL  )
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

	nsIURL* dialogURL;
 	if (!NS_SUCCEEDED (result = NS_NewURL(&dialogURL, inXULURL ) ) )
 	{
 		appShellService->Release();
 		return result;
 	}
 	
 	nsString controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";
 	result = appShellService->CreateDialogWindow( nsnull, dialogURL, controllerCID, mWebShellWindow, nsnull, this, 300,  150);

	// Run the dialog
	// Results will be in the XUL callback
	if ( mWebShellWindow )
		mWebShellWindow->ShowModal();
	
	// cleanup
	if ( mOKButton )
		RemoveEventListener( mOKButton );
	if ( mCancelButton )
		RemoveEventListener( mCancelButton );
	appShellService->Release();
	dialogURL->Release();
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
	*mReturnValue = kOKButton;
	// Cleanup

	mWebShellWindow->Close();
}

void nsNetSupportDialog::OnCancel()
{
	*mReturnValue = kCancelButton;
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

  if ( aIID.Equals( kINetSupportDialogIID ) )
  {
    *aInstancePtr = (void*) ((nsINetSupportDialogService*)this);
    AddRef();
    return NS_OK;
  }
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