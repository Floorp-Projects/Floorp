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
#ifndef __IDispatcher_h__
#define __IDispatcher_h__
#include "nsISupports.h"
#include "IRPCall.h"
#include "rproxydefs.h"


// {59678955-A52B-11d3-837C-0004AC56C49E}
#define IDISPATCHER_IID \
{ 0x59678955, 0xa52b, 0x11d3, { 0x83, 0x7c, 0x0, 0x4, 0xac, 0x56, 0xc4, 0x9e } }

class  IDispatcher : public nsISupports {
public :
	NS_DEFINE_STATIC_IID_ACCESSOR(IDISPATCHER_IID)
	NS_IMETHOD Dispatch(IRPCall *call) = 0;
    NS_IMETHOD OIDToPointer(nsISupports **p,OID oid) = 0;
    NS_IMETHOD PointerToOID(OID *oid, nsISupports *p) = 0;
    NS_IMETHOD RegisterWithOID(nsISupports *p, OID oid) = 0;
    NS_IMETHOD UnregisterWithOID(nsISupports *p, OID oid) = 0;
    NS_IMETHOD GetOID(OID *oid) = 0;
};
#endif /* __IDispatcher_h__ */
