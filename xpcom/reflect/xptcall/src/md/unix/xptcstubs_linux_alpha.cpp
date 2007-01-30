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
 * Portions created by the Initial Developer are Copyright (C) 1999
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

/* Implement shared vtbl methods. */

#include "xptcprivate.h"
#include "xptiprivate.h"

/* Prototype specifies unmangled function name and disables unused warning */
static nsresult
PrepareAndDispatch(nsXPTCStubBase* self, uint32 methodIndex, PRUint64* args)
__asm__("PrepareAndDispatch") __attribute__((used));

static nsresult
PrepareAndDispatch(nsXPTCStubBase* self, uint32 methodIndex, PRUint64* args)
{
    const PRUint8 PARAM_BUFFER_COUNT = 16;
    const PRUint8 NUM_ARG_REGS = 6-1;        // -1 for "this" pointer

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = NULL;
    const nsXPTMethodInfo* info;
    PRUint8 paramCount;
    PRUint8 i;
    nsresult result = NS_ERROR_FAILURE;

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(PRUint16(methodIndex), &info);

    paramCount = info->GetParamCount();

    // setup variant array pointer
    if(paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;
    NS_ASSERTION(dispatchParams,"no place for params");

    // args[0] to args[NUM_ARG_REGS] hold floating point register values
    PRUint64* ap = args + NUM_ARG_REGS;
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
        case nsXPTType::T_I8     : dp->val.i8  = (PRInt8)    *ap;    break;
        case nsXPTType::T_I16    : dp->val.i16 = (PRInt16)   *ap;    break;
        case nsXPTType::T_I32    : dp->val.i32 = (PRInt32)   *ap;    break;
        case nsXPTType::T_I64    : dp->val.i64 = (PRInt64)   *ap;    break;
        case nsXPTType::T_U8     : dp->val.u8  = (PRUint8)   *ap;    break;
        case nsXPTType::T_U16    : dp->val.u16 = (PRUint16)  *ap;    break;
        case nsXPTType::T_U32    : dp->val.u32 = (PRUint32)  *ap;    break;
        case nsXPTType::T_U64    : dp->val.u64 = (PRUint64)  *ap;    break;
        case nsXPTType::T_FLOAT  :
            if(i < NUM_ARG_REGS)
            {
                // floats passed via registers are stored as doubles
                // in the first NUM_ARG_REGS entries in args
                dp->val.u64 = (PRUint64) args[i];
                dp->val.f = (float) dp->val.d;    // convert double to float
            }
            else
                dp->val.u32 = (PRUint32) *ap;
            break;
        case nsXPTType::T_DOUBLE :
            // doubles passed via registers are also stored
            // in the first NUM_ARG_REGS entries in args
            dp->val.u64 = (i < NUM_ARG_REGS) ? args[i] : *ap;
            break;
        case nsXPTType::T_BOOL   : dp->val.b   = (PRBool)    *ap;    break;
        case nsXPTType::T_CHAR   : dp->val.c   = (char)      *ap;    break;
        case nsXPTType::T_WCHAR  : dp->val.wc  = (PRUnichar) *ap;    break;
        default:
            NS_ASSERTION(0, "bad type");
            break;
        }
    }

    result = self->mOuter->CallMethod((PRUint16)methodIndex, info, dispatchParams);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

/*
 * SharedStub()
 *  Collects arguments and calls PrepareAndDispatch.  The "methodIndex" is
 *  passed to this function via $1 to preserve the argument registers.
 */
__asm__(
    "#### SharedStub ####\n"
".text\n\t"
    ".align 5\n\t"
    ".ent SharedStub\n"
"SharedStub:\n\t"
    ".frame $30,96,$26,0\n\t"
    ".mask 0x4000000,-96\n\t"
    "ldgp $29,0($27)\n"
"$SharedStub..ng:\n\t"
    "subq $30,96,$30\n\t"
    "stq $26,0($30)\n\t"
    ".prologue 1\n\t"

    /*
     * Store arguments passed via registers to the stack.
     * Floating point registers are stored as doubles and converted
     * to floats in PrepareAndDispatch if necessary.
     */
    "stt $f17,16($30)\n\t"   /* floating point registers */
    "stt $f18,24($30)\n\t"
    "stt $f19,32($30)\n\t"
    "stt $f20,40($30)\n\t"
    "stt $f21,48($30)\n\t"
    "stq $17,56($30)\n\t"    /* integer registers */
    "stq $18,64($30)\n\t"
    "stq $19,72($30)\n\t"
    "stq $20,80($30)\n\t"
    "stq $21,88($30)\n\t"

    /*
     * Call PrepareAndDispatch function.
     */
    "bis $1,$1,$17\n\t"      /* pass "methodIndex" */
    "addq $30,16,$18\n\t"    /* pass "args" */
    "bsr $26,$PrepareAndDispatch..ng\n\t"

    "ldq $26,0($30)\n\t"
    "addq $30,96,$30\n\t"
    "ret $31,($26),1\n\t"
    ".end SharedStub"
    );

/*
 * nsresult nsXPTCStubBase::Stub##n()
 *  Sets register $1 to "methodIndex" and jumps to SharedStub.
 */
#define STUB_MANGLED_ENTRY(n, symbol) \
    "#### Stub"#n" ####"      "\n\t" \
    ".text"                   "\n\t" \
    ".align 5"                "\n\t" \
    ".globl " symbol          "\n\t" \
    ".ent " symbol            "\n"   \
symbol ":"                    "\n\t" \
    ".frame $30,0,$26,0"      "\n\t" \
    "ldgp $29,0($27)"         "\n"   \
"$" symbol "..ng:"            "\n\t" \
    ".prologue 1"             "\n\t" \
    "lda $1,"#n               "\n\t" \
    "br $31,$SharedStub..ng"  "\n\t" \
    ".end " symbol

#if defined(__GXX_ABI_VERSION) && __GXX_ABI_VERSION >= 100 /* G++ V3 ABI */

#define STUB_ENTRY(n) \
__asm__( \
    ".if "#n" < 10"                                              "\n\t" \
        STUB_MANGLED_ENTRY(n, "_ZN14nsXPTCStubBase5Stub"#n"Ev")  "\n\t" \
    ".elseif "#n" < 100"                                         "\n\t" \
        STUB_MANGLED_ENTRY(n, "_ZN14nsXPTCStubBase6Stub"#n"Ev")  "\n\t" \
    ".elseif "#n" < 1000"                                        "\n\t" \
        STUB_MANGLED_ENTRY(n, "_ZN14nsXPTCStubBase7Stub"#n"Ev")  "\n\t" \
    ".else"                                                      "\n\t" \
    ".err \"Stub"#n" >= 1000 not yet supported.\""               "\n\t" \
    ".endif" \
    );

#else /* not G++ V3 ABI */

#define STUB_ENTRY(n) \
__asm__( \
    STUB_MANGLED_ENTRY(n, "Stub"#n"__14nsXPTCStubBase") \
    );

#endif /* G++ V3 ABI */

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ASSERTION(0,"nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"
