/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * Version: MPL 1.1
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
 * Netscape Communications Corp, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Brendan Eich     <brendan@mozilla.org>
 *   Thiemo Seufer    <seufer@csv.ica.uni-stuttgart.de>
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

/* This code is for MIPS using the O32 ABI. */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"


extern "C" uint32
invoke_count_words(PRUint32 paramCount, nsXPTCVariant* s)
{
    // Count a word for a0 even though it's never stored or loaded
    // We do this only for alignment of register pairs.
    PRUint32 result = 1;
    for (PRUint32 i = 0; i < paramCount; i++, result++, s++)
    {
        if (s->IsPtrData())
            continue;

        switch(s->type)
        {
        case nsXPTType::T_I64    :
        case nsXPTType::T_U64    :
        case nsXPTType::T_DOUBLE :
	    if (result & 1)
		result++;
	    result++;
	    break;

        default:
            break;
        }
    }
    return (result + 1) & ~(PRUint32)1;
}

extern "C" void
invoke_copy_to_stack(PRUint32* d, PRUint32 paramCount,
                     nsXPTCVariant* s)
{
    // Skip the unused a0 slot, which we keep only for register pair alignment.
    d++;

    for (PRUint32 i = 0; i < paramCount; i++, d++, s++)
    {
        if (s->IsPtrData())
        {
            *((void**)d) = s->ptr;
            continue;
        }

        switch(s->type)
        {
        case nsXPTType::T_I8     : *d = (PRUint32) s->val.i8;   break;
        case nsXPTType::T_I16    : *d = (PRUint32) s->val.i16;  break;
        case nsXPTType::T_I32    : *d = (PRUint32) s->val.i32;  break;
        case nsXPTType::T_I64    :
            if ((PRWord)d & 4) d++;
            *((PRInt64*) d)  = s->val.i64;    d++;
            break;
        case nsXPTType::T_U8     : *d = (PRUint32) s->val.u8;   break;
        case nsXPTType::T_U16    : *d = (PRUint32) s->val.u16;  break;
        case nsXPTType::T_U32    : *d = (PRUint32) s->val.u32;  break;
        case nsXPTType::T_U64    :
            if ((PRWord)d & 4) d++;
            *((PRUint64*) d) = s->val.u64;    d++;
            break;
        case nsXPTType::T_FLOAT  : *((float*)   d) = s->val.f;  break;
        case nsXPTType::T_DOUBLE :
            if ((PRWord)d & 4) d++;
            *((double*)   d) = s->val.d;      d++;
            break;
        case nsXPTType::T_BOOL   : *d = (bool)  s->val.b;     break;
        case nsXPTType::T_CHAR   : *d = (char)    s->val.c;     break;
        case nsXPTType::T_WCHAR  : *d = (wchar_t) s->val.wc;    break;
        default:
            *((void**)d) = s->val.p;
            break;
        }
    }
}

extern "C" nsresult _NS_InvokeByIndex_P(nsISupports* that, PRUint32 methodIndex,
                                        PRUint32 paramCount,
                                        nsXPTCVariant* params);

EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex_P(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params)
{
    return _NS_InvokeByIndex_P(that, methodIndex, paramCount, params);
}
