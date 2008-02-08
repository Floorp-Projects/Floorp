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
// To cope with both msvs8 header and sdk6 header
#ifdef _WIN32_IE_IE60SP2
#undef _WIN32_IE
#define _WIN32_IE _WIN32_IE_IE60SP2
#endif
#include <shlobj.h>

#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsDownloadManager.h"
#include "nsTArray.h"

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
  PRBool mHaveAttachmentExecute;
  nsTArray<CLSID> mScanCLSID;
  PRBool IsAESAvailable();
  PRInt32 ListCLSID();

  static unsigned int __stdcall ScannerThreadFunction(void *p);
  class Scan : public nsRunnable
  {
  public:
    Scan(nsDownloadScanner *scanner, nsDownload *download);
    nsresult Start();

  private:
    nsDownloadScanner *mDLScanner;
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
    void DoScanAES();
    void DoScanOAV();

    friend unsigned int __stdcall nsDownloadScanner::ScannerThreadFunction(void *);
  };
};
#endif
