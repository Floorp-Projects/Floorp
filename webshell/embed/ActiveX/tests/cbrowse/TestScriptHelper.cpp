// TestScriptHelper.cpp : Implementation of CTestScriptHelper
#include "stdafx.h"
#include "Cbrowse.h"
#include "TestScriptHelper.h"

/////////////////////////////////////////////////////////////////////////////
// CTestScriptHelper


STDMETHODIMP CTestScriptHelper::OutputString(BSTR bstrMessage)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	if (bstrMessage == NULL)
	{
		return E_INVALIDARG;
	}
	if (m_pBrowserInfo == NULL)
	{
		return E_UNEXPECTED;
	}

	USES_CONVERSION;
	m_pBrowserInfo->OutputString(OLE2T(bstrMessage));
	return S_OK;
}


STDMETHODIMP CTestScriptHelper::get_WebBrowser(LPDISPATCH *pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	if (pVal == NULL)
	{
		return E_INVALIDARG;
	}

	*pVal = NULL;
	if (m_pBrowserInfo == NULL)
	{
		return E_UNEXPECTED;
	}

	CIPtr(IWebBrowserApp) spWebBrowserApp;
	m_pBrowserInfo->GetWebBrowser(&spWebBrowserApp);
	return spWebBrowserApp->QueryInterface(IID_IDispatch, (void **) pVal);
}


STDMETHODIMP CTestScriptHelper::put_Result(TestResult newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	if (m_pBrowserInfo == NULL)
	{
		return E_UNEXPECTED;
	}

	m_pBrowserInfo->nResult = newVal;

	return S_OK;
}


STDMETHODIMP CTestScriptHelper::get_TestURL(BSTR *pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	if (pVal == NULL)
	{
		return E_INVALIDARG;
	}
	if (m_pBrowserInfo)
	{
		USES_CONVERSION;
		*pVal = SysAllocString(T2OLE(m_pBrowserInfo->szTestURL));
	}

	return S_OK;
}


STDMETHODIMP CTestScriptHelper::get_TestCGI(BSTR *pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	if (pVal == NULL)
	{
		return E_INVALIDARG;
	}
	if (m_pBrowserInfo)
	{
		USES_CONVERSION;
		*pVal = SysAllocString(T2OLE(m_pBrowserInfo->szTestCGI));
	}

	return S_OK;
}
