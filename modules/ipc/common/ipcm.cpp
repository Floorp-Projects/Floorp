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

#include <string.h>
#include "ipcm.h"
#include "prlog.h"

const nsID IPCM_TARGET =
{ /* 753ca8ff-c8c2-4601-b115-8c2944da1150 */
    0x753ca8ff,
    0xc8c2,
    0x4601,
    {0xb1, 0x15, 0x8c, 0x29, 0x44, 0xda, 0x11, 0x50}
};

//
//  +--------------------+
//  | DWORD : MSG_TYPE   |
//  +--------------------+
//  | (variable)         |
//  +--------------------+
//

int
IPCM_GetMsgType(const ipcMessage *msg)
{
    // make sure message topic matches
    if (msg->Target().Equals(IPCM_TARGET)) {
        // the type is encoded as the first byte
        PRUint32 type = * (PRUint32 *) msg->Data();
        if (type < IPCM_MSG_TYPE_UNKNOWN)
            return type;
    }
    return IPCM_MSG_TYPE_UNKNOWN;
}

//
// MSG_TYPE values
//
const PRUint32 ipcmMessagePing::MSG_TYPE = IPCM_MSG_TYPE_PING;
const PRUint32 ipcmMessageError::MSG_TYPE = IPCM_MSG_TYPE_ERROR;
const PRUint32 ipcmMessageClientHello::MSG_TYPE = IPCM_MSG_TYPE_CLIENT_HELLO;
const PRUint32 ipcmMessageClientID::MSG_TYPE = IPCM_MSG_TYPE_CLIENT_ID;
//const PRUint32 ipcmMessageClientInfo::MSG_TYPE = IPCM_MSG_TYPE_CLIENT_INFO;
const PRUint32 ipcmMessageClientAddName::MSG_TYPE = IPCM_MSG_TYPE_CLIENT_ADD_NAME;
const PRUint32 ipcmMessageClientDelName::MSG_TYPE = IPCM_MSG_TYPE_CLIENT_DEL_NAME;
const PRUint32 ipcmMessageClientAddTarget::MSG_TYPE = IPCM_MSG_TYPE_CLIENT_ADD_TARGET;
const PRUint32 ipcmMessageClientDelTarget::MSG_TYPE = IPCM_MSG_TYPE_CLIENT_DEL_TARGET;
const PRUint32 ipcmMessageQueryClientByName::MSG_TYPE = IPCM_MSG_TYPE_QUERY_CLIENT_BY_NAME;
const PRUint32 ipcmMessageForward::MSG_TYPE = IPCM_MSG_TYPE_FORWARD;

#if 0
//
// CLIST message
//
//  +-----------------------+
//  | BYTE - IPCM_MSG_CLIST |
//  +-----------------------+
//  | cname[0]              |
//  +-----------------------+
//  | null                  |
//  +-----------------------+
//  .                       .
//  .                       .
//  +-----------------------+
//  | cname[num - 1]        |
//  +-----------------------+
//  | null                  |
//  +-----------------------+
//

const char ipcmMessageCLIST::MSG_TYPE = (char) IPCM_MSG_CLIST;

ipcmMessageCLIST::ipcmMessageCLIST(const char *cNames[], PRUint32 num)
{
    PRUint32 i, dataLen = 1;

    for (i = 0; i < num; ++i) {
        dataLen += strlen(cNames[i]);
        dataLen ++;
    }

    Init(IPCM_TARGET, NULL, dataLen);
    
    PRUint32 offset = 1;
    for (i = 0; i < num; ++i) {
        PRUint32 len = strlen(cNames[i]);
        SetData(offset, cNames[i], len + 1);
        offset += len + 1;
    }
}

const char *
ipcmMessageCLIST::NextClientName(const char *cName) const
{
    if (!cName)
        return Data() + 1;

    cName += strlen(cName) + 1;
    if (cName == MsgBuf() + MsgLen())
        cName = NULL;
    return cName;
}
#endif

//
// FWD message
//
//  +-------------------------------+
//  | DWORD : IPCM_MSG_TYPE_FORWARD |
//  +-------------------------------+
//  | clientID                      |
//  +-------------------------------+
//  | innerMsgHeader                |
//  +-------------------------------+
//  | innerMsgData                  |
//  +-------------------------------+
//

ipcmMessageForward::ipcmMessageForward(PRUint32 cID,
                                       const nsID &target,
                                       const char *data,
                                       PRUint32 dataLen)
{
    int len = sizeof(MSG_TYPE) +     // MSG_TYPE
              sizeof(cID) +          // cID
              IPC_MSG_HEADER_SIZE +  // innerMsgHeader
              dataLen;               // innerMsgData

    Init(IPCM_TARGET, NULL, len);

    SetData(0, (char *) &MSG_TYPE, sizeof(MSG_TYPE));
    SetData(4, (char *) &cID, sizeof(cID));

    ipcMessageHeader hdr;
    hdr.mLen = IPC_MSG_HEADER_SIZE + dataLen;
    hdr.mVersion = IPC_MSG_VERSION;
    hdr.mFlags = 0;
    hdr.mTarget = target;

    SetData(8, (char *) &hdr, IPC_MSG_HEADER_SIZE);
    if (data)
        SetInnerData(0, data, dataLen);
}

void
ipcmMessageForward::SetInnerData(PRUint32 offset, const char *data, PRUint32 dataLen)
{
    SetData(8 + IPC_MSG_HEADER_SIZE + offset, data, dataLen);
}

PRUint32
ipcmMessageForward::DestClientID() const
{
    return ((PRUint32 *) Data())[1];
}

const nsID &
ipcmMessageForward::InnerTarget() const
{
    ipcMessageHeader *hdr = (ipcMessageHeader *) (Data() + 8);
    return hdr->mTarget;
}

const char *
ipcmMessageForward::InnerData() const
{
    return Data() + 8 + IPC_MSG_HEADER_SIZE;
}

PRUint32
ipcmMessageForward::InnerDataLen() const
{
    ipcMessageHeader *hdr = (ipcMessageHeader *) (Data() + 8);
    return hdr->mLen - IPC_MSG_HEADER_SIZE;
}
