
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
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

#include "nsCOMPtr.h"
#include "nsRDFCore.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsAppCores.h"
#include "nsAppCoresCIDs.h"
#include "nsAppCoresManager.h"

#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"

#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "rdf.h"
#include "nsIXULSortService.h"
#include "nsIBookmarkDataSource.h"
#include "nsIRDFService.h"


// Globals
static NS_DEFINE_IID(kISupportsIID,              NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRDFCoreIID,               NS_IDOMRDFCORE_IID);

static NS_DEFINE_IID(kIDOMDocumentIID,           nsIDOMDocument::GetIID());
static NS_DEFINE_IID(kIDocumentIID,              nsIDocument::GetIID());

static NS_DEFINE_CID(kRDFCoreCID,                NS_RDFCORE_CID);
static NS_DEFINE_CID(kBrowserWindowCID,          NS_BROWSER_WINDOW_CID);

static NS_DEFINE_CID(kXULSortServiceCID,         NS_XULSORTSERVICE_CID);
static NS_DEFINE_IID(kIXULSortServiceIID,        NS_IXULSORTSERVICE_IID);

static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);

/////////////////////////////////////////////////////////////////////////
// nsRDFCore
/////////////////////////////////////////////////////////////////////////

nsRDFCore::nsRDFCore()
{
  mScriptObject   = nsnull;
  mScriptContext  = nsnull;
/*
  mWindow         = nsnull;
*/
  IncInstanceCount();
  NS_INIT_REFCNT();
}

nsRDFCore::~nsRDFCore()
{
  NS_IF_RELEASE(mScriptContext);
/*
  NS_IF_RELEASE(mWindow);
*/
  DecInstanceCount();
}


NS_IMPL_ADDREF(nsRDFCore)
NS_IMPL_RELEASE(nsRDFCore)


NS_IMETHODIMP 
nsRDFCore::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kIRDFCoreIID) ) {
    *aInstancePtr = (void*) ((nsIDOMRDFCore*)this);
    AddRef();
    return NS_OK;
  }

  return nsBaseAppCore::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP 
nsRDFCore::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) 
  {
      res = NS_NewScriptRDFCore(aContext, 
                                (nsISupports *)(nsIDOMRDFCore*)this, 
                                nsnull, 
                                &mScriptObject);
  }

  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP    
nsRDFCore::Init(const nsString& aId)
{
   
  nsBaseAppCore::Init(aId);
  return NS_OK;
}

NS_IMETHODIMP    
nsRDFCore::DoSort(nsIDOMNode* node, const nsString& sortResource,
                  const nsString &sortDirection)
{
/*
  if (nsnull == mScriptContext) {
    return NS_ERROR_FAILURE;
  }
*/

        printf("----------------------------\n");
        printf("-- Sort \n");
        printf("-- Column: %s  \n", sortResource.ToNewCString());
        printf("-- Direction: %s  \n", sortDirection.ToNewCString());
        printf("----------------------------\n");

	nsIXULSortService		*XULSortService = nsnull;

	nsresult rv = nsServiceManager::GetService(kXULSortServiceCID,
		kIXULSortServiceIID, (nsISupports**) &XULSortService);
	if (NS_SUCCEEDED(rv))
	{
		if (nsnull != XULSortService)
		{
			XULSortService->DoSort(node, sortResource, sortDirection);
			nsServiceManager::ReleaseService(kXULSortServiceCID, XULSortService);
		}
	}

/*
  if (nsnull != mScriptContext) {
    const char* url = "";
    PRBool isUndefined = PR_FALSE;
    nsString rVal;
    mScriptContext->EvaluateString(mScript, url, 0, rVal, &isUndefined);
  }
*/
        return(rv);
}


NS_IMETHODIMP
nsRDFCore::AddBookmark(const nsString& aUrl, const nsString& aOptionalTitle)
{
#ifdef	DEBUG
        printf("----------------------------\n");
        printf("-- Add Bookmark \n");
        char *str1 = aUrl.ToNewCString();
        if (str1)
        {
		printf("-- URL: %s  \n", str1);
		delete [] str1;
	}
	char *str2 = aOptionalTitle.ToNewCString();
	if (str2)
	{
		printf("-- Title (opt): %s  \n", str2);
		delete [] str2;
	}
        printf("----------------------------\n");
#endif

        nsIRDFService* rdf;
	nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                                   nsIRDFService::GetIID(),
                                                   (nsISupports**) &rdf);
	if (NS_SUCCEEDED(rv))
	{
                nsCOMPtr<nsIRDFDataSource> ds;
                rv = rdf->GetDataSource("rdf:bookmarks", getter_AddRefs(ds));

                nsCOMPtr<nsIRDFBookmarkDataSource> RDFBookmarkDataSource
                    = do_QueryInterface(ds);

		if (RDFBookmarkDataSource)
		{
			char *url = aUrl.ToNewCString();
			char *optionalTitle = aOptionalTitle.ToNewCString();
			rv = RDFBookmarkDataSource->AddBookmark(url, optionalTitle);
			if (url)		delete []url;
			if (optionalTitle)	delete []optionalTitle;
		}
	}

        nsServiceManager::ReleaseService(kRDFServiceCID, rdf);

        return(rv);
}



NS_IMETHODIMP
nsRDFCore::FindBookmarkShortcut(const nsString& aUserInput, nsString & shortcutURL)
{
#ifdef	DEBUG
        printf("----------------------------\n");
        printf("-- Find Bookmark Shortcut\n");
        char *str1 = aUserInput.ToNewCString();
        if (str1)
        {
		printf("-- user input: %s  \n", str1);
		delete [] str1;
	}
        printf("----------------------------\n");
#endif

        nsIRDFService* rdf;
	nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                                   nsIRDFService::GetIID(),
                                                   (nsISupports**) &rdf);

	if (NS_SUCCEEDED(rv))
	{
                nsCOMPtr<nsIRDFDataSource> ds;
                rv = rdf->GetDataSource("rdf:bookmarks", getter_AddRefs(ds));

                nsCOMPtr<nsIRDFBookmarkDataSource> RDFBookmarkDataSource
                    = do_QueryInterface(ds);

		if (RDFBookmarkDataSource)
		{
			char *userInput = aUserInput.ToNewCString();
			char *cShortcutURL = nsnull;
			if (NS_SUCCEEDED(rv = RDFBookmarkDataSource->FindBookmarkShortcut(userInput,
					&cShortcutURL)))
			{
				shortcutURL = cShortcutURL;
			}
			if (userInput)		delete []userInput;
		}
	}

        nsServiceManager::ReleaseService(kRDFServiceCID, rdf);

	if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE))
	{
		shortcutURL = "";
		rv = NS_OK;
	}
        return(rv);
}

