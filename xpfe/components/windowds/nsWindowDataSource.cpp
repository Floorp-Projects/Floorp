/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 200
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsWindowDataSource.h"
#include "nsIXULWindow.h"
#include "rdf.h"
#include "nsIRDFContainerUtils.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"

#include "nsIWindowMediator.h"

PRUint32 nsWindowDataSource::windowCount = 0;

nsIRDFResource* nsWindowDataSource::kNC_Name = nsnull;
nsIRDFResource* nsWindowDataSource::kNC_WindowRoot = nsnull;
nsIRDFResource* nsWindowDataSource::kNC_KeyIndex = nsnull;

nsIRDFService*  nsWindowDataSource::gRDFService = nsnull;
nsIRDFContainer* nsWindowDataSource::mContainer = nsnull;
nsIRDFDataSource* nsWindowDataSource::mInner = nsnull;

PRUint32 nsWindowDataSource::gRefCnt = 0;

static const char kURINC_WindowRoot[] = "NC:WindowMediatorRoot";

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, KeyIndex);

nsresult
nsWindowDataSource::Init()
{
    nsresult rv;

    rv = CallGetService("@mozilla.org/rdf/rdf-service;1", &gRDFService);
    if (NS_FAILED(rv)) return rv;

    gRDFService->GetResource(kURINC_WindowRoot, &kNC_WindowRoot);
    gRDFService->GetResource(kURINC_Name,       &kNC_Name);
    gRDFService->GetResource(kURINC_KeyIndex,   &kNC_KeyIndex);

    rv = CallCreateInstance("@mozilla.org/rdf/datasource;1?name=in-memory-datasource", &mInner);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFContainerUtils> rdfc =
        do_GetService("@mozilla.org/rdf/container-utils;1", &rv);
    if (NS_FAILED(rv)) return rv;

    rv = rdfc->MakeSeq(this, kNC_WindowRoot, &mContainer);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIWindowMediator> windowMediator =
        do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = windowMediator->AddListener(this);
    if (NS_FAILED(rv)) return rv;
        
    return NS_OK;
}

nsWindowDataSource::~nsWindowDataSource()
{
    if (--gRefCnt == 0) {
        NS_IF_RELEASE(kNC_Name);
        NS_IF_RELEASE(kNC_KeyIndex);
        NS_IF_RELEASE(kNC_WindowRoot);
        NS_IF_RELEASE(gRDFService);
        NS_IF_RELEASE(mContainer);
        NS_IF_RELEASE(mInner);
    }
}

#if 0
NS_IMETHODIMP_(nsrefcnt)
nsWindowMediator::Release()
{
	// We need a special implementation of Release() due to having
	// two circular references:  mInner and mContainer

	NS_PRECONDITION(PRInt32(mRefCnt) > 0, "duplicate release");
	--mRefCnt;
	NS_LOG_RELEASE(this, mRefCnt, "nsWindowMediator");

	if (mInner && mRefCnt == 2)
	{
		NS_IF_RELEASE(mContainer);
		mContainer = nsnull;

		nsIRDFDataSource* tmp = mInner;
		mInner = nsnull;
		NS_IF_RELEASE(tmp);
		return(0);
	}
	else if (mRefCnt == 0)
	{
		mRefCnt = 1;
		delete this;
		return(0);
	}
	return(mRefCnt);
}

#endif


NS_IMPL_ISUPPORTS3(nsWindowDataSource,
                   nsIWindowMediatorListener,
                   nsIWindowDataSource,
                   nsIRDFDataSource)

// nsIWindowMediatorListener implementation
// handle notifications from the window mediator and reflect them into
// RDF

/* void onWindowTitleChange (in nsIXULWindow window, in wstring newTitle); */
NS_IMETHODIMP
nsWindowDataSource::OnWindowTitleChange(nsIXULWindow *window,
                                        const PRUnichar *newTitle)
{
    nsresult rv;
    
    nsISupportsKey key(window);

    nsCOMPtr<nsISupports> sup =
        dont_AddRef(mWindowResources.Get(&key));

    NS_ENSURE_TRUE(sup, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIRDFResource> windowResource =
        do_QueryInterface(sup);

    nsCOMPtr<nsIRDFLiteral> newTitleLiteral;
    rv = gRDFService->GetLiteral(newTitle, getter_AddRefs(newTitleLiteral));
    NS_ENSURE_SUCCESS(rv, rv);

    // get the old title
    nsCOMPtr<nsIRDFNode> oldTitleNode;
    rv = GetTarget(windowResource, kNC_Name, PR_TRUE,
                   getter_AddRefs(oldTitleNode));
    
    // assert the change
    if (NS_SUCCEEDED(rv) && oldTitleNode)
        // has an existing window title, update it
        rv = Change(windowResource, kNC_Name, oldTitleNode, newTitleLiteral);
    else
        // removed from the tasklist
        rv = Assert(windowResource, kNC_Name, newTitleLiteral, PR_TRUE);

    if (rv != NS_RDF_ASSERTION_ACCEPTED)
    {
      NS_ERROR("unable to set window name");
    }
    
    return NS_OK;
}

/* void onOpenWindow (in nsIXULWindow window); */
NS_IMETHODIMP
nsWindowDataSource::OnOpenWindow(nsIXULWindow *window)
{
    nsCAutoString windowId(NS_LITERAL_CSTRING("window-"));
    windowId.AppendInt(windowCount++, 10);

    nsCOMPtr<nsIRDFResource> windowResource;
    gRDFService->GetResource(windowId.get(), getter_AddRefs(windowResource));

    nsISupportsKey key(window);
    mWindowResources.Put(&key, windowResource);

    // assert the new window
    mContainer->AppendElement(windowResource);

    return NS_OK;
}

/* void onCloseWindow (in nsIXULWindow window); */
NS_IMETHODIMP
nsWindowDataSource::OnCloseWindow(nsIXULWindow *window)
{
    nsISupportsKey key(window);
    nsCOMPtr<nsIRDFResource> resource;

    nsresult rv;

    if (!mWindowResources.Remove(&key, getter_AddRefs(resource)))
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIRDFNode> oldKeyNode;
    nsCOMPtr<nsIRDFInt> oldKeyInt;
    
    // get the old keyIndex, if any
    rv = GetTarget(resource, kNC_KeyIndex, PR_TRUE,
                   getter_AddRefs(oldKeyNode));
    if (NS_SUCCEEDED(rv) && (rv != NS_RDF_NO_VALUE))
        oldKeyInt = do_QueryInterface(oldKeyNode);

    
    // update RDF and keyindex - from this point forward we'll ignore
    // errors, because they just indicate some kind of RDF inconsistency
    PRInt32 winIndex = -1;
    rv = mContainer->IndexOf(resource, &winIndex);
        
    if (NS_FAILED(rv))
        return NS_OK;
            
    // unassert the old window, ignore any error
    mContainer->RemoveElement(resource, PR_TRUE);
    
    nsCOMPtr<nsISimpleEnumerator> children;
    rv = mContainer->GetElements(getter_AddRefs(children));
    if (NS_FAILED(rv))
        return NS_OK;

    PRBool more = PR_FALSE;

    while (NS_SUCCEEDED(rv = children->HasMoreElements(&more)) && more) {
        nsCOMPtr<nsISupports> sup;
        rv = children->GetNext(getter_AddRefs(sup));
        if (NS_FAILED(rv))
            break;

        nsCOMPtr<nsIRDFResource> windowResource = do_QueryInterface(sup, &rv);
        if (NS_FAILED(rv))
            continue;

        PRInt32 currentIndex = -1;
        mContainer->IndexOf(windowResource, &currentIndex);

        // can skip updating windows with lower indexes
        // than the window that was removed
        if (currentIndex < winIndex)
            continue;

        nsCOMPtr<nsIRDFNode> newKeyNode;
        nsCOMPtr<nsIRDFInt> newKeyInt;

        rv = GetTarget(windowResource, kNC_KeyIndex, PR_TRUE,
                       getter_AddRefs(newKeyNode));
        if (NS_SUCCEEDED(rv) && (rv != NS_RDF_NO_VALUE))
            newKeyInt = do_QueryInterface(newKeyNode);

        // changing from one key index to another
        if (oldKeyInt && newKeyInt)
            Change(windowResource, kNC_KeyIndex, oldKeyInt, newKeyInt);
        // creating a new keyindex - probably window going
        // from (none) to "9"
        else if (newKeyInt)
            Assert(windowResource, kNC_KeyIndex, newKeyInt, PR_TRUE);
        
        // somehow inserting a window above this one,
        // "9" to (none)
        else if (oldKeyInt)
            Unassert(windowResource, kNC_KeyIndex, oldKeyInt);
        
    }
    return NS_OK;
}

// nsIWindowDataSource implementation

NS_IMETHODIMP
nsWindowDataSource::GetWindowForResource(const char *aResourceString,
                                         nsIDOMWindowInternal** aResult)
{
    nsCOMPtr<nsIRDFResource> windowResource;
    gRDFService->GetResource(aResourceString, getter_AddRefs(windowResource));

    // now reverse-lookup in the hashtable

    return NS_OK;
}


// nsIRDFDataSource implementation
// mostly, we just forward to mInner, except:
// GetURI() - need to return "rdf:window-mediator"
// GetTarget() - need to handle kNC_KeyIndex


/* readonly attribute string URI; */
NS_IMETHODIMP nsWindowDataSource::GetURI(char * *aURI)
{
    NS_ENSURE_ARG_POINTER(aURI);
    
    *aURI = ToNewCString(NS_LITERAL_CSTRING("rdf:window-mediator"));

    if (!*aURI)
        return NS_ERROR_OUT_OF_MEMORY;
    
    return NS_OK;
}

/* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP nsWindowDataSource::GetTarget(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsIRDFNode **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    // special case kNC_KeyIndex before we forward to mInner
    if (aProperty == kNC_KeyIndex) {

        PRInt32 theIndex = 0;
        nsresult rv = mContainer->IndexOf(aSource, &theIndex);
        if (NS_FAILED(rv)) return rv;

        // only allow the range of 1 to 9 for single key access
        if (theIndex < 1 || theIndex > 9) return(NS_RDF_NO_VALUE);

        nsCOMPtr<nsIRDFInt> indexInt;
        rv = gRDFService->GetIntLiteral(theIndex, getter_AddRefs(indexInt));
        if (NS_FAILED(rv)) return(rv);
        if (!indexInt) return(NS_ERROR_FAILURE);
        
        return CallQueryInterface(indexInt, _retval);
    }

    
    return mInner->GetTarget(aSource, aProperty, aTruthValue, _retval);
}

/* nsIRDFResource GetSource (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP nsWindowDataSource::GetSource(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsIRDFResource **_retval)
{
    return mInner->GetSource(aProperty, aTarget, aTruthValue, _retval);
}

/* nsISimpleEnumerator GetSources (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP nsWindowDataSource::GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
    return mInner->GetSources(aProperty, aTarget, aTruthValue, _retval);
}

/* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP nsWindowDataSource::GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
    return mInner->GetTargets(aSource, aProperty, aTruthValue, _retval);
}

/* void Assert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP nsWindowDataSource::Assert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue)
{
    return mInner->Assert(aSource, aProperty, aTarget, aTruthValue);
}

/* void Unassert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget); */
NS_IMETHODIMP nsWindowDataSource::Unassert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
    return mInner->Unassert(aSource, aProperty, aTarget);
}

/* void Change (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aOldTarget, in nsIRDFNode aNewTarget); */
NS_IMETHODIMP nsWindowDataSource::Change(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aOldTarget, nsIRDFNode *aNewTarget)
{
    return mInner->Change(aSource, aProperty, aOldTarget, aNewTarget);
}

/* void Move (in nsIRDFResource aOldSource, in nsIRDFResource aNewSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget); */
NS_IMETHODIMP nsWindowDataSource::Move(nsIRDFResource *aOldSource, nsIRDFResource *aNewSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
    return mInner->Move(aOldSource, aNewSource, aProperty, aTarget);
}

/* boolean HasAssertion (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP nsWindowDataSource::HasAssertion(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, PRBool *_retval)
{
    return mInner->HasAssertion(aSource, aProperty, aTarget, aTruthValue, _retval);
}

/* void AddObserver (in nsIRDFObserver aObserver); */
NS_IMETHODIMP nsWindowDataSource::AddObserver(nsIRDFObserver *aObserver)
{
    return mInner->AddObserver(aObserver);
}

/* void RemoveObserver (in nsIRDFObserver aObserver); */
NS_IMETHODIMP nsWindowDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{
    return mInner->RemoveObserver(aObserver);
}

/* nsISimpleEnumerator ArcLabelsIn (in nsIRDFNode aNode); */
NS_IMETHODIMP nsWindowDataSource::ArcLabelsIn(nsIRDFNode *aNode, nsISimpleEnumerator **_retval)
{
    return mInner->ArcLabelsIn(aNode, _retval);
}

/* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
NS_IMETHODIMP nsWindowDataSource::ArcLabelsOut(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
    return mInner->ArcLabelsOut(aSource, _retval);
}

/* nsISimpleEnumerator GetAllResources (); */
NS_IMETHODIMP nsWindowDataSource::GetAllResources(nsISimpleEnumerator **_retval)
{
    return mInner->GetAllResources(_retval);
}

/* nsIEnumerator GetAllCommands (in nsIRDFResource aSource); */
NS_IMETHODIMP nsWindowDataSource::GetAllCommands(nsIRDFResource *aSource, nsIEnumerator **_retval)
{
    return mInner->GetAllCommands(aSource, _retval);
}

/* boolean IsCommandEnabled (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
NS_IMETHODIMP nsWindowDataSource::IsCommandEnabled(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments, PRBool *_retval)
{
    return mInner->IsCommandEnabled(aSources, aCommand, aArguments, _retval);
}

/* void DoCommand (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
NS_IMETHODIMP nsWindowDataSource::DoCommand(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments)
{
    return mInner->DoCommand(aSources, aCommand, aArguments);
}

/* nsISimpleEnumerator GetAllCmds (in nsIRDFResource aSource); */
NS_IMETHODIMP nsWindowDataSource::GetAllCmds(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
    return mInner->GetAllCmds(aSource, _retval);
}

/* boolean hasArcIn (in nsIRDFNode aNode, in nsIRDFResource aArc); */
NS_IMETHODIMP nsWindowDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *_retval)
{
    return mInner->HasArcIn(aNode, aArc, _retval);
}

/* boolean hasArcOut (in nsIRDFResource aSource, in nsIRDFResource aArc); */
NS_IMETHODIMP nsWindowDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *_retval)
{
    return mInner->HasArcOut(aSource, aArc, _retval);
}

