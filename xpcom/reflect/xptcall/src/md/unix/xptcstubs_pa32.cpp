
/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"
#include "xptiprivate.h" 

#if _HPUX
#error "This code is for HP-PA RISC 32 bit mode only"
#endif

extern "C" nsresult
PrepareAndDispatch(nsXPTCStubBase* self, uint32_t methodIndex,
  uint32_t* args, uint32_t* floatargs)
{

  typedef struct {
    uint32_t hi;
    uint32_t lo;
  } DU;

#define PARAM_BUFFER_COUNT     16

  nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
  nsXPTCMiniVariant* dispatchParams = nullptr;
  const nsXPTMethodInfo* info;
  int32_t regwords = 1; /* self pointer is not in the variant records */
  nsresult result = NS_ERROR_FAILURE;
  uint8_t paramCount;
  uint8_t i;

  NS_ASSERTION(self,"no self");

  self->mEntry->GetMethodInfo(uint16_t(methodIndex), &info);
  NS_ASSERTION(info,"no method info");
  if (!info)
    return NS_ERROR_UNEXPECTED;

  paramCount = info->GetParamCount();

  // setup variant array pointer
  if(paramCount > PARAM_BUFFER_COUNT)
    dispatchParams = new nsXPTCMiniVariant[paramCount];
  else
    dispatchParams = paramBuffer;
  NS_ASSERTION(dispatchParams,"no place for params");
  if (!dispatchParams)
    return NS_ERROR_OUT_OF_MEMORY;

  for(i = 0; i < paramCount; ++i, --args)
  {
    const nsXPTParamInfo& param = info->GetParam(i);
    const nsXPTType& type = param.GetType();
    nsXPTCMiniVariant* dp = &dispatchParams[i];

    if(param.IsOut() || !type.IsArithmetic())
    {
      dp->val.p = (void*) *args;
      ++regwords;
      continue;
    }
    switch(type)
    {
    case nsXPTType::T_I8     : dp->val.i8  = *((int32_t*) args); break;
    case nsXPTType::T_I16    : dp->val.i16 = *((int32_t*) args); break;
    case nsXPTType::T_I32    : dp->val.i32 = *((int32_t*) args); break;
    case nsXPTType::T_DOUBLE :
                               if (regwords & 1)
                               {
                                 ++regwords; /* align on double word */
                                 --args;
                               }
                               if (regwords == 0 || regwords == 2)
                               {
                                 dp->val.d=*((double*) (floatargs + regwords));
                                 --args;
                               }
                               else
                               {
                                 dp->val.d = *((double*) --args);
                               }
                               regwords += 2;
                               continue;
    case nsXPTType::T_U64    :
    case nsXPTType::T_I64    :
                               if (regwords & 1)
                               {
                                 ++regwords; /* align on double word */
                                 --args;
                               }
                               ((DU *)dp)->lo = *((uint32_t*) args);
                               ((DU *)dp)->hi = *((uint32_t*) --args);
                               regwords += 2;
                               continue;
    case nsXPTType::T_FLOAT  :
                               if (regwords >= 4)
                                 dp->val.f = *((float*) args);
                               else
                                 dp->val.f = *((float*) floatargs+4+regwords);
                               break;
    case nsXPTType::T_U8     : dp->val.u8  = *((uint32_t*) args); break;
    case nsXPTType::T_U16    : dp->val.u16 = *((uint32_t*) args); break;
    case nsXPTType::T_U32    : dp->val.u32 = *((uint32_t*) args); break;
    case nsXPTType::T_BOOL   : dp->val.b   = *((uint32_t*) args); break;
    case nsXPTType::T_CHAR   : dp->val.c   = *((uint32_t*) args); break;
    case nsXPTType::T_WCHAR  : dp->val.wc  = *((int32_t*)  args); break;
    default:
      NS_ERROR("bad type");
      break;
    }
    ++regwords;
  }

  result = self->mOuter->CallMethod((uint16_t) methodIndex, info, dispatchParams); 

  if(dispatchParams != paramBuffer)
    delete [] dispatchParams;

  return result;
}

extern "C" nsresult SharedStub(int);

#define STUB_ENTRY(n)       \
nsresult nsXPTCStubBase::Stub##n()  \
{                           \
    return SharedStub(n);   \
}

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"

