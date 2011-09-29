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

#ifndef WIN32
#error "This code is for Win32 only"
#endif

extern "C" {

static nsresult __stdcall
PrepareAndDispatch(nsXPTCStubBase* self, PRUint32 methodIndex,
                   PRUint32* args, PRUint32* stackBytesToPop)
{
#define PARAM_BUFFER_COUNT     16

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = NULL;
    const nsXPTMethodInfo* info = NULL;
    PRUint8 paramCount;
    PRUint8 i;
    nsresult result = NS_ERROR_FAILURE;

    // If anything fails before stackBytesToPop can be set then
    // the failure is completely catastrophic!

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(PRUint16(methodIndex), &info);
    NS_ASSERTION(info,"no method info");

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
        case nsXPTType::T_BOOL   : dp->val.b   = *((bool*)  ap);       break;
        case nsXPTType::T_CHAR   : dp->val.c   = *((char*)    ap);       break;
        case nsXPTType::T_WCHAR  : dp->val.wc  = *((wchar_t*) ap);       break;
        default:
            NS_ERROR("bad type");
            break;
        }
    }
    *stackBytesToPop = ((PRUint32)ap) - ((PRUint32)args);

    result = self->mOuter->CallMethod((PRUint16)methodIndex, info, dispatchParams);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

} // extern "C"

// declspec(naked) is broken in gcc
#ifndef __GNUC__
static 
__declspec(naked)
void SharedStub(void)
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
__declspec(naked) nsresult __stdcall nsXPTCStubBase::Stub##n() \
{ __asm mov ecx, n __asm jmp SharedStub }

#else

#define STUB_ENTRY(n) \
nsresult __stdcall nsXPTCStubBase::Stub##n() \
{ \
  PRUint32 *args, stackBytesToPop = 0; \
  nsresult result = 0; \
  nsXPTCStubBase *obj; \
  __asm__ __volatile__ ( \
    "leal   0x0c(%%ebp), %%ecx\n\t"    /* args */ \
    "movl   0x08(%%ebp), %%edx\n\t"    /* this */ \
    : "=c" (args), \
      "=d" (obj)); \
  result = PrepareAndDispatch(obj, n, args, &stackBytesToPop); \
  return result; \
}   

#endif /* __GNUC__ */

#define SENTINEL_ENTRY(n) \
nsresult __stdcall nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#ifdef _MSC_VER
#pragma warning(disable : 4035) // OK to have no return value
#endif
#include "xptcstubsdef.inc"
#ifdef _MSC_VER
#pragma warning(default : 4035) // restore default
#endif

void
#ifdef __GNUC__
__cdecl
#endif
xptc_dummy()
{
}
