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

#include "nsIAccessible.h"

#include "Accessible.h"
#include "nsIWidget.h"
#include "nsWindow.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

//#define DEBUG_LEAKS

#ifdef DEBUG_LEAKS
static gAccessibles = 0;
#endif

// {61044601-A811-4e2b-BBBA-17BFABD329D7}
EXTERN_C GUID CDECL CLSID_Accessible =
{ 0x61044601, 0xa811, 0x4e2b, { 0xbb, 0xba, 0x17, 0xbf, 0xab, 0xd3, 0x29, 0xd7 } };

/*
 * Class Accessible
 */

#ifdef IS_ACCESSIBLE

// accessibility only on Windows2000 and Windows98
#ifdef OBJID_WINDOW


//-----------------------------------------------------
// construction 
//-----------------------------------------------------
Accessible::Accessible(nsIAccessible* aAcc, HWND aWnd)
{
  mAccessible = aAcc;
  mWnd = aWnd;
	m_cRef	        = 0;

#ifdef DEBUG_LEAKS
  printf("Accessibles=%d\n", ++gAccessibles);
#endif

}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
Accessible::~Accessible()
{
  m_cRef = 0;
#ifdef DEBUG_LEAKS
  printf("Accessibles=%d\n", --gAccessibles);
#endif

}


//-----------------------------------------------------
// IUnknown interface methods - see inknown.h for documentation
//-----------------------------------------------------
STDMETHODIMP Accessible::QueryInterface(REFIID riid, void** ppv)
{
	*ppv=NULL;

	if ( (IID_IUnknown == riid) || (IID_IAccessible	== riid) || (IID_IDispatch	== riid)) {
		*ppv = this;
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

//-----------------------------------------------------
STDMETHODIMP_(ULONG) Accessible::AddRef()
{
	return ++m_cRef;
}


//-----------------------------------------------------
STDMETHODIMP_(ULONG) Accessible::Release()
{
	if (0 != --m_cRef)
		return m_cRef;

	delete this;

	return 0;
}

//-----------------------------------------------------
// IAccessible methods
//-----------------------------------------------------

STDMETHODIMP Accessible::get_accParent( IDispatch __RPC_FAR *__RPC_FAR *ppdispParent)
{
  nsCOMPtr<nsIAccessible> parent = nsnull;
  mAccessible->GetAccParent(getter_AddRefs(parent));

  if (parent) {
    IAccessible* a = new Accessible(parent, mWnd);
    a->AddRef();
    *ppdispParent = a;
    return S_OK;
  }

  // if we have a widget but no parent nsIAccessible then we might have a native
  // widget parent that could give us a IAccessible. Lets check.
  HWND pWnd = ::GetParent(mWnd);
  if (pWnd) {
    // get the accessible.
    void* ptr = nsnull;
    AccessibleObjectFromWindow(pWnd, OBJID_WINDOW, IID_IAccessible, &ptr);
    IAccessible* a = (IAccessible*)ptr;
    // got one? return it.
    if (a) {
      *ppdispParent = a;
      return NS_OK;
    }
  }

  *ppdispParent = NULL;
  return S_FALSE;  
}

STDMETHODIMP Accessible::get_accChildCount( long __RPC_FAR *pcountChildren)
{
  PRInt32 count = 0;
  mAccessible->GetAccChildCount(&count);
  //printf("Count=%d\n",count);
  *pcountChildren = count;
  return S_OK;
}

STDMETHODIMP Accessible::get_accChild( 
      /* [in] */ VARIANT varChild,
      /* [retval][out] */ IDispatch __RPC_FAR *__RPC_FAR *ppdispChild)
{
  nsCOMPtr<nsIAccessible> a;
  GetNSAccessibleFor(varChild,a);

  if (a) 
  {
    // if its us return us
    if (a == mAccessible) {
      *ppdispChild = this;
      AddRef();
      return S_OK;
    }
      
    // create a new one.
    IAccessible* ia = new Accessible(a, mWnd);
    ia->AddRef();
    *ppdispChild = ia;
    return S_OK;
  }

  *ppdispChild = NULL;
  return S_FALSE;
}

STDMETHODIMP Accessible::get_accName( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszValue)
{
  *pszValue = NULL;
  nsCOMPtr<nsIAccessible> a;
  GetNSAccessibleFor(varChild,a);
  if (a) {
     nsXPIDLString name;
     nsresult rv = a->GetAccName(getter_Copies(name));
     if (NS_FAILED(rv))
        return S_FALSE;

     *pszValue = ::SysAllocString(name.get());
  }

  return S_OK;
}

STDMETHODIMP Accessible::get_accValue( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszValue)
{
  *pszValue = NULL;
  nsCOMPtr<nsIAccessible> a;
  GetNSAccessibleFor(varChild,a);
  if (a) {
     nsXPIDLString name;
     nsresult rv = a->GetAccValue(getter_Copies(name));
     if (NS_FAILED(rv))
        return S_FALSE;

     *pszValue = ::SysAllocString(name.get());
  }

  return S_OK;
}


STDMETHODIMP Accessible::get_accDescription( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszValue)
{
  *pszValue = NULL;
  nsCOMPtr<nsIAccessible> a;
  GetNSAccessibleFor(varChild,a);
  if (a) {
     nsXPIDLString name;
     nsresult rv = a->GetAccDescription(getter_Copies(name));
     if (NS_FAILED(rv))
        return S_FALSE;

     *pszValue = ::SysAllocString(name.get());
  }

  return S_OK;
}

STDMETHODIMP Accessible::get_accRole( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarRole)
{
   VariantInit(pvarRole);
   pvarRole->vt = VT_I4;

   nsCOMPtr<nsIAccessible> a;
   GetNSAccessibleFor(varChild,a);

   if (!a)
     return S_FALSE;

   nsXPIDLString idlrole;
   nsresult rv = a->GetAccRole(getter_Copies(idlrole));
   if (NS_FAILED(rv))
       return S_FALSE;

   nsAutoString role(idlrole);

   if (role.EqualsIgnoreCase("text"))
      pvarRole->lVal = ROLE_SYSTEM_TEXT;
   else if (role.EqualsIgnoreCase("static text"))
      pvarRole->lVal = ROLE_SYSTEM_STATICTEXT;
   else if (role.EqualsIgnoreCase("graphic"))
      pvarRole->lVal = ROLE_SYSTEM_GRAPHIC;
   else if (role.EqualsIgnoreCase("table"))
      pvarRole->lVal = ROLE_SYSTEM_TABLE;
   else if (role.EqualsIgnoreCase("cell"))
      pvarRole->lVal = ROLE_SYSTEM_CELL;
   else if (role.EqualsIgnoreCase("row"))
      pvarRole->lVal = ROLE_SYSTEM_ROW;
   else if (role.EqualsIgnoreCase("text"))
      pvarRole->lVal = ROLE_SYSTEM_TEXT;
   else if (role.EqualsIgnoreCase("combo box"))
      pvarRole->lVal = ROLE_SYSTEM_COMBOBOX;
   else if (role.EqualsIgnoreCase("link"))
      pvarRole->lVal = ROLE_SYSTEM_LINK;
   else if (role.EqualsIgnoreCase("list"))
      pvarRole->lVal = ROLE_SYSTEM_LIST;
   else if (role.EqualsIgnoreCase("list item"))
      pvarRole->lVal = ROLE_SYSTEM_LISTITEM;
   else if (role.EqualsIgnoreCase("push button"))
      pvarRole->lVal = ROLE_SYSTEM_PUSHBUTTON;
   else if (role.EqualsIgnoreCase("radio button"))
      pvarRole->lVal = ROLE_SYSTEM_RADIOBUTTON;
   else if (role.EqualsIgnoreCase("slider"))
      pvarRole->lVal = ROLE_SYSTEM_SLIDER;
   else if (role.EqualsIgnoreCase("client"))
      pvarRole->lVal = ROLE_SYSTEM_CLIENT;
   else if (role.EqualsIgnoreCase("window"))
      pvarRole->lVal = ROLE_SYSTEM_WINDOW;
   else
      pvarRole->lVal = ROLE_SYSTEM_ALERT;

   return S_OK;

}

STDMETHODIMP Accessible::get_accState( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarState)
{
   VariantInit(pvarState);
   pvarState->vt = VT_I4;
   pvarState->lVal = 0;

   nsCOMPtr<nsIAccessible> a;
   GetNSAccessibleFor(varChild,a);
   if (!a)
     return S_FALSE;

   PRUint32 state;
   nsresult rv = a->GetAccState(&state);
   if (NS_FAILED(rv))
     return S_FALSE;
   pvarState->lVal = state;

   return S_OK;
}

PRBool Accessible::InState(const nsString& aStates, const char* aState)
{
  return (aStates.Find(aState) == 0);
}

STDMETHODIMP Accessible::get_accHelp( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszHelp)
{
  return NULL;
}

STDMETHODIMP Accessible::get_accHelpTopic( 
      /* [out] */ BSTR __RPC_FAR *pszHelpFile,
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ long __RPC_FAR *pidTopic)
{
  return S_FALSE;
}

STDMETHODIMP Accessible::get_accKeyboardShortcut( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszKeyboardShortcut)
{
  return S_FALSE;
}

STDMETHODIMP Accessible::get_accFocus( 
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChild)
{
  VariantInit(pvarChild);
  pvarChild->vt = VT_EMPTY;
  return S_OK;
}

STDMETHODIMP Accessible::get_accSelection( 
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChildren)
{
  pvarChildren->vt = VT_EMPTY;
  return S_OK;
}

STDMETHODIMP Accessible::get_accDefaultAction( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszDefaultAction)
{
  return NULL;
}

STDMETHODIMP Accessible::accSelect( 
      /* [in] */ long flagsSelect,
      /* [optional][in] */ VARIANT varChild)
{
  return S_FALSE;
}

STDMETHODIMP Accessible::accLocation( 
      /* [out] */ long __RPC_FAR *pxLeft,
      /* [out] */ long __RPC_FAR *pyTop,
      /* [out] */ long __RPC_FAR *pcxWidth,
      /* [out] */ long __RPC_FAR *pcyHeight,
      /* [optional][in] */ VARIANT varChild)
{
  PRInt32 x,y,w,h;
  mAccessible->AccGetBounds(&x,&y,&w,&h);

  POINT cpos;
  cpos.x = x;
  cpos.y = y;

  ::ClientToScreen(mWnd, &cpos);

  *pxLeft = cpos.x;
  *pyTop = cpos.y;
  *pcxWidth = w;
  *pcyHeight = h;

  return S_OK;
}

STDMETHODIMP Accessible::accNavigate( 
      /* [in] */ long navDir,
      /* [optional][in] */ VARIANT varStart,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarEndUpAt)
{
  VariantInit(pvarEndUpAt);

  nsCOMPtr<nsIAccessible> acc;

  switch(navDir) {
    case NAVDIR_DOWN: 
      mAccessible->AccNavigateDown(getter_AddRefs(acc));
      break;
    case NAVDIR_FIRSTCHILD:
      mAccessible->GetAccFirstChild(getter_AddRefs(acc));
      break;
    case NAVDIR_LASTCHILD:
      mAccessible->GetAccLastChild(getter_AddRefs(acc));
      break;
    case NAVDIR_LEFT:
      mAccessible->AccNavigateLeft(getter_AddRefs(acc));
      break;
    case NAVDIR_NEXT:
      mAccessible->GetAccNextSibling(getter_AddRefs(acc));
      break;
    case NAVDIR_PREVIOUS:
      mAccessible->GetAccPreviousSibling(getter_AddRefs(acc));
      break;
    case NAVDIR_RIGHT:
      mAccessible->AccNavigateRight(getter_AddRefs(acc));
      break;
    case NAVDIR_UP:
      mAccessible->AccNavigateUp(getter_AddRefs(acc));
      break;
  }

  if (acc) {
     IAccessible* a = new Accessible(acc,mWnd);
     a->AddRef();
     pvarEndUpAt->vt = VT_DISPATCH;
     pvarEndUpAt->pdispVal = a;
     return NS_OK;
  } else {
     pvarEndUpAt->vt = VT_EMPTY;
     return S_FALSE;
  }
}

STDMETHODIMP Accessible::accHitTest( 
      /* [in] */ long xLeft,
      /* [in] */ long yTop,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChild)
{
  VariantInit(pvarChild);

  // convert to window coords
  nsCOMPtr<nsIAccessible> a;

  POINT cpos;
  cpos.x = xLeft;
  cpos.y = yTop;

  ::ScreenToClient(mWnd, &cpos);
  xLeft = cpos.x;
  yTop = cpos.y;

  mAccessible->AccGetAt(xLeft,yTop, getter_AddRefs(a));

  // if we got a child
  if (a) 
  {
    // if the child is us
    if (a == mAccessible) {
      pvarChild->vt = VT_I4;
      pvarChild->lVal = CHILDID_SELF;
    } else { // its not create an Accessible for it.
      pvarChild->vt = VT_DISPATCH;
      pvarChild->pdispVal = new Accessible(a, mWnd);
      pvarChild->pdispVal->AddRef();
    }
  } else {
      // no child at that point
      pvarChild->vt = VT_EMPTY;
      return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP Accessible::accDoDefaultAction( 
      /* [optional][in] */ VARIANT varChild)
{
    if (NS_SUCCEEDED(mAccessible->AccDoDefaultAction()))
      return S_OK;
    else
      return DISP_E_MEMBERNOTFOUND;
}

STDMETHODIMP Accessible::put_accName( 
      /* [optional][in] */ VARIANT varChild,
      /* [in] */ BSTR szName)
{
  return S_FALSE;
}

STDMETHODIMP Accessible::put_accValue( 
      /* [optional][in] */ VARIANT varChild,
      /* [in] */ BSTR szValue)
{
  return S_FALSE;
}

STDMETHODIMP 
Accessible::GetTypeInfoCount(UINT *p)
{
	*p = 0;
	return E_NOTIMPL;
}

STDMETHODIMP Accessible::GetTypeInfo(UINT i, LCID lcid, ITypeInfo **ppti)
{
	*ppti = 0;
	return E_NOTIMPL;
}

STDMETHODIMP 
Accessible::GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames,
                           UINT cNames, LCID lcid, DISPID *rgDispId)
{
	return E_NOTIMPL;
}

STDMETHODIMP Accessible::Invoke(DISPID dispIdMember, REFIID riid,
    LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
    VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
	return E_NOTIMPL;
}

//------- Helper methods ---------

void Accessible::GetNSAccessibleFor(VARIANT varChild, nsCOMPtr<nsIAccessible>& aAcc)
{
  if (varChild.lVal == CHILDID_SELF) 
    aAcc = mAccessible;
  else {
    long count = 1;
    nsCOMPtr<nsIAccessible> a;
    nsCOMPtr<nsIAccessible> next;
    mAccessible->GetAccFirstChild(getter_AddRefs(a));
    while(a) 
    {
      if (varChild.lVal == count)
      {
         aAcc = a;
         return;
      }

      a->GetAccNextSibling(getter_AddRefs(next));
      a = next;
      count++;
    }

    aAcc = nsnull;
  }  
}
#endif
#endif