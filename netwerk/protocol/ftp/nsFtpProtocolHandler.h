/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
        
        timerStruct() : conn(nullptr), key(nullptr) {}
        
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
