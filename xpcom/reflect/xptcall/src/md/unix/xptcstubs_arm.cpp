/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"

#if !defined(LINUX) || !defined(__arm__)
#error "This code is for Linux ARM only. Please check if it works for you, too.\nDepends strongly on gcc behaviour."
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


/* The easiest way to implement this is to do it as a c++ method. This means that
 * the compiler will same some registers to the stack as soon as this method is
 * entered. We have to be aware of that and have to know the number of registers
 * that will be pushed to the stack for now.
 * The compiler passes arguments to functions/methods in r1-r3 and the rest is on the
 * stack. r0 is (self).
 *
 *
 * !!! IMPORTANT !!!
 * This code will *not* work if compiled without optimization (-O / -O2) because
 * the compiler overwrites the registers r0-r3 (it thinks it's called without 
 * parameters) and perhaps reserves more stack space than we think.
 *
 *
 * Since we don't know the number of parameters we have to pass to the required
 * method we use the following scheme:
 * 1.) Save method parameters that are passed in r1-r3 to a secure location
 * 2.) copy the stack space that is reserved at method entry to a secure location
 * 3.) copy the method arguments that formerly where in r1-r3 right in front of the
 *     other arguments (if any). PrepareAndDispatch needs all arguments in an array.
 *     It will sort out the correct argument passing convention using the InterfaceInfo's.
 * 4.) Call PrepareAndDispatch
 * 5.) Copy the stack contents from our method entry back in place to exit cleanly.
 * 
 * The easier way would be to completly implement this in assembler. This way one could get rid
 * of the compiler generated function prologue.
 */

#define STUB_ENTRY(n)								   \
nsresult nsXPTCStubBase::Stub##n()						   \
{ 										   \
  register nsresult result;							   \
  __asm__ __volatile__(								   \
    "sub 	sp, sp, #32	\n\t"    /* correct stack for pushing all args	*/ \
    "str	r1, [sp]	\n\t"    /* push all args in r1-r3 to stack	*/ \
    "str   	r2, [sp, #4]	\n\t"    /* 					*/ \
    "str	r3, [sp, #8]	\n\t"	 /* 					*/ \
    "add	r1, sp, #32	\n\t"	 /* copy saved registers:		*/ \
    "ldr	r2, [r1]	\n\t"	 /* The scene is as follows - 		*/ \
    "str	r2, [sp, #12]	\n\t"    /* sl, fp, ip, lr, pc get saved to the */ \
    "ldr        r2, [r1, # 4]   \n\t"    /* stack, and behind that is the rest	*/ \
    "str        r2, [sp, #16]   \n\t"    /* of our function parameters.		*/ \
    "ldr        r2, [r1, # 8]   \n\t"    /*					*/ \
    "str        r2, [sp, #20]   \n\t"    /*					*/ \
    "ldr        r2, [r1, #12]   \n\t"    \
    "str        r2, [sp, #24]   \n\t"    \
    "ldr        r2, [r1, #16]   \n\t"    \
    "str        r2, [sp, #28]   \n\t"    \
    "ldmia	sp, {r1, r2, r3}\n\t"	 /* Copy method arguments to the right  */ \
    "add	lr, sp, #40	\n\t"	 /* location.				*/ \
    "stmia	lr, {r1, r2, r3}\n\t"	 \
    "add	sp, sp, #12	\n\t"	 \
    "mov	r1, #"#n"	\n\t"    /* = methodIndex 			*/ \
    "mov	r2, lr		\n\t"	 /* = &(args)				*/ \
    "bl	PrepareAndDispatch__FP14nsXPTCStubBaseUiPUi   \n\t" /*PrepareAndDispatch*/ \
    "mov	%0, r0		\n\t"	 /* Result				*/ \
    "add	r0, sp, #20	\n\t"	 /* copy everything back in place for	*/ \
    "ldmia	sp!, {r1,r2,r3} \n\t"	 /* the normal c++ m,ethod exit		*/ \
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
