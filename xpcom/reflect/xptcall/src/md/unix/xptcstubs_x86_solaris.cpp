/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributor(s): 
 */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"
#include "xptc_platforms_unixish_x86.h"

static nsresult
PrepareAndDispatch(nsXPTCStubBase* self, uint32 methodIndex, PRUint32* args)
{
#define PARAM_BUFFER_COUNT     16

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
    for(i = 0; i < paramCount; i++, ap++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &dispatchParams[i];

        if(param.IsOut() || !type.IsArithmetic())
        {
            dp->val.p = (void*) *ap;
            continue;
        }
        // else
	    dp->val.p = (void*) *ap;
        switch(type)
        {
        case nsXPTType::T_I64    : dp->val.i64 = *((PRInt64*) ap); ap++; break;
        case nsXPTType::T_U64    : dp->val.u64 = *((PRUint64*)ap); ap++; break;
        case nsXPTType::T_DOUBLE : dp->val.d   = *((double*)  ap); ap++; break;
        }
    }

    result = self->CallMethod((PRUint16)methodIndex, info, dispatchParams);

    NS_RELEASE(iface_info);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

#ifdef __GNUC__         /* Gnu Compiler. */
#define STUB_ENTRY(n) \
nsresult nsXPTCStubBase::Stub##n() \
{ \
  register nsresult (*method) (nsXPTCStubBase *, uint32, PRUint32 *) = PrepareAndDispatch; \
  int temp0, temp1; \
  register nsresult result; \
  __asm__ __volatile__( \
    "leal   0x0c(%%ebp), %%ecx\n\t"    /* args */ \
    "pushl  %%ecx\n\t" \
    "pushl  $"#n"\n\t"                 /* method index */ \
    "movl   0x08(%%ebp), %%ecx\n\t"    /* this */ \
    "pushl  %%ecx\n\t" \
    "call   *%%edx\n\t"                /* PrepareAndDispatch */ \
    "addl   $12, %%esp" \
    : "=a" (result),    /* %0 */ \
      "=&c" (temp0),    /* %1 */ \
      "=d" (temp1)      /* %2 */ \
    : "2" (method)      /* %2 */ \
    : "memory" ); \
    return result; \
}

#elif defined(__SUNPRO_CC)           /* Sun Workshop Compiler. */

#define STUB_ENTRY(n) \
nsresult nsXPTCStubBase::Stub##n() \
{ \
  asm ( \
	"\n\t leal   0x0c(%ebp), %ecx\t / args" \
	"\n\t pushl  %ecx" \
	"\n\t pushl  $"#n"\t / method index" \
	"\n\t movl   0x08(%ebp), %ecx\t / this" \
	"\n\t pushl  %ecx" \
	"\n\t call   __1cSPrepareAndDispatch6FpnOnsXPTCStubBase_IpI_I_\t / PrepareAndDispatch" \
	"\n\t addl  $12, %esp" \
 ); \
/* result == %eax */ \
  if(0) /* supress "*** is expected to return a value." error */ \
     return 0; \
}

#else
#error "can't find a compiler to use"
#endif /* __GNUC__ */

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ASSERTION(0,"nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"
