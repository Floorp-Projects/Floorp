/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _Accessible_H_
#define _Accessible_H_

#include "OLEIDL.H"
#include "OLEACC.H"

#include "nsCOMPtr.h"
#include "nsIAccessible.h"

#include "nsString.h"
//#define IS_ACCESSIBLE

class Accessible : public IAccessible
{
#ifdef IS_ACCESSIBLE

// accessibility only on Windows2000 and Windows98
#ifdef OBJID_WINDOW

	public: // construction, destruction
		Accessible(nsIAccessible*, HWND aWin = 0);
		~Accessible();

	public: // IUnknown methods - see iunknown.h for documentation
		STDMETHODIMP_(ULONG) AddRef        ();
		STDMETHODIMP 			QueryInterface(REFIID, void**);
		STDMETHODIMP_(ULONG) Release       ();

    // Return the registered OLE class ID of this object's CfDataObj.
		CLSID GetClassID() const;

	public: 

  virtual /* [id][propget][hidden] */ HRESULT STDMETHODCALLTYPE get_accParent( 
      /* [retval][out] */ IDispatch __RPC_FAR *__RPC_FAR *ppdispParent);

  virtual /* [id][propget][hidden] */ HRESULT STDMETHODCALLTYPE get_accChildCount( 
      /* [retval][out] */ long __RPC_FAR *pcountChildren);

  virtual /* [id][propget][hidden] */ HRESULT STDMETHODCALLTYPE get_accChild( 
      /* [in] */ VARIANT varChild,
      /* [retval][out] */ IDispatch __RPC_FAR *__RPC_FAR *ppdispChild);

  virtual /* [id][propget][hidden] */ HRESULT STDMETHODCALLTYPE get_accName( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszName);

  virtual /* [id][propget][hidden] */ HRESULT STDMETHODCALLTYPE get_accValue( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszValue);

  virtual /* [id][propget][hidden] */ HRESULT STDMETHODCALLTYPE get_accDescription( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszDescription);

  virtual /* [id][propget][hidden] */ HRESULT STDMETHODCALLTYPE get_accRole( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarRole);

  virtual /* [id][propget][hidden] */ HRESULT STDMETHODCALLTYPE get_accState( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarState);

  virtual /* [id][propget][hidden] */ HRESULT STDMETHODCALLTYPE get_accHelp( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszHelp);

  virtual /* [id][propget][hidden] */ HRESULT STDMETHODCALLTYPE get_accHelpTopic( 
      /* [out] */ BSTR __RPC_FAR *pszHelpFile,
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ long __RPC_FAR *pidTopic);

  virtual /* [id][propget][hidden] */ HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszKeyboardShortcut);

  virtual /* [id][propget][hidden] */ HRESULT STDMETHODCALLTYPE get_accFocus( 
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChild);

  virtual /* [id][propget][hidden] */ HRESULT STDMETHODCALLTYPE get_accSelection( 
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChildren);

  virtual /* [id][propget][hidden] */ HRESULT STDMETHODCALLTYPE get_accDefaultAction( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszDefaultAction);

  virtual /* [id][hidden] */ HRESULT STDMETHODCALLTYPE accSelect( 
      /* [in] */ long flagsSelect,
      /* [optional][in] */ VARIANT varChild);

  virtual /* [id][hidden] */ HRESULT STDMETHODCALLTYPE accLocation( 
      /* [out] */ long __RPC_FAR *pxLeft,
      /* [out] */ long __RPC_FAR *pyTop,
      /* [out] */ long __RPC_FAR *pcxWidth,
      /* [out] */ long __RPC_FAR *pcyHeight,
      /* [optional][in] */ VARIANT varChild);

  virtual /* [id][hidden] */ HRESULT STDMETHODCALLTYPE accNavigate( 
      /* [in] */ long navDir,
      /* [optional][in] */ VARIANT varStart,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarEndUpAt);

  virtual /* [id][hidden] */ HRESULT STDMETHODCALLTYPE accHitTest( 
      /* [in] */ long xLeft,
      /* [in] */ long yTop,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChild);

  virtual /* [id][hidden] */ HRESULT STDMETHODCALLTYPE accDoDefaultAction( 
      /* [optional][in] */ VARIANT varChild);

  virtual /* [id][propput][hidden] */ HRESULT STDMETHODCALLTYPE put_accName( 
      /* [optional][in] */ VARIANT varChild,
      /* [in] */ BSTR szName);

  virtual /* [id][propput][hidden] */ HRESULT STDMETHODCALLTYPE put_accValue( 
      /* [optional][in] */ VARIANT varChild,
      /* [in] */ BSTR szValue);


 	STDMETHODIMP GetTypeInfoCount(UINT *p);
	STDMETHODIMP GetTypeInfo(UINT i, LCID lcid, ITypeInfo **ppti);
  STDMETHODIMP GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames,
                               UINT cNames, LCID lcid, DISPID *rgDispId);
  STDMETHODIMP Invoke(DISPID dispIdMember, REFIID riid,
        LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr);



   	static ULONG g_cRef;              // the cum reference count of all instances
		ULONG        m_cRef;              // the reference count
    nsCOMPtr<nsIAccessible> mAccessible;
    HWND mWnd;

    protected:

    void GetNSAccessibleFor(VARIANT varChild, nsCOMPtr<nsIAccessible>& aAcc);
    PRBool InState(const nsString& aStates, const char* aState);
    STDMETHODIMP GetAttribute(const char* aName, VARIANT varChild, BSTR __RPC_FAR *aString);

#endif
#endif
};


#endif  
