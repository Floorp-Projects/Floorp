/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xptcprivate.h"

#ifndef __AARCH64EL__
#error "Only little endian compatibility was tested"
#endif

/*
 * This is for AArch64 ABI
 *
 * When we're called, the "gp" registers are stored in gprData and
 * the "fp" registers are stored in fprData. Each array has 8 regs
 * but first reg in gprData is a placeholder for 'self'.
 */
extern "C" nsresult ATTRIBUTE_USED
PrepareAndDispatch(nsXPTCStubBase* self, uint32_t methodIndex, uint64_t* args,
                   uint64_t *gprData, double *fprData)
{
#define PARAM_BUFFER_COUNT        16
#define PARAM_GPR_COUNT            8
#define PARAM_FPR_COUNT            8

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = NULL;
    const nsXPTMethodInfo* info;

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(uint16_t(methodIndex), &info);
    NS_ASSERTION(info,"no method info");

    uint32_t paramCount = info->GetParamCount();

    // setup variant array pointer
    if (paramCount > PARAM_BUFFER_COUNT) {
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    } else {
        dispatchParams = paramBuffer;
    }
    NS_ASSERTION(dispatchParams,"no place for params");

    const uint8_t indexOfJSContext = info->IndexOfJSContext();

    uint64_t* ap = args;
    uint32_t next_gpr = 1; // skip first arg which is 'self'
    uint32_t next_fpr = 0;
    for (uint32_t i = 0; i < paramCount; i++) {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &dispatchParams[i];

        if (i == indexOfJSContext) {
            if (next_gpr < PARAM_GPR_COUNT)
                next_gpr++;
            else
                ap++;
        }

        if (param.IsOut() || !type.IsArithmetic()) {
            if (next_gpr < PARAM_GPR_COUNT) {
                dp->val.p = (void*)gprData[next_gpr++];
            } else {
                dp->val.p = (void*)*ap++;
            }
            continue;
        }

        switch (type) {
            case nsXPTType::T_I8:
                if (next_gpr < PARAM_GPR_COUNT) {
                    dp->val.i8  = (int8_t)gprData[next_gpr++];
                } else {
                    dp->val.i8  = (int8_t)*ap++;
                }
                break;

            case nsXPTType::T_I16:
                if (next_gpr < PARAM_GPR_COUNT) {
                    dp->val.i16  = (int16_t)gprData[next_gpr++];
                } else {
                    dp->val.i16  = (int16_t)*ap++;
                }
                break;

            case nsXPTType::T_I32:
                if (next_gpr < PARAM_GPR_COUNT) {
                    dp->val.i32  = (int32_t)gprData[next_gpr++];
                } else {
                    dp->val.i32  = (int32_t)*ap++;
                }
                break;

            case nsXPTType::T_I64:
                if (next_gpr < PARAM_GPR_COUNT) {
                    dp->val.i64  = (int64_t)gprData[next_gpr++];
                } else {
                    dp->val.i64  = (int64_t)*ap++;
                }
                break;

            case nsXPTType::T_U8:
                if (next_gpr < PARAM_GPR_COUNT) {
                    dp->val.u8  = (uint8_t)gprData[next_gpr++];
                } else {
                    dp->val.u8  = (uint8_t)*ap++;
                }
                break;

            case nsXPTType::T_U16:
                if (next_gpr < PARAM_GPR_COUNT) {
                    dp->val.u16  = (uint16_t)gprData[next_gpr++];
                } else {
                    dp->val.u16  = (uint16_t)*ap++;
                }
                break;

            case nsXPTType::T_U32:
                if (next_gpr < PARAM_GPR_COUNT) {
                    dp->val.u32  = (uint32_t)gprData[next_gpr++];
                } else {
                    dp->val.u32  = (uint32_t)*ap++;
                }
                break;

            case nsXPTType::T_U64:
                if (next_gpr < PARAM_GPR_COUNT) {
                    dp->val.u64  = (uint64_t)gprData[next_gpr++];
                } else {
                    dp->val.u64  = (uint64_t)*ap++;
                }
                break;

            case nsXPTType::T_FLOAT:
                if (next_fpr < PARAM_FPR_COUNT) {
                    memcpy(&dp->val.f, &fprData[next_fpr++], sizeof(dp->val.f));
                } else {
                    memcpy(&dp->val.f, ap++, sizeof(dp->val.f));
                }
                break;

            case nsXPTType::T_DOUBLE:
                if (next_fpr < PARAM_FPR_COUNT) {
                    memcpy(&dp->val.d, &fprData[next_fpr++], sizeof(dp->val.d));
                } else {
                    memcpy(&dp->val.d, ap++, sizeof(dp->val.d));
                }
                break;

            case nsXPTType::T_BOOL:
                if (next_gpr < PARAM_GPR_COUNT) {
                    dp->val.b  = (bool)(uint8_t)gprData[next_gpr++];
                } else {
                    dp->val.b  = (bool)(uint8_t)*ap++;
                }
                break;

            case nsXPTType::T_CHAR:
                if (next_gpr < PARAM_GPR_COUNT) {
                    dp->val.c  = (char)gprData[next_gpr++];
                } else {
                    dp->val.c  = (char)*ap++;
                }
                break;

            case nsXPTType::T_WCHAR:
                if (next_gpr < PARAM_GPR_COUNT) {
                    dp->val.wc  = (wchar_t)gprData[next_gpr++];
                } else {
                    dp->val.wc  = (wchar_t)*ap++;
                }
                break;

            default:
                NS_ASSERTION(0, "bad type");
                break;
        }
    }

    nsresult result = self->mOuter->CallMethod((uint16_t)methodIndex, info,
                                               dispatchParams);

    if (dispatchParams != paramBuffer) {
        delete [] dispatchParams;
    }

    return result;
}

// Load w17 with the constant 'n' and branch to SharedStub().
# define STUB_ENTRY(n)                                                  \
    __asm__ (                                                           \
            ".section \".text\" \n\t"                                   \
            ".align 2\n\t"                                              \
            ".if "#n" < 10 \n\t"                                        \
            ".globl  _ZN14nsXPTCStubBase5Stub"#n"Ev \n\t"               \
            ".hidden _ZN14nsXPTCStubBase5Stub"#n"Ev \n\t"               \
            ".type   _ZN14nsXPTCStubBase5Stub"#n"Ev,@function \n\n"     \
            "_ZN14nsXPTCStubBase5Stub"#n"Ev: \n\t"                      \
            ".elseif "#n" < 100 \n\t"                                   \
            ".globl  _ZN14nsXPTCStubBase6Stub"#n"Ev \n\t"               \
            ".hidden _ZN14nsXPTCStubBase6Stub"#n"Ev \n\t"               \
            ".type   _ZN14nsXPTCStubBase6Stub"#n"Ev,@function \n\n"     \
            "_ZN14nsXPTCStubBase6Stub"#n"Ev: \n\t"                      \
            ".elseif "#n" < 1000 \n\t"                                  \
            ".globl  _ZN14nsXPTCStubBase7Stub"#n"Ev \n\t"               \
            ".hidden _ZN14nsXPTCStubBase7Stub"#n"Ev \n\t"               \
            ".type   _ZN14nsXPTCStubBase7Stub"#n"Ev,@function \n\n"     \
            "_ZN14nsXPTCStubBase7Stub"#n"Ev: \n\t"                      \
            ".else  \n\t"                                               \
            ".err   \"stub number "#n" >= 1000 not yet supported\"\n"   \
            ".endif \n\t"                                               \
            "mov    w17,#"#n" \n\t"                                     \
            "b      SharedStub \n"                                      \
);

#define SENTINEL_ENTRY(n)                              \
    nsresult nsXPTCStubBase::Sentinel##n()             \
{                                                      \
    NS_ASSERTION(0,"nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED;                   \
}

#include "xptcstubsdef.inc"
