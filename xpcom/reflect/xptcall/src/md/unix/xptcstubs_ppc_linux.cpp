/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"

/*
        For Linux/PPC, the first 8 integral and the first 8 f.p. parameters 
        arrive in a separate chunk of data that has been loaded from the registers. 
        The args pointer has been set to the start of the parameters BEYOND the ones
        arriving in registers.
*/

extern "C" nsresult
PrepareAndDispatch(nsXPTCStubBase* self, PRUint32 methodIndex, PRUint32* args, PRUint32 *gprData, double *fprData)
{

#define PARAM_BUFFER_COUNT     16
#define PARAM_GPR_COUNT         8
#define PARAM_FPR_COUNT         8

  nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
  nsXPTCMiniVariant* dispatchParams = NULL;
  nsIInterfaceInfo* iface_info = NULL;
  const nsXPTMethodInfo* info;
  PRUint8 paramCount;
  PRUint8 i;
  nsresult result = NS_ERROR_FAILURE;

  NS_ASSERTION(self,"no self");

  self->GetInterfaceInfo(&iface_info);
  NS_ASSERTION(iface_info,"no interface info");

  iface_info->GetMethodInfo(PRUint16(methodIndex), &info);
  NS_ASSERTION(info,"no interface info");

  paramCount = info->GetParamCount();

  // setup variant array pointer
  if(paramCount > PARAM_BUFFER_COUNT)
    dispatchParams = new nsXPTCMiniVariant[paramCount];
  else
    dispatchParams = paramBuffer;
  NS_ASSERTION(dispatchParams,"no place for params");

  PRUint32* ap = args;
  PRUint32 gprCount = 1; //skip one GPR reg
  PRUint32 fprCount = 0;
  for(i = 0; i < paramCount; i++) {
    const nsXPTParamInfo& param = info->GetParam(i);
    const nsXPTType& type = param.GetType();
    nsXPTCMiniVariant* dp = &dispatchParams[i];

    if(param.IsOut() || !type.IsArithmetic()) {
      if (gprCount < PARAM_GPR_COUNT) {
        dp->val.p = (void*) *gprData++;
        gprCount++;
      }
      else {
        dp->val.p   = (void*) *ap++;
      }
      continue;
    }
    // else
    switch(type) {
    case nsXPTType::T_I8:
      if (gprCount < PARAM_GPR_COUNT) {
        dp->val.i8 = (PRInt8) *gprData++;
        gprCount++;
      }
      else
        dp->val.i8   = (PRInt8) *ap++;
      break;

    case nsXPTType::T_I16:
      if (gprCount < PARAM_GPR_COUNT) {
        dp->val.i16 = (PRInt16) *gprData++;
        gprCount++;
      }
      else
        dp->val.i16   = (PRInt16) *ap++;
      break;

    case nsXPTType::T_I32:
      if (gprCount < PARAM_GPR_COUNT)
        {
          dp->val.i32 = (PRInt32) *gprData++;
          gprCount++;
        }
      else
        dp->val.i32   = (PRInt32) *ap++;
      break;

    case nsXPTType::T_I64:
      if (gprCount & 1) gprCount++;
      if ((gprCount + 1) < PARAM_GPR_COUNT)
        {
          dp->val.i64 = *(PRInt64*) gprData;
          gprData += 2;
          gprCount++;
        }
      else
        {
          if ((PRUint32) ap & 4) ap++;
          dp->val.i64 = *(PRInt64*) ap;
          ap += 2;
        }
      break;

    case nsXPTType::T_U8:
      if (gprCount < PARAM_GPR_COUNT)
        {
          dp->val.u8 = (PRUint8) *gprData++;
          gprCount++;
        }
      else
        dp->val.u8   = (PRUint8) *ap++;
      break;

    case nsXPTType::T_U16:
      if (gprCount < PARAM_GPR_COUNT)
        {
          dp->val.u16 = (PRUint16) *gprData++;
          gprCount++;
        }
      else
        dp->val.u16   = (PRUint16) *ap++;
      break;

    case nsXPTType::T_U32:
      if (gprCount < PARAM_GPR_COUNT)
        {
          dp->val.u32 = (PRUint32) *gprData++;
          gprCount++;
        }
      else
        dp->val.u32   = (PRUint32) *ap++;
      break;

    case nsXPTType::T_U64:
      if (gprCount & 1) gprCount++;
      if ((gprCount + 1) < PARAM_GPR_COUNT)
        {
          dp->val.u64 = *(PRUint64*) gprData;
          gprData += 2;
          gprCount++;
        }
      else
        {
          if ((PRUint32) ap & 4) ap++;
          dp->val.u64 = *(PRUint64*) ap;
          ap += 2;
        }
      break;

    case nsXPTType::T_FLOAT:
      if (fprCount < PARAM_FPR_COUNT)
        {
          dp->val.f = (float) *fprData++;
          fprCount++;
        }
      else
        dp->val.f = *(float*) ap++;
      break;

    case nsXPTType::T_DOUBLE:
      if (fprCount < PARAM_FPR_COUNT)
        {
          dp->val.d = *fprData++;
          fprCount++;
        }
      else
        {
          if ((PRUint32) ap & 4) ap++;
          dp->val.d = *(double*) ap;
          ap += 2;
        }
      break;

    case nsXPTType::T_BOOL:
      if (gprCount < PARAM_GPR_COUNT)
        {
          dp->val.b = (PRBool) *gprData++;
          gprCount++;
        }
      else
        dp->val.b   = (PRBool) *ap++;
      break;

    case nsXPTType::T_CHAR:
      if (gprCount < PARAM_GPR_COUNT)
        {
          dp->val.c = (char) *gprData++;
          gprCount++;
        }
      else
        dp->val.c   = (char) *ap++;
      break;

    case nsXPTType::T_WCHAR:
      if (gprCount < PARAM_GPR_COUNT)
        {
          dp->val.wc = (wchar_t) *gprData++;
          gprCount++;
        }
      else
        dp->val.wc   = (wchar_t) *ap++;
      break;

    default:
      NS_ASSERTION(0, "bad type");
      break;
    }
  }

  result = self->CallMethod((PRUint16) methodIndex, info, dispatchParams);

  NS_RELEASE(iface_info);

  if(dispatchParams != paramBuffer)
    delete [] dispatchParams;

  return result;
}

// We give a bogus prototype to generate the correct glue code. We
// actually pass the method index in r12 (an "unused" register),
// because r4 is used to pass the first parameter.
extern "C" nsresult SharedStub(void);

#define STUB_ENTRY(n)                \
nsresult nsXPTCStubBase::Stub##n()   \
{                                    \
   __asm__(                          \
   "\tli 12,"#n"\n"                  \
   );                                \
   return SharedStub();              \
}

#define SENTINEL_ENTRY(n)                            \
nsresult nsXPTCStubBase::Sentinel##n()               \
{                                                    \
  NS_ASSERTION(0,"nsXPTCStubBase::Sentinel called"); \
  return NS_ERROR_NOT_IMPLEMENTED;                   \
}

#include "xptcstubsdef.inc"
