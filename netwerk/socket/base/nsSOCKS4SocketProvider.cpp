/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Justin Bradford <jab@atdot.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsSOCKS4SocketProvider.h"
#include "nsSOCKSIOLayer.h"

//////////////////////////////////////////////////////////////////////////

nsSOCKS4SocketProvider::nsSOCKS4SocketProvider()
{
}

nsresult
nsSOCKS4SocketProvider::Init()
{
    nsresult rv = NS_OK;
    return rv;
}

nsSOCKS4SocketProvider::~nsSOCKS4SocketProvider()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsSOCKS4SocketProvider, nsISocketProvider, nsISOCKS4SocketProvider)

NS_METHOD
nsSOCKS4SocketProvider::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    
    nsSOCKS4SocketProvider * inst;
    
    if (NULL == aResult) {
        rv = NS_ERROR_NULL_POINTER;
        return rv;
    }
    *aResult = NULL;
    if (NULL != aOuter) {
        rv = NS_ERROR_NO_AGGREGATION;
        return rv;
    }
    
    NS_NEWXPCOM(inst, nsSOCKS4SocketProvider);
    if (NULL == inst) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        return rv;
    }
    NS_ADDREF(inst);
    rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);
    
    return rv;
}

NS_IMETHODIMP
nsSOCKS4SocketProvider::NewSocket(const char *host, 
                                 PRInt32 port,
                                 const char *proxyHost,
                                 PRInt32 proxyPort,
                                 PRFileDesc **_result, 
                                 nsISupports **socksInfo)
{
    nsresult rv = nsSOCKSIOLayerNewSocket(host, 
                                          port,
                                          proxyHost,
                                          proxyPort,
                                          4,          // SOCKS 4
                                          _result, 
                                          socksInfo);
    
    return (NS_FAILED(rv)) ? NS_ERROR_SOCKET_CREATE_FAILED : NS_OK;
}

NS_IMETHODIMP
nsSOCKS4SocketProvider::AddToSocket(const char *host,
                                   PRInt32 port,
                                   const char *proxyHost,
                                   PRInt32 proxyPort,
                                   PRFileDesc *socket, 
                                   nsISupports **socksInfo)
{
    nsresult rv = nsSOCKSIOLayerAddToSocket(host, 
                                            port,
                                            proxyHost,
                                            proxyPort,
                                            4,        // SOCKS 4
                                            socket, 
                                            socksInfo);
    
    return (NS_FAILED(rv)) ? NS_ERROR_SOCKET_CREATE_FAILED : NS_OK;
}
