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

#include "ipcIService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"
#include "nsString.h"

static const nsID TestTargetID =
{ /* e628fc6e-a6a7-48c7-adba-f241d1128fb8 */
    0xe628fc6e,
    0xa6a7,
    0x48c7,
    {0xad, 0xba, 0xf2, 0x41, 0xd1, 0x12, 0x8f, 0xb8}
};

#define RETURN_IF_FAILED(rv, step) \
    PR_BEGIN_MACRO \
    if (NS_FAILED(rv)) { \
        printf("*** %s failed: rv=%x\n", step, rv); \
        return rv;\
    } \
    PR_END_MACRO

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static nsIEventQueue* gEventQ = nsnull;
static PRBool gKeepRunning = PR_TRUE;
//static PRInt32 gMsgCount = 0;
static ipcIService *gIpcServ = nsnull;

//-----------------------------------------------------------------------------

class myIpcMessageObserver : public ipcIMessageObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_IPCIMESSAGEOBSERVER

    myIpcMessageObserver() { NS_INIT_ISUPPORTS(); }
};

NS_IMPL_ISUPPORTS1(myIpcMessageObserver, ipcIMessageObserver)

NS_IMETHODIMP
myIpcMessageObserver::OnMessageAvailable(const nsID &target, const char *data, PRUint32 dataLen)
{
    printf("*** got message: [%s]\n", data);

//    if (--gMsgCount == 0)
//        gKeepRunning = PR_FALSE;

    return NS_OK;
}

//-----------------------------------------------------------------------------

class myIpcClientObserver : public ipcIClientObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_IPCICLIENTOBSERVER

    myIpcClientObserver() { NS_INIT_ISUPPORTS(); }
};

NS_IMPL_ISUPPORTS1(myIpcClientObserver, ipcIClientObserver)

NS_IMETHODIMP
myIpcClientObserver::OnClientStatus(PRUint32 aReqToken, PRUint32 aStatus, ipcIClientInfo *aClientInfo)
{
    printf("*** got client status [token=%u status=%u info=%p]\n",
            aReqToken, aStatus, (void *) aClientInfo);

    if (aClientInfo) {
        PRUint32 cID;
        if (NS_SUCCEEDED(aClientInfo->GetID(&cID))) {
            nsCAutoString cName;
            if (NS_SUCCEEDED(aClientInfo->GetName(cName))) {
                printf("***   name:%s --> ID:%u\n", cName.get(), cID);

                const char hello[] = "hello friend!";
                gIpcServ->SendMessage(cID, TestTargetID, hello, sizeof(hello));
            }
        }
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------

void SendMsg(ipcIService *ipc, const nsID &target, const char *data, PRUint32 dataLen)
{
    printf("*** sending message: [dataLen=%u]\n", dataLen);

    ipc->SendMessage(0, target, data, dataLen);
//    gMsgCount++;
}

int main(int argc, char **argv)
{
    nsresult rv;

    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
        NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
        if (registrar)
            registrar->AutoRegister(nsnull);

        // Create the Event Queue for this thread...
        nsCOMPtr<nsIEventQueueService> eqs =
                 do_GetService(kEventQueueServiceCID, &rv);
        RETURN_IF_FAILED(rv, "do_GetService(EventQueueService)");

        rv = eqs->CreateMonitoredThreadEventQueue();
        RETURN_IF_FAILED(rv, "CreateMonitoredThreadEventQueue");

        rv = eqs->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);
        RETURN_IF_FAILED(rv, "GetThreadEventQueue");

        if (argc > 1) {
            printf("*** using client name [%s]\n", argv[1]);
            nsCOMPtr<nsIPrefService> prefserv(do_GetService(NS_PREFSERVICE_CONTRACTID));
            if (prefserv) {
                nsCOMPtr<nsIPrefBranch> prefbranch;
                prefserv->GetBranch(nsnull, getter_AddRefs(prefbranch));
                if (prefbranch)
                    prefbranch->SetCharPref("ipc.client-name", argv[1]);
            }
        }

        nsCOMPtr<ipcIService> ipcServ(do_GetService("@mozilla.org/ipc/service;1", &rv));
        RETURN_IF_FAILED(rv, "do_GetService(ipcServ)");
        NS_ADDREF(gIpcServ = ipcServ);

        ipcServ->SetMessageObserver(TestTargetID, new myIpcMessageObserver());

        const char *data =
                "01 this is a really long message.\n"
                "02 this is a really long message.\n"
                "03 this is a really long message.\n"
                "04 this is a really long message.\n"
                "05 this is a really long message.\n"
                "06 this is a really long message.\n"
                "07 this is a really long message.\n"
                "08 this is a really long message.\n"
                "09 this is a really long message.\n"
                "10 this is a really long message.\n"
                "11 this is a really long message.\n"
                "12 this is a really long message.\n"
                "13 this is a really long message.\n"
                "14 this is a really long message.\n"
                "15 this is a really long message.\n"
                "16 this is a really long message.\n"
                "17 this is a really long message.\n"
                "18 this is a really long message.\n"
                "19 this is a really long message.\n"
                "20 this is a really long message.\n"
                "21 this is a really long message.\n"
                "22 this is a really long message.\n"
                "23 this is a really long message.\n"
                "24 this is a really long message.\n"
                "25 this is a really long message.\n"
                "26 this is a really long message.\n"
                "27 this is a really long message.\n"
                "28 this is a really long message.\n"
                "29 this is a really long message.\n"
                "30 this is a really long message.\n"
                "31 this is a really long message.\n"
                "32 this is a really long message.\n"
                "33 this is a really long message.\n"
                "34 this is a really long message.\n"
                "35 this is a really long message.\n"
                "36 this is a really long message.\n"
                "37 this is a really long message.\n"
                "38 this is a really long message.\n"
                "39 this is a really long message.\n"
                "40 this is a really long message.\n"
                "41 this is a really long message.\n"
                "42 this is a really long message.\n"
                "43 this is a really long message.\n"
                "44 this is a really long message.\n"
                "45 this is a really long message.\n"
                "46 this is a really long message.\n"
                "47 this is a really long message.\n"
                "48 this is a really long message.\n"
                "49 this is a really long message.\n"
                "50 this is a really long message.\n"
                "51 this is a really long message.\n"
                "52 this is a really long message.\n"
                "53 this is a really long message.\n"
                "54 this is a really long message.\n"
                "55 this is a really long message.\n"
                "56 this is a really long message.\n"
                "57 this is a really long message.\n"
                "58 this is a really long message.\n"
                "59 this is a really long message.\n"
                "60 this is a really long message.\n";
        SendMsg(ipcServ, TestTargetID, data, strlen(data)+1);

        PRUint32 reqToken;
        nsCOMPtr<ipcIClientObserver> obs(new myIpcClientObserver());
        ipcServ->QueryClientByName(NS_LITERAL_CSTRING("foopy"), obs, &reqToken);

        while (gKeepRunning)
            gEventQ->ProcessPendingEvents();

        NS_RELEASE(gIpcServ);

        printf("*** processing remaining events\n");

        // process any remaining events
        PLEvent *ev;
        while (NS_SUCCEEDED(gEventQ->GetEvent(&ev)) && ev)
            gEventQ->HandleEvent(ev);

        printf("*** done\n");
    } // this scopes the nsCOMPtrs

    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM(nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");

    return 0;
}
