/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsDNSService_h__
#define nsDNSService_h__

#include "nsIDNSService.h"
#include "nsIRunnable.h"
#include "nsIObserver.h"
#include "nsIThread.h"
#include "nsISupportsArray.h"
#if defined(XP_MAC)
#include <OSUtils.h>
#include <OpenTransport.h>
#include <OpenTptInternet.h>
#elif defined (XP_WIN)
#include <windows.h>
#include <Winsock2.h>
#endif
#include "nsCOMPtr.h"
#include "prclist.h"
#include "prcvar.h"
#include "pldhash.h"

#ifdef DEBUG
#define DNS_TIMING 1
#endif

class nsIDNSListener;
class nsDNSLookup;

/******************************************************************************
 *  nsDNSService
 *****************************************************************************/
class nsDNSService : public nsIDNSService,
                     public nsIRunnable,
                     public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSIDNSSERVICE
    NS_DECL_NSIOBSERVER
    
    // nsDNSService methods:
    nsDNSService();
    virtual ~nsDNSService();
 
    // Define a Create method to be used with a factory:
    static NS_METHOD        Create( nsISupports * outer,
                                    const nsIID & iid,
                                    void **       result);
    
    static void             Lock();
    static void             Unlock();

    void                    EnqueuePendingQ(nsDNSLookup * lookup);
    nsDNSLookup *           DequeuePendingQ();
    
    static PRInt32          ExpirationInterval();

    static nsDNSService *   gService;

private:
    nsresult                LateInit();
    nsresult                InstallPrefObserver();
    nsresult                RemovePrefObserver();
    nsDNSLookup *           FindOrCreateLookup( const char *  hostName);
    void                    EvictLookup( nsDNSLookup * lookup);
    void                    AddToEvictionQ( nsDNSLookup *  lookup);
    void                    AbortLookups();
    
        
    static PRBool           gNeedLateInitialization;
    static PLDHashTableOps  gHashTableOps;

    PRLock *                mDNSServiceLock;
    PRCondVar *             mDNSCondVar;
    PLDHashTable            mHashTable;
    PRCList                 mPendingQ;
    PRCList                 mEvictionQ;
    PRInt32                 mEvictionQCount;
    PRInt32                 mMaxCachedLookups;
    PRInt32                 mExpirationInterval;
    
    char *                  mMyIPAddress;
    nsCOMPtr<nsIThread>     mThread;
    PRUint32                mState;

    enum {
        DNS_NOT_INITIALIZED = 0,
        DNS_RUNNING         = 1,
        DNS_SHUTTING_DOWN   = 2,
        DNS_SHUTDOWN        = 3
    };

#if defined(XP_MAC)
    friend pascal void      nsDnsServiceNotifierRoutine(void * contextPtr, OTEventCode code, OTResult result, void * cookie);
public:
    InetSvcRef              mServiceRef;
    OTNotifyUPP             nsDnsServiceNotifierRoutineUPP;
#if TARGET_CARBON
    OTClientContextPtr      mClientContext;
#endif /* TARGET_CARBON */
private:
#endif /* XP_MAC */

#if defined(XP_WIN)
    friend static
    LRESULT CALLBACK        nsDNSEventProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:

    PRUint32                AllocMsgID( void);
    HWND                    mDNSWindow;

private:
    void                    FreeMsgID( PRUint32 msgID);
	LRESULT                 ProcessLookup( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
 
    PRUint32                mMsgIDBitVector[4];
#endif /* XP_WIN */


#ifdef DNS_TIMING
    friend class nsDNSRequest;

    double              mCount;
    double              mTimes;
    double              mSquaredTimes;
    FILE *              mOut;
#endif
};

#endif /* nsDNSService_h__ */
