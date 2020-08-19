/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

#if !defined(__aarch64__)
#error "This code is for Linux AArch64 only."
#endif


/* "Procedure Call Standard for the ARM 64-bit Architecture" document, sections
 * "5.4 Parameter Passing" and "6.1.2 Procedure Calling" contain all the
 * needed information.
 *
 * http://infocenter.arm.com/help/topic/com.arm.doc.ihi0042d/IHI0042D_aapcs.pdf
 */

#ifndef __AARCH64EL__
#error "Only little endian compatibility was tested"
#endif

/*
 * Allocation of function arguments to their appropriate place in registers
 * if possible and then to the stack.  Handling of 'that' argument which
 * goes to register r0 is handled separately and does not belong here.
 *
 * Note that we are handling integer arguments and floating-point arguments
 * identically, depending on which register area is passed to this function.
 *
 * 'reg_args'     - pointer to the current position in the buffer,
 *                  corresponding to the register arguments.
 * 'reg_args_end' - pointer to the end of the registers argument
 *                  buffer.
 * 'stack_args'   - pointer to the current position in the buffer,
 *                  corresponding to the arguments on stack.
 * 'data'         - typed data to put on the stack.
 */
template<typename T>
static inline void alloc_arg(uint64_t* &reg_args,
                             uint64_t* reg_args_end,
                             uint64_t* &stack_args,
                             T*     data)
{
    if (reg_args < reg_args_end) {
        memcpy(reg_args, data, sizeof(T));
        reg_args++;
    } else {
        memcpy(stack_args, data, sizeof(T));
        // Allocate a word-sized stack slot no matter what.
        stack_args++;
    }
}

extern "C" void
invoke_copy_to_stack(uint64_t* stk, uint64_t *end,
                     uint32_t paramCount, nsXPTCVariant* s)
{
    uint64_t *ireg_args = stk;
    uint64_t *ireg_end  = ireg_args + 8;
    // Pun on integer and floating-point registers being the same size.
    uint64_t *freg_args = ireg_end;
    uint64_t *freg_end  = freg_args + 8;
    uint64_t *stack_args = freg_end;

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
                    alloc_arg(freg_args, freg_end, stack_args, &s->val.f);
                    continue;
                case nsXPTType::T_DOUBLE:
                    alloc_arg(freg_args, freg_end, stack_args, &s->val.d);
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

        alloc_arg(ireg_args, ireg_end, stack_args, &word);
    }
}

extern "C" nsresult _NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
                                      uint32_t paramCount, nsXPTCVariant* params);

EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
                 uint32_t paramCount, nsXPTCVariant* params)
{
    return _NS_InvokeByIndex(that, methodIndex, paramCount, params);
}
