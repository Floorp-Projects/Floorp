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
#include "ipcMessagePrimitives.h"

//
// IPCM (IPC Manager) protocol support
//

extern const nsID IPCM_TARGET;

enum {
    IPCM_MSG_TYPE_PING,
    IPCM_MSG_TYPE_ERROR,
    IPCM_MSG_TYPE_CLIENT_HELLO,
    IPCM_MSG_TYPE_CLIENT_ID,
    IPCM_MSG_TYPE_CLIENT_INFO,
    IPCM_MSG_TYPE_CLIENT_ADD_NAME,
    IPCM_MSG_TYPE_CLIENT_DEL_NAME,
    IPCM_MSG_TYPE_CLIENT_ADD_TARGET,
    IPCM_MSG_TYPE_CLIENT_DEL_TARGET,
    IPCM_MSG_TYPE_QUERY_CLIENT_BY_NAME,
    IPCM_MSG_TYPE_QUERY_CLIENT_INFO,
    IPCM_MSG_TYPE_QUERY_FAILED,
    IPCM_MSG_TYPE_FORWARD,
    IPCM_MSG_TYPE_UNKNOWN // unknown message type
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
//          ipcMessage *msg = new ipcmMessageClientHello("foo");
//
//       given an arbitrary ipcMessage, it can be parsed using logic similar
//       to the following:
//
//          void func(const ipcMessage *unknown)
//          {
//            if (unknown->Topic().Equals(IPCM_TARGET)) {
//              if (IPCM_GetMsgType(unknown) == IPCM_MSG_TYPE_CLIENT_ID) {
//                ipcMessageCast<ipcmMessageClientID> msg(unknown);
//                printf("Client ID: %u\n", msg->ClientID());
//              }
//            }
//          }
//
//       in other words, these classes are very very lightweight.
//

//
// IPCM_MSG_TYPE_PING
//
// this message may be sent from either the client or the daemon.
// if the daemon receives this message, then it will respond by 
// sending back a PING to the client.
//
class ipcmMessagePing : public ipcMessage_DWORD
{
public:
    static const PRUint32 MSG_TYPE;

    ipcmMessagePing()
        : ipcMessage_DWORD(IPCM_TARGET, MSG_TYPE) {}
};

//
// IPCM_MSG_TYPE_ERROR
//
// thie message may be sent from the daemon in response to a query.
//
class ipcmMessageError : public ipcMessage_DWORD_DWORD
{
public:
    static const PRUint32 MSG_TYPE;

    ipcmMessageError(PRUint32 reason)
        : ipcMessage_DWORD_DWORD(IPCM_TARGET, MSG_TYPE, reason) {}

    PRUint32 Reason() const { return Second(); }
};

enum {
    IPCM_ERROR_CLIENT_NOT_FOUND = 1
};

//
// IPCM_MSG_TYPE_CLIENT_HELLO
//
// this message is always the first message sent from a client to register
// itself with the daemon.  the daemon responds to this message by sending
// the client a CLIENT_ID message informing the client of its client ID.
//
// XXX may want to pass other information here.
//
class ipcmMessageClientHello : public ipcMessage_DWORD_STR
{
public:
    static const PRUint32 MSG_TYPE;

    ipcmMessageClientHello(const char *primaryName)
        : ipcMessage_DWORD_STR(IPCM_TARGET, MSG_TYPE, primaryName) {}

    const char *PrimaryName() const { return Second(); }
};

//
// IPCM_MSG_TYPE_CLIENT_ID
//
// this message is sent from the daemon to identify a client's ID.
//
class ipcmMessageClientID : public ipcMessage_DWORD_DWORD
{
public:
    static const PRUint32 MSG_TYPE;

    ipcmMessageClientID(PRUint32 clientID)
        : ipcMessage_DWORD_DWORD(IPCM_TARGET, MSG_TYPE, clientID) {}

    PRUint32 ClientID() const { return Second(); }
};

//
// IPCM_MSG_TYPE_QUERY_CLIENT_BY_NAME
//
// this message is sent from a client to the daemon to request the ID of the
// client corresponding to the given name or alias.  in response the daemon
// will either send a CLIENT_ID or ERROR message.
//
class ipcmMessageQueryClientByName : public ipcMessage_DWORD_STR
{
public:
    static const PRUint32 MSG_TYPE;

    ipcmMessageQueryClientByName(const char *name)
        : ipcMessage_DWORD_STR(IPCM_TARGET, MSG_TYPE, name) {}

    const char *Name() const { return Second(); }
};

//
// IPCM_MSG_TYPE_FORWARD
//
// this message is only sent from the client to the daemon.  the daemon
// will forward the contained message to the specified client.
//
class ipcmMessageForward : public ipcMessage
{
public:
    static const PRUint32 MSG_TYPE;

    //
    // params:
    //   clientID - the client to which the message should be forwarded
    //   target   - the message target
    //   data     - the message data
    //   dataLen  - the message data length
    //
    ipcmMessageForward(PRUint32 clientID,
                       const nsID &target,
                       const char *data,
                       PRUint32 dataLen);

    //
    // set inner message data, constrained to the data length passed
    // to this class's constructor.
    //
    void SetInnerData(PRUint32 offset, const char *data, PRUint32 dataLen);

    PRUint32    DestClientID() const;
    const nsID &InnerTarget() const;
    const char *InnerData() const;
    PRUint32    InnerDataLen() const;
};

#endif // !ipcm_h__
