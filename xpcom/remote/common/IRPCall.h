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
#ifndef __IRPCall_h__
#define __IRPCall_h__
#include "nsISupports.h"
#include "xptcall.h"
#include "rproxydefs.h"

// {59678952-A52B-11d3-837C-0004AC56C49E}
#define IRPCALL_IID\
{ 0x59678952, 0xa52b, 0x11d3, { 0x83, 0x7c, 0x0, 0x4, 0xac, 0x56, 0xc4, 0x9e } }

class IRPCall : public  nsISupports {
 public:
     NS_DEFINE_STATIC_IID_ACCESSOR(IRPCALL_IID)
     NS_IMETHOD Marshal(const nsIID &iid, OID oid, PRUint16 methodIndex,
			 const nsXPTMethodInfo* info,
			 nsXPTCMiniVariant* params) = 0;
     NS_IMETHOD Marshal() = 0;
     NS_IMETHOD Demarshal(void *  data, PRUint32  size) = 0;
     NS_IMETHOD GetRawData(void ** data, PRUint32 *size) = 0;
     //you should not free(params) 
     NS_IMETHOD GetXPTCData(OID *oid,PRUint32 *methodIndex, 
			    PRUint32 *paramCount, nsXPTCVariant** params, nsresult **result) = 0;  
     
};
#endif /* __IRPCall_h__ */
