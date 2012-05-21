/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* vim: se cin sw=2 ts=2 et : */

#ifndef nsDownloadScanner_h_
#define nsDownloadScanner_h_

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#define INITGUID
#include <windows.h>
#define AVVENDOR
#include <objidl.h>
#include <msoav.h>
#include <shlobj.h>

#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsTArray.h"
#include "nsIObserver.h"
#include "nsIURI.h"

enum AVScanState
{
  AVSCAN_NOTSTARTED = 0,
  AVSCAN_SCANNING,
  AVSCAN_GOOD,
  AVSCAN_BAD,
  AVSCAN_UGLY,
  AVSCAN_FAILED,
  AVSCAN_TIMEDOUT
};

enum AVCheckPolicyState
{
  AVPOLICY_DOWNLOAD,
  AVPOLICY_PROMPT,
  AVPOLICY_BLOCKED
};

// See nsDownloadScanner.cpp for declaration and definition
class nsDownloadScannerWatchdog;
class nsDownload;

class nsDownloadScanner
{
public:
  nsDownloadScanner();
  ~nsDownloadScanner();
  nsresult Init();
  nsresult ScanDownload(nsDownload *download);
  AVCheckPolicyState CheckPolicy(nsIURI *aSource, nsIURI *aTarget);

private:
  bool mAESExists;
  nsTArray<CLSID> mScanCLSID;
  bool IsAESAvailable();
  bool EnumerateOAVProviders();

  nsAutoPtr<nsDownloadScannerWatchdog> mWatchdog;

  static unsigned int __stdcall ScannerThreadFunction(void *p);
  class Scan : public nsRunnable
  {
  public:
    Scan(nsDownloadScanner *scanner, nsDownload *download);
    ~Scan();
    nsresult Start();

    // Returns the time that Start was called
    PRTime GetStartTime() const { return mStartTime; }
    // Returns a copy of the thread handle that can be waited on, but not
    // terminated
    // The caller is responsible for closing the handle
    // If the thread has terminated, then this will return the pseudo-handle
    // INVALID_HANDLE_VALUE
    HANDLE GetWaitableThreadHandle() const;

    // Called on a secondary thread to notify the scan that it has timed out
    // this is used only by the watchdog thread
    bool NotifyTimeout();

  private:
    nsDownloadScanner *mDLScanner;
    PRTime mStartTime;
    HANDLE mThread;
    nsRefPtr<nsDownload> mDownload;
    // Guards mStatus
    CRITICAL_SECTION mStateSync;
    AVScanState mStatus;
    nsString mPath;
    nsString mName;
    nsString mOrigin;
    // Also true if it is an ftp download
    bool mIsHttpDownload;
    bool mSkipSource;

    /* @summary Sets the Scan's state to newState if the current state is
                expectedState
     * @param newState The new state of the scan
     * @param expectedState The state that the caller expects the scan to be in
     * @return If the old state matched expectedState
     */
    bool CheckAndSetState(AVScanState newState, AVScanState expectedState);

    NS_IMETHOD Run();

    void DoScan();
    bool DoScanAES();
    bool DoScanOAV();

    friend unsigned int __stdcall nsDownloadScanner::ScannerThreadFunction(void *);
  };
  // Used to give access to Scan
  friend class nsDownloadScannerWatchdog;
};
#endif

