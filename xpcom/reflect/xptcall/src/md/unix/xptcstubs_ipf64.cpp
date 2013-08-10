
/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "xptcprivate.h"
#include "xptiprivate.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

// "This code is for IA64 only"

/* Implement shared vtbl methods. */

extern "C" nsresult
PrepareAndDispatch(nsXPTCStubBase* self, uint32_t methodIndex,
  uint64_t* intargs, uint64_t* floatargs, uint64_t* restargs)
{

#define PARAM_BUFFER_COUNT     16

  nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
  nsXPTCMiniVariant* dispatchParams = NULL;
  const nsXPTMethodInfo* info;
  nsresult result = NS_ERROR_FAILURE;
  uint64_t* iargs = intargs;
  uint64_t* fargs = floatargs;
  uint8_t paramCount;
  uint8_t i;

  NS_ASSERTION(self,"no self");

  self->mEntry->GetMethodInfo(uint16_t(methodIndex), &info);
  NS_ASSERTION(info,"no method info");
  if (! info)
      return NS_ERROR_UNEXPECTED;

  paramCount = info->GetParamCount();

  // setup variant array pointer
  if(paramCount > PARAM_BUFFER_COUNT)
    dispatchParams = new nsXPTCMiniVariant[paramCount];
  else
    dispatchParams = paramBuffer;
  NS_ASSERTION(dispatchParams,"no place for params");
  if (! dispatchParams)
      return NS_ERROR_OUT_OF_MEMORY;

  for(i = 0; i < paramCount; ++i)
  {
    int isfloat = 0;
    const nsXPTParamInfo& param = info->GetParam(i);
    const nsXPTType& type = param.GetType();
    nsXPTCMiniVariant* dp = &dispatchParams[i];

    if(param.IsOut() || !type.IsArithmetic())
    {
#ifdef __LP64__
        /* 64 bit pointer mode */
        dp->val.p = (void*) *iargs;
#else
        /* 32 bit pointer mode */
        uint32_t* adr = (uint32_t*) iargs;
        dp->val.p = (void*) (*(adr+1));
#endif
    }
    else
    switch(type)
    {
    case nsXPTType::T_I8     : dp->val.i8  = *(iargs); break;
    case nsXPTType::T_I16    : dp->val.i16 = *(iargs); break;
    case nsXPTType::T_I32    : dp->val.i32 = *(iargs); break;
    case nsXPTType::T_I64    : dp->val.i64 = *(iargs); break;
    case nsXPTType::T_U8     : dp->val.u8  = *(iargs); break;
    case nsXPTType::T_U16    : dp->val.u16 = *(iargs); break;
    case nsXPTType::T_U32    : dp->val.u32 = *(iargs); break;
    case nsXPTType::T_U64    : dp->val.u64 = *(iargs); break;
    case nsXPTType::T_FLOAT  :
      isfloat = 1;
      if (i < 7)
        dp->val.f = (float) *((double*) fargs); /* register */
      else
        dp->val.u32 = *(fargs); /* memory */
      break;
    case nsXPTType::T_DOUBLE :
      isfloat = 1;
      dp->val.u64 = *(fargs);
      break;
    case nsXPTType::T_BOOL   : dp->val.b   = *(iargs); break;
    case nsXPTType::T_CHAR   : dp->val.c   = *(iargs); break;
    case nsXPTType::T_WCHAR  : dp->val.wc  = *(iargs); break;
    default:
      NS_ERROR("bad type");
      break;
    }
    if (i < 7)
    {
      /* we are parsing register arguments */
      if (i == 6)
      {
        // run out of register arguments, move on to memory arguments
        iargs = restargs;
        fargs = restargs;
      }
      else
      {
        ++iargs; // advance one integer register slot
        if (isfloat) ++fargs; // advance float register slot if isfloat
      }
    }
    else
    {
      /* we are parsing memory arguments */
      ++iargs;
      ++fargs;
    }
  }

  result = self->mOuter->CallMethod((uint16_t) methodIndex, info, dispatchParams);

  if(dispatchParams != paramBuffer)
    delete [] dispatchParams;

  return result;
}

extern "C" nsresult SharedStub(uint64_t,uint64_t,uint64_t,uint64_t,
 uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t *);

/* Variable a0-a7 were put there so we can have access to the 8 input
   registers on Stubxyz entry */

#define STUB_ENTRY(n) \
nsresult nsXPTCStubBase::Stub##n(uint64_t a1, \
uint64_t a2,uint64_t a3,uint64_t a4,uint64_t a5,uint64_t a6,uint64_t a7, \
uint64_t a8) \
{ uint64_t a0 = (uint64_t) this; \
 return SharedStub(a0,a1,a2,a3,a4,a5,a6,a7,(uint64_t) n, &a8); \
}

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"

