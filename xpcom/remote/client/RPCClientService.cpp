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
#include "RPCClientService.h"
#include "RemoteObjectProxy.h"
#include "RPCChannelClientImpl.h"
#include "RPCallImpl.h"
#include "TransportImpl.h"
#include "IMarshalToolkit.h"
#include "MarshalToolkitImpl.h"

static IMarshalToolkit * marshalToolkit = new MarshalToolkitImpl();

RPCClientService * RPCClientService::instance = NULL;
char * RPCClientService::serverName = NULL;

RPCClientService::RPCClientService(void) {
    transport = NULL;
    rpcChannel = NULL;
}

RPCClientService::~RPCClientService(void) {
    instance = NULL;
    NS_IF_RELEASE(rpcChannel);
}

RPCClientService *  RPCClientService::GetInstance(void) {
    if (!instance) {
        instance = new RPCClientService();
    }
    return instance;
}

void  RPCClientService::Initialize(char *_serverName) {
    serverName = _serverName;
}



NS_IMETHODIMP RPCClientService::CreateRPCall(IRPCall **result) {
    if (!result) {
        return NS_ERROR_NULL_POINTER;
    }
    *result = new RPCallImpl(marshalToolkit);
    NS_ADDREF(*result);
    return NS_OK;
    
}

NS_IMETHODIMP RPCClientService::CreateObjectProxy(OID oid, const nsIID &iid, nsISupports **result) {
    if (!result) {
        return NS_ERROR_NULL_POINTER;
    }
    *result = new RemoteObjectProxy(oid, iid);
    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP RPCClientService::GetTransport(ITransport **result) {
    if (!result) {
        return NS_ERROR_NULL_POINTER;
    }
    if (!transport) {
        transport = new TransportImpl(serverName);
        if (!transport) {
            return NS_ERROR_FAILURE;
        }
    }
    NS_ADDREF(transport);
    *result = transport;
    return NS_OK;
}



NS_IMETHODIMP RPCClientService::GetRPCChannel(IRPCChannel **result) {
    nsresult rv = NS_OK;
    if (!result) {
        return NS_ERROR_NULL_POINTER;
    }
    if (!rpcChannel) {
        ITransport * _transport = NULL;
        if (NS_FAILED(rv = GetTransport(&_transport))) {
            return rv;
        }
        rpcChannel = new RPCChannelClientImpl(_transport);
        if (!rpcChannel) {
            return NS_ERROR_FAILURE;
        }
        NS_ADDREF(rpcChannel); //nb !! it is for future
    }
    NS_ADDREF(rpcChannel);
    *result = rpcChannel;
    return NS_OK;
}
