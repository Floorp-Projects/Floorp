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

#ifndef IPCD_H__
#define IPCD_H__

#include "ipcModule.h"

#define IPC_EXPORT extern "C" NS_EXPORT
#define IPC_IMPORT extern "C" NS_IMPORT

#ifdef IPC_DAEMON
#define IPC_API IPC_EXPORT
#else
#define IPC_API IPC_IMPORT
#endif

class ipcClient;
class ipcMessage;

//-----------------------------------------------------------------------------
// IPC daemon API
//-----------------------------------------------------------------------------

//
// IPC_DispatchMsg
//
// params:
//   client - identifies the client from which this message originated.
//   msg    - the message received.  this function does not modify |msg|,
//            and ownership stays with the caller.
//
IPC_API PRStatus IPC_DispatchMsg(ipcClient *client, const ipcMessage *msg);

//
// IPC_SendMsg
//
// params:
//   client - identifies the client that should receive the message.
//            if null, then the message is broadcast to all clients.
//   msg    - the message to be sent.  this function subsumes
//            ownership of the message.  the caller must not attempt
//            to access |msg| after this function returns.
//
IPC_API PRStatus IPC_SendMsg(ipcClient *client, ipcMessage *msg);

//
// returns the client ID dynamically generated for the given client.
//
IPC_API PRUint32 IPC_GetClientID(ipcClient *client);

//
// returns the primary client name (NULL if the client did not specify a name).
// this is the name specified by the client in its "client hello" message.
//
IPC_API const char *IPC_GetPrimaryClientName(ipcClient *client);

//
// client lookup functions
//
IPC_API ipcClient *IPC_GetClientByID(PRUint32 id);
IPC_API ipcClient *IPC_GetClientByName(const char *name);

//
// functions for inspecting the names and targets defined for a particular
// client instance.
//
IPC_API PRBool IPC_ClientHasName(ipcClient *client, const char *name);
IPC_API PRBool IPC_ClientHasTarget(ipcClient *client, const nsID &target);

// return PR_FALSE to end enumeration
typedef PRBool (* ipcClientNameEnumFunc)(void *closure, ipcClient *client, const char *name);
typedef PRBool (* ipcClientTargetEnumFunc)(void *closure, ipcClient *client, const nsID &target);

IPC_API void IPC_EnumerateClientNames(ipcClient *client, ipcClientNameEnumFunc func, void *closure);
IPC_API void IPC_EnumerateClientTargets(ipcClient *client, ipcClientTargetEnumFunc func, void *closure);

//
// return array of all clients, length equal to |count|.
//
IPC_API ipcClient *IPC_GetClients(PRUintn *count);

//
// returns the ipcModule object registered under the given module ID.
//
IPC_API ipcModule *IPC_GetModuleByID(const nsID &moduleID);

#endif // !IPCD_H__
