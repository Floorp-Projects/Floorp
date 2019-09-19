/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Platform specific code to invoke XPCOM methods on native objects

#include "xptcprivate.h"

// The purpose of NS_InvokeByIndex() is to map a platform
// independent call to the platform ABI. To do that,
// NS_InvokeByIndex() has to determine the method to call via vtable
// access. The parameters for the method are read from the
// nsXPTCVariant* and prepared for the native ABI.
//
// Prior to POWER8, all 64-bit Power ISA systems used ELF v1 ABI, found
// here:
//   https://refspecs.linuxfoundation.org/ELF/ppc64/PPC-elf64abi.html
// and in particular:
//   https://refspecs.linuxfoundation.org/ELF/ppc64/PPC-elf64abi.html#FUNC-CALL
// Little-endian ppc64le, however, uses ELF v2 ABI, which is here:
//   http://openpowerfoundation.org/wp-content/uploads/resources/leabi/leabi-20170510.pdf
// and in particular section 2.2, page 22. However, most big-endian ppc64
// systems still use ELF v1, so this file should support both.

// 7 integral parameters are passed in registers, not including |this|
// (i.e., r3-r10, with r3 being |this|).
const uint32_t GPR_COUNT = 7;

// 13 floating point parameters are passed in registers, either single or
// double precision (i.e., f1-f13).
const uint32_t FPR_COUNT = 13;

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

extern "C" void invoke_copy_to_stack(uint64_t* gpregs, double* fpregs,
                                     uint32_t paramCount, nsXPTCVariant* s,
                                     uint64_t* d)
{
    uint32_t nr_gpr = 0u;
    uint32_t nr_fpr = 0u;
    uint64_t value = 0u;

    for (uint32_t i = 0; i < paramCount; i++, s++) {
        if (s->IsIndirect())
            value = (uint64_t) &s->val;
        else {
            switch (s->type) {
            case nsXPTType::T_FLOAT:                                break;
            case nsXPTType::T_DOUBLE:                               break;
            case nsXPTType::T_I8:     value = s->val.i8;            break;
            case nsXPTType::T_I16:    value = s->val.i16;           break;
            case nsXPTType::T_I32:    value = s->val.i32;           break;
            case nsXPTType::T_I64:    value = s->val.i64;           break;
            case nsXPTType::T_U8:     value = s->val.u8;            break;
            case nsXPTType::T_U16:    value = s->val.u16;           break;
            case nsXPTType::T_U32:    value = s->val.u32;           break;
            case nsXPTType::T_U64:    value = s->val.u64;           break;
            case nsXPTType::T_BOOL:   value = s->val.b;             break;
            case nsXPTType::T_CHAR:   value = s->val.c;             break;
            case nsXPTType::T_WCHAR:  value = s->val.wc;            break;
            default:                  value = (uint64_t) s->val.p;  break;
            }
        }

        if (!s->IsIndirect() && s->type == nsXPTType::T_DOUBLE) {
            if (nr_fpr < FPR_COUNT) {
                fpregs[nr_fpr++] = s->val.d;
                nr_gpr++;
            } else {
                *((double *)d) = s->val.d;
                d++;
            }
        }
        else if (!s->IsIndirect() && s->type == nsXPTType::T_FLOAT) {
            if (nr_fpr < FPR_COUNT) {
                // Single-precision floats are passed in FPRs too.
                fpregs[nr_fpr++] = s->val.f;
                nr_gpr++;
            } else {
#ifdef __LITTLE_ENDIAN__
                *((float *)d) = s->val.f;
#else
                // Big endian needs adjustment to point to the least
                // significant word.
                float* p = (float*)d;
                p++;
                *p = s->val.f;
#endif
                d++;
            }
        }
        else {
            if (nr_gpr < GPR_COUNT) {
                gpregs[nr_gpr++] = value;
            } else {
                *d++ = value;
            }
        }
    }
}

EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex, uint32_t paramCount,
                 nsXPTCVariant* params);
