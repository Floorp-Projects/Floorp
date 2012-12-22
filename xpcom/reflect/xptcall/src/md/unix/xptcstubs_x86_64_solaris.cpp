/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implement shared vtbl methods.

// Keep this in sync with the darwin version.

#include "xptcprivate.h"
#include "xptiprivate.h"

// The Linux/x86-64 ABI passes the first 6 integer parameters and the
// first 8 floating point parameters in registers (rdi, rsi, rdx, rcx,
// r8, r9 and xmm0-xmm7), no stack space is allocated for these by the
// caller.  The rest of the parameters are passed in the callers stack
// area.

const uint32_t PARAM_BUFFER_COUNT   = 16;
const uint32_t GPR_COUNT            = 6;
const uint32_t FPR_COUNT            = 8;

// PrepareAndDispatch() is called by SharedStub() and calls the actual method.
//
// - 'args[]' contains the arguments passed on stack
// - 'gpregs[]' contains the arguments passed in integer registers
// - 'fpregs[]' contains the arguments passed in floating point registers
// 
// The parameters are mapped into an array of type 'nsXPTCMiniVariant'
// and then the method gets called.

extern "C" nsresult
PrepareAndDispatch(nsXPTCStubBase * self, uint32_t methodIndex,
                   uint64_t * args, uint64_t * gpregs, double *fpregs)
{
    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = NULL;
    const nsXPTMethodInfo* info;
    uint32_t paramCount;
    uint32_t i;
    nsresult result = NS_ERROR_FAILURE;

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(uint16_t(methodIndex), &info);
    NS_ASSERTION(info,"no method info");
    if (!info)
        return NS_ERROR_UNEXPECTED;

    paramCount = info->GetParamCount();

    // setup variant array pointer
    if (paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;

    NS_ASSERTION(dispatchParams,"no place for params");
    if (!dispatchParams)
        return NS_ERROR_OUT_OF_MEMORY;

    uint64_t* ap = args;
    uint32_t nr_gpr = 1;    // skip one GPR register for 'that'
    uint32_t nr_fpr = 0;
    uint64_t value;

    for (i = 0; i < paramCount; i++) {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &dispatchParams[i];
	
        if (!param.IsOut() && type == nsXPTType::T_DOUBLE) {
            if (nr_fpr < FPR_COUNT)
                dp->val.d = fpregs[nr_fpr++];
            else
                dp->val.d = *(double*) ap++;
            continue;
        }
        else if (!param.IsOut() && type == nsXPTType::T_FLOAT) {
            if (nr_fpr < FPR_COUNT)
                // The value in %xmm register is already prepared to
                // be retrieved as a float. Therefore, we pass the
                // value verbatim, as a double without conversion.
                dp->val.d = fpregs[nr_fpr++];
            else
                dp->val.f = *(float*) ap++;
            continue;
        }
        else {
            if (nr_gpr < GPR_COUNT)
                value = gpregs[nr_gpr++];
            else
                value = *ap++;
        }

        if (param.IsOut() || !type.IsArithmetic()) {
            dp->val.p = (void*) value;
            continue;
        }

        switch (type) {
        case nsXPTType::T_I8:      dp->val.i8  = (int8_t)   value; break;
        case nsXPTType::T_I16:     dp->val.i16 = (int16_t)  value; break;
        case nsXPTType::T_I32:     dp->val.i32 = (int32_t)  value; break;
        case nsXPTType::T_I64:     dp->val.i64 = (int64_t)  value; break;
        case nsXPTType::T_U8:      dp->val.u8  = (uint8_t)  value; break;
        case nsXPTType::T_U16:     dp->val.u16 = (uint16_t) value; break;
        case nsXPTType::T_U32:     dp->val.u32 = (uint32_t) value; break;
        case nsXPTType::T_U64:     dp->val.u64 = (uint64_t) value; break;
        // Cast to uint8_t first, to remove garbage on upper 56 bits.
        case nsXPTType::T_BOOL:    dp->val.b   = (bool)(uint8_t)   value; break;
        case nsXPTType::T_CHAR:    dp->val.c   = (char)     value; break;
        case nsXPTType::T_WCHAR:   dp->val.wc  = (wchar_t)  value; break;

        default:
            NS_ERROR("bad type");
            break;
        }
    }

    result = self->mOuter->CallMethod((uint16_t) methodIndex, info, dispatchParams);

    if (dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

#define STUB_ENTRY(n)

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"
