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
 *   Franz.Sirl-kernel@lauterbach.com (Franz Sirl)
 *   beard@netscape.com (Patrick Beard)
 *   waterson@netscape.com (Chris Waterson)
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

// Platform specific code to invoke XPCOM methods on native objects

// The purpose of XPTC_InvokeByIndex() is to map a platform
// indepenpent call to the platform ABI. To do that,
// XPTC_InvokeByIndex() has to determine the method to call via vtable
// access. The parameters for the method are read from the
// nsXPTCVariant* and prepared for th native ABI.  For the Linux/PPC
// ABI this means that the first 8 integral and floating point
// parameters are passed in registers.

#include "xptcprivate.h"

// 8 integral parameters are passed in registers
#define GPR_COUNT     8

// 8 floating point parameters are passed in registers, floats are
// promoted to doubles when passed in registers
#define FPR_COUNT     8

extern "C" PRUint32
invoke_count_words(PRUint32 paramCount, nsXPTCVariant* s)
{
  return PRUint32(((paramCount * 2) + 3) & ~3);
}

extern "C" void
invoke_copy_to_stack(PRUint32* d,
                     PRUint32 paramCount,
                     nsXPTCVariant* s, 
                     PRUint32* gpregs,
                     double* fpregs)
{
    PRUint32 gpr = 1; // skip one GP reg for 'that'
    PRUint32 fpr = 0;
    PRUint32 tempu32;
    PRUint64 tempu64;
    
    for(uint32 i = 0; i < paramCount; i++, s++) {
        if(s->IsPtrData())
            tempu32 = (PRUint32) s->ptr;
        else {
            switch(s->type) {
            case nsXPTType::T_FLOAT:                                  break;
            case nsXPTType::T_DOUBLE:                                 break;
            case nsXPTType::T_I8:     tempu32 = s->val.i8;            break;
            case nsXPTType::T_I16:    tempu32 = s->val.i16;           break;
            case nsXPTType::T_I32:    tempu32 = s->val.i32;           break;
            case nsXPTType::T_I64:    tempu64 = s->val.i64;           break;
            case nsXPTType::T_U8:     tempu32 = s->val.u8;            break;
            case nsXPTType::T_U16:    tempu32 = s->val.u16;           break;
            case nsXPTType::T_U32:    tempu32 = s->val.u32;           break;
            case nsXPTType::T_U64:    tempu64 = s->val.u64;           break;
            case nsXPTType::T_BOOL:   tempu32 = s->val.b;             break;
            case nsXPTType::T_CHAR:   tempu32 = s->val.c;             break;
            case nsXPTType::T_WCHAR:  tempu32 = s->val.wc;            break;
            default:                  tempu32 = (PRUint32) s->val.p;  break;
            }
        }

        if (!s->IsPtrData() && s->type == nsXPTType::T_DOUBLE) {
            if (fpr < FPR_COUNT)
                fpregs[fpr++]    = s->val.d;
            else {
                if ((PRUint32) d & 4) d++; // doubles are 8-byte aligned on stack
                *((double*) d) = s->val.d;
                d += 2;
            }
        }
        else if (!s->IsPtrData() && s->type == nsXPTType::T_FLOAT) {
            if (fpr < FPR_COUNT)
                fpregs[fpr++]   = s->val.f; // if passed in registers, floats are promoted to doubles
            else
                *((float*) d++) = s->val.f;
        }
        else if (!s->IsPtrData() && (s->type == nsXPTType::T_I64
                                     || s->type == nsXPTType::T_U64)) {
            if ((gpr + 1) < GPR_COUNT) {
                if (gpr & 1) gpr++; // longlongs are aligned in odd/even register pairs, eg. r5/r6
                *((PRUint64*) &gpregs[gpr]) = tempu64;
                gpr += 2;
            }
            else {
                if ((PRUint32) d & 4) d++; // longlongs are 8-byte aligned on stack
                *((PRUint64*) d)            = tempu64;
                d += 2;
            }
        }
        else {
            if (gpr < GPR_COUNT)
                gpregs[gpr++] = tempu32;
            else
                *d++          = tempu32;
        }
        
    }
}

extern "C"
XPTC_PUBLIC_API(nsresult)
XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params);
