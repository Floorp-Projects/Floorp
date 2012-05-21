/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"
#include "xptiprivate.h"

#if defined(AIX)

/*
        For PPC (AIX & MAC), the first 8 integral and the first 13 f.p. parameters 
        arrive in a separate chunk of data that has been loaded from the registers. 
        The args pointer has been set to the start of the parameters BEYOND the ones
        arriving in registers
*/
extern "C" nsresult
PrepareAndDispatch(nsXPTCStubBase* self, PRUint32 methodIndex, PRUint32* args, PRUint32 *gprData, double *fprData)
{
    typedef struct {
        uint32 hi;
        uint32 lo;      // have to move 64 bit entities as 32 bit halves since
    } DU;               // stack slots are not guaranteed 16 byte aligned

#define PARAM_BUFFER_COUNT     16
#define PARAM_GPR_COUNT         7  

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = NULL;
    const nsXPTMethodInfo* info = NULL;
    PRUint8 paramCount;
    PRUint8 i;
    nsresult result = NS_ERROR_FAILURE;

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(PRUint16(methodIndex), &info);
    NS_ASSERTION(info,"no method info");

    paramCount = info->GetParamCount();

    // setup variant array pointer
    if(paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;
    NS_ASSERTION(dispatchParams,"no place for params");

    PRUint32* ap = args;
    PRUint32 iCount = 0;
    PRUint32 fpCount = 0;
    for(i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &dispatchParams[i];

        if(param.IsOut() || !type.IsArithmetic())
        {
            if (iCount < PARAM_GPR_COUNT)
                dp->val.p = (void*) gprData[iCount++];
            else
                dp->val.p = (void*) *ap++;
            continue;
        }
        // else
        switch(type)
        {
        case nsXPTType::T_I8      :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.i8  = (PRInt8) gprData[iCount++];
                                     else
                                         dp->val.i8  = (PRInt8)  *ap++;
                                     break;
        case nsXPTType::T_I16     :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.i16  = (PRInt16) gprData[iCount++];
                                     else
                                         dp->val.i16  = (PRInt16)  *ap++;
                                     break;
        case nsXPTType::T_I32     :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.i32  = (PRInt32) gprData[iCount++];
                                     else
                                         dp->val.i32  = (PRInt32)  *ap++;
                                     break;
        case nsXPTType::T_I64     :  if (iCount < PARAM_GPR_COUNT)
                                         ((DU *)dp)->hi  = (PRInt32) gprData[iCount++];
                                     else
                                         ((DU *)dp)->hi  = (PRInt32)  *ap++;
                                     if (iCount < PARAM_GPR_COUNT)
                                         ((DU *)dp)->lo  = (PRUint32) gprData[iCount++];
                                     else
                                         ((DU *)dp)->lo  = (PRUint32)  *ap++;
                                     break;
        case nsXPTType::T_U8      :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.u8  = (PRUint8) gprData[iCount++];
                                     else
                                         dp->val.u8  = (PRUint8)  *ap++;
                                     break;
        case nsXPTType::T_U16     :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.u16  = (PRUint16) gprData[iCount++];
                                     else
                                         dp->val.u16  = (PRUint16)  *ap++;
                                     break;
        case nsXPTType::T_U32     :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.u32  = (PRUint32) gprData[iCount++];
                                     else
                                         dp->val.u32  = (PRUint32)  *ap++;
                                     break;
        case nsXPTType::T_U64     :  if (iCount < PARAM_GPR_COUNT)
                                         ((DU *)dp)->hi  = (PRUint32) gprData[iCount++];
                                     else
                                         ((DU *)dp)->hi  = (PRUint32)  *ap++;
                                     if (iCount < PARAM_GPR_COUNT)
                                         ((DU *)dp)->lo  = (PRUint32) gprData[iCount++];
                                     else
                                         ((DU *)dp)->lo  = (PRUint32)  *ap++;
                                     break;
        case nsXPTType::T_FLOAT   :  if (fpCount < 13) {
                                         dp->val.f  = (float) fprData[fpCount++];
                                         if (iCount < PARAM_GPR_COUNT)
                                             ++iCount;
                                         else
                                             ++ap;
                                     }
                                     else
                                         dp->val.f   = *((float*)   ap++);
                                     break;
        case nsXPTType::T_DOUBLE  :  if (fpCount < 13) {
                                         dp->val.d  = (double) fprData[fpCount++];
                                         if (iCount < PARAM_GPR_COUNT)
                                             ++iCount;
                                         else
                                             ++ap;
                                         if (iCount < PARAM_GPR_COUNT)
                                             ++iCount;
                                         else
                                             ++ap;
                                     }
                                     else {
                                         dp->val.f   = *((double*)   ap);
                                         ap += 2;
                                     }
                                     break;
        case nsXPTType::T_BOOL    :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.b  = (bool) gprData[iCount++];
                                     else
                                         dp->val.b  = (bool)  *ap++;
                                     break;
        case nsXPTType::T_CHAR    :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.c  = (char) gprData[iCount++];
                                     else
                                         dp->val.c  = (char)  *ap++;
                                     break;
        case nsXPTType::T_WCHAR   :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.wc  = (wchar_t) gprData[iCount++];
                                     else
                                         dp->val.wc  = (wchar_t)  *ap++;
                                     break;
        default:
            NS_ERROR("bad type");
            break;
        }
    }

    result = self->mOuter->CallMethod((PRUint16)methodIndex,info,dispatchParams);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

#define STUB_ENTRY(n)

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"

#endif /* AIX */
