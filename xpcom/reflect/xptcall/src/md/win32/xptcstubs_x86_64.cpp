/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"
#include "xptiprivate.h"

/*
 * This is for Windows XP 64-Bit Edition / Server 2003 for AMD64 or later.
 */

extern "C" nsresult
PrepareAndDispatch(nsXPTCStubBase* self, PRUint32 methodIndex, PRUint64* args,
                   PRUint64 *gprData, double *fprData)
{
#define PARAM_BUFFER_COUNT  16
//
// "this" pointer is first parameter, so parameter count is 3.
//
#define PARAM_GPR_COUNT   3
#define PARAM_FPR_COUNT   3

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

    //
    // setup variant array pointer
    //

    if(paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;

    NS_ASSERTION(dispatchParams,"no place for params");

    PRUint64* ap = args;
    PRUint32 iCount = 0;

    for(i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &dispatchParams[i];

        if(param.IsOut() || !type.IsArithmetic())
        {
            if (iCount < PARAM_GPR_COUNT)
                dp->val.p = (void*)gprData[iCount++];
            else
                dp->val.p = (void*)*ap++;

            continue;
        }
        // else
        switch(type)
        {
        case nsXPTType::T_I8:
           if (iCount < PARAM_GPR_COUNT)
              dp->val.i8  = (PRInt8)gprData[iCount++];
           else
              dp->val.i8  = *((PRInt8*)ap++);
           break;

        case nsXPTType::T_I16:
            if (iCount < PARAM_GPR_COUNT)
               dp->val.i16  = (PRInt16)gprData[iCount++];
            else
               dp->val.i16  = *((PRInt16*)ap++);
            break;

        case nsXPTType::T_I32:
            if (iCount < PARAM_GPR_COUNT)
               dp->val.i32  = (PRInt32)gprData[iCount++];
            else
               dp->val.i32  = *((PRInt32*)ap++);
            break;

        case nsXPTType::T_I64:
            if (iCount < PARAM_GPR_COUNT)
               dp->val.i64  = (PRInt64)gprData[iCount++];
            else
               dp->val.i64  = *((PRInt64*)ap++);
            break;

        case nsXPTType::T_U8:
            if (iCount < PARAM_GPR_COUNT)
               dp->val.u8  = (PRUint8)gprData[iCount++];
            else
               dp->val.u8  = *((PRUint8*)ap++);
            break;

        case nsXPTType::T_U16:
            if (iCount < PARAM_GPR_COUNT)
               dp->val.u16  = (PRUint16)gprData[iCount++];
            else
                dp->val.u16  = *((PRUint16*)ap++);
            break;

        case nsXPTType::T_U32:
            if (iCount < PARAM_GPR_COUNT)
               dp->val.u32  = (PRUint32)gprData[iCount++];
            else
               dp->val.u32  = *((PRUint32*)ap++);
            break;

        case nsXPTType::T_U64:
            if (iCount < PARAM_GPR_COUNT)
               dp->val.u64  = (PRUint64)gprData[iCount++];
            else
               dp->val.u64  = *((PRUint64*)ap++);
            break;

        case nsXPTType::T_FLOAT:
             if (iCount < PARAM_FPR_COUNT)
                // The value in xmm register is already prepared to
                // be retrieved as a float. Therefore, we pass the
                // value verbatim, as a double without conversion.
                dp->val.d  = (double)fprData[iCount++];
             else
                dp->val.f  = *((float*)ap++);
             break;

        case nsXPTType::T_DOUBLE:
              if (iCount < PARAM_FPR_COUNT)
                 dp->val.d  = (double)fprData[iCount++];
              else
                 dp->val.d  = *((double*)ap++);
              break;

        case nsXPTType::T_BOOL:
           if (iCount < PARAM_GPR_COUNT)
              // We need cast to PRUint8 to remove garbage on upper 56-bit
              // at first.
              dp->val.b  = (bool)(PRUint8)gprData[iCount++];
           else
              dp->val.b  = *((bool*)ap++);
           break;

        case nsXPTType::T_CHAR:
           if (iCount < PARAM_GPR_COUNT)
              dp->val.c  = (char)gprData[iCount++];
           else
              dp->val.c  = *((char*)ap++);
           break;

        case nsXPTType::T_WCHAR:
           if (iCount < PARAM_GPR_COUNT)
              dp->val.wc  = (wchar_t)gprData[iCount++];
           else
              dp->val.wc  = *((wchar_t*)ap++);
           break;

        default:
            NS_ERROR("bad type");
            break;
        }
    }

    result = self->mOuter->CallMethod((PRUint16)methodIndex, info, dispatchParams);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

#define STUB_ENTRY(n)  /* defined in the assembly file */

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"

void
xptc_dummy()
{
}

