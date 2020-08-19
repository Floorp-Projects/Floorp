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

// The AAPCS doesn't require argument widening, but Apple's calling convention
// does. If we are really fortunate, the compiler will clean up all the
// copying for us.
template<typename T>
inline uint64_t normalize_arg(T value) {
    return (uint64_t)value;
}

template<>
inline uint64_t normalize_arg(float value) {
    uint64_t result = 0;
    memcpy(&result, &value, sizeof(value));
    return result;
}

template<>
inline uint64_t normalize_arg(double value) {
    uint64_t result = 0;
    memcpy(&result, &value, sizeof(value));
    return result;
}

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
                             void* &stack_args,
                             T*     data)
{
    if (reg_args < reg_args_end) {
        *reg_args = normalize_arg(*data);
        reg_args++;
    } else {
        // According to the ABI, types that are smaller than 8 bytes are
        // passed in registers or 8-byte stack slots.  This rule is only
        // partially true on Apple platforms, where types smaller than 8
        // bytes occupy only the space they require on the stack and
        // their stack slot must be properly aligned.
#ifdef __APPLE__
        const size_t aligned_size = sizeof(T);
#else
        const size_t aligned_size = 8;
#endif
        // Ensure the pointer is aligned for the type
        uintptr_t addr = (reinterpret_cast<uintptr_t>(stack_args) + aligned_size - 1) & ~(aligned_size - 1);
        memcpy(reinterpret_cast<void*>(addr), data, sizeof(T));
        // Point the stack to the next slot.
        stack_args = reinterpret_cast<void*>(addr + aligned_size);
    }
}

extern "C" void
invoke_copy_to_stack(uint64_t* stk, uint64_t *end,
                     uint32_t paramCount, nsXPTCVariant* s)
{
    uint64_t* ireg_args = stk;
    uint64_t* ireg_end  = ireg_args + 8;
    // Pun on integer and floating-point registers being the same size.
    uint64_t* freg_args = ireg_end;
    uint64_t* freg_end  = freg_args + 8;
    void* stack_args = freg_end;

    // leave room for 'that' argument in x0
    ++ireg_args;

    for (uint32_t i = 0; i < paramCount; i++, s++) {
        if (s->IsIndirect()) {
            void* ptr = &s->val;
            alloc_arg(ireg_args, ireg_end, stack_args, &ptr);
        } else {
            switch (s->type) {
                case nsXPTType::T_FLOAT:
                    alloc_arg(freg_args, freg_end, stack_args, &s->val.f);
                    break;
                case nsXPTType::T_DOUBLE:
                    alloc_arg(freg_args, freg_end, stack_args, &s->val.d);
                    break;
                case nsXPTType::T_I8:
                    alloc_arg(ireg_args, ireg_end, stack_args, &s->val.i8);
                    break;
                case nsXPTType::T_I16:
                    alloc_arg(ireg_args, ireg_end, stack_args, &s->val.i16);
                    break;
                case nsXPTType::T_I32:
                    alloc_arg(ireg_args, ireg_end, stack_args, &s->val.i32);
                    break;
                case nsXPTType::T_I64:
                    alloc_arg(ireg_args, ireg_end, stack_args, &s->val.i64);
                    break;
                case nsXPTType::T_U8:
                    alloc_arg(ireg_args, ireg_end, stack_args, &s->val.u8);
                    break;
                case nsXPTType::T_U16:
                    alloc_arg(ireg_args, ireg_end, stack_args, &s->val.u16);
                    break;
                case nsXPTType::T_U32:
                    alloc_arg(ireg_args, ireg_end, stack_args, &s->val.u32);
                    break;
                case nsXPTType::T_U64:
                    alloc_arg(ireg_args, ireg_end, stack_args, &s->val.u64);
                    break;
                case nsXPTType::T_BOOL:
                    alloc_arg(ireg_args, ireg_end, stack_args, &s->val.b);
                    break;
                case nsXPTType::T_CHAR:
                    alloc_arg(ireg_args, ireg_end, stack_args, &s->val.c);
                    break;
                case nsXPTType::T_WCHAR:
                    alloc_arg(ireg_args, ireg_end, stack_args, &s->val.wc);
                    break;
                default:
                    // all the others are plain pointer types
                    alloc_arg(ireg_args, ireg_end, stack_args, &s->val.p);
                    break;
            }
        }
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
