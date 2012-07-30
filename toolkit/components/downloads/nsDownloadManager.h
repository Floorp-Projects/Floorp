/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef downloadmanager___h___
#define downloadmanager___h___

#if defined(XP_WIN)
#define DOWNLOAD_SCANNER
#endif

#include "nsIDownload.h"
#include "nsIDownloadManager.h"
#include "nsIDownloadProgressListener.h"
#include "nsIFile.h"
#include "nsIMIMEInfo.h"
#include "nsINavHistoryService.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIStringBundle.h"
#include "nsISupportsPrimitives.h"
#include "nsWeakReference.h"
#include "nsITimer.h"
#include "nsString.h"

#include "mozStorageHelper.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"

typedef PRInt16 DownloadState;
typedef PRInt16 DownloadType;

class nsDownload;

#ifdef DOWNLOAD_SCANNER
#include "nsDownloadScanner.h"
#endif

class nsDownloadManager : public nsIDownloadManager,
                          public nsINavHistoryObserver,
                          public nsIObserver,
                          public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOWNLOADMANAGER
  NS_DECL_NSINAVHISTORYOBSERVER
  NS_DECL_NSIOBSERVER

  nsresult Init();

  static nsDownloadManager *GetSingleton();

  virtual ~nsDownloadManager();
  nsDownloadManager() :
      mDBType(DATABASE_DISK)
    , mInPrivateBrowsing(false)
#ifdef DOWNLOAD_SCANNER
    , mScanner(nullptr)
#endif
  {
  }

protected:
  enum DatabaseType
  {
    DATABASE_DISK = 0, // default
    DATABASE_MEMORY
  };

  nsresult InitDB();
  nsresult InitFileDB();
  void CloseDB();
  nsresult InitMemoryDB();
  already_AddRefed<mozIStorageConnection> GetFileDBConnection(nsIFile *dbFile) const;
  already_AddRefed<mozIStorageConnection> GetMemoryDBConnection() const;
  nsresult SwitchDatabaseTypeTo(enum DatabaseType aType);
  nsresult CreateTable();

  /**
   * Fix up the database after a crash such as dealing with previously-active
   * downloads. Call this before RestoreActiveDownloads to get the downloads
   * fixed here to be auto-resumed.
   */
  nsresult RestoreDatabaseState();

  /**
   * Paused downloads that survive across sessions are considered active, so
   * rebuild the list of these downloads.
   */
  nsresult RestoreActiveDownloads();

  nsresult GetDownloadFromDB(PRUint32 aID, nsDownload **retVal);

  /**
   * Specially track the active downloads so that we don't need to check
   * every download to see if they're in progress.
   */
  nsresult AddToCurrentDownloads(nsDownload *aDl);

  void SendEvent(nsDownload *aDownload, const char *aTopic);

  /**
   * Adds a download with the specified information to the DB.
   *
   * @return The id of the download, or 0 if there was an error.
   */
  PRInt64 AddDownloadToDB(const nsAString &aName,
                          const nsACString &aSource,
                          const nsACString &aTarget,
                          const nsAString &aTempPath,
                          PRInt64 aStartTime,
                          PRInt64 aEndTime,
                          const nsACString &aMimeType,
                          const nsACString &aPreferredApp,
                          nsHandlerInfoAction aPreferredAction);

  void NotifyListenersOnDownloadStateChange(PRInt16 aOldState,
                                            nsIDownload *aDownload);
  void NotifyListenersOnProgressChange(nsIWebProgress *aProgress,
                                       nsIRequest *aRequest,
                                       PRInt64 aCurSelfProgress,
                                       PRInt64 aMaxSelfProgress,
                                       PRInt64 aCurTotalProgress,
                                       PRInt64 aMaxTotalProgress,
                                       nsIDownload *aDownload);
  void NotifyListenersOnStateChange(nsIWebProgress *aProgress,
                                    nsIRequest *aRequest,
                                    PRUint32 aStateFlags,
                                    nsresult aStatus,
                                    nsIDownload *aDownload);

  nsDownload *FindDownload(PRUint32 aID);

  /**
   * First try to resume the download, and if that fails, retry it.
   *
   * @param aDl The download to resume and/or retry.
   */
  nsresult ResumeRetry(nsDownload *aDl);

  /**
   * Pause all active downloads and remember if they should try to auto-resume
   * when the download manager starts again.
   *
   * @param aSetResume Indicate if the downloads that get paused should be set
   *                   as auto-resume.
   */
  nsresult PauseAllDownloads(bool aSetResume);

  /**
   * Resume all paused downloads unless we're only supposed to do the automatic
   * ones; in that case, try to retry them as well if resuming doesn't work.
   *
   * @param aResumeAll If true, all downloads will be resumed; otherwise, only
   *                   those that are marked as auto-resume will resume.
   */
  nsresult ResumeAllDownloads(bool aResumeAll);

  /**
   * Stop tracking the active downloads. Only use this when we're about to quit
   * the download manager because we destroy our list of active downloads to
   * break the dlmgr<->dl cycle. Active downloads that aren't real-paused will
   * be canceled.
   */
  nsresult RemoveAllDownloads();

  /**
   * Find all downloads from a source URI and delete them.
   *
   * @param aURI
   *        The source URI to remove downloads
   */
  nsresult RemoveDownloadsForURI(nsIURI *aURI);

  /**
   * Callback used for resuming downloads after getting a wake notification.
   *
   * @param aTimer
   *        Timer object fired after some delay after a wake notification
   * @param aClosure
   *        nsDownloadManager object used to resume downloads
   */
  static void ResumeOnWakeCallback(nsITimer *aTimer, void *aClosure);
  nsCOMPtr<nsITimer> mResumeOnWakeTimer;

  void ConfirmCancelDownloads(PRInt32 aCount,
                              nsISupportsPRBool *aCancelDownloads,
                              const PRUnichar *aTitle,
                              const PRUnichar *aCancelMessageMultiple,
                              const PRUnichar *aCancelMessageSingle,
                              const PRUnichar *aDontCancelButton);

  PRInt32 GetRetentionBehavior();

  /**
   * Type to indicate possible behaviors for active downloads across sessions.
   *
   * Possible values are:
   *  QUIT_AND_RESUME  - downloads should be auto-resumed
   *  QUIT_AND_PAUSE   - downloads should be paused
   *  QUIT_AND_CANCEL  - downloads should be cancelled
   */
  enum QuitBehavior {
    QUIT_AND_RESUME = 0, 
    QUIT_AND_PAUSE = 1, 
    QUIT_AND_CANCEL = 2
  };

  /**
   * Indicates user-set behavior for active downloads across sessions,
   *
   * @return value of user-set pref for active download behavior
   */
  enum QuitBehavior GetQuitBehavior();

  void OnEnterPrivateBrowsingMode();
  void OnLeavePrivateBrowsingMode();

  // Virus scanner for windows
#ifdef DOWNLOAD_SCANNER
private:
  nsDownloadScanner* mScanner;
#endif

private:
  nsCOMArray<nsIDownloadProgressListener> mListeners;
  nsCOMPtr<nsIStringBundle> mBundle;
  nsCOMPtr<mozIStorageConnection> mDBConn;
  nsCOMArray<nsDownload> mCurrentDownloads;
  nsCOMPtr<nsIObserverService> mObserverService;
  nsCOMPtr<mozIStorageStatement> mUpdateDownloadStatement;
  nsCOMPtr<mozIStorageStatement> mGetIdsForURIStatement;
  nsAutoPtr<mozStorageTransaction> mHistoryTransaction;

  enum DatabaseType mDBType;
  bool mInPrivateBrowsing;

  static nsDownloadManager *gDownloadManagerService;

  friend class nsDownload;
};

class nsDownload : public nsIDownload
{
public:
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSIWEBPROGRESSLISTENER2
  NS_DECL_NSITRANSFER
  NS_DECL_NSIDOWNLOAD
  NS_DECL_ISUPPORTS

  nsDownload();
  virtual ~nsDownload();

  /**
   * This method MUST be called when changing states on a download.  It will
   * notify the download listener when a change happens.  This also updates the
   * database, by calling UpdateDB().
   */
  nsresult SetState(DownloadState aState);

protected:
  /**
   * Finish up the download by breaking reference cycles and clearing unneeded
   * data. Additionally, the download removes itself from the download
   * manager's list of current downloads.
   *
   * NOTE: This method removes the cycle created when starting the download, so
   * make sure to use kungFuDeathGrip if you want to access member variables.
   */
  void Finalize();

  /**
   * For finished resumed downloads that came in from exthandler, perform the
   * action that would have been done if the download wasn't resumed.
   */
  nsresult ExecuteDesiredAction();

  /**
   * Move the temporary file to the final destination by removing the existing
   * dummy target and renaming the temporary.
   */
  nsresult MoveTempToTarget();

  /**
   * Update the start time which also implies the last update time is the same.
   */
  void SetStartTime(PRInt64 aStartTime);

  /**
   * Update the amount of bytes transferred and max bytes; and recalculate the
   * download percent.
   */
  void SetProgressBytes(PRInt64 aCurrBytes, PRInt64 aMaxBytes);

  /**
   * Pause the download, but in certain cases it might get fake-paused instead
   * of real-paused.
   */
  nsresult Pause();

  /**
   * All this does is cancel the connection that the download is using. It does
   * not remove it from the download manager.
   */
  nsresult Cancel();

  /**
   * Resume the download.
   */
  nsresult Resume();

  /**
   * Download is not transferring?
   */
  bool IsPaused();

  /**
   * Download can continue from the middle of a transfer?
   */
  bool IsResumable();

  /**
   * Download was resumed?
   */
  bool WasResumed();

  /**
   * Indicates if the download should try to automatically resume or not.
   */
  bool ShouldAutoResume();

  /**
   * Download is in a state to stop and complete the download?
   */
  bool IsFinishable();

  /**
   * Download is totally done transferring and all?
   */
  bool IsFinished();

  /**
   * Update the DB with the current state of the download including time,
   * download state and other values not known when first creating the
   * download DB entry.
   */
  nsresult UpdateDB();

  /**
   * Fail a download because of a failure status and prompt the provided
   * message or use a generic download failure message if nullptr.
   */
  nsresult FailDownload(nsresult aStatus, const PRUnichar *aMessage);

  /**
   * Opens the downloaded file with the appropriate application, which is
   * either the OS default, MIME type default, or the one selected by the user.
   *
   * This also adds the temporary file to the "To be deleted on Exit" list, if
   * the corresponding user preference is set (except on OS X).
   *
   * This function was adopted from nsExternalAppHandler::OpenWithApplication
   * (uriloader/exthandler/nsExternalHelperAppService.cpp).
   */
  nsresult OpenWithApplication();

  nsDownloadManager *mDownloadManager;
  nsCOMPtr<nsIURI> mTarget;

private:
  nsString mDisplayName;
  nsCString mEntityID;

  nsCOMPtr<nsIURI> mSource;
  nsCOMPtr<nsIURI> mReferrer;
  nsCOMPtr<nsICancelable> mCancelable;
  nsCOMPtr<nsIRequest> mRequest;
  nsCOMPtr<nsIFile> mTempFile;
  nsCOMPtr<nsIMIMEInfo> mMIMEInfo;

  DownloadState mDownloadState;
  DownloadType mDownloadType;

  PRUint32 mID;
  PRInt32 mPercentComplete;

  /**
   * These bytes are based on the position of where the request started, so 0
   * doesn't necessarily mean we have nothing. Use GetAmountTransferred and
   * GetSize for the real transferred amount and size.
   */
  PRInt64 mCurrBytes;
  PRInt64 mMaxBytes;

  PRTime mStartTime;
  PRTime mLastUpdate;
  PRInt64 mResumedAt;
  double mSpeed;

  bool mHasMultipleFiles;

  /**
   * Track various states of the download trying to auto-resume when starting
   * the download manager or restoring from a crash.
   *
   * DONT_RESUME: Don't automatically resume the download
   * AUTO_RESUME: Automaically resume the download
   */
  enum AutoResume { DONT_RESUME, AUTO_RESUME };
  AutoResume mAutoResume;

  friend class nsDownloadManager;
};

#endif
