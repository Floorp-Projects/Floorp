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

#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIFile.h"
#include "nsIProcess.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsNetCID.h"
#include "netCore.h"
#include "prerror.h"
#include "plstr.h"

#include "ipcConfig.h"
#include "ipcLog.h"
#include "ipcMessageUtils.h"
#include "ipcTransport.h"
#include "ipcm.h"

//-----------------------------------------------------------------------------
// ipcTransport
//-----------------------------------------------------------------------------

nsresult
ipcTransport::Init(ipcTransportObserver *obs)
{
    LOG(("ipcTransport::Init\n"));

    mObserver = obs;

#ifdef XP_UNIX
    nsresult rv = InitUnix();
    if (NS_FAILED(rv)) return rv;
#endif

    // XXX service should be the observer
    nsCOMPtr<nsIObserverService> observ(do_GetService("@mozilla.org/observer-service;1"));
    if (observ) {
        observ->AddObserver(this, "xpcom-shutdown", PR_FALSE);
        observ->AddObserver(this, "profile-change-net-teardown", PR_FALSE);
    }

    return Connect();
}

nsresult
ipcTransport::SendMsg(ipcMessage *msg)
{
    NS_ENSURE_ARG_POINTER(msg);

    LOG(("ipcTransport::SendMsg [dataLen=%u]\n", msg->DataLen()));

    if (!mHaveConnection) {
        LOG(("  delaying message until connected\n"));
        mDelayedQ.Append(msg);
        return NS_OK;
    }

    return SendMsg_Internal(msg);
}

void
ipcTransport::OnMessageAvailable(const ipcMessage *rawMsg)
{
    LOG(("ipcTransport::OnMsgAvailable [dataLen=%u]\n", rawMsg->DataLen()));

    if (!mHaveConnection) {
        if (rawMsg->Target().Equals(IPCM_TARGET)) {
            if (IPCM_GetMsgType(rawMsg) == IPCM_MSG_TYPE_CLIENT_ID) {
                LOG(("  connection established!\n"));
                mHaveConnection = PR_TRUE;

                // remember our client ID
                ipcMessageCast<ipcmMessageClientID> msg(rawMsg);
                if (mObserver)
                    mObserver->OnConnectionEstablished(msg->ClientID());

                // move messages off the delayed message queue
                while (!mDelayedQ.IsEmpty()) {
                    ipcMessage *msg = mDelayedQ.First();
                    mDelayedQ.RemoveFirst();
                    SendMsg_Internal(msg);
                }
                return;
            }
        }
        LOG(("  received unexpected first message!\n"));
    }
    else if (mObserver)
        mObserver->OnMessageAvailable(rawMsg);
}

nsresult
ipcTransport::SpawnDaemon()
{
    if (mSpawnedDaemon)
        return NS_OK;

    LOG(("ipcTransport::SpawnDaemon\n"));

    nsresult rv;
    nsCOMPtr<nsIFile> file;

    rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR, getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;

    rv = file->AppendNative(NS_LITERAL_CSTRING(IPC_DAEMON_APP_NAME));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIProcess> proc(do_CreateInstance(NS_PROCESS_CONTRACTID,&rv));
    if (NS_FAILED(rv)) return rv;

    rv = proc->Init(file);
    if (NS_FAILED(rv)) return rv;

    PRUint32 pid;
#ifdef XP_UNIX
    const char *args[] = { mSocketPath.get() };
    return proc->Run(PR_FALSE, args, 1, &pid);
#else
    return proc->Run(PR_FALSE, NULL, 0, &pid);
#endif
}

nsresult
ipcTransport::OnConnectFailure()
{
    if (mTimer)
        return NS_OK;

    nsresult rv;

    //
    // spawn daemon on connection failure
    //
    rv = SpawnDaemon();
    if (NS_FAILED(rv)) {
        LOG(("  failed to spawn daemon [rv=%x]\n", rv));
        return rv;
    }
    mSpawnedDaemon = PR_TRUE;

    //
    // re-initialize connection after timeout
    //
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
        LOG(("  failed to create timer [rv=%x]\n", rv));
        return rv;
    }

    // use a simple exponential growth algorithm 2^(n-1)
    PRUint32 ms = 500 * (1 << (mConnectionAttemptCount - 1));
    if (ms > 10000)
        ms = 10000;

    LOG(("  waiting %u milliseconds\n", ms));

    rv = mTimer->Init(this, ms, nsITimer::TYPE_ONE_SHOT);
    if (NS_FAILED(rv)) {
        LOG(("  failed to initialize timer [rv=%x]\n", rv));
        return rv;
    }

    return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS0(ipcTransport)

NS_IMETHODIMP
ipcTransport::Observe(nsISupports *subject, const char *topic, const PRUnichar *data)
{
    LOG(("ipcTransport::Observe [topic=%s]\n", topic));

    if (strcmp(topic, "timer-callback") == 0) {
        // 
        // try reconnecting to the daemon
        //
        Shutdown();
        Connect();

        mTimer = nsnull;
    }
    else if (strcmp(topic, "xpcom-shutdown") == 0 ||
             strcmp(topic, "profile-change-net-teardown") == 0)
        Shutdown(); 

    return NS_OK;
}
