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
#ifndef __RPCClientServoce_h__
#define __RPCClientService_h__
#include "nsISupports.h"
#include "ITransport.h"
#include "IRPCall.h"
#include "IRPCChannel.h"

class RPCClientService {
 public:
    static RPCClientService * GetInstance(void);
    static void Initialize(char *serverName);
    ~RPCClientService();
    NS_IMETHOD GetRPCChannel(IRPCChannel **channel);
    NS_IMETHOD CreateRPCall(IRPCall **rcall);
    NS_IMETHOD CreateObjectProxy(OID oid, const nsIID &iid, nsISupports **result);
 protected:
    RPCClientService(void);
    NS_IMETHOD GetTransport(ITransport **transport);
    static RPCClientService * instance;
    ITransport  * transport;
    IRPCChannel * rpcChannel;
    static char * serverName;
};
#endif /*  __RPCClientService_h__ */
