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
#ifndef __RPCallImpl_h__
#define __RPCallImpl_h__
#include "IRPCall.h"
#include "IMarshalToolkit.h"

class ostream;
class istream;
class nsIInterfaceInfo;

class RPCallImpl : public IRPCall {
public:
    NS_DECL_ISUPPORTS
    NS_IMETHOD Marshal(const nsIID &iid, OID oid, PRUint16 methodIndex,
                       const nsXPTMethodInfo* info,
                       nsXPTCMiniVariant* params);
    NS_IMETHOD Marshal(void);
    NS_IMETHOD Demarshal(void *  data, PRUint32  size);
    NS_IMETHOD GetRawData(void ** data, PRUint32 *size);
    //you should not free(params) 
    NS_IMETHOD GetXPTCData(OID *oid,PRUint32 *methodIndex, 
                           PRUint32 *paramCount, nsXPTCVariant** params, nsresult **result);
    RPCallImpl(IMarshalToolkit * _marshalToolkit);
    virtual ~RPCallImpl();
protected:
    enum { unDefined, onServer, onClient } callSide; 
    enum SizeMode { GET_SIZE, GET_LENGTH } ; 
    nsIID iid;
    OID oid;
    PRUint16 methodIndex;
    nsXPTMethodInfo* info;
    nsXPTCVariant *params;
    PRUint32 paramCount;
    nsIInterfaceInfo * interfaceInfo;
    void *bundle;
    PRUint32 size;
    nsresult result;
    IMarshalToolkit * marshalToolkit;
    static PRBool GetArraySizeFromParam( nsIInterfaceInfo *interfaceInfo,
                                         const nsXPTMethodInfo* method,
                                         const nsXPTParamInfo& param,
                                         uint16 methodIndex,
                                         uint8 paramIndex,
                                         nsXPTCMiniVariant* nativeParams,
                                         SizeMode mode,
                                         PRUint32* result);
};  

#endif /*  __RPCallImpl_h__ */
