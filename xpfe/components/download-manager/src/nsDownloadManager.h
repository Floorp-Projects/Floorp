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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "nsIRDFDataSource.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventListener.h"
#include "nsIRDFContainerUtils.h"
#include "nsIWebProgressListener.h"
#include "nsIURI.h"
#include "nsIWebBrowserPersist.h"
#include "nsILocalFile.h"
#include "nsRefPtrHashtable.h"
#include "nsIRequest.h"
#include "nsIObserver.h"
#include "nsIStringBundle.h"
#include "nsIProgressDialog.h"
#include "nsIMIMEInfo.h"
#include "nsIAlertsService.h"
 
enum DownloadState { NOTSTARTED = -1, DOWNLOADING, FINISHED, FAILED, CANCELED };

class nsDownload;

class nsDownloadManager : public nsIDownloadManager,
                          public nsIDOMEventListener,
                          public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOWNLOADMANAGER
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIOBSERVER

  nsresult Init();

  nsDownloadManager();
  ~nsDownloadManager();

protected:
  nsresult GetDownloadsContainer(nsIRDFContainer** aResult);
  nsresult GetProfileDownloadsFileURL(nsCString& aDownloadsFileURL);
  nsresult GetInternalListener(nsIDownloadProgressListener** aInternalListener);
  nsresult GetDataSource(nsIRDFDataSource** aDataSource);
  nsresult AssertProgressInfo();
  nsresult AssertProgressInfoFor(const nsACString& aTargetPath);
  nsresult DownloadStarted(const nsACString& aTargetPath);
  nsresult DownloadEnded(const nsACString& aTargetPath, const PRUnichar* aMessage);
  PRBool MustUpdateUI() { if (mDocument) return PR_TRUE; return PR_FALSE; }

private:
  nsCOMPtr<nsIRDFDataSource> mDataSource;
  nsCOMPtr<nsIDOMDocument> mDocument;
  nsCOMPtr<nsIRDFContainer> mDownloadsContainer;
  nsCOMPtr<nsIDownloadProgressListener> mListener;
  nsCOMPtr<nsIRDFContainerUtils> mRDFContainerUtils;
  nsCOMPtr<nsIStringBundle> mBundle;
  PRInt32 mBatches;
  nsRefPtrHashtable<nsCStringHashKey, nsDownload> mCurrDownloads;

  friend class nsDownload;
};

class nsDownload : public nsIDownload,
                   public nsIWebProgressListener,
                   public nsIObserver
{
public:
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSITRANSFER
  NS_DECL_NSIDOWNLOAD
  NS_DECL_NSIOBSERVER
  NS_DECL_ISUPPORTS

  nsDownload(nsDownloadManager* aManager, nsIURI* aTarget, nsIURI* aSource);
  ~nsDownload();

  nsresult Suspend();
  nsresult Resume();
  void DisplayDownloadFinishedAlert();

  void SetDialogListener(nsIWebProgressListener* aInternalListener) {
    mDialogListener = aInternalListener;
  }
  void SetDialog(nsIProgressDialog* aDialog) {
    mDialog = aDialog;
  }
  // May return null
  nsIProgressDialog* GetDialog() {
    return mDialog;
  }
  void SetPersist(nsIWebBrowserPersist* aPersist) {
    mPersist = aPersist;
  }

  struct TransferInformation {
    PRInt32 mCurrBytes, mMaxBytes;
    TransferInformation(PRInt32 aCurr, PRInt32 aMax) :
      mCurrBytes(aCurr),
      mMaxBytes(aMax)
      {}
  };

  TransferInformation GetTransferInformation() {
    return TransferInformation(mCurrBytes, mMaxBytes);
  }
  DownloadState GetDownloadState() {
    return mDownloadState;
  }
  void SetDownloadState(DownloadState aState) {
    mDownloadState = aState;
  }
  void SetMIMEInfo(nsIMIMEInfo* aMIMEInfo) {
    mMIMEInfo = aMIMEInfo;
  }
  void SetStartTime(PRInt64 aStartTime) {
    mStartTime = aStartTime;
  }
private:
  nsDownloadManager* mDownloadManager;

  nsString mDisplayName;

  nsCOMPtr<nsIURI> mTarget;
  nsCOMPtr<nsIURI> mSource;
  nsCOMPtr<nsIWebProgressListener> mListener;
  nsCOMPtr<nsIWebProgressListener> mDialogListener;
  nsCOMPtr<nsIWebBrowserPersist> mPersist;
  nsCOMPtr<nsIRequest> mRequest;
  nsCOMPtr<nsIProgressDialog> mDialog;
  nsCOMPtr<nsIObserver> mObserver;
  nsCOMPtr<nsIMIMEInfo> mMIMEInfo;
  DownloadState mDownloadState;

  PRInt32 mPercentComplete;
  PRInt32 mCurrBytes;
  PRInt32 mMaxBytes;
  PRTime mStartTime;
  PRTime mLastUpdate;
};

#endif
