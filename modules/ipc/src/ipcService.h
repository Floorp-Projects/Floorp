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

#ifndef ipcService_h__
#define ipcService_h__

#include "ipcIService.h"
#include "ipcTransport.h"
#include "ipcList.h"
#include "ipcMessage.h"
#include "ipcMessageQ.h"
#include "nsIRequest.h"
#include "nsIStreamListener.h"
#include "nsIStreamProvider.h"
#include "nsCOMPtr.h"
#include "nsHashtable.h"

//----------------------------------------------------------------------------
// ipcClientInfo
//----------------------------------------------------------------------------

class ipcClientInfo : public ipcIClientInfo
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_IPCICLIENTINFO

    ipcClientInfo() : mID(0) { NS_INIT_ISUPPORTS(); }
    virtual ~ipcClientInfo() {}

    void Init(PRUint32 cID, const nsACString &cName)
    {
        mID = cID;
        mName = cName;
    }

private:
    PRUint32  mID;
    nsCString mName;
};

//----------------------------------------------------------------------------
// ipcClientQuery
//----------------------------------------------------------------------------

class ipcClientQuery
{
public:
    ipcClientQuery()
        : mNext(nsnull)
        , mReqToken(0)
        { }

    ipcClientQuery              *mNext;
    nsCString                    mName;
    PRUint32                     mReqToken;
    nsCOMPtr<ipcIClientObserver> mObserver;
};

typedef ipcList<ipcClientQuery> ipcClientQueryQ;

//----------------------------------------------------------------------------
// ipcService
//----------------------------------------------------------------------------

class ipcService : public ipcIService
                 , public ipcTransportObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_IPCISERVICE

    ipcService();
    virtual ~ipcService();

    nsresult Init();

private:
    nsresult ErrorAccordingToIPCM(PRUint32 err);
    void     HandleQueryResult(const ipcMessage *, PRBool succeeded); 

    // ipcTransportObserver:
    void OnConnectionEstablished(PRUint32 clientID);
    void OnConnectionLost();
    void OnMessageAvailable(const ipcMessage *);

    static PRUint32 gLastReqToken;

    nsHashtable     mObserverDB;
    ipcTransport   *mTransport;
    PRUint32        mClientID;

    ipcClientQueryQ mQueryQ;
};

#endif // !ipcService_h__
