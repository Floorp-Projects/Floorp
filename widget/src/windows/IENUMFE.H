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
