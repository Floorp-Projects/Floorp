#include "stdafx.h"

#include "WebShellContainer.h"


static NS_DEFINE_IID(kIWebShellContainerIID, NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);
/* static NS_DEFINE_IID(kINetSupportIID, NS_INETSUPPORT_IID); */


CWebShellContainer::CWebShellContainer(CMozillaBrowser *pOwner)
{
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

	USES_CONVERSION;
	OLECHAR *pszURL = W2OLE((WCHAR *)aURL);
	BSTR bstrURL = SysAllocString(pszURL);
	BSTR bstrTargetFrameName = NULL;
	BSTR bstrHeaders = NULL;
	VARIANT *pvPostData = NULL;
	VARIANT_BOOL bCancel = VARIANT_FALSE;
	long lFlags = 0;

	m_pEvents1->Fire_BeforeNavigate(bstrURL, lFlags, bstrTargetFrameName, pvPostData, bstrHeaders, &bCancel);
	// TODO m_pEvents2->Fire_BeforeNavigate2(...)

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
	USES_CONVERSION;
	OLECHAR *pszURL = W2OLE((WCHAR *) aURL);
	BSTR bstrURL = SysAllocString(pszURL);
	m_pEvents1->Fire_NavigateComplete(bstrURL);
// TODO m_pEvents2->Fire_NavigateComplete2(...)
	m_pOwner->m_bBusy = FALSE;
	SysFreeString(bstrURL);

	return NS_OK;
}


NS_IMETHODIMP
CWebShellContainer::NewWebShell(nsIWebShell *&aNewWebShell)
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
