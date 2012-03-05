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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Kyle Huey <me@kylehuey.com>
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

#ifndef _NSDATAOBJCOLLECTION_H_
#define _NSDATAOBJCOLLECTION_H_

#include <oleidl.h>

#include "nsString.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsDataObj.h"

class CEnumFormatEtc;

#define MULTI_MIME "Mozilla/IDataObjectCollectionFormat"

EXTERN_C const IID IID_IDataObjCollection;

// An interface to make sure we have the right kind of object for D&D
// this way we can filter out collection objects that aren't ours
class nsIDataObjCollection : public IUnknown {
public:
  
};

/*
 * This ole registered class is used to facilitate drag-drop of objects which
 * can be adapted by an object derived from CfDragDrop. The CfDragDrop is
 * associated with instances via SetDragDrop().
 */
 
class nsDataObjCollection : public nsIDataObjCollection, public nsDataObj
{
  public:
    nsDataObjCollection();
    ~nsDataObjCollection();

  public: // IUnknown methods - see iunknown.h for documentation
    STDMETHODIMP_(ULONG) AddRef        ();
    STDMETHODIMP       QueryInterface(REFIID, void**);
    STDMETHODIMP_(ULONG) Release       ();

  public: // DataGet and DataSet helper methods
    virtual HRESULT AddSetFormat(FORMATETC&  FE);
    virtual HRESULT AddGetFormat(FORMATETC&  FE);

    virtual HRESULT GetFile(LPFORMATETC pFE, LPSTGMEDIUM pSTM);
    virtual HRESULT GetText(LPFORMATETC pFE, LPSTGMEDIUM pSTM);
    virtual HRESULT GetFileDescriptors(LPFORMATETC pFE, LPSTGMEDIUM pSTM);
    virtual HRESULT GetFileContents(LPFORMATETC pFE, LPSTGMEDIUM pSTM);
    virtual HRESULT GetFirstSupporting(LPFORMATETC pFE, LPSTGMEDIUM pSTM);

    // support for clipboard
    void AddDataFlavor(const char * aDataFlavor, LPFORMATETC aFE);

    // from nsPIDataObjCollection
    void AddDataObject(IDataObject * aDataObj);
    PRInt32 GetNumDataObjects() { return mDataObjects.Length(); }
    nsDataObj* GetDataObjectAt(PRUint32 aItem)
            { return mDataObjects.SafeElementAt(aItem, nsRefPtr<nsDataObj>()); }

    // Return the registered OLE class ID of this object's CfDataObj.
    CLSID GetClassID() const;

  public:
    // Store data in pSTM according to the format specified by pFE, if the
    // format is supported (supported formats are specified in CfDragDrop::
    // GetFormats) and return NOERROR; otherwise return DATA_E_FORMATETC. It
    // is the callers responsibility to free pSTM if NOERROR is returned.
    STDMETHODIMP GetData  (LPFORMATETC pFE, LPSTGMEDIUM pSTM);

    // Similar to GetData except that the caller allocates the structure
    // referenced by pSTM.
    STDMETHODIMP GetDataHere (LPFORMATETC pFE, LPSTGMEDIUM pSTM);

    // Returns S_TRUE if this object supports the format specified by pSTM,
    // S_FALSE otherwise.
    STDMETHODIMP QueryGetData (LPFORMATETC pFE);

    // Set pCanonFE to the canonical format of pFE if one exists and return
    // NOERROR, otherwise return DATA_S_SAMEFORMATETC. A canonical format
    // implies an identical rendering.
    STDMETHODIMP GetCanonicalFormatEtc (LPFORMATETC pFE, LPFORMATETC pCanonFE);

    // Set this objects data according to the format specified by pFE and
    // the storage medium specified by pSTM and return NOERROR, if the format
    // is supported. If release is TRUE this object must release the storage
    // associated with pSTM.
    STDMETHODIMP SetData  (LPFORMATETC pFE, LPSTGMEDIUM pSTM, BOOL release);

    // Set ppEnum to an IEnumFORMATETC object which will iterate all of the
    // data formats that this object supports. direction is either DATADIR_GET
    // or DATADIR_SET.
    STDMETHODIMP EnumFormatEtc  (DWORD direction, LPENUMFORMATETC* ppEnum);

    // Set up an advisory connection to this object based on the format specified
    // by pFE, flags, and the pAdvise. Set pConn to the established advise
    // connection.
    STDMETHODIMP DAdvise  (LPFORMATETC pFE, DWORD flags, LPADVISESINK pAdvise,
                   DWORD* pConn);

    // Turn off advising of a previous call to DAdvise which set pConn.
    STDMETHODIMP DUnadvise (DWORD pConn);

    // Set ppEnum to an IEnumSTATDATA object which will iterate over the
    // existing objects which have established advisory connections to this
      // object.
    STDMETHODIMP EnumDAdvise (LPENUMSTATDATA *ppEnum);

  public:
    // Set the adapter to dragDrop 
    //void SetDragDrop(CfDragDrop& dragDrop);

    // Return the adapter
    //CfDragDrop& GetDragDrop() const;

  protected:
    BOOL FormatsMatch(const FORMATETC& source, const FORMATETC& target) const;

    ULONG m_cRef;              // the reference count

    // nsDataObjCollection owns and ref counts CEnumFormatEtc
    CEnumFormatEtc   * m_enumFE;

    nsTArray<nsRefPtr<nsDataObj> > mDataObjects;
    
    BOOL mIsAsyncMode;
    BOOL mIsInOperation;
};

#endif  //
