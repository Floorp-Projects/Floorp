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

#include "prprf.h"
#include "nsIScriptGlobalObject.h"

#include "nsIDOMWindow.h"
#include "nsIServiceManager.h"
#include "nsIDocumentViewer.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIContentViewer.h"
#include "nsIDOMElement.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsPIXPIManagerCallbacks.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsInstallProgressDialog::nsInstallProgressDialog(nsPIXPIManagerCallbacks *aManager)
  : mManager(aManager)
{
    NS_INIT_ISUPPORTS();
}

nsInstallProgressDialog::~nsInstallProgressDialog()
{
}


NS_IMPL_THREADSAFE_ADDREF( nsInstallProgressDialog );
NS_IMPL_THREADSAFE_RELEASE( nsInstallProgressDialog );

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
  nsresult rv = NS_ERROR_FAILURE;

  // build parameter list
  nsCOMPtr<nsISupportsArray> params(do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID));
  nsCOMPtr<nsISupportsInterfacePointer> pbwrap(do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID));
  if (pbwrap) {
    pbwrap->SetData(ioParamBlock);
    pbwrap->SetDataIID(&NS_GET_IID(nsIDialogParamBlock));
  }
  nsCOMPtr<nsPIXPIManagerCallbacks> mgr = do_QueryInterface(mManager);
  nsCOMPtr<nsISupportsInterfacePointer> callbackwrap(do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID));
  if (callbackwrap) {
    callbackwrap->SetData(mgr);
    callbackwrap->SetDataIID(&NS_GET_IID(nsPIXPIManagerCallbacks));
  }
  if (params && pbwrap && callbackwrap) {
    params->AppendElement(pbwrap);
    params->AppendElement(callbackwrap);

    // then open the window
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
    if (wwatch) {
      nsCOMPtr<nsIDOMWindow> newWindow;
      rv = wwatch->OpenWindow(0, "chrome://communicator/content/xpinstall/xpistatus.xul",
                     "_blank", "chrome,centerscreen,titlebar,resizable",
                     params, getter_AddRefs(newWindow));
      mWindow = do_QueryInterface(newWindow);
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
  nsresult rv = NS_OK;  
  const PRInt32 maxChars = 50;

    nsString theMessage(aActionText);
    PRInt32 len = theMessage.Length();
    if (len > 0)  // don't write message unless there's something to write
    {
        if (len > maxChars)
        {
            PRInt32 offset = (len/2) - ((len - maxChars)/2);
            PRInt32 count  = (len - maxChars);
            theMessage.Cut(offset, count);  
            theMessage.Insert(NS_LITERAL_STRING("..."), offset);
        }
        rv = setDlgAttribute( "dialog.currentAction", "value", theMessage );
    }

    return rv;
}

NS_IMETHODIMP
nsInstallProgressDialog::SetProgress(PRInt32 aValue, PRInt32 aMax, char mode)
{
    char buf[16];
    static char modeFlag = 'n';
    nsresult rv = NS_OK;

    if ( mode != modeFlag )
    {
        modeFlag = mode;
        if ( modeFlag == 'n' )
            rv = setDlgAttribute( "dialog.progress", "mode", NS_LITERAL_STRING("normal"));
        else
            rv = setDlgAttribute( "dialog.progress", "mode", NS_LITERAL_STRING("undetermined"));
    }

    if ( (NS_SUCCEEDED(rv)) && (modeFlag == 'n'))
    {
        if (aMax != 0)
            PR_snprintf( buf, sizeof buf, "%lu", 100 * aValue/aMax );
        else
            PR_snprintf( buf, sizeof buf, "%lu", 0 );

        rv = setDlgAttribute( "dialog.progress", "value", NS_ConvertASCIItoUCS2(buf));
    }
    return rv;
}

NS_IMETHODIMP
nsInstallProgressDialog::StartInstallPhase()
{
    nsresult rv = NS_OK;

    // don't care if this fails
    setDlgAttribute("cancel", "disabled", NS_LITERAL_STRING("true"));

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
                                                   const nsAReadableString &value )
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
              NS_WARNING("SetAttribute failed");
            }
        } else {
          NS_WARNING("GetElementById failed");
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

// Utility to get element attribute.
nsresult nsInstallProgressDialog::getDlgAttribute(  const char *id,
                                                    const char *name,
                                                    nsAWritableString &value ) 
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
              NS_WARNING("GetAttribute failed");
            }
        } else {
          NS_WARNING("GetElementById failed");
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}
