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

#include "WebShellContainer.h"


static NS_DEFINE_IID(kIWebShellContainerIID, NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);
/* static NS_DEFINE_IID(kINetSupportIID, NS_INETSUPPORT_IID); */


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
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtrResult = NULL;
/*
  if (aIID.Equals(kIBrowserWindowIID)) {
    *aInstancePtrResult = (void*) ((nsIBrowserWindow*)this);
    AddRef();
    return NS_OK;
  } */
  if (aIID.Equals(kIStreamObserverIID)) {
    *aInstancePtrResult = (void*) ((nsIStreamObserver*)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kIWebShellContainerIID)) {
    *aInstancePtrResult = (void*) ((nsIWebShellContainer*)this);
    AddRef();
    return NS_OK;
  }
/*  if (aIID.Equals(kINetSupportIID)) {
    *aInstancePtrResult = (void*) ((nsINetSupport*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)((nsIBrowserWindow*)this));
    AddRef();
    return NS_OK;
  }
  */
  return NS_NOINTERFACE;
}


///////////////////////////////////////////////////////////////////////////////
// nsIWebShellContainer implementation


NS_IMETHODIMP
CWebShellContainer::WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason)
{
	ATLTRACE(_T("CWebShellContainer::WillLoadURL()\n"));
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
	ATLTRACE(_T("CWebShellContainer::BeginLoadURL()\n"));

	//Fire a BeforeNavigate event
	USES_CONVERSION;
	OLECHAR *pszURL = W2OLE((WCHAR *)aURL);
	BSTR bstrURL = SysAllocString(pszURL);
	BSTR bstrTargetFrameName = NULL;
	BSTR bstrHeaders = NULL;
	VARIANT *pvPostData = NULL;
	VARIANT_BOOL bCancel = VARIANT_FALSE;
	long lFlags = 0;

	m_pEvents1->Fire_BeforeNavigate(bstrURL, lFlags, bstrTargetFrameName, pvPostData, bstrHeaders, &bCancel);

	//Fire a BeforeNavigate2 event
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


NS_IMETHODIMP
CWebShellContainer::ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax)
{
	ATLTRACE(_T("CWebShellContainer::ProgressLoadURL()\n"));
	m_pEvents1->Fire_ProgressChange(aProgress, aProgressMax);
	m_pEvents2->Fire_ProgressChange(aProgress, aProgressMax);
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aStatus)
{
	ATLTRACE(_T("CWebShellContainer::EndLoadURL()\n"));

	//Fire a NavigateComplete event
	USES_CONVERSION;
	OLECHAR *pszURL = W2OLE((WCHAR *) aURL);
	BSTR bstrURL = SysAllocString(pszURL);
	m_pEvents1->Fire_NavigateComplete(bstrURL);

	//Fire a NavigateComplete2 event
	CComVariant vURL(bstrURL);
	m_pEvents2->Fire_NavigateComplete2(m_pOwner, &vURL);

	m_pOwner->m_bBusy = FALSE;
	SysFreeString(bstrURL);

	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::NewWebShell(PRUint32 aChromeMask,
                                PRBool aVisible,
                                nsIWebShell *&aNewWebShell)
{
	ATLTRACE(_T("CWebShellContainer::NewWebShell()\n"));
	nsresult rv = NS_ERROR_OUT_OF_MEMORY;
	return rv;
}


NS_IMETHODIMP
CWebShellContainer::FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult)
{
	ATLTRACE(_T("CWebShellContainer::FindWebShellWithName()\n"));
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CWebShellContainer::FocusAvailable(nsIWebShell* aFocusedWebShell)
{
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver implementation


NS_IMETHODIMP
CWebShellContainer::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
	ATLTRACE(_T("CWebShellContainer::OnStartBinding()\n"));
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax)
{
	ATLTRACE(_T("CWebShellContainer::OnProgress()\n"));
	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::OnStatus(nsIURL* aURL, const nsString &aMsg)
{
	ATLTRACE(_T("CWebShellContainer::OnStatus()\n"));

	USES_CONVERSION;
	BSTR bstrText = SysAllocString(W2OLE((PRUnichar *) aMsg));
	m_pEvents1->Fire_StatusTextChange(bstrText);
	m_pEvents2->Fire_StatusTextChange(bstrText);
	SysFreeString(bstrText);

	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::OnStopBinding(nsIURL* aURL, PRInt32 aStatus, const nsString &aMsg)
{
	ATLTRACE(_T("CWebShellContainer::OnStopBinding()\n"));
	return NS_OK;
}
