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
#include "ipcMessageUtils.h"
#include "ipcd.h"
#include "ipcm.h"

typedef const char * constCharPtr;

class ipcCommandModule : public ipcModule
{
public:
    typedef void (ipcCommandModule:: *MsgHandler)(ipcClient *, const ipcMessage *);

    //
    // message handlers
    //

    void handlePing(ipcClient *client, const ipcMessage *rawMsg)
    {
        printf("### got PING\n");

        IPC_SendMsg(client, new ipcmMessagePing());
    }

    void handleClientHello(ipcClient *client, const ipcMessage *rawMsg)
    {
        printf("### got CLIENT_HELLO\n");

        ipcMessageCast<ipcmMessageClientHello> msg(rawMsg);
        const char *name = msg->PrimaryName();
        if (name)
            client->SetName(name);

        IPC_SendMsg(client, new ipcmMessageClientID(client->ID()));
    }

    void handleForward(ipcClient *client, const ipcMessage *rawMsg)
    {
        printf("### got FORWARD\n");

        ipcMessageCast<ipcmMessageForward> msg(rawMsg);
        ipcClient *dest = IPC_GetClientByID(msg->DestClientID());

        ipcMessage *newMsg = new ipcMessage();
        newMsg->Init(msg->InnerTarget(),
                     msg->InnerData(),
                     msg->InnerDataLen());
        IPC_SendMsg(dest, newMsg);
    }

    //
    // ipcModule interface impl
    //

    void Shutdown()
    {
    }

    const nsID &ID()
    {
        return IPCM_TARGET;
    }

    void HandleMsg(ipcClient *client, const ipcMessage *rawMsg)
    {
        static MsgHandler handlers[] =
        {
            &ipcCommandModule::handlePing,
            NULL, // ERROR
            &ipcCommandModule::handleClientHello,
            NULL, // CLIENT_ID
            NULL, // CLIENT_INFO
            NULL, // CLIENT_ADD_NAME
            NULL, // CLIENT_DEL_NAME
            NULL, // CLIENT_ADD_TARGET
            NULL, // CLIENT_DEL_TARGET
            NULL, // QUERY_CLIENT_BY_NAME
            NULL, // QUERY_CLIENT_INFO
            NULL, // QUERY_FAILED
            &ipcCommandModule::handleForward,
        };

        int type = IPCM_GetMsgType(rawMsg);
        if (type < IPCM_MSG_TYPE_UNKNOWN) {
            if (handlers[type]) {
                MsgHandler handler = handlers[type];
                (this->*handler)(client, rawMsg);
            }
        }

#if 0
        case IPCM_MSG_FWD:
            printf("### got fwd\n");
            {
            }
            break;
        default:
            printf("### got unknown message\n");
        }
#endif
    }
};

ipcModule *IPC_GetCommandModule()
{
    static ipcCommandModule module;
    return &module;
}
