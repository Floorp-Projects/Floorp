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

#ifndef ipcClient_h__
#define ipcClient_h__

#include "prio.h"
#include "ipcMessageQ.h"
#include "ipcStringList.h"
#include "ipcIDList.h"

//-----------------------------------------------------------------------------
// ipcClient
//
// NOTE: this class is an implementation detail of the IPC daemon. IPC daemon
// modules (other than the built-in IPCM module) must not access methods on
// this class directly. use the API provided via ipcd.h instead.
//-----------------------------------------------------------------------------

class ipcClient
{
public:
    int Init();
    int Finalize();
    int Process(PRFileDesc *fd, int poll_flags);

    int ID() const { return mID; }

    void   AddName(const char *name);
    void   DelName(const char *name);
    PRBool HasName(const char *name) const { return mNames.Find(name) != NULL; }

    void   AddTarget(const nsID &target);
    void   DelTarget(const nsID &target);
    PRBool HasTarget(const nsID &target) const { return mTargets.Find(target) != NULL; }

    // list iterators
    const ipcStringNode *Names()   const { return mNames.First(); }
    const ipcIDNode     *Targets() const { return mTargets.First(); }

    //
    // returns TRUE if successfully enqueued.  will return FALSE if client
    // does not have a registered message handler for this message's target.
    //
    // on success or failure, this function takes ownership of |msg| and will
    // delete it when appropriate.
    //
    PRBool EnqueueOutboundMsg(ipcMessage *msg);

private:
    int WriteMsgs(PRFileDesc *fd);

    static int gLastID;

    int                       mID;
    ipcStringList             mNames;
    ipcIDList                 mTargets;
    ipcMessage               *mInMsg;    // buffer for incoming message
    ipcMessageQ               mOutMsgQ;  // outgoing message queue

    // keep track of the amount of the first message sent
    PRUint32                  mSendOffset;
};

#endif // !ipcClient_h__
