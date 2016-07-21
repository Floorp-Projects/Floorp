/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSDATAOBJCOLLECTION_H_
#define _NSDATAOBJCOLLECTION_H_

#include <oleidl.h>

#include "mozilla/RefPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsDataObj.h"
#include "mozilla/Attributes.h"

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

class nsDataObjCollection final : public nsIDataObjCollection, public nsDataObj
{
  public:
    nsDataObjCollection();
    ~nsDataObjCollection();

  public: // IUnknown methods - see iunknown.h for documentation
    STDMETHODIMP_(ULONG) AddRef        ();
    STDMETHODIMP       QueryInterface(REFIID, void**);
    STDMETHODIMP_(ULONG) Release       ();

  public: // DataGet and DataSet helper methods
    virtual HRESULT GetFile(LPFORMATETC pFE, LPSTGMEDIUM pSTM);
    virtual HRESULT GetText(LPFORMATETC pFE, LPSTGMEDIUM pSTM);
    virtual HRESULT GetFileDescriptors(LPFORMATETC pFE, LPSTGMEDIUM pSTM);
    virtual HRESULT GetFileContents(LPFORMATETC pFE, LPSTGMEDIUM pSTM);
    virtual HRESULT GetFirstSupporting(LPFORMATETC pFE, LPSTGMEDIUM pSTM);

    using nsDataObj::GetFile;
    using nsDataObj::GetFileContents;
    using nsDataObj::GetText;

    // support for clipboard
    void AddDataFlavor(const char * aDataFlavor, LPFORMATETC aFE);

    // from nsPIDataObjCollection
    void AddDataObject(IDataObject * aDataObj);
    int32_t GetNumDataObjects() { return mDataObjects.Length(); }
    nsDataObj* GetDataObjectAt(uint32_t aItem)
            { return mDataObjects.SafeElementAt(aItem, RefPtr<nsDataObj>()); }

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

    // Set this objects data according to the format specified by pFE and
    // the storage medium specified by pSTM and return NOERROR, if the format
    // is supported. If release is TRUE this object must release the storage
    // associated with pSTM.
    STDMETHODIMP SetData  (LPFORMATETC pFE, LPSTGMEDIUM pSTM, BOOL release);

  protected:
    ULONG m_cRef;              // the reference count

    nsTArray<RefPtr<nsDataObj> > mDataObjects;
};

#endif  //
