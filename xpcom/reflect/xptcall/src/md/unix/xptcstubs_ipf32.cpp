
/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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

#include <stddef.h>
#include <stdlib.h>

// "This code is for IA64 only"

/* Implement shared vtbl methods. */

extern "C" nsresult
PrepareAndDispatch(nsXPTCStubBase* self, PRUint32 methodIndex,
  uint64_t* intargs, uint64_t* floatargs, uint64_t* restargs)
{

#define PARAM_BUFFER_COUNT     16

  nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
  nsXPTCMiniVariant* dispatchParams = NULL;
  nsIInterfaceInfo* iface_info = NULL;
  const nsXPTMethodInfo* info;
  nsresult result = NS_ERROR_FAILURE;
  uint64_t* iargs = intargs;
  uint64_t* fargs = floatargs;
  PRUint8 paramCount;
  PRUint8 i;

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
      NS_ASSERTION(0, "bad type");
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

  result = self->CallMethod((PRUint16) methodIndex, info, dispatchParams);

  NS_RELEASE(iface_info);

  if(dispatchParams != paramBuffer)
    delete [] dispatchParams;

  return result;
}

extern "C" int SharedStub(PRUint64,PRUint64,PRUint64,PRUint64,
 PRUint64,PRUint64,PRUint64,PRUint64,PRUint64);

/* Variable a0-a7 were put there so we can have access to the 8 input
   registers on Stubxyz entry */

#define STUB_ENTRY(n) \
nsresult nsXPTCStubBase::Stub##n(PRUint64 a1, \
PRUint64 a2,PRUint64 a3,PRUint64 a4,PRUint64 a5,PRUint64 a6,PRUint64 a7) \
{ uint64_t a0 = (uint64_t) this; \
 return SharedStub(a0,a1,a2,a3,a4,a5,a6,a7,(PRUint64) n); \
}

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ASSERTION(0,"nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"

