/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "factory.h"
#include "globals.h"
#include "ngprefs.h"

/**
 * CNGLayoutPrefs
 */

class CNGLayoutPrefs: public INGLayoutPrefs {
private:
    int m_ObjRefCount;

public:
    CNGLayoutPrefs();
    ~CNGLayoutPrefs();

    //IUnknown methods
    STDMETHOD (QueryInterface)(REFIID, LPVOID*);
    STDMETHOD_(DWORD, AddRef)();
    STDMETHOD_(DWORD, Release)();

    //INGLayoutPrefs
    STDMETHOD (Show)(HWND hwnd);
};

CNGLayoutPrefs::CNGLayoutPrefs()
{
    m_ObjRefCount = 1;
    g_DllRefCount++;
}

CNGLayoutPrefs::~CNGLayoutPrefs()
{
    g_DllRefCount--;
}

STDMETHODIMP CNGLayoutPrefs::QueryInterface(REFIID riid, LPVOID *ppReturn)
{
    *ppReturn = NULL;
    
    //IUnknown
    if(IsEqualIID(riid, IID_IUnknown))
    {
        *ppReturn = this;
    }
    
    //INGLayoutPrefs
    else if(IsEqualIID(riid, IID_INGLayoutPrefs))
    {
        *ppReturn = (INGLayoutPrefs*)this;
    }   
    
    if(*ppReturn)
    {
        (*(LPUNKNOWN*)ppReturn)->AddRef();
        return S_OK;
    }
    
    return E_NOINTERFACE;
}                                             

STDMETHODIMP_(DWORD) CNGLayoutPrefs::AddRef()
{
    return ++m_ObjRefCount;
}

STDMETHODIMP_(DWORD) CNGLayoutPrefs::Release()
{
    if(--m_ObjRefCount == 0)
    {
        delete this;
        return 0;
    }
   
    return m_ObjRefCount;
}

STDMETHODIMP CNGLayoutPrefs::Show(HWND hwnd)
{
  DisplayPreferences(hwnd);

  return S_OK;
}

/**
 * CNGLayoutPrefs
 */

CClassFactory::CClassFactory(CLSID clsid)
{
    m_clsidObject = clsid;
    m_ObjRefCount = 1;
    g_DllRefCount++;
}

CClassFactory::~CClassFactory()
{
    g_DllRefCount--;
}

STDMETHODIMP CClassFactory::QueryInterface(REFIID riid, LPVOID *ppReturn)
{
    *ppReturn = NULL;

    if(IsEqualIID(riid, IID_IUnknown))
    {
        *ppReturn = this;
    }

    else if(IsEqualIID(riid, IID_IClassFactory))
    {
        *ppReturn = (IClassFactory*)this;
    }   

    if(*ppReturn)
    {
        (*(LPUNKNOWN*)ppReturn)->AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}                                             

STDMETHODIMP_(DWORD) CClassFactory::AddRef()
{
    return ++m_ObjRefCount;
}

STDMETHODIMP_(DWORD) CClassFactory::Release()
{
    if(--m_ObjRefCount == 0)
    {
        delete this;
        return 0;
    }

    return m_ObjRefCount;
}

STDMETHODIMP CClassFactory::CreateInstance(  LPUNKNOWN pUnknown, 
                                             REFIID riid, 
                                             LPVOID *ppObject)
{
    HRESULT  hResult = E_FAIL;
    LPVOID   pTemp = NULL;

    *ppObject = NULL;

    if(pUnknown != NULL)
    return CLASS_E_NOAGGREGATION;

    //create the proper object
    if(IsEqualCLSID(m_clsidObject, CLSID_NGLayoutPrefs))
    {
        CNGLayoutPrefs *pPrefs = new CNGLayoutPrefs();
        if(NULL == pPrefs)
            return E_OUTOFMEMORY;

        pTemp = pPrefs;
    }

    if(pTemp)
    {
        //get the QueryInterface return for our return value
        hResult = ((LPUNKNOWN)pTemp)->QueryInterface(riid, ppObject);

        //call Release to decement the ref count
        ((LPUNKNOWN)pTemp)->Release();
    }

    return hResult;
}

/**************************************************************************

   CClassFactory::LockServer

**************************************************************************/

STDMETHODIMP CClassFactory::LockServer(BOOL)
{
    return E_NOTIMPL;
}

