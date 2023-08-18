/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, V. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xptcprivate.h"

extern "C" nsresult ATTRIBUTE_USED PrepareAndDispatch(nsXPTCStubBase* self,
                                                      uint32_t methodIndex,
                                                      uint64_t* args,
                                                      uint64_t* gpregs,
                                                      double* fpregs) {
  static const uint32_t GPR_COUNT = 8;
  static const uint32_t FPR_COUNT = 8;
  nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
  const nsXPTMethodInfo* info;

  self->mEntry->GetMethodInfo(uint16_t(methodIndex), &info);

  uint32_t paramCount = info->GetParamCount();
  const uint8_t indexOfJSContext = info->IndexOfJSContext();

  uint64_t* ap = args;
  uint32_t nr_gpr = 1;    // skip the arg which is 'self'
  uint32_t nr_fpr = 0;
  uint64_t value;

  for (uint32_t i = 0; i < paramCount; i++) {
    const nsXPTParamInfo& param = info->GetParam(i);
    const nsXPTType& type = param.GetType();
    nsXPTCMiniVariant* dp = &paramBuffer[i];

    if (i == indexOfJSContext) {
      if (nr_gpr < GPR_COUNT)
        nr_gpr++;
      else
        ap++;
    }

    if (!param.IsOut() && type == nsXPTType::T_DOUBLE) {
      if (nr_fpr < FPR_COUNT) {
        dp->val.d = fpregs[nr_fpr++];
      } else if (nr_gpr < GPR_COUNT) {
        memcpy(&dp->val.d, &gpregs[nr_gpr++], sizeof(dp->val.d));
      } else {
        memcpy(&dp->val.d, ap++, sizeof(dp->val.d));
      }
      continue;
    }

    if (!param.IsOut() && type == nsXPTType::T_FLOAT) {
      if (nr_fpr < FPR_COUNT) {
        memcpy(&dp->val.f, &fpregs[nr_fpr++], sizeof(dp->val.f));
      } else if (nr_gpr < GPR_COUNT) {
        memcpy(&dp->val.f, &gpregs[nr_gpr++], sizeof(dp->val.f));
      } else {
        memcpy(&dp->val.f, ap++, sizeof(dp->val.f));
      }
      continue;
    }

    if (nr_gpr < GPR_COUNT) {
      value = gpregs[nr_gpr++];
    } else {
      value = *ap++;
    }

    if (param.IsOut() || !type.IsArithmetic()) {
      dp->val.p = (void*)value;
      continue;
    }

    switch (type) {
      case nsXPTType::T_I8:
        dp->val.i8 = (int8_t)value;
        break;
      case nsXPTType::T_I16:
        dp->val.i16 = (int16_t)value;
        break;
      case nsXPTType::T_I32:
        dp->val.i32 = (int32_t)value;
        break;
      case nsXPTType::T_I64:
        dp->val.i64 = (int64_t)value;
        break;
      case nsXPTType::T_U8:
        dp->val.u8 = (uint8_t)value;
        break;
      case nsXPTType::T_U16:
        dp->val.u16 = (uint16_t)value;
        break;
      case nsXPTType::T_U32:
        dp->val.u32 = (uint32_t)value;
        break;
      case nsXPTType::T_U64:
        dp->val.u64 = (uint64_t)value;
        break;
      case nsXPTType::T_BOOL:
        dp->val.b = (bool)(uint8_t)value;
        break;
      case nsXPTType::T_CHAR:
        dp->val.c = (char)value;
        break;
      case nsXPTType::T_WCHAR:
        dp->val.wc = (wchar_t)value;
        break;
      default:
        NS_ERROR("bad type");
        break;
    }
  }

  nsresult result = self->mOuter->CallMethod((uint16_t)methodIndex, info,
                                             paramBuffer);
  return result;
}

// Load $t6 with the constant 'n' and branch to SharedStub().
// clang-format off
#define STUB_ENTRY(n)                                                 \
  __asm__(                                                            \
      ".text\n\t"                                                     \
      ".if "#n" < 10 \n\t"                                            \
      ".globl  _ZN14nsXPTCStubBase5Stub"#n"Ev \n\t"                   \
      ".hidden _ZN14nsXPTCStubBase5Stub"#n"Ev \n\t"                   \
      ".type   _ZN14nsXPTCStubBase5Stub"#n"Ev,@function \n\n"         \
      "_ZN14nsXPTCStubBase5Stub"#n"Ev: \n\t"                          \
      ".elseif "#n" < 100 \n\t"                                       \
      ".globl  _ZN14nsXPTCStubBase6Stub"#n"Ev \n\t"                   \
      ".hidden _ZN14nsXPTCStubBase6Stub"#n"Ev \n\t"                   \
      ".type   _ZN14nsXPTCStubBase6Stub"#n"Ev,@function \n\n"         \
      "_ZN14nsXPTCStubBase6Stub"#n"Ev: \n\t"                          \
      ".elseif "#n" < 1000 \n\t"                                      \
      ".globl  _ZN14nsXPTCStubBase7Stub"#n"Ev \n\t"                   \
      ".hidden _ZN14nsXPTCStubBase7Stub"#n"Ev \n\t"                   \
      ".type   _ZN14nsXPTCStubBase7Stub"#n"Ev,@function \n\n"         \
      "_ZN14nsXPTCStubBase7Stub"#n"Ev: \n\t"                          \
      ".else  \n\t"                                                   \
      ".err   \"stub number "#n" >= 1000 not yet supported\"\n"       \
      ".endif \n\t"                                                   \
      "li.d   $t6, "#n" \n\t"                                         \
      "b      SharedStub \n"                                          \
      ".if "#n" < 10 \n\t"                                            \
      ".size   _ZN14nsXPTCStubBase5Stub"#n"Ev,.-_ZN14nsXPTCStubBase5Stub"#n"Ev\n\t" \
      ".elseif "#n" < 100 \n\t"                                                     \
      ".size   _ZN14nsXPTCStubBase6Stub"#n"Ev,.-_ZN14nsXPTCStubBase6Stub"#n"Ev\n\t" \
      ".else \n\t"                                                                  \
      ".size   _ZN14nsXPTCStubBase7Stub"#n"Ev,.-_ZN14nsXPTCStubBase7Stub"#n"Ev\n\t" \
      ".endif"                                                                      \
);
// clang-format on

#define SENTINEL_ENTRY(n)                         \
  nsresult nsXPTCStubBase::Sentinel##n() {        \
    NS_ERROR("nsXPTCStubBase::Sentinel called");  \
    return NS_ERROR_NOT_IMPLEMENTED;              \
  }

#include "xptcstubsdef.inc"
