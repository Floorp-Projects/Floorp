/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"

#ifndef WIN32
#error "This code is for Win32 only"
#endif

extern "C" {

static
nsresult __stdcall
PrepareAndDispatch(nsXPTCStubBase* self, uint32_t methodIndex,
                   uint32_t* args, uint32_t* stackBytesToPop)
{

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    const nsXPTMethodInfo* info = nullptr;
    uint8_t paramCount;
    uint8_t i;

    // If anything fails before stackBytesToPop can be set then
    // the failure is completely catastrophic!

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(uint16_t(methodIndex), &info);
    NS_ASSERTION(info,"no method info");

    paramCount = info->GetParamCount();

    const uint8_t indexOfJSContext = info->IndexOfJSContext();

    uint32_t* ap = args;
    for(i = 0; i < paramCount; i++, ap++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &paramBuffer[i];

        if (i == indexOfJSContext)
            ap++;

        if(param.IsOut() || !type.IsArithmetic())
        {
            dp->val.p = (void*) *ap;
            continue;
        }
        // else
        switch(type)
        {
        case nsXPTType::T_I8     : dp->val.i8  = *((int8_t*)  ap);       break;
        case nsXPTType::T_I16    : dp->val.i16 = *((int16_t*) ap);       break;
        case nsXPTType::T_I32    : dp->val.i32 = *((int32_t*) ap);       break;
        case nsXPTType::T_I64    : dp->val.i64 = *((int64_t*) ap); ap++; break;
        case nsXPTType::T_U8     : dp->val.u8  = *((uint8_t*) ap);       break;
        case nsXPTType::T_U16    : dp->val.u16 = *((uint16_t*)ap);       break;
        case nsXPTType::T_U32    : dp->val.u32 = *((uint32_t*)ap);       break;
        case nsXPTType::T_U64    : dp->val.u64 = *((uint64_t*)ap); ap++; break;
        case nsXPTType::T_FLOAT  : dp->val.f   = *((float*)   ap);       break;
        case nsXPTType::T_DOUBLE : dp->val.d   = *((double*)  ap); ap++; break;
        case nsXPTType::T_BOOL   : dp->val.b   = *((bool*)  ap);       break;
        case nsXPTType::T_CHAR   : dp->val.c   = *((char*)    ap);       break;
        case nsXPTType::T_WCHAR  : dp->val.wc  = *((wchar_t*) ap);       break;
        default:
            NS_ERROR("bad type");
            break;
        }
    }
    *stackBytesToPop = ((uint32_t)ap) - ((uint32_t)args);

    nsresult result = self->mOuter->CallMethod((uint16_t)methodIndex, info,
                                               paramBuffer);

    return result;
}

} // extern "C"

static MOZ_NAKED void SharedStub(void)
{
    __asm {
        push ebp            // set up simple stack frame
        mov  ebp, esp       // stack has: ebp/vtbl_index/retaddr/this/args
        push ecx            // make room for a ptr
        lea  eax, [ebp-4]   // pointer to stackBytesToPop
        push eax
        lea  eax, [ebp+12]  // pointer to args
        push eax
        push ecx            // vtbl_index
        mov  eax, [ebp+8]   // this
        push eax
        call PrepareAndDispatch
        mov  edx, [ebp+4]   // return address
        mov  ecx, [ebp-4]   // stackBytesToPop
        add  ecx, 8         // for 'this' and return address
        mov  esp, ebp
        pop  ebp
        add  esp, ecx       // fix up stack pointer
        jmp  edx            // simulate __stdcall return
    }
}

// these macros get expanded (many times) in the file #included below
#define STUB_ENTRY(n) \
MOZ_NAKED nsresult __stdcall nsXPTCStubBase::Stub##n() \
{ __asm mov ecx, n __asm jmp SharedStub }

#define SENTINEL_ENTRY(n) \
nsresult __stdcall nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4035) // OK to have no return value
#endif
#include "xptcstubsdef.inc"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

void
xptc_dummy()
{
}
