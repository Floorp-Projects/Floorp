/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"

#ifdef __GNUC__
/* This tells gcc3.4+ not to optimize away symbols.
 * @see http://gcc.gnu.org/gcc-3.4/changes.html
 */
#define DONT_DROP_OR_WARN __attribute__((used))
#else
/* This tells older gccs not to warn about unused vairables.
 * @see http://docs.freebsd.org/info/gcc/gcc.info.Variable_Attributes.html
 */
#define DONT_DROP_OR_WARN __attribute__((unused))
#endif

/* Specify explicitly a symbol for this function, don't try to guess the c++ mangled symbol.  */
static nsresult PrepareAndDispatch(nsXPTCStubBase* self, uint32_t methodIndex, uint32_t* args) asm("_PrepareAndDispatch")
DONT_DROP_OR_WARN;

static nsresult ATTRIBUTE_USED
PrepareAndDispatch(nsXPTCStubBase* self, uint32_t methodIndex, uint32_t* args)
{
#define PARAM_BUFFER_COUNT     16

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = nullptr;
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
    if (!dispatchParams)
        return NS_ERROR_OUT_OF_MEMORY;

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

    result = self->mOuter->CallMethod((uint16_t)methodIndex, info, dispatchParams);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

/*
 * This is our shared stub.
 *
 * r0 = Self.
 *
 * The Rules:
 *   We pass an (undefined) number of arguments into this function.
 *   The first 3 C++ arguments are in r1 - r3, the rest are built
 *   by the calling function on the stack.
 *
 *   We are allowed to corrupt r0 - r3, ip, and lr.
 *
 * Other Info:
 *   We pass the stub number in using `ip'.
 *
 * Implementation:
 * - We save r1 to r3 inclusive onto the stack, which will be
 *   immediately below the caller saved arguments.
 * - setup r2 (PrepareAndDispatch's args pointer) to point at
 *   the base of all these arguments
 * - Save LR (for the return address)
 * - Set r1 (PrepareAndDispatch's methodindex argument) from ip
 * - r0 is passed through (self)
 * - Call PrepareAndDispatch
 * - When the call returns, we return by loading the PC off the
 *   stack, and undoing the stack (one instruction)!
 *
 */
__asm__ ("\n\
        .text							\n\
        .align 2						\n\
SharedStub:							\n\
	stmfd	sp!, {r1, r2, r3}				\n\
	mov	r2, sp						\n\
	str	lr, [sp, #-4]!					\n\
	mov	r1, ip						\n\
	bl	_PrepareAndDispatch	                        \n\
	ldr	pc, [sp], #16");

/*
 * Create sets of stubs to call the SharedStub.
 * We don't touch the stack here, nor any registers, other than IP.
 * IP is defined to be corruptable by a called function, so we are
 * safe to use it.
 *
 * This will work with or without optimisation.
 */

/*
 * Note : As G++3 ABI contains the length of the functionname in the
 *  mangled name, it is difficult to get a generic assembler mechanism like
 *  in the G++ 2.95 case.
 *  Create names would be like :
 *    _ZN14nsXPTCStubBase5Stub9Ev
 *    _ZN14nsXPTCStubBase6Stub13Ev
 *    _ZN14nsXPTCStubBase7Stub144Ev
 *  Use the assembler directives to get the names right...
 */

#define STUB_ENTRY(n)						\
  __asm__(							\
	".section \".text\"\n"					\
"	.align 2\n"						\
"	.iflt ("#n" - 10)\n"                                    \
"	.globl	_ZN14nsXPTCStubBase5Stub"#n"Ev\n"		\
"	.type	_ZN14nsXPTCStubBase5Stub"#n"Ev,#function\n"	\
"_ZN14nsXPTCStubBase5Stub"#n"Ev:\n"				\
"	.else\n"                                                \
"	.iflt  ("#n" - 100)\n"                                  \
"	.globl	_ZN14nsXPTCStubBase6Stub"#n"Ev\n"		\
"	.type	_ZN14nsXPTCStubBase6Stub"#n"Ev,#function\n"	\
"_ZN14nsXPTCStubBase6Stub"#n"Ev:\n"				\
"	.else\n"                                                \
"	.iflt ("#n" - 1000)\n"                                  \
"	.globl	_ZN14nsXPTCStubBase7Stub"#n"Ev\n"		\
"	.type	_ZN14nsXPTCStubBase7Stub"#n"Ev,#function\n"	\
"_ZN14nsXPTCStubBase7Stub"#n"Ev:\n"				\
"	.else\n"                                                \
"	.err \"stub number "#n"> 1000 not yet supported\"\n"    \
"	.endif\n"                                               \
"	.endif\n"                                               \
"	.endif\n"                                               \
"	mov	ip, #"#n"\n"					\
"	b	SharedStub\n\t");

#if 0
/*
 * This part is left in as comment : this is how the method definition
 * should look like.
 */

#define STUB_ENTRY(n)  \
nsresult nsXPTCStubBase::Stub##n ()  \
{ \
  __asm__ (	  		        \
"	mov	ip, #"#n"\n"					\
"	b	SharedStub\n\t");                               \
  return 0; /* avoid warnings */                                \
}
#endif


#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"
