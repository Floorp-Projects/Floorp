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

#include "stdafx.h"

#include <limits.h>

#include "WebShellContainer.h"

CWebShellContainer::CWebShellContainer(CMozillaBrowser *pOwner)
{
	NS_INIT_REFCNT();
	m_pOwner = pOwner;
	m_pEvents1 = m_pOwner;
	m_pEvents2 = m_pOwner;
}


CWebShellContainer::~CWebShellContainer()
{
}


///////////////////////////////////////////////////////////////////////////////
// nsISupports implementation


NS_IMPL_ADDREF(CWebShellContainer)
NS_IMPL_RELEASE(CWebShellContainer)


nsresult CWebShellContainer::QueryInterface(const nsIID& aIID, void** aInstancePtrResult)
{
	NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
	if (nsnull == aInstancePtrResult)
	{
		return NS_ERROR_NULL_POINTER;
	}

	*aInstancePtrResult = NULL;

	if (aIID.Equals(kIStreamObserverIID))
	{
		*aInstancePtrResult = (void*) ((nsIStreamObserver*)this);
		AddRef();
		return NS_OK;
	}

	if (aIID.Equals(kIDocumentLoaderObserverIID))
	{
		*aInstancePtrResult = (void*) ((nsIDocumentLoaderObserver*)this);
		AddRef();
		return NS_OK;
	}

	if (aIID.Equals(kIWebShellContainerIID))
	{
		*aInstancePtrResult = (void*) ((nsIWebShellContainer*)this);
		AddRef();
		return NS_OK;
	}

	if (aIID.Equals(kISupportsIID))
	{
		*aInstancePtrResult = (void*) ((nsIStreamObserver*)this);
		AddRef();
		return NS_OK;
	}

	return NS_NOINTERFACE;
}


///////////////////////////////////////////////////////////////////////////////
// nsIWebShellContainer implementation


NS_IMETHODIMP
CWebShellContainer::WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::WillLoadURL(..., \"%s\", %d)\n"), W2T(aURL), (int) aReason);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::BeginLoadURL(..., \"%s\")\n"), W2T(aURL));

	// Fire a DownloadBegin
//	m_pEvents1->Fire_DownloadBegin();
	//m_pEvents2->Fire_DownloadBegin();

	// Fire a BeforeNavigate event
	OLECHAR *pszURL = W2OLE((WCHAR *)aURL);
	BSTR bstrURL = SysAllocString(pszURL);
	BSTR bstrTargetFrameName = NULL;
	BSTR bstrHeaders = NULL;
	VARIANT *pvPostData = NULL;
	VARIANT_BOOL bCancel = VARIANT_FALSE;
	long lFlags = 0;

	m_pEvents1->Fire_BeforeNavigate(bstrURL, lFlags, bstrTargetFrameName, pvPostData, bstrHeaders, &bCancel);

	// Fire a BeforeNavigate2 event
	CComVariant vURL(bstrURL);
	CComVariant vFlags(lFlags);
	CComVariant vTargetFrameName(bstrTargetFrameName);
	CComVariant vHeaders(bstrHeaders);

	m_pEvents2->Fire_BeforeNavigate2(m_pOwner, &vURL, &vFlags, &vTargetFrameName, pvPostData, &vHeaders, &bCancel);

	SysFreeString(bstrURL);
	SysFreeString(bstrTargetFrameName);
	SysFreeString(bstrHeaders);

	if (bCancel == VARIANT_TRUE)
	{
		if (aShell)
		{
			aShell->Stop();
		}
	}
	else
	{
		m_pOwner->m_bBusy = TRUE;
	}
	return NS_OK;
}


#define FPKLUDGE
#ifdef FPKLUDGE
#include <float.h>
#endif

NS_IMETHODIMP
CWebShellContainer::ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::ProgressLoadURL(..., \"%s\", %d, %d)\n"), W2T(aURL), (int) aProgress, (int) aProgressMax);
	
	long nProgress = aProgress;
	long nProgressMax = aProgressMax;

	if (nProgress == 0)
	{
	}
	if (nProgressMax == 0)
	{
		nProgressMax = LONG_MAX;
	}
	if (nProgress > nProgressMax)
	{
		nProgress = nProgressMax; // Progress complete
	}

#ifdef FPKLUDGE
	_fpreset();
#endif

	m_pEvents1->Fire_ProgressChange(nProgress, nProgressMax);
	m_pEvents2->Fire_ProgressChange(nProgress, nProgressMax);

	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aStatus)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::EndLoadURL(..., \"%s\", %d)\n"), W2T(aURL), (int) aStatus);

	// Fire a NavigateComplete event
	OLECHAR *pszURL = W2OLE((WCHAR *) aURL);
	BSTR bstrURL = SysAllocString(pszURL);
	m_pEvents1->Fire_NavigateComplete(bstrURL);

	// Fire a NavigateComplete2 event
	CComVariant vURL(bstrURL);
	m_pEvents2->Fire_NavigateComplete2(m_pOwner, &vURL);

	// Fire the new NavigateForward state
	VARIANT_BOOL bEnableForward = VARIANT_FALSE;
	if (m_pOwner->m_pIWebShell->CanForward() == NS_OK)
	{
		bEnableForward = VARIANT_TRUE;
	}
	m_pEvents2->Fire_CommandStateChange(CSC_NAVIGATEFORWARD, bEnableForward);

	// Fire the new NavigateBack state
	VARIANT_BOOL bEnableBack = VARIANT_FALSE;
	if (m_pOwner->m_pIWebShell->CanBack() == NS_OK)
	{
		bEnableBack = VARIANT_TRUE;
	}
	m_pEvents2->Fire_CommandStateChange(CSC_NAVIGATEBACK, bEnableBack);

	m_pOwner->m_bBusy = FALSE;
	SysFreeString(bstrURL);

	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::NewWebShell(PRUint32 aChromeMask, PRBool aVisible, nsIWebShell *&aNewWebShell)
{
	NG_TRACE(_T("CWebShellContainer::NewWebShell()\n"));
	nsresult rv = NS_ERROR_OUT_OF_MEMORY;
	return rv;
}


NS_IMETHODIMP
CWebShellContainer::FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::FindWebShellWithName(\"%s\", ...)\n"), W2T(aName));

	nsresult rv = NS_OK; 

	// Zero result (in case we fail). 
	aResult = nsnull;  

	if (m_pOwner->m_pIWebShell != NULL)
	{ 
		rv = m_pOwner->m_pIWebShell->FindChildWithName(aName, aResult); 
	} 

	return rv; 
}


NS_IMETHODIMP
CWebShellContainer::FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken)
{
	NG_TRACE(_T("CWebShellContainer::FocusAvailable()\n"));
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::CanCreateNewWebShell(PRBool& aResult)
{		
	aResult = PR_FALSE;
	nsresult rv = NS_OK;
	return rv;
}

NS_IMETHODIMP
CWebShellContainer::SetNewWebShellInfo(const nsString& aName, const nsString& anURL, 
							nsIWebShell* aOpenerShell, PRUint32 aChromeMask,
							nsIWebShell** aNewShell, nsIWebShell** anInnerShell)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}


NS_IMETHODIMP
CWebShellContainer::ChildShellAdded(nsIWebShell** aChildShell, nsIContent* frameNode)
{
	nsresult rv = NS_OK;
	return rv;
}


///////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver implementation


NS_IMETHODIMP
CWebShellContainer::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::OnStartBinding(..., \"%s\")\n"), A2CT(aContentType));
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::OnProgress(..., \"%d\", \"%d\")\n"), (int) aProgress, (int) aProgressMax);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::OnStatus(..., \"%s\")\n"), W2T((PRUnichar *) aMsg));

	BSTR bstrText = SysAllocString(W2OLE((PRUnichar *) aMsg));
	m_pEvents1->Fire_StatusTextChange(bstrText);
	m_pEvents2->Fire_StatusTextChange(bstrText);
	SysFreeString(bstrText);

	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
	USES_CONVERSION;
	NG_TRACE(_T("CWebShellContainer::OnStopBinding(..., %d, \"%s\")\n"), (int) aStatus, W2T((PRUnichar *) aMsg));

	// Fire a DownloadComplete event
	m_pEvents1->Fire_DownloadComplete();
	m_pEvents2->Fire_DownloadComplete();

	return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// nsIDocumentLoaderObserver implementation 


NS_IMETHODIMP 
CWebShellContainer::OnStartDocumentLoad(nsIURL* aURL, const char* aCommand) 
{ 
	return NS_OK; 
} 

// we need this to fire the document complete 
NS_IMETHODIMP 
CWebShellContainer::OnEndDocumentLoad(nsIURL *aUrl, PRInt32 aStatus) 
{
	PRUnichar* wString = nsnull;    

	aUrl->ToString(&wString);
	if (wString == NULL)
	{
		return NS_ERROR_NULL_POINTER;
	}

	USES_CONVERSION; 
	delete [] wString; // clean up. 

	BSTR bstrURL = SysAllocString(W2OLE((WCHAR *) wString)); 
		
	// Fire a DocumentComplete event
	CComVariant vURL(bstrURL);
	m_pEvents2->Fire_DocumentComplete(m_pOwner, &vURL);
	SysFreeString(bstrURL);

	return NS_OK; 
} 

// we don't care about these. 
NS_IMETHODIMP 
CWebShellContainer::OnStartURLLoad(nsIURL* aURL, const char* aContentType, nsIContentViewer* aViewer) 
{ 
	return NS_OK; 
} 


NS_IMETHODIMP 
CWebShellContainer::OnProgressURLLoad(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax) 
{ 
	return NS_OK; 
} 


NS_IMETHODIMP 
CWebShellContainer::OnStatusURLLoad(nsIURL* aURL, nsString& aMsg) 
{ 
	return NS_OK; 
} 


NS_IMETHODIMP 
CWebShellContainer::OnEndURLLoad(nsIURL* aURL, PRInt32 aStatus) 
{ 
	return NS_OK; 
} 


NS_IMETHODIMP 
CWebShellContainer::HandleUnknownContentType( nsIURL *aURL, const char *aContentType, const char *aCommand ) 
{ 
	return NS_OK; 
} 