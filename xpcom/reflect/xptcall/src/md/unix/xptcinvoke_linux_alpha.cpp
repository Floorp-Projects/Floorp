/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Glen Nakamura <glen@imodulo.com>
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
 * NS_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
 *                  PRUint32 paramCount, nsXPTCVariant* params)
 */
__asm__(
    "#### NS_InvokeByIndex ####\n"
".text\n\t"
    ".align 5\n\t"
    ".globl NS_InvokeByIndex\n\t"
    ".ent NS_InvokeByIndex\n"
"NS_InvokeByIndex:\n\t"
    ".frame $15,32,$26,0\n\t"
    ".mask 0x4008000,-32\n\t"
    "ldgp $29,0($27)\n"
"$NS_InvokeByIndex..ng:\n\t"
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
#if defined(__GXX_ABI_VERSION) && __GXX_ABI_VERSION >= 100 /* G++ V3 ABI */
    "s8addq $2,$31,$2\n\t"  /* vtable index = "methodIndex" * 8 */
#else /* not G++ V3 ABI */
    "s8addq $2,16,$2\n\t"   /* vtable index = "methodIndex" * 8 + 16 */
#endif /* G++ V3 ABI */
    "addq $1,$2,$1\n\t"
    "ldq $27,0($1)\n\t"     /* load address of function */
    "jsr $26,($27),0\n\t"   /* call virtual function */
    "ldgp $29,0($26)\n\t"

    "bis $15,$15,$30\n\t"
    "ldq $26,0($30)\n\t"
    "ldq $15,8($30)\n\t"
    "addq $30,32,$30\n\t"
    "ret $31,($26),1\n\t"
    ".end NS_InvokeByIndex"
    );
