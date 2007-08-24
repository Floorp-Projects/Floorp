/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: se cin sw=2 ts=2 et : */
#ifndef nsDownloadScanner_h_
#define nsDownloadScanner_h_

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#define INITGUID
#include <Windows.h>
#define AVVENDOR
#include <msoav.h>

#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsDownloadManager.h"

enum AVScanState
{
  AVSCAN_NOTSTARTED = 0,
  AVSCAN_SCANNING,
  AVSCAN_GOOD,
  AVSCAN_BAD,
  AVSCAN_UGLY,
  AVSCAN_FAILED
};

class nsDownloadScanner
{
public:
  nsDownloadScanner();
  nsresult Init();
  nsresult ScanDownload(nsDownload *download);

private:
  PRBool mHaveAVScanner;
  CLSID mScannerCLSID;
  PRInt32 FindCLSID();

  static unsigned int __stdcall ScannerThreadFunction(void *p);
  class Scan : public nsRunnable
  {
  public:
    Scan(nsDownloadScanner *scanner, nsDownload *download);
    nsresult Start();

  private:
    nsDownloadScanner *mDLScanner;
    nsRefPtr<IOfficeAntiVirus> mAVScanner;
    HANDLE mThread;
    nsRefPtr<nsDownload> mDownload;
    AVScanState mStatus;
    nsString mPath;
    nsString mName;
    nsString mOrigin;
    // Also true if it is an ftp download
    PRBool mIsHttpDownload;
    PRBool mIsReadOnlyRequest;

    NS_IMETHOD Run();

    void DoScan();

    friend unsigned int __stdcall nsDownloadScanner::ScannerThreadFunction(void *);
  };
};
#endif
