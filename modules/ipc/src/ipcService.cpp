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

#ifdef XP_UNIX
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#endif

#include "plstr.h"

#include "nsIFile.h"
#include "nsIServiceManager.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "ipcConfig.h"
#include "ipcLog.h"
#include "ipcService.h"
#include "ipcMessageUtils.h"
#include "ipcm.h"

//-----------------------------------------------------------------------------
// helpers
//-----------------------------------------------------------------------------

static PRBool PR_CALLBACK
ipcReleaseMessageObserver(nsHashKey *aKey, void *aData, void* aClosure)
{
    ipcIMessageObserver *obs = (ipcIMessageObserver *) aData;
    NS_RELEASE(obs);
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// ipcClientInfo
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(ipcClientInfo, ipcIClientInfo)

NS_IMETHODIMP
ipcClientInfo::GetID(PRUint32 *aID)
{
    *aID = mID;
    return NS_OK;
}

NS_IMETHODIMP
ipcClientInfo::GetName(nsACString &aName)
{
    aName = mName;
    return NS_OK;
}

NS_IMETHODIMP
ipcClientInfo::GetAliases(nsISimpleEnumerator **aliases)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ipcClientInfo::GetTargets(nsISimpleEnumerator **targets)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// ipcService
//-----------------------------------------------------------------------------

PRUint32 ipcService::gLastReqToken = 0;

ipcService::ipcService()
    : mTransport(nsnull)
    , mClientID(0)
{
    NS_INIT_ISUPPORTS();

#ifdef DEBUG
    IPC_InitLog(">>>");
#endif
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

    // read preferences
    nsCAutoString appName;
    nsCOMPtr<nsIPrefService> prefserv(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefserv) {
        nsCOMPtr<nsIPrefBranch> prefbranch;
        prefserv->GetBranch(nsnull, getter_AddRefs(prefbranch));
        if (prefbranch) {
            nsXPIDLCString val;
            prefbranch->GetCharPref("ipc.client-name", getter_Copies(val));
            if (!val.IsEmpty())
                appName = val;
        }
    }
    if (appName.IsEmpty())
        appName = NS_LITERAL_CSTRING("test-app");

    // get socket path from directory service
    nsCAutoString socketPath;
    if (NS_FAILED(GetSocketPath(socketPath)))
        socketPath = NS_LITERAL_CSTRING(IPC_DEFAULT_SOCKET_PATH);

    rv = mTransport->Init(appName, socketPath, this);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

void
ipcService::HandleQueryResult(const ipcMessage *rawMsg, PRBool succeeded)
{
    ipcClientQuery *query = mQueryQ.First();
    if (!query) {
        NS_ERROR("no pending query; ignoring message.");
        return;
    }

    PRUint32 cStatus;
    ipcClientInfo *info;

    if (succeeded) {
        cStatus = ipcIClientObserver::CLIENT_UP;

        ipcMessageCast<ipcmMessageClientID> msg(rawMsg);
        info = new ipcClientInfo();
        if (info) {
            NS_ADDREF(info);
            info->Init(msg->ClientID(), query->mName);
        }
    }
    else {
        cStatus = ipcIClientObserver::CLIENT_DOWN;
        info = nsnull;
    }

    query->mObserver->OnClientStatus(query->mReqToken, cStatus, info);

    NS_IF_RELEASE(info);
    mQueryQ.DeleteFirst();
}

nsresult
ipcService::GetSocketPath(nsACString &socketPath)
{
    nsCOMPtr<nsIFile> file;
    NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(file));
    if (!file)
        return NS_ERROR_FAILURE;
#ifdef XP_UNIX
    // XXX may want to use getpwuid_r when available
    struct passwd *pw = getpwuid(geteuid());
    if (!pw)
        return NS_ERROR_UNEXPECTED;
    nsCAutoString leaf;
    leaf = NS_LITERAL_CSTRING(".mozilla-ipc-")
         + nsDependentCString(pw->pw_name);
    file->AppendNative(leaf);
#endif
    file->AppendNative(NS_LITERAL_CSTRING("ipcd"));
    file->GetNativePath(socketPath);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// interface impl
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(ipcService, ipcIService)

NS_IMETHODIMP
ipcService::GetClientID(PRUint32 *clientID)
{
    if (mClientID == 0)
        return NS_ERROR_NOT_AVAILABLE;

    *clientID = mClientID;
    return NS_OK;
}

NS_IMETHODIMP
ipcService::AddClientAlias(const nsACString &alias)
{
    NS_ENSURE_TRUE(mTransport, NS_ERROR_NOT_INITIALIZED);

    ipcMessage *msg = new ipcmMessageClientAddName(PromiseFlatCString(alias).get());
    if (!msg)
        return NS_ERROR_OUT_OF_MEMORY;

    return mTransport->SendMsg(msg);
}

NS_IMETHODIMP
ipcService::RemoveClientAlias(const nsACString &alias)
{
    NS_ENSURE_TRUE(mTransport, NS_ERROR_NOT_INITIALIZED);

    ipcMessage *msg = new ipcmMessageClientDelName(PromiseFlatCString(alias).get());
    if (!msg)
        return NS_ERROR_OUT_OF_MEMORY;

    return mTransport->SendMsg(msg);
}

NS_IMETHODIMP
ipcService::QueryClientByName(const nsACString &name,
                              ipcIClientObserver *observer,
                              PRUint32 *token)
{
    if (!mTransport)
        return NS_ERROR_NOT_AVAILABLE;

    ipcMessage *msg;

    msg = new ipcmMessageQueryClientByName(PromiseFlatCString(name).get());
    if (!msg)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    
    rv = mTransport->SendMsg(msg);
    if (NS_FAILED(rv)) return rv;

    //
    // now queue up the observer and generate a token.
    //
    ipcClientQuery *query = new ipcClientQuery();
    query->mName = name;
    query->mReqToken = *token = ++gLastReqToken;
    query->mObserver = observer;
    mQueryQ.Append(query);
    return NS_OK;
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
    NS_ENSURE_TRUE(mTransport, NS_ERROR_NOT_INITIALIZED);

    nsIDKey key(target);
    PRBool sendAdd = PR_TRUE;

    ipcIMessageObserver *cobs = (ipcIMessageObserver *) mObserverDB.Get(&key);
    if (cobs) {
        NS_RELEASE(cobs);
        if (!observer) {
            mObserverDB.Remove(&key);
            //
            // send CLIENT_DEL_TARGET
            //
            mTransport->SendMsg(new ipcmMessageClientDelTarget(target));
            return NS_OK;
        }
        sendAdd = PR_FALSE;
    }
    if (observer) {
        NS_ADDREF(observer);
        mObserverDB.Put(&key, observer);
        if (sendAdd) {
            //
            // send CLIENT_ADD_TARGET
            //
            mTransport->SendMsg(new ipcmMessageClientAddTarget(target));
        }
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

    return mTransport->SendMsg(msg);
}

//-----------------------------------------------------------------------------
// ipcTransportObserver impl
//-----------------------------------------------------------------------------

void
ipcService::OnConnectionEstablished(PRUint32 clientID)
{
    mClientID = clientID;
}

void
ipcService::OnConnectionLost()
{
    //
    // XXX error out any pending queries
    //
 
    mClientID = 0;
}

void
ipcService::OnMessageAvailable(const ipcMessage *msg)
{
    if (msg->Target().Equals(IPCM_TARGET)) {
        //
        // all IPCM messages stop here.
        //
        switch (IPCM_GetMsgType(msg)) {
        case IPCM_MSG_TYPE_CLIENT_ID:
            HandleQueryResult(msg, PR_TRUE);
            break;
        case IPCM_MSG_TYPE_ERROR:
            HandleQueryResult(msg, PR_FALSE);
            break;
        }
    }
    else {
        nsIDKey key(msg->Target());
        ipcIMessageObserver *observer = (ipcIMessageObserver *) mObserverDB.Get(&key);
        if (observer)
            observer->OnMessageAvailable(msg->Target(),
                                         msg->Data(),
                                         msg->DataLen());
    }
}
