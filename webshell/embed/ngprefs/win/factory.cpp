/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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

