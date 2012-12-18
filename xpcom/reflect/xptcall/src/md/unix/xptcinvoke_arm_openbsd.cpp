/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

// Remember that these 'words' are 32bit DWORDS

static uint32_t
invoke_count_words(uint32_t paramCount, nsXPTCVariant* s)
{
    uint32_t result = 0;
    for(uint32_t i = 0; i < paramCount; i++, s++)
    {
        if(s->IsPtrData())
        {
            result++;
            continue;
        }
        switch(s->type)
        {
        case nsXPTType::T_I8     :
        case nsXPTType::T_I16    :
        case nsXPTType::T_I32    :
            result++;
            break;
        case nsXPTType::T_I64    :
            result+=2;
            break;
        case nsXPTType::T_U8     :
        case nsXPTType::T_U16    :
        case nsXPTType::T_U32    :
            result++;
            break;
        case nsXPTType::T_U64    :
            result+=2;
            break;
        case nsXPTType::T_FLOAT  :
            result++;
            break;
        case nsXPTType::T_DOUBLE :
            result+=2;
            break;
        case nsXPTType::T_BOOL   :
        case nsXPTType::T_CHAR   :
        case nsXPTType::T_WCHAR  :
            result++;
            break;
        default:
            // all the others are plain pointer types
            result++;
            break;
        }
    }
    return result;
}

static void
invoke_copy_to_stack(uint32_t* d, uint32_t paramCount, nsXPTCVariant* s)
{
    for(uint32_t i = 0; i < paramCount; i++, d++, s++)
    {
        if(s->IsPtrData())
        {
            *((void**)d) = s->ptr;
            continue;
        }
        switch(s->type)
        {
        case nsXPTType::T_I8     : *((int8_t*)  d) = s->val.i8;          break;
        case nsXPTType::T_I16    : *((int16_t*) d) = s->val.i16;         break;
        case nsXPTType::T_I32    : *((int32_t*) d) = s->val.i32;         break;
        case nsXPTType::T_I64    : *((int64_t*) d) = s->val.i64; d++;    break;
        case nsXPTType::T_U8     : *((uint8_t*) d) = s->val.u8;          break;
        case nsXPTType::T_U16    : *((uint16_t*)d) = s->val.u16;         break;
        case nsXPTType::T_U32    : *((uint32_t*)d) = s->val.u32;         break;
        case nsXPTType::T_U64    : *((uint64_t*)d) = s->val.u64; d++;    break;
        case nsXPTType::T_FLOAT  : *((float*)   d) = s->val.f;           break;
        case nsXPTType::T_DOUBLE : *((double*)  d) = s->val.d;   d++;    break;
        case nsXPTType::T_BOOL   : *((bool*)  d) = s->val.b;           break;
        case nsXPTType::T_CHAR   : *((char*)    d) = s->val.c;           break;
        case nsXPTType::T_WCHAR  : *((wchar_t*) d) = s->val.wc;          break;
        default:
            // all the others are plain pointer types
            *((void**)d) = s->val.p;
            break;
        }
    }
}

extern "C" {
    struct my_params_struct {
        nsISupports* that;      
        uint32_t Index;         
        uint32_t Count;         
        nsXPTCVariant* params;  
        uint32_t fn_count;     
        uint32_t fn_copy;      
    };
}

EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
                   uint32_t paramCount, nsXPTCVariant* params)
{
    uint32_t result;
    struct my_params_struct my_params;
    my_params.that = that;
    my_params.Index = methodIndex;
    my_params.Count = paramCount;
    my_params.params = params;
    my_params.fn_copy = (uint32_t) &invoke_copy_to_stack;
    my_params.fn_count = (uint32_t) &invoke_count_words;

/* This is to call a given method of class that.
 * The parameters are in params, the number is in paramCount.
 * The routine will issue calls to count the number of words
 * required for argument passing and to copy the arguments to
 * the stack.
 * Since APCS passes the first 3 params in r1-r3, we need to
 * load the first three words from the stack and correct the stack
 * pointer (sp) in the appropriate way. This means:
 *
 * 1.) more than 3 arguments: load r1-r3, correct sp and remember No.
 *			      of bytes left on the stack in r4
 *
 * 2.) <= 2 args: load r1-r3 (we won't be causing a stack overflow I hope),
 *		  restore sp as if nothing had happened and set the marker r4 to zero.
 *
 * Afterwards sp will be restored using the value in r4 (which is not a temporary register
 * and will be preserved by the function/method called according to APCS [ARM Procedure
 * Calling Standard]).
 *
 * !!! IMPORTANT !!!
 * This routine makes assumptions about the vtable layout of the c++ compiler. It's implemented
 * for arm-linux GNU g++ >= 2.8.1 (including egcs and gcc-2.95.[1-3])!
 *
 */

#ifdef __GNUC__
  __asm__ __volatile__(
    "ldr	r1, [%1, #12]	\n\t"	/* prepare to call invoke_count_words	*/
    "ldr	ip, [%1, #16]	\n\t"	/* r0=paramCount, r1=params		*/
    "ldr	r0, [%1,  #8]	\n\t"
    "mov	lr, pc		\n\t"	/* call it...				*/
    "mov	pc, ip		\n\t"
    "mov	r4, r0, lsl #2	\n\t"	/* This is the amount of bytes needed.	*/
    "sub	sp, sp, r4	\n\t"	/* use stack space for the args...	*/
    "mov	r0, sp		\n\t"	/* prepare a pointer an the stack	*/
    "ldr	r1, [%1,  #8]	\n\t"	/* =paramCount				*/
    "ldr	r2, [%1, #12]	\n\t"	/* =params				*/
    "ldr	ip, [%1, #20]	\n\t"	/* =invoke_copy_to_stack		*/
    "mov	lr, pc		\n\t"	/* copy args to the stack like the	*/
    "mov	pc, ip		\n\t"	/* compiler would.			*/
    "ldr	r0, [%1]	\n\t"	/* =that				*/
    "ldr	r1, [r0, #0]	\n\t"	/* get that->vtable offset		*/
    "ldr	r2, [%1, #4]	\n\t"
    "mov	r2, r2, lsl #2	\n\t"	/* a vtable_entry(x)=8 + (4 bytes * x)	*/
    "ldr        ip, [r1, r2]    \n\t"	/* get method adress from vtable        */
    "cmp	r4, #12		\n\t"	/* more than 3 arguments???		*/
    "ldmgtia	sp!, {r1, r2, r3}\n\t"	/* yes: load arguments for r1-r3	*/
    "subgt	r4, r4, #12	\n\t"	/*      and correct the stack pointer	*/
    "ldmleia	sp, {r1, r2, r3}\n\t"	/* no:  load r1-r3 from stack		*/ 
    "addle	sp, sp, r4	\n\t"	/*      and restore stack pointer	*/
    "movle	r4, #0		\n\t"	/*	a mark for restoring sp		*/
    "ldr	r0, [%1, #0]	\n\t"	/* get (self)				*/
    "mov	lr, pc		\n\t"	/* call mathod				*/
    "mov	pc, ip		\n\t"
    "add	sp, sp, r4	\n\t"	/* restore stack pointer		*/
    "mov	%0, r0		\n\t"	/* the result...			*/
    : "=r" (result)
    : "r" (&my_params), "m" (my_params)
    : "r0", "r1", "r2", "r3", "r4", "ip", "lr", "sp"
    );
#else
#error "Unsupported compiler. Use g++ >= 2.8 for OpenBSD/arm."
#endif /* G++ >= 2.8 */

  return result;
}    
