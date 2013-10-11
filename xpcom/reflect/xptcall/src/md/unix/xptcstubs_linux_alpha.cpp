/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"
#include "xptiprivate.h"

/* Prototype specifies unmangled function name and disables unused warning */
static nsresult
PrepareAndDispatch(nsXPTCStubBase* self, uint32_t methodIndex, uint64_t* args)
__asm__("PrepareAndDispatch") __attribute__((used));

static nsresult
PrepareAndDispatch(nsXPTCStubBase* self, uint32_t methodIndex, uint64_t* args)
{
    const uint8_t PARAM_BUFFER_COUNT = 16;
    const uint8_t NUM_ARG_REGS = 6-1;        // -1 for "this" pointer

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = nullptr;
    const nsXPTMethodInfo* info;
    uint8_t paramCount;
    uint8_t i;
    nsresult result = NS_ERROR_FAILURE;

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(uint16_t(methodIndex), &info);

    paramCount = info->GetParamCount();

    // setup variant array pointer
    if(paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;
    NS_ASSERTION(dispatchParams,"no place for params");

    // args[0] to args[NUM_ARG_REGS] hold floating point register values
    uint64_t* ap = args + NUM_ARG_REGS;
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
        case nsXPTType::T_I8     : dp->val.i8  = (int8_t)    *ap;    break;
        case nsXPTType::T_I16    : dp->val.i16 = (int16_t)   *ap;    break;
        case nsXPTType::T_I32    : dp->val.i32 = (int32_t)   *ap;    break;
        case nsXPTType::T_I64    : dp->val.i64 = (int64_t)   *ap;    break;
        case nsXPTType::T_U8     : dp->val.u8  = (uint8_t)   *ap;    break;
        case nsXPTType::T_U16    : dp->val.u16 = (uint16_t)  *ap;    break;
        case nsXPTType::T_U32    : dp->val.u32 = (uint32_t)  *ap;    break;
        case nsXPTType::T_U64    : dp->val.u64 = (uint64_t)  *ap;    break;
        case nsXPTType::T_FLOAT  :
            if(i < NUM_ARG_REGS)
            {
                // floats passed via registers are stored as doubles
                // in the first NUM_ARG_REGS entries in args
                dp->val.u64 = (uint64_t) args[i];
                dp->val.f = (float) dp->val.d;    // convert double to float
            }
            else
                dp->val.u32 = (uint32_t) *ap;
            break;
        case nsXPTType::T_DOUBLE :
            // doubles passed via registers are also stored
            // in the first NUM_ARG_REGS entries in args
            dp->val.u64 = (i < NUM_ARG_REGS) ? args[i] : *ap;
            break;
        case nsXPTType::T_BOOL   : dp->val.b   = (bool)    *ap;    break;
        case nsXPTType::T_CHAR   : dp->val.c   = (char)      *ap;    break;
        case nsXPTType::T_WCHAR  : dp->val.wc  = (PRUnichar) *ap;    break;
        default:
            NS_ERROR("bad type");
            break;
        }
    }

    result = self->mOuter->CallMethod((uint16_t)methodIndex, info, dispatchParams);

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


#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"
