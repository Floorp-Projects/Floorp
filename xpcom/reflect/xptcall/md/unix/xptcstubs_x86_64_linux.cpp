/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implement shared vtbl methods.

// Keep this in sync with the darwin version.

#include "xptcprivate.h"

// The Linux/x86-64 ABI passes the first 6 integer parameters and the
// first 8 floating point parameters in registers (rdi, rsi, rdx, rcx,
// r8, r9 and xmm0-xmm7), no stack space is allocated for these by the
// caller.  The rest of the parameters are passed in the callers stack
// area.

const uint32_t PARAM_BUFFER_COUNT   = 16;
const uint32_t GPR_COUNT            = 6;
const uint32_t FPR_COUNT            = 8;

// PrepareAndDispatch() is called by SharedStub() and calls the actual method.
//
// - 'args[]' contains the arguments passed on stack
// - 'gpregs[]' contains the arguments passed in integer registers
// - 'fpregs[]' contains the arguments passed in floating point registers
//
// The parameters are mapped into an array of type 'nsXPTCMiniVariant'
// and then the method gets called.

extern "C" nsresult ATTRIBUTE_USED
PrepareAndDispatch(nsXPTCStubBase * self, uint32_t methodIndex,
                   uint64_t * args, uint64_t * gpregs, double *fpregs)
{
    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = nullptr;
    const nsXPTMethodInfo* info;
    uint32_t paramCount;
    uint32_t i;
    nsresult result = NS_ERROR_FAILURE;

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(uint16_t(methodIndex), &info);
    NS_ASSERTION(info,"no method info");
    if (!info)
        return NS_ERROR_UNEXPECTED;

    paramCount = info->GetParamCount();

    // setup variant array pointer
    if (paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;

    NS_ASSERTION(dispatchParams,"no place for params");
    if (!dispatchParams)
        return NS_ERROR_OUT_OF_MEMORY;

    const uint8_t indexOfJSContext = info->IndexOfJSContext();

    uint64_t* ap = args;
    uint32_t nr_gpr = 1;    // skip one GPR register for 'that'
    uint32_t nr_fpr = 0;
    uint64_t value;

    for (i = 0; i < paramCount; i++) {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &dispatchParams[i];

        if (i == indexOfJSContext) {
            if (nr_gpr < GPR_COUNT)
                nr_gpr++;
            else
                ap++;
        }

        if (!param.IsOut() && type == nsXPTType::T_DOUBLE) {
            if (nr_fpr < FPR_COUNT)
                dp->val.d = fpregs[nr_fpr++];
            else
                dp->val.d = *(double*)ap++;
            continue;
        }
        if (!param.IsOut() && type == nsXPTType::T_FLOAT) {
            if (nr_fpr < FPR_COUNT)
                // The value in %xmm register is already prepared to
                // be retrieved as a float. Therefore, we pass the
                // value verbatim, as a double without conversion.
                dp->val.d = fpregs[nr_fpr++];
            else
                dp->val.f = *(float*)ap++;
            continue;
        }
        if (nr_gpr < GPR_COUNT)
            value = gpregs[nr_gpr++];
        else
            value = *ap++;

        if (param.IsOut() || !type.IsArithmetic()) {
            dp->val.p = (void*) value;
            continue;
        }

        switch (type) {
        case nsXPTType::T_I8:      dp->val.i8  = (int8_t)   value; break;
        case nsXPTType::T_I16:     dp->val.i16 = (int16_t)  value; break;
        case nsXPTType::T_I32:     dp->val.i32 = (int32_t)  value; break;
        case nsXPTType::T_I64:     dp->val.i64 = (int64_t)  value; break;
        case nsXPTType::T_U8:      dp->val.u8  = (uint8_t)  value; break;
        case nsXPTType::T_U16:     dp->val.u16 = (uint16_t) value; break;
        case nsXPTType::T_U32:     dp->val.u32 = (uint32_t) value; break;
        case nsXPTType::T_U64:     dp->val.u64 = (uint64_t) value; break;
        // Cast to uint8_t first, to remove garbage on upper 56 bits.
        case nsXPTType::T_BOOL:    dp->val.b   = (bool)(uint8_t)   value; break;
        case nsXPTType::T_CHAR:    dp->val.c   = (char)     value; break;
        case nsXPTType::T_WCHAR:   dp->val.wc  = (wchar_t)  value; break;

        default:
            NS_ERROR("bad type");
            break;
        }
    }

    result = self->mOuter->CallMethod((uint16_t) methodIndex, info, dispatchParams);

    if (dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

// Linux/x86-64 uses gcc >= 3.1
// We don't include .cfi_startproc/endproc directives for the individual stubs
// because there's no extra CFI bits to define beyond the default CIE.
#define STUB_ENTRY(n) \
asm(".section	\".text\"\n\t" \
    ".align	2\n\t" \
    ".if	" #n " < 10\n\t" \
    ".globl	_ZN14nsXPTCStubBase5Stub" #n "Ev\n\t" \
    ".hidden	_ZN14nsXPTCStubBase5Stub" #n "Ev\n\t" \
    ".type	_ZN14nsXPTCStubBase5Stub" #n "Ev,@function\n" \
    "_ZN14nsXPTCStubBase5Stub" #n "Ev:\n\t" \
    ".elseif	" #n " < 100\n\t" \
    ".globl	_ZN14nsXPTCStubBase6Stub" #n "Ev\n\t" \
    ".hidden	_ZN14nsXPTCStubBase6Stub" #n "Ev\n\t" \
    ".type	_ZN14nsXPTCStubBase6Stub" #n "Ev,@function\n" \
    "_ZN14nsXPTCStubBase6Stub" #n "Ev:\n\t" \
    ".elseif    " #n " < 1000\n\t" \
    ".globl     _ZN14nsXPTCStubBase7Stub" #n "Ev\n\t" \
    ".hidden    _ZN14nsXPTCStubBase7Stub" #n "Ev\n\t" \
    ".type      _ZN14nsXPTCStubBase7Stub" #n "Ev,@function\n" \
    "_ZN14nsXPTCStubBase7Stub" #n "Ev:\n\t" \
    ".else\n\t" \
    ".err	\"stub number " #n " >= 1000 not yet supported\"\n\t" \
    ".endif\n\t" \
    "movl	$" #n ", %eax\n\t" \
    "jmp	SharedStub\n\t" \
    ".if	" #n " < 10\n\t" \
    ".size	_ZN14nsXPTCStubBase5Stub" #n "Ev,.-_ZN14nsXPTCStubBase5Stub" #n "Ev\n\t" \
    ".elseif	" #n " < 100\n\t" \
    ".size	_ZN14nsXPTCStubBase6Stub" #n "Ev,.-_ZN14nsXPTCStubBase6Stub" #n "Ev\n\t" \
    ".else\n\t" \
    ".size	_ZN14nsXPTCStubBase7Stub" #n "Ev,.-_ZN14nsXPTCStubBase7Stub" #n "Ev\n\t" \
    ".endif");

// static nsresult SharedStub(uint32_t methodIndex)
asm(".section   \".text\"\n\t"
    ".align     2\n\t"
    ".type      SharedStub,@function\n\t"
    "SharedStub:\n\t"
    ".cfi_startproc\n\t"
    // make room for gpregs (48), fpregs (64)
    "pushq      %rbp\n\t"
    ".cfi_def_cfa_offset 16\n\t"
    ".cfi_offset 6, -16\n\t"
    "movq       %rsp,%rbp\n\t"
    ".cfi_def_cfa_register 6\n\t"
    "subq       $112,%rsp\n\t"
    // save GP registers
    "movq       %rdi,-112(%rbp)\n\t"
    "movq       %rsi,-104(%rbp)\n\t"
    "movq       %rdx, -96(%rbp)\n\t"
    "movq       %rcx, -88(%rbp)\n\t"
    "movq       %r8 , -80(%rbp)\n\t"
    "movq       %r9 , -72(%rbp)\n\t"
    ".cfi_offset 5, -24\n\t"	// rdi
    ".cfi_offset 4, -32\n\t"	// rsi
    ".cfi_offset 1, -40\n\t"	// rdx
    ".cfi_offset 2, -48\n\t"	// rcx
    ".cfi_offset 8, -56\n\t"	// r8
    ".cfi_offset 9, -64\n\t"	// r9
    "leaq       -112(%rbp),%rcx\n\t"
    // save FP registers
    "movsd      %xmm0,-64(%rbp)\n\t"
    "movsd      %xmm1,-56(%rbp)\n\t"
    "movsd      %xmm2,-48(%rbp)\n\t"
    "movsd      %xmm3,-40(%rbp)\n\t"
    "movsd      %xmm4,-32(%rbp)\n\t"
    "movsd      %xmm5,-24(%rbp)\n\t"
    "movsd      %xmm6,-16(%rbp)\n\t"
    "movsd      %xmm7, -8(%rbp)\n\t"
    "leaq       -64(%rbp),%r8\n\t"
    // rdi has the 'self' pointer already
    "movl       %eax,%esi\n\t"
    "leaq       16(%rbp),%rdx\n\t"
    "call       PrepareAndDispatch@plt\n\t"
    "leave\n\t"
    ".cfi_def_cfa 7, 8\n\t"
    "ret\n\t"
    ".cfi_endproc\n\t"
    ".size      SharedStub,.-SharedStub");

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"
