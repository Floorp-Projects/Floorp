/*
 * IENUMFE.CPP
 *
 * Standard implementation of a FORMATETC enumerator with the
 * IEnumFORMATETC interface that will generally not need
 * modification.
 *
 * Copyright (c)1993-1994 Microsoft Corporation, All Rights Reserved
 *
 * Kraig Brockschmidt, Software Design Engineer
 * Microsoft Systems Developer Relations
 *
 * Internet  :  kraigb@microsoft.com
 * Compuserve:  >INTERNET:kraigb@microsoft.com
 */


#include "ienumfe.h"


/*
 * CEnumFormatEtc::CEnumFormatEtc
 * CEnumFormatEtc::~CEnumFormatEtc
 *
 * Parameters (Constructor):
 *  pUnkRef         LPUNKNOWN to use for reference counting.
 *  cFE             ULONG number of FORMATETCs in pFE
 *  prgFE           LPFORMATETC to the array to enumerate.
 */

CEnumFormatEtc::CEnumFormatEtc(LPUNKNOWN pUnkRef, ULONG cFE, LPFORMATETC prgFE)
    {
    UINT        i;

    m_cRef=0;
    m_pUnkRef=pUnkRef;

    m_iCur   = 0;
    m_cfe    = cFE;
    m_maxcfe = cFE;
    m_prgfe=new FORMATETC[(UINT)cFE];

    if (NULL!=m_prgfe) {
      for (i=0; i < cFE; i++)
        m_prgfe[i]=prgFE[i];
      }

      return;
    }

/*
 * CEnumFormatEtc::CEnumFormatEtc
 * CEnumFormatEtc::~CEnumFormatEtc
 *
 * Parameters (Constructor):
 *  pUnkRef         LPUNKNOWN to use for reference counting.
 *  cFE             ULONG number of FORMATETCs in pFE
 */

CEnumFormatEtc::CEnumFormatEtc(LPUNKNOWN pUnkRef, ULONG cMaxFE)
{
  //UINT        i;

  m_cRef=0;
  m_pUnkRef=pUnkRef;

  m_iCur   = 0;
  m_cfe    = 0;
  m_maxcfe = cMaxFE;
  m_prgfe=new FORMATETC[(UINT)cMaxFE];
  }


CEnumFormatEtc::~CEnumFormatEtc(void)
{
  if (NULL!=m_prgfe)
    delete [] m_prgfe;

}

void CEnumFormatEtc::AddFE(LPFORMATETC aFE)
{
  m_prgfe[m_cfe++] = *aFE;
}

/*
 * CEnumFormatEtc::QueryInterface
 * CEnumFormatEtc::AddRef
 * CEnumFormatEtc::Release
 *
 * Purpose:
 *  IUnknown members for CEnumFormatEtc object.  For QueryInterface
 *  we only return out own interfaces and not those of the data
 *  object.  However, since enumerating formats only makes sense
 *  when the data object is around, we insure that it stays as
 *  long as we stay by calling an outer IUnknown for AddRef
 *  and Release.  But since we are not controlled by the lifetime
 *  of the outer object, we still keep our own reference count in
 *  order to free ourselves.
 */

STDMETHODIMP CEnumFormatEtc::QueryInterface(REFIID riid, LPVOID *ppv)
{
    *ppv=NULL;

    /*
     * Enumerators are separate objects, not the data object, so
     * we only need to support out IUnknown and IEnumFORMATETC
     * interfaces here with no concern for aggregation.
     */
    if (IsEqualIID(riid, IID_IUnknown)
        || IsEqualIID(riid, IID_IEnumFORMATETC))
        *ppv=(LPVOID)this;

    //AddRef any interface we'll return.
    if (NULL!=*ppv)
        {
        ((LPUNKNOWN)*ppv)->AddRef();
        return NOERROR;
        }

    return ResultFromScode(E_NOINTERFACE);
    }


STDMETHODIMP_(ULONG) CEnumFormatEtc::AddRef(void)
    {
    ++m_cRef;
    m_pUnkRef->AddRef();
    return m_cRef;
    }

STDMETHODIMP_(ULONG) CEnumFormatEtc::Release(void)
    {
    ULONG       cRefT;

    cRefT=--m_cRef;

    m_pUnkRef->Release();

    if (0L==m_cRef)
        delete this;

    return cRefT;
    }







/*
 * CEnumFormatEtc::Next
 *
 * Purpose:
 *  Returns the next element in the enumeration.
 *
 * Parameters:
 *  cFE             ULONG number of FORMATETCs to return.
 *  pFE             LPFORMATETC in which to store the returned
 *                  structures.
 *  pulFE           ULONG * in which to return how many we
 *                  enumerated.
 *
 * Return Value:
 *  HRESULT         NOERROR if successful, S_FALSE otherwise,
 */

STDMETHODIMP CEnumFormatEtc::Next(ULONG cFE, LPFORMATETC pFE, ULONG * pulFE)
    {
    ULONG               cReturn=0L;

    if (NULL==m_prgfe)
        return ResultFromScode(S_FALSE);

    if (NULL==pulFE)
        {
        if (1L!=cFE)
            return ResultFromScode(E_POINTER);
        }
    else
        *pulFE=0L;

    if (NULL==pFE || m_iCur >= m_cfe)
        return ResultFromScode(S_FALSE);

    while (m_iCur < m_cfe && cFE > 0)
        {
        *pFE++=m_prgfe[m_iCur++];
        cReturn++;
        cFE--;
        }

    if (NULL!=pulFE)
        *pulFE=cReturn;

    return NOERROR;
    }







/*
 * CEnumFormatEtc::Skip
 *
 * Purpose:
 *  Skips the next n elements in the enumeration.
 *
 * Parameters:
 *  cSkip           ULONG number of elements to skip.
 *
 * Return Value:
 *  HRESULT         NOERROR if successful, S_FALSE if we could not
 *                  skip the requested number.
 */

STDMETHODIMP CEnumFormatEtc::Skip(ULONG cSkip)
    {
    if (((m_iCur+cSkip) >= m_cfe) || NULL==m_prgfe)
        return ResultFromScode(S_FALSE);

    m_iCur+=cSkip;
    return NOERROR;
    }






/*
 * CEnumFormatEtc::Reset
 *
 * Purpose:
 *  Resets the current element index in the enumeration to zero.
 *
 * Parameters:
 *  None
 *
 * Return Value:
 *  HRESULT         NOERROR
 */

STDMETHODIMP CEnumFormatEtc::Reset(void)
    {
    m_iCur=0;
    return NOERROR;
    }






/*
 * CEnumFormatEtc::Clone
 *
 * Purpose:
 *  Returns another IEnumFORMATETC with the same state as ourselves.
 *
 * Parameters:
 *  ppEnum          LPENUMFORMATETC * in which to return the
 *                  new object.
 *
 * Return Value:
 *  HRESULT         NOERROR or a general error value.
 */

STDMETHODIMP CEnumFormatEtc::Clone(LPENUMFORMATETC *ppEnum)
    {
    LPCEnumFormatEtc    pNew;

    *ppEnum=NULL;

    //Create the clone
    pNew=new CEnumFormatEtc(m_pUnkRef, m_cfe, m_prgfe);

    if (NULL==pNew)
        return ResultFromScode(E_OUTOFMEMORY);

    pNew->AddRef();
    pNew->m_iCur=m_iCur;

    *ppEnum=pNew;
    return NOERROR;
    }
