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

#include <stdio.h>
#include <string.h>
#include "ipcCommandModule.h"
#include "ipcModule.h"
#include "ipcClient.h"
#include "ipcMessage.h"
#include "ipcd.h"
#include "ipcm.h"

typedef const char * constCharPtr;

class ipcCommandModule : public ipcModule
{
public:
    void Shutdown()
    {
    }

    const nsID &ID()
    {
        return IPCM_TARGET;
    }

    void HandleMsg(ipcClient *client, const ipcMessage *msg)
    {
        // XXX replace w/ function table
        switch (IPCM_GetMsgType(msg)) {
        case IPCM_MSG_PING: 
            printf("### got ping\n");
            {
                IPC_SendMsg(client, new ipcmMessagePING());
            }
            break;
        case IPCM_MSG_CNAME:
            printf("### got cname\n");
            {
                const char *name = ((const ipcmMessageCNAME *) msg)->ClientName();
                if (name)
                    client->SetName(name);
            }
            break;
        case IPCM_MSG_CENUM:
            printf("### got cenum\n");
            {
                int len;
                ipcClient *clients = IPC_GetClients(&len);
                if (clients) {
                    constCharPtr *clist = new constCharPtr[len-1];
                    if (clist) {
                        for (int i = 0; i < len; ++i) {
                            if (clients + i != client)
                                clist[i] = clients[i].Name();
                        }
                        IPC_SendMsg(client, new ipcmMessageCLIST(clist, len-1));
                        delete[] clist;
                    }
                }
            }
            break;
        case IPCM_MSG_FWD:
            printf("### got fwd\n");
            {
                const ipcmMessageFWD *fMsg = (const ipcmMessageFWD *) msg;
                ipcClient *client = IPC_GetClientByName(fMsg->DestClient());
                ipcMessage *newMsg = new ipcMessage();
                if (fMsg->DestMessage(newMsg) == PR_SUCCESS)
                    IPC_SendMsg(client, newMsg);
            }
            break;
        default:
            printf("### got unknown message\n");
        }
    }
};

ipcModule *IPC_GetCommandModule()
{
    static ipcCommandModule module;
    return &module;
}
