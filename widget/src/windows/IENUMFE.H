/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#ifndef _IENUMFE_H_
#define _IENUMFE_H_
#include "OLE2.h"
#include "URLMON.h"

/*
 * IEnumFORMATETC object that is created from
 * IDataObject::EnumFormatEtc.  This object lives on its own,
 * that is, QueryInterface only knows IUnknown and IEnumFormatEtc,
 * nothing more.  We still use an outer unknown for reference
 * counting, because as long as this enumerator lives, the data
 * object should live, thereby keeping the application up.
 */

class CEnumFormatEtc;
typedef class CEnumFormatEtc *LPCEnumFormatEtc;

class CEnumFormatEtc : public IEnumFORMATETC
    {
    private:
        ULONG           mRefCnt;      // Object reference count
        ULONG           mCurrentInx;  // Current element
        ULONG           mNumFEs;      // Number of FORMATETCs in us
        LPFORMATETC     mFEList;      // Source of FORMATETCs
        ULONG           mMaxNumFEs;   // Max number of us

    public:
        CEnumFormatEtc(ULONG, LPFORMATETC);
        CEnumFormatEtc(ULONG aMaxSize);
        ~CEnumFormatEtc(void);

        //IUnknown members that delegate to m_pUnkOuter.
        STDMETHODIMP         QueryInterface(REFIID, LPVOID*);
        STDMETHODIMP_(ULONG) AddRef(void);
        STDMETHODIMP_(ULONG) Release(void);

        //IEnumFORMATETC members
        STDMETHODIMP Next(ULONG, LPFORMATETC, ULONG *);
        STDMETHODIMP Skip(ULONG);
        STDMETHODIMP Reset(void);
        STDMETHODIMP Clone(IEnumFORMATETC **);

        // Extra Methods
        void AddFE(LPFORMATETC);
    };


#endif //_IENUMFE_H_
