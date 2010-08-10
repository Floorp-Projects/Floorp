/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bradley Baetz <bbaetz@cs.mcgill.ca>
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

#ifndef nsFtpProtocolHandler_h__
#define nsFtpProtocolHandler_h__

#include "nsFtpControlConnection.h"
#include "nsIServiceManager.h"
#include "nsIProxiedProtocolHandler.h"
#include "nsTArray.h"
#include "nsIIOService.h"
#include "nsITimer.h"
#include "nsIObserverService.h"
#include "nsICacheSession.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsCRT.h"

class nsITimer;
class nsIStreamListener;

//-----------------------------------------------------------------------------

class nsFtpProtocolHandler : public nsIProxiedProtocolHandler
                           , public nsIObserver
                           , public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIPROXIEDPROTOCOLHANDLER
    NS_DECL_NSIOBSERVER
    
    nsFtpProtocolHandler();
    virtual ~nsFtpProtocolHandler();
    
    nsresult Init();

    // FTP Connection list access
    nsresult InsertConnection(nsIURI *aKey, nsFtpControlConnection *aConn);
    nsresult RemoveConnection(nsIURI *aKey, nsFtpControlConnection **aConn);
    PRUint32 GetSessionId() { return mSessionId; }

    PRUint8 GetDataQoSBits() { return mDataQoSBits; }
    PRUint8 GetControlQoSBits() { return mControlQoSBits; }

private:
    // Stuff for the timer callback function
    struct timerStruct {
        nsCOMPtr<nsITimer>      timer;
        nsFtpControlConnection *conn;
        char                   *key;
        
        timerStruct() : conn(nsnull), key(nsnull) {}
        
        ~timerStruct() {
            if (timer)
                timer->Cancel();
            if (key)
                nsMemory::Free(key);
            if (conn) {
                conn->Disconnect(NS_ERROR_ABORT);
                NS_RELEASE(conn);
            }
        }
    };

    static void Timeout(nsITimer *aTimer, void *aClosure);
    void ClearAllConnections();

    nsTArray<timerStruct*> mRootConnectionList;

    nsCOMPtr<nsICacheSession> mCacheSession;
    PRInt32 mIdleTimeout;

    // When "clear active logins" is performed, all idle connection are dropped
    // and mSessionId is incremented. When nsFtpState wants to insert idle
    // connection we refuse to cache if its mSessionId is different (i.e.
    // control connection had been created before last "clear active logins" was
    // performed.
    PRUint32 mSessionId;

    PRUint8 mControlQoSBits;
    PRUint8 mDataQoSBits;
};

//-----------------------------------------------------------------------------

extern nsFtpProtocolHandler *gFtpHandler;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gFTPLog;
#endif

#endif // !nsFtpProtocolHandler_h__
