/*
 * IENUMFE.H
 *
 * Definitions of a template IDataObject interface implementation.
 *
 * Copyright (c)1993-1994 Microsoft Corporation, All Right Reserved
 *
 * Kraig Brockschmidt, Software Design Engineer
 * Microsoft Systems Developer Relations
 *
 * Internet  :  kraigb@microsoft.com
 * Compuserve:  >INTERNET:kraigb@microsoft.com
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
        ULONG           m_cRef;     //Object reference count
        LPUNKNOWN       m_pUnkRef;  //IUnknown for ref counting
        ULONG           m_iCur;     //Current element
        ULONG           m_cfe;      //Number of FORMATETCs in us
        LPFORMATETC     m_prgfe;    //Source of FORMATETCs
        ULONG           m_maxcfe;   // Max number of us

    public:
        CEnumFormatEtc(LPUNKNOWN, ULONG, LPFORMATETC);
        CEnumFormatEtc(LPUNKNOWN, ULONG aMaxSize);
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
