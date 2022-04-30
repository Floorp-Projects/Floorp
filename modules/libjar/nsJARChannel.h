/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJARChannel_h__
#define nsJARChannel_h__

#include "mozilla/net/MemoryDownloader.h"
#include "nsIJARChannel.h"
#include "nsIJARURI.h"
#include "nsIEventTarget.h"
#include "nsIInputStreamPump.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsIStreamListener.h"
#include "nsIZipReader.h"
#include "nsILoadGroup.h"
#include "nsILoadInfo.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsHashPropertyBag.h"
#include "nsIFile.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "mozilla/Atomics.h"
#include "mozilla/Logging.h"

class nsJARInputThunk;
class nsJARProtocolHandler;
class nsInputStreamPump;

//-----------------------------------------------------------------------------

class nsJARChannel final : public nsIJARChannel,
                           public nsIStreamListener,
                           public nsIThreadRetargetableRequest,
                           public nsIThreadRetargetableStreamListener,
                           public nsHashPropertyBag {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL
  NS_DECL_NSIJARCHANNEL
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITHREADRETARGETABLEREQUEST
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

  nsJARChannel();

  nsresult Init(nsIURI* uri);

  void SetFile(nsIFile* file);

 private:
  virtual ~nsJARChannel();

  nsresult CreateJarInput(nsIZipReaderCache*, nsJARInputThunk**);
  nsresult LookupFile();
  nsresult OpenLocalFile();
  nsresult ContinueOpenLocalFile(nsJARInputThunk* aInput, bool aIsSyncCall);
  nsresult OnOpenLocalFileComplete(nsresult aResult, bool aIsSyncCall);
  nsresult CheckPendingEvents();
  void NotifyError(nsresult aError);
  void FireOnProgress(uint64_t aProgress);

  // Returns false if we don't know the content type of this channel, in which
  // case we should use the content-type hint.
  bool GetContentTypeGuess(nsACString&) const;
  void SetOpened();

  nsCString mSpec;

  bool mOpened;
  mozilla::Atomic<bool, mozilla::ReleaseAcquire> mCanceled;
  bool mOnDataCalled = false;

  RefPtr<nsJARProtocolHandler> mJarHandler;
  nsCOMPtr<nsIJARURI> mJarURI;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsCOMPtr<nsISupports> mOwner;
  nsCOMPtr<nsILoadInfo> mLoadInfo;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsISupports> mSecurityInfo;
  nsCOMPtr<nsIProgressEventSink> mProgressSink;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsCOMPtr<nsIStreamListener> mListener;
  nsCString mContentType;
  nsCString mContentCharset;
  int64_t mContentLength;
  uint32_t mLoadFlags;
  mozilla::Atomic<nsresult, mozilla::ReleaseAcquire> mStatus;
  bool mIsPending;  // the AsyncOpen is in progress.

  bool mEnableOMT;
  // |Cancel()|, |Suspend()|, and |Resume()| might be called during AsyncOpen.
  struct {
    bool isCanceled;
    mozilla::Atomic<uint32_t> suspendCount;
  } mPendingEvent;

  nsCOMPtr<nsIInputStreamPump> mPump;
  // mRequest is only non-null during OnStartRequest, so we'll have a pointer
  // to the request if we get called back via RetargetDeliveryTo.
  nsCOMPtr<nsIRequest> mRequest;
  nsCOMPtr<nsIFile> mJarFile;
  nsCOMPtr<nsIFile> mJarFileOverride;
  nsCOMPtr<nsIZipReader> mPreCachedJarReader;
  nsCOMPtr<nsIURI> mJarBaseURI;
  nsCString mJarEntry;
  nsCString mInnerJarEntry;

  // use StreamTransportService as background thread
  nsCOMPtr<nsIEventTarget> mWorker;
};

#endif  // nsJARChannel_h__
