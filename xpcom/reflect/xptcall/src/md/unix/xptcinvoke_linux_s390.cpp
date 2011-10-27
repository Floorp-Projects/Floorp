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
 * Portions created by the Initial Developer are Copyright (C) 1998
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

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"


static PRUint32
invoke_count_words(PRUint32 paramCount, nsXPTCVariant* s)
{
    PRUint32 overflow = 0, gpr = 1 /*this*/, fpr = 0;
    for(PRUint32 i = 0; i < paramCount; i++, s++)
    {
        if(s->IsPtrData())
        {
            if (gpr < 5) gpr++; else overflow++;
            continue;
        }
        switch(s->type)
        {
        case nsXPTType::T_I8     :
        case nsXPTType::T_I16    :
        case nsXPTType::T_I32    :
            if (gpr < 5) gpr++; else overflow++;
            break;
        case nsXPTType::T_I64    :
            if (gpr < 4) gpr+=2; else gpr=5, overflow+=2;
            break;
        case nsXPTType::T_U8     :
        case nsXPTType::T_U16    :
        case nsXPTType::T_U32    :
            if (gpr < 5) gpr++; else overflow++;
            break;
        case nsXPTType::T_U64    :
            if (gpr < 4) gpr+=2; else gpr=5, overflow+=2;
            break;
        case nsXPTType::T_FLOAT  :
            if (fpr < 2) fpr++; else overflow++;
            break;
        case nsXPTType::T_DOUBLE :
            if (fpr < 2) fpr++; else overflow+=2;
            break;
        case nsXPTType::T_BOOL   :
        case nsXPTType::T_CHAR   :
        case nsXPTType::T_WCHAR  :
            if (gpr < 5) gpr++; else overflow++;
            break;
        default:
            // all the others are plain pointer types
            if (gpr < 5) gpr++; else overflow++;
            break;
        }
    }
    /* Round up number of overflow words to ensure stack
       stays aligned to 8 bytes.  */
    return (overflow + 1) & ~1;
}

static void
invoke_copy_to_stack(PRUint32 paramCount, nsXPTCVariant* s, PRUint32* d_ov, PRUint32 overflow)
{
    PRUint32 *d_gpr = d_ov + overflow; 
    PRUint64 *d_fpr = (PRUint64 *)(d_gpr + 4);
    PRUint32 gpr = 1 /*this*/, fpr = 0;

    for(PRUint32 i = 0; i < paramCount; i++, s++)
    {
        if(s->IsPtrData())
        {
            if (gpr < 5) 
                *((void**)d_gpr) = s->ptr, d_gpr++, gpr++;
            else
                *((void**)d_ov ) = s->ptr, d_ov++;
            continue;
        }
        switch(s->type)
        {
        case nsXPTType::T_I8     : 
            if (gpr < 5)
                *((PRInt32*) d_gpr) = s->val.i8, d_gpr++, gpr++;
            else
                *((PRInt32*) d_ov ) = s->val.i8, d_ov++;
            break;
        case nsXPTType::T_I16    : 
            if (gpr < 5)
                *((PRInt32*) d_gpr) = s->val.i16, d_gpr++, gpr++;
            else
                *((PRInt32*) d_ov ) = s->val.i16, d_ov++;
            break;
        case nsXPTType::T_I32    : 
            if (gpr < 5)
                *((PRInt32*) d_gpr) = s->val.i32, d_gpr++, gpr++;
            else
                *((PRInt32*) d_ov ) = s->val.i32, d_ov++;
            break;
        case nsXPTType::T_I64    : 
            if (gpr < 4)
                *((PRInt64*) d_gpr) = s->val.i64, d_gpr+=2, gpr+=2;
            else
                *((PRInt64*) d_ov ) = s->val.i64, d_ov+=2, gpr=5;
            break;
        case nsXPTType::T_U8     : 
            if (gpr < 5)
                *((PRUint32*) d_gpr) = s->val.u8, d_gpr++, gpr++;
            else
                *((PRUint32*) d_ov ) = s->val.u8, d_ov++;
            break;
        case nsXPTType::T_U16    : 
            if (gpr < 5)
                *((PRUint32*)d_gpr) = s->val.u16, d_gpr++, gpr++;
            else
                *((PRUint32*)d_ov ) = s->val.u16, d_ov++;
            break;
        case nsXPTType::T_U32    : 
            if (gpr < 5)
                *((PRUint32*)d_gpr) = s->val.u32, d_gpr++, gpr++;
            else
                *((PRUint32*)d_ov ) = s->val.u32, d_ov++;
            break;
        case nsXPTType::T_U64    : 
            if (gpr < 4)
                *((PRUint64*)d_gpr) = s->val.u64, d_gpr+=2, gpr+=2;
            else
                *((PRUint64*)d_ov ) = s->val.u64, d_ov+=2, gpr=5;
            break;
        case nsXPTType::T_FLOAT  : 
            if (fpr < 2)
                *((float*)   d_fpr) = s->val.f, d_fpr++, fpr++;
            else
                *((float*)   d_ov ) = s->val.f, d_ov++;
            break;
        case nsXPTType::T_DOUBLE : 
            if (fpr < 2)
                *((double*)  d_fpr) = s->val.d, d_fpr++, fpr++;
            else
                *((double*)  d_ov ) = s->val.d, d_ov+=2;
            break;
        case nsXPTType::T_BOOL   : 
            if (gpr < 5)
                *((PRUint32*)d_gpr) = s->val.b, d_gpr++, gpr++;
            else
                *((PRUint32*)d_ov ) = s->val.b, d_ov++;
            break;
        case nsXPTType::T_CHAR   : 
            if (gpr < 5)
                *((PRUint32*)d_gpr) = s->val.c, d_gpr++, gpr++;
            else
                *((PRUint32*)d_ov ) = s->val.c, d_ov++;
            break;
        case nsXPTType::T_WCHAR  : 
            if (gpr < 5)
                *((PRUint32*)d_gpr) = s->val.wc, d_gpr++, gpr++;
            else
                *((PRUint32*)d_ov ) = s->val.wc, d_ov++;
            break;
        default:
            // all the others are plain pointer types
            if (gpr < 5) 
                *((void**)   d_gpr) = s->val.p, d_gpr++, gpr++;
            else
                *((void**)   d_ov ) = s->val.p, d_ov++;
            break;
        }
    }
}

typedef nsresult (*vtable_func)(nsISupports *, PRUint32, PRUint32, PRUint32, PRUint32, double, double);

EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                 PRUint32 paramCount, nsXPTCVariant* params)
{
    vtable_func *vtable = *reinterpret_cast<vtable_func **>(that);
    vtable_func method = vtable[methodIndex];
    PRUint32 overflow = invoke_count_words (paramCount, params);
    PRUint32 *stack_space = reinterpret_cast<PRUint32 *>(__builtin_alloca((overflow + 8 /* 4 32-bits gpr + 2 64-bits fpr */) * 4));

    invoke_copy_to_stack(paramCount, params, stack_space, overflow);

    PRUint32 *d_gpr = stack_space + overflow;
    double *d_fpr = reinterpret_cast<double *>(d_gpr + 4);

    return method(that, d_gpr[0], d_gpr[1], d_gpr[2], d_gpr[3],  d_fpr[0], d_fpr[1]);
}

