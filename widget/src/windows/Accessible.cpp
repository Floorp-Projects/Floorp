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
#include "nsIAccessibleEventReceiver.h"

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

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


//-----------------------------------------------------
// construction 
//-----------------------------------------------------
Accessible::Accessible(nsIAccessible* aAcc, HWND aWnd)
{
  mAccessible = aAcc;  // The nsIAccessible we're proxying from

  mWnd = aWnd;  // The window handle, for NotifyWinEvent, or getting the accessible parent thru the window parent
  m_cRef = 0;   // for reference counting, so we know when to delete ourselves

  // mCachedIndex and mCachedChild allows fast order(N) indexing of children when moving forward through the 
  // list 1 at a time,rather than going back to the first child each time, it can just get the next sibling
  mCachedIndex = 0;
  mCachedChild = NULL;   

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

  if ( (IID_IUnknown == riid) || (IID_IAccessible == riid) || (IID_IDispatch  == riid)) {
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

HINSTANCE Accessible::gmAccLib = 0;
LPFNACCESSIBLEOBJECTFROMWINDOW Accessible::gmAccessibleObjectFromWindow = 0;
LPFNLRESULTFROMOBJECT Accessible::gmLresultFromObject = 0;

//-----------------------------------------------------
// IAccessible methods
//-----------------------------------------------------

STDMETHODIMP Accessible::AccessibleObjectFromWindow(
  HWND hwnd,
  DWORD dwObjectID,
  REFIID riid,
  void **ppvObject)
{

  // open the dll dynamically
    if (!gmAccLib) 
       gmAccLib =::LoadLibrary("OLEACC.DLL");  

    if (gmAccLib) {
      if (!gmAccessibleObjectFromWindow)
       gmAccessibleObjectFromWindow = (LPFNACCESSIBLEOBJECTFROMWINDOW)GetProcAddress(gmAccLib,"AccessibleObjectFromWindow");

      return gmAccessibleObjectFromWindow(hwnd, dwObjectID, riid, ppvObject);
    }
    

    return S_FALSE;
}



STDMETHODIMP_(LRESULT) Accessible::LresultFromObject(
  REFIID riid,
  WPARAM wParam,
  LPUNKNOWN pAcc)
{
    // open the dll dynamically
    if (!gmAccLib) 
       gmAccLib =::LoadLibrary("OLEACC.DLL");  

    if (gmAccLib) {
      if (!gmAccessibleObjectFromWindow)
       gmLresultFromObject = (LPFNLRESULTFROMOBJECT)GetProcAddress(gmAccLib,"LresultFromObject");

      return gmLresultFromObject(riid,wParam,pAcc);
    }

    return 0;
}


STDMETHODIMP Accessible::get_accParent( IDispatch __RPC_FAR *__RPC_FAR *ppdispParent)
{
  nsCOMPtr<nsIAccessible> parent(nsnull);
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
    if (AccessibleObjectFromWindow(pWnd, OBJID_WINDOW, IID_IAccessible, &ptr) != S_FALSE) {
      IAccessible* a = (IAccessible*)ptr;
      // got one? return it.
      if (a) {
        *ppdispParent = a;
        return NS_OK;
      }
    }
  }

  *ppdispParent = NULL;
  return S_FALSE;  
}

STDMETHODIMP Accessible::get_accChildCount( long __RPC_FAR *pcountChildren)
{
  PRInt32 count = 0;
  mAccessible->GetAccChildCount(&count);
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

   PRUint32 role = 0;
   nsresult rv = a->GetAccRole(&role);
   if (NS_FAILED(rv))
     return S_FALSE;

   pvarRole->lVal = role;
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
  // Return the current nsIAccessible that has focus
  VariantInit(pvarChild);

  nsCOMPtr<nsIAccessible> focusedAccessible;
  if (NS_SUCCEEDED(mAccessible->GetAccFocused(getter_AddRefs(focusedAccessible)))) {
    pvarChild->vt = VT_DISPATCH;
    pvarChild->pdispVal = new Accessible(focusedAccessible, mWnd);
    pvarChild->pdispVal->AddRef();
    return S_OK;
  }

  pvarChild->vt = VT_EMPTY;
  return S_FALSE;
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
  *pszDefaultAction = NULL;
  nsCOMPtr<nsIAccessible> a;
  GetNSAccessibleFor(varChild,a);
  if (a) {
     nsXPIDLString name;
     nsresult rv = a->GetAccActionName(0,getter_Copies(name));
     if (NS_FAILED(rv))
        return S_FALSE;

     *pszDefaultAction = ::SysAllocString(name.get());
  }

  return S_OK;
}

STDMETHODIMP Accessible::accSelect( 
      /* [in] */ long flagsSelect,
      /* [optional][in] */ VARIANT varChild)
{
  // currently only handle focus and selection
  if (flagsSelect & (SELFLAG_TAKEFOCUS|SELFLAG_TAKESELECTION|SELFLAG_REMOVESELECTION))
  {
    if (flagsSelect & SELFLAG_TAKEFOCUS)
      mAccessible->AccTakeFocus();

    if (flagsSelect & SELFLAG_TAKESELECTION)
      mAccessible->AccTakeSelection();

    if (flagsSelect & SELFLAG_REMOVESELECTION)
      mAccessible->AccRemoveSelection();


    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP Accessible::accLocation( 
      /* [out] */ long __RPC_FAR *pxLeft,
      /* [out] */ long __RPC_FAR *pyTop,
      /* [out] */ long __RPC_FAR *pcxWidth,
      /* [out] */ long __RPC_FAR *pcyHeight,
      /* [optional][in] */ VARIANT varChild)
{
  nsCOMPtr<nsIAccessible> a;
  GetNSAccessibleFor(varChild,a);

  if (a) {
    PRInt32 x,y,w,h;
    a->AccGetBounds(&x,&y,&w,&h);

    POINT cpos;
    cpos.x = x;
    cpos.y = y;

    *pxLeft = x;
    *pyTop = y;
    *pcxWidth = w;
    *pcyHeight = h;
    return S_OK;
  }

  return S_FALSE;  
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

  xLeft = xLeft;
  yTop = yTop;

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
  return NS_SUCCEEDED(mAccessible->AccDoAction(0))? NS_OK: DISP_E_MEMBERNOTFOUND;
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

// For IDispatch support
STDMETHODIMP 
Accessible::GetTypeInfoCount(UINT *p)
{
  *p = 0;
  return E_NOTIMPL;
}

// For IDispatch support
STDMETHODIMP Accessible::GetTypeInfo(UINT i, LCID lcid, ITypeInfo **ppti)
{
  *ppti = 0;
  return E_NOTIMPL;
}

// For IDispatch support
STDMETHODIMP 
Accessible::GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames,
                           UINT cNames, LCID lcid, DISPID *rgDispId)
{
  return E_NOTIMPL;
}

// For IDispatch support
STDMETHODIMP Accessible::Invoke(DISPID dispIdMember, REFIID riid,
    LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
    VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
  return E_NOTIMPL;
}

//------- Helper methods ---------

void Accessible::GetNSAccessibleFor(VARIANT varChild, nsCOMPtr<nsIAccessible>& aAcc)
{
  // if its us real easy
  if (varChild.lVal == CHILDID_SELF) {
    aAcc = mAccessible;
  } else if (mCachedChild && varChild.lVal == mCachedIndex+1) {
    // if the cachedIndex is not self and the if its the one right
    // before the one we want to get. Then just ask it for its next.
    nsCOMPtr<nsIAccessible> next;
    mCachedChild->GetAccNextSibling(getter_AddRefs(next));
    if (next) {
      mCachedIndex++;
      mCachedChild = next;
    }

    aAcc = next;
    return;

  } else {
    // if the cachedindex is not the one right before we have to start at the begining
    nsCOMPtr<nsIAccessible> a;
    mAccessible->GetAccFirstChild(getter_AddRefs(mCachedChild));
    mCachedIndex = 1;
    while(mCachedChild) 
    {
      if (varChild.lVal == mCachedIndex)
      {
         aAcc = mCachedChild;
         return;
      }

      mCachedChild->GetAccNextSibling(getter_AddRefs(a));
      mCachedChild = a;
      mCachedIndex++;
    }

    aAcc = nsnull;
  }  
}

//----- Root Accessible -----

NS_IMPL_QUERY_INTERFACE1(RootAccessible, nsIAccessibleEventListener)

NS_IMETHODIMP_(nsrefcnt) 
RootAccessible::AddRef(void)
{
  return Accessible::AddRef();
}

NS_IMETHODIMP_(nsrefcnt) 
RootAccessible::Release(void)
{
  return Accessible::Release();
}

RootAccessible::RootAccessible(nsIAccessible* aAcc, HWND aWnd):Accessible(aAcc,aWnd)

{
    mListCount = 0;
    mNextId = -1;
    mNextPos = 0;

    nsCOMPtr<nsIAccessibleEventReceiver> r(do_QueryInterface(mAccessible));
    if (r) 
      r->AddAccessibleEventListener(this);
}

RootAccessible::~RootAccessible()
{
    nsCOMPtr<nsIAccessibleEventReceiver> r(do_QueryInterface(mAccessible));
    if (r) 
      r->RemoveAccessibleEventListener(this);

    // free up accessibles
    for (int i=0; i < mListCount; i++)
      mList[i].mAccessible = nsnull;
}

void RootAccessible::GetNSAccessibleFor(VARIANT varChild, nsCOMPtr<nsIAccessible>& aAcc)
{
  aAcc = nsnull;
  Accessible::GetNSAccessibleFor(varChild, aAcc);

  if (aAcc)
    return;

  for (int i=0; i < mListCount; i++)
  {
    if (varChild.lVal == mList[i].mId) {
        aAcc = mList[i].mAccessible;
        return;
    }
  }    
}

NS_IMETHODIMP RootAccessible::HandleEvent(PRUint32 aEvent, nsIAccessible* aAccessible)
{
#ifdef DEBUG
  // print focus event!!
  printf("Focus Changed!!!\n");
#endif

  // get the id for the accessible
  PRInt32 id = GetIdFor(aAccessible);

  // notify the window system
  NotifyWinEvent(aEvent, mWnd, OBJID_CLIENT, id);
  

  return NS_OK;
}

PRInt32 RootAccessible::GetIdFor(nsIAccessible* aAccessible)
{
  // max of 99999 ids can be generated
  if (mNextId < -99999)
    mNextId = -1;
  
  // Lets make one and add it to the list
  mList[mNextPos].mId = mNextId;
  mList[mNextPos].mAccessible = aAccessible;

  mNextId--;
  if (++mNextPos >= MAX_LIST_SIZE)
    mNextPos = 0;
  if (mListCount < MAX_LIST_SIZE)
    mListCount++;

  return mNextId+1;
}

