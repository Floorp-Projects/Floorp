/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"

#if defined(sparc) || defined(__sparc__)

extern "C" nsresult ATTRIBUTE_USED
PrepareAndDispatch(nsXPTCStubBase* self, uint64_t methodIndex, uint64_t* args)
{


    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    const nsXPTMethodInfo* info;
    uint8_t paramCount;
    uint8_t i;

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(uint16_t(methodIndex), &info);
    NS_ASSERTION(info,"no interface info");

    paramCount = info->GetParamCount();

    const uint8_t indexOfJSContext = info->IndexOfJSContext();

    uint64_t* ap = args;
    for(i = 0; i < paramCount; i++, ap++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &paramBuffer[i];

        if (i == indexOfJSContext)
            ap++;

        if(param.IsOut() || !type.IsArithmetic())
        {
            dp->val.p = (void*) *ap;
            continue;
        }
        // else
        switch(type)
        {
        case nsXPTType::T_BOOL   : dp->val.b   = *((int64_t*)  ap);      break;
        case nsXPTType::T_CHAR   : dp->val.c   = *((uint64_t*) ap);      break;
        case nsXPTType::T_WCHAR  : dp->val.wc  = *((int64_t*) ap);       break;
        case nsXPTType::T_I8     : dp->val.i8  = *((int64_t*)  ap);      break;
        case nsXPTType::T_I16    : dp->val.i16 = *((int64_t*) ap);       break;
        case nsXPTType::T_I32    : dp->val.i32 = *((int64_t*) ap);       break;
        case nsXPTType::T_I64    : dp->val.i64 = *((int64_t*) ap);       break;
        case nsXPTType::T_U8     : dp->val.u8  = *((uint64_t*) ap);      break;
        case nsXPTType::T_U16    : dp->val.u16 = *((uint64_t*)ap);       break;
        case nsXPTType::T_U32    : dp->val.u32 = *((uint64_t*)ap);       break;
        case nsXPTType::T_U64    : dp->val.u64 = *((uint64_t*) ap);      break;
        case nsXPTType::T_FLOAT  : dp->val.f   = ((float*)   ap)[1];     break;
        case nsXPTType::T_DOUBLE : dp->val.d   = *((double*) ap);        break;
        default:
            NS_ERROR("bad type");
            break;
        }
    }

    nsresult result = self->mOuter->CallMethod((uint16_t)methodIndex, info,
                                               paramBuffer);

    return result;
}

extern "C" nsresult SharedStub(int, int*);

/*
 * Avoid GCC stack protector to wipe out input registers since the compiler
 * thinks the function takes no arguments.
 */
#ifndef nostackprotector
#define nostackprotector __attribute__((__optimize__("no-stack-protector")))
#endif

#define STUB_ENTRY(n) \
nsresult nostackprotector nsXPTCStubBase::Stub##n() \
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
