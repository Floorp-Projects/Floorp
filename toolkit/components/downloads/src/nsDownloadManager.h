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
  nsresult RestoreDatabaseState();
  nsresult GetDownloadFromDB(PRUint32 aID, nsDownload **retVal);

  inline nsresult AddToCurrentDownloads(nsDownload *aDl)
  {
    if (!mCurrentDownloads.AppendObject(aDl))
      return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
  }

  void SendEvent(nsDownload *aDownload, const char *aTopic);


  /**
   * Adds a download with the specified information to the DB.
   *
   * @return The id of the download, or 0 if there was an error.
   */
  PRInt64 AddDownloadToDB(const nsAString &aName,
                          const nsACString &aSource,
                          const nsACString &aTarget,
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
  nsresult PauseResumeDownload(PRUint32 aID, PRBool aPause);
  nsresult CancelAllDownloads();

  /**
   * Removes download from "current downloads". 
   *
   * This method removes the cycle created when starting the download, so 
   * make sure to use kungFuDeathGrip if you want to access member variables
   */
  void CompleteDownload(nsDownload *aDownload);

  void     ConfirmCancelDownloads(PRInt32 aCount,
                                  nsISupportsPRBool* aCancelDownloads,
                                  const PRUnichar* aTitle, 
                                  const PRUnichar* aCancelMessageMultiple, 
                                  const PRUnichar* aCancelMessageSingle,
                                  const PRUnichar* aDontCancelButton);

  PRInt32  GetRetentionBehavior();
  nsresult ExecuteDesiredAction(nsDownload *aDownload);

  static PRBool IsInFinalStage(DownloadState aState)
  {
    return aState == nsIDownloadManager::DOWNLOAD_NOTSTARTED ||
           aState == nsIDownloadManager::DOWNLOAD_QUEUED ||
           aState == nsIDownloadManager::DOWNLOAD_DOWNLOADING;
  }

  static PRBool IsInProgress(DownloadState aState) 
  {
    return aState == nsIDownloadManager::DOWNLOAD_NOTSTARTED || 
           aState == nsIDownloadManager::DOWNLOAD_QUEUED ||
           aState == nsIDownloadManager::DOWNLOAD_DOWNLOADING || 
           aState == nsIDownloadManager::DOWNLOAD_PAUSED;
  }

  static PRBool CompletedSuccessfully(DownloadState aState)
  {
    return aState == nsIDownloadManager::DOWNLOAD_FINISHED;
  }

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

public:
  /**
   * This method MUST be called when changing states on a download.  It will
   * notify the download listener when a change happens.  This also updates the
   * database, by calling UpdateDB().
   */
  nsresult SetState(DownloadState aState);

  DownloadType GetDownloadType();
  void SetDownloadType(DownloadType aType);

  nsresult UpdateDB();

protected:
  void SetStartTime(PRInt64 aStartTime);

  nsresult PauseResume(PRBool aPause);

  nsDownloadManager* mDownloadManager;
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
  DownloadType  mDownloadType;

  PRUint32 mID;
  PRInt32 mPercentComplete;
  PRUint64 mCurrBytes;
  PRUint64 mMaxBytes;
  PRTime mStartTime;
  PRTime mLastUpdate;
  PRBool mPaused;
  PRBool mWasResumed;
  double mSpeed;

  friend class nsDownloadManager;
};

#endif
