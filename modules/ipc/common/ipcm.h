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

#ifndef ipcm_h__
#define ipcm_h__

#include "ipcMessage.h"

//
// IPCM (IPC Manager) protocol support
//

extern const nsID IPCM_TARGET;

enum {
    IPCM_MSG_PING,
    IPCM_MSG_CNAME,
    IPCM_MSG_CENUM,
    IPCM_MSG_CLIST,
    IPCM_MSG_FWD,
    IPCM_MSG_UNKNOWN // unknown message type
};

//
// returns IPCM message type.
//
int IPCM_GetMsgType(const ipcMessage *msg);

//
// NOTE: this file declares some helper classes that simplify construction
//       and parsing of IPCM messages.  each class subclasses ipcMessage, but
//       adds no additional member variables.  operator new should be used
//       to allocate one of the IPCM helper classes, e.g.:
//         
//          ipcMessage *msg = new ipcmMessageCNAME("foo");
//
//       given an arbitrary ipcMessage, it can be parsed using logic similar
//       to the following:
//
//          ipcMessage *msg = ...
//          if (strcmp(msg->Topic(), "ipcm") == 0) {
//             if (IPCM_GetMsgType(msg) == IPCM_MSG_CNAME) {
//                ipcmMessageCNAME *cnameMsg = (ipcmMessageCNAME *) msg;
//                printf("client name: %s\n", cnameMsg->ClientName());
//             }
//          }
//
//       in other words, these classes are very very lightweight.
//

//
// IPCM_MSG_PING
//
// this message may be sent from either the client or the daemon.
//
class ipcmMessagePING : public ipcMessage
{
public:
    static const char MSG_TYPE;

    ipcmMessagePING();
};

//
// IPCM_MSG_CNAME
//
// this message is sent from a client to specify its name.
//
class ipcmMessageCNAME : public ipcMessage
{
public:
    static const char MSG_TYPE;

    ipcmMessageCNAME(const char *cName);

    //
    // extracts the "client name"
    //
    const char *ClientName() const;
};

//
// IPCM_MSG_CENUM
//
// this message is sent from a client to request a CLIST response.
//
class ipcmMessageCENUM : public ipcMessage
{
public:
    static const char MSG_TYPE;

    ipcmMessageCENUM();
};

//
// IPCM_MSG_CLIST
//
// this message is sent from the daemon in response to a CENUM request.  the
// message contains the list of clients by name.
//
class ipcmMessageCLIST : public ipcMessage
{
public:
    static const char MSG_TYPE;

    ipcmMessageCLIST(const char *clientNames[], PRUint32 numClients);

    //
    // extracts the next client name from the message.  if null is passed in, then
    // the first client name in the list is returned.  null is returned if there
    // aren't anymore client names.  (the returned client name acts as an iterator.)
    //
    const char *NextClientName(const char *clientName = NULL) const;
};

//
// IPCM_MSG_FWD
//
// this message is only sent from the client to the daemon.  the daemon
// will forward the contained message to the specified client.
//
class ipcmMessageFWD : public ipcMessage
{
public:
    static const char MSG_TYPE;

    ipcmMessageFWD(const char *destClient, const ipcMessage *destMsg);

    //
    // extracts the "destination client name"
    //
    const char *DestClient() const;

    //
    // extracts (a copy of) the "destination message"
    //
    PRStatus DestMessage(ipcMessage *destMsg) const;
};

#endif // !ipcm_h__
