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

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

#ifndef _Accessible_H_
#define _Accessible_H_

#include "OLEIDL.H"
#include "OLEACC.H"
#include "winable.h"
#ifndef WM_GETOBJECT
#define WM_GETOBJECT 0x03d
#endif

#include "nsCOMPtr.h"
#include "nsIAccessible.h"
#include "nsIAccessibleEventListener.h"
#include "SimpleDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIContent.h"

#include "nsString.h"

typedef LRESULT (STDAPICALLTYPE *LPFNNOTIFYWINEVENT)(DWORD event,HWND hwnd,LONG idObjectType,LONG idObject);

class Accessible : public SimpleDOMNode, public IAccessible
{
  public: // construction, destruction
    Accessible(nsIAccessible*, nsIDOMNode*, HWND aWin = 0);
    virtual ~Accessible();

  public: // IUnknown methods - see iunknown.h for documentation
    STDMETHODIMP_(ULONG) AddRef        ();
    STDMETHODIMP      QueryInterface(REFIID, void**);
    STDMETHODIMP_(ULONG) Release       ();

    // Return the registered OLE class ID of this object's CfDataObj.
    CLSID GetClassID() const;

  public: 

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accParent( 
        /* [retval][out] */ IDispatch __RPC_FAR *__RPC_FAR *ppdispParent);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accChildCount( 
        /* [retval][out] */ long __RPC_FAR *pcountChildren);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accChild( 
        /* [in] */ VARIANT varChild,
        /* [retval][out] */ IDispatch __RPC_FAR *__RPC_FAR *ppdispChild);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accName( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ BSTR __RPC_FAR *pszName);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accValue( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ BSTR __RPC_FAR *pszValue);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accDescription( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ BSTR __RPC_FAR *pszDescription);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accRole( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ VARIANT __RPC_FAR *pvarRole);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accState( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ VARIANT __RPC_FAR *pvarState);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accHelp( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ BSTR __RPC_FAR *pszHelp);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accHelpTopic( 
        /* [out] */ BSTR __RPC_FAR *pszHelpFile,
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ long __RPC_FAR *pidTopic);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ BSTR __RPC_FAR *pszKeyboardShortcut);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accFocus( 
        /* [retval][out] */ VARIANT __RPC_FAR *pvarChild);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accSelection( 
        /* [retval][out] */ VARIANT __RPC_FAR *pvarChildren);

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accDefaultAction( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ BSTR __RPC_FAR *pszDefaultAction);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE accSelect( 
        /* [in] */ long flagsSelect,
        /* [optional][in] */ VARIANT varChild);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE accLocation( 
        /* [out] */ long __RPC_FAR *pxLeft,
        /* [out] */ long __RPC_FAR *pyTop,
        /* [out] */ long __RPC_FAR *pcxWidth,
        /* [out] */ long __RPC_FAR *pcyHeight,
        /* [optional][in] */ VARIANT varChild);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE accNavigate( 
        /* [in] */ long navDir,
        /* [optional][in] */ VARIANT varStart,
        /* [retval][out] */ VARIANT __RPC_FAR *pvarEndUpAt);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE accHitTest( 
        /* [in] */ long xLeft,
        /* [in] */ long yTop,
        /* [retval][out] */ VARIANT __RPC_FAR *pvarChild);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE accDoDefaultAction( 
        /* [optional][in] */ VARIANT varChild);

    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_accName( 
        /* [optional][in] */ VARIANT varChild,
        /* [in] */ BSTR szName);

    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_accValue( 
        /* [optional][in] */ VARIANT varChild,
        /* [in] */ BSTR szValue);

  //   ======  Methods for IDispatch - for VisualBasic bindings (not implemented) ======

  STDMETHODIMP GetTypeInfoCount(UINT *p);
  STDMETHODIMP GetTypeInfo(UINT i, LCID lcid, ITypeInfo **ppti);
  STDMETHODIMP GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames,
                               UINT cNames, LCID lcid, DISPID *rgDispId);
  STDMETHODIMP Invoke(DISPID dispIdMember, REFIID riid,
        LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr);


  // NT4 does not have the oleacc that defines these methods. So we define copies here that automatically
  // load the library only if needed.
  static STDMETHODIMP AccessibleObjectFromWindow(HWND hwnd,DWORD dwObjectID,REFIID riid,void **ppvObject);
  static STDMETHODIMP_(LRESULT) LresultFromObject(REFIID riid,WPARAM wParam,LPUNKNOWN pAcc);
  static STDMETHODIMP NotifyWinEvent(DWORD event,HWND hwnd,LONG idObjectType,LONG idObject);

protected:
  nsCOMPtr<nsIAccessible> mAccessible;

  nsCOMPtr<nsIAccessible> mCachedChild;
  LONG mCachedIndex;

  virtual void GetNSAccessibleFor(VARIANT varChild, nsCOMPtr<nsIAccessible>& aAcc);
  IAccessible *Accessible::NewAccessible(nsIAccessible *aNSAcc, nsIDOMNode *aNode, HWND aWnd);

private:
  /// the accessible library and cached methods
  static HINSTANCE gmAccLib;
  static HINSTANCE gmUserLib;
  static LPFNACCESSIBLEOBJECTFROMWINDOW gmAccessibleObjectFromWindow;
  static LPFNLRESULTFROMOBJECT gmLresultFromObject;
  static LPFNNOTIFYWINEVENT gmNotifyWinEvent;
};

class nsAccessibleEventMap
{
public:
  PRInt32 mId;
  nsCOMPtr<nsIAccessible> mAccessible;
};


class DocAccessible: public Accessible, public ISimpleDOMDocument
{
public:
    DocAccessible(nsIAccessible*, nsIDOMNode *, HWND aWin = 0);
    virtual ~DocAccessible();

    STDMETHODIMP_(ULONG) AddRef        ();
    STDMETHODIMP      QueryInterface(REFIID, void**);
    STDMETHODIMP_(ULONG) Release       ();


    // ISimpleDOMDocument

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_URL( 
        /* [out] */ BSTR __RPC_FAR *url);
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_title( 
        /* [out] */ BSTR __RPC_FAR *title);
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_mimeType( 
        /* [out] */ BSTR __RPC_FAR *mimeType);
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_docType( 
        /* [out] */ BSTR __RPC_FAR *docType);
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_nameSpaceURIForID( 
        /* [in] */ short nameSpaceID,
        /* [out] */ BSTR __RPC_FAR *nameSpaceURI);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE put_alternateViewMediaTypes( 
        /* [in] */ BSTR __RPC_FAR *commaSeparatedMediaTypes);
        
};


#define MAX_LIST_SIZE 100

class RootAccessible: public DocAccessible, public nsIAccessibleEventListener
{
public:
    RootAccessible(nsIAccessible*, HWND aWin = 0);
    virtual ~RootAccessible();

    STDMETHODIMP      QueryInterface(REFIID, void**);
    NS_DECL_ISUPPORTS

    // nsIAccessibleEventListener
    NS_DECL_NSIACCESSIBLEEVENTLISTENER

    virtual PRUint32 GetIdFor(nsIAccessible* aAccessible);
    virtual void GetNSAccessibleFor(VARIANT varChild, nsCOMPtr<nsIAccessible>& aAcc);

private:
    // list of accessible that may have had
    // events fire.
    nsAccessibleEventMap mList[MAX_LIST_SIZE];
    PRInt32 mListCount;
    PRInt32 mNextPos;
};
#endif

