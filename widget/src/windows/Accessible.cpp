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
#include "nsIAccessibleDocument.h"
#include "nsIAccessibleSelectable.h"
#include "nsIAccessibilityService.h"
#include "Accessible.h"
#include "nsIWidget.h"
#include "nsWindow.h"
#include "nsCOMPtr.h"
#include "nsIAccessibleEventReceiver.h"
#include "nsReadableUtils.h"
#include "nsITextContent.h"
#include "nsIDocument.h"
#include "nsIXULDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsINameSpaceManager.h"
#include "String.h"

// for the COM IEnumVARIANT solution in get_AccSelection()
#define _ATLBASE_IMPL
#include <atlbase.h>
extern CComModule _Module;
#define _ATLCOM_IMPL
#include <atlcom.h>

 /* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

#define DEBUG_LEAKS 1
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

CComModule _Module;

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
Accessible::Accessible(nsIAccessible* aAcc, nsIDOMNode* aNode, HWND aWnd): SimpleDOMNode(aAcc, aNode, aWnd)
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
// IUnknown interface methods - see iunknown.h for documentation
//-----------------------------------------------------
STDMETHODIMP Accessible::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IUnknown == iid || IID_IDispatch == iid || IID_IAccessible == iid)
    *ppv = NS_STATIC_CAST(IAccessible*, this);
 
  if (NULL == *ppv)
    return SimpleDOMNode::QueryInterface(iid,ppv);
    
  (NS_REINTERPRET_CAST(IUnknown*, *ppv))->AddRef();
  return S_OK;
}

//-----------------------------------------------------
STDMETHODIMP_(ULONG) Accessible::AddRef()
{
  return SimpleDOMNode::AddRef();
}


//-----------------------------------------------------
STDMETHODIMP_(ULONG) Accessible::Release()
{
  return SimpleDOMNode::Release();
}

HINSTANCE Accessible::gmAccLib = 0;
HINSTANCE Accessible::gmUserLib = 0;
LPFNACCESSIBLEOBJECTFROMWINDOW Accessible::gmAccessibleObjectFromWindow = 0;
LPFNLRESULTFROMOBJECT Accessible::gmLresultFromObject = 0;


LPFNNOTIFYWINEVENT Accessible::gmNotifyWinEvent = 0;



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

      if (gmAccessibleObjectFromWindow)
        return gmAccessibleObjectFromWindow(hwnd, dwObjectID, riid, ppvObject);
    }
    

    return E_FAIL;
}

STDMETHODIMP Accessible::NotifyWinEvent(
  DWORD event,
  HWND hwnd,
  LONG idObjectType,
  LONG idObject)
{

  // open the dll dynamically
    if (!gmUserLib) 
       gmUserLib =::LoadLibrary("USER32.DLL");  

    if (gmUserLib) {
      if (!gmNotifyWinEvent)
       gmNotifyWinEvent = (LPFNNOTIFYWINEVENT)GetProcAddress(gmUserLib,"NotifyWinEvent");

      if (gmNotifyWinEvent)
         return gmNotifyWinEvent(event, hwnd, idObjectType, idObject);
    }
    
    return E_FAIL;
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

      if (gmLresultFromObject)
         return gmLresultFromObject(riid,wParam,pAcc);
    }

    return 0;
}


STDMETHODIMP Accessible::get_accParent( IDispatch __RPC_FAR *__RPC_FAR *ppdispParent)
{
  nsCOMPtr<nsIAccessible> parent(nsnull);
  mAccessible->GetAccParent(getter_AddRefs(parent));

  if (parent) {
    IAccessible* a = NewAccessible(parent, nsnull, mWnd);
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
    HRESULT result = AccessibleObjectFromWindow(pWnd, OBJID_WINDOW, IID_IAccessible, &ptr);
    if (SUCCEEDED(result)) {
      IAccessible* a = (IAccessible*)ptr;
      // got one? return it.
      if (a) {
        *ppdispParent = a;
        return NS_OK;
      }
    }
  }

  *ppdispParent = NULL;
  return E_FAIL;  
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
    IAccessible* ia = NewAccessible(a, nsnull, mWnd);
    ia->AddRef();
    *ppdispChild = ia;
    return S_OK;
  }

  *ppdispChild = NULL;
  return E_FAIL;
}

STDMETHODIMP Accessible::get_accName( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszName)
{
  *pszName = NULL;
  nsCOMPtr<nsIAccessible> a;
  GetNSAccessibleFor(varChild,a);
  if (a) {
     nsAutoString name;
     nsresult rv = a->GetAccName(name);
     if (NS_FAILED(rv))
        return S_FALSE;

     *pszName = ::SysAllocString(name.get());
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
     nsAutoString value;
     nsresult rv = a->GetAccValue(value);
     if (NS_FAILED(rv))
        return S_FALSE;

     *pszValue = ::SysAllocString(value.get());
  }

  return S_OK;
}


STDMETHODIMP Accessible::get_accDescription( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszDescription)
{
  *pszDescription = NULL;
  nsCOMPtr<nsIAccessible> a;
  GetNSAccessibleFor(varChild,a);
  if (a) {
     nsAutoString description;
     nsresult rv = a->GetAccDescription(description);
     if (NS_FAILED(rv))
        return S_FALSE;

     *pszDescription = ::SysAllocString(description.get());
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
     return E_FAIL;

   PRUint32 role = 0;
   nsresult rv = a->GetAccRole(&role);
   if (NS_FAILED(rv))
     return E_FAIL;

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
     return E_FAIL;

   PRUint32 state;
   nsresult rv = a->GetAccState(&state);
   if (NS_FAILED(rv))
     return E_FAIL;

   pvarState->lVal = state;

   return S_OK;
}


STDMETHODIMP Accessible::get_accHelp( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszHelp)
{
  *pszHelp = NULL;
  return S_FALSE;
}

STDMETHODIMP Accessible::get_accHelpTopic( 
      /* [out] */ BSTR __RPC_FAR *pszHelpFile,
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ long __RPC_FAR *pidTopic)
{
  *pszHelpFile = NULL;
  *pidTopic = 0;
  return S_FALSE;
}

STDMETHODIMP Accessible::get_accKeyboardShortcut( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszKeyboardShortcut)
{
  *pszKeyboardShortcut = NULL;
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
    pvarChild->pdispVal = NewAccessible(focusedAccessible, nsnull, mWnd);
    pvarChild->pdispVal->AddRef();
    return S_OK;
  }

  pvarChild->vt = VT_EMPTY;
  return E_FAIL;
}
/**
  * This method is called when a client wants to know which children of a node
  *  are selected. Currently we only handle this for HTML selects, which are the
  *  only nsIAccessible objects to implement nsIAccessibleSelectable.
  *
  * The VARIANT return value arguement is expected to either contain a single IAccessible
  *  or an IEnumVARIANT of IAccessibles. We return the IEnumVARIANT regardless of the number
  *  of options selected, unless there are none selected in which case we return an empty
  *  VARIANT.
  *
  * The typedefs at the beginning set up the structure that will contain an array 
  *  of the IAccessibles. It implements the IEnumVARIANT interface, allowing us to 
  *  use it to return the IAccessibles in the VARIANT.
  *
  * We get the selected options from the select's accessible object and then put create 
  *  IAccessible objects for them and put those in the CComObject<EnumeratorType> 
  *  object. Then we put the CComObject<EnumeratorType> object in the VARIANT and return.
  *
  * returns a VT_EMPTY VARIANT if:
  *  - there are no options in the select
  *  - none of the options are selected
  *  - there is an error QIing to IEnumVARIANT
  *  - The object is not the type that can have children selected
  */
STDMETHODIMP Accessible::get_accSelection( 
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChildren)
{
  typedef VARIANT                      ItemType;              /* type of the object to be stored in container */
  typedef ItemType                     EnumeratorExposedType; /* the type of the item exposed by the enumerator interface */
  typedef IEnumVARIANT                 EnumeratorInterface;   /* a COM enumerator ( IEnumXXXXX ) interface */
  typedef _Copy<EnumeratorExposedType> EnumeratorCopyPolicy;  /* Copy policy class */
  typedef CComEnum<EnumeratorInterface,
                   &__uuidof(EnumeratorInterface),
                   EnumeratorExposedType,
                   EnumeratorCopyPolicy > EnumeratorType;

  IEnumVARIANT* pUnk = NULL;
  CComObject<EnumeratorType>* pEnum = NULL;
  VariantInit(pvarChildren);
  pvarChildren->vt = VT_EMPTY;
  
  nsCOMPtr<nsIAccessibleSelectable> select(do_QueryInterface(mAccessible));
  if (select) {  // do we have an nsIAccessibleSelectable?
    // we have an accessible that can have children selected
    nsCOMPtr<nsISupportsArray> selectedOptions;
    // gets the selected options as nsIAccessibles.
    select->GetSelectedChildren(getter_AddRefs(selectedOptions));
    if (selectedOptions) { // false if the select has no children or none are selected
      PRUint32 length;
      selectedOptions->Count(&length);
      CComVariant* optionArray = new CComVariant[length]; // needs to be a CComVariant to go into the EnumeratorType object

      // 1) Populate an array to store in the enumeration
      for (PRUint32 i = 0 ; i < length ; i++) {
        nsCOMPtr<nsISupports> tempOption;
        selectedOptions->GetElementAt(i,getter_AddRefs(tempOption)); // this expects an nsISupports
        if (tempOption) {
          nsCOMPtr<nsIAccessible> tempAccess(do_QueryInterface(tempOption));
          if ( tempAccess ) {
            optionArray[i] = NewAccessible(tempAccess, nsnull, mWnd);
          }
        }
      }

      // 2) Create and initialize the enumeration
      HRESULT hr = CComObject<EnumeratorType>::CreateInstance(&pEnum);
      pEnum->Init(&optionArray[0], &optionArray[length], NULL, AtlFlagCopy);
      pEnum->QueryInterface(IID_IEnumVARIANT, reinterpret_cast<void**>(&pUnk));
      delete [] optionArray; // clean up, the Init call copies the data (AtlFlagCopy)

      // 3) Put the enumerator in the VARIANT
      if (!pUnk)
        return NS_ERROR_FAILURE;
      pvarChildren->vt = VT_UNKNOWN;    // this must be VT_UNKNOWN for an IEnumVARIANT
      pvarChildren->punkVal = pUnk;
    }
  }
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
     nsAutoString defaultAction;
     nsresult rv = a->GetAccActionName(0,defaultAction);
     if (NS_FAILED(rv))
        return S_FALSE;

     *pszDefaultAction = ::SysAllocString(defaultAction.get());
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

  return E_FAIL;
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

  return E_FAIL;  
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
     IAccessible* a = NewAccessible(acc, nsnull, mWnd);
     a->AddRef();
     pvarEndUpAt->vt = VT_DISPATCH;
     pvarEndUpAt->pdispVal = a;
     return NS_OK;
  } else {
     pvarEndUpAt->vt = VT_EMPTY;
     return E_FAIL;
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
      pvarChild->pdispVal = NewAccessible(a, nsnull, mWnd);
      pvarChild->pdispVal->AddRef();
    }
  } else {
      // no child at that point
      pvarChild->vt = VT_EMPTY;
      return E_FAIL;
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
  return E_NOTIMPL;
}

STDMETHODIMP Accessible::put_accValue( 
      /* [optional][in] */ VARIANT varChild,
      /* [in] */ BSTR szValue)
{
  return E_NOTIMPL;
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

IAccessible *Accessible::NewAccessible(nsIAccessible *aNSAcc, nsIDOMNode *aNode, HWND aWnd)
{
  IAccessible *retval = nsnull;
  if (aNSAcc) {
    nsCOMPtr<nsIAccessibleDocument> accDoc(do_QueryInterface(aNSAcc));
    if (accDoc) {
      nsCOMPtr<nsIDocument> doc;
      accDoc->GetDocument(getter_AddRefs(doc));
      nsCOMPtr<nsIDOMNode> node(do_QueryInterface(doc));
      retval = new DocAccessible(aNSAcc, node, aWnd);
    }
    else 
      retval = new Accessible(aNSAcc, aNode, aWnd);
  }
  return retval;
}


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



//----- DocAccessible -----

// Microsoft COM QueryInterface
STDMETHODIMP DocAccessible::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IUnknown == iid || IID_IDispatch == iid || IID_ISimpleDOMDocument == iid)
    *ppv = NS_STATIC_CAST(ISimpleDOMDocument*, this);
 
  if (NULL == *ppv)
    return Accessible::QueryInterface(iid,ppv);
    
  (NS_REINTERPRET_CAST(IUnknown*, *ppv))->AddRef();
  return S_OK;
}


NS_IMETHODIMP_(nsrefcnt) 
DocAccessible::AddRef(void)
{
  return Accessible::AddRef();
}
  
NS_IMETHODIMP_(nsrefcnt) 
DocAccessible::Release(void)
{
  return Accessible::Release();
}


DocAccessible::DocAccessible(nsIAccessible* aAcc, nsIDOMNode *aNode, HWND aWnd):Accessible(aAcc,aNode,aWnd)
{
}

DocAccessible::~DocAccessible()
{
}


STDMETHODIMP DocAccessible::get_URL(/* [out] */ BSTR __RPC_FAR *aURL)
{
  nsCOMPtr<nsIAccessibleDocument> accDoc(do_QueryInterface(mAccessible));
  if (accDoc) {
    nsAutoString URL;
    if (NS_SUCCEEDED(accDoc->GetURL(URL))) {
      *aURL= ::SysAllocString(URL.get());
      return S_OK;
    }
  }
  return E_FAIL;
}

STDMETHODIMP DocAccessible::get_title( /* [out] */ BSTR __RPC_FAR *aTitle)
{
  nsCOMPtr<nsIAccessibleDocument> accDoc(do_QueryInterface(mAccessible));
  if (accDoc) {
    nsAutoString title;
    if (NS_SUCCEEDED(accDoc->GetTitle(title))) { // getter_Copies(pszTitle)))) {
      *aTitle= ::SysAllocString(title.get());
      return S_OK;
    }
  }
  return E_FAIL;
}

STDMETHODIMP DocAccessible::get_mimeType(/* [out] */ BSTR __RPC_FAR *aMimeType)
{
  nsCOMPtr<nsIAccessibleDocument> accDoc(do_QueryInterface(mAccessible));
  if (accDoc) {
    nsAutoString mimeType;
    if (NS_SUCCEEDED(accDoc->GetMimeType(mimeType))) {
      *aMimeType= ::SysAllocString(mimeType.get());
      return S_OK;
    }
  }
  return E_FAIL;
}

STDMETHODIMP DocAccessible::get_docType(/* [out] */ BSTR __RPC_FAR *aDocType)
{
  nsCOMPtr<nsIAccessibleDocument> accDoc(do_QueryInterface(mAccessible));
  if (accDoc) {
    nsAutoString docType;
    if (NS_SUCCEEDED(accDoc->GetDocType(docType))) {
      *aDocType= ::SysAllocString(docType.get());
      return S_OK;
    }
  }
  return E_FAIL;
}

STDMETHODIMP DocAccessible::get_nameSpaceURIForID(/* [in] */  short aNameSpaceID,
  /* [out] */ BSTR __RPC_FAR *aNameSpaceURI)
{
  nsCOMPtr<nsIAccessibleDocument> accDoc(do_QueryInterface(mAccessible));
  if (accDoc) {
    *aNameSpaceURI = NULL;
    nsAutoString nameSpaceURI;
    if (NS_SUCCEEDED(accDoc->GetNameSpaceURIForID(aNameSpaceID, nameSpaceURI))) 
      *aNameSpaceURI = ::SysAllocString(nameSpaceURI.get());
    return S_OK;
  }

  return E_FAIL;
}


STDMETHODIMP DocAccessible::put_alternateViewMediaTypes( /* [in] */ BSTR __RPC_FAR *commaSeparatedMediaTypes)
{
  return E_NOTIMPL;
}

//----- Root Accessible -----

// XPCOM QueryInterface
NS_IMPL_QUERY_INTERFACE1(RootAccessible, nsIAccessibleEventListener)

// Microsoft COM QueryInterface
STDMETHODIMP RootAccessible::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IUnknown == iid || IID_IDispatch == iid || IID_ISimpleDOMDocument == iid)
    *ppv = NS_STATIC_CAST(ISimpleDOMDocument*, this);
 
  if (NULL == *ppv)
    return DocAccessible::QueryInterface(iid, ppv);
    
  (NS_REINTERPRET_CAST(IUnknown*, *ppv))->AddRef();
  return S_OK;
}


NS_IMETHODIMP_(nsrefcnt) 
RootAccessible::AddRef(void)
{
  return DocAccessible::AddRef();
}
  
NS_IMETHODIMP_(nsrefcnt) 
RootAccessible::Release(void)
{
  return DocAccessible::Release();
}

RootAccessible::RootAccessible(nsIAccessible* aAcc, HWND aWnd):DocAccessible(aAcc,nsnull,aWnd)
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
    r->RemoveAccessibleEventListener();

  // free up accessibles
  for (int i=0; i < mListCount; i++)
    mList[i].mAccessible = nsnull;
}

void RootAccessible::GetNSAccessibleFor(VARIANT varChild, nsCOMPtr<nsIAccessible>& aAcc)
{
  aAcc = nsnull;
  DocAccessible::GetNSAccessibleFor(varChild, aAcc);

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

