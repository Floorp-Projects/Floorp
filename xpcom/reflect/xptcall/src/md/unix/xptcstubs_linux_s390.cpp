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

static nsresult
PrepareAndDispatch(nsXPTCStubBase* self, uint32 methodIndex, 
                   PRUint32* a_gpr, PRUint64 *a_fpr, PRUint32 *a_ov)
{
#define PARAM_BUFFER_COUNT     16

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = NULL;
    nsIInterfaceInfo* iface_info = NULL;
    const nsXPTMethodInfo* info;
    PRUint8 paramCount;
    PRUint8 i;
    nsresult result = NS_ERROR_FAILURE;

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

    PRUint32 gpr = 1, fpr = 0;

    for(i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &dispatchParams[i];

        if(param.IsOut() || !type.IsArithmetic())
        {
            if (gpr < 5)
                dp->val.p = (void*) *a_gpr++, gpr++;
            else
                dp->val.p = (void*) *a_ov++;
            continue;
        }
        // else
        switch(type)
        {
        case nsXPTType::T_I8     : 
            if (gpr < 5)
                dp->val.i8  = *((PRInt32*) a_gpr), a_gpr++, gpr++;
            else
                dp->val.i8  = *((PRInt32*) a_ov ), a_ov++;
            break;
        case nsXPTType::T_I16    : 
            if (gpr < 5)
                dp->val.i16 = *((PRInt32*) a_gpr), a_gpr++, gpr++;
            else
                dp->val.i16 = *((PRInt32*) a_ov ), a_ov++;
            break;
        case nsXPTType::T_I32    : 
            if (gpr < 5)
                dp->val.i32 = *((PRInt32*) a_gpr), a_gpr++, gpr++;
            else
                dp->val.i32 = *((PRInt32*) a_ov ), a_ov++;
            break;
        case nsXPTType::T_I64    : 
            if (gpr < 4)
                dp->val.i64 = *((PRInt64*) a_gpr), a_gpr+=2, gpr+=2;
            else
                dp->val.i64 = *((PRInt64*) a_ov ), a_ov+=2, gpr=5;
            break;
        case nsXPTType::T_U8     : 
            if (gpr < 5)
                dp->val.u8  = *((PRUint32*)a_gpr), a_gpr++, gpr++;
            else
                dp->val.u8  = *((PRUint32*)a_ov ), a_ov++;
            break;
        case nsXPTType::T_U16    : 
            if (gpr < 5)
                dp->val.u16 = *((PRUint32*)a_gpr), a_gpr++, gpr++;
            else
                dp->val.u16 = *((PRUint32*)a_ov ), a_ov++;
            break;
        case nsXPTType::T_U32    : 
            if (gpr < 5)
                dp->val.u32 = *((PRUint32*)a_gpr), a_gpr++, gpr++;
            else
                dp->val.u32 = *((PRUint32*)a_ov ), a_ov++;
            break;
        case nsXPTType::T_U64    : 
            if (gpr < 4)
                dp->val.u64 = *((PRUint64*)a_gpr), a_gpr+=2, gpr+=2;
            else
                dp->val.u64 = *((PRUint64*)a_ov ), a_ov+=2, gpr=5;
            break;
        case nsXPTType::T_FLOAT  : 
            if (fpr < 2)
                dp->val.f   = *((float*)   a_fpr), a_fpr++, fpr++;
            else
                dp->val.f   = *((float*)   a_ov ), a_ov++;
            break;
        case nsXPTType::T_DOUBLE : 
            if (fpr < 2)
                dp->val.d   = *((double*)  a_fpr), a_fpr++, fpr++;
            else
                dp->val.d   = *((double*)  a_ov ), a_ov+=2;
            break;
        case nsXPTType::T_BOOL   : 
            if (gpr < 5)
                dp->val.b   = *((PRUint32*)a_gpr), a_gpr++, gpr++;
            else
                dp->val.b   = *((PRUint32*)a_ov ), a_ov++;
            break;
        case nsXPTType::T_CHAR   : 
            if (gpr < 5)
                dp->val.c   = *((PRUint32*)a_gpr), a_gpr++, gpr++;
            else
                dp->val.c   = *((PRUint32*)a_ov ), a_ov++;
            break;
        case nsXPTType::T_WCHAR  : 
            if (gpr < 5)
                dp->val.wc  = *((PRUint32*)a_gpr), a_gpr++, gpr++;
            else
                dp->val.wc  = *((PRUint32*)a_ov ), a_ov++;
            break;
        default:
            NS_ASSERTION(0, "bad type");
            break;
        }
    }

    result = self->CallMethod((PRUint16)methodIndex, info, dispatchParams);

    NS_RELEASE(iface_info);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

#define STUB_ENTRY(n) \
nsresult nsXPTCStubBase::Stub##n() \
{                             \
    PRUint32 a_gpr[4];        \
    PRUint64 a_fpr[2];        \
    PRUint32 *a_ov;           \
                              \
    __asm__ __volatile__      \
    (                         \
        "l     %0,0(15)\n\t"  \
        "ahi   %0,96\n\t"     \
        "stm   3,6,0(%3)\n\t" \
        "std   0,%1\n\t"      \
        "std   2,%2\n\t"      \
        : "=&a" (a_ov),       \
          "=m" (a_fpr[0]),    \
          "=m" (a_fpr[1])     \
        : "a" (a_gpr)         \
       : "memory", "cc",      \
         "3", "4", "5", "6"   \
    );                        \
                              \
    return PrepareAndDispatch(this, n, a_gpr, a_fpr, a_ov); \
}

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ASSERTION(0,"nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"

