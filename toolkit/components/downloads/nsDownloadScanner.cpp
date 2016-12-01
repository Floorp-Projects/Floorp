/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: se cin sw=2 ts=2 et : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDownloadScanner.h"
#include <comcat.h>
#include <process.h>
#include "nsDownloadManager.h"
#include "nsIXULAppInfo.h"
#include "nsXULAppAPI.h"
#include "nsIPrefService.h"
#include "nsNetUtil.h"
#include "prtime.h"
#include "nsDeque.h"
#include "nsIFileURL.h"
#include "nsIPrefBranch.h"
#include "nsXPCOMCIDInternal.h"

/**
 * Code overview
 *
 * Download scanner attempts to make use of one of two different virus
 * scanning interfaces available on Windows - IOfficeAntiVirus (Windows
 * 95/NT 4 and IE 5) and IAttachmentExecute (XPSP2 and up).  The latter
 * interface supports calling IOfficeAntiVirus internally, while also
 * adding support for XPSP2+ ADS forks which define security related
 * prompting on downloaded content.  
 *
 * Both interfaces are synchronous and can take a while, so it is not a
 * good idea to call either from the main thread. Some antivirus scanners can
 * take a long time to scan or the call might block while the scanner shows
 * its UI so if the user were to download many files that finished around the
 * same time, they would have to wait a while if the scanning were done on
 * exactly one other thread. Since the overhead of creating a thread is
 * relatively small compared to the time it takes to download a file and scan
 * it, a new thread is spawned for each download that is to be scanned. Since
 * most of the mozilla codebase is not threadsafe, all the information needed
 * for the scanner is gathered in the main thread in nsDownloadScanner::Scan::Start.
 * The only function of nsDownloadScanner::Scan which is invoked on another
 * thread is DoScan.
 *
 * Watchdog overview
 *
 * The watchdog is used internally by the scanner. It maintains a queue of
 * current download scans. In a separate thread, it dequeues the oldest scan
 * and waits on that scan's thread with a timeout given by WATCHDOG_TIMEOUT
 * (default is 30 seconds). If the wait times out, then the watchdog notifies
 * the Scan that it has timed out. If the scan really has timed out, then the
 * Scan object will dispatch its run method to the main thread; this will
 * release the watchdog thread's addref on the Scan. If it has not timed out
 * (i.e. the Scan just finished in time), then the watchdog dispatches a 
 * ReleaseDispatcher to release its ref of the Scan on the main thread.
 *
 * In order to minimize execution time, there are two events used to notify the
 * watchdog thread of a non-empty queue and a quit event. Every blocking wait 
 * that the watchdog thread does waits on the quit event; this lets the thread
 * quickly exit when shutting down. Also, the download scan queue will be empty
 * most of the time; rather than use a spin loop, a simple event is triggered
 * by the main thread when a new scan is added to an empty queue. When the
 * watchdog thread knows that it has run out of elements in the queue, it will
 * wait on the new item event.
 *
 * Memory/resource leaks due to timeout:
 * In the event of a timeout, the thread must remain alive; terminating it may
 * very well cause the antivirus scanner to crash or be put into an
 * inconsistent state; COM resources may also not be cleaned up. The downside
 * is that we need to leave the thread running; suspending it may lead to a
 * deadlock. Because the scan call may be ongoing, it may be dependent on the
 * memory referenced by the MSOAVINFO structure, so we cannot free mName, mPath
 * or mOrigin; this means that we cannot free the Scan object since doing so
 * will deallocate that memory. Note that mDownload is set to null upon timeout
 * or completion, so the download itself is never leaked. If the scan does
 * eventually complete, then the all the memory and resources will be freed.
 * It is possible, however extremely rare, that in the event of a timeout, the
 * mStateSync critical section will leak its event; this will happen only if
 * the scanning thread, watchdog thread or main thread try to enter the
 * critical section when one of the others is already in it.
 *
 * Reasoning for CheckAndSetState - there exists a race condition between the time when
 * either the timeout or normal scan sets the state and when Scan::Run is
 * executed on the main thread. Ex: mStatus could be set by Scan::DoScan* which
 * then queues a dispatch on the main thread. Before that dispatch is executed,
 * the timeout code fires and sets mStatus to AVSCAN_TIMEDOUT which then queues
 * its dispatch to the main thread (the same function as DoScan*). Both
 * dispatches run and both try to set the download state to AVSCAN_TIMEDOUT
 * which is incorrect.
 *
 * There are 5 possible outcomes of the virus scan:
 *    AVSCAN_GOOD     => the file is clean
 *    AVSCAN_BAD      => the file has a virus
 *    AVSCAN_UGLY     => the file had a virus, but it was cleaned
 *    AVSCAN_FAILED   => something else went wrong with the virus scanner.
 *    AVSCAN_TIMEDOUT => the scan (thread setup + execution) took too long
 *
 * Both the good and ugly states leave the user with a benign file, so they
 * transition to the finished state. Bad files are sent to the blocked state.
 * The failed and timedout states transition to finished downloads.
 *
 * Possible Future enhancements:
 *  * Create an interface for scanning files in general
 *  * Make this a service
 *  * Get antivirus scanner status via WMI/registry
 */

// IAttachementExecute supports user definable settings for certain
// security related prompts. This defines a general GUID for use in
// all projects. Individual projects can define an individual guid
// if they want to.
#ifndef MOZ_VIRUS_SCANNER_PROMPT_GUID
#define MOZ_VIRUS_SCANNER_PROMPT_GUID \
  { 0xb50563d1, 0x16b6, 0x43c2, { 0xa6, 0x6a, 0xfa, 0xe6, 0xd2, 0x11, 0xf2, \
  0xea } }
#endif
static const GUID GUID_MozillaVirusScannerPromptGeneric =
  MOZ_VIRUS_SCANNER_PROMPT_GUID;

// Initial timeout is 30 seconds
#define WATCHDOG_TIMEOUT (30*PR_USEC_PER_SEC)

// Maximum length for URI's passed into IAE
#define MAX_IAEURILENGTH 1683

class nsDownloadScannerWatchdog 
{
  typedef nsDownloadScanner::Scan Scan;
public:
  nsDownloadScannerWatchdog();
  ~nsDownloadScannerWatchdog();

  nsresult Init();
  nsresult Shutdown();

  void Watch(Scan *scan);
private:
  static unsigned int __stdcall WatchdogThread(void *p);
  CRITICAL_SECTION mQueueSync;
  nsDeque mScanQueue;
  HANDLE mThread;
  HANDLE mNewItemEvent;
  HANDLE mQuitEvent;
};

nsDownloadScanner::nsDownloadScanner() :
  mAESExists(false)
{
}
 
// This destructor appeases the compiler; it would otherwise complain about an
// incomplete type for nsDownloadWatchdog in the instantiation of
// nsAutoPtr::~nsAutoPtr
// Plus, it's a handy location to call nsDownloadScannerWatchdog::Shutdown from
nsDownloadScanner::~nsDownloadScanner() {
  if (mWatchdog)
    (void)mWatchdog->Shutdown();
}

nsresult
nsDownloadScanner::Init()
{
  // This CoInitialize/CoUninitialize pattern seems to be common in the Mozilla
  // codebase. All other COM calls/objects are made on different threads.
  nsresult rv = NS_OK;
  CoInitialize(nullptr);

  if (!IsAESAvailable()) {
    CoUninitialize();
    return NS_ERROR_NOT_AVAILABLE;
  }

  mAESExists = true;

  // Initialize scanning
  mWatchdog = new nsDownloadScannerWatchdog();
  if (mWatchdog) {
    rv = mWatchdog->Init();
    if (FAILED(rv))
      mWatchdog = nullptr;
  } else {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  
  if (NS_FAILED(rv))
    return rv;

  return rv;
}

bool
nsDownloadScanner::IsAESAvailable()
{
  // Try to instantiate IAE to see if it's available.    
  RefPtr<IAttachmentExecute> ae;
  HRESULT hr;
  hr = CoCreateInstance(CLSID_AttachmentServices, nullptr, CLSCTX_INPROC,
                        IID_IAttachmentExecute, getter_AddRefs(ae));
  if (FAILED(hr)) {
    NS_WARNING("Could not instantiate attachment execution service\n");
    return false;
  }
  return true;
}

// If IAttachementExecute is available, use the CheckPolicy call to find out
// if this download should be prevented due to Security Zone Policy settings.
AVCheckPolicyState
nsDownloadScanner::CheckPolicy(nsIURI *aSource, nsIURI *aTarget)
{
  nsresult rv;

  if (!mAESExists || !aSource || !aTarget)
    return AVPOLICY_DOWNLOAD;

  nsAutoCString source;
  rv = aSource->GetSpec(source);
  if (NS_FAILED(rv))
    return AVPOLICY_DOWNLOAD;

  nsCOMPtr<nsIFileURL> fileUrl(do_QueryInterface(aTarget));
  if (!fileUrl)
    return AVPOLICY_DOWNLOAD;

  nsCOMPtr<nsIFile> theFile;
  nsAutoString aFileName;
  if (NS_FAILED(fileUrl->GetFile(getter_AddRefs(theFile))) ||
      NS_FAILED(theFile->GetLeafName(aFileName)))
    return AVPOLICY_DOWNLOAD;

  // IAttachementExecute prohibits src data: schemes by default but we
  // support them. If this is a data src, skip off doing a policy check.
  // (The file will still be scanned once it lands on the local system.)
  bool isDataScheme(false);
  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(aSource);
  if (innerURI)
    (void)innerURI->SchemeIs("data", &isDataScheme);
  if (isDataScheme)
    return AVPOLICY_DOWNLOAD;

  RefPtr<IAttachmentExecute> ae;
  HRESULT hr;
  hr = CoCreateInstance(CLSID_AttachmentServices, nullptr, CLSCTX_INPROC,
                        IID_IAttachmentExecute, getter_AddRefs(ae));
  if (FAILED(hr))
    return AVPOLICY_DOWNLOAD;

  (void)ae->SetClientGuid(GUID_MozillaVirusScannerPromptGeneric);
  (void)ae->SetSource(NS_ConvertUTF8toUTF16(source).get());
  (void)ae->SetFileName(aFileName.get());

  // Any failure means the file download/exec will be blocked by the system.
  // S_OK or S_FALSE imply it's ok.
  hr = ae->CheckPolicy();

  if (hr == S_OK)
    return AVPOLICY_DOWNLOAD;

  if (hr == S_FALSE)
    return AVPOLICY_PROMPT;

  if (hr == E_INVALIDARG)
    return AVPOLICY_PROMPT;

  return AVPOLICY_BLOCKED;
}

#ifndef THREAD_MODE_BACKGROUND_BEGIN
#define THREAD_MODE_BACKGROUND_BEGIN 0x00010000
#endif

#ifndef THREAD_MODE_BACKGROUND_END
#define THREAD_MODE_BACKGROUND_END 0x00020000
#endif

unsigned int __stdcall
nsDownloadScanner::ScannerThreadFunction(void *p)
{
  HANDLE currentThread = GetCurrentThread();
  NS_ASSERTION(!NS_IsMainThread(), "Antivirus scan should not be run on the main thread");
  nsDownloadScanner::Scan *scan = static_cast<nsDownloadScanner::Scan*>(p);
  if (!SetThreadPriority(currentThread, THREAD_MODE_BACKGROUND_BEGIN))
    (void)SetThreadPriority(currentThread, THREAD_PRIORITY_IDLE);
  scan->DoScan();
  (void)SetThreadPriority(currentThread, THREAD_MODE_BACKGROUND_END);
  _endthreadex(0);
  return 0;
}

// The sole purpose of this class is to release an object on the main thread
// It assumes that its creator will addref it and it will release itself on
// the main thread too
class ReleaseDispatcher : public mozilla::Runnable {
public:
  ReleaseDispatcher(nsISupports *ptr)
    : mPtr(ptr) {}
  NS_IMETHOD Run();
private:
  nsISupports *mPtr;
};

nsresult ReleaseDispatcher::Run() {
  NS_ASSERTION(NS_IsMainThread(), "Antivirus scan release dispatch should be run on the main thread");
  NS_RELEASE(mPtr);
  NS_RELEASE_THIS();
  return NS_OK;
}

nsDownloadScanner::Scan::Scan(nsDownloadScanner *scanner, nsDownload *download)
  : mDLScanner(scanner), mThread(nullptr), 
    mDownload(download), mStatus(AVSCAN_NOTSTARTED),
    mSkipSource(false)
{
  InitializeCriticalSection(&mStateSync);
}

nsDownloadScanner::Scan::~Scan() {
  DeleteCriticalSection(&mStateSync);
}

nsresult
nsDownloadScanner::Scan::Start()
{
  mStartTime = PR_Now();

  mThread = (HANDLE)_beginthreadex(nullptr, 0, ScannerThreadFunction,
      this, CREATE_SUSPENDED, nullptr);
  if (!mThread)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = NS_OK;

  // Get the path to the file on disk
  nsCOMPtr<nsIFile> file;
  rv = mDownload->GetTargetFile(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = file->GetPath(mPath);
  NS_ENSURE_SUCCESS(rv, rv);

  // Grab the app name
  nsCOMPtr<nsIXULAppInfo> appinfo =
    do_GetService(XULAPPINFO_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString name;
  rv = appinfo->GetName(name);
  NS_ENSURE_SUCCESS(rv, rv);
  CopyUTF8toUTF16(name, mName);

  // Get the origin
  nsCOMPtr<nsIURI> uri;
  rv = mDownload->GetSource(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString origin;
  rv = uri->GetSpec(origin);
  NS_ENSURE_SUCCESS(rv, rv);

  // Certain virus interfaces do not like extremely long uris.
  // Chop off the path and cgi data and just pass the base domain. 
  if (origin.Length() > MAX_IAEURILENGTH) {
    rv = uri->GetPrePath(origin);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  CopyUTF8toUTF16(origin, mOrigin);

  // We count https/ftp/http as an http download
  bool isHttp(false), isFtp(false), isHttps(false);
  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(uri);
  if (!innerURI) innerURI = uri;
  (void)innerURI->SchemeIs("http", &isHttp);
  (void)innerURI->SchemeIs("ftp", &isFtp);
  (void)innerURI->SchemeIs("https", &isHttps);
  mIsHttpDownload = isHttp || isFtp || isHttps;

  // IAttachementExecute prohibits src data: schemes by default but we
  // support them. Mark the download if it's a data scheme, so we
  // can skip off supplying the src to IAttachementExecute when we scan
  // the resulting file.
  (void)innerURI->SchemeIs("data", &mSkipSource);

  // ResumeThread returns the previous suspend count
  if (1 != ::ResumeThread(mThread)) {
    CloseHandle(mThread);
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

nsresult
nsDownloadScanner::Scan::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Antivirus scan dispatch should be run on the main thread");

  // Cleanup our thread
  if (mStatus != AVSCAN_TIMEDOUT)
    WaitForSingleObject(mThread, INFINITE);
  CloseHandle(mThread);

  DownloadState downloadState = 0;
  EnterCriticalSection(&mStateSync);
  switch (mStatus) {
    case AVSCAN_BAD:
      downloadState = nsIDownloadManager::DOWNLOAD_DIRTY;
      break;
    default:
    case AVSCAN_FAILED:
    case AVSCAN_GOOD:
    case AVSCAN_UGLY:
    case AVSCAN_TIMEDOUT:
      downloadState = nsIDownloadManager::DOWNLOAD_FINISHED;
      break;
  }
  LeaveCriticalSection(&mStateSync);
  // Download will be null if we already timed out
  if (mDownload)
    (void)mDownload->SetState(downloadState);

  // Clean up some other variables
  // In the event of a timeout, our destructor won't be called
  mDownload = nullptr;

  NS_RELEASE_THIS();
  return NS_OK;
}

static DWORD
ExceptionFilterFunction(DWORD exceptionCode) {
  switch(exceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW:
      return EXCEPTION_EXECUTE_HANDLER;
    default:
      return EXCEPTION_CONTINUE_SEARCH;
  }
}

bool
nsDownloadScanner::Scan::DoScanAES()
{
  // This warning is for the destructor of ae which will not be invoked in the
  // event of a win32 exception
#pragma warning(disable: 4509)
  HRESULT hr;
  RefPtr<IAttachmentExecute> ae;
  MOZ_SEH_TRY {
    hr = CoCreateInstance(CLSID_AttachmentServices, nullptr, CLSCTX_ALL,
                          IID_IAttachmentExecute, getter_AddRefs(ae));
  } MOZ_SEH_EXCEPT(ExceptionFilterFunction(GetExceptionCode())) {
    return CheckAndSetState(AVSCAN_NOTSTARTED,AVSCAN_FAILED);
  }

  // If we (somehow) already timed out, then don't bother scanning
  if (CheckAndSetState(AVSCAN_SCANNING, AVSCAN_NOTSTARTED)) {
    AVScanState newState;
    if (SUCCEEDED(hr)) {
      bool gotException = false;
      MOZ_SEH_TRY {
        (void)ae->SetClientGuid(GUID_MozillaVirusScannerPromptGeneric);
        (void)ae->SetLocalPath(mPath.get());
        // Provide the src for everything but data: schemes.
        if (!mSkipSource)
          (void)ae->SetSource(mOrigin.get());

        // Save() will invoke the scanner
        hr = ae->Save();
      } MOZ_SEH_EXCEPT(ExceptionFilterFunction(GetExceptionCode())) {
        gotException = true;
      }

      MOZ_SEH_TRY {
        ae = nullptr;
      } MOZ_SEH_EXCEPT(ExceptionFilterFunction(GetExceptionCode())) {
        gotException = true;
      }

      if(gotException) {
        newState = AVSCAN_FAILED;
      }
      else if (SUCCEEDED(hr)) { // Passed the scan
        newState = AVSCAN_GOOD;
      }
      else if (HRESULT_CODE(hr) == ERROR_FILE_NOT_FOUND) {
        NS_WARNING("Downloaded file disappeared before it could be scanned");
        newState = AVSCAN_FAILED;
      }
      else if (hr == E_INVALIDARG) {
        NS_WARNING("IAttachementExecute returned invalid argument error");
        newState = AVSCAN_FAILED;
      }
      else { 
        newState = AVSCAN_UGLY;
      }
    }
    else {
      newState = AVSCAN_FAILED;
    }
    return CheckAndSetState(newState, AVSCAN_SCANNING);
  }
  return false;
}
#pragma warning(default: 4509)

void
nsDownloadScanner::Scan::DoScan()
{
  CoInitialize(nullptr);

  if (DoScanAES()) {
    // We need to do a few more things on the main thread
    NS_DispatchToMainThread(this);
  } else {
    // We timed out, so just release
    ReleaseDispatcher* releaser = new ReleaseDispatcher(ToSupports(this));
    if(releaser) {
      NS_ADDREF(releaser);
      NS_DispatchToMainThread(releaser);
    }
  }

  MOZ_SEH_TRY {
    CoUninitialize();
  } MOZ_SEH_EXCEPT(ExceptionFilterFunction(GetExceptionCode())) {
    // Not much we can do at this point...
  }
}

HANDLE
nsDownloadScanner::Scan::GetWaitableThreadHandle() const
{
  HANDLE targetHandle = INVALID_HANDLE_VALUE;
  (void)DuplicateHandle(GetCurrentProcess(), mThread,
                        GetCurrentProcess(), &targetHandle,
                        SYNCHRONIZE, // Only allow clients to wait on this handle
                        FALSE, // cannot be inherited by child processes
                        0);
  return targetHandle;
}

bool
nsDownloadScanner::Scan::NotifyTimeout()
{
  bool didTimeout = CheckAndSetState(AVSCAN_TIMEDOUT, AVSCAN_SCANNING) ||
                      CheckAndSetState(AVSCAN_TIMEDOUT, AVSCAN_NOTSTARTED);
  if (didTimeout) {
    // We need to do a few more things on the main thread
    NS_DispatchToMainThread(this);
  }
  return didTimeout;
}

bool
nsDownloadScanner::Scan::CheckAndSetState(AVScanState newState, AVScanState expectedState) {
  bool gotExpectedState = false;
  EnterCriticalSection(&mStateSync);
  if((gotExpectedState = (mStatus == expectedState)))
    mStatus = newState;
  LeaveCriticalSection(&mStateSync);
  return gotExpectedState;
}

nsresult
nsDownloadScanner::ScanDownload(nsDownload *download)
{
  if (!mAESExists)
    return NS_ERROR_NOT_AVAILABLE;

  // No ref ptr, see comment below
  Scan *scan = new Scan(this, download);
  if (!scan)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(scan);

  nsresult rv = scan->Start();

  // Note that we only release upon error. On success, the scan is passed off
  // to a new thread. It is eventually released in Scan::Run on the main thread.
  if (NS_FAILED(rv))
    NS_RELEASE(scan);
  else
    // Notify the watchdog
    mWatchdog->Watch(scan);

  return rv;
}

nsDownloadScannerWatchdog::nsDownloadScannerWatchdog() 
  : mNewItemEvent(nullptr), mQuitEvent(nullptr) {
  InitializeCriticalSection(&mQueueSync);
}
nsDownloadScannerWatchdog::~nsDownloadScannerWatchdog() {
  DeleteCriticalSection(&mQueueSync);
}

nsresult
nsDownloadScannerWatchdog::Init() {
  // Both events are auto-reset
  mNewItemEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (INVALID_HANDLE_VALUE == mNewItemEvent)
    return NS_ERROR_OUT_OF_MEMORY;
  mQuitEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (INVALID_HANDLE_VALUE == mQuitEvent) {
    (void)CloseHandle(mNewItemEvent);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // This thread is always running, however it will be asleep
  // for most of the dlmgr's lifetime
  mThread = (HANDLE)_beginthreadex(nullptr, 0, WatchdogThread,
                                   this, 0, nullptr);
  if (!mThread) {
    (void)CloseHandle(mNewItemEvent);
    (void)CloseHandle(mQuitEvent);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult
nsDownloadScannerWatchdog::Shutdown() {
  // Tell the watchdog thread to quite
  (void)SetEvent(mQuitEvent);
  (void)WaitForSingleObject(mThread, INFINITE);
  (void)CloseHandle(mThread);
  // Manually clear and release the queued scans
  while (mScanQueue.GetSize() != 0) {
    Scan *scan = reinterpret_cast<Scan*>(mScanQueue.Pop());
    NS_RELEASE(scan);
  }
  (void)CloseHandle(mNewItemEvent);
  (void)CloseHandle(mQuitEvent);
  return NS_OK;
}

void
nsDownloadScannerWatchdog::Watch(Scan *scan) {
  bool wasEmpty;
  // Note that there is no release in this method
  // The scan will be released by the watchdog ALWAYS on the main thread
  // when either the watchdog thread processes the scan or the watchdog
  // is shut down
  NS_ADDREF(scan);
  EnterCriticalSection(&mQueueSync);
  wasEmpty = mScanQueue.GetSize()==0;
  mScanQueue.Push(scan);
  LeaveCriticalSection(&mQueueSync);
  // If the queue was empty, then the watchdog thread is/will be asleep
  if (wasEmpty)
    (void)SetEvent(mNewItemEvent);
}

unsigned int
__stdcall
nsDownloadScannerWatchdog::WatchdogThread(void *p) {
  NS_ASSERTION(!NS_IsMainThread(), "Antivirus scan watchdog should not be run on the main thread");
  nsDownloadScannerWatchdog *watchdog = (nsDownloadScannerWatchdog*)p;
  HANDLE waitHandles[3] = {watchdog->mNewItemEvent, watchdog->mQuitEvent, INVALID_HANDLE_VALUE};
  DWORD waitStatus;
  DWORD queueItemsLeft = 0;
  // Loop until quit event or error
  while (0 != queueItemsLeft ||
         ((WAIT_OBJECT_0 + 1) !=
           (waitStatus =
              WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE)) &&
         waitStatus != WAIT_FAILED)) {
    Scan *scan = nullptr;
    PRTime startTime, expectedEndTime, now;
    DWORD waitTime;

    // Pop scan from queue
    EnterCriticalSection(&watchdog->mQueueSync);
    scan = reinterpret_cast<Scan*>(watchdog->mScanQueue.Pop());
    queueItemsLeft = watchdog->mScanQueue.GetSize();
    LeaveCriticalSection(&watchdog->mQueueSync);

    // Calculate expected end time
    startTime = scan->GetStartTime();
    expectedEndTime = WATCHDOG_TIMEOUT + startTime;
    now = PR_Now();
    // PRTime is not guaranteed to be a signed integral type (afaik), but
    // currently it is
    if (now > expectedEndTime) {
      waitTime = 0;
    } else {
      // This is a positive value, and we know that it will not overflow
      // (bounded by WATCHDOG_TIMEOUT)
      // waitTime is in milliseconds, nspr uses microseconds
      waitTime = static_cast<DWORD>((expectedEndTime - now)/PR_USEC_PER_MSEC);
    }
    HANDLE hThread = waitHandles[2] = scan->GetWaitableThreadHandle();

    // Wait for the thread (obj 1) or quit event (obj 0)
    waitStatus = WaitForMultipleObjects(2, (waitHandles+1), FALSE, waitTime);
    CloseHandle(hThread);

    ReleaseDispatcher* releaser = new ReleaseDispatcher(ToSupports(scan));
    if(!releaser)
      continue;
    NS_ADDREF(releaser);
    // Got quit event or error
    if (waitStatus == WAIT_FAILED || waitStatus == WAIT_OBJECT_0) {
      NS_DispatchToMainThread(releaser);
      break;
    // Thread exited normally
    } else if (waitStatus == (WAIT_OBJECT_0+1)) {
      NS_DispatchToMainThread(releaser);
      continue;
    // Timeout case
    } else { 
      NS_ASSERTION(waitStatus == WAIT_TIMEOUT, "Unexpected wait status in dlmgr watchdog thread");
      if (!scan->NotifyTimeout()) {
        // If we didn't time out, then release the thread
        NS_DispatchToMainThread(releaser);
      } else {
        // NotifyTimeout did a dispatch which will release the scan, so we
        // don't need to release the scan
        NS_RELEASE(releaser);
      }
    }
  }
  _endthreadex(0);
  return 0;
}
