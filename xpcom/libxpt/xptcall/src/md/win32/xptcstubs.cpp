/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"

#ifndef WIN32
#error "This code is for Win32 only"
#endif

static nsresult __stdcall
PrepareAndDispatch(nsXPTCStubBase* self, PRUint32 methodIndex,
                   PRUint32* args, PRUint32* stackBytesToPop)
{
#define PARAM_BUFFER_COUNT     16

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = NULL;
    nsIInterfaceInfo* iface_info = NULL;
    const nsXPTMethodInfo* info;
    PRUint8 paramCount;
    PRUint8 i;
    nsresult result = NS_ERROR_FAILURE;

    // If anything fails before stackBytesToPop can be set then
    // the failure is completely catastrophic!

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
    *stackBytesToPop = ((PRUint32)ap) - ((PRUint32)args);

    result = self->CallMethod((PRUint16)methodIndex, info, dispatchParams);

    NS_RELEASE(iface_info);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

static __declspec(naked) void SharedStub(void)
{
    __asm {
        push ebp            // set up simple stack frame
        mov  ebp, esp       // stack has: ebp/vtbl_index/retaddr/this/args
        push ecx            // make room for a ptr
        lea  eax, [ebp-4]   // pointer to stackBytesToPop
        push eax
        lea  ecx, [ebp+16]  // pointer to args
        push ecx
        mov  edx, [ebp+4]   // vtbl_index
        push edx
        mov  eax, [ebp+12]  // this
        push eax
        call PrepareAndDispatch
        mov  edx, [ebp+8]   // return address
        mov  ecx, [ebp-4]   // stackBytesToPop
        add  ecx, 12        // for this, the index, and ret address
        mov  esp, ebp
        pop  ebp
        add  esp, ecx       // fix up stack pointer
        jmp  edx            // simulate __stdcall return
    }
}

// these macros get expanded (many times) in the file #included below
#define STUB_ENTRY(n) \
__declspec(naked) nsresult __stdcall nsXPTCStubBase::Stub##n() \
{ __asm push n __asm jmp SharedStub }

#define SENTINEL_ENTRY(n) \
nsresult __stdcall nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ASSERTION(0,"nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#pragma warning(disable : 4035) // OK to have no return value
#include "xptcstubsdef.inc"
#pragma warning(default : 4035) // restore default

