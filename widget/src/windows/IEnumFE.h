/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jim Mathies <jmathies@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef IEnumeFE_h__
#define IEnumeFE_h__

/*
 * CEnumFormatEtc - implements IEnumFORMATETC
 */

#include <ole2.h>

#include "nsTArray.h"

// FORMATETC container
class FormatEtc
{
public:
  FormatEtc() { memset(&mFormat, 0, sizeof(FORMATETC)); }
  FormatEtc(const FormatEtc& copy) { CopyIn(&copy.mFormat); }
  ~FormatEtc() { if (mFormat.ptd) CoTaskMemFree(mFormat.ptd); }

  void CopyIn(const FORMATETC *aSrc) {
    if (!aSrc) {
        memset(&mFormat, 0, sizeof(FORMATETC));
        return;
    }
    mFormat = *aSrc;
    if (aSrc->ptd) {
        mFormat.ptd = (DVTARGETDEVICE*)CoTaskMemAlloc(sizeof(DVTARGETDEVICE));
        *(mFormat.ptd) = *(aSrc->ptd);
    }
  }

  void CopyOut(LPFORMATETC aDest) {
    if (!aDest)
        return;
    *aDest = mFormat;
    if (mFormat.ptd) {
        aDest->ptd = (DVTARGETDEVICE*)CoTaskMemAlloc(sizeof(DVTARGETDEVICE));
        *(aDest->ptd) = *(mFormat.ptd);
    }
  }

private:
  FORMATETC mFormat;
};

/*
 * CEnumFormatEtc is created within IDataObject::EnumFormatEtc. This object lives
 * on its own, that is, QueryInterface only knows IUnknown and IEnumFormatEtc,
 * nothing more.  We still use an outer unknown for reference counting, because as
 * long as this enumerator lives, the data object should live, thereby keeping the
 * application up.
 */

class CEnumFormatEtc : public IEnumFORMATETC
{
public:
    CEnumFormatEtc(nsTArray<FormatEtc>& aArray);
    CEnumFormatEtc();
    ~CEnumFormatEtc();

    // IUnknown impl.
    STDMETHODIMP QueryInterface(REFIID riid, LPVOID *ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IEnumFORMATETC impl.
    STDMETHODIMP Next(ULONG aMaxToFetch, FORMATETC *aResult, ULONG *aNumFetched);
    STDMETHODIMP Skip(ULONG aSkipNum);
    STDMETHODIMP Reset();
    STDMETHODIMP Clone(LPENUMFORMATETC *aResult); // Addrefs

    // Utils 
    void AddFormatEtc(LPFORMATETC aFormat);

private:
    nsTArray<FormatEtc> mFormatList; // Formats
    ULONG mRefCnt; // Object reference count
    ULONG mCurrentIdx; // Current element

    void SetIndex(PRUint32 aIdx);
};


#endif //_IENUMFE_H_
