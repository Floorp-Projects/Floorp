// TestScriptHelper.h : Declaration of the CTestScriptHelper

#ifndef __TESTSCRIPTHELPER_H_
#define __TESTSCRIPTHELPER_H_

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CTestScriptHelper
class ATL_NO_VTABLE CTestScriptHelper : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CTestScriptHelper, &CLSID_TestScriptHelper>,
	public IDispatchImpl<DITestScriptHelper, &IID_DITestScriptHelper, &LIBID_CbrowseLib>
{
public:
	CTestScriptHelper()
	{
		m_pBrowserInfo = NULL;
	}

	BrowserInfo *m_pBrowserInfo;

DECLARE_REGISTRY_RESOURCEID(IDR_TESTSCRIPTHELPER)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CTestScriptHelper)
	COM_INTERFACE_ENTRY(DITestScriptHelper)
	COM_INTERFACE_ENTRY(IDispatch)
END_COM_MAP()

// DITestScriptHelper
public:
	STDMETHOD(put_Result)(/*[in]*/ TestResult newVal);
	STDMETHOD(get_WebBrowser)(/*[out, retval]*/ LPDISPATCH *pVal);
	STDMETHOD(OutputString)(BSTR bstrMessage);
};

typedef CComObject<CTestScriptHelper> CTestScriptHelperInstance;

#endif //__TESTSCRIPTHELPER_H_
