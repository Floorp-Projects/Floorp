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

#include <windows.h>

#include "ipcConfig.h"
#include "ipcLog.h"
#include "ipcMessage.h"
#include "ipcClient.h"
#include "ipcModuleReg.h"
#include "ipcdPrivate.h"
#include "ipcd.h"
#include "ipcm.h"

//
// declared in ipcdPrivate.h
//
ipcClient *ipcClients;
int        ipcClientCount;

static ipcClient ipcClientArray[IPC_MAX_CLIENTS];

static HWND ipcHwnd;

//-----------------------------------------------------------------------------
// client array manipulation
//-----------------------------------------------------------------------------

static void
RemoveClient(ipcClient *client)
{
    LOG(("RemoveClient\n"));

    int clientIndex = client - ipcClientArray;

    client->Finalize();

    //
    // move last ipcClient object down into the spot occupied by this client.
    //
    int fromIndex = ipcClientCount - 1;
    int toIndex = clientIndex;
    if (toIndex != fromIndex)
        memcpy(&ipcClientArray[toIndex], &ipcClientArray[fromIndex], sizeof(ipcClient));

    memset(&ipcClientArray[fromIndex], 0, sizeof(ipcClient));

    --ipcClientCount;
    LOG(("  num clients = %u\n", ipcClientCount));
}

static void
PurgeStaleClients()
{
    LOG(("PurgeStaleClients [num-clients=%u]\n", ipcClientCount));
    //
    // walk the list of supposedly active clients, and verify the existance of
    // their respective message windows.
    //
    char wName[IPC_CLIENT_WINDOW_NAME_MAXLEN];
    for (int i=ipcClientCount-1; i>=0; --i) {
        ipcClient *client = &ipcClientArray[i];

        LOG(("  checking client at index %u [client-id=%u pid=%u]\n", 
            i, client->ID(), client->PID()));

        IPC_GetClientWindowName(client->PID(), wName);

        HWND hwnd = FindWindow(IPC_CLIENT_WINDOW_CLASS, wName);
        if (!hwnd) {
            LOG(("  client window not found; removing client!\n"));
            RemoveClient(client);
        }
    }
}

static ipcClient *
AddClient(HWND hwnd, PRUint32 pid)
{
    LOG(("AddClient\n"));

    //
    // before adding a new client, verify that all existing clients are
    // still up and running.  remove any stale clients.
    //
    if (ipcClientCount > 0)
        PurgeStaleClients();

    if (ipcClientCount == IPC_MAX_CLIENTS) {
        LOG(("  reached maximum client count!\n"));
        return NULL;
    }

    ipcClient *client = &ipcClientArray[ipcClientCount];
    client->Init();
    client->SetHwnd(hwnd);
    client->SetPID(pid);

    ++ipcClientCount;
    LOG(("  num clients = %u\n", ipcClientCount));
    return client;
}

static ipcClient *
GetClientByPID(PRUint32 pid)
{
    for (int i=0; i<ipcClientCount; ++i) {
        if (ipcClientArray[i].PID() == pid)
            return &ipcClientArray[i];
    }
    return NULL;
}

//-----------------------------------------------------------------------------
// message processing
//-----------------------------------------------------------------------------

void
ProcessMsg(HWND hwnd, PRUint32 pid, const ipcMessage *msg)
{
    LOG(("ProcessMsg [pid=%u len=%u]\n", pid, msg->MsgLen()));

    ipcClient *client = GetClientByPID(pid);
    if (client) {
        //
        // if this is an IPCM "client hello" message, then reset the client
        // instance object.
        //
        if (msg->Target().Equals(IPCM_TARGET) &&
            IPCM_GetMsgType(msg) == IPCM_MSG_TYPE_CLIENT_HELLO) {
            RemoveClient(client);
            client = AddClient(hwnd, pid);
        }
    }
    else
        client = AddClient(hwnd, pid);

    if (client == NULL)
        return;

    IPC_DispatchMsg(client, msg);
}

//-----------------------------------------------------------------------------

void
IPC_SendMessageNow(ipcClient *client, const ipcMessage *msg)
{
    LOG(("IPC_SendMessageNow [clientID=%u clientPID=%u]\n",
        client->ID(), client->PID()));

    COPYDATASTRUCT cd;
    cd.dwData = GetCurrentProcessId();
    cd.cbData = (DWORD) msg->MsgLen();
    cd.lpData = (PVOID) msg->MsgBuf();


    LOG(("calling SendMessage...\n"));
    SendMessageA(client->Hwnd(), WM_COPYDATA, 0, (LPARAM) &cd);
   // SendMessageA(hwnd, WM_COPYDATA, (WPARAM) ipcHwnd, (LPARAM) &cd);
    LOG(("  done.\n"));
}

//-----------------------------------------------------------------------------
// windows message loop
//-----------------------------------------------------------------------------

static LRESULT CALLBACK
WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LOG(("got message [msg=%x wparam=%x lparam=%x]\n", uMsg, wParam, lParam));

    if (uMsg == WM_COPYDATA) {
        COPYDATASTRUCT *cd = (COPYDATASTRUCT *) lParam;
        if (cd && cd->lpData) {
            ipcMessage msg;
            PRUint32 bytesRead;
            PRBool complete;
            PRStatus rv = msg.ReadFrom((const char *) cd->lpData, cd->cbData,
                                       &bytesRead, &complete);
            if (rv == PR_SUCCESS && complete) {
                //
                // grab client PID and hwnd.
                //
                DWORD pid = cd->dwData;
                HWND hwnd = (HWND) wParam;

                ProcessMsg(hwnd, pid, &msg);
            }
            else
                LOG(("ignoring malformed message\n"));
        }
        return TRUE;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

//-----------------------------------------------------------------------------
// daemon startup synchronization
//-----------------------------------------------------------------------------

static HANDLE ipcSyncEvent;

static PRBool
AcquireLock()
{
    ipcSyncEvent = CreateEvent(NULL, FALSE, FALSE,
                               IPC_SYNC_EVENT_NAME);
    if (!ipcSyncEvent) {
        LOG(("CreateEvent failed [%u]\n", GetLastError()));
        return PR_FALSE;
    }

    if (ipcSyncEvent) {
        // check to see if event already existed prior to this call.
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            LOG(("  lock already set; exiting...\n"));
            return PR_FALSE;
        }
    }
    
    LOG(("  acquired lock\n"));
    return PR_TRUE;
}

static void
ReleaseLock()
{
    if (ipcSyncEvent) {
        CloseHandle(ipcSyncEvent);
        ipcSyncEvent = NULL;
    }
}

//-----------------------------------------------------------------------------
// main
//-----------------------------------------------------------------------------

int
main(int argc, char **argv)
{
#ifdef DEBUG
    IPC_InitLog("###");
#endif
    LOG(("daemon started...\n"));

    if (!AcquireLock())
        return 0;

    ipcClients = ipcClientArray;
    ipcClientCount = 0;

    //
    // create message window up front...
    //
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = IPC_WINDOW_CLASS;

    RegisterClass(&wc);

    ipcHwnd = CreateWindow(IPC_WINDOW_CLASS, IPC_WINDOW_NAME,
                           0, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    if (!ipcHwnd)
        return -1;

    //
    // load modules
    //
    IPC_InitModuleReg(argv[0]);

    //
    // process messages
    //
    LOG(("entering message loop...\n"));

    MSG msg;
    while (GetMessage(&msg, ipcHwnd, 0, 0))
        DispatchMessage(&msg);

    ReleaseLock();

    LOG(("exiting\n"));
    return 0;
}
