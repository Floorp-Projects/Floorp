/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef __MarshalToolkitImpl_h__
#define __MarshalToolkitImpl_h__
#include "nsISupports.h"
#include "xptcall.h"
#include "IMarshalToolkit.h"

class MarshalToolkitImpl : public IMarshalToolkit {
    NS_DECL_ISUPPORTS
    /* nsXPTType::T_I8, nsXPTType::T_U8, nsXPTType::T_I16,  nsXPTType::T_U16
     * nsXPTType::T_I32, nsXPTType::T_U32, nsXPTType::T_I64, nsXPTType::T_U64,
     * nsXPTType::T_FLOAT, nsXPTType::T_DOUBLE, nsXPTType::T_BOOL, nsXPTType::T_CHAR, nsXPTType::T_WCHAR,
     */
    NS_IMETHOD ReadSimple(istream *in, nsXPTCMiniVariant *value,nsXPTType * type, PRBool  isOut, nsIAllocator * allocator = NULL);
    NS_IMETHOD WriteSimple(ostream *out, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut);

    /* nsXPTType::T_IID
     */
    NS_IMETHOD ReadIID(istream *in, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut, nsIAllocator * allocator = NULL);
    NS_IMETHOD WriteIID(ostream *out, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut);

    /* case nsXPTType::T_CHAR_STR:
     * case nsXPTType::T_WCHAR_STR:
     * case nsXPTType::T_PSTRING_SIZE_IS:
     * case nsXPTType::T_PWSTRING_SIZE_IS:
     */
    NS_IMETHOD ReadString(istream *in, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut, nsIAllocator * allocator = NULL);
    NS_IMETHOD WriteString(ostream *out, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut, PRInt32 size = -1);

    /* nsXPTType::T_INTERFACE, nsXPTType::T_INTERFACE_IS
     */
    NS_IMETHOD ReadInterface(istream *in, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut, nsIID * iid = NULL,
                             nsIAllocator * allocator = NULL);
    NS_IMETHOD WriteInterface(ostream *out, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut, nsIID * iid = NULL);
    
    /* nsXPTType::T_ARRAY
     */
    NS_IMETHOD ReadArray(istream *in, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut, nsXPTType * datumType, nsIAllocator * allocator = NULL);
    NS_IMETHOD WriteArray(ostream *out, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut, nsXPTType * datumType, PRUint32 size);

    MarshalToolkitImpl(void);
    virtual ~MarshalToolkitImpl();
 private:
    PRInt16 GetSimpleSize(nsXPTType *type);
};
#endif  /* __MarshalToolkitImpl_h__ */

















