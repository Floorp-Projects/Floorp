/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"
#include "xptiprivate.h"

#if defined(sparc) || defined(__sparc__)

extern "C" nsresult
PrepareAndDispatch(nsXPTCStubBase* self, PRUint64 methodIndex, PRUint64* args)
{

#define PARAM_BUFFER_COUNT     16

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = NULL;
    const nsXPTMethodInfo* info;
    PRUint8 paramCount;
    PRUint8 i;
    nsresult result = NS_ERROR_FAILURE;

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(PRUint16(methodIndex), &info);
    NS_ASSERTION(info,"no interface info");

    paramCount = info->GetParamCount();

    // setup variant array pointer
    if(paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;
    NS_ASSERTION(dispatchParams,"no place for params");

    PRUint64* ap = args;
    for(i = 0; i < paramCount; i++, ap++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &dispatchParams[i];

        if(param.IsOut() || !type.IsArithmetic())
        {
            dp->val.p = (void*) *ap;
            continue;
        }
        // else
        switch(type)
        {
        case nsXPTType::T_I8     : dp->val.i8  = *((PRInt64*) ap);       break;
        case nsXPTType::T_I16    : dp->val.i16 = *((PRInt64*) ap);       break;
        case nsXPTType::T_I32    : dp->val.i32 = *((PRInt64*) ap);       break;
        case nsXPTType::T_DOUBLE : dp->val.d   = *((double*)  ap);       break;
        case nsXPTType::T_U64    : dp->val.u64 = *((PRUint64*)ap);       break;
        case nsXPTType::T_I64    : dp->val.i64 = *((PRInt64*) ap);       break;
        case nsXPTType::T_U8     : dp->val.u8  = *((PRUint64*)ap);       break;
        case nsXPTType::T_U16    : dp->val.u16 = *((PRUint64*)ap);       break;
        case nsXPTType::T_U32    : dp->val.u32 = *((PRUint64*)ap);       break;
        case nsXPTType::T_FLOAT  : dp->val.f   =  ((float*)   ap)[1];    break;
        case nsXPTType::T_BOOL   : dp->val.b   = *((PRUint64*)ap);       break;
        case nsXPTType::T_CHAR   : dp->val.c   = *((PRUint64*)ap);       break;
        case nsXPTType::T_WCHAR  : dp->val.wc  = *((PRInt64*) ap);       break;
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

extern "C" int SharedStub(int, int*);

#define STUB_ENTRY(n) \
nsresult nsXPTCStubBase::Stub##n() \
{ \
	int dummy; /* defeat tail-call optimization */ \
	return SharedStub(n, &dummy); \
}

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"

#endif /* sparc || __sparc__ */
