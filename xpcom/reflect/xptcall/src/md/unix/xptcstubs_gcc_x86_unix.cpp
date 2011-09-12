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
#include "xptc_platforms_unixish_x86.h"
#include "xptc_gcc_x86_unix.h"

extern "C" {
static nsresult ATTRIBUTE_USED
__attribute__ ((regparm (3)))
PrepareAndDispatch(uint32 methodIndex, nsXPTCStubBase* self, PRUint32* args)
{
#define PARAM_BUFFER_COUNT     16

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
	    dp->val.p = (void*) *ap;
        switch(type)
        {
        case nsXPTType::T_I64    : dp->val.i64 = *((PRInt64*) ap); ap++; break;
        case nsXPTType::T_U64    : dp->val.u64 = *((PRUint64*)ap); ap++; break;
        case nsXPTType::T_DOUBLE : dp->val.d   = *((double*)  ap); ap++; break;
        }
    }

    result = self->mOuter->CallMethod((PRUint16)methodIndex, info, dispatchParams);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}
} // extern "C"

// gcc3 mangling tends to insert the length of the method name
#define STUB_ENTRY(n) \
asm(".text\n\t" \
    ".align	2\n\t" \
    ".if	" #n " < 10\n\t" \
    ".globl	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase5Stub" #n "Ev\n\t" \
    ".hidden	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase5Stub" #n "Ev\n\t" \
    ".type	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase5Stub" #n "Ev,@function\n" \
    SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase5Stub" #n "Ev:\n\t" \
    ".elseif	" #n " < 100\n\t" \
    ".globl	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase6Stub" #n "Ev\n\t" \
    ".hidden	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase6Stub" #n "Ev\n\t" \
    ".type	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase6Stub" #n "Ev,@function\n" \
    SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase6Stub" #n "Ev:\n\t" \
    ".elseif    " #n " < 1000\n\t" \
    ".globl     " SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase7Stub" #n "Ev\n\t" \
    ".hidden    " SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase7Stub" #n "Ev\n\t" \
    ".type      " SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase7Stub" #n "Ev,@function\n" \
    SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase7Stub" #n "Ev:\n\t" \
    ".else\n\t" \
    ".err	\"stub number " #n " >= 1000 not yet supported\"\n\t" \
    ".endif\n\t" \
    "movl	$" #n ", %eax\n\t" \
    "jmp	" SYMBOL_UNDERSCORE "SharedStub\n\t" \
    ".if	" #n " < 10\n\t" \
    ".size	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase5Stub" #n "Ev,.-" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase5Stub" #n "Ev\n\t" \
    ".elseif	" #n " < 100\n\t" \
    ".size	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase6Stub" #n "Ev,.-" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase6Stub" #n "Ev\n\t" \
    ".else\n\t" \
    ".size	" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase7Stub" #n "Ev,.-" SYMBOL_UNDERSCORE "_ZN14nsXPTCStubBase7Stub" #n "Ev\n\t" \
    ".endif");

// static nsresult SharedStub(PRUint32 methodIndex) __attribute__((regparm(1)))
asm(".text\n\t"
    ".align	2\n\t"
    ".type	" SYMBOL_UNDERSCORE "SharedStub,@function\n\t"
    SYMBOL_UNDERSCORE "SharedStub:\n\t"
    "leal	0x08(%esp), %ecx\n\t"
    "movl	0x04(%esp), %edx\n\t"
    "jmp	" SYMBOL_UNDERSCORE "PrepareAndDispatch\n\t"
    ".size	" SYMBOL_UNDERSCORE "SharedStub,.-" SYMBOL_UNDERSCORE "SharedStub");

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"

void
xptc_dummy()
{
}
