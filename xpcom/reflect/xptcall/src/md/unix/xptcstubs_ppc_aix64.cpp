/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/LGPL 2.1/GPL 2.0
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM are
 *   Copyright (C) 2002, International Business Machines Corporation.
 *   All Rights Reserved.
 *
 * Contributor(s):
 *
 *  Alternatively, the contents of this file may be used under the terms of
 *  either of the GNU General Public License Version 2 or later (the "GPL"),
 *  or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 *  in which case the provisions of the GPL or the LGPL are applicable instead
 *  of those above. If you wish to allow use of your version of this file only
 *  under the terms of either the GPL or the LGPL, and not to allow others to
 *  use your version of this file under the terms of the MPL, indicate your
 *  decision by deleting the provisions above and replace them with the notice
 *  and other provisions required by the LGPL or the GPL. If you do not delete
 *  the provisions above, a recipient may use your version of this file under
 *  the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Implement shared vtbl methods. */

#include "xptcprivate.h"
#include "xptiprivate.h"

#if defined(AIX)

/*
        For PPC (AIX & MAC), the first 8 integral and the first 13 f.p. parameters 
        arrive in a separate chunk of data that has been loaded from the registers. 
        The args pointer has been set to the start of the parameters BEYOND the ones
        arriving in registers
*/
extern "C" nsresult
PrepareAndDispatch(nsXPTCStubBase* self, PRUint64 methodIndex, PRUint64* args, PRUint64 *gprData, double *fprData)
{

#define PARAM_BUFFER_COUNT     16
#define PARAM_GPR_COUNT         7  

    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = NULL;
    const nsXPTMethodInfo* info = NULL;
    PRUint8 paramCount;
    PRUint8 i;
    nsresult result = NS_ERROR_FAILURE;

    NS_ASSERTION(self,"no self");

    self->mEntry->GetMethodInfo(PRUint16(methodIndex), &info);
    NS_ASSERTION(info,"no method info");

    paramCount = info->GetParamCount();

    // setup variant array pointer
    if(paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;
    NS_ASSERTION(dispatchParams,"no place for params");

    PRUint64* ap = args;
    PRUint32 iCount = 0;
    PRUint32 fpCount = 0;
    for(i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &dispatchParams[i];

        if(param.IsOut() || !type.IsArithmetic())
        {
            if (iCount < PARAM_GPR_COUNT)
                dp->val.p = (void*) gprData[iCount++];
            else
                dp->val.p = (void*) *ap++;
            continue;
        }
        // else
        switch(type)
        {
        case nsXPTType::T_I8      :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.i8  = (PRInt8) gprData[iCount++];
                                     else
                                         dp->val.i8  = (PRInt8)  *ap++;
                                     break;
        case nsXPTType::T_I16     :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.i16  = (PRInt16) gprData[iCount++];
                                     else
                                         dp->val.i16  = (PRInt16)  *ap++;
                                     break;
        case nsXPTType::T_I32     :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.i32  = (PRInt32) gprData[iCount++];
                                     else
                                         dp->val.i32  = (PRInt32)  *ap++;
                                     break;
        case nsXPTType::T_I64     :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.i64  = (PRInt64) gprData[iCount++];
                                     else
                                         dp->val.i64  = (PRInt64) *ap++;
                                     break;
        case nsXPTType::T_U8      :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.u8  = (PRUint8) gprData[iCount++];
                                     else
                                         dp->val.u8  = (PRUint8)  *ap++;
                                     break;
        case nsXPTType::T_U16     :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.u16  = (PRUint16) gprData[iCount++];
                                     else
                                         dp->val.u16  = (PRUint16)  *ap++;
                                     break;
        case nsXPTType::T_U32     :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.u32  = (PRUint32) gprData[iCount++];
                                     else
                                         dp->val.u32  = (PRUint32)  *ap++;
                                     break;
        case nsXPTType::T_U64     :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.u64  = (PRUint64) gprData[iCount++];
                                     else
                                         dp->val.u64  = (PRUint64)  *ap++;
                                     break;
        case nsXPTType::T_FLOAT   :  if (fpCount < 13) {
                                         dp->val.f  = (float) fprData[fpCount++];
                                         if (iCount < PARAM_GPR_COUNT)
                                             ++iCount;
                                         else
                                             ++ap;
                                     }
                                     else
                                         dp->val.f   = *((float*)   ap++);
                                     break;
        case nsXPTType::T_DOUBLE  :  if (fpCount < 13) {
                                         dp->val.d  = (double) fprData[fpCount++];
                                         if (iCount < PARAM_GPR_COUNT)
                                             ++iCount;
                                         else
                                             ++ap;
                                         if (iCount < PARAM_GPR_COUNT)
                                             ++iCount;
                                         else
                                             ++ap;
                                     }
                                     else {
                                         dp->val.f   = *((double*)   ap);
                                         ap += 2;
                                     }
                                     break;
        case nsXPTType::T_BOOL    :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.b  = (bool) gprData[iCount++];
                                     else
                                         dp->val.b  = (bool)  *ap++;
                                     break;
        case nsXPTType::T_CHAR    :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.c  = (char) gprData[iCount++];
                                     else
                                         dp->val.c  = (char)  *ap++;
                                     break;
        case nsXPTType::T_WCHAR   :  if (iCount < PARAM_GPR_COUNT)
                                         dp->val.wc  = (wchar_t) gprData[iCount++];
                                     else
                                         dp->val.wc  = (wchar_t)  *ap++;
                                     break;
        default:
            NS_ERROR("bad type");
            break;
        }
    }

    result = self->mOuter->CallMethod((PRUint16)methodIndex,info,dispatchParams);

    if(dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
}

#define STUB_ENTRY(n)

#define SENTINEL_ENTRY(n) \
nsresult nsXPTCStubBase::Sentinel##n() \
{ \
    NS_ERROR("nsXPTCStubBase::Sentinel called"); \
    return NS_ERROR_NOT_IMPLEMENTED; \
}

#include "xptcstubsdef.inc"

#endif /* AIX */
