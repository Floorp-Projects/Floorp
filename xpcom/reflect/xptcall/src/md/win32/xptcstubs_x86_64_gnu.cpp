/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xptcprivate.h"
#include "xptiprivate.h"

/*
 * This is for Windows 64 bit (x86_64) using GCC syntax
 * Code was copied from the MSVC version.
 */

#if !defined(_AMD64_) || !defined(__GNUC__)
#  error xptcstubs_x86_64_gnu.cpp being used unexpectedly
#endif

extern "C" nsresult
PrepareAndDispatch(nsXPTCStubBase * self, uint32_t methodIndex,
                   uint64_t * args, uint64_t * gprData, double *fprData)
{
#define PARAM_BUFFER_COUNT  16
//
// "this" pointer is first parameter, so parameter count is 3.
//
#define PARAM_GPR_COUNT   3
#define PARAM_FPR_COUNT   3

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = NULL;
    const nsXPTMethodInfo* info = NULL;
    uint8_t paramCount;
    uint8_t i;
    nsresult result = NS_ERROR_FAILURE;

    NS_ASSERTION(self, "no self");

    self->mEntry->GetMethodInfo(uint16_t(methodIndex), &info);
    NS_ASSERTION(info, "no method info");

    paramCount = info->GetParamCount();

    //
    // setup variant array pointer
    //

    if(paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;

    NS_ASSERTION(dispatchParams,"no place for params");

    uint64_t* ap = args;
    uint32_t iCount = 0;

    for(i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &dispatchParams[i];

        if(param.IsOut() || !type.IsArithmetic())
        {
            if (iCount < PARAM_GPR_COUNT)
                dp->val.p = (void*)gprData[iCount++];
            else
                dp->val.p = (void*)*ap++;

            continue;
        }
        // else
        switch(type)
        {
        case nsXPTType::T_I8:
           if (iCount < PARAM_GPR_COUNT)
              dp->val.i8  = (int8_t)gprData[iCount++];
           else
              dp->val.i8  = *((int8_t*)ap++);
           break;

        case nsXPTType::T_I16:
            if (iCount < PARAM_GPR_COUNT)
               dp->val.i16  = (int16_t)gprData[iCount++];
            else
               dp->val.i16  = *((int16_t*)ap++);
            break;

        case nsXPTType::T_I32:
            if (iCount < PARAM_GPR_COUNT)
               dp->val.i32  = (int32_t)gprData[iCount++];
            else
               dp->val.i32  = *((int32_t*)ap++);
            break;

        case nsXPTType::T_I64:
            if (iCount < PARAM_GPR_COUNT)
               dp->val.i64  = (int64_t)gprData[iCount++];
            else
               dp->val.i64  = *((int64_t*)ap++);
            break;

        case nsXPTType::T_U8:
            if (iCount < PARAM_GPR_COUNT)
               dp->val.u8  = (uint8_t)gprData[iCount++];
            else
               dp->val.u8  = *((uint8_t*)ap++);
            break;

        case nsXPTType::T_U16:
            if (iCount < PARAM_GPR_COUNT)
               dp->val.u16  = (uint16_t)gprData[iCount++];
            else
                dp->val.u16  = *((uint16_t*)ap++);
            break;

        case nsXPTType::T_U32:
            if (iCount < PARAM_GPR_COUNT)
               dp->val.u32  = (uint32_t)gprData[iCount++];
            else
               dp->val.u32  = *((uint32_t*)ap++);
            break;

        case nsXPTType::T_U64:
            if (iCount < PARAM_GPR_COUNT)
               dp->val.u64  = (uint64_t)gprData[iCount++];
            else
               dp->val.u64  = *((uint64_t*)ap++);
            break;

        case nsXPTType::T_FLOAT:
             if (iCount < PARAM_FPR_COUNT)
                dp->val.f  = (float)fprData[iCount++];
             else
                dp->val.f  = *((float*)ap++);
             break;

        case nsXPTType::T_DOUBLE:
              if (iCount < PARAM_FPR_COUNT)
                 dp->val.d  = (double)fprData[iCount++];
              else
                 dp->val.d  = *((double*)ap++);
              break;

        case nsXPTType::T_BOOL:
           if (iCount < PARAM_GPR_COUNT)
              dp->val.b  = (bool)gprData[iCount++];
           else
              dp->val.b  = *((bool*)ap++);
           break;

        case nsXPTType::T_CHAR:
           if (iCount < PARAM_GPR_COUNT)
              dp->val.c  = (char)gprData[iCount++];
           else
              dp->val.c  = *((char*)ap++);
           break;

        case nsXPTType::T_WCHAR:
           if (iCount < PARAM_GPR_COUNT)
              dp->val.wc  = (wchar_t)gprData[iCount++];
           else
              dp->val.wc  = *((wchar_t*)ap++);
           break;

        default:
            NS_ASSERTION(0, "bad type");
            break;
        }
    }

    result = self->mOuter->CallMethod((uint16_t)methodIndex, info, dispatchParams);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

__asm__ (
  ".text\n"
  ".intel_syntax noprefix\n"            /* switch to Intel syntax to look like the MSVC assembly */
  ".globl SharedStub\n"
  ".def SharedStub ; .scl 3 ; .type 46 ; .endef \n"
  "SharedStub:\n"
  "sub     rsp, 104\n"

  /* rcx is this pointer.  Need backup for optimized build */

  "mov      qword ptr [rsp+88], rcx\n"

  /*
   * fist 4 parameters (1st is "this" pointer) are passed in registers.
   */

  /* for floating value */

  "movsd   qword ptr [rsp+64], xmm1\n"
  "movsd   qword ptr [rsp+72], xmm2\n"
  "movsd   qword ptr [rsp+80], xmm3\n"

  /* for integer value */

  "mov     qword ptr [rsp+40], rdx\n"
  "mov     qword ptr [rsp+48], r8\n"
  "mov     qword ptr [rsp+56], r9\n"

  /*
   * Call PrepareAndDispatch function
   */

  /* 5th parameter (floating parameters) of PrepareAndDispatch */

  "lea     r9, qword ptr [rsp+64]\n"
  "mov     qword ptr [rsp+32], r9\n"

  /* 4th parameter (normal parameters) of PrepareAndDispatch */

  "lea     r9, qword ptr [rsp+40]\n"

  /* 3rd parameter (pointer to args on stack) */

  "lea     r8, qword ptr [rsp+40+104]\n"

  /* 2nd parameter (vtbl_index) */

  "mov     rdx, r11\n"

  /* 1st parameter (this) (rcx) */

  "call    PrepareAndDispatch\n"

  /* restore rcx */

  "mov     rcx, qword ptr [rsp+88]\n"

  /*
   * clean up register
   */

  "add     rsp, 104+8\n"

  /* set return address */

  "mov     rdx, qword ptr [rsp-8]\n"

  /* simulate __stdcall return */

  "jmp     rdx\n"

  /* back to AT&T syntax */
  ".att_syntax\n"
);

#define STUB_ENTRY(n) \
asm(".intel_syntax noprefix\n" /* this is in intel syntax */ \
    ".text\n" \
    ".align 2\n" \
    ".if        " #n " < 10\n" \
    ".globl       _ZN14nsXPTCStubBase5Stub" #n "Ev@4\n" \
    ".def         _ZN14nsXPTCStubBase5Stub" #n "Ev@4\n" \
    ".scl         3\n" /* private */ \
    ".type        46\n" /* function returning unsigned int */ \
    ".endef\n" \
    "_ZN14nsXPTCStubBase5Stub" #n "Ev@4:\n" \
    ".elseif    " #n " < 100\n" \
    ".globl       _ZN14nsXPTCStubBase6Stub" #n "Ev@4\n" \
    ".def         _ZN14nsXPTCStubBase6Stub" #n "Ev@4\n" \
    ".scl         3\n" /* private */\
    ".type        46\n" /* function returning unsigned int */ \
    ".endef\n" \
    "_ZN14nsXPTCStubBase6Stub" #n "Ev@4:\n" \
    ".elseif    " #n " < 1000\n" \
    ".globl       _ZN14nsXPTCStubBase7Stub" #n "Ev@4\n" \
    ".def         _ZN14nsXPTCStubBase7Stub" #n "Ev@4\n" \
    ".scl         3\n" /* private */ \
    ".type        46\n" /* function returning unsigned int */ \
    ".endef\n" \
    "_ZN14nsXPTCStubBase7Stub" #n "Ev@4:\n" \
    ".else\n" \
    ".err       \"stub number " #n " >= 1000 not yet supported\"\n" \
    ".endif\n" \
    "mov          r11, " #n "\n" \
    "jmp          SharedStub\n" \
    ".att_syntax\n" /* back to AT&T syntax */ \
    "");

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ASSERTION(0,"nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"

