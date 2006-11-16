/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
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

// Implement shared vtbl methods.

#include "xptcprivate.h"

// The Linux/x86-64 ABI passes the first 6 integer parameters and the
// first 8 floating point parameters in registers (rdi, rsi, rdx, rcx,
// r8, r9 and xmm0-xmm7), no stack space is allocated for these by the
// caller.  The rest of the parameters are passed in the callers stack
// area.

const PRUint32 PARAM_BUFFER_COUNT   = 16;
const PRUint32 GPR_COUNT            = 6;
const PRUint32 FPR_COUNT            = 8;

// PrepareAndDispatch() is called by SharedStub() and calls the actual method.
//
// - 'args[]' contains the arguments passed on stack
// - 'gpregs[]' contains the arguments passed in integer registers
// - 'fpregs[]' contains the arguments passed in floating point registers
// 
// The parameters are mapped into an array of type 'nsXPTCMiniVariant'
// and then the method gets called.

extern "C" nsresult
PrepareAndDispatch(nsXPTCStubBase * self, PRUint32 methodIndex,
                   PRUint64 * args, PRUint64 * gpregs, double *fpregs)
{
    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = NULL;
    nsIInterfaceInfo* iface_info = NULL;
    const nsXPTMethodInfo* info;
    PRUint32 paramCount;
    PRUint32 i;
    nsresult result = NS_ERROR_FAILURE;

    NS_ASSERTION(self,"no self");

    self->GetInterfaceInfo(&iface_info);
    NS_ASSERTION(iface_info,"no interface info");
    if (!iface_info)
        return NS_ERROR_UNEXPECTED;

    iface_info->GetMethodInfo(PRUint16(methodIndex), &info);
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

    PRUint64* ap = args;
    PRUint32 nr_gpr = 1;    // skip one GPR register for 'that'
    PRUint32 nr_fpr = 0;
    PRUint64 value;

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
                dp->val.d = *(double*) ap++;
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
        case nsXPTType::T_I8:      dp->val.i8  = (PRInt8)   value; break;
        case nsXPTType::T_I16:     dp->val.i16 = (PRInt16)  value; break;
        case nsXPTType::T_I32:     dp->val.i32 = (PRInt32)  value; break;
        case nsXPTType::T_I64:     dp->val.i64 = (PRInt64)  value; break;
        case nsXPTType::T_U8:      dp->val.u8  = (PRUint8)  value; break;
        case nsXPTType::T_U16:     dp->val.u16 = (PRUint16) value; break;
        case nsXPTType::T_U32:     dp->val.u32 = (PRUint32) value; break;
        case nsXPTType::T_U64:     dp->val.u64 = (PRUint64) value; break;
        case nsXPTType::T_BOOL:    dp->val.b   = (PRBool)   value; break;
        case nsXPTType::T_CHAR:    dp->val.c   = (char)     value; break;
        case nsXPTType::T_WCHAR:   dp->val.wc  = (wchar_t)  value; break;

        default:
            NS_ASSERTION(0, "bad type");
            break;
        }
    }

    result = self->CallMethod((PRUint16) methodIndex, info, dispatchParams);

    NS_RELEASE(iface_info);

    if (dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

#if defined(__GXX_ABI_VERSION) && __GXX_ABI_VERSION >= 100 /* G++ V3 ABI */
// Linux/x86-64 uses gcc >= 3.1
#define STUB_ENTRY(n) \
asm(".section	\".text\"\n\t" \
    ".align	2\n\t" \
    ".if	" #n " < 10\n\t" \
    ".globl	_ZN14nsXPTCStubBase5Stub" #n "Ev\n\t" \
    ".type	_ZN14nsXPTCStubBase5Stub" #n "Ev,@function\n" \
    "_ZN14nsXPTCStubBase5Stub" #n "Ev:\n\t" \
    ".elseif	" #n " < 100\n\t" \
    ".globl	_ZN14nsXPTCStubBase6Stub" #n "Ev\n\t" \
    ".type	_ZN14nsXPTCStubBase6Stub" #n "Ev,@function\n" \
    "_ZN14nsXPTCStubBase6Stub" #n "Ev:\n\t" \
    ".elseif    " #n " < 1000\n\t" \
    ".globl     _ZN14nsXPTCStubBase7Stub" #n "Ev\n\t" \
    ".type      _ZN14nsXPTCStubBase7Stub" #n "Ev,@function\n" \
    "_ZN14nsXPTCStubBase7Stub" #n "Ev:\n\t" \
    ".else\n\t" \
    ".err	\"stub number " #n " >= 1000 not yet supported\"\n\t" \
    ".endif\n\t" \
    "movl	$" #n ", %eax\n\t" \
    "jmp	SharedStub\n\t" \
    ".if	" #n " < 10\n\t" \
    ".size	_ZN14nsXPTCStubBase5Stub" #n "Ev,.-_ZN14nsXPTCStubBase5Stub" #n "Ev\n\t" \
    ".elseif	" #n " < 100\n\t" \
    ".size	_ZN14nsXPTCStubBase6Stub" #n "Ev,.-_ZN14nsXPTCStubBase6Stub" #n "Ev\n\t" \
    ".else\n\t" \
    ".size	_ZN14nsXPTCStubBase7Stub" #n "Ev,.-_ZN14nsXPTCStubBase7Stub" #n "Ev\n\t" \
    ".endif");

// static nsresult SharedStub(PRUint32 methodIndex)
asm(".section   \".text\"\n\t"
    ".align     2\n\t"
    ".type      SharedStub,@function\n\t"
    "SharedStub:\n\t"
    // make room for gpregs (48), fpregs (64)
    "pushq      %rbp\n\t"
    "movq       %rsp,%rbp\n\t"
    "subq       $112,%rsp\n\t"
    // save GP registers
    "movq       %rdi,-112(%rbp)\n\t"
    "movq       %rsi,-104(%rbp)\n\t"
    "movq       %rdx, -96(%rbp)\n\t"
    "movq       %rcx, -88(%rbp)\n\t"
    "movq       %r8 , -80(%rbp)\n\t"
    "movq       %r9 , -72(%rbp)\n\t"
    "leaq       -112(%rbp),%rcx\n\t"
    // save FP registers
    "movsd      %xmm0,-64(%rbp)\n\t"
    "movsd      %xmm1,-56(%rbp)\n\t"
    "movsd      %xmm2,-48(%rbp)\n\t"
    "movsd      %xmm3,-40(%rbp)\n\t"
    "movsd      %xmm4,-32(%rbp)\n\t"
    "movsd      %xmm5,-24(%rbp)\n\t"
    "movsd      %xmm6,-16(%rbp)\n\t"
    "movsd      %xmm7, -8(%rbp)\n\t"
    "leaq       -64(%rbp),%r8\n\t"
    // rdi has the 'self' pointer already
    "movl       %eax,%esi\n\t"
    "leaq       16(%rbp),%rdx\n\t"
    "call       PrepareAndDispatch@plt\n\t"
    "leave\n\t"
    "ret\n\t"
    ".size      SharedStub,.-SharedStub");

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ASSERTION(0,"nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"

#else
#error "Unsupported compiler. Use gcc >= 3.1 for Linux/x86-64."
#endif /* __GNUC__ */
