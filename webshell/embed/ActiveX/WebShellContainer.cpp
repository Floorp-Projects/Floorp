#include "stdafx.h"

#include "WebShellContainer.h"


static NS_DEFINE_IID(kIWebShellContainerIID, NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);


CWebShellContainer::CWebShellContainer(CMozillaBrowser *pOwner)
{
	m_pOwner = pOwner;
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
	return NS_OK;
}

NS_IMETHODIMP
CWebShellContainer::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
	USES_CONVERSION;
	OLECHAR *pszURL = W2OLE((WCHAR *)aURL);
	BSTR bstrURL = SysAllocString(pszURL);
	BSTR bstrTargetFrameName = NULL;
	BSTR bstrHeaders = NULL;
	VARIANT *pvPostData = NULL;
	VARIANT_BOOL bCancel = VARIANT_FALSE;
	long lFlags = 0;

	m_pOwner->Fire_BeforeNavigate(bstrURL, lFlags, bstrTargetFrameName, pvPostData, bstrHeaders, &bCancel);
	if (bCancel == VARIANT_TRUE)
	{
		// TODO cancel browsing somehow
	}

	SysFreeString(bstrURL);
	SysFreeString(bstrTargetFrameName);
	SysFreeString(bstrHeaders);

	return NS_OK;
}

NS_IMETHODIMP
CWebShellContainer::ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax)
{
	m_pOwner->Fire_ProgressChange(aProgress, aProgressMax);
	return NS_OK;
}

NS_IMETHODIMP
CWebShellContainer::EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aStatus)
{
	USES_CONVERSION;
	OLECHAR *pszURL = W2OLE((WCHAR *) aURL);
	BSTR bstrURL = SysAllocString(pszURL);
	m_pOwner->Fire_NavigateComplete(bstrURL);
	SysFreeString(bstrURL);

	return NS_OK;
}

NS_IMETHODIMP
CWebShellContainer::NewWebShell(nsIWebShell *&aNewWebShell)
{
  nsresult rv = NS_ERROR_OUT_OF_MEMORY;
  return rv;
}

NS_IMETHODIMP
CWebShellContainer::FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult)
{
  return NS_ERROR_FAILURE;
}


///////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver implementation


NS_IMETHODIMP
CWebShellContainer::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CWebShellContainer::OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CWebShellContainer::OnStatus(nsIURL* aURL, const nsString &aMsg)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CWebShellContainer::OnStopBinding(nsIURL* aURL, PRInt32 aStatus, const nsString &aMsg)
{
  return NS_ERROR_FAILURE;
}
