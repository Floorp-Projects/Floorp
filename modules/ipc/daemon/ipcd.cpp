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

#include "ipcConfig.h"
#include "ipcLog.h"
#include "ipcMessage.h"
#include "ipcClient.h"
#include "ipcModuleReg.h"
#include "ipcModule.h"
#include "ipcdPrivate.h"
#include "ipcd.h"

//-----------------------------------------------------------------------------
// IPC API
//-----------------------------------------------------------------------------

PRStatus
IPC_DispatchMsg(ipcClient *client, const ipcMessage *msg)
{
    // lookup handler for this message's topic and forward message to it.
    ipcModuleMethods *methods = IPC_GetModuleByTarget(msg->Target());
    if (methods) {
        methods->handleMsg(client, msg);
        return PR_SUCCESS;
    }
    LOG(("no registered module; ignoring message\n"));
    return PR_FAILURE;
}

PRStatus
IPC_SendMsg(ipcClient *client, const ipcMessage *msg)
{
    LOG(("IPC_SendMsg [clientID=%u]\n", client->ID()));

    if (client == NULL) {
        //
        // broadcast
        //
        for (int i=0; i<ipcClientCount; ++i) {
            if (client->HasTarget(msg->Target()))
                IPC_PlatformSendMsg(&ipcClients[i], msg);
        }
        return PR_SUCCESS;
    }
    if (!client->HasTarget(msg->Target())) {
        LOG(("  no registered message handler\n"));
        return PR_FAILURE;
    }
    return IPC_PlatformSendMsg(client, msg);
}

ipcClient *
IPC_GetClientByID(PRUint32 clientID)
{
    // linear search OK since number of clients should be small
    for (int i = 0; i < ipcClientCount; ++i) {
        if (ipcClients[i].ID() == clientID)
            return &ipcClients[i];
    }
    return NULL;
}

ipcClient *
IPC_GetClientByName(const char *name)
{
    // linear search OK since number of clients should be small
    for (int i = 0; i < ipcClientCount; ++i) {
        if (ipcClients[i].HasName(name))
            return &ipcClients[i];
    }
    return NULL;
}

void
IPC_EnumClients(ipcClientEnumFunc func, void *closure)
{
    for (int i = 0; i < ipcClientCount; ++i)
        func(closure, &ipcClients[i], ipcClients[i].ID());
}

PRUint32
IPC_GetClientID(ipcClient *client)
{
    return client->ID();
}

const char *
IPC_GetPrimaryClientName(ipcClient *client)
{
    return client->PrimaryName();
}

PRBool
IPC_ClientHasName(ipcClient *client, const char *name)
{
    return client->HasName(name);
}

PRBool
IPC_ClientHasTarget(ipcClient *client, const nsID &target)
{
    return client->HasTarget(target);
}

void
IPC_EnumClientNames(ipcClient *client, ipcClientNameEnumFunc func, void *closure)
{
    const ipcStringNode *node = client->Names();
    while (node) {
        if (func(closure, client, node->Value()) == PR_FALSE)
            break;
        node = node->mNext;
    }
}

void
IPC_EnumClientTargets(ipcClient *client, ipcClientTargetEnumFunc func, void *closure)
{
    const ipcIDNode *node = client->Targets();
    while (node) {
        if (func(closure, client, node->Value()) == PR_FALSE)
            break;
        node = node->mNext;
    }
}

ipcClient *
IPC_GetClients(PRUint32 *count)
{
    *count = (PRUint32) ipcClientCount;
    return ipcClients;
}
