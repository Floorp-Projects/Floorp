/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

/* Prototype specifies unmangled function name and disables unused warning */
static void
invoke_copy_to_stack(PRUint64* d, PRUint32 paramCount, nsXPTCVariant* s)
__asm__("invoke_copy_to_stack") __attribute__((used));

static void
invoke_copy_to_stack(PRUint64* d, PRUint32 paramCount, nsXPTCVariant* s)
{
    const PRUint8 NUM_ARG_REGS = 6-1;        // -1 for "this" pointer

    for(PRUint32 i = 0; i < paramCount; i++, d++, s++)
    {
        if(s->IsPtrData())
        {
            *d = (PRUint64)s->ptr;
            continue;
        }
        switch(s->type)
        {
        case nsXPTType::T_I8     : *d = (PRUint64)s->val.i8;     break;
        case nsXPTType::T_I16    : *d = (PRUint64)s->val.i16;    break;
        case nsXPTType::T_I32    : *d = (PRUint64)s->val.i32;    break;
        case nsXPTType::T_I64    : *d = (PRUint64)s->val.i64;    break;
        case nsXPTType::T_U8     : *d = (PRUint64)s->val.u8;     break;
        case nsXPTType::T_U16    : *d = (PRUint64)s->val.u16;    break;
        case nsXPTType::T_U32    : *d = (PRUint64)s->val.u32;    break;
        case nsXPTType::T_U64    : *d = (PRUint64)s->val.u64;    break;
        case nsXPTType::T_FLOAT  :
            if(i < NUM_ARG_REGS)
            {
                // convert floats to doubles if they are to be passed
                // via registers so we can just deal with doubles later
                union { PRUint64 u64; double d; } t;
                t.d = (double)s->val.f;
                *d = t.u64;
            }
            else
                // otherwise copy to stack normally
                *d = (PRUint64)s->val.u32;
            break;
        case nsXPTType::T_DOUBLE : *d = (PRUint64)s->val.u64;    break;
        case nsXPTType::T_BOOL   : *d = (PRUint64)s->val.b;      break;
        case nsXPTType::T_CHAR   : *d = (PRUint64)s->val.c;      break;
        case nsXPTType::T_WCHAR  : *d = (PRUint64)s->val.wc;     break;
        default:
            // all the others are plain pointer types
            *d = (PRUint64)s->val.p;
            break;
        }
    }
}

/*
 * EXPORT_XPCOM_API(nsresult)
 * NS_InvokeByIndex_P(nsISupports* that, PRUint32 methodIndex,
 *                  PRUint32 paramCount, nsXPTCVariant* params)
 */
__asm__(
    "#### NS_InvokeByIndex_P ####\n"
".text\n\t"
    ".align 5\n\t"
    ".globl NS_InvokeByIndex_P\n\t"
    ".ent NS_InvokeByIndex_P\n"
"NS_InvokeByIndex_P:\n\t"
    ".frame $15,32,$26,0\n\t"
    ".mask 0x4008000,-32\n\t"
    "ldgp $29,0($27)\n"
"$NS_InvokeByIndex_P..ng:\n\t"
    "subq $30,32,$30\n\t"
    "stq $26,0($30)\n\t"
    "stq $15,8($30)\n\t"
    "bis $30,$30,$15\n\t"
    ".prologue 1\n\t"

    /*
     * Allocate enough stack space to hold the greater of 6 or "paramCount"+1
     * parameters. (+1 for "this" pointer)  Room for at least 6 parameters
     * is required for storage of those passed via registers.
     */

    "bis $31,5,$2\n\t"      /* count = MAX(5, "paramCount") */
    "cmplt $2,$18,$1\n\t"
    "cmovne $1,$18,$2\n\t"
    "s8addq $2,16,$1\n\t"   /* room for count+1 params (8 bytes each) */
    "bic $1,15,$1\n\t"      /* stack space is rounded up to 0 % 16 */
    "subq $30,$1,$30\n\t"

    "stq $16,0($30)\n\t"    /* save "that" (as "this" pointer) */
    "stq $17,16($15)\n\t"   /* save "methodIndex" */

    "addq $30,8,$16\n\t"    /* pass stack pointer */
    "bis $18,$18,$17\n\t"   /* pass "paramCount" */
    "bis $19,$19,$18\n\t"   /* pass "params" */
    "bsr $26,$invoke_copy_to_stack..ng\n\t"     /* call invoke_copy_to_stack */

    /*
     * Copy the first 6 parameters to registers and remove from stack frame.
     * Both the integer and floating point registers are set for each parameter
     * except the first which is the "this" pointer.  (integer only)
     * The floating point registers are all set as doubles since the
     * invoke_copy_to_stack function should have converted the floats.
     */
    "ldq $16,0($30)\n\t"    /* integer registers */
    "ldq $17,8($30)\n\t"
    "ldq $18,16($30)\n\t"
    "ldq $19,24($30)\n\t"
    "ldq $20,32($30)\n\t"
    "ldq $21,40($30)\n\t"
    "ldt $f17,8($30)\n\t"   /* floating point registers */
    "ldt $f18,16($30)\n\t"
    "ldt $f19,24($30)\n\t"
    "ldt $f20,32($30)\n\t"
    "ldt $f21,40($30)\n\t"

    "addq $30,48,$30\n\t"   /* remove params from stack */

    /*
     * Call the virtual function with the constructed stack frame.
     */
    "bis $16,$16,$1\n\t"    /* load "this" */
    "ldq $2,16($15)\n\t"    /* load "methodIndex" */
    "ldq $1,0($1)\n\t"      /* load vtable */
    "s8addq $2,$31,$2\n\t"  /* vtable index = "methodIndex" * 8 */
    "addq $1,$2,$1\n\t"
    "ldq $27,0($1)\n\t"     /* load address of function */
    "jsr $26,($27),0\n\t"   /* call virtual function */
    "ldgp $29,0($26)\n\t"

    "bis $15,$15,$30\n\t"
    "ldq $26,0($30)\n\t"
    "ldq $15,8($30)\n\t"
    "addq $30,32,$30\n\t"
    "ret $31,($26),1\n\t"
    ".end NS_InvokeByIndex_P"
    );
