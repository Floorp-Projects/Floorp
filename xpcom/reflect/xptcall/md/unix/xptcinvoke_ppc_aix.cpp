/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

#ifndef AIX
#error "This code is for PowerPC only"
#endif

extern "C" void
invoke_copy_to_stack(uint32_t* d, uint32_t paramCount, nsXPTCVariant* s, double *fprData)
{
/*
    We need to copy the parameters for this function to locals and use them
    from there since the parameters occupy the same stack space as the stack
    we're trying to populate.
*/
    uint32_t *l_d = d;
    nsXPTCVariant *l_s = s;
    uint32_t l_paramCount = paramCount, fpCount = 0;
    double *l_fprData = fprData;

    typedef struct {
        uint32_t hi;
        uint32_t lo;
    } DU;               // have to move 64 bit entities as 32 bit halves since
                        // stack slots are not guaranteed 16 byte aligned

    for(uint32_t i = 0; i < l_paramCount; i++, l_d++, l_s++)
    {
        if(l_s->IsPtrData())
        {
            *((void**)l_d) = l_s->ptr;
            continue;
        }
        switch(l_s->type)
        {
        case nsXPTType::T_I8     : *((int32_t*)  l_d) = l_s->val.i8;          break;
        case nsXPTType::T_I16    : *((int32_t*)  l_d) = l_s->val.i16;         break;
        case nsXPTType::T_I32    : *((int32_t*)  l_d) = l_s->val.i32;         break;
        case nsXPTType::T_I64    : 
        case nsXPTType::T_U64    :
            *((uint32_t*) l_d++) = ((DU *)l_s)->hi;
            *((uint32_t*) l_d) = ((DU *)l_s)->lo;
            break;
        case nsXPTType::T_DOUBLE :
            *((uint32_t*) l_d++) = ((DU *)l_s)->hi;
            *((uint32_t*) l_d) = ((DU *)l_s)->lo;
            if(fpCount < 13)
                l_fprData[fpCount++] = l_s->val.d;
            break;
        case nsXPTType::T_U8     : *((uint32_t*) l_d) = l_s->val.u8;          break;
        case nsXPTType::T_U16    : *((uint32_t*) l_d) = l_s->val.u16;         break;
        case nsXPTType::T_U32    : *((uint32_t*) l_d) = l_s->val.u32;         break;
        case nsXPTType::T_FLOAT  :
            *((float*)  l_d) = l_s->val.f;
            if(fpCount < 13)
                l_fprData[fpCount++] = l_s->val.f;
            break;
        case nsXPTType::T_BOOL   : *((uint32_t*) l_d) = l_s->val.b;           break;
        case nsXPTType::T_CHAR   : *((uint32_t*) l_d) = l_s->val.c;           break;
        case nsXPTType::T_WCHAR  : *((int32_t*)  l_d) = l_s->val.wc;          break;
        default:
            // all the others are plain pointer types
            *((void**)l_d) = l_s->val.p;
            break;
        }
    }
}

