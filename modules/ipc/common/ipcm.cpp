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

static const char     IPCM_PING[]    = { (char) IPCM_MSG_PING };
static const PRUint32 IPCM_PING_LEN  = sizeof(IPCM_PING);

static const char     IPCM_CNAME[]   = { (char) IPCM_MSG_CNAME };
static const PRUint32 IPCM_CNAME_LEN = sizeof(IPCM_CNAME);

//
//  +--------------------+
//  | BYTE : msgType     |
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
        char type = msg->Data()[0];
        if (type < IPCM_MSG_UNKNOWN)
            return type;
    }
    return IPCM_MSG_UNKNOWN;
}

//
// PING message
//
//  +----------------------+
//  | BYTE - IPCM_MSG_PING |
//  +----------------------+
//

const char ipcmMessagePING::MSG_TYPE = (char) IPCM_MSG_PING;

ipcmMessagePING::ipcmMessagePING()
{
    Init(IPCM_TARGET, &MSG_TYPE, 1);
}

//
// CNAME message
//
//  +-----------------------+
//  | BYTE - IPCM_MSG_CNAME |
//  +-----------------------+
//  | clientName            |
//  +-----------------------+
//  | null                  |
//  +-----------------------+
//

const char ipcmMessageCNAME::MSG_TYPE = (char) IPCM_MSG_CNAME;

ipcmMessageCNAME::ipcmMessageCNAME(const char *cName)
{
    int cLen = strlen(cName);
    int dataLen = 1 +       // msg_type
                  cLen +    // cName
                  1;        // null

    Init(IPCM_TARGET, NULL, dataLen);
    SetData(0, &MSG_TYPE, 1);
    SetData(1, cName, cLen + 1);
}

const char *
ipcmMessageCNAME::ClientName() const
{
    // make sure data is null terminated
    const char *data = Data();
    if (data[DataLen() - 1] != '\0')
        return NULL;

    return Data() + 1;
}

//
// CENUM message
//
//  +-----------------------+
//  | BYTE - IPCM_MSG_CENUM |
//  +-----------------------+
//

const char ipcmMessageCENUM::MSG_TYPE = (char) IPCM_MSG_CENUM;

ipcmMessageCENUM::ipcmMessageCENUM()
{
    Init(IPCM_TARGET, &MSG_TYPE, 1);
}

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

//
// FWD message
//
//  +----------------------+
//  | BYTE - IPCM_MSG_FWD  |
//  +----------------------+
//  | dest_client          |
//  +----------------------+
//  | null                 |
//  +----------------------+
//  | dest_msg             |
//  +----------------------+
//

const char ipcmMessageFWD::MSG_TYPE = (char) IPCM_MSG_FWD;

ipcmMessageFWD::ipcmMessageFWD(const char *dClient, const ipcMessage *dMsg)
{
    int cLen = strlen(dClient);
    int dataLen = 1 +             // msg_type
                  cLen +          // dest_client
                  1 +             // null
                  dMsg->MsgLen(); // dest_msg

    Init(IPCM_TARGET, NULL, dataLen);
    SetData(0, &MSG_TYPE, 1);
    SetData(1, dClient, cLen + 1);
    SetData(1 + cLen + 1, dMsg->MsgBuf(), dMsg->MsgLen());
}

const char *
ipcmMessageFWD::DestClient() const
{
    return Data() + 1;
}

PRStatus
ipcmMessageFWD::DestMessage(ipcMessage *msg) const
{
    const char *ptr = DestClient();

    // XXX use a custom loop here to avoid walking off the end of the buffer
    PRUint32 cLen = strlen(ptr);
    PRUint32 dLen = MsgLen() - 1 - cLen - 1;
    PRUint32 bytesRead;
    PRBool complete;

    ptr += (cLen + 1);
    if (msg->ReadFrom(ptr, dLen, &bytesRead, &complete) != PR_SUCCESS || !complete)
        return PR_FAILURE;

    return PR_SUCCESS;
}
