/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributor: Henry Sobotka <sobotka@axess.com>
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 03/23/2000       IBM Corp.      Various fixes for incorrect parameter passing.
 */

/* Platform specific code to invoke XPCOM methods on native objects */
#if !defined(__IBMCPP__)
#error "This code is only for OS/2 VisualAge for C++"
#endif

#include "xptcprivate.h"

// For holding up to two conforming parameters to load into edx and ecx
// ('this' goes into eax)
// conforming: 4 bytes or less, enum or pointer
static uint32 cparams[2];
static int cpcount;

// For holding up to four floating-point parameters to load into the FP registers
static double fpparams[4];
static int fpcount;


// Remember that these 'words' are 32-bit DWORDs
static PRUint32
invoke_count_words(PRUint32 paramCount, nsXPTCVariant* s)
{
    PRUint32 result = 0;
    cpcount = 0;
    fpcount = 0;
    uint32 firsthalf, secondhalf;

    for(PRUint32 i = 0; i < paramCount; i++, s++)
    {
        if(s->IsPtrData())
        {
            if (cpcount < 2)
                cparams[cpcount++] = (uint32) s->ptr;
            result++;
        }

        else {

            switch(s->type)
            {
             case nsXPTType::T_I8 :
                if( cpcount < 2 )
                    cparams[cpcount++] = (uint32) s->val.i8;
                result++;
                break;
             case nsXPTType::T_I16 :
                if( cpcount < 2 )
                    cparams[cpcount++] = (uint32) s->val.i16;
                result++;
                break;
             case nsXPTType::T_I32 :
                if( cpcount < 2 )
                    cparams[cpcount++] = (uint32) s->val.i32;
                result++;
                break;
             case nsXPTType::T_U8 :
                if( cpcount < 2 )
                    cparams[cpcount++] = (uint32) s->val.u8;
                result++;
                break;
             case nsXPTType::T_U16 :
                if( cpcount < 2 )
                    cparams[cpcount++] = (uint32) s->val.u16;
                result++;
                break;
             case nsXPTType::T_U32 :
                if( cpcount < 2 )
                    cparams[cpcount++] = (uint32) s->val.u32;
                result++;
                break;
	         case nsXPTType::T_BOOL :
                if( cpcount < 2 )
                    cparams[cpcount++] = (uint32) s->val.b;
                result++;
                break;
             case nsXPTType::T_CHAR :
                if( cpcount < 2 )
                    cparams[cpcount++] = (uint32) s->val.c;
                result++;
                break;
             case nsXPTType::T_WCHAR :
                if( cpcount < 2 )
                    cparams[cpcount++] = (uint32) s->val.wc;
                result++;
                break;

             case nsXPTType::T_I64 : 
                firsthalf = (uint32) (s->val.i64 >> 32);
                secondhalf = (uint32) s->val.i32;
                if( cpcount == 0 ) {
                    cparams[cpcount++] = secondhalf;
                    cparams[cpcount++] = firsthalf;
                } else if( cpcount == 1 )
                    cparams[cpcount++] = secondhalf;
                result += 2;
                break;
             case nsXPTType::T_U64 : 
                firsthalf = (uint32) (s->val.u64 >> 32);
                secondhalf = (uint32) s->val.u32;
                if( cpcount = 0 ) {
                    cparams[cpcount++] = secondhalf;
                    cparams[cpcount++] = firsthalf;
                } else if( cpcount = 1 )
                    cparams[cpcount++] = secondhalf;
                result += 2;
                break;
             case nsXPTType::T_DOUBLE : 
                if( fpcount < 4 )
                    fpparams[fpcount++] = s->val.d;
                result += 2;
                break;
             case nsXPTType::T_FLOAT : 
                if( fpcount < 4 )
                    fpparams[fpcount++] = (double) s->val.f;
                result++;
                break;

             default: 
                // all the others are plain pointer types
                if( cpcount < 2 )
                    cparams[cpcount++] = (uint32) s->val.p;
                result++;
                break;
            }

        }
    }
    return result;
}


// Even though the cParams and fpParams don't have to be on the stack, space has
// to be made for them at the right spots in the parameter list, and copying all
// seems the simplest way to do that. Called from xptcall_vacpp.asm.
void
invoke_copy_to_stack(PRUint32* d, uint32 paramCount, nsXPTCVariant* s)
{
    for( PRUint32 i = 0; i < paramCount; i++, d++, s++)
    {
        if(s->IsPtrData())
            *((void**)d) = s->ptr;

        else {

            switch(s->type)
            {
             case nsXPTType::T_I8     : *((int8*)   d) = s->val.i8;          break;
             case nsXPTType::T_I16    : *((int16*)  d) = s->val.i16;         break;
             case nsXPTType::T_I32    : *((int32*)  d) = s->val.i32;         break;
             case nsXPTType::T_I64    : *((int64*)  d) = s->val.i64; d++;    break;
             case nsXPTType::T_U8     : *((uint8*)  d) = s->val.u8;          break;
             case nsXPTType::T_U16    : *((uint16*) d) = s->val.u16;         break;
             case nsXPTType::T_U32    : *((uint32*) d) = s->val.u32;         break;
             case nsXPTType::T_U64    : *((uint64*) d) = s->val.u64; d++;    break;
             case nsXPTType::T_FLOAT  : *((float*)  d) = s->val.f;           break;
             case nsXPTType::T_DOUBLE : *((double*) d) = s->val.d;   d++;    break;
	         case nsXPTType::T_BOOL   : *((PRBool*) d) = s->val.b;           break;
             case nsXPTType::T_CHAR   : *((char*)   d) = s->val.c;           break;
             case nsXPTType::T_WCHAR  : *((wchar_t*)d) = s->val.wc;          break;
             default:
                // all the others are plain pointer types
                *((void**)d) = s->val.p;
                break;
            }
        }
    }
}

extern void _Optlink CallMethodFromVTable__FPvUiPUiT1T2iT6T3( void*, PRUint32, 
        nsXPTCVariant*,nsISupports*, PRUint32, int, int, uint32*, int, double* );

XPTC_PUBLIC_API(nsresult)
XPTC_InvokeByIndex(nsISupports *that, PRUint32 index,
                   PRUint32 paramcount, nsXPTCVariant* params)
{
   int   ibytes;
   void *pStack;
   int   result = NS_OK;

   // Find size in bytes necessary for call and load param structs
   ibytes = 4 * invoke_count_words(paramcount, params);

   // XXXX DO REST FROM xptcall_vacpp.asm
   // pStack = get_stack() + ibytes;
   CallMethodFromVTable__FPvUiPUiT1T2iT6T3(      
                                                0,        /*pStack*/ 
                                                paramcount,
                                                params,
                                                that,
                                                index,
                                                ibytes,
                                                cpcount,
                                                cparams,
                                                fpcount,
                                                fpparams
                                        );

   return result;
}    
