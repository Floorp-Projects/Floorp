/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#include "plstr.h"

#include "ipcConfig.h"
#include "ipcService.h"
#include "ipcm.h"

static PRBool PR_CALLBACK
ipcReleaseMessageObserver(nsHashKey *aKey, void *aData, void* aClosure)
{
    ipcIMessageObserver *obs = (ipcIMessageObserver *) aData;
    NS_RELEASE(obs);
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// ipcService
//-----------------------------------------------------------------------------

ipcService::ipcService()
{
    NS_INIT_ISUPPORTS();
}

ipcService::~ipcService()
{
    if (mTransport) {
        mTransport->Shutdown();
        NS_RELEASE(mTransport);
    }

    mObserverDB.Reset(ipcReleaseMessageObserver, nsnull);
}

nsresult
ipcService::Init()
{
    nsresult rv;

    mTransport = new ipcTransport();
    if (!mTransport)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(mTransport);

    // XXX use directory service to locate socket
    rv = mTransport->Init(NS_LITERAL_CSTRING(IPC_DEFAULT_SOCKET_PATH), this);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

//-----------------------------------------------------------------------------
// interface impl
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(ipcService, ipcIService)

NS_IMETHODIMP
ipcService::GetClientID(PRUint32 *clientID)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ipcService::AddClientAlias(const nsACString &alias)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ipcService::RemoveClientAlias(const nsACString &alias)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ipcService::QueryClientByName(const nsACString &name,
                              ipcIClientObserver *observer,
                              PRUint32 *token)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ipcService::QueryClientByID(PRUint32 clientID,
                            ipcIClientObserver *observer,
                            PRUint32 *token)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ipcService::SetClientObserver(PRUint32 clientID,
                              ipcIClientObserver *observer)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ipcService::SetMessageObserver(const nsID &target, ipcIMessageObserver *observer)
{
    nsIDKey key(target);

    ipcIMessageObserver *cobs = (ipcIMessageObserver *) mObserverDB.Get(&key);
    if (cobs) {
        NS_RELEASE(cobs);
        if (!observer) {
            mObserverDB.Remove(&key);
            return NS_OK;
        }
    }
    if (observer) {
        NS_ADDREF(observer);
        mObserverDB.Put(&key, observer);
    }
    return NS_OK;
}

NS_IMETHODIMP
ipcService::SendMessage(PRUint32 clientID,
                        const nsID &target,
                        const char *data,
                        PRUint32 dataLen)
{
    NS_ENSURE_TRUE(mTransport, NS_ERROR_NOT_INITIALIZED);

    if (target.Equals(IPCM_TARGET)) {
        NS_ERROR("do not try to talk to the IPCM target directly");
        return NS_ERROR_INVALID_ARG;
    }

    ipcMessage *msg;
    if (clientID)
        msg = new ipcmMessageForward(clientID, target, data, dataLen);
    else
        msg = new ipcMessage(target, data, dataLen);

    if (!msg)
        return NS_ERROR_OUT_OF_MEMORY;

    mTransport->SendMsg(msg);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// ipcTransportObserver impl
//-----------------------------------------------------------------------------

void
ipcService::OnMsgAvailable(const ipcMessage *msg)
{
    nsIDKey key(msg->Target());

    ipcIMessageObserver *observer = (ipcIMessageObserver *) mObserverDB.Get(&key);
    if (observer)
        observer->OnMessageAvailable(msg->Target(),
                                     msg->Data(),
                                     msg->DataLen());
}
