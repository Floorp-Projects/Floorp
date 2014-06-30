/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

#ifndef WIN32
#error "This code is for Win32 only"
#endif

static void __fastcall
invoke_copy_to_stack(uint32_t* d, uint32_t paramCount, nsXPTCVariant* s)
{
    for(; paramCount > 0; paramCount--, d++, s++)
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

#pragma warning(disable : 4035) // OK to have no return value
// Tell the PDB file this function has a standard frame pointer, and not to use
// a custom FPO program.
#pragma optimize( "y", off )
extern "C" NS_EXPORT nsresult NS_FROZENCALL
NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
                 uint32_t paramCount, nsXPTCVariant* params)
{
    __asm {
        mov     edx,paramCount      // Save paramCount for later
        test    edx,edx             // maybe we don't have any params to copy
        jz      noparams
        mov     eax,edx             
        shl     eax,3               // *= 8 (max possible param size)
        sub     esp,eax             // make space for params
        mov     ecx,esp
        push    params
        call    invoke_copy_to_stack // fastcall, ecx = d, edx = paramCount, params is on the stack
noparams:
        mov     ecx,that            // instance in ecx
        push    ecx                 // push this
        mov     edx,[ecx]           // vtable in edx
        mov     eax,methodIndex
        call    dword ptr[edx+eax*4] // stdcall, i.e. callee cleans up stack.
        mov     esp,ebp
#ifdef __clang__
        // clang-cl doesn't emit the pop and ret instructions because it doesn't realize
        // that the call instruction sets a return value in eax.  See
        // https://bugzilla.mozilla.org/show_bug.cgi?id=1028613 and
        // http://llvm.org/bugs/show_bug.cgi?id=17201.
        pop     ebp
        ret
#endif
    }
}
#pragma warning(default : 4035) // restore default
