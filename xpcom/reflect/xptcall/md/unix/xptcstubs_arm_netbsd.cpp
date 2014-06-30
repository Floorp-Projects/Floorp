/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"

nsresult ATTRIBUTE_USED
PrepareAndDispatch(nsXPTCStubBase* self, uint32_t methodIndex, uint32_t* args)
{
#define PARAM_BUFFER_COUNT     16

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = nullptr;
    nsIInterfaceInfo* iface_info = nullptr;
    const nsXPTMethodInfo* info;
    uint8_t paramCount;
    uint8_t i;
    nsresult result = NS_ERROR_FAILURE;

    NS_ASSERTION(self,"no self");

    self->GetInterfaceInfo(&iface_info);
    NS_ASSERTION(iface_info,"no interface info");

    iface_info->GetMethodInfo(uint16_t(methodIndex), &info);
    NS_ASSERTION(info,"no interface info");

    paramCount = info->GetParamCount();

    // setup variant array pointer
    if(paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;
    NS_ASSERTION(dispatchParams,"no place for params");

    uint32_t* ap = args;
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
        case nsXPTType::T_I8     : dp->val.i8  = *((int8_t*)  ap);       break;
        case nsXPTType::T_I16    : dp->val.i16 = *((int16_t*) ap);       break;
        case nsXPTType::T_I32    : dp->val.i32 = *((int32_t*) ap);       break;
        case nsXPTType::T_I64    : dp->val.i64 = *((int64_t*) ap); ap++; break;
        case nsXPTType::T_U8     : dp->val.u8  = *((uint8_t*) ap);       break;
        case nsXPTType::T_U16    : dp->val.u16 = *((uint16_t*)ap);       break;
        case nsXPTType::T_U32    : dp->val.u32 = *((uint32_t*)ap);       break;
        case nsXPTType::T_U64    : dp->val.u64 = *((uint64_t*)ap); ap++; break;
        case nsXPTType::T_FLOAT  : dp->val.f   = *((float*)   ap);       break;
        case nsXPTType::T_DOUBLE : dp->val.d   = *((double*)  ap); ap++; break;
        case nsXPTType::T_BOOL   : dp->val.b   = *((bool*)  ap);       break;
        case nsXPTType::T_CHAR   : dp->val.c   = *((char*)    ap);       break;
        case nsXPTType::T_WCHAR  : dp->val.wc  = *((wchar_t*) ap);       break;
        default:
            NS_ERROR("bad type");
            break;
        }
    }

    result = self->CallMethod((uint16_t)methodIndex, info, dispatchParams);

    NS_RELEASE(iface_info);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

/*
 * These stubs move just move the values passed in registers onto the stack,
 * so they are contiguous with values passed on the stack, and then calls
 * PrepareAndDispatch() to do the dirty work.
 */

#define STUB_ENTRY(n)							\
__asm__(								\
    ".global	_Stub"#n"__14nsXPTCStubBase\n\t"			\
"_Stub"#n"__14nsXPTCStubBase:\n\t"					\
    "stmfd	sp!, {r1, r2, r3}	\n\t"				\
    "mov	ip, sp			\n\t"				\
    "stmfd	sp!, {fp, ip, lr, pc}	\n\t"				\
    "sub	fp, ip, #4		\n\t"				\
    "mov	r1, #"#n"		\n\t"    /* = methodIndex 	*/ \
    "add	r2, sp, #16		\n\t"				\
    "bl		_PrepareAndDispatch__FP14nsXPTCStubBaseUiPUi   \n\t"	\
    "ldmea	fp, {fp, sp, lr}	\n\t"				\
    "add	sp, sp, #12		\n\t"				\
    "mov	pc, lr			\n\t"				\
);

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"
