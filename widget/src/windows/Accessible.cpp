/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "Accessible.h"
#include "nsCOMPtr.h"
#include "nsIAccessible.h"
#include "nsIAccessibleDocument.h"
#include "nsIAccessibleEventReceiver.h"
#include "nsIAccessibleSelectable.h"
#include "nsIAccessibleWin32Object.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIViewManager.h"
#include "nsIDOMDocumentType.h"
#include "nsINameSpaceManager.h"
#include "nsITextContent.h"
#include "nsIWidget.h"
#include "nsIXULDocument.h"
#include "nsReadableUtils.h"
#include "nsWindow.h"
#include "String.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"

// for the COM IEnumVARIANT solution in get_AccSelection()
#define _ATLBASE_IMPL
#include <atlbase.h>
extern CComModule _Module;
#define _ATLCOM_IMPL
#include <atlcom.h>

 /* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

//#define DEBUG_LEAKS

#ifdef DEBUG_LEAKS
static gAccessibles = 0;
#endif

CComModule _Module;

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

PRInt32 RootAccessible::gListCount = 0;
PRInt32 RootAccessible::gNextPos = 0;
nsAccessibleEventMap RootAccessible::gList[MAX_LIST_SIZE];
PRBool Accessible::gIsCacheDisabled = PR_FALSE;
PRBool Accessible::gIsEnumVariantSupportDisabled = PR_FALSE;

// {61044601-A811-4e2b-BBBA-17BFABD329D7}
EXTERN_C GUID CDECL CLSID_Accessible =
{ 0x61044601, 0xa811, 0x4e2b, { 0xbb, 0xba, 0x17, 0xbf, 0xab, 0xd3, 0x29, 0xd7 } };

/*
 * Class Accessible
 */

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
Accessible::Accessible(nsIAccessible* aXPAccessible, nsIDOMNode* aNode, HWND aWnd): 
                       SimpleDOMNode(aXPAccessible, aNode, aWnd), mCachedChildCount(-1), 
                       mCachedFirstChild(nsnull), mCachedNextSibling(nsnull), mEnumVARIANTPosition(0),
                       mXPAccessible(aXPAccessible)
{
  // So we know when mCachedNextSibling is uninitialized vs. null
  mFlagBits.mIsLastSibling = PR_FALSE;

  // Inherited from SimpleDOMNode
  mWnd = aWnd;  // The window handle, for NotifyWinEvent, or getting the accessible parent thru the window parent
  m_cRef = 0;   // for reference counting, so we know when to delete ourselves

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
  mXPAccessible = nsnull;
  if (mCachedNextSibling) {
    mCachedNextSibling->Release();
  }
  if (mCachedFirstChild) {
    mCachedFirstChild->Release();
  }
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
  else if (IID_IEnumVARIANT == iid && !gIsEnumVariantSupportDisabled) {
    CacheMSAAChildren();
    if (mCachedChildCount > 0)  // Don't support this interface for leaf elements
      *ppv = NS_STATIC_CAST(IEnumVARIANT*, this);
  }

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


STDMETHODIMP Accessible::AccessibleObjectFromWindow(HWND hwnd,
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

STDMETHODIMP Accessible::NotifyWinEvent(DWORD event,
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

STDMETHODIMP_(LRESULT) Accessible::LresultFromObject(REFIID riid,
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
  nsCOMPtr<nsIAccessible> xpParentAccessible;
  mXPAccessible->GetAccParent(getter_AddRefs(xpParentAccessible));

  if (xpParentAccessible) {
    IAccessible* msaaParentAccessible = 
      NewAccessible(xpParentAccessible, nsnull, mWnd);
    msaaParentAccessible->AddRef();
    *ppdispParent = msaaParentAccessible;
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
      IAccessible* msaaParentAccessible = (IAccessible*)ptr;
      // got one? return it.
      if (msaaParentAccessible) {
        *ppdispParent = msaaParentAccessible;
        return NS_OK;
      }
    }
  }

  *ppdispParent = NULL;
  return E_FAIL;  
}

STDMETHODIMP Accessible::get_accChildCount( long __RPC_FAR *pcountChildren)
{
  CacheMSAAChildren();
  
  *pcountChildren = mCachedChildCount;

  return S_OK;
}

STDMETHODIMP Accessible::get_accChild( 
      /* [in] */ VARIANT varChild,
      /* [retval][out] */ IDispatch __RPC_FAR *__RPC_FAR *ppdispChild)
{
  *ppdispChild = NULL;

  if (varChild.vt != VT_I4)
    return E_FAIL;

  if (varChild.lVal == CHILDID_SELF) {
    *ppdispChild = this;
    AddRef();
    return S_OK;
  }

  CacheMSAAChildren();

  *ppdispChild = GetCachedChild(varChild.lVal - 1); // already addrefed

  return (*ppdispChild)? S_OK: E_FAIL;
}

STDMETHODIMP Accessible::get_accName( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszName)
{
  *pszName = NULL;
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetNSAccessibleFor(varChild, xpAccessible);
  if (xpAccessible) {
    nsAutoString name;
    if (NS_FAILED(xpAccessible->GetAccName(name)))
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
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetNSAccessibleFor(varChild, xpAccessible);
  if (xpAccessible) {
    nsAutoString value;
    if (NS_FAILED(xpAccessible->GetAccValue(value)))
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
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetNSAccessibleFor(varChild, xpAccessible);
  if (xpAccessible) {
     nsAutoString description;
     if (NS_FAILED(xpAccessible->GetAccDescription(description)))
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

  nsCOMPtr<nsIAccessible> xpAccessible;
  GetNSAccessibleFor(varChild, xpAccessible);

  if (!xpAccessible)
    return E_FAIL;

  PRUint32 role = 0;
  if (NS_FAILED(xpAccessible->GetAccRole(&role)))
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

  nsCOMPtr<nsIAccessible> xpAccessible;
  GetNSAccessibleFor(varChild, xpAccessible);
  if (!xpAccessible)
    return E_FAIL;

  PRUint32 state;
  if (NS_FAILED(xpAccessible->GetAccState(&state)))
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
  return E_NOTIMPL;
}

STDMETHODIMP Accessible::get_accKeyboardShortcut( 
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR *pszKeyboardShortcut)
{
  *pszKeyboardShortcut = NULL;
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetNSAccessibleFor(varChild, xpAccessible);
  if (xpAccessible) {
    nsAutoString shortcut;
    nsresult rv = xpAccessible->GetAccKeyboardShortcut(shortcut);
    if (NS_FAILED(rv))
      return S_FALSE;

    *pszKeyboardShortcut = ::SysAllocString(shortcut.get());
    return S_OK;
  }
  return S_FALSE;
}

STDMETHODIMP Accessible::get_accFocus( 
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChild)
{
  // Return the current nsIAccessible that has focus
  VariantInit(pvarChild);

  nsCOMPtr<nsIAccessible> focusedAccessible;
  if (NS_SUCCEEDED(mXPAccessible->GetAccFocused(getter_AddRefs(focusedAccessible)))) {
    pvarChild->vt = VT_DISPATCH;
    pvarChild->pdispVal = NS_STATIC_CAST(IDispatch*, NewAccessible(focusedAccessible, nsnull, mWnd));
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
  
  nsCOMPtr<nsIAccessibleSelectable> select(do_QueryInterface(mXPAccessible));
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
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetNSAccessibleFor(varChild, xpAccessible);
  if (xpAccessible) {
    nsAutoString defaultAction;
    if (NS_FAILED(xpAccessible->GetAccActionName(0, defaultAction)))
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
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetNSAccessibleFor(varChild, xpAccessible);

  if (flagsSelect & (SELFLAG_TAKEFOCUS|SELFLAG_TAKESELECTION|SELFLAG_REMOVESELECTION))
  {
    if (flagsSelect & SELFLAG_TAKEFOCUS)
      xpAccessible->AccTakeFocus();

    if (flagsSelect & SELFLAG_TAKESELECTION)
      xpAccessible->AccTakeSelection();

    if (flagsSelect & SELFLAG_REMOVESELECTION)
      xpAccessible->AccRemoveSelection();

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
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetNSAccessibleFor(varChild, xpAccessible);

  if (xpAccessible) {
    PRInt32 x, y, width, height;
    if (NS_FAILED(xpAccessible->AccGetBounds(&x, &y, &width, &height)))
      return E_FAIL;

    *pxLeft = x;
    *pyTop = y;
    *pcxWidth = width;
    *pcyHeight = height;
    return S_OK;
  }

  return E_FAIL;  
}

IAccessible *
Accessible::GetCachedChild(long aChildNum)
{
  // aChildNum of -1 just counts up the children
  // and stores the value in mCachedChildCount
  VARIANT varStart, varResult;
  VariantInit(&varStart);
  varStart.lVal = CHILDID_SELF;
  varStart.vt = VT_I4;

  accNavigate(NAVDIR_FIRSTCHILD, varStart, &varResult);
  for (long index = 0; varResult.vt == VT_DISPATCH; ++index) {
    IAccessible *msaaAccessible = NS_STATIC_CAST(IAccessible*, varResult.pdispVal);
    if (aChildNum == index)
      return msaaAccessible;
    msaaAccessible->accNavigate(NAVDIR_NEXT, varStart, &varResult);
    msaaAccessible->Release();
  }
  if (mCachedChildCount < 0)
    mCachedChildCount = index;
  return nsnull;
}

void Accessible::CacheMSAAChildren()
{
  if (gIsCacheDisabled)
    mCachedChildCount = -1;  // Don't use old info
  if (mCachedChildCount < 0)
    GetCachedChild(-1); // This caches the children and sets mCachedChildCount
}

STDMETHODIMP Accessible::accNavigate( 
      /* [in] */ long navDir,
      /* [optional][in] */ VARIANT varStart,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarEndUpAt)
{
  nsCOMPtr<nsIAccessible> xpAccessibleStart, xpAccessibleResult;
  GetNSAccessibleFor(varStart, xpAccessibleStart);
  PRBool isNavigatingFromSelf = (xpAccessibleStart == mXPAccessible);
  VariantInit(pvarEndUpAt);

  IAccessible* msaaAccessible = nsnull;

  switch(navDir) {
    case NAVDIR_DOWN: 
      xpAccessibleStart->AccNavigateDown(getter_AddRefs(xpAccessibleResult));
      break;
    case NAVDIR_FIRSTCHILD:
      if (mCachedChildCount == -1 || xpAccessibleStart != mXPAccessible ||
          gIsCacheDisabled) {
        xpAccessibleStart->GetAccFirstChild(getter_AddRefs(xpAccessibleResult));
      }
      else if (mCachedFirstChild) {
        msaaAccessible = mCachedFirstChild;
      }
      break;
    case NAVDIR_LASTCHILD:
      xpAccessibleStart->GetAccLastChild(getter_AddRefs(xpAccessibleResult));
      break;
    case NAVDIR_LEFT:
      xpAccessibleStart->AccNavigateLeft(getter_AddRefs(xpAccessibleResult));
      break;
    case NAVDIR_NEXT:
      if (isNavigatingFromSelf && mCachedNextSibling && !gIsCacheDisabled) {
        msaaAccessible = mCachedNextSibling;
      }
      else if (!mFlagBits.mIsLastSibling) {
        xpAccessibleStart->GetAccNextSibling(getter_AddRefs(xpAccessibleResult));
        if (!xpAccessibleResult && isNavigatingFromSelf && !gIsCacheDisabled)
          mFlagBits.mIsLastSibling = PR_TRUE;
      }
      break;
    case NAVDIR_PREVIOUS:
      xpAccessibleStart->GetAccPreviousSibling(getter_AddRefs(xpAccessibleResult));
      break;
    case NAVDIR_RIGHT:
      xpAccessibleStart->AccNavigateRight(getter_AddRefs(xpAccessibleResult));
      break;
    case NAVDIR_UP:
      xpAccessibleStart->AccNavigateUp(getter_AddRefs(xpAccessibleResult));
      break;
  }

  if (xpAccessibleResult) {
    msaaAccessible = NewAccessible(xpAccessibleResult, nsnull, mWnd);
    if (isNavigatingFromSelf) {
      if (navDir == NAVDIR_NEXT) {
        if (mCachedNextSibling)
          mCachedNextSibling->Release();
        if (!gIsCacheDisabled) {
          mCachedNextSibling = msaaAccessible;
          msaaAccessible->AddRef(); // addref for the caching
        }
      }
      if (navDir == NAVDIR_FIRSTCHILD) {
        if (mCachedFirstChild)
          mCachedFirstChild->Release();
        if (!gIsCacheDisabled) {
          mCachedFirstChild = msaaAccessible;
          msaaAccessible->AddRef(); // addref for the caching
        }
      }
    }
  } 
  else if (!msaaAccessible) {
    if (isNavigatingFromSelf) {
      if (navDir == NAVDIR_NEXT) {
        if (mCachedNextSibling)
          mCachedNextSibling->Release();
        mCachedNextSibling = nsnull;
      }
      if (navDir == NAVDIR_FIRSTCHILD) {
        if (mCachedFirstChild)
          mCachedFirstChild->Release();
        mCachedFirstChild = nsnull;
        mCachedChildCount = 0;
      }
    }
    pvarEndUpAt->vt = VT_EMPTY;
    return E_FAIL;
  }

  pvarEndUpAt->pdispVal = NS_STATIC_CAST(IDispatch*, msaaAccessible);
  msaaAccessible->AddRef(); //  addref for the getter
  pvarEndUpAt->vt = VT_DISPATCH;
  return NS_OK;
}

STDMETHODIMP Accessible::accHitTest( 
      /* [in] */ long xLeft,
      /* [in] */ long yTop,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChild)
{
  VariantInit(pvarChild);

  // convert to window coords
  nsCOMPtr<nsIAccessible> xpAccessible;

  xLeft = xLeft;
  yTop = yTop;

  mXPAccessible->AccGetAt(xLeft, yTop, getter_AddRefs(xpAccessible));

  // if we got a child
  if (xpAccessible) {
    // if the child is us
    if (xpAccessible == mXPAccessible) {
      pvarChild->vt = VT_I4;
      pvarChild->lVal = CHILDID_SELF;
    } else { // its not create an Accessible for it.
      pvarChild->vt = VT_DISPATCH;
      pvarChild->pdispVal = NS_STATIC_CAST(IDispatch*, NewAccessible(xpAccessible, nsnull, mWnd));
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
  nsCOMPtr<nsIAccessible> xpAccessible;
  GetNSAccessibleFor(varChild, xpAccessible);

  if (!xpAccessible || FAILED(xpAccessible->AccDoAction(0))) {
    return E_FAIL;
  }
  return S_OK;
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


STDMETHODIMP
Accessible::Next(ULONG aNumElementsRequested, VARIANT FAR* pvar, ULONG FAR* aNumElementsFetched)
{
  CacheMSAAChildren();
  *aNumElementsFetched = 0;

  if (aNumElementsRequested <= 0 || !pvar ||
      mEnumVARIANTPosition >= mCachedChildCount) {
    return E_FAIL;
  }

  VARIANT varStart;
  VariantInit(&varStart);
  varStart.lVal = CHILDID_SELF;
  varStart.vt = VT_I4;

  accNavigate(NAVDIR_FIRSTCHILD, varStart, &pvar[0]);

  for (long childIndex = 0; pvar[*aNumElementsFetched].vt == VT_DISPATCH; ++childIndex) {
    PRBool wasAccessibleFetched = PR_FALSE;
    Accessible *msaaAccessible = NS_STATIC_CAST(Accessible*, pvar[*aNumElementsFetched].pdispVal);
    if (childIndex >= mEnumVARIANTPosition) {
      if (++*aNumElementsFetched >= aNumElementsRequested)
        break;
      wasAccessibleFetched = PR_TRUE;
    }
    msaaAccessible->accNavigate(NAVDIR_NEXT, varStart, &pvar[*aNumElementsFetched] );
    if (!wasAccessibleFetched)
      msaaAccessible->Release(); // this accessible will not be received by the caller
  }

  mEnumVARIANTPosition += *aNumElementsFetched;
  return NOERROR;
}

STDMETHODIMP
Accessible::Skip(ULONG aNumElements)
{
  CacheMSAAChildren();

  mEnumVARIANTPosition += aNumElements;

  if (mEnumVARIANTPosition > mCachedChildCount)
  {
    mEnumVARIANTPosition = mCachedChildCount;
    return S_FALSE;
  }
  return NOERROR;
}

STDMETHODIMP 
Accessible::Reset(void)
{
  mEnumVARIANTPosition = 0;
  return NOERROR;
}

STDMETHODIMP
Accessible::Clone(IEnumVARIANT FAR* FAR* ppenum)
{
  *ppenum = nsnull;
  CacheMSAAChildren();

  IAccessible *msaaAccessible = new Accessible(mXPAccessible, mDOMNode, mWnd);
  if (!msaaAccessible)
    return E_FAIL;
  msaaAccessible->AddRef();
  QueryInterface(IID_IEnumVARIANT, (void**)ppenum);
  if (*ppenum)
    (*ppenum)->Skip(mEnumVARIANTPosition); // QI addrefed
  msaaAccessible->Release();

  return NOERROR;
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
    nsCOMPtr<nsIAccessibleWin32Object> accObject(do_QueryInterface(aNSAcc));
    if (accObject) {
      PRInt32 iHwnd;
      accObject->GetHwnd(&iHwnd);
      if (iHwnd) {
        AccessibleObjectFromWindow(NS_REINTERPRET_CAST(HWND, iHwnd), OBJID_CLIENT, IID_IAccessible, (void **) &retval);
        return retval;
      }
    }
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


void Accessible::GetNSAccessibleFor(VARIANT varChild, nsCOMPtr<nsIAccessible>& aXPAccessible)
{
  aXPAccessible = nsnull;

  // if its us real easy - this seems to always be the case
  if (varChild.lVal == CHILDID_SELF) {
    aXPAccessible = mXPAccessible;
  } 
  else {
    // XXX aaronl
    // I can't find anything that uses VARIANT with a child num
    // so let's not worry about optimizing this.
    NS_NOTREACHED("MSAA VARIANT with child number being used");
    nsCOMPtr<nsIAccessible> xpAccessible;
    mXPAccessible->GetAccFirstChild(getter_AddRefs(xpAccessible));
    for (PRInt32 index = 0; xpAccessible; index ++) {
      aXPAccessible = xpAccessible;
      if (varChild.lVal == index)
        break;
      aXPAccessible->GetAccNextSibling(getter_AddRefs(xpAccessible));
    }
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
  nsCOMPtr<nsIAccessibleDocument> accDoc(do_QueryInterface(mXPAccessible));
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
  nsCOMPtr<nsIAccessibleDocument> accDoc(do_QueryInterface(mXPAccessible));
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
  nsCOMPtr<nsIAccessibleDocument> accDoc(do_QueryInterface(mXPAccessible));
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
  nsCOMPtr<nsIAccessibleDocument> accDoc(do_QueryInterface(mXPAccessible));
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
  nsCOMPtr<nsIAccessibleDocument> accDoc(do_QueryInterface(mXPAccessible));
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
  nsCOMPtr<nsIAccessibleEventReceiver> r(do_QueryInterface(mXPAccessible));
  if (r)
    r->AddAccessibleEventListener(this);

  static PRBool prefsInitialized;

  if (!prefsInitialized) {
    nsCOMPtr<nsIPref> prefService(do_GetService(kPrefCID));
    if (prefService) {
      prefService->GetBoolPref("accessibility.disablecache", &Accessible::gIsCacheDisabled);
      prefService->GetBoolPref("accessibility.disableenumvariant", &Accessible::gIsEnumVariantSupportDisabled);
    }
    prefsInitialized = PR_TRUE;
  }
}

RootAccessible::~RootAccessible()
{
  nsCOMPtr<nsIAccessibleEventReceiver> r(do_QueryInterface(mXPAccessible));
  if (r) 
    r->RemoveAccessibleEventListener();

  // free up accessibles
  for (int i=0; i < gListCount; i++)
    gList[i].mAccessible = nsnull;
}

void RootAccessible::GetNSAccessibleFor(VARIANT varChild, nsCOMPtr<nsIAccessible>& aAcc)
{
  // Check our circular array of event information, because the child ID
  // asked for corresponds to an event target. See RootAccessible::HandleEvent to see how we provide this unique ID.

  aAcc = nsnull;
  if (varChild.lVal == UNIQUE_ID_CARET) {
    nsCOMPtr<nsIAccessibleDocument> accDoc(do_QueryInterface(mXPAccessible));
    if (accDoc) { 
      nsCOMPtr<nsIAccessibleCaret> accessibleCaret;
      accDoc->GetCaretAccessible(getter_AddRefs(accessibleCaret));
      aAcc = do_QueryInterface(accessibleCaret);
    }
    return;
  }

  if (varChild.lVal < 0) {
    for (int i=0; i < gListCount; i++) {
      if (varChild.lVal == gList[i].mId) {
        aAcc = gList[i].mAccessible;
        return;
      }
    }
  }

  Accessible::GetNSAccessibleFor(varChild, aAcc);
}

STDMETHODIMP RootAccessible::get_accChild( 
      /* [in] */ VARIANT varChild,
      /* [retval][out] */ IDispatch __RPC_FAR *__RPC_FAR *ppdispChild)
{
  *ppdispChild = NULL;

  if (varChild.vt == VT_I4 && varChild.lVal < 0) {
    // AccessibleObjectFromEvent() being called
    // that's why the lVal < 0
    nsCOMPtr<nsIAccessible> xpAccessible;
    GetNSAccessibleFor(varChild, xpAccessible);
    if (xpAccessible) {
      IAccessible* msaaAccessible = NewAccessible(xpAccessible, nsnull, mWnd);
      if (msaaAccessible) {
        msaaAccessible->AddRef();
        *ppdispChild = msaaAccessible;
        return S_OK;
      }
    }
    return E_FAIL;
  }

  // Otherwise, the normal get_accChild() will do
  return Accessible::get_accChild(varChild, ppdispChild);
}
  
NS_IMETHODIMP RootAccessible::HandleEvent(PRUint32 aEvent, nsIAccessible* aAccessible, AccessibleEventData* aData)
{
#ifdef SWALLOW_DOC_FOCUS_EVENTS
  // Remove this until we can figure out which focus events are coming at
  // the same time as native window focus events, although
  // perhaps 2 duplicate focus events on the window isn't really a problem
  if (aEvent == EVENT_FOCUS) {
    // Don't fire accessible focus event for documents, 
    // Microsoft Windows will generate those from native window focus events
    nsCOMPtr<nsIAccessibleDocument> accessibleDoc(do_QueryInterface(aAccessible));
    if (accessibleDoc)
      return NS_OK;
  }
#endif

  PRInt32 childID, worldID = OBJID_CLIENT;
  PRUint32 role = ROLE_SYSTEM_TEXT; // Default value

  if (NS_SUCCEEDED(aAccessible->GetAccRole(&role)) && role == ROLE_SYSTEM_CARET) {
    childID = CHILDID_SELF;
    worldID = OBJID_CARET;
  }
  else 
    childID = GetIdFor(aAccessible); // get the id for the accessible

  if (role == ROLE_SYSTEM_PANE && aEvent == EVENT_STATE_CHANGE) {
    // Something on the document has changed
    // Clear out the cache in this subtree
    if (mCachedFirstChild) {
      mCachedFirstChild->Release();
      mCachedFirstChild = nsnull;
    }
    mCachedChildCount = -1;
  }

  HWND hWnd = mWnd;
  if (aEvent == EVENT_FOCUS) {
    GUITHREADINFO guiInfo;
    if (!::GetGUIThreadInfo(NULL, &guiInfo))
      return NS_OK;
    hWnd = guiInfo.hwndFocus;
  }

  // notify the window system
  NotifyWinEvent(aEvent, hWnd, worldID, childID);
  
  return NS_OK;
}

PRUint32 RootAccessible::GetIdFor(nsIAccessible* aAccessible)
{
  // A child ID of the window is required, when we use NotifyWinEvent, so that the 3rd party application
  // can call back and get the IAccessible the event occured on.
  // We use the unique ID exposed through nsIContent::GetContentID()

  PRInt32 uniqueID;
  aAccessible->GetAccId(&uniqueID);

  for (PRInt32 index = 0; index < gListCount; index++)
    if (uniqueID == gList[index].mId) {
      // Change old ID in list to use most recent accessible,
      // rather than create multiple entries for same accessible
      gList[index].mAccessible = aAccessible;
      return uniqueID;
    }

  gList[gNextPos].mId = uniqueID;
  gList[gNextPos].mAccessible = aAccessible;

  if (++gNextPos >= MAX_LIST_SIZE)
    gNextPos = 0;
  if (gListCount < MAX_LIST_SIZE)
    gListCount++;

  return uniqueID;
}

