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

#ifndef ipcModule_h__
#define ipcModule_h__

#include "nsID.h"

class ipcMessage;
class ipcClient;

//-----------------------------------------------------------------------------
// abstract module class
//-----------------------------------------------------------------------------

class ipcModule
{
public:
    //
    // called when this module will no longer be accessed.  if this module was
    // allocated on the heap, then it can be free'd.
    //
    virtual void Shutdown() = 0;

    //
    // called to determine the ID of this module.  the ID of a module
    // indicates the "message target" for which it will be registered
    // as a handler.
    //
    virtual const nsID &ID() = 0;

    //
    // called when a new message arrives for this module.
    //
    // params:
    //
    //   client - an opaque "handle" to an object representing the client that
    //            sent the message.  modules should not store the value of this
    //            beyond the duration fo this function call.  (e.g., the handle
    //            may be invalid after this function call returns.)  modules
    //            wishing to hold onto a reference to a "client" should store
    //            the client's ID (see IPC_GetClientID).
    //
    //   msg    - the message sent from the client.  the target of this message
    //            matches the ID of this module.
    //
    virtual void HandleMsg(ipcClient *client, const ipcMessage *msg) = 0;
};

//
// factory method signature for DLLs, which may define more than one ipcModule
// implementation.  the DLL must export the following symbol:
//
// extern "C" ipcModule **IPC_GetModuleList();
//
// return:
//   null terminated array of modules.
//
typedef ipcModule ** (*ipcGetModuleListFunc)(void);

#endif // !ipcModule_h__
