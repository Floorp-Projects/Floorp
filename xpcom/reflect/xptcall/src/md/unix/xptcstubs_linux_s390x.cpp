/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"
#include "xptiprivate.h"

static nsresult
PrepareAndDispatch(nsXPTCStubBase* self, uint32_t methodIndex, 
                   uint64_t* a_gpr, uint64_t *a_fpr, uint64_t *a_ov)
{
#define PARAM_BUFFER_COUNT     16

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = NULL;
    const nsXPTMethodInfo* info;
    uint8_t paramCount;
    uint8_t i;
    nsresult result = NS_ERROR_FAILURE;

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(uint16_t(methodIndex), &info);
    NS_ASSERTION(info,"no info");

    paramCount = info->GetParamCount();

    // setup variant array pointer
    if(paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;
    NS_ASSERTION(dispatchParams,"no place for params");

    uint32_t gpr = 1, fpr = 0;

    for(i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &dispatchParams[i];

        if(param.IsOut() || !type.IsArithmetic())
        {
            if (gpr < 5)
                dp->val.p = (void*) *a_gpr++, gpr++;
            else
                dp->val.p = (void*) *a_ov++;
            continue;
        }
        // else
        switch(type)
        {
        case nsXPTType::T_I8     : 
            if (gpr < 5)
                dp->val.i8  = *((int64_t*) a_gpr), a_gpr++, gpr++;
            else
                dp->val.i8  = *((int64_t*) a_ov ), a_ov++;
            break;
        case nsXPTType::T_I16    : 
            if (gpr < 5)
                dp->val.i16 = *((int64_t*) a_gpr), a_gpr++, gpr++;
            else
                dp->val.i16 = *((int64_t*) a_ov ), a_ov++;
            break;
        case nsXPTType::T_I32    : 
            if (gpr < 5)
                dp->val.i32 = *((int64_t*) a_gpr), a_gpr++, gpr++;
            else
                dp->val.i32 = *((int64_t*) a_ov ), a_ov++;
            break;
        case nsXPTType::T_I64    : 
            if (gpr < 5)
                dp->val.i64 = *((int64_t*) a_gpr), a_gpr++, gpr++;
            else
                dp->val.i64 = *((int64_t*) a_ov ), a_ov++;
            break;
        case nsXPTType::T_U8     : 
            if (gpr < 5)
                dp->val.u8  = *((uint64_t*)a_gpr), a_gpr++, gpr++;
            else
                dp->val.u8  = *((uint64_t*)a_ov ), a_ov++;
            break;
        case nsXPTType::T_U16    : 
            if (gpr < 5)
                dp->val.u16 = *((uint64_t*)a_gpr), a_gpr++, gpr++;
            else
                dp->val.u16 = *((uint64_t*)a_ov ), a_ov++;
            break;
        case nsXPTType::T_U32    : 
            if (gpr < 5)
                dp->val.u32 = *((uint64_t*)a_gpr), a_gpr++, gpr++;
            else
                dp->val.u32 = *((uint64_t*)a_ov ), a_ov++;
            break;
        case nsXPTType::T_U64    : 
            if (gpr < 5)
                dp->val.u64 = *((uint64_t*)a_gpr), a_gpr++, gpr++;
            else
                dp->val.u64 = *((uint64_t*)a_ov ), a_ov++;
            break;
        case nsXPTType::T_FLOAT  : 
            if (fpr < 4)
                dp->val.f   = *((float*)   a_fpr), a_fpr++, fpr++;
            else
                dp->val.f   = *(((float*)  a_ov )+1), a_ov++;
            break;
        case nsXPTType::T_DOUBLE : 
            if (fpr < 4)
                dp->val.d   = *((double*)  a_fpr), a_fpr++, fpr++;
            else
                dp->val.d   = *((double*)  a_ov ), a_ov++;
            break;
        case nsXPTType::T_BOOL   : 
            if (gpr < 5)
                dp->val.b   = *((uint64_t*)a_gpr), a_gpr++, gpr++;
            else
                dp->val.b   = *((uint64_t*)a_ov ), a_ov++;
            break;
        case nsXPTType::T_CHAR   : 
            if (gpr < 5)
                dp->val.c   = *((uint64_t*)a_gpr), a_gpr++, gpr++;
            else
                dp->val.c   = *((uint64_t*)a_ov ), a_ov++;
            break;
        case nsXPTType::T_WCHAR  : 
            if (gpr < 5)
                dp->val.wc  = *((uint64_t*)a_gpr), a_gpr++, gpr++;
            else
                dp->val.wc  = *((uint64_t*)a_ov ), a_ov++;
            break;
        default:
            NS_ERROR("bad type");
            break;
        }
    }

    result = self->mOuter->CallMethod((uint16_t)methodIndex, info, dispatchParams);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

#define STUB_ENTRY(n) \
nsresult nsXPTCStubBase::Stub##n() \
{                             \
    uint64_t a_gpr[4];        \
    uint64_t a_fpr[4];        \
    uint64_t *a_ov;           \
                              \
    __asm__ __volatile__      \
    (                         \
        "lg     %0,0(15)\n\t" \
        "aghi   %0,160\n\t"   \
        "stmg   3,6,0(%5)\n\t"\
        "std    0,%1\n\t"     \
        "std    2,%2\n\t"     \
        "std    4,%3\n\t"     \
        "std    6,%4\n\t"     \
        : "=&a" (a_ov),       \
          "=m" (a_fpr[0]),    \
          "=m" (a_fpr[1]),    \
          "=m" (a_fpr[2]),    \
          "=m" (a_fpr[3])     \
        : "a" (a_gpr)         \
       : "memory", "cc",      \
         "3", "4", "5", "6"   \
    );                        \
                              \
    return PrepareAndDispatch(this, n, a_gpr, a_fpr, a_ov); \
}

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"

