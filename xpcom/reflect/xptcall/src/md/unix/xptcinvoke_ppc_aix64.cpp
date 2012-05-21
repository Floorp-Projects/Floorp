/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

#ifdef _AIX

extern "C" void
invoke_copy_to_stack(PRUint64* d, PRUint32 paramCount, nsXPTCVariant* s, double *fprData)
{
/*
    We need to copy the parameters for this function to locals and use them
    from there since the parameters occupy the same stack space as the stack
    we're trying to populate.
*/
    PRUint64 *l_d = d;
    nsXPTCVariant *l_s = s;
    PRUint32 l_paramCount = paramCount, fpCount = 0;
    double *l_fprData = fprData;

    for(PRUint32 i = 0; i < l_paramCount; i++, l_d++, l_s++)
    {
        if(l_s->IsPtrData())
        {
            *l_d = (PRUint64)l_s->ptr;
            continue;
        }
        switch(l_s->type)
        {
        case nsXPTType::T_I8:    *l_d = (PRUint64)l_s->val.i8;   break;
        case nsXPTType::T_I16:   *l_d = (PRUint64)l_s->val.i16;  break;
        case nsXPTType::T_I32:   *l_d = (PRUint64)l_s->val.i32;  break;
        case nsXPTType::T_I64:   *l_d = (PRUint64)l_s->val.i64;  break;
        case nsXPTType::T_U8:    *l_d = (PRUint64)l_s->val.u8;   break;
        case nsXPTType::T_U16:   *l_d = (PRUint64)l_s->val.u16;  break;
        case nsXPTType::T_U32:   *l_d = (PRUint64)l_s->val.u32;  break;
        case nsXPTType::T_U64:   *l_d = (PRUint64)l_s->val.u64;  break;
        case nsXPTType::T_BOOL:  *l_d = (PRUint64)l_s->val.b;    break;
        case nsXPTType::T_CHAR:  *l_d = (PRUint64)l_s->val.c;    break;
        case nsXPTType::T_WCHAR: *l_d = (PRUint64)l_s->val.wc;   break;

        case nsXPTType::T_DOUBLE:
            *((double*)l_d) = l_s->val.d;
            if(fpCount < 13)
                l_fprData[fpCount++] = l_s->val.d;
            break;
        case nsXPTType::T_FLOAT:
            *((float*)l_d) = l_s->val.f;
            if(fpCount < 13)
                l_fprData[fpCount++] = l_s->val.f;
            break;
        default:
            // all the others are plain pointer types
            *l_d = (PRUint64)l_s->val.p;
            break;
        }
    }
}
#endif

