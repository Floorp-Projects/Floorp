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

// Remember that these 'words' are 32bit DWORDS

extern "C" {
    static PRUint32
    invoke_count_words(PRUint32 paramCount, nsXPTCVariant* s)
    {
        PRUint32 result = 0;
        for(PRUint32 i = 0; i < paramCount; i++, s++)
        {
            if(s->IsPtrData())
            {
                result++;
                continue;
            }
            switch(s->type)
            {
            case nsXPTType::T_I8     :
            case nsXPTType::T_I16    :
            case nsXPTType::T_I32    :
                result++;
                break;
            case nsXPTType::T_I64    :
                result+=2;
                break;
            case nsXPTType::T_U8     :
            case nsXPTType::T_U16    :
            case nsXPTType::T_U32    :
                result++;
                break;
            case nsXPTType::T_U64    :
                result+=2;
                break;
            case nsXPTType::T_FLOAT  :
                result++;
                break;
            case nsXPTType::T_DOUBLE :
                result+=2;
                break;
            case nsXPTType::T_BOOL   :
            case nsXPTType::T_CHAR   :
            case nsXPTType::T_WCHAR  :
                result++;
                break;
            default:
                // all the others are plain pointer types
                result++;
                break;
            }
        }
        return result;
    }

    void
    invoke_copy_to_stack(PRUint32* d, PRUint32 paramCount, nsXPTCVariant* s)
    {
        for(PRUint32 i = 0; i < paramCount; i++, d++, s++)
        {
            if(s->IsPtrData())
            {
                *((void**)d) = s->ptr;
                continue;
            }
            switch(s->type)
            {
            // 8 and 16 bit types should be promoted to 32 bits when copying
            // onto the stack.
            case nsXPTType::T_I8     : *((PRUint32*)d) = s->val.i8;          break;
            case nsXPTType::T_I16    : *((PRUint32*)d) = s->val.i16;         break;
            case nsXPTType::T_I32    : *((PRInt32*) d) = s->val.i32;         break;
            case nsXPTType::T_I64    : *((PRInt64*) d) = s->val.i64; d++;    break;
            case nsXPTType::T_U8     : *((PRUint32*)d) = s->val.u8;          break;
            case nsXPTType::T_U16    : *((PRUint32*)d) = s->val.u16;         break;
            case nsXPTType::T_U32    : *((PRUint32*)d) = s->val.u32;         break;
            case nsXPTType::T_U64    : *((PRUint64*)d) = s->val.u64; d++;    break;
            case nsXPTType::T_FLOAT  : *((float*)   d) = s->val.f;           break;
            case nsXPTType::T_DOUBLE : *((double*)  d) = s->val.d;   d++;    break;
            case nsXPTType::T_BOOL   : *((PRUint32*)d) = s->val.b;           break;
            case nsXPTType::T_CHAR   : *((PRUint32*)d) = s->val.c;           break;
            case nsXPTType::T_WCHAR  : *((wchar_t*) d) = s->val.wc;          break;

            default:
                // all the others are plain pointer types
                *((void**)d) = s->val.p;
                break;
            }
        }
    }
}

EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex_P(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params)
{
    PRUint32 result, n;

    n = invoke_count_words(paramCount, params) * 4;

 __asm__ __volatile__(
    "subl  %5, %%sp\n\t"      /* make room for params */
    "movel %4, %%sp@-\n\t"
    "movel %3, %%sp@-\n\t"
    "pea   %%sp@(8)\n\t"
    "jbsr  invoke_copy_to_stack\n\t"   /* copy params */
    "addw  #12, %%sp\n\t"
    "movel %1, %%sp@-\n\t"
    "movel %1@, %%a0\n\t"
    "movel %%a0@(%2:l:4), %%a0\n\t"
    "jbsr  %%a0@\n\t"         /* safe to not cleanup sp */
    "lea   %%sp@(4,%5:l), %%sp\n\t"
    "movel %%d0, %0"
    : "=d" (result)         /* %0 */
    : "a" (that),           /* %1 */
      "d" (methodIndex),    /* %2 */
      "g" (paramCount),     /* %3 */
      "g" (params),         /* %4 */
      "d" (n)               /* %5 */
    : "a0", "a1", "d0", "d1", "memory"
    );
  
  return result;
}
