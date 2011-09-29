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
 *   Franz.Sirl-kernel@lauterbach.com (Franz Sirl)
 *   beard@netscape.com (Patrick Beard)
 *   waterson@netscape.com (Chris Waterson)
 *   bigeasy@linutronix.de (Sebastian Andrzej Siewior)
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

// Implement shared vtbl methods.

#include "xptcprivate.h"
#include "xptiprivate.h"

// The Linux/PPC ABI (aka PPC/SYSV ABI) passes the first 8 integral
// parameters and the first 8 floating point parameters in registers
// (r3-r10 and f1-f8), no stack space is allocated for these by the
// caller.  The rest of the parameters are passed in the callers stack
// area. The stack pointer has to retain 16-byte alignment, longlongs
// and doubles are aligned on 8-byte boundaries.
#ifndef __NO_FPRS__
#define PARAM_BUFFER_COUNT     16
#define GPR_COUNT               8
#define FPR_COUNT               8
#else
#define PARAM_BUFFER_COUNT      8
#define GPR_COUNT               8
#endif
// PrepareAndDispatch() is called by SharedStub() and calls the actual method.
//
// - 'args[]' contains the arguments passed on stack
// - 'gprData[]' contains the arguments passed in integer registers
// - 'fprData[]' contains the arguments passed in floating point registers
// 
// The parameters are mapped into an array of type 'nsXPTCMiniVariant'
// and then the method gets called.

extern "C" nsresult
PrepareAndDispatch(nsXPTCStubBase* self,
                   PRUint32 methodIndex,
                   PRUint32* args,
                   PRUint32 *gprData,
                   double *fprData)
{
    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = NULL;
    const nsXPTMethodInfo* info = NULL;
    PRUint32 paramCount;
    PRUint32 i;
    nsresult result = NS_ERROR_FAILURE;

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(PRUint16(methodIndex), &info);
    NS_ASSERTION(info,"no method info");
    if (! info)
        return NS_ERROR_UNEXPECTED;

    paramCount = info->GetParamCount();

    // setup variant array pointer
    if(paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;

    NS_ASSERTION(dispatchParams,"no place for params");
    if (! dispatchParams)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32* ap = args;
    PRUint32 gpr = 1;    // skip one GPR register
#ifndef __NO_FPRS__
    PRUint32 fpr = 0;
#endif
    PRUint32 tempu32;
    PRUint64 tempu64;

    for(i = 0; i < paramCount; i++) {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &dispatchParams[i];
	
        if (!param.IsOut() && type == nsXPTType::T_DOUBLE) {
#ifndef __NO_FPRS__
            if (fpr < FPR_COUNT)
                dp->val.d = fprData[fpr++];
#else
            if (gpr & 1)
                gpr++;
            if (gpr + 1 < GPR_COUNT) {
                dp->val.d = *(double*) &gprData[gpr];
                gpr += 2;
            }
#endif
            else {
                if ((PRUint32) ap & 4) ap++; // doubles are 8-byte aligned on stack
                dp->val.d = *(double*) ap;
                ap += 2;
            }
            continue;
        }
        else if (!param.IsOut() && type == nsXPTType::T_FLOAT) {
#ifndef __NO_FPRS__
            if (fpr < FPR_COUNT)
                dp->val.f = (float) fprData[fpr++]; // in registers floats are passed as doubles
#else
            if (gpr  < GPR_COUNT)
                dp->val.f = *(float*) &gprData[gpr++];
#endif
            else
                dp->val.f = *(float*) ap++;
            continue;
        }
        else if (!param.IsOut() && (type == nsXPTType::T_I64
                                    || type == nsXPTType::T_U64)) {
            if (gpr & 1) gpr++; // longlongs are aligned in odd/even register pairs, eg. r5/r6
            if ((gpr + 1) < GPR_COUNT) {
                tempu64 = *(PRUint64*) &gprData[gpr];
                gpr += 2;
            }
            else {
                if ((PRUint32) ap & 4) ap++; // longlongs are 8-byte aligned on stack
                tempu64 = *(PRUint64*) ap;
                ap += 2;
            }
        }
        else {
            if (gpr < GPR_COUNT)
                tempu32 = gprData[gpr++];
            else
                tempu32 = *ap++;
        }

        if(param.IsOut() || !type.IsArithmetic()) {
            dp->val.p = (void*) tempu32;
            continue;
        }

        switch(type) {
        case nsXPTType::T_I8:      dp->val.i8  = (PRInt8)   tempu32; break;
        case nsXPTType::T_I16:     dp->val.i16 = (PRInt16)  tempu32; break;
        case nsXPTType::T_I32:     dp->val.i32 = (PRInt32)  tempu32; break;
        case nsXPTType::T_I64:     dp->val.i64 = (PRInt64)  tempu64; break;
        case nsXPTType::T_U8:      dp->val.u8  = (PRUint8)  tempu32; break;
        case nsXPTType::T_U16:     dp->val.u16 = (PRUint16) tempu32; break;
        case nsXPTType::T_U32:     dp->val.u32 = (PRUint32) tempu32; break;
        case nsXPTType::T_U64:     dp->val.u64 = (PRUint64) tempu64; break;
        case nsXPTType::T_BOOL:    dp->val.b   = (bool)   tempu32; break;
        case nsXPTType::T_CHAR:    dp->val.c   = (char)     tempu32; break;
        case nsXPTType::T_WCHAR:   dp->val.wc  = (wchar_t)  tempu32; break;

        default:
            NS_ERROR("bad type");
            break;
        }
    }

    result = self->mOuter->CallMethod((PRUint16)methodIndex,
                                      info,
                                      dispatchParams);

    if (dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

// Load r11 with the constant 'n' and branch to SharedStub().
//
// XXX Yes, it's ugly that we're relying on gcc's name-mangling here;
// however, it's quick, dirty, and'll break when the ABI changes on
// us, which is what we want ;-).

// gcc-3 version
//
// As G++3 ABI contains the length of the functionname in the mangled
// name, it is difficult to get a generic assembler mechanism like
// in the G++ 2.95 case.
// Create names would be like:
// _ZN14nsXPTCStubBase5Stub1Ev
// _ZN14nsXPTCStubBase6Stub12Ev
// _ZN14nsXPTCStubBase7Stub123Ev
// _ZN14nsXPTCStubBase8Stub1234Ev
// etc.
// Use assembler directives to get the names right...

# define STUB_ENTRY(n)							\
__asm__ (								\
	".align	2 \n\t"							\
	".if	"#n" < 10 \n\t"						\
	".globl	_ZN14nsXPTCStubBase5Stub"#n"Ev \n\t"			\
	".type	_ZN14nsXPTCStubBase5Stub"#n"Ev,@function \n\n"		\
"_ZN14nsXPTCStubBase5Stub"#n"Ev: \n\t"					\
									\
	".elseif "#n" < 100 \n\t"					\
	".globl	_ZN14nsXPTCStubBase6Stub"#n"Ev \n\t"			\
	".type	_ZN14nsXPTCStubBase6Stub"#n"Ev,@function \n\n"		\
"_ZN14nsXPTCStubBase6Stub"#n"Ev: \n\t"					\
									\
	".elseif "#n" < 1000 \n\t"					\
	".globl	_ZN14nsXPTCStubBase7Stub"#n"Ev \n\t"			\
	".type	_ZN14nsXPTCStubBase7Stub"#n"Ev,@function \n\n"		\
"_ZN14nsXPTCStubBase7Stub"#n"Ev: \n\t"					\
									\
	".else \n\t"							\
	".err	\"stub number "#n" >= 1000 not yet supported\"\n"	\
	".endif \n\t"							\
									\
	"li	11,"#n" \n\t"						\
	"b	SharedStub@local \n"					\
);

#define SENTINEL_ENTRY(n)                            \
nsresult nsXPTCStubBase::Sentinel##n()               \
{                                                    \
  NS_ERROR("nsXPTCStubBase::Sentinel called"); \
  return NS_ERROR_NOT_IMPLEMENTED;                   \
}

#include "xptcstubsdef.inc"
