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
#include "nsIXPInstallManagerUI.h"
#include "nsIDownloadProgressListener.h"
#include "nsIDownload.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventListener.h"
#include "nsIRDFContainerUtils.h"
#include "nsIWebProgressListener.h"
#include "nsIWebProgressListener2.h"
#include "nsIXPIProgressDialog.h"
#include "nsIURI.h"
#include "nsIWebBrowserPersist.h"
#include "nsILocalFile.h"
#include "nsHashtable.h"
#include "nsIRequest.h"
#include "nsIObserver.h"
#include "nsIStringBundle.h"
#include "nsISupportsPrimitives.h"
#include "nsIProgressDialog.h"
#include "nsIMIMEInfo.h"
#include "nsITimer.h"
#include "nsIAlertsService.h"
#include "nsCycleCollectionParticipant.h"

typedef PRInt16 DownloadState;
typedef PRInt16 DownloadType;

class nsXPIProgressListener;
class nsDownload;

class nsDownloadManager : public nsIDownloadManager,
                          public nsIXPInstallManagerUI,
                          public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOWNLOADMANAGER
  NS_DECL_NSIXPINSTALLMANAGERUI
  NS_DECL_NSIOBSERVER

  nsresult Init();

  nsDownloadManager();
  virtual ~nsDownloadManager();

  static PRInt32 PR_CALLBACK CancelAllDownloads(nsHashKey* aKey, void* aData, void* aClosure);
  static PRInt32 PR_CALLBACK BuildActiveDownloadsList(nsHashKey* aKey, void* aData, void* aClosure);
  nsresult DownloadEnded(const PRUnichar* aPersistentDescriptor, const PRUnichar* aMessage); 

public:
  nsresult AssertProgressInfoFor(const PRUnichar* aPersistentDescriptor);

protected:
  nsresult GetDownloadsContainer(nsIRDFContainer** aResult);
  nsresult GetProfileDownloadsFileURL(nsCString& aDownloadsFileURL);
  nsresult GetDataSource(nsIRDFDataSource** aDataSource);
  nsresult DownloadStarted(const PRUnichar* aPersistentDescriptor);
  nsresult GetInternalListener(nsIDownloadProgressListener** aListener);
  nsresult PauseResumeDownload(const PRUnichar* aPath, PRBool aPause);
  nsresult RemoveDownload(nsIRDFResource* aDownload);
  nsresult ValidateDownloadsContainer();

  void     ConfirmCancelDownloads(PRInt32 aCount, nsISupportsPRBool* aCancelDownloads,
                                  const PRUnichar* aTitle, 
                                  const PRUnichar* aCancelMessageMultiple, 
                                  const PRUnichar* aCancelMessageSingle,
                                  const PRUnichar* aDontCancelButton);

  static void     OpenTimerCallback(nsITimer* aTimer, void* aClosure);
  static nsresult OpenDownloadManager(PRBool aShouldFocus, PRInt32 aFlashCount, nsIDownload* aDownload, nsIDOMWindow* aParent);

  PRBool   NeedsUIUpdate() { return mListener != nsnull; }
  PRInt32  GetRetentionBehavior();

  static PRBool IsInFinalStage(DownloadState aState)
  {
    return aState == nsIDownloadManager::DOWNLOAD_NOTSTARTED ||
           aState == nsIDownloadManager::DOWNLOAD_DOWNLOADING ||
           aState == nsIXPInstallManagerUI::INSTALL_INSTALLING;
  };

  static PRBool IsInProgress(DownloadState aState) 
  {
    return aState == nsIDownloadManager::DOWNLOAD_NOTSTARTED || 
           aState == nsIDownloadManager::DOWNLOAD_DOWNLOADING || 
           aState == nsIDownloadManager::DOWNLOAD_PAUSED || 
           aState == nsIXPInstallManagerUI::INSTALL_DOWNLOADING ||
           aState == nsIXPInstallManagerUI::INSTALL_INSTALLING;
  };

  static PRBool CompletedSuccessfully(DownloadState aState)
  {
    return aState == nsIDownloadManager::DOWNLOAD_FINISHED || 
           aState == nsIXPInstallManagerUI::INSTALL_FINISHED;
  };

private:
  nsCOMPtr<nsIDownloadProgressListener> mListener;
  nsCOMPtr<nsIRDFDataSource> mDataSource;
  nsCOMPtr<nsIXPIProgressDialog> mXPIProgress;
  nsCOMPtr<nsIRDFContainer> mDownloadsContainer;
  nsCOMPtr<nsIRDFContainerUtils> mRDFContainerUtils;
  nsCOMPtr<nsIStringBundle> mBundle;
  nsCOMPtr<nsITimer> mDMOpenTimer;
  PRInt32 mBatches;
  nsHashtable mCurrDownloads;
  
  friend class nsDownload;
};

class nsXPIProgressListener : public nsIXPIProgressDialog
{
public:
  NS_DECL_NSIXPIPROGRESSDIALOG
  NS_DECL_ISUPPORTS

  nsXPIProgressListener() { };
  nsXPIProgressListener(nsDownloadManager* aManager);
  virtual ~nsXPIProgressListener();

  void AddDownload(nsIDownload* aDownload);

  PRBool HasActiveXPIOperations();

protected:
  void RemoveDownloadAtIndex(PRUint32 aIndex);

  inline void AssertProgressInfoForDownload(nsDownload* aDownload);

private:
  nsDownloadManager* mDownloadManager;
  nsCOMPtr<nsISupportsArray> mDownloads;
};

class nsDownloadsDataSource : public nsIRDFDataSource, 
                              public nsIRDFRemoteDataSource
{
public:
  NS_DECL_NSIRDFDATASOURCE
  NS_DECL_NSIRDFREMOTEDATASOURCE
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDownloadsDataSource,
                                           nsIRDFDataSource)

  nsDownloadsDataSource() { };
  virtual ~nsDownloadsDataSource() { };

  nsresult LoadDataSource();

private:
  nsCOMPtr<nsIRDFDataSource> mInner;
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
  DownloadState GetDownloadState();
  void SetDownloadState(DownloadState aState);
  DownloadType GetDownloadType();
  void SetDownloadType(DownloadType aType);

protected:
  nsresult SetDownloadManager(nsDownloadManager* aDownloadManager);
  nsresult SetDialogListener(nsIWebProgressListener* aInternalListener);
  nsresult GetDialogListener(nsIWebProgressListener** aInternalListener);
  nsresult SetDialog(nsIProgressDialog* aDialog);
  nsresult GetDialog(nsIProgressDialog** aDialog);
  nsresult SetTempFile(nsILocalFile* aTempFile);
  nsresult GetTempFile(nsILocalFile** aTempFile);
  nsresult SetCancelable(nsICancelable* aCancelable);
  nsresult SetTarget(nsIURI* aTarget);
  nsresult SetDisplayName(const PRUnichar* aDisplayName);
  nsresult SetSource(nsIURI* aSource);
  nsresult SetMIMEInfo(nsIMIMEInfo* aMIMEInfo);
  nsresult SetStartTime(PRInt64 aStartTime);

  void Pause(PRBool aPaused);
  PRBool IsPaused();

  struct TransferInformation {
    PRInt64 mCurrBytes, mMaxBytes;
    TransferInformation(PRInt64 aCurr, PRInt64 aMax) :
      mCurrBytes(aCurr),
      mMaxBytes(aMax)
      {}
  };
  TransferInformation GetTransferInformation();

  nsDownloadManager* mDownloadManager;
  nsCOMPtr<nsIURI> mTarget;

private:
  nsString mDisplayName;

  nsCOMPtr<nsIURI> mSource;
  nsCOMPtr<nsIWebProgressListener> mDialogListener;
  nsCOMPtr<nsICancelable> mCancelable;
  nsCOMPtr<nsIRequest> mRequest;
  nsCOMPtr<nsIProgressDialog> mDialog;
  nsCOMPtr<nsILocalFile> mTempFile;
  nsCOMPtr<nsIMIMEInfo> mMIMEInfo;
  
  DownloadState mDownloadState;
  DownloadType  mDownloadType;

  PRBool  mPaused;
  PRInt32 mPercentComplete;
  PRUint64 mCurrBytes;
  PRUint64 mMaxBytes;
  PRTime mStartTime;
  PRTime mLastUpdate;
  double mSpeed;

  friend class nsDownloadManager;
};

#endif
