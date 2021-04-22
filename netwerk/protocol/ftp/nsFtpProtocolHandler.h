/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFtpProtocolHandler_h__
#define nsFtpProtocolHandler_h__

#include "nsFtpControlConnection.h"
#include "nsIProxiedProtocolHandler.h"
#include "nsTArray.h"
#include "nsITimer.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

//-----------------------------------------------------------------------------

class nsFtpProtocolHandler final : public nsIProxiedProtocolHandler,
                                   public nsIObserver,
                                   public nsSupportsWeakReference {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER
  NS_DECL_NSIPROXIEDPROTOCOLHANDLER
  NS_DECL_NSIOBSERVER

  nsFtpProtocolHandler();

  nsresult Init();

  // FTP Connection list access
  nsresult InsertConnection(nsIURI* aKey, nsFtpControlConnection* aConn);
  nsresult RemoveConnection(nsIURI* aKey, nsFtpControlConnection** aConn);
  uint32_t GetSessionId() { return mSessionId; }

  uint8_t GetDataQoSBits() { return mDataQoSBits; }
  uint8_t GetControlQoSBits() { return mControlQoSBits; }

 private:
  virtual ~nsFtpProtocolHandler();

  // Stuff for the timer callback function
  struct timerStruct {
    nsCOMPtr<nsITimer> timer;
    RefPtr<nsFtpControlConnection> conn;
    char* key;

    timerStruct() : key(nullptr) {}

    ~timerStruct() {
      if (timer) timer->Cancel();
      if (key) free(key);
      if (conn) {
        conn->Disconnect(NS_ERROR_ABORT);
      }
    }
  };

  static void Timeout(nsITimer* aTimer, void* aClosure);
  void ClearAllConnections();

  nsTArray<timerStruct*> mRootConnectionList;

  int32_t mIdleTimeout;
  bool mEnabled;

  // When "clear active logins" is performed, all idle connection are dropped
  // and mSessionId is incremented. When nsFtpState wants to insert idle
  // connection we refuse to cache if its mSessionId is different (i.e.
  // control connection had been created before last "clear active logins" was
  // performed.
  uint32_t mSessionId;

  uint8_t mControlQoSBits;
  uint8_t mDataQoSBits;
};

//-----------------------------------------------------------------------------

extern nsFtpProtocolHandler* gFtpHandler;

#include "mozilla/Logging.h"
extern mozilla::LazyLogModule gFTPLog;

#endif  // !nsFtpProtocolHandler_h__
