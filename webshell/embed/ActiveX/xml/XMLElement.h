// XMLElement.h : Declaration of the CXMLElement

#ifndef __XMLELEMENT_H_
#define __XMLELEMENT_H_

#include "resource.h"       // main symbols

#include <vector>
#include <string>
#include <map>

typedef std::map<std::string, std::string> StringMap;
typedef std::vector< CComQIPtr<IXMLElement, &IID_IXMLElement> > ElementList;

/////////////////////////////////////////////////////////////////////////////
// CXMLElement
class ATL_NO_VTABLE CXMLElement : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CXMLElement, &CLSID_MozXMLElement>,
	public IDispatchImpl<IXMLElement, &IID_IXMLElement, &LIBID_MozActiveXMLLib>
{
	// Pointer to parent
	IXMLElement *m_pParent;
	// List of children
	ElementList m_cChildren;
	// Tag name
	std::string m_szTagName;
	// Text
	std::string m_szText;
	// Type
	long m_nType;
	// Attribute list
	StringMap m_cAttributes;

public:
	CXMLElement();
	virtual ~CXMLElement();

	virtual HRESULT SetParent(IXMLElement *pParent);
	virtual HRESULT PutType(long nType);
	virtual HRESULT ReleaseAll();

DECLARE_REGISTRY_RESOURCEID(IDR_XMLELEMENT)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CXMLElement)
	COM_INTERFACE_ENTRY(IXMLElement)
	COM_INTERFACE_ENTRY(IDispatch)
END_COM_MAP()

// IXMLElement
	virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_tagName(/* [out][retval] */ BSTR __RPC_FAR *p);
	virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_tagName(/* [in] */ BSTR p);
	virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_parent(/* [out][retval] */ IXMLElement __RPC_FAR *__RPC_FAR *ppParent);
	virtual /* [id] */ HRESULT STDMETHODCALLTYPE setAttribute(/* [in] */ BSTR strPropertyName, /* [in] */ VARIANT PropertyValue);
	virtual /* [id] */ HRESULT STDMETHODCALLTYPE getAttribute(/* [in] */ BSTR strPropertyName, /* [out][retval] */ VARIANT __RPC_FAR *PropertyValue);
	virtual /* [id] */ HRESULT STDMETHODCALLTYPE removeAttribute(/* [in] */ BSTR strPropertyName);
	virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_children(/* [out][retval] */ IXMLElementCollection __RPC_FAR *__RPC_FAR *pp);
	virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_type(/* [out][retval] */ long __RPC_FAR *plType);
	virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_text(/* [out][retval] */ BSTR __RPC_FAR *p);
	virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_text(/* [in] */ BSTR p);
	virtual /* [id] */ HRESULT STDMETHODCALLTYPE addChild(/* [in] */ IXMLElement __RPC_FAR *pChildElem, long lIndex, long lReserved);
	virtual /* [id] */ HRESULT STDMETHODCALLTYPE removeChild(/* [in] */ IXMLElement __RPC_FAR *pChildElem);

public:
};

typedef CComObject<CXMLElement> CXMLElementInstance;

#endif //__XMLELEMENT_H_
