/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Douglas Turner <dougt@netscape.com>
 *     Pierre Phaneuf <pp@ludusdesign.com>
 */


#include "nsInstallProgressDialog.h"

#include "nsIAppShellComponentImpl.h"


#include "nsIDOMWindowInternal.h"
#include "nsIServiceManager.h"
#include "nsIDocumentViewer.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIContentViewer.h"
#include "nsIDOMElement.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsPIXPIManagerCallbacks.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID( kAppShellServiceCID, NS_APPSHELL_SERVICE_CID );

static NS_DEFINE_CID(kDialogParamBlockCID, NS_DialogParamBlock_CID);

nsInstallProgressDialog::nsInstallProgressDialog(nsPIXPIManagerCallbacks *aManager)
  : mManager(aManager)
{
    NS_INIT_ISUPPORTS();
}

nsInstallProgressDialog::~nsInstallProgressDialog()
{
}


NS_IMPL_ADDREF( nsInstallProgressDialog );
NS_IMPL_RELEASE( nsInstallProgressDialog );

NS_IMETHODIMP 
nsInstallProgressDialog::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if (aIID.Equals(NS_GET_IID(nsIXPIListener))) {
    *aInstancePtr = (void*) ((nsIXPIListener*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIXPIProgressDlg))) {
    *aInstancePtr = (void*) ((nsIXPIProgressDlg*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) (nsISupports*)((nsIXPIListener*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP 
nsInstallProgressDialog::BeforeJavascriptEvaluation(const PRUnichar *URL)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsInstallProgressDialog::AfterJavascriptEvaluation(const PRUnichar *URL)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsInstallProgressDialog::InstallStarted(const PRUnichar *URL, const PRUnichar *UIPackageName)
{
    return SetHeading( UIPackageName );
}

NS_IMETHODIMP 
nsInstallProgressDialog::ItemScheduled(const PRUnichar *message)
{
    return SetActionText( message );
}

NS_IMETHODIMP 
nsInstallProgressDialog::FinalizeProgress(const PRUnichar *message, PRInt32 itemNum, PRInt32 totNum)
{

    nsresult rv = SetActionText( message );

    if (NS_SUCCEEDED(rv))
        rv = SetProgress( itemNum, totNum, 'n' );
    
    return rv;
}

NS_IMETHODIMP 
nsInstallProgressDialog::FinalStatus(const PRUnichar *URL, PRInt32 status)
{
    return NS_OK;
}


NS_IMETHODIMP 
nsInstallProgressDialog::LogComment(const PRUnichar* comment)
{
    return NS_OK;
}



NS_IMETHODIMP
nsInstallProgressDialog::Open(nsIDialogParamBlock* ioParamBlock)
{
    nsresult rv = NS_OK;
    
    // Now do the stuff to create a window and pass the JS args to it.
    NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv );
    if ( NS_SUCCEEDED( rv ) ) 
    {
        nsCOMPtr<nsIDOMWindowInternal> hiddenWindow;
        JSContext* jsContext;
        rv = appShell->GetHiddenWindowAndJSContext( getter_AddRefs(hiddenWindow), &jsContext);
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsPIXPIManagerCallbacks> mgr = do_QueryInterface(mManager);

            void* stackPtr;
            jsval *argv = JS_PushArguments( jsContext,
                                            &stackPtr,
                                            "sss%ip%ip",
                                            "chrome://communicator/content/xpinstall/xpistatus.xul",
                                            "_blank",
                                            "chrome,titlebar",
                                            (const nsIID*)&NS_GET_IID(nsIDialogParamBlock),
                                            (nsISupports*)ioParamBlock,
                                            (const nsIID*)&NS_GET_IID(nsPIXPIManagerCallbacks),
                                            (nsISupports*)mgr
                                            );
            if (argv)
            {
                rv = hiddenWindow->OpenDialog( jsContext,
                                               argv,
                                               5,
                                               getter_AddRefs( mWindow ));
            }

            JS_PopArguments( jsContext, stackPtr);  
        }
        
    }
    return rv;
}


NS_IMETHODIMP
nsInstallProgressDialog::Close()
{
    mWindow->Close();
    return NS_OK;
}

NS_IMETHODIMP
nsInstallProgressDialog::SetTitle(const PRUnichar * aTitle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsInstallProgressDialog::SetHeading(const PRUnichar * aHeading)
{
    return setDlgAttribute( "dialog.uiPackageName", "value", nsString(aHeading) );
}

NS_IMETHODIMP
nsInstallProgressDialog::SetActionText(const PRUnichar * aActionText)
{
    const PRInt32 maxChars = 50;

    nsString theMessage(aActionText);
    PRInt32 len = theMessage.Length();
    if (len > maxChars)
    {
        PRInt32 offset = (len/2) - ((len - maxChars)/2);
        PRInt32 count  = (len - maxChars);
        theMessage.Cut(offset, count);  
        theMessage.Insert(NS_ConvertASCIItoUCS2("..."), offset);
    }

    return setDlgAttribute( "dialog.currentAction", "value", theMessage );
}

NS_IMETHODIMP
nsInstallProgressDialog::SetProgress(PRInt32 aValue, PRInt32 aMax, char mode)
{
    char buf[16];
    static char modeFlag = 'n';
    nsresult rv;
    static PRInt32 previousMax;

    //First check to see if the max value changed so we don't
    //have to send a max value across the proxy every time.
    if ( aMax != previousMax)
    {
        previousMax = aMax;

        PR_snprintf( buf, sizeof buf, "%lu", aMax );
        rv = setDlgAttribute( "dialog.progress", "max", NS_ConvertASCIItoUCS2(buf) );
    }
    
    //I use this modeFlag business so I don't have to send
    //progressmeter mode information across the proxy every time.
    if ( mode != modeFlag )
    {
        modeFlag = mode;
        if ( modeFlag == 'n' )
            rv = setDlgAttribute( "dialog.progress", "mode", NS_ConvertASCIItoUCS2("normal"));
        else
            rv = setDlgAttribute( "dialog.progress", "mode", NS_ConvertASCIItoUCS2("undetermined"));
    }

    if ( NS_SUCCEEDED(rv))
    {
        if (aMax != 0)
            PR_snprintf( buf, sizeof buf, "%lu", 100 * aValue/aMax );
        else
            PR_snprintf( buf, sizeof buf, "%lu", 0 );

        rv = setDlgAttribute( "dialog.progress", "value", NS_ConvertASCIItoUCS2(buf) );
    }
    return rv;
}

NS_IMETHODIMP
nsInstallProgressDialog::StartInstallPhase()
{
    nsresult rv = NS_OK;

    // don't care if this fails
    setDlgAttribute("dialog.cancel", "disabled", NS_ConvertASCIItoUCS2("true"));

    return rv;
}

NS_IMETHODIMP
nsInstallProgressDialog::GetCancelStatus(PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_OK;
}

// Utility to set element attribute.
nsresult nsInstallProgressDialog::setDlgAttribute( const char *id,
                                                   const char *name,
                                                   const nsString &value )
{
    nsresult rv = NS_OK;

    if (!mDocument)
    {
        nsCOMPtr<nsIDOMDocument> doc;
        rv = mWindow->GetDocument( getter_AddRefs(doc) );
        if (NS_SUCCEEDED(rv))
        {
            mDocument = do_QueryInterface(doc,&rv);
        }
        NS_WARN_IF_FALSE(rv == NS_OK,"couldn't get nsIDOMXULDocument from nsXPIProgressDlg");
    }

    if ( mDocument ) {
        // Find specified element.
        nsCOMPtr<nsIDOMElement> elem;
        rv = mDocument->GetElementById( NS_ConvertASCIItoUCS2(id), getter_AddRefs( elem ) );
        if ( elem ) {
            // Set the text attribute.
            rv = elem->SetAttribute( NS_ConvertASCIItoUCS2(name), value );
            if ( NS_SUCCEEDED( rv ) ) {
            } else {
                 DEBUG_PRINTF( PR_STDOUT, "%s %d: SetAttribute failed, rv=0x%X\n",
                               __FILE__, (int)__LINE__, (int)rv );
            }
        } else {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: GetElementById failed, rv=0x%X\n",
                          __FILE__, (int)__LINE__, (int)rv );
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

// Utility to get element attribute.
nsresult nsInstallProgressDialog::getDlgAttribute(  const char *id,
                                                    const char *name,
                                                    nsString &value ) 
{
    nsresult rv = NS_OK;

    if (!mDocument)
    {
        nsCOMPtr<nsIDOMDocument> doc;
        rv = mWindow->GetDocument( getter_AddRefs(doc) );
        if (NS_SUCCEEDED(rv))
        {
            mDocument = do_QueryInterface(doc,&rv);
        }
        NS_WARN_IF_FALSE(rv == NS_OK,"couldn't get nsIDOMXULDocument from nsXPIProgressDlg");
    }

    if ( mDocument ) {
        // Find specified element.
        nsCOMPtr<nsIDOMElement> elem;
        rv = mDocument->GetElementById( NS_ConvertASCIItoUCS2(id), getter_AddRefs( elem ) );
        if ( elem ) {
            // Set the text attribute.
            rv = elem->GetAttribute( NS_ConvertASCIItoUCS2(name), value );
            if ( NS_SUCCEEDED( rv ) ) {
            } else {
                 DEBUG_PRINTF( PR_STDOUT, "%s %d: GetAttribute failed, rv=0x%X\n",
                               __FILE__, (int)__LINE__, (int)rv );
            }
        } else {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: GetElementById failed, rv=0x%X\n",
                          __FILE__, (int)__LINE__, (int)rv );
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}
