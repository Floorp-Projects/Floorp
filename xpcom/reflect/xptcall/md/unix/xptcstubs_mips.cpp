/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * Version: MPL 1.1
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xptcprivate.h"

#include <stdint.h>

/*
 * This is for MIPS O32 ABI
 * Args contains a0-3 and then the stack.
 * Because a0 is 'this', we want to skip it
 */
extern "C" nsresult ATTRIBUTE_USED
PrepareAndDispatch(nsXPTCStubBase* self, uint32_t methodIndex, uint32_t* args)
{
    args++; // always skip over a0

#define PARAM_BUFFER_COUNT		16

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = nullptr;
    const nsXPTMethodInfo* info;
    uint8_t paramCount;
    uint8_t i;
    nsresult result = NS_ERROR_FAILURE;

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(uint16_t(methodIndex), &info);
    NS_ASSERTION(info,"no method info");

    paramCount = info->GetParamCount();

    // setup variant array pointer
    if(paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;
    NS_ASSERTION(dispatchParams,"no place for params");

    const uint8_t indexOfJSContext = info->IndexOfJSContext();

    uint32_t* ap = args;
    for(i = 0; i < paramCount; i++, ap++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &dispatchParams[i];

        if (i == indexOfJSContext)
            ap++;

        if(param.IsOut() || !type.IsArithmetic())
        {
            dp->val.p = (void*) *ap;
            continue;
        }

        switch(type)
        {
        case nsXPTType::T_I64   :
            if ((intptr_t)ap & 4) ap++;
            dp->val.i64 = *((int64_t*) ap); ap++;
            break;
        case nsXPTType::T_U64   :
            if ((intptr_t)ap & 4) ap++;
            dp->val.u64 = *((int64_t*) ap); ap++;
            break;
        case nsXPTType::T_DOUBLE:
            if ((intptr_t)ap & 4) ap++;
            dp->val.d   = *((double*) ap);  ap++;
            break;
#ifdef IS_LITTLE_ENDIAN
        default:
            dp->val.p = (void*) *ap;
            break;
#else
        case nsXPTType::T_I8    : dp->val.i8  = (int8_t)   *ap; break;
        case nsXPTType::T_I16   : dp->val.i16 = (int16_t)  *ap; break;
        case nsXPTType::T_I32   : dp->val.i32 = (int32_t)  *ap; break;
        case nsXPTType::T_U8    : dp->val.u8  = (uint8_t)  *ap; break;
        case nsXPTType::T_U16   : dp->val.u16 = (uint16_t) *ap; break;
        case nsXPTType::T_U32   : dp->val.u32 = (uint32_t) *ap; break;
        case nsXPTType::T_BOOL  : dp->val.b   = (bool)   *ap; break;
        case nsXPTType::T_CHAR  : dp->val.c   = (char)     *ap; break;
        case nsXPTType::T_WCHAR : dp->val.wc  = (wchar_t)  *ap; break;
        case nsXPTType::T_FLOAT : dp->val.f   = *(float *)  ap; break;
        default:
            NS_ASSERTION(0, "bad type");
            break;
#endif
        }
    }

    result = self->mOuter->CallMethod((uint16_t)methodIndex, info, dispatchParams);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

#define STUB_ENTRY(n) // done in the .s file

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"
