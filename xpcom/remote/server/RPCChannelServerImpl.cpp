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
#include "nsISupports.h"
#include "RPCChannelServerImpl.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "RPCServerService.h"
#include "Queue.h"
#include "IDispatcher.h"


class RPCChannelRunner : public nsIRunnable {
public:
    NS_DECL_ISUPPORTS
    NS_IMETHOD Run() {
        while(1) { //nb 
            rpcChannel->SendReceive(NULL);
            PR_Sleep(100);
        }
        return NS_OK;
    }
    RPCChannelRunner(IRPCChannel *_rpcChannel) {
        NS_INIT_REFCNT();
        rpcChannel = _rpcChannel;
        NS_ADDREF(rpcChannel);
    }
    virtual ~RPCChannelRunner() {
        NS_IF_RELEASE(rpcChannel);
    }
private:
    IRPCChannel * rpcChannel;
};

NS_IMPL_ISUPPORTS(RPCChannelRunner, NS_GET_IID(nsIRunnable));
NS_IMPL_ISUPPORTS(RPCChannelServerImpl, NS_GET_IID(IRPCChannel));




RPCChannelServerImpl::RPCChannelServerImpl(ITransport * _transport) {
    NS_INIT_REFCNT();
    transport = _transport;
    queue = new Queue();
    rawData = NULL;
    dispatcher = NULL;
    service = NULL;

    NS_NewThread(&thread,new RPCChannelRunner(this));
}

RPCChannelServerImpl::~RPCChannelServerImpl() {
    NS_IF_RELEASE(dispatcher);
    NS_IF_RELEASE(thread);
    NS_IF_RELEASE(transport);
}

NS_IMETHODIMP RPCChannelServerImpl::SendReceive(IRPCall * call) {
    if (call) {
        queue->Put(call);
    } else {
        if (!rawData) {
            IRPCall * call = (IRPCall*) queue->Get();
            if (call) {
                call->GetRawData(&rawData,&size);
                NS_RELEASE(call);
            }
        }
        if (rawData) {
            if (NS_SUCCEEDED(transport->Write(rawData,size))) {
                PR_Free(rawData);
                rawData = NULL;
            }
        }
        char *data;
        PRUint32 s;
        if (NS_SUCCEEDED(transport->Read((void**)&data,&s))) {
            IRPCall *call = NULL;
            if (!service) {
                service = RPCServerService::GetInstance();
            }
            service->CreateRPCall(&call);
            call->Demarshal(data,s); //nb error handling
            PR_Free(data);
            if (!dispatcher) {
                service->GetDispatcher(&dispatcher);
            }
            dispatcher->Dispatch(call);
        }
        
    }
    return NS_OK;
}









