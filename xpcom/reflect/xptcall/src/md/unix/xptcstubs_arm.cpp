/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"

#if !defined(LINUX) || !defined(__arm)
#error "this code is for Linux ARM only"
#endif

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
        switch(type)
        {
        case nsXPTType::T_I8     : dp->val.i8  = *((PRInt8*)  ap);       break;
        case nsXPTType::T_I16    : dp->val.i16 = *((PRInt16*) ap);       break;
        case nsXPTType::T_I32    : dp->val.i32 = *((PRInt32*) ap);       break;
        case nsXPTType::T_I64    : dp->val.i64 = *((PRInt64*) ap); ap++; break;
        case nsXPTType::T_U8     : dp->val.u8  = *((PRUint8*) ap);       break;
        case nsXPTType::T_U16    : dp->val.u16 = *((PRUint16*)ap);       break;
        case nsXPTType::T_U32    : dp->val.u32 = *((PRUint32*)ap);       break;
        case nsXPTType::T_U64    : dp->val.u64 = *((PRUint64*)ap); ap++; break;
        case nsXPTType::T_FLOAT  : dp->val.f   = *((float*)   ap);       break;
        case nsXPTType::T_DOUBLE : dp->val.d   = *((double*)  ap); ap++; break;
        case nsXPTType::T_BOOL   : dp->val.b   = *((PRBool*)  ap);       break;
        case nsXPTType::T_CHAR   : dp->val.c   = *((char*)    ap);       break;
        case nsXPTType::T_WCHAR  : dp->val.wc  = *((wchar_t*) ap);       break;
        default:
            NS_ASSERTION(0, "bad type");
            break;
        }
    }

    result = self->CallMethod((PRUint16)methodIndex, info, dispatchParams);

    NS_RELEASE(iface_info);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

#define STUB_ENTRY(n)								   \
nsresult nsXPTCStubBase::Stub##n()						   \
{ 										   \
  register void* method = &PrepareAndDispatch;					   \
  register nsresult result;							   \
  __asm__ __volatile__(								   \
    "sub 	sp, sp, #32	\n\t"    /* correct stack for pushing all args	*/ \
    "str	r1, [sp]	\n\t"    /* push all args in r1-r3 to stack	*/ \
    "str   	r2, [sp, #4]	\n\t"    /* to make a PRUint32 array of		*/ \
    "str	r3, [sp, #8]	\n\t"	 /* functin parameters			*/ \
    "add	r1, sp, #32	\n\t"	 /* copy saved registers:		*/ \
    "ldr	r2, [r1]	\n\t"	 /* The scene is as follows - 		*/ \
    "str	r2, [sp, #12]	\n\t"    /* sl, fp, ip, lr, pc get saved to the */ \
    "ldr        r2, [r1, # 4]   \n\t"    /* stack, and behind that is the rest	*/ \
    "str        r2, [sp, #16]   \n\t"    /* of our function parameters.		*/ \
    "ldr        r2, [r1, # 8]   \n\t"    /* So we make some room and copy r1-r3	*/ \
    "str        r2, [sp, #20]   \n\t"    /* right in place.			*/ \
    "ldr        r2, [r1, #12]   \n\t"    \
    "str        r2, [sp, #24]   \n\t"    \
    "ldr        r2, [r1, #16]   \n\t"    \
    "str        r2, [sp, #28]   \n\t"    \
    "ldmia	sp, {r1, r2, r3}\n\t"	 \
    "add	lr, sp, #40	\n\t"	 \
    "stmia	lr, {r1, r2, r3}\n\t"	 \
    "add	sp, sp, #12	\n\t"	 \
    "mov	r1, #"#n"	\n\t"    /* methodIndex 			*/ \
    "mov	r2, lr		\n\t"	 /* &args				*/ \
    "bl	PrepareAndDispatch__FP14nsXPTCStubBaseUiPUi   \n\t" /*PrepareAndDispatch*/ \
    "mov	%0, r0		\n\t"	 /* Result				*/ \
    "add	r0, sp, #20	\n\t"	 \
    "ldmia	sp!, {r1,r2,r3} \n\t"	 \
    "stmia	r0!, {r1,r2,r3}	\n\t"	 \
    "ldmia	sp!, {r1, r2}	\n\t"	 \
    "stmia	r0, {r1, r2}	\n\t"	 \
    : "=r" (result)								   \
    : 										   \
    : "r0", "r1", "r2", "r3", "lr" );						   \
    return result;								   \
}

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ASSERTION(0,"nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"
