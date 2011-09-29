
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

/* Implement shared vtbl methods. */

#include "xptcprivate.h"
#include "xptiprivate.h" 

#if _HPUX
#error "This code is for HP-PA RISC 32 bit mode only"
#endif

extern "C" nsresult
PrepareAndDispatch(nsXPTCStubBase* self, PRUint32 methodIndex,
  PRUint32* args, PRUint32* floatargs)
{

  typedef struct {
    uint32 hi;
    uint32 lo;
  } DU;

#define PARAM_BUFFER_COUNT     16

  nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
  nsXPTCMiniVariant* dispatchParams = NULL;
  const nsXPTMethodInfo* info;
  PRInt32 regwords = 1; /* self pointer is not in the variant records */
  nsresult result = NS_ERROR_FAILURE;
  PRUint8 paramCount;
  PRUint8 i;

  NS_ASSERTION(self,"no self");

  self->mEntry->GetMethodInfo(PRUint16(methodIndex), &info);
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
    case nsXPTType::T_I8     : dp->val.i8  = *((PRInt32*) args); break;
    case nsXPTType::T_I16    : dp->val.i16 = *((PRInt32*) args); break;
    case nsXPTType::T_I32    : dp->val.i32 = *((PRInt32*) args); break;
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
                               ((DU *)dp)->lo = *((PRUint32*) args);
                               ((DU *)dp)->hi = *((PRUint32*) --args);
                               regwords += 2;
                               continue;
    case nsXPTType::T_FLOAT  :
                               if (regwords >= 4)
                                 dp->val.f = *((float*) args);
                               else
                                 dp->val.f = *((float*) floatargs+4+regwords);
                               break;
    case nsXPTType::T_U8     : dp->val.u8  = *((PRUint32*) args); break;
    case nsXPTType::T_U16    : dp->val.u16 = *((PRUint32*) args); break;
    case nsXPTType::T_U32    : dp->val.u32 = *((PRUint32*) args); break;
    case nsXPTType::T_BOOL   : dp->val.b   = *((bool*)   args); break;
    case nsXPTType::T_CHAR   : dp->val.c   = *((PRUint32*) args); break;
    case nsXPTType::T_WCHAR  : dp->val.wc  = *((PRInt32*)  args); break;
    default:
      NS_ERROR("bad type");
      break;
    }
    ++regwords;
  }

  result = self->mOuter->CallMethod((PRUint16) methodIndex, info, dispatchParams); 

  if(dispatchParams != paramBuffer)
    delete [] dispatchParams;

  return result;
}

extern "C" int SharedStub(int);

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

