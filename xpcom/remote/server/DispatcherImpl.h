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
#ifndef __DispatcherImpl_h__
#define __DispatcherImpl_h__
#include "nsISupports.h"
#include "IRPCall.h"
#include "IDispatcher.h"
#include "rproxydefs.h"
#include "nsVector.h"

class  DispatcherImpl : public IDispatcher {
public :
    NS_DECL_ISUPPORTS
	NS_IMETHOD Dispatch(IRPCall *call);
    NS_IMETHOD OIDToPointer(nsISupports **p,OID oid);
    NS_IMETHOD PointerToOID(OID *oid, nsISupports *p);
    NS_IMETHOD RegisterWithOID(nsISupports *p, OID oid);
    NS_IMETHOD UnregisterWithOID(nsISupports *p, OID oid);
    NS_IMETHOD GetOID(OID *oid);
    DispatcherImpl();
	virtual ~DispatcherImpl();
private:
    nsVector* m_objects; // vector of registred objects
    OID m_currentOID;
};
#endif /* __DispatcherImpl_h__ */



