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
#include "nsIInterfaceInfoManager.h"
#include "RemoteObjectProxy.h"
#include "IRPCall.h"
#include "IRPCChannel.h"

#include "RPCClientService.h"


NS_IMPL_ISUPPORTS(RemoteObjectProxy,NS_GET_IID(RemoteObjectProxy));

RemoteObjectProxy::RemoteObjectProxy(OID _oid, REFNSIID _iid ) {
    NS_INIT_REFCNT();
    oid = _oid;
    iid = _iid;
    interfaceInfo = NULL;
}
 
RemoteObjectProxy::~RemoteObjectProxy() {
    NS_IF_RELEASE(interfaceInfo); 
}

NS_IMETHODIMP RemoteObjectProxy::GetInterfaceInfo(nsIInterfaceInfo** info) {
    printf("--RemoteObjectProxy::GetInterfaceInfo\n");
    if(!info) {
	return NS_ERROR_FAILURE;
    }
    if (!interfaceInfo) {
	nsIInterfaceInfoManager* iimgr;
	if(iimgr = XPTI_GetInterfaceInfoManager()) {
	    if (NS_FAILED(iimgr->GetInfoForIID(&iid, &interfaceInfo))) {
		printf("--RemoteObjectProxy::GetInterfaceInfo NS_ERROR_FAILURE 1 \n");
		return NS_ERROR_FAILURE;
	    }
	    NS_RELEASE(iimgr);
	} else {
	    printf("--RemoteObjectProxy::GetInterfaceInfo NS_ERROR_FAILURE 2 \n");
	    return NS_ERROR_FAILURE;
	}
    }
    NS_ADDREF(interfaceInfo);
    *info =  interfaceInfo;
    return NS_OK;
}

NS_IMETHODIMP RemoteObjectProxy::CallMethod(PRUint16 methodIndex,
					   const nsXPTMethodInfo* info,
					   nsXPTCMiniVariant* params) {
    printf("--RemoteObjectProxy::CallMethod methodIndex %d\n", methodIndex);
    RPCClientService * rpcService = RPCClientService::GetInstance();
    IRPCall * call = NULL;
    IRPCChannel *rpcChannel = NULL;
    nsresult rv;
    if (NS_FAILED(rv = rpcService->GetRPCChannel(&rpcChannel))) {
        printf("--RemoteObjectProxy::CallMethod: failed to get channel\n");
        return rv;
    }
    if (NS_FAILED(rv = rpcService->CreateRPCall(&call))) {
        printf("--RemoteObjectProxy::CallMethod: failed to create call\n"); 
        NS_IF_RELEASE(rpcChannel);
        return rv;
    }
    if (NS_FAILED(rv = call->Marshal(iid,oid,methodIndex,info,params))) {
        printf("--RemoteObjectProxy::CallMethod: failed marshal\n"); 
        NS_IF_RELEASE(call);
        NS_IF_RELEASE(rpcChannel);
        return rv;
    }
    rv = rpcChannel->SendReceive(call);
    NS_IF_RELEASE(call);
    NS_IF_RELEASE(rpcChannel);
    return rv;
}








