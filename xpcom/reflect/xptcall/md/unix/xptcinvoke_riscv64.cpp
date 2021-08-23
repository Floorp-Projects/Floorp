/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Platform specific code to invoke XPCOM methods on native objects

#if defined(__riscv_float_abi_soft)
#  error "Not support soft float ABI"
#endif

#include "xptcprivate.h"

extern "C" void invoke_copy_to_stack(uint64_t* gpregs, double* fpregs,
                                     uint32_t paramCount, nsXPTCVariant* s,
                                     uint64_t* d) {
  static const uint32_t GPR_COUNT = 8;
  static const uint32_t FPR_COUNT = 8;

  uint32_t nr_gpr = 1;  // skip one GPR register for "this"
  uint32_t nr_fpr = 0;
  uint64_t value = 0;

  for (uint32_t i = 0; i < paramCount; i++, s++) {
    if (s->IsIndirect()) {
      value = (uint64_t)&s->val;
    } else {
      switch (s->type) {
        case nsXPTType::T_FLOAT:
          break;
        case nsXPTType::T_DOUBLE:
          break;
        case nsXPTType::T_I8:
          value = s->val.i8;
          break;
        case nsXPTType::T_I16:
          value = s->val.i16;
          break;
        case nsXPTType::T_I32:
          value = s->val.i32;
          break;
        case nsXPTType::T_I64:
          value = s->val.i64;
          break;
        case nsXPTType::T_U8:
          value = s->val.u8;
          break;
        case nsXPTType::T_U16:
          value = s->val.u16;
          break;
        case nsXPTType::T_U32:
          value = s->val.u32;
          break;
        case nsXPTType::T_U64:
          value = s->val.u64;
          break;
        case nsXPTType::T_BOOL:
          value = s->val.b;
          break;
        case nsXPTType::T_CHAR:
          value = s->val.c;
          break;
        case nsXPTType::T_WCHAR:
          value = s->val.wc;
          break;
        default:
          value = (uint64_t)s->val.p;
          break;
      }
    }

    if (!s->IsIndirect() && s->type == nsXPTType::T_DOUBLE) {
      if (nr_fpr < FPR_COUNT) {
        fpregs[nr_fpr++] = s->val.d;
      } else {
        *((double*)d) = s->val.d;
        d++;
      }
    } else if (!s->IsIndirect() && s->type == nsXPTType::T_FLOAT) {
      if (nr_fpr < FPR_COUNT) {
        // The value in %fa register is already prepared to
        // be retrieved as a float. Therefore, we pass the
        // value verbatim, as a double without conversion.
        fpregs[nr_fpr++] = s->val.d;
      } else {
        *((float*)d) = s->val.f;
        d++;
      }
    } else {
      if (nr_gpr < GPR_COUNT) {
        gpregs[nr_gpr++] = value;
      } else {
        *d++ = value;
      }
    }
  }
}

extern "C" nsresult _NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
                                      uint32_t paramCount,
                                      nsXPTCVariant* params);

EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex, uint32_t paramCount,
                 nsXPTCVariant* params) {
  return _NS_InvokeByIndex(that, methodIndex, paramCount, params);
}
