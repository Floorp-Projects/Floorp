/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

extern "C" {

// Remember that these 'words' are 32bit DWORDS

uint32_t
invoke_count_words(uint32_t paramCount, nsXPTCVariant* s)
{
    uint32_t result = 0;
    for(uint32_t i = 0; i < paramCount; i++, s++)
    {
        if(s->IsPtrData())
        {
            result++;
            continue;
        }
        result++;
        switch(s->type)
        {
        case nsXPTType::T_I64    :
        case nsXPTType::T_U64    :
        case nsXPTType::T_DOUBLE :
            result++;
            break;
        }
    }
    return result;
}

void
invoke_copy_to_stack(uint32_t paramCount, nsXPTCVariant* s, uint32_t* d)
{
    for(uint32_t i = 0; i < paramCount; i++, d++, s++)
    {
        if(s->IsPtrData())
        {
            *((void**)d) = s->ptr;
            continue;
        }

/* XXX: the following line is here (rather than as the default clause in
 *      the following switch statement) so that the Sun native compiler
 *      will generate the correct assembly code on the Solaris Intel
 *      platform. See the comments in bug #28817 for more details.
 */

        *((void**)d) = s->val.p;

        switch(s->type)
        {
        case nsXPTType::T_I64    : *((int64_t*) d) = s->val.i64; d++;    break;
        case nsXPTType::T_U64    : *((uint64_t*)d) = s->val.u64; d++;    break;
        case nsXPTType::T_DOUBLE : *((double*)  d) = s->val.d;   d++;    break;
        }
    }
}

}
