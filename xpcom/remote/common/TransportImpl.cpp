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
#include "TransportImpl.h"

NS_IMPL_ISUPPORTS(TransportImpl, NS_GET_IID(ITransport));

NS_IMETHODIMP TransportImpl::Read(void** data, PRUint32 * size) {
    if(!bridge) {
        return NS_ERROR_FAILURE;
    }
    *size = bridge->Read(data);
    if (*size > 0) {
        printf("--TransportImpl::Read size %d\n",*size);
    }
    return (*size > 0 ) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP TransportImpl::Write(void* data, PRUint32 size) {
    if(!bridge) {
        return NS_ERROR_FAILURE;
    }
    if (!bridge->Write(size,data)) {
        return NS_ERROR_FAILURE;
    }
    printf("--TransportImpl::Write size %d\n",size);
    return NS_OK;
}

TransportImpl::TransportImpl(const char* serverName) {
    NS_INIT_REFCNT();
    bridge = new IPCBridge(serverName);
}

TransportImpl::~TransportImpl() {
    if (bridge) {
        delete bridge;
    }
}



