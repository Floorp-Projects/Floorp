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
#include "RPCServerService.h"
#include "RPCallImpl.h"
#include "TransportImpl.h"
#include "DispatcherImpl.h"
#include "RPCChannelServerImpl.h"
#include "MarshalToolkitImpl.h"

static IMarshalToolkit * marshalToolkit = new MarshalToolkitImpl();

RPCServerService * RPCServerService::instance = NULL;

RPCServerService * RPCServerService::GetInstance() {
    if (!instance) {
	instance = new RPCServerService();
    }
    return instance;
}

NS_IMETHODIMP RPCServerService::CreateRPCall(IRPCall **result) {
    if (!result) {
        return NS_ERROR_NULL_POINTER;
    }
    *result = new RPCallImpl(marshalToolkit);
    NS_ADDREF(*result);
    return NS_OK;
    
}

NS_IMETHODIMP RPCServerService::GetTransport(ITransport **result) {
    if (!result) {
        return NS_ERROR_NULL_POINTER;
    }
    if (!transport) {
        transport = new TransportImpl();
        if (!transport) {
            return NS_ERROR_FAILURE;
        }
    }
    NS_ADDREF(transport);
    *result = transport;
    return NS_OK;
}


NS_IMETHODIMP RPCServerService::GetDispatcher(IDispatcher **result) {
    if (!result) {
        return NS_ERROR_NULL_POINTER;
    }
    if (!dispatcher) {
        dispatcher = new DispatcherImpl();
        if (!dispatcher) {
            return NS_ERROR_FAILURE;
        }
    }
    NS_ADDREF(dispatcher);
    *result = dispatcher;
    return NS_OK;
}

NS_IMETHODIMP RPCServerService::GetRPCChannel(IRPCChannel **result) {
    nsresult rv = NS_OK;
    if (!result) {
        return NS_ERROR_NULL_POINTER;
    }
    if (!rpcChannel) {
        ITransport * _transport = NULL;
        if (NS_FAILED(rv = GetTransport(&_transport))) {
            return rv;
        }
        rpcChannel = new RPCChannelServerImpl(_transport);
        if (!rpcChannel) {
            return NS_ERROR_FAILURE;
        }
    }
    NS_ADDREF(rpcChannel);
    *result = rpcChannel;
    return NS_OK;
}

RPCServerService::RPCServerService() {
   transport = NULL;
   dispatcher = NULL;
   GetRPCChannel(&rpcChannel);
}

RPCServerService::~RPCServerService() {
    NS_IF_RELEASE(rpcChannel);
}
