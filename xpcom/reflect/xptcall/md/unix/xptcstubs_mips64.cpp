/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xptcprivate.h"

#if (_MIPS_SIM != _ABIN32) && (_MIPS_SIM != _ABI64)
#error "This code is for MIPS n32/n64 only"
#endif

/*
 * This is for MIPS n32/n64 ABI
 *
 * When we're called, the "gp" registers are stored in gprData and
 * the "fp" registers are stored in fprData.  There are 8 regs
 * available which correspond to the first 7 parameters of the
 * function and the "this" pointer.  If there are additional parms,
 * they are stored on the stack at address "args".
 *
 */
extern "C" nsresult ATTRIBUTE_USED
PrepareAndDispatch(nsXPTCStubBase* self, uint32_t methodIndex, uint64_t* args,
                   uint64_t *gprData, double *fprData)
{
#define PARAM_BUFFER_COUNT        16
#define PARAM_GPR_COUNT            7
#define PARAM_FPR_COUNT            7

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = nullptr;
    const nsXPTMethodInfo* info;
    uint8_t paramCount;
    uint8_t i;

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(uint16_t(methodIndex), &info);
    NS_ASSERTION(info,"no method info");

    paramCount = info->GetParamCount();

    // setup variant array pointer
    if(paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;
    NS_ASSERTION(dispatchParams,"no place for params");

    const uint8_t indexOfJSContext = info->IndexOfJSContext();

    uint64_t* ap = args;
    uint32_t iCount = 0;
    for(i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &dispatchParams[i];

        if (i == indexOfJSContext) {
            if (iCount < PARAM_GPR_COUNT)
                iCount++;
            else
                ap++;
        }

        if(param.IsOut() || !type.IsArithmetic())
        {
            if (iCount < PARAM_GPR_COUNT)
                dp->val.p = (void*)gprData[iCount++];
            else
                dp->val.p = (void*)*ap++;
            continue;
        }
        // else
        switch(type)
        {
        case nsXPTType::T_I8:
            if (iCount < PARAM_GPR_COUNT)
                dp->val.i8  = (int8_t)gprData[iCount++];
            else
                dp->val.i8  = (int8_t)*ap++;
            break;

        case nsXPTType::T_I16:
             if (iCount < PARAM_GPR_COUNT)
                 dp->val.i16  = (int16_t)gprData[iCount++];
             else
                 dp->val.i16  = (int16_t)*ap++;
             break;

        case nsXPTType::T_I32:
             if (iCount < PARAM_GPR_COUNT)
                 dp->val.i32  = (int32_t)gprData[iCount++];
             else
                 dp->val.i32  = (int32_t)*ap++;
             break;

        case nsXPTType::T_I64:
             if (iCount < PARAM_GPR_COUNT)
                 dp->val.i64  = (int64_t)gprData[iCount++];
             else
                 dp->val.i64  = (int64_t)*ap++;
             break;

        case nsXPTType::T_U8:
             if (iCount < PARAM_GPR_COUNT)
                 dp->val.u8  = (uint8_t)gprData[iCount++];
             else
                 dp->val.u8  = (uint8_t)*ap++;
             break;

        case nsXPTType::T_U16:
             if (iCount < PARAM_GPR_COUNT)
                 dp->val.u16  = (uint16_t)gprData[iCount++];
             else
                  dp->val.u16  = (uint16_t)*ap++;
             break;

        case nsXPTType::T_U32:
             if (iCount < PARAM_GPR_COUNT)
                 dp->val.u32  = (uint32_t)gprData[iCount++];
             else
                 dp->val.u32  = (uint32_t)*ap++;
             break;

        case nsXPTType::T_U64:
             if (iCount < PARAM_GPR_COUNT)
                 dp->val.u64  = (uint64_t)gprData[iCount++];
             else
                 dp->val.u64  = (uint64_t)*ap++;
             break;

        case nsXPTType::T_FLOAT:
              // the float data formate must not be converted!
              // Just only copy without conversion.
              if (iCount < PARAM_FPR_COUNT)
                  dp->val.f  = *(float*)&fprData[iCount++];
              else
                  dp->val.f  = *((float*)ap++);
              break;

        case nsXPTType::T_DOUBLE:
               if (iCount < PARAM_FPR_COUNT)
                   dp->val.d  = (double)fprData[iCount++];
               else
                   dp->val.d  = *((double*)ap++);
               break;

        case nsXPTType::T_BOOL:
            if (iCount < PARAM_GPR_COUNT)
                dp->val.b  = (bool)gprData[iCount++];
            else
                dp->val.b  = (bool)*ap++;
            break;

        case nsXPTType::T_CHAR:
            if (iCount < PARAM_GPR_COUNT)
                dp->val.c  = (char)gprData[iCount++];
            else
                dp->val.c  = (char)*ap++;
            break;

        case nsXPTType::T_WCHAR:
            if (iCount < PARAM_GPR_COUNT)
                dp->val.wc  = (wchar_t)gprData[iCount++];
            else
                dp->val.wc  = (wchar_t)*ap++;
            break;

        default:
            NS_ASSERTION(0, "bad type");
            break;
        }
    }

    nsresult result = self->mOuter->CallMethod((uint16_t)methodIndex, info,
                                               dispatchParams);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

#define STUB_ENTRY(n)        /* defined in the assembly file */

#define SENTINEL_ENTRY(n)                              \
nsresult nsXPTCStubBase::Sentinel##n()                 \
{                                                      \
    NS_ASSERTION(0,"nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED;                   \
}

#include "xptcstubsdef.inc"
