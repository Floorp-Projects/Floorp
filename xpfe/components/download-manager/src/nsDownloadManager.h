/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Blake Ross <blakeross@telocity.com>
 *   Ben Goodger <ben@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
#ifndef downloadmanager___h___
#define downloadmanager___h___

#include "nsIDownloadManager.h"
#include "nsIDownloadProgressListener.h"
#include "nsIDownloadItem.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventListener.h"
#include "nsIRDFContainerUtils.h"
#include "nsIWebProgressListener.h"
#include "nsIURI.h"
#include "nsILocalFile.h"
#include "nsHashtable.h"
#include "nsIWebBrowserPersist.h"
#include "nsIRequest.h"
#include "nsIObserver.h"
 
class nsDownloadManager : public nsIDownloadManager,
                          public nsIRDFDataSource,
                          public nsIRDFRemoteDataSource,
                          public nsIDOMEventListener
{
public:
  NS_DECL_NSIRDFDATASOURCE
  NS_DECL_NSIRDFREMOTEDATASOURCE
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOWNLOADMANAGER
  NS_DECL_NSIDOMEVENTLISTENER

  nsresult Init();

  nsDownloadManager();
  virtual ~nsDownloadManager();

protected:
  nsresult GetDownloadsContainer(nsIRDFContainer** aResult);
  nsresult GetProfileDownloadsFileURL(char** aDownloadsFileURL);
  nsresult GetInternalListener(nsIDownloadProgressListener** aInternalListener);
  nsresult AssertProgressInfo();
  nsresult DownloadFinished(const char* aTargetPath);
  PRBool MustUpdateUI() { if (mDocument) return PR_TRUE; return PR_FALSE; }

private:
  nsCOMPtr<nsIRDFDataSource> mInner;
  nsCOMPtr<nsIDOMDocument> mDocument;
  nsCOMPtr<nsIDownloadProgressListener> mListener;
  nsCOMPtr<nsIRDFContainerUtils> mRDFContainerUtils;
  nsHashtable* mCurrDownloadItems;

  friend class DownloadItem;
};

class DownloadItem : public nsIDownloadItem
{
public:
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSIDOWNLOADITEM
  NS_DECL_ISUPPORTS

  DownloadItem();
  virtual ~DownloadItem();

protected:
  nsresult UpdateProgressInfo();
  nsresult SetDownloadManager(nsDownloadManager* aDownloadManager);
  nsresult SetDialogListener(nsIWebProgressListener* aInternalListener);
  nsresult GetDialogListener(nsIWebProgressListener** aInternalListener);

private:
  nsDownloadManager* mDownloadManager;

  nsString mPrettyName;
  nsCOMPtr<nsILocalFile> mTarget;
  nsCOMPtr<nsIURI> mSource;
  nsCOMPtr<nsIWebProgressListener> mListener;
  nsCOMPtr<nsIWebProgressListener> mDialogListener;
  nsCOMPtr<nsIWebBrowserPersist> mPersist;
  nsCOMPtr<nsIRequest> mRequest;
  nsCOMPtr<nsIObserver> mObserver;

  PRInt32 mPercentComplete;
  PRInt64 mStartTime;

  friend class nsDownloadManager;
};
#endif