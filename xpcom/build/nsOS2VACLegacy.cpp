/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is the Mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * InnoTek Systemberatung GmbH.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    InnoTek Systemberatung GmbH / Knut St. Osmundsen
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

/*
 * This module contains wrappers for a handful of XPCOM methods which someone
 * have been so kind as to link their plugins against. This module will only
 * provide the minimum of what necessary to make legacy plugins work with 
 * the GCC based mozilla. Luckily this only means the IBM oji JAVA plugins.
 *
 * Actually, I haven't seen npoji6 calling any of these yet.
 */
 
/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @group Visual Age for C++ v3.6.5 target (OS/2). */
/* @{ */
/** Indicate Visual Age for C++ v3.6.5 target */
#define VFT_VAC365          1
/** VFTable/Interface Calling Convention for Win32. */
#define VFTCALL             _Optlink
/** First Entry which VAC uses. */
#define VFTFIRST_DECL       unsigned    uFirst[2]
#define VFTFIRST_VAL()      {0, 0},
/** This deltas which VAC uses. */
#define VFTDELTA_DECL(n)    unsigned    uDelta##n
#define VFTDELTA_VAL()      0,
/** @} */
 

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "nscore.h"
#include "nsIServiceManagerUtils.h"

 
/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
#ifndef __cplusplus
typedef struct nsID 
{
  PRUint32  m0;
  PRUint16  m1;
  PRUint16  m2;
  PRUint8   m3[8];
} nsID, nsCID, nsIID;
#define REFNSIID const nsIID *

typedef PRUint32 nsrefcnt;
#endif 


/**
 * nsISupports vftable.
 */
typedef struct vftable_nsISupports
{
    VFTFIRST_DECL;
    nsresult (*VFTCALL QueryInterface)(void *pvThis, REFNSIID aIID, void** aInstancePtr);
    VFTDELTA_DECL(QueryInterface);
    nsrefcnt (*VFTCALL AddRef)(void *pvThis);
    VFTDELTA_DECL(AddRef);
    nsrefcnt (*VFTCALL Release)(void *pvThis);
    VFTDELTA_DECL(Release);
} VFTnsISupports;

/** 
 * nsGetServiceByCID::nsCOMPtr_helper vftable.
 */
typedef struct vftable_nsGetServiceByCID_nsCOMPtr_helper
{
    VFTFIRST_DECL;
    /* virtual nsresult operator()( const nsIID&, void** ) const; */
    nsresult (*VFTCALL __operator_paratheses)(void *pvThis, REFNSIID aIID, void** aInstancePtr);
    VFTDELTA_DECL(__operator_paratheses);
    void *   (*VFTCALL __destructor)(void *pvThis, unsigned __dtorFlags, unsigned __vtt);
    VFTDELTA_DECL(__destructor);
} VFTnsGetServiceByCID_nsCOMPtr_helper;

/**
 * nsQueryInterface::nsCOMPtr_helper vftable.
 */
typedef struct vftable_nsQueryInterface_nsCOMPtr_helper
{
    VFTFIRST_DECL;
    nsresult (*VFTCALL __operator_paratheses)(void *pvThis, REFNSIID aIID, void** aInstancePtr);
    VFTDELTA_DECL(__operator_paratheses);
} VFTnsQueryInterface_nsCOMPtr_helper;




/** 
 * nsISupport object.
 */
typedef struct obj_nsISupports
{
    VFTnsISupports     *pVFT;
} obj_nsISupports;

/** 
 * nsCOMPtr_base object.
 */
typedef struct obj_nsCOMPtr_base
{
    obj_nsISupports    *mRawPtr;
} obj_nsCOMPtr_base;
 
/**
 * nsGetServiceByCID_nsCOMPtr_helper object.
 */
typedef struct obj_nsGetServiceByCID_nsCOMPtr_helper
{
    VFTnsGetServiceByCID_nsCOMPtr_helper   *pVFT;           /* ?? */
    nsID                                   *mCID;           /* const nsCID& */
    void                                   *mServiceManager;/* nsCOMPtr<nsIServiceManager> */
    nsresult                               *mErrorPtr;
} obj_nsGetServiceByCID_nsCOMPtr_helper;

/**
 * nsQueryInterface_nsCOMPtr_helper object.
 */
typedef struct obj_nsQueryInterface_nsCOMPtr_helper
{
    VFTnsQueryInterface_nsCOMPtr_helper    *pVFT;           /* ?? */
    obj_nsISupports                        *mRawPtr;        /* const nsCID& */
    nsresult                               *mErrorPtr;
} obj_nsQueryInterface_nsCOMPtr_helper;




/**
 * nsCOMPtr_base::~nsCOMPtr_base()
 *
 * @remark  This guys doing the oji plugin have been very unfortunate to link in this 
 *          without any similar new operator. The object is thus created in the plugin
 *          but freed by xpcom.dll. As the plugin and mozilla have different CRTs this
 *          is a good way of asking for trouble. But, they guys've been lucky, the VAC
 *          CRT might just handle this ok.
 *          However, we cannot perform this delete as we have no VAC CRT around, and 
 *          hence we will leak this object.
 * ----
 * assembly:
	public __dt__13nsCOMPtr_baseFv
__dt__13nsCOMPtr_baseFv	proc
	push	ebp
	mov	ebp,esp
	sub	esp,08h
	mov	[ebp+08h],eax;	this
	mov	[ebp+0ch],edx;	__dtorFlags

; 63 		if ( mRawPtr )
	mov	eax,[ebp+08h];	this
	cmp	dword ptr [eax],0h
	je	@BLBL4

; 64 			NSCAP_RELEASE(this, mRawPtr);
	mov	ecx,[ebp+08h];	this
	mov	ecx,[ecx]
	mov	ecx,[ecx]
	mov	eax,[ebp+08h];	this
	mov	eax,[eax]
	add	eax,[ecx+01ch]
	mov	ecx,[ebp+08h];	this
	mov	ecx,[ecx]
	mov	ecx,[ecx]
	call	dword ptr [ecx+018h]
@BLBL4:

; 65 	}
	test	byte ptr [ebp+0ch],01h;	__dtorFlags
	je	@BLBL6
	mov	eax,[ebp+08h];	this
	call	__dl__FPv
@BLBL6:
	mov	eax,[ebp+08h];	this
	add	esp,08h
	pop	ebp
	ret	
__dt__13nsCOMPtr_baseFv	endp
*/
extern "C" void * VFTCALL __dt__13nsCOMPtr_baseFv(void *pvThis, unsigned __dtorFlags)
{
    obj_nsCOMPtr_base *pThis = (obj_nsCOMPtr_base*)pvThis;
//asm("int $3");
    if (pThis->mRawPtr)
    {
        /* NSCAP_RELEASE(this, mRawPtr); */
        pThis->mRawPtr->pVFT->Release((char*)pThis->mRawPtr + pThis->mRawPtr->pVFT->uDeltaRelease);
    }

    /* 
     * Delete the object...
     * (As memtioned before we'll rather leak this.)
     */
    #if 0
    if (!(__dtorFlags & 1))
        __dl__FPv(this)
    #endif

    return pvThis;
}

/** workaround for _Optlink bug.. */
extern "C" void * VFTCALL _dt__13nsCOMPtr_baseFv(void *pvThis, unsigned __dtorFlags)
{
    return __dt__13nsCOMPtr_baseFv(pvThis, __dtorFlags);
}



/**
 *
 * -----
 * assembly:
; 92 nsGetServiceByCID::operator()( const nsIID& aIID, void** aInstancePtr ) const
	align 010h

	public __cl__17nsGetServiceByCIDCFRC4nsIDPPv
__cl__17nsGetServiceByCIDCFRC4nsIDPPv	proc
	push	ebp
	mov	ebp,esp
	sub	esp,014h
	push	ebx
	sub	esp,08h
	mov	[ebp+08h],eax;	this
	mov	[ebp+0ch],edx;	aIID
	mov	[ebp+010h],ecx;	aInstancePtr

; 94     nsresult status = NS_ERROR_FAILURE;
	mov	dword ptr [ebp-04h],080004005h;	status

; 95     if ( mServiceManager ) {
	mov	eax,[ebp+08h];	this
	add	eax,08h
	call	__opP13nsDerivedSafeXT17nsIServiceManager___8nsCOMPtrXT17nsIServiceManager_CFv
	test	eax,eax
	je	@BLBL13

; 96         status = mServiceManager->GetService(mCID, aIID, (void**)aInstancePtr);
	mov	eax,[ebp+08h];	this
	add	eax,08h
	call	__rf__8nsCOMPtrXT17nsIServiceManager_CFv
	mov	[ebp-08h],eax;	__212
	mov	eax,[ebp+010h];	aInstancePtr
	push	eax
	mov	ecx,[ebp+0ch];	aIID
	mov	edx,[ebp+08h];	this
	mov	edx,[edx+04h]
	mov	ebx,[ebp-08h];	__212
	mov	ebx,[ebx]
	mov	eax,[ebp-08h];	__212
	add	eax,[ebx+024h]
	sub	esp,0ch
	mov	ebx,[ebp-08h];	__212
	mov	ebx,[ebx]
	call	dword ptr [ebx+020h]
	add	esp,010h
	mov	[ebp-04h],eax;	status

; 97     } else {
	jmp	@BLBL14
	align 010h
@BLBL13:

; 95     if ( mServiceManager ) {

; 98         nsCOMPtr<nsIServiceManager> mgr;
	lea	eax,[ebp-0ch];	mgr
	call	__ct__8nsCOMPtrXT17nsIServiceManager_Fv

; 99         NS_GetServiceManager(getter_AddRefs(mgr));
	lea	edx,[ebp-0ch];	mgr
	lea	eax,[ebp-010h];	__216
	call	getter_AddRefs__FR8nsCOMPtrXT17nsIServiceManager_
	sub	esp,04h
	lea	eax,[ebp-010h];	__216
	sub	esp,04h
	call	__opPP17nsIServiceManager__15nsGetterAddRefsXT17nsIServiceManager_Fv
	add	esp,08h
	call	NS_GetServiceManager
	mov	edx,02h
	lea	eax,[ebp-010h];	__216
	call	__dt__15nsGetterAddRefsXT17nsIServiceManager_Fv

; 100         if (mgr)
	lea	eax,[ebp-0ch];	mgr
	call	__opP13nsDerivedSafeXT17nsIServiceManager___8nsCOMPtrXT17nsIServiceManager_CFv
	test	eax,eax
	je	@BLBL15

; 101             status = mgr->GetService(mCID, aIID, (void**)aInstancePtr);
	lea	eax,[ebp-0ch];	mgr
	call	__rf__8nsCOMPtrXT17nsIServiceManager_CFv
	mov	[ebp-014h],eax;	__217
	mov	eax,[ebp+010h];	aInstancePtr
	push	eax
	mov	ecx,[ebp+0ch];	aIID
	mov	edx,[ebp+08h];	this
	mov	edx,[edx+04h]
	mov	ebx,[ebp-014h];	__217
	mov	ebx,[ebx]
	mov	eax,[ebp-014h];	__217
	add	eax,[ebx+024h]
	sub	esp,0ch
	mov	ebx,[ebp-014h];	__217
	mov	ebx,[ebx]
	call	dword ptr [ebx+020h]
	add	esp,010h
	mov	[ebp-04h],eax;	status
@BLBL15:

; 102     }
	mov	edx,02h
	lea	eax,[ebp-0ch];	mgr
	call	__dt__8nsCOMPtrXT17nsIServiceManager_Fv
@BLBL14:

; 103     if ( NS_FAILED(status) )
	test	byte ptr [ebp-01h],080h;	status
	je	@BLBL16

; 104         *aInstancePtr = 0;
	mov	eax,[ebp+010h];	aInstancePtr
	mov	dword ptr [eax],0h
@BLBL16:

; 106     if ( mErrorPtr )
	mov	eax,[ebp+08h];	this
	cmp	dword ptr [eax+0ch],0h
	je	@BLBL17

; 107         *mErrorPtr = status;
	mov	eax,[ebp+08h];	this
	mov	eax,[eax+0ch]
	mov	ebx,[ebp-04h];	status
	mov	[eax],ebx
@BLBL17:

; 108     return status;
	mov	eax,[ebp-04h];	status
	add	esp,08h
	pop	ebx
	mov	esp,ebp
	pop	ebp
	ret	
__cl__17nsGetServiceByCIDCFRC4nsIDPPv	endp
 
 * -----
 * C++ Code:
nsresult
nsGetServiceByCID::operator()( const nsIID& aIID, void** aInstancePtr ) const
{
    nsresult status = NS_ERROR_FAILURE;
    if ( mServiceManager ) {
        status = mServiceManager->GetService(mCID, aIID, (void**)aInstancePtr);
    } else {
        nsCOMPtr<nsIServiceManager> mgr;
        NS_GetServiceManager(getter_AddRefs(mgr));
        if (mgr)
            status = mgr->GetService(mCID, aIID, (void**)aInstancePtr);
    }
    if ( NS_FAILED(status) )
        *aInstancePtr = 0;
    
    if ( mErrorPtr )
        *mErrorPtr = status;
    return status;
}
 */
extern "C" nsresult VFTCALL GSBC_COM__operator_paratheses(void *pvThis, REFNSIID aIID, void** aInstancePtr)
{
    obj_nsGetServiceByCID_nsCOMPtr_helper *pThis = (obj_nsGetServiceByCID_nsCOMPtr_helper *)pvThis;
    nsresult status = NS_ERROR_FAILURE;
//asm("int $3");
    
    /* For convenience we don't use mServiceManager here because it's a wrapped object. 
     * We ASSUME that there is only one service manager floating around.... 
     */
    nsCOMPtr<nsIServiceManager>     mgr;
    NS_GetServiceManager(getter_AddRefs(mgr));
    if (mgr)
        status = mgr->GetService(*pThis->mCID, aIID, (void**)aInstancePtr);

    if (NS_FAILED(status))
        *aInstancePtr = 0;

    if (pThis->mErrorPtr)
        *pThis->mErrorPtr = status;
    return status;
}

/**
 * Just a destructor.
 * -----
 * assembly:
; 59     virtual ~nsGetServiceByCID() {};
	align 010h

__dt__17nsGetServiceByCIDFv	proc
	push	ebp
	mov	ebp,esp
	sub	esp,08h
	mov	[ebp+08h],eax;	this
	mov	[ebp+0ch],edx;	__dtorFlags
	mov	[ebp+010h],ecx;	__vtt
	mov	eax,[ebp+08h];	this
	mov	dword ptr [eax],offset FLAT:__vft17nsGetServiceByCID15nsCOMPtr_helper
	mov	edx,02h
	mov	eax,[ebp+08h];	this
	add	eax,08h
	call	__dt__8nsCOMPtrXT17nsIServiceManager_Fv
	test	byte ptr [ebp+0ch],01h;	__dtorFlags
	je	@BLBL24
	mov	eax,[ebp+08h];	this
	call	__dl__FPv
@BLBL24:
	mov	eax,[ebp+08h];	this
	add	esp,08h
	pop	ebp
	ret	
__dt__17nsGetServiceByCIDFv	endp
 */
extern "C" void * VFTCALL GSBC_COM__destructor(void *pvThis, unsigned __dtorFlags, unsigned __vtt)
{
    obj_nsGetServiceByCID_nsCOMPtr_helper *pThis = (obj_nsGetServiceByCID_nsCOMPtr_helper *)pvThis;
//asm("int $3");

    /*
     * Because previously mentioned issues with VAC heaps, we'll  
     * not do anything in here.
     * (We will then skip destruction of all parents and such, but 
     *  I don't think that will hurt anyone.)
     */
    __dtorFlags = __dtorFlags;
    __vtt = __vtt;
    return pThis;
}


/** 
 * VFT for nsGetServiceByCID::nsCOMPtr_helper or something like that.
 * It's just implementing an operator() and the destructor.
 *
 * @remark  We need to skip an underscore to get the name right.
 * ----
 * assembly:
__vft17nsGetServiceByCID15nsCOMPtr_helper	dd 0
	db 0h,0h,0h,0h
	dd offset FLAT:__cl__17nsGetServiceByCIDCFRC4nsIDPPv
	db 0h,0h,0h,0h
	dd offset FLAT:__dt__17nsGetServiceByCIDFv
	db 0h,0h,0h,0h
  */
extern const VFTnsGetServiceByCID_nsCOMPtr_helper _vft17nsGetServiceByCID15nsCOMPtr_helper =
{
    VFTFIRST_VAL()
    GSBC_COM__operator_paratheses,                          VFTDELTA_VAL()
    GSBC_COM__destructor,                                   VFTDELTA_VAL()
};



/**
 *
 * -----
 * assembly
; 42 nsQueryInterface::operator()( const nsIID& aIID, void** answer ) const
	align 010h

	public __cl__16nsQueryInterfaceCFRC4nsIDPPv
__cl__16nsQueryInterfaceCFRC4nsIDPPv	proc
	push	ebp
	mov	ebp,esp
	sub	esp,08h
	push	ebx
	sub	esp,0ch
	mov	[ebp+08h],eax;	this
	mov	[ebp+0ch],edx;	aIID
	mov	[ebp+010h],ecx;	answer

; 45 		if ( mRawPtr )
	mov	eax,[ebp+08h];	this
	cmp	dword ptr [eax+04h],0h
	je	@BLBL1

; 46 			{
; 47 				status = mRawPtr->QueryInterface(aIID, answer);
	mov	ecx,[ebp+010h];	answer
	mov	edx,[ebp+0ch];	aIID
	mov	ebx,[ebp+08h];	this
	mov	ebx,[ebx+04h]
	mov	ebx,[ebx]
	mov	eax,[ebp+08h];	this
	mov	eax,[eax+04h]
	add	eax,[ebx+0ch]
	mov	ebx,[ebp+08h];	this
	mov	ebx,[ebx+04h]
	mov	ebx,[ebx]
	call	dword ptr [ebx+08h]
	mov	[ebp-04h],eax;	status

; 48 #ifdef NSCAP_FEATURE_TEST_NONNULL_QUERY_SUCCEEDS
; 49 				NS_WARN_IF_FALSE(NS_SUCCEEDED(status), "interface not found---were you expecting that?");
; 50 #endif
; 51 			}
	jmp	@BLBL2
	align 010h
@BLBL1:

; 45 		if ( mRawPtr )

; 53 			status = NS_ERROR_NULL_POINTER;
	mov	dword ptr [ebp-04h],080004003h;	status
@BLBL2:

; 55 		if ( mErrorPtr )
	mov	eax,[ebp+08h];	this
	cmp	dword ptr [eax+08h],0h
	je	@BLBL3

; 56 			*mErrorPtr = status;
	mov	eax,[ebp+08h];	this
	mov	eax,[eax+08h]
	mov	ebx,[ebp-04h];	status
	mov	[eax],ebx
@BLBL3:

; 57 		return status;
	mov	eax,[ebp-04h];	status
	add	esp,0ch
	pop	ebx
	mov	esp,ebp
	pop	ebp
	ret	
__cl__16nsQueryInterfaceCFRC4nsIDPPv	endp
 
 * -----
 * C++ Code:
nsresult
nsQueryInterface::operator()( const nsIID& aIID, void** answer ) const
	{
		nsresult status;
		if ( mRawPtr )
			{
				status = mRawPtr->QueryInterface(aIID, answer);
#ifdef NSCAP_FEATURE_TEST_NONNULL_QUERY_SUCCEEDS
				NS_WARN_IF_FALSE(NS_SUCCEEDED(status), "interface not found---were you expecting that?");
#endif
			}
		else
			status = NS_ERROR_NULL_POINTER;
		
		if ( mErrorPtr )
			*mErrorPtr = status;
		return status;
	}
 */
extern "C" nsresult VFTCALL QI_COM__operator_paratheses(void *pvThis, REFNSIID aIID, void** aInstancePtr)
{
    obj_nsQueryInterface_nsCOMPtr_helper *pThis = (obj_nsQueryInterface_nsCOMPtr_helper *)pvThis;
    nsresult status = NS_ERROR_NULL_POINTER;
//asm("int $3");

    if (pThis->mRawPtr)
    {
        status = pThis->mRawPtr->pVFT->QueryInterface(pThis->mRawPtr, aIID, aInstancePtr);
        /* don't care about warnings, do we? */
    }
    
    if (pThis->mErrorPtr)
        *pThis->mErrorPtr = status;
    return status;
}



/** 
 * VFT for nsQueryInterface::nsCOMPtr_helper or something like that.
 * No destructor, only an operator().
 * 
 * @remark  We need to skip an underscore to get the name right.
 * -----
 * assembly:
__vft16nsQueryInterface15nsCOMPtr_helper	dd 0
	db 0h,0h,0h,0h
	dd offset FLAT:__cl__16nsQueryInterfaceCFRC4nsIDPPv
	db 0h,0h,0h,0h
 */
extern const VFTnsQueryInterface_nsCOMPtr_helper _vft16nsQueryInterface15nsCOMPtr_helper = 
{
    VFTFIRST_VAL()
    QI_COM__operator_paratheses,                          VFTDELTA_VAL()
};

/**
 * 
 * -----
 * C++ Code:
void
assign_assuming_AddRef( nsISupports* newPtr )
{
    / *
      |AddRef()|ing the new value (before entering this function) before
      |Release()|ing the old lets us safely ignore the self-assignment case.
      We must, however, be careful only to |Release()| _after_ doing the
      assignment, in case the |Release()| leads to our _own_ destruction,
      which would, in turn, cause an incorrect second |Release()| of our old
      pointer.  Thank <waterson@netscape.com> for discovering this.
    * /
  nsISupports* oldPtr = mRawPtr;
  mRawPtr = newPtr;
  NSCAP_LOG_ASSIGNMENT(this, newPtr);
  NSCAP_LOG_RELEASE(this, oldPtr);
  if ( oldPtr )
    NSCAP_RELEASE(this, oldPtr);
}
 */
extern "C" void VFTCALL assign_assuming_AddRef__13nsCOMPtr_baseFP11nsISupports(void *pvThis, obj_nsISupports *newPtr)
{
    obj_nsCOMPtr_base  *pThis = (obj_nsCOMPtr_base *)pvThis;
    obj_nsISupports    *oldPtr;
     
    oldPtr = pThis->mRawPtr;
    pThis->mRawPtr = newPtr;
    if (oldPtr)
    {
        /* NSCAP_RELEASE(this, oldPtr); */
        pThis->mRawPtr->pVFT->Release(oldPtr + oldPtr->pVFT->uDeltaRelease);
    }
}




/**
 *
 * -----
 * Assembly:
; 77 nsCOMPtr_base::assign_from_helper( const nsCOMPtr_helper& helper, const nsIID& iid )
	align 010h

	public assign_from_helper__13nsCOMPtr_baseFRC15nsCOMPtr_helperRC4nsID
assign_from_helper__13nsCOMPtr_baseFRC15nsCOMPtr_helperRC4nsID	proc
	push	ebp
	mov	ebp,esp
	sub	esp,08h
	push	ebx
	sub	esp,0ch
	mov	[ebp+08h],eax;	this
	mov	[ebp+0ch],edx;	helper
	mov	[ebp+010h],ecx;	iid

; 80 		if ( NS_FAILED( helper(iid, NS_REINTERPRET_CAST(void**, &newRawPtr)) ) )
	lea	ecx,[ebp-04h];	newRawPtr
	mov	edx,[ebp+010h];	iid
	mov	ebx,[ebp+0ch];	helper
	mov	ebx,[ebx]
	mov	eax,[ebp+0ch];	helper
	add	eax,[ebx+0ch]
	mov	ebx,[ebp+0ch];	helper
	mov	ebx,[ebx]
	call	dword ptr [ebx+08h]
	test	eax,080000000h
	je	@BLBL8

; 81 			newRawPtr = 0;
	mov	dword ptr [ebp-04h],0h;	newRawPtr
@BLBL8:

; 82     assign_assuming_AddRef(newRawPtr);
	mov	edx,[ebp-04h];	newRawPtr
	mov	eax,[ebp+08h];	this
	call	assign_assuming_AddRef__13nsCOMPtr_baseFP11nsISupports
	add	esp,0ch
	pop	ebx
	mov	esp,ebp
	pop	ebp
	ret	
assign_from_helper__13nsCOMPtr_baseFRC15nsCOMPtr_helperRC4nsID	endp
 
 * -----
 * C Code:
void
nsCOMPtr_base::assign_from_helper( const nsCOMPtr_helper& helper, const nsIID& iid )
	{
		nsISupports* newRawPtr;
		if ( NS_FAILED( helper(iid, NS_REINTERPRET_CAST(void**, &newRawPtr)) ) )
			newRawPtr = 0;
    assign_assuming_AddRef(newRawPtr);
	}
 */
extern "C" void VFTCALL assign_from_helper__13nsCOMPtr_baseFRC15nsCOMPtr_helperRC4nsID(
    void *pvThis, void * helper, REFNSIID iid)
{
    obj_nsCOMPtr_base  *pThis = (obj_nsCOMPtr_base *)pvThis;
    obj_nsISupports*    newRawPtr = NULL;
    nsresult            status = NS_ERROR_FAILURE;
//asm("int $3");

    /* this may or may not be correct but the layout is the same. */
    obj_nsQueryInterface_nsCOMPtr_helper  * pHelper = (obj_nsQueryInterface_nsCOMPtr_helper*)helper;

    /* if ( NS_FAILED( helper(iid, NS_REINTERPRET_CAST(void**, &newRawPtr)) ) ) */
    status = pHelper->pVFT->__operator_paratheses((char*)pHelper + pHelper->pVFT->uDelta__operator_paratheses, 
                                                   iid, (void**)&newRawPtr);
    if (NS_FAILED(status))
        newRawPtr = 0;

    /* assign_assuming_AddRef(newRawPtr); */
    assign_assuming_AddRef__13nsCOMPtr_baseFP11nsISupports(pThis, newRawPtr);
}



