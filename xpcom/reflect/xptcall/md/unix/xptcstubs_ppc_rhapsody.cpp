/* -*- Mode: C -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xptcprivate.h"

/* Under the Mac OS X PowerPC ABI, the first 8 integer and 13 floating point
 * parameters are delivered in registers and are not on the stack, although
 * stack space is allocated for them.  The integer parameters are delivered
 * in GPRs r3 through r10.  The first 8 words of the parameter area on the
 * stack shadow these registers.  A word will either be in a register or on
 * the stack, but not in both.  Although the first floating point parameters
 * are passed in floating point registers, GPR space and stack space is
 * reserved for them as well.
 *
 * SharedStub has passed pointers to the parameter section of the stack
 * and saved copies of the GPRs and FPRs used for parameter passing.  We
 * don't care about the first parameter (which is delivered here as the self
 * pointer), so SharedStub pointed us past that.  argsGPR thus points to GPR
 * r4 (corresponding to the first argument after the self pointer) and
 * argsStack points to the parameter section of the caller's stack frame
 * reserved for the same argument.  This way, it is possible to reference
 * either argsGPR or argsStack with the same index.
 *
 * Contrary to the assumption made by the previous implementation, the
 * Mac OS X PowerPC ABI doesn't impose any special alignment restrictions on
 * parameter sections of stacks.  Values that are 64 bits wide appear on the
 * stack without any special padding.
 *
 * See also xptcstubs_asm_ppc_darwin.s.m4:_SharedStub.
 *
 * ABI reference:
 * http://developer.apple.com/documentation/DeveloperTools/Conceptual/
 *  MachORuntime/PowerPCConventions/chapter_3_section_1.html */

extern "C" nsresult ATTRIBUTE_USED
PrepareAndDispatch(
  nsXPTCStubBase *self,
  uint32_t        methodIndex,
  uint32_t       *argsStack,
  uint32_t       *argsGPR,
  double         *argsFPR) {
#define PARAM_BUFFER_COUNT 16
#define PARAM_FPR_COUNT    13
#define PARAM_GPR_COUNT     7

  nsXPTCMiniVariant      paramBuffer[PARAM_BUFFER_COUNT];
  nsXPTCMiniVariant     *dispatchParams = nullptr;
  const nsXPTMethodInfo *methodInfo;
  uint8_t                paramCount;
  uint8_t                i;
  nsresult               result         = NS_ERROR_FAILURE;
  uint32_t               argIndex       = 0;
  uint32_t               fprIndex       = 0;

  typedef struct {
    uint32_t hi;
    uint32_t lo;
  } DU;

  NS_ASSERTION(self, "no self");

  self->mEntry->GetMethodInfo(uint16_t(methodIndex), &methodInfo);
  NS_ASSERTION(methodInfo, "no method info");

  paramCount = methodInfo->GetParamCount();

  if(paramCount > PARAM_BUFFER_COUNT) {
    dispatchParams = new nsXPTCMiniVariant[paramCount];
  }
  else {
    dispatchParams = paramBuffer;
  }
  NS_ASSERTION(dispatchParams,"no place for params");

  for(i = 0; i < paramCount; i++, argIndex++) {
    const nsXPTParamInfo &param = methodInfo->GetParam(i);
    const nsXPTType      &type  = param.GetType();
    nsXPTCMiniVariant    *dp    = &dispatchParams[i];
    uint32_t              theParam;

    if(argIndex < PARAM_GPR_COUNT)
      theParam =   argsGPR[argIndex];
    else
      theParam = argsStack[argIndex];

    if(param.IsOut() || !type.IsArithmetic())
      dp->val.p = (void *) theParam;
    else {
      switch(type) {
        case nsXPTType::T_I8:
          dp->val.i8  =   (int8_t) theParam;
          break;
        case nsXPTType::T_I16:
          dp->val.i16 =  (int16_t) theParam;
          break;
        case nsXPTType::T_I32:
          dp->val.i32 =  (int32_t) theParam;
          break;
        case nsXPTType::T_U8:
          dp->val.u8  =  (uint8_t) theParam;
          break;
        case nsXPTType::T_U16:
          dp->val.u16 = (uint16_t) theParam;
          break;
        case nsXPTType::T_U32:
          dp->val.u32 = (uint32_t) theParam;
          break;
        case nsXPTType::T_I64:
        case nsXPTType::T_U64:
          ((DU *)dp)->hi = (uint32_t) theParam;
          if(++argIndex < PARAM_GPR_COUNT)
            ((DU *)dp)->lo = (uint32_t)   argsGPR[argIndex];
          else
            ((DU *)dp)->lo = (uint32_t) argsStack[argIndex];
          break;
        case nsXPTType::T_BOOL:
          dp->val.b   =   (bool) theParam;
          break;
        case nsXPTType::T_CHAR:
          dp->val.c   =     (char) theParam;
          break;
        case nsXPTType::T_WCHAR:
          dp->val.wc  =  (wchar_t) theParam;
          break;
        case nsXPTType::T_FLOAT:
          if(fprIndex < PARAM_FPR_COUNT)
            dp->val.f = (float) argsFPR[fprIndex++];
          else
            dp->val.f = *(float *) &argsStack[argIndex];
          break;
        case nsXPTType::T_DOUBLE:
          if(fprIndex < PARAM_FPR_COUNT)
            dp->val.d = argsFPR[fprIndex++];
          else
            dp->val.d = *(double *) &argsStack[argIndex];
          argIndex++;
          break;
        default:
          NS_ERROR("bad type");
          break;
      }
    }
  }

  result = self->mOuter->
    CallMethod((uint16_t)methodIndex, methodInfo, dispatchParams);

  if(dispatchParams != paramBuffer)
    delete [] dispatchParams;

  return result;
}

#define STUB_ENTRY(n)
#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"
