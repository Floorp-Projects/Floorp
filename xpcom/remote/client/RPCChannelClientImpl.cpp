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
#include "RPCChannelClientImpl.h"
#include "prmem.h"

NS_IMPL_ISUPPORTS(RPCChannelClientImpl, NS_GET_IID(IRPCChannel));


RPCChannelClientImpl::RPCChannelClientImpl(ITransport * _transport) {
    NS_INIT_REFCNT();
    transport = _transport;
}

RPCChannelClientImpl::~RPCChannelClientImpl() {
    NS_IF_RELEASE(transport);
}

NS_IMETHODIMP RPCChannelClientImpl::SendReceive(IRPCall * call) {
    //nb temporary implamentation
    if(!transport 
       || !call) {
        return NS_ERROR_NULL_POINTER;
    }
    void *rawData = NULL;
    PRUint32 size = 0;
    nsresult rv;
    if (NS_FAILED(rv = call->GetRawData(&rawData,&size))) {
        return rv;
    }
    transport->Write(rawData,size);
    PR_Free(rawData);
    rawData = NULL;
    while (!NS_SUCCEEDED(transport->Read(&rawData,&size))) { //nb ugly just for testing
        ;
    }
    rv = call->Demarshal(rawData,size);
    PR_Free(rawData);
    return rv;
}

