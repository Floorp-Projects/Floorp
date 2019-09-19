/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implement shared vtbl methods.

#include "xptcprivate.h"

// Prior to POWER8, all 64-bit Power ISA systems used ELF v1 ABI, found
// here:
//   https://refspecs.linuxfoundation.org/ELF/ppc64/PPC-elf64abi.html
// and in particular:
//   https://refspecs.linuxfoundation.org/ELF/ppc64/PPC-elf64abi.html#FUNC-CALL
// Little-endian ppc64le, however, uses ELF v2 ABI, which is here:
//   http://openpowerfoundation.org/wp-content/uploads/resources/leabi/leabi-20170510.pdf
// and in particular section 2.2, page 22. However, most big-endian ppc64
// systems still use ELF v1, so this file should support both.
//
// Both ABIs pass the first 8 integral parameters and the first 13 floating
// point parameters in registers r3-r10 and f1-f13. No stack space is
// allocated for these by the caller. The rest of the parameters are passed
// in the caller's stack area. The stack pointer must stay 16-byte aligned.

const uint32_t PARAM_BUFFER_COUNT   = 16;
const uint32_t GPR_COUNT            = 7;
const uint32_t FPR_COUNT            = 13;

// PrepareAndDispatch() is called by SharedStub() and calls the actual method.
//
// - 'args[]' contains the arguments passed on stack
// - 'gpregs[]' contains the arguments passed in integer registers
// - 'fpregs[]' contains the arguments passed in floating point registers
//
// The parameters are mapped into an array of type 'nsXPTCMiniVariant'
// and then the method gets called.
//
// Both ABIs use the same register assignment strategy, as per this
// example from V1 ABI section 3.2.3 and V2 ABI section 2.2.3.2 [page 43]:
//
// typedef struct {
//   int    a;
//   double dd;
// } sparm;
// sparm   s, t;
// int     c, d, e;
// long double ld;
// double  ff, gg, hh;
//
// x = func(c, ff, d, ld, s, gg, t, e, hh);
//
// Parameter     Register     Offset in parameter save area
// c             r3           0-7    (not stored in parameter save area)
// ff            f1           8-15   (not stored)
// d             r5           16-23  (not stored)
// ld            f2,f3        24-39  (not stored)
// s             r8,r9        40-55  (not stored)
// gg            f4           56-63  (not stored)
// t             (none)       64-79  (stored in parameter save area)
// e             (none)       80-87  (stored)
// hh            f5           88-95  (not stored)
//
// i.e., each successive FPR usage skips a GPR, but not the other way around.

extern "C" nsresult ATTRIBUTE_USED
PrepareAndDispatch(nsXPTCStubBase * self, uint32_t methodIndex,
                   uint64_t * args, uint64_t * gpregs, double *fpregs)
{
    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = nullptr;
    const nsXPTMethodInfo* info;
    uint32_t paramCount;
    uint32_t i;

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(uint16_t(methodIndex), &info);
    NS_ASSERTION(info,"no method info");
    if (!info)
        return NS_ERROR_UNEXPECTED;

    paramCount = info->GetParamCount();

    // setup variant array pointer
    if(paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;

    NS_ASSERTION(dispatchParams,"no place for params");

    const uint8_t indexOfJSContext = info->IndexOfJSContext();

    uint64_t* ap = args;
    // |that| is implicit in the calling convention; we really do start at the
    // first GPR (as opposed to x86_64).
    uint32_t nr_gpr = 0;
    uint32_t nr_fpr = 0;
    uint64_t value;

    for(i = 0; i < paramCount; i++) {
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
            if (nr_fpr < FPR_COUNT) {
                dp->val.d = fpregs[nr_fpr++];
                nr_gpr++;
            } else {
                dp->val.d = *(double*)ap++;
            }
            continue;
        }
        if (!param.IsOut() && type == nsXPTType::T_FLOAT) {
            if (nr_fpr < FPR_COUNT) {
                // Single-precision floats are passed in FPRs too.
                dp->val.f = (float)fpregs[nr_fpr++];
                nr_gpr++;
            } else {
#ifdef __LITTLE_ENDIAN__
                dp->val.f = *(float*)ap++;
#else
                // Big endian needs adjustment to point to the least
                // significant word.
                float* p = (float*)ap;
                p++;
                dp->val.f = *p;
                ap++;
#endif
            }
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
        case nsXPTType::T_BOOL:    dp->val.b   = (bool)     value; break;
        case nsXPTType::T_CHAR:    dp->val.c   = (char)     value; break;
        case nsXPTType::T_WCHAR:   dp->val.wc  = (wchar_t)  value; break;

        default:
            NS_ERROR("bad type");
            break;
        }
    }

    nsresult result = self->mOuter->CallMethod((uint16_t) methodIndex, info,
                                               dispatchParams);

    if (dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

// Load r11 with the constant 'n' and branch to SharedStub().
//
// As G++3 ABI contains the length of the functionname in the mangled
// name, it is difficult to get a generic assembler mechanism like
// in the G++ 2.95 case.
// XXX Yes, it's ugly that we're relying on gcc's name-mangling here;
// however, it's quick, dirty, and'll break when the ABI changes on
// us, which is what we want ;-).
// Create names would be like:
// _ZN14nsXPTCStubBase5Stub1Ev
// _ZN14nsXPTCStubBase6Stub12Ev
// _ZN14nsXPTCStubBase7Stub123Ev
// _ZN14nsXPTCStubBase8Stub1234Ev
// etc.
// Use assembler directives to get the names right.

#if _CALL_ELF == 2
# define STUB_ENTRY(n)                                                  \
__asm__ (                                                               \
        ".section \".text\" \n\t"                                       \
        ".align 2 \n\t"                                                 \
        ".if "#n" < 10 \n\t"                                            \
        ".globl _ZN14nsXPTCStubBase5Stub"#n"Ev \n\t"                    \
        ".type  _ZN14nsXPTCStubBase5Stub"#n"Ev,@function \n\n"          \
"_ZN14nsXPTCStubBase5Stub"#n"Ev: \n\t"                                  \
        "0: addis 2,12,.TOC.-0b@ha \n\t"                                \
        "addi     2,2,.TOC.-0b@l \n\t"                                  \
        ".localentry _ZN14nsXPTCStubBase5Stub"#n"Ev,.-_ZN14nsXPTCStubBase5Stub"#n"Ev \n\t" \
                                                                        \
        ".elseif "#n" < 100 \n\t"                                       \
        ".globl _ZN14nsXPTCStubBase6Stub"#n"Ev \n\t"                    \
        ".type  _ZN14nsXPTCStubBase6Stub"#n"Ev,@function \n\n"          \
"_ZN14nsXPTCStubBase6Stub"#n"Ev: \n\t"                                  \
        "0: addis 2,12,.TOC.-0b@ha \n\t"                                \
        "addi     2,2,.TOC.-0b@l \n\t"                                  \
        ".localentry _ZN14nsXPTCStubBase6Stub"#n"Ev,.-_ZN14nsXPTCStubBase6Stub"#n"Ev \n\t" \
                                                                        \
        ".elseif "#n" < 1000 \n\t"                                      \
        ".globl _ZN14nsXPTCStubBase7Stub"#n"Ev \n\t"                    \
        ".type  _ZN14nsXPTCStubBase7Stub"#n"Ev,@function \n\n"          \
"_ZN14nsXPTCStubBase7Stub"#n"Ev: \n\t"                                  \
        "0: addis 2,12,.TOC.-0b@ha \n\t"                                \
        "addi     2,2,.TOC.-0b@l \n\t"                                  \
        ".localentry _ZN14nsXPTCStubBase7Stub"#n"Ev,.-_ZN14nsXPTCStubBase7Stub"#n"Ev \n\t" \
                                                                        \
        ".else  \n\t"                                                   \
        ".err   \"stub number "#n" >= 1000 not yet supported\"\n"       \
        ".endif \n\t"                                                   \
                                                                        \
        "li     11,"#n" \n\t"                                           \
        "b      SharedStub \n"                                          \
);
#else
# define STUB_ENTRY(n)                                                  \
__asm__ (                                                               \
        ".section \".toc\",\"aw\" \n\t"                                 \
        ".section \".text\" \n\t"                                       \
        ".align 2 \n\t"                                                 \
        ".if "#n" < 10 \n\t"                                            \
        ".globl _ZN14nsXPTCStubBase5Stub"#n"Ev \n\t"                    \
        ".section \".opd\",\"aw\" \n\t"                                 \
        ".align 3 \n\t"                                                 \
"_ZN14nsXPTCStubBase5Stub"#n"Ev: \n\t"                                  \
        ".quad  ._ZN14nsXPTCStubBase5Stub"#n"Ev,.TOC.@tocbase \n\t"     \
        ".previous \n\t"                                                \
        ".type  _ZN14nsXPTCStubBase5Stub"#n"Ev,@function \n\n"          \
"._ZN14nsXPTCStubBase5Stub"#n"Ev: \n\t"                                 \
                                                                        \
        ".elseif "#n" < 100 \n\t"                                       \
        ".globl _ZN14nsXPTCStubBase6Stub"#n"Ev \n\t"                    \
        ".section \".opd\",\"aw\" \n\t"                                 \
        ".align 3 \n\t"                                                 \
"_ZN14nsXPTCStubBase6Stub"#n"Ev: \n\t"                                  \
        ".quad  ._ZN14nsXPTCStubBase6Stub"#n"Ev,.TOC.@tocbase \n\t"     \
        ".previous \n\t"                                                \
        ".type  _ZN14nsXPTCStubBase6Stub"#n"Ev,@function \n\n"          \
"._ZN14nsXPTCStubBase6Stub"#n"Ev: \n\t"                                 \
                                                                        \
        ".elseif "#n" < 1000 \n\t"                                      \
        ".globl _ZN14nsXPTCStubBase7Stub"#n"Ev \n\t"                    \
        ".section \".opd\",\"aw\" \n\t"                                 \
        ".align 3 \n\t"                                                 \
"_ZN14nsXPTCStubBase7Stub"#n"Ev: \n\t"                                  \
        ".quad  ._ZN14nsXPTCStubBase7Stub"#n"Ev,.TOC.@tocbase \n\t"     \
        ".previous \n\t"                                                \
        ".type  _ZN14nsXPTCStubBase7Stub"#n"Ev,@function \n\n"          \
"._ZN14nsXPTCStubBase7Stub"#n"Ev: \n\t"                                 \
                                                                        \
        ".else  \n\t"                                                   \
        ".err   \"stub number "#n" >= 1000 not yet supported\"\n"       \
        ".endif \n\t"                                                   \
                                                                        \
        "li     11,"#n" \n\t"                                           \
        "b      SharedStub \n"                                          \
);
#endif

#define SENTINEL_ENTRY(n)                                               \
nsresult nsXPTCStubBase::Sentinel##n()                                  \
{                                                                       \
    NS_ERROR("nsXPTCStubBase::Sentinel called");                        \
    return NS_ERROR_NOT_IMPLEMENTED;                                    \
}

#include "xptcstubsdef.inc"
