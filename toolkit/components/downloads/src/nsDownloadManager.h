/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Blake Ross <blaker@netscape.com>
 *   Ben Goodger <ben@netscape.com>
 *   Shawn Wilsher <me@shawnwilsher.com>
 *   Srirang G Doddihal <brahmana@doddihal.com>
 *   Edward Lee <edward.lee@engineering.uiuc.edu>
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

#ifndef downloadmanager___h___
#define downloadmanager___h___

#include "nsIDownloadManager.h"
#include "nsIDownloadProgressListener.h"
#include "nsIDownload.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventListener.h"
#include "nsIWebProgressListener.h"
#include "nsIWebProgressListener2.h"
#include "nsIURI.h"
#include "nsIWebBrowserPersist.h"
#include "nsILocalFile.h"
#include "nsIRequest.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "nsIStringBundle.h"
#include "nsISupportsPrimitives.h"
#include "nsIMIMEInfo.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "nsISupportsArray.h"
#include "nsCOMArray.h"
#include "nsArrayEnumerator.h"
#include "nsAutoPtr.h"
#include "nsIObserverService.h"

typedef PRInt16 DownloadState;
typedef PRInt16 DownloadType;

class nsDownload;

#ifdef XP_WIN
class nsDownloadScanner;
#endif

class nsDownloadManager : public nsIDownloadManager,
                          public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOWNLOADMANAGER
  NS_DECL_NSIOBSERVER

  nsresult Init();

  static nsDownloadManager *GetSingleton();

  virtual ~nsDownloadManager();
#ifdef XP_WIN
  nsDownloadManager() : mScanner(nsnull) { };
private:
  nsDownloadScanner *mScanner;
#endif

protected:
  nsresult InitDB(PRBool *aDoImport);
  nsresult CreateTable();
  nsresult ImportDownloadHistory();

  /**
   * Fix up the database after a crash such as dealing with previously-active
   * downloads.
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
                          PRInt32 aState);

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
   * Stop tracking the active downloads. Only use this when we're about to quit
   * the download manager because we destroy our list of active downloads to
   * break the dlmgr<->dl cycle. Active downloads that aren't real-paused will
   * be canceled.
   */
  nsresult RemoveAllDownloads();

  void ConfirmCancelDownloads(PRInt32 aCount,
                              nsISupportsPRBool *aCancelDownloads,
                              const PRUnichar *aTitle,
                              const PRUnichar *aCancelMessageMultiple,
                              const PRUnichar *aCancelMessageSingle,
                              const PRUnichar *aDontCancelButton);

  PRInt32 GetRetentionBehavior();

private:
  nsCOMArray<nsIDownloadProgressListener> mListeners;
  nsCOMPtr<nsIStringBundle> mBundle;
  nsCOMPtr<mozIStorageConnection> mDBConn;
  nsCOMArray<nsDownload> mCurrentDownloads;
  nsCOMPtr<nsIObserverService> mObserverService;
  nsCOMPtr<mozIStorageStatement> mUpdateDownloadStatement;

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
   * Resume the download. Works for both real-paused and fake-paused.
   */
  nsresult Resume();

  /**
   * Resume the real-paused download. Let Resume decide if this should get used.
   */
  nsresult RealResume();

  /**
   * Download is not transferring?
   */
  PRBool IsPaused();

  /**
   * Download can continue from the middle of a transfer?
   */
  PRBool IsResumable();

  /**
   * Download was resumed?
   */
  PRBool WasResumed();

  /**
   * Download is real-paused? (not fake-paused by stalling the channel)
   */
  PRBool IsRealPaused();

  /**
   * Download is in a state to stop and complete the download?
   */
  PRBool IsFinishable();

  /**
   * Download is totally done transferring and all?
   */
  PRBool IsFinished();

  /**
   * Update the DB with the current state of the download including time,
   * download state and other values not known when first creating the
   * download DB entry.
   */
  nsresult UpdateDB();

  /**
   * Fail a download because of a failure status and prompt the provided
   * message or use a generic download failure message if nsnull.
   */
  nsresult FailDownload(nsresult aStatus, const PRUnichar *aMessage);

  nsDownloadManager *mDownloadManager;
  nsCOMPtr<nsIURI> mTarget;

private:
  nsString mDisplayName;
  nsCString mEntityID;

  nsCOMPtr<nsIURI> mSource;
  nsCOMPtr<nsIURI> mReferrer;
  nsCOMPtr<nsICancelable> mCancelable;
  nsCOMPtr<nsIRequest> mRequest;
  nsCOMPtr<nsILocalFile> mTempFile;
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

  friend class nsDownloadManager;
};

#endif
