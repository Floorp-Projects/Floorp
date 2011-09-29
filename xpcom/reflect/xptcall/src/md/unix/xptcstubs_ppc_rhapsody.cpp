/* -*- Mode: C -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Mentovai <mark@moxienet.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "xptcprivate.h"
#include "xptiprivate.h"

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

extern "C" nsresult
PrepareAndDispatch(
  nsXPTCStubBase *self,
  PRUint32        methodIndex,
  PRUint32       *argsStack,
  PRUint32       *argsGPR,
  double         *argsFPR) {
#define PARAM_BUFFER_COUNT 16
#define PARAM_FPR_COUNT    13
#define PARAM_GPR_COUNT     7

  nsXPTCMiniVariant      paramBuffer[PARAM_BUFFER_COUNT];
  nsXPTCMiniVariant     *dispatchParams = NULL;
  const nsXPTMethodInfo *methodInfo;
  PRUint8                paramCount;
  PRUint8                i;
  nsresult               result         = NS_ERROR_FAILURE;
  PRUint32               argIndex       = 0;
  PRUint32               fprIndex       = 0;

  typedef struct {
    PRUint32 hi;
    PRUint32 lo;
  } DU;

  NS_ASSERTION(self, "no self");

  self->mEntry->GetMethodInfo(PRUint16(methodIndex), &methodInfo);
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
    PRUint32              theParam;

    if(argIndex < PARAM_GPR_COUNT)
      theParam =   argsGPR[argIndex];
    else
      theParam = argsStack[argIndex];

    if(param.IsOut() || !type.IsArithmetic())
      dp->val.p = (void *) theParam;
    else {
      switch(type) {
        case nsXPTType::T_I8:
          dp->val.i8  =   (PRInt8) theParam;
          break;
        case nsXPTType::T_I16:
          dp->val.i16 =  (PRInt16) theParam;
          break;
        case nsXPTType::T_I32:
          dp->val.i32 =  (PRInt32) theParam;
          break;
        case nsXPTType::T_U8:
          dp->val.u8  =  (PRUint8) theParam;
          break;
        case nsXPTType::T_U16:
          dp->val.u16 = (PRUint16) theParam;
          break;
        case nsXPTType::T_U32:
          dp->val.u32 = (PRUint32) theParam;
          break;
        case nsXPTType::T_I64:
        case nsXPTType::T_U64:
          ((DU *)dp)->hi = (PRUint32) theParam;
          if(++argIndex < PARAM_GPR_COUNT)
            ((DU *)dp)->lo = (PRUint32)   argsGPR[argIndex];
          else
            ((DU *)dp)->lo = (PRUint32) argsStack[argIndex];
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
    CallMethod((PRUint16)methodIndex, methodInfo, dispatchParams);

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
