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

static nsresult
PrepareAndDispatch(nsXPTCStubBase* self, uint32 methodIndex, PRUint32* args)
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
	    dp->val.p = (void*) *ap;
        switch(type)
        {
        case nsXPTType::T_I64    : dp->val.i64 = *((PRInt64*) ap); ap++; break;
        case nsXPTType::T_U64    : dp->val.u64 = *((PRUint64*)ap); ap++; break;
        case nsXPTType::T_DOUBLE : dp->val.d   = *((double*)  ap); ap++; break;
        }
    }

    result = self->mOuter->
        CallMethod((PRUint16)methodIndex, info, dispatchParams);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

#ifdef KEEP_STACK_16_BYTE_ALIGNED
/* Make sure the stack is 16-byte aligned.  Do that by aligning to 16 bytes and
 * then subtracting 4 so the three subsequent pushes result in a 16-byte aligned
 * stack. */
#define ALIGN_STACK_DECL \
  unsigned int saved_esp;

#define ALIGN_STACK_SAVE \
  "movl %%esp, %3\n\t"

#define ALIGN_STACK_ALIGN \
  "addl $0x4, %%esp\n\t" \
  "andl $0xfffffff0, %%esp\n\t" \
  "subl $0x4, %%esp\n\t"

#define STACK_RESTORE \
  "movl %3, %%esp\n"

#define ALIGN_STACK_REGS_IN \
  , "=r"(saved_esp) /* 3 */

#define ALIGN_STACK_REGS_OUT \
  , "3"(saved_esp)

#else
#define ALIGN_STACK_DECL
#define ALIGN_STACK_SAVE
#define ALIGN_STACK_ALIGN
#define STACK_RESTORE \
  "addl $12, %%esp\n"
#define ALIGN_STACK_REGS_IN
#define ALIGN_STACK_REGS_OUT
#endif

#define STUB_ENTRY(n) \
nsresult nsXPTCStubBase::Stub##n() \
{ \
  register nsresult (*method) (nsXPTCStubBase *, uint32, PRUint32 *) = PrepareAndDispatch; \
  int temp0, temp1; \
  register nsresult result; \
  ALIGN_STACK_DECL \
  __asm__ __volatile__( \
    ALIGN_STACK_SAVE \
    ALIGN_STACK_ALIGN \
    "leal   0x0c(%%ebp), %%ecx\n\t"    /* args */ \
    "pushl  %%ecx\n\t" \
    "pushl  $"#n"\n\t"                 /* method index */ \
    "movl   0x08(%%ebp), %%ecx\n\t"    /* this */ \
    "pushl  %%ecx\n\t" \
    "call   *%%edx\n\t"                /* PrepareAndDispatch */ \
    STACK_RESTORE                      /* "addl $12, %%esp" or restore saved */ \
    : "=a" (result),    /* %0 */ \
      "=&c" (temp0),    /* %1 */ \
      "=d" (temp1)      /* %2 */ \
      ALIGN_STACK_REGS_IN \
    : "2" (method)      /* %2 */ \
      ALIGN_STACK_REGS_OUT \
    : "memory" \
	); \
    return result; \
}

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"
