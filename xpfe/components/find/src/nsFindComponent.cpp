/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIFindComponent.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "pratom.h"
#include "prio.h"
#include "prprf.h"

#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsITextServicesDocument.h"
#include "nsTextServicesCID.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIDOMWindowInternal.h"

#include "nsIURL.h"

#include "nsIServiceManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsISupportsPrimitives.h"

#include "nsISound.h"
#include "nsWidgetsCID.h"   // ugh! contractID, please

#include "nsFindComponent.h"
#include "nsIFindAndReplace.h"
#include "nsIEditor.h"

#include "nsIGenericFactory.h"

#ifdef DEBUG
#define DEBUG_FIND
#define DEBUG_PRINTF PR_fprintf
#else
#define DEBUG_PRINTF (void)
#endif


/*

    Warning: this code is soon to become obsolete. The Find code has moved
    into mozilla/embedding/components/find, and is now shared between
    embedding apps and mozilla.
    
    This code remains because editor needs its find and replace functionality,
    for now.
    
    Simon Fraser sfraser@netscape.com

*/


nsFindComponent::Context::Context()
{
  NS_INIT_REFCNT();
  // all our other members are self-initiating
}


nsFindComponent::Context::~Context()
{
    #ifdef DEBUG_law
    printf( "\nnsFindComponent::Context destructor called\n\n" );
    #endif

    // Close the dialog (if there is one).
    if ( mFindDialog ) {
        mFindDialog->Close();
        mFindDialog = 0;
    }

    // Close the dialog (if there is one).
    if ( mReplaceDialog ) {
        mReplaceDialog->Close();
        mReplaceDialog = 0;
    }
}

// nsFindComponent::Context implementation...
NS_IMPL_ISUPPORTS1( nsFindComponent::Context, nsISearchContext)

NS_IMETHODIMP
nsFindComponent::Context::Init( nsIDOMWindowInternal *aWindow,
                 nsIEditorShell* aEditorShell,
                 const nsString& lastSearchString,
                 const nsString& lastReplaceString,
                 PRBool lastCaseSensitive,
                 PRBool lastSearchBackward,
                 PRBool lastWrapSearch)
{
  if (!aWindow)
    return NS_ERROR_INVALID_ARG;
	
  mEditorShell     = aEditorShell;				// don't AddRef
  mTargetWindow    = aWindow;			// don't AddRef
  mSearchString    = lastSearchString;
  mReplaceString   = lastReplaceString;
  mCaseSensitive   = lastCaseSensitive;
  mSearchBackwards = lastSearchBackward;
  mWrapSearch      = lastWrapSearch;
  mFindDialog      = 0;
  mReplaceDialog      = 0;

  nsresult rv = NS_OK;
  mTSFind = do_CreateInstance(NS_FINDANDREPLACE_CONTRACTID, &rv);

  return rv;
}


static NS_DEFINE_CID(kCTextServicesDocumentCID, NS_TEXTSERVICESDOCUMENT_CID);


NS_IMETHODIMP
nsFindComponent::Context::MakeTSDocument(nsIDOMWindowInternal* aWindow, nsITextServicesDocument** aDoc)
{
  if (!aWindow)
    return NS_ERROR_INVALID_ARG;
    
  if (!aDoc)
    return NS_ERROR_NULL_POINTER;

  *aDoc = NULL;
  
  // Create the text services document.
  nsCOMPtr<nsITextServicesDocument>  tempDoc;
  nsresult rv = nsComponentManager::CreateInstance(kCTextServicesDocumentCID,
                                                      nsnull,
                                                      NS_GET_IID(nsITextServicesDocument),
                                                      getter_AddRefs(tempDoc));
  if (NS_FAILED(rv) || !tempDoc)
    return rv;

  if (mEditorShell)
  {
    nsCOMPtr<nsIEditor> editor;
    rv = mEditorShell->GetEditor(getter_AddRefs(editor));
    if (NS_FAILED(rv))
      return rv;
    if (!editor)
      return NS_ERROR_FAILURE;
    rv = tempDoc->InitWithEditor(editor);
    if (NS_FAILED(rv))
      return rv;
  }
  else
  {
    nsCOMPtr<nsIScriptGlobalObject> globalObj = do_QueryInterface(aWindow, &rv);
    if (NS_FAILED(rv) || !globalObj)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocShell> docShell;
    globalObj->GetDocShell(getter_AddRefs(docShell));
    if (!docShell)
      return NS_ERROR_FAILURE;
	
    nsCOMPtr<nsIPresShell> presShell;
    docShell->GetPresShell(getter_AddRefs(presShell));
    if (!presShell)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocument> document;
    presShell->GetDocument(getter_AddRefs(document));
    nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(document);
    if (!domDoc)
      return NS_ERROR_FAILURE;

    // Initialize the text services document.
    rv = tempDoc->InitWithDocument(domDoc, presShell);
    if (NS_FAILED(rv))
      return rv;
  }

  // Return the resulting text services document.
  *aDoc = tempDoc;
  NS_IF_ADDREF(*aDoc);
  
  return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::DoFind(PRBool *aDidFind)
{
  if (!aDidFind)
    return NS_ERROR_NULL_POINTER;

  *aDidFind = PR_FALSE;
  
  if (!mTargetWindow)
    return NS_ERROR_NOT_INITIALIZED;
	
  nsresult	rv = NS_OK;

  // Construct a text services document to use. This is freed when we
  // return from this function.
  nsCOMPtr<nsITextServicesDocument> txtDoc;
  rv = MakeTSDocument(mTargetWindow, getter_AddRefs(txtDoc));
  if (NS_FAILED(rv) || !txtDoc)
    return rv;
  
  if (!mTSFind)
    return NS_ERROR_NOT_INITIALIZED;

  rv = mTSFind->SetCaseSensitive(mCaseSensitive);
  rv = mTSFind->SetFindBackwards(mSearchBackwards);
  rv = mTSFind->SetWrapFind(mWrapSearch);

  rv = mTSFind->SetTsDoc(txtDoc);
  if (NS_FAILED(rv))
    return rv;

  rv =  mTSFind->Find(mSearchString.get(), aDidFind);

  mTSFind->SetTsDoc(nsnull);

  return rv;
}


NS_IMETHODIMP
nsFindComponent::Context::DoReplace(PRBool aAllOccurrences, PRBool *aDidFind)
{
  if (!mTargetWindow)
    return NS_ERROR_NOT_INITIALIZED;

  if (!aDidFind)
    return NS_ERROR_NULL_POINTER;

  *aDidFind = PR_FALSE;

  nsresult	rv = NS_OK;

  // Construct a text services document to use. This is freed when we
  // return from this function.
  nsCOMPtr<nsITextServicesDocument> txtDoc;
  rv = MakeTSDocument(mTargetWindow, getter_AddRefs(txtDoc));
  if (NS_FAILED(rv) || !txtDoc)
    return rv;
  
  if (!mTSFind)
    return NS_ERROR_NOT_INITIALIZED;

  rv = mTSFind->SetCaseSensitive(mCaseSensitive);
  rv = mTSFind->SetFindBackwards(mSearchBackwards);
  rv = mTSFind->SetWrapFind(mWrapSearch);

  rv = mTSFind->SetTsDoc(txtDoc);
  if (NS_FAILED(rv))
    return rv;

  rv =  mTSFind->Replace(mSearchString.get(),
                              mReplaceString.get(),
                              aAllOccurrences, aDidFind);

  mTSFind->SetTsDoc(nsnull);

  return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::Reset( nsIDOMWindowInternal *aNewWindow )
{
	if (!aNewWindow)
		return NS_ERROR_INVALID_ARG;

  mTargetWindow = aNewWindow;		// don't AddRef
  return NS_OK;
}

NS_IMETHODIMP
nsFindComponent::Context::GetSearchString(PRUnichar * *aSearchString) {
    nsresult rv = NS_OK;
    if ( aSearchString ) {
        *aSearchString = mSearchString.ToNewUnicode();
        if ( !*aSearchString ) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::SetSearchString(const PRUnichar *aSearchString) {
    nsresult rv = NS_OK;
    mSearchString = aSearchString ? nsString( aSearchString ) : nsString();
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::GetReplaceString(PRUnichar * *aReplaceString) {
    nsresult rv = NS_OK;
    if ( aReplaceString ) {
        *aReplaceString = mReplaceString.ToNewUnicode();
        if ( !*aReplaceString ) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::SetReplaceString(const PRUnichar *aReplaceString) {
    nsresult rv = NS_OK;
    mReplaceString = aReplaceString ? nsString( aReplaceString ) : nsString();
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::GetSearchBackwards(PRBool *aBool) {
    nsresult rv = NS_OK;
    if ( aBool ) {
        *aBool = mSearchBackwards;
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::SetSearchBackwards(PRBool aBool) {
    nsresult rv = NS_OK;
    mSearchBackwards = aBool;
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::GetCaseSensitive(PRBool *aBool) {
    nsresult rv = NS_OK;
    if ( aBool ) {
        *aBool = mCaseSensitive;
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::SetCaseSensitive(PRBool aBool) {
    nsresult rv = NS_OK;
    mCaseSensitive = aBool;
    return rv;
}

NS_IMETHODIMP 
nsFindComponent::Context::GetWrapSearch(PRBool *aBool) {
    nsresult rv = NS_OK;
    if ( aBool ) {
        *aBool = mWrapSearch;
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::SetWrapSearch(PRBool aBool)
{
    nsresult rv = NS_OK;
    mWrapSearch = aBool;
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::GetTargetWindow( nsIDOMWindowInternal * *aWindow)
{
  NS_ENSURE_ARG_POINTER(aWindow);
  NS_IF_ADDREF(*aWindow = mTargetWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsFindComponent::Context::GetFindDialog( nsIDOMWindowInternal  * *aDialog)
{
  NS_ENSURE_ARG_POINTER(aDialog);
  NS_IF_ADDREF(*aDialog = mFindDialog);
  return NS_OK;
}

NS_IMETHODIMP
nsFindComponent::Context::SetFindDialog( nsIDOMWindowInternal *aDialog )
{
  mFindDialog = aDialog;
  return NS_OK;
}

NS_IMETHODIMP
nsFindComponent::Context::GetReplaceDialog( nsIDOMWindowInternal  * *aDialog)
{
  NS_ENSURE_ARG_POINTER(aDialog);
  NS_IF_ADDREF(*aDialog = mReplaceDialog);
  return NS_OK;
}

NS_IMETHODIMP
nsFindComponent::Context::SetReplaceDialog( nsIDOMWindowInternal *aDialog )
{
  mReplaceDialog = aDialog;
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif


/*

    Warning: this code is soon to become obsolete. The Find code has moved
    into mozilla/embedding/components/find, and is now shared between
    embedding apps and mozilla.
    
    This code remains because editor needs its find and replace functionality,
    for now.
    
    Simon Fraser sfraser@netscape.com

*/



// ctor
nsFindComponent::nsFindComponent()
    : mLastSearchString(),
      mLastCaseSensitive( PR_FALSE ),
      mLastSearchBackwards( PR_FALSE ),
      mLastWrapSearch( PR_FALSE )
{
    NS_INIT_REFCNT();

    // Initialize "last" stuff from prefs, if we wanted to be really clever...
}

NS_IMPL_ISUPPORTS1(nsFindComponent, nsIFindComponent)

// dtor
nsFindComponent::~nsFindComponent()
{
}

NS_IMETHODIMP
nsFindComponent::CreateContext( nsIDOMWindowInternal *aWindow, nsIEditorShell* aEditorShell,
                                nsISupports **aResult )
{

    if (!aResult)
			return NS_ERROR_NULL_POINTER;
      
    // Construct a new Context with this document.
    Context		*newContext = new Context();
   	if (!newContext)
   		return NS_ERROR_OUT_OF_MEMORY;
   
     // Do the expected AddRef on behalf of caller.
    newContext->AddRef();

    nsresult	rv = newContext->Init( aWindow,
                        aEditorShell,
                        mLastSearchString,
                        mLastReplaceString,
                        mLastCaseSensitive,
                        mLastSearchBackwards,
                        mLastWrapSearch);
 		if (NS_FAILED(rv))
 		{
 			NS_RELEASE(newContext);
 			return rv;
 		}
 
		*aResult = newContext;
    return NS_OK;
}

static nsresult
OpenDialogWithArg(nsIDOMWindowInternal *parent, nsISearchContext *arg,
                  const char *url)
{
  nsresult rv = NS_OK;

  if (parent && arg && url) {
    nsCOMPtr<nsISupportsInterfacePointer> ifptr =
      do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    ifptr->SetData(arg);
    ifptr->SetDataIID(&NS_GET_IID(nsISearchContext));

    nsCOMPtr<nsIDOMWindow> newWindow;
    rv = parent->OpenDialog(NS_ConvertASCIItoUCS2(url),
                            NS_LITERAL_STRING("_blank"),
                            NS_LITERAL_STRING("chrome,resizable=no,dependent=yes"),
                            ifptr, getter_AddRefs(newWindow));
  }

  return rv;
}

NS_IMETHODIMP
nsFindComponent::Find(nsISupports *aContext, PRBool *aDidFind)
{
    nsresult rv = NS_OK;

    // See if find dialog is already up.
    if ( aContext ) {
        nsCOMPtr<nsISearchContext> context = do_QueryInterface( aContext, &rv );
        if ( NS_SUCCEEDED( rv ) && context ) {
            nsCOMPtr<nsIDOMWindowInternal> dialog;
            rv = context->GetFindDialog( getter_AddRefs( dialog ) );
            if ( NS_SUCCEEDED( rv ) && dialog ) {
                // Just give focus back to the dialog.
                dialog->Focus();
                return NS_OK;
            }
        }
        // Test for error (GetFindDialog succeeds if there's no dialog).
        if ( NS_FAILED( rv ) ) {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Error getting find dialog, rv=0x%08X\n",
                          __FILE__, (int)__LINE__, (int)rv );
            return rv;
        }
    }

    if (aContext)
    {
        nsCOMPtr<nsISearchContext> context = do_QueryInterface( aContext, &rv );
        if (NS_FAILED(rv))
            return rv;

        // Open Find dialog and prompt for search parameters.
        char * urlStr = "chrome://global/content/finddialog.xul";

        // We need the parent's nsIDOMWindowInternal...
        // 1. Get topLevelWindow nsIWebShellContainer (chrome included).
        nsCOMPtr<nsIDOMWindowInternal> window;
        rv = context->GetTargetWindow( getter_AddRefs( window ) );
        if ( NS_SUCCEEDED( rv ) && window )
        {
        	nsCOMPtr<nsIDOMWindow> topWindow;
        	window->GetTop(getter_AddRefs(topWindow));
        	if (topWindow) {
              nsCOMPtr<nsIDOMWindowInternal> topInternal = do_QueryInterface(topWindow);
              rv = OpenDialogWithArg(topInternal, context, urlStr);
            }
        }
     } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

NS_IMETHODIMP
nsFindComponent::FindNext(nsISupports *aContext, PRBool *aDidFind)
{
  nsresult rv = NS_OK;

  if (!aContext)
    return NS_ERROR_NULL_POINTER;
			
  Context *context = (Context*)aContext;

  // If we haven't searched yet, put up dialog (via Find).
  if ( context->mSearchString.IsEmpty() ) {
    return this->Find( aContext, aDidFind );
  }

  context->DoFind(aDidFind);

  // Record this for out-of-the-blue FindNext calls.
  mLastSearchString    = context->mSearchString;
  mLastCaseSensitive   = context->mCaseSensitive;
  mLastSearchBackwards = context->mSearchBackwards;
  mLastWrapSearch      = context->mWrapSearch;

  if (!*aDidFind)
  {
    static NS_DEFINE_IID(kSoundCID,       NS_SOUND_CID);
    nsCOMPtr<nsISound> sound = do_CreateInstance(kSoundCID);
    if (sound)
      sound->Beep();    
  }

  return rv;
}

NS_IMETHODIMP
nsFindComponent::Replace( nsISupports *aContext )
{
    nsresult rv = NS_OK;

    // See if replace dialog is already up.
    if ( aContext ) {
        nsCOMPtr<nsISearchContext> context = do_QueryInterface( aContext, &rv );
        if ( NS_SUCCEEDED( rv ) && context ) {
            nsCOMPtr<nsIDOMWindowInternal> dialog;
            rv = context->GetReplaceDialog( getter_AddRefs( dialog ) );
            if ( NS_SUCCEEDED( rv ) && dialog ) {
                // Just give focus back to the dialog.
                dialog->Focus();
                return NS_OK;
            }
        }
        // Test for error (GetReplaceDialog succeeds if there's no dialog).
        if ( NS_FAILED( rv ) ) {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Error getting replace dialog, rv=0x%08X\n",
                          __FILE__, (int)__LINE__, (int)rv );
            return rv;
        }
    }

    if (aContext)
    {
        nsCOMPtr<nsISearchContext> context = do_QueryInterface( aContext, &rv );
        if (NS_FAILED(rv))
            return rv;

        // Open Replace dialog and prompt for search parameters.
        char * urlStr = "chrome://global/content/replacedialog.xul";

        // We need the parent's nsIDOMWindowInternal...
        // 1. Get topLevelWindow nsIWebShellContainer (chrome included).
        nsCOMPtr<nsIDOMWindowInternal> window;
        rv = context->GetTargetWindow( getter_AddRefs( window ) );
        if ( NS_SUCCEEDED( rv ) && window )
        {
        	nsCOMPtr<nsIDOMWindow> topWindow;
        	window->GetTop(getter_AddRefs(topWindow));
        	if (topWindow) {
              nsCOMPtr<nsIDOMWindowInternal> topInternal = do_QueryInterface(topWindow);
              rv = OpenDialogWithArg(topInternal, context, urlStr);
            }
        }
     } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

NS_IMETHODIMP
nsFindComponent::ReplaceNext( nsISupports *aContext, PRBool aAllOccurrences, PRBool *aDidFind )
{
  if (!aContext)
    return NS_ERROR_NULL_POINTER;

  Context *context = (Context*)aContext;

  if (!context)
    return NS_ERROR_FAILURE;

  nsresult rv = context->DoReplace(aAllOccurrences, aDidFind);

  // Record this for out-of-the-blue FindNext calls.
  mLastSearchString    = context->mSearchString;
  mLastCaseSensitive   = context->mCaseSensitive;
  mLastSearchBackwards = context->mSearchBackwards;
  mLastWrapSearch      = context->mWrapSearch;

  return rv;
}


NS_IMETHODIMP
nsFindComponent::ResetContext( nsISupports *aContext,
                               nsIDOMWindowInternal *aNewWindow,
                               nsIEditorShell* aEditorShell )
{
  NS_ENSURE_ARG(aContext);
  NS_ENSURE_ARG(aNewWindow);
	
  // Pass on the new document to the context.
  Context *context = (Context*)aContext;
  context->Reset(aNewWindow);

  return NS_OK;
}

