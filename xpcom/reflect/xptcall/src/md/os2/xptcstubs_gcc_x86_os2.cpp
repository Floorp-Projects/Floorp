/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"
#include "xptiprivate.h"
#include "xptc_gcc_x86_unix.h"

extern "C" {
static nsresult ATTRIBUTE_USED
__attribute__ ((regparm (3)))
PrepareAndDispatch(uint32_t methodIndex, nsXPTCStubBase* self, uint32_t* args)
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
	    dp->val.p = (void*) *ap;
        switch(type)
        {
        case nsXPTType::T_I64    : dp->val.i64 = *((int64_t*) ap); ap++; break;
        case nsXPTType::T_U64    : dp->val.u64 = *((uint64_t*)ap); ap++; break;
        case nsXPTType::T_DOUBLE : dp->val.d   = *((double*)  ap); ap++; break;
        }
    }

    result = self->mOuter->CallMethod((uint16_t)methodIndex, info, dispatchParams);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}
} // extern "C"

#define SYMBOL_EXPORT(sym) \
    ".stabs \"" sym ",0=" sym ",code\", 0x6c,0,0,-42\n\t"

#define STUB_ENTRY(n) \
asm(".text\n\t" \
    ".align	2\n\t" \
    ".if	" #n " < 10\n\t" \
    ".globl	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase5Stub" #n "Ev\n\t" \
    ".type	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase5Stub" #n "Ev,@function\n" \
    SYMBOL_EXPORT(SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase5Stub" #n "Ev") \
    SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase5Stub" #n "Ev:\n\t" \
    ".elseif	" #n " < 100\n\t" \
    ".globl	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase6Stub" #n "Ev\n\t" \
    ".type	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase6Stub" #n "Ev,@function\n" \
    SYMBOL_EXPORT(SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase6Stub" #n "Ev") \
    SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase6Stub" #n "Ev:\n\t" \
    ".elseif    " #n " < 1000\n\t" \
    ".globl     " SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase7Stub" #n "Ev\n\t" \
    ".type      " SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase7Stub" #n "Ev,@function\n" \
    SYMBOL_EXPORT(SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase7Stub" #n "Ev") \
    SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase7Stub" #n "Ev:\n\t" \
    ".else\n\t" \
    ".err	\"stub number " #n " >= 1000 not yet supported\"\n\t" \
    ".endif\n\t" \
    "movl	$" #n ", %eax\n\t" \
    "jmp	" SYMBOL_UNDERSCORE "SharedStub\n\t" \
    ".if	" #n " < 10\n\t" \
    ".size	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase5Stub" #n "Ev,.-" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase5Stub" #n "Ev\n\t" \
    ".elseif	" #n " < 100\n\t" \
    ".size	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase6Stub" #n "Ev,.-" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase6Stub" #n "Ev\n\t" \
    ".else\n\t" \
    ".size	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase7Stub" #n "Ev,.-" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase7Stub" #n "Ev\n\t" \
    ".endif");

// static nsresult SharedStub(uint32_t methodIndex) __attribute__((regparm(1)))
asm(".text\n\t"
    ".align	2\n\t"
    ".type	" SYMBOL_UNDERSCORE "SharedStub,@function\n\t"
    SYMBOL_UNDERSCORE "SharedStub:\n\t"
    "leal	0x08(%esp), %ecx\n\t"
    "movl	0x04(%esp), %edx\n\t"
    "jmp	" SYMBOL_UNDERSCORE "PrepareAndDispatch\n\t"
    ".size	" SYMBOL_UNDERSCORE "SharedStub,.-" SYMBOL_UNDERSCORE "SharedStub");

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

