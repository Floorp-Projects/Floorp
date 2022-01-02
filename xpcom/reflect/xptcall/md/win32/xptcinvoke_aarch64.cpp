/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

#if !defined(_MSC_VER) && !defined(_M_ARM64)
#error "This code is for AArch64 Windows only"
#endif

/* "Procedure Call Standard for the ARM 64-bit Architecture" document, sections
 * "5.4 Parameter Passing" and "6.1.2 Procedure Calling" contain all the
 * needed information.
 *
 * http://infocenter.arm.com/help/topic/com.arm.doc.ihi0042d/IHI0042D_aapcs.pdf
 *
 * Windows follows this document, see:
 *
 * https://docs.microsoft.com/en-us/cpp/build/arm64-windows-abi-conventions
 */

/*
 * Allocation of integer function arguments initially to registers r1-r7
 * and then to stack. Handling of 'that' argument which goes to register r0
 * is handled separately and does not belong here.
 *
 * 'ireg_args'  - pointer to the current position in the buffer,
 *                corresponding to the register arguments
 * 'stack_args' - pointer to the current position in the buffer,
 *                corresponding to the arguments on stack
 * 'end'        - pointer to the end of the registers argument
 *                buffer.
 */
static inline void alloc_word(uint64_t* &ireg_args,
                              uint64_t* &stack_args,
                              uint64_t* end,
                              uint64_t  data)
{
    if (ireg_args < end) {
        *ireg_args = data;
        ireg_args++;
    } else {
        *stack_args = data;
        stack_args++;
    }
}

static inline void alloc_double(double* &freg_args,
                                uint64_t* &stack_args,
                                double* end,
                                double  data)
{
    if (freg_args < end) {
        *freg_args = data;
        freg_args++;
    } else {
        memcpy(stack_args, &data, sizeof(data));
        stack_args++;
    }
}

static inline void alloc_float(double* &freg_args,
                               uint64_t* &stack_args,
                               double* end,
                               float  data)
{
    if (freg_args < end) {
        memcpy(freg_args, &data, sizeof(data));
        freg_args++;
    } else {
        memcpy(stack_args, &data, sizeof(data));
        stack_args++;
    }
}


extern "C" void
invoke_copy_to_stack(uint64_t* stk, uint64_t *end,
                     uint32_t paramCount, nsXPTCVariant* s)
{
    uint64_t *ireg_args = stk;
    uint64_t *ireg_end  = ireg_args + 8;
    double *freg_args = (double *)ireg_end;
    double *freg_end  = freg_args + 8;
    uint64_t *stack_args = (uint64_t *)freg_end;

    // leave room for 'that' argument in x0
    ++ireg_args;

    for (uint32_t i = 0; i < paramCount; i++, s++) {
        uint64_t word;

        if (s->IsIndirect()) {
            word = (uint64_t)&s->val;
        } else {
            // According to the ABI, integral types that are smaller than 8
            // bytes are to be passed in 8-byte registers or 8-byte stack
            // slots.
            switch (s->type) {
                case nsXPTType::T_FLOAT:
                    alloc_float(freg_args, stack_args, freg_end, s->val.f);
                    continue;
                case nsXPTType::T_DOUBLE:
                    alloc_double(freg_args, stack_args, freg_end, s->val.d);
                    continue;
                case nsXPTType::T_I8:    word = s->val.i8;  break;
                case nsXPTType::T_I16:   word = s->val.i16; break;
                case nsXPTType::T_I32:   word = s->val.i32; break;
                case nsXPTType::T_I64:   word = s->val.i64; break;
                case nsXPTType::T_U8:    word = s->val.u8;  break;
                case nsXPTType::T_U16:   word = s->val.u16; break;
                case nsXPTType::T_U32:   word = s->val.u32; break;
                case nsXPTType::T_U64:   word = s->val.u64; break;
                case nsXPTType::T_BOOL:  word = s->val.b;   break;
                case nsXPTType::T_CHAR:  word = s->val.c;   break;
                case nsXPTType::T_WCHAR: word = s->val.wc;  break;
                default:
                    // all the others are plain pointer types
                    word = reinterpret_cast<uint64_t>(s->val.p);
                    break;
            }
        }

        alloc_word(ireg_args, stack_args, ireg_end, word);
    }
}

extern "C" nsresult
XPTC__InvokebyIndex(nsISupports* that, uint32_t methodIndex,
                    uint32_t paramCount, nsXPTCVariant* params);

extern "C"
EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
                   uint32_t paramCount, nsXPTCVariant* params)
{
    return XPTC__InvokebyIndex(that, methodIndex, paramCount, params);
}
