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

#include "ipcLog.h"
#include "ipcClient.h"
#include "ipcMessage.h"
#include "ipcd.h"
#include "ipcm.h"

#ifdef XP_UNIX
#include "prio.h"
#include "ipcdUnix.h"
#endif

PRUint32 ipcClient::gLastID = 0;

//
// called to initialize this client context
//
// assumptions:
//  - object's memory has already been zero'd out.
//
void
ipcClient::Init()
{
    mID = ++gLastID;
    mInMsg = new ipcMessage();

    // every client must be able to handle IPCM messages.
    mTargets.Append(IPCM_TARGET);
}

//
// called when this client context is going away
//
void
ipcClient::Finalize()
{
    if (mInMsg)
        delete mInMsg;
    mOutMsgQ.DeleteAll();
    mNames.DeleteAll();
    mTargets.DeleteAll();
}

void
ipcClient::AddName(const char *name)
{
    LOG(("adding client name: %s\n", name));

    if (HasName(name))
        return;

    mNames.Append(name);
}

void
ipcClient::DelName(const char *name)
{
    LOG(("deleting client name: %s\n", name));

    mNames.FindAndDelete(name);
}

void
ipcClient::AddTarget(const nsID &target)
{
    LOG(("adding client target\n"));

    if (HasTarget(target))
        return;

    mTargets.Append(target);
}

void
ipcClient::DelTarget(const nsID &target)
{
    LOG(("deleting client target\n"));

    //
    // cannot remove the IPCM target
    //
    if (!target.Equals(IPCM_TARGET))
        mTargets.FindAndDelete(target);
}

PRBool
ipcClient::EnqueueOutboundMsg(ipcMessage *msg)
{
    LOG(("enqueue outbound message\n"));

    if (!HasTarget(msg->Target())) {
        LOG(("  no registered message handler\n"));
        delete msg;
        return PR_FALSE;
    }

    mOutMsgQ.Append(msg);

#ifdef XP_UNIX
    //
    // the message was successfully put on the queue...
    //
    // since our Process method may have already been called, we must ensure
    // that the PR_POLL_WRITE flag is set.
    //
    IPC_ClientWritePending(this);
#endif
    return PR_TRUE;
}

#ifdef XP_UNIX
//
// called to process a client socket
//
// params:
//   fd         - the client socket
//   poll_flags - the state of the client socket
//
// return:
//   0             - to end session with this client
//   PR_POLL_READ  - to wait for the client socket to become readable
//   PR_POLL_WRITE - to wait for the client socket to become writable
//
int
ipcClient::Process(PRFileDesc *fd, int poll_flags)
{
    if ((poll_flags & PR_POLL_ERR) || 
        (poll_flags & PR_POLL_HUP) ||
        (poll_flags & PR_POLL_EXCEPT) ||
        (poll_flags & PR_POLL_NVAL)) {
        //
        // expect Finalize method to be called next.
        //
        LOG(("client socket appears to have closed\n"));
        return 0;
    }

    // always wait for more data
    int ret_flags = PR_POLL_READ;

    if (poll_flags & PR_POLL_READ) {
        LOG(("client socket is now readable\n"));

        char buf[1024];
        PRInt32 n;

        // find out how much data is available for reading...
        // n = PR_Available(fd);

        n = PR_Read(fd, buf, sizeof(buf));
        if (n <= 0)
            return 0; // cancel connection

        char *ptr = buf;
        while (n) {
            PRUint32 nread;
            PRBool complete;

            mInMsg->ReadFrom(ptr, PRUint32(n), &nread, &complete);

            if (complete) {
                IPC_DispatchMsg(this, mInMsg);
                mInMsg->Reset();
            }

            n -= nread;
            ptr += nread;
        }
    }
  
    if (poll_flags & PR_POLL_WRITE) {
        LOG(("client socket is now writable\n"));

        if (mOutMsgQ.First())
            WriteMsgs(fd);
    }

    if (mOutMsgQ.First())
        ret_flags |= PR_POLL_WRITE;

    return ret_flags;
}

//
// called to write out any messages from the outgoing queue.
//
int
ipcClient::WriteMsgs(PRFileDesc *fd)
{
    while (mOutMsgQ.First()) {
        const char *buf = (const char *) mOutMsgQ.First()->MsgBuf();
        PRInt32 bufLen = (PRInt32) mOutMsgQ.First()->MsgLen();

        if (mSendOffset) {
            buf += mSendOffset;
            bufLen -= mSendOffset;
        }

        PRInt32 nw = PR_Write(fd, buf, bufLen);
        if (nw <= 0)
            break;

        LOG(("wrote %d bytes\n", nw));

        if (nw == bufLen)
            mOutMsgQ.DeleteFirst();
        else
            mSendOffset += nw;
    }

    return 0;
}
#endif
