/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"
#include "xptc_gcc_x86_unix.h"

extern "C" {
static nsresult ATTRIBUTE_USED
__attribute__ ((regparm (3)))
PrepareAndDispatch(uint32_t methodIndex, nsXPTCStubBase* self, uint32_t* args)
{
#define PARAM_BUFFER_COUNT     16

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = nullptr;
    const nsXPTMethodInfo* info;
    uint8_t paramCount;
    uint8_t i;

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(uint16_t(methodIndex), &info);
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
        // else
	    dp->val.p = (void*) *ap;
        switch(type)
        {
        case nsXPTType::T_I64    : dp->val.i64 = *((int64_t*) ap); ap++; break;
        case nsXPTType::T_U64    : dp->val.u64 = *((uint64_t*)ap); ap++; break;
        case nsXPTType::T_DOUBLE : dp->val.d   = *((double*)  ap); ap++; break;
        }
    }

    nsresult result = self->mOuter->CallMethod((uint16_t)methodIndex, info,
                                               dispatchParams);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}
} // extern "C"

#if !defined(XP_MACOSX)

#define STUB_HEADER(a, b) ".hidden " SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase" #a "Stub" #b "Ev\n\t" \
                          ".type   " SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase" #a "Stub" #b "Ev,@function\n"

#define STUB_SIZE(a, b)  ".size        " SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase" #a "Stub" #b "Ev,.-" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase" #a "Stub" #b "Ev\n\t"

#else

#define STUB_HEADER(a, b)
#define STUB_SIZE(a, b)

#endif

// gcc3 mangling tends to insert the length of the method name
#define STUB_ENTRY(n) \
asm(".text\n\t" \
    ".align	2\n\t" \
    ".if	" #n " < 10\n\t" \
    ".globl	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase5Stub" #n "Ev\n\t" \
    STUB_HEADER(5, n)                                                   \
    SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase5Stub" #n "Ev:\n\t" \
    ".elseif	" #n " < 100\n\t" \
    ".globl	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase6Stub" #n "Ev\n\t" \
    STUB_HEADER(6, n)                                                   \
    SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase6Stub" #n "Ev:\n\t" \
    ".elseif    " #n " < 1000\n\t" \
    ".globl     " SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase7Stub" #n "Ev\n\t" \
    STUB_HEADER(7, n)                                                   \
    SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase7Stub" #n "Ev:\n\t" \
    ".else\n\t" \
    ".err	\"stub number " #n " >= 1000 not yet supported\"\n\t" \
    ".endif\n\t" \
    "movl	$" #n ", %eax\n\t" \
    "jmp	" SYMBOL_UNDERSCORE "SharedStub\n\t" \
    ".if	" #n " < 10\n\t" \
    STUB_SIZE(5, n) \
    ".elseif	" #n " < 100\n\t" \
    STUB_SIZE(6, n) \
    ".else\n\t" \
    STUB_SIZE(7, n) \
    ".endif");

// static nsresult SharedStub(uint32_t methodIndex) __attribute__((regparm(1)))
asm(".text\n\t"
    ".align	2\n\t"
#if !defined(XP_MACOSX)
    ".type	" SYMBOL_UNDERSCORE "SharedStub,@function\n\t"
#endif
    SYMBOL_UNDERSCORE "SharedStub:\n\t"
    "leal	0x08(%esp), %ecx\n\t"
    "movl	0x04(%esp), %edx\n\t"
    "jmp	" SYMBOL_UNDERSCORE "PrepareAndDispatch\n\t"
#if !defined(XP_MACOSX)
    ".size	" SYMBOL_UNDERSCORE "SharedStub,.-" SYMBOL_UNDERSCORE "SharedStub"
#endif
);

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
