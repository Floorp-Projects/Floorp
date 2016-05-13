/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file defines two implementations of the nsIBackgroundFileSaver
 * interface.  See the "test_backgroundfilesaver.js" file for usage examples.
 */

#ifndef BackgroundFileSaver_h__
#define BackgroundFileSaver_h__

#include "mozilla/Mutex.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsNSSShutDown.h"
#include "nsIAsyncOutputStream.h"
#include "nsIBackgroundFileSaver.h"
#include "nsIStreamListener.h"
#include "nsStreamUtils.h"
#include "ScopedNSSTypes.h"

class nsIAsyncInputStream;
class nsIThread;
class nsIX509CertList;

namespace mozilla {
namespace net {

class DigestOutputStream;

////////////////////////////////////////////////////////////////////////////////
//// BackgroundFileSaver

class BackgroundFileSaver : public nsIBackgroundFileSaver,
                            public nsNSSShutDownObject
{
public:
  NS_DECL_NSIBACKGROUNDFILESAVER

  BackgroundFileSaver();

  /**
   * Initializes the pipe and the worker thread on XPCOM construction.
   *
   * This is called automatically by the XPCOM infrastructure, and if this
   * fails, the factory will delete this object without returning a reference.
   */
  nsresult Init();

  /**
   * Used by nsNSSShutDownList to manage nsNSSShutDownObjects.
   */
  void virtualDestroyNSSReference() override;

  /**
   * Number of worker threads that are currently running.
   */
  static uint32_t sThreadCount;

  /**
   * Maximum number of worker threads reached during the current download session,
   * used for telemetry.
   *
   * When there are no more worker threads running, we consider the download
   * session finished, and this counter is reset.
   */
  static uint32_t sTelemetryMaxThreadCount;


protected:
  virtual ~BackgroundFileSaver();

  /**
   * Helper function for managing NSS objects (mDigestContext).
   */
  void destructorSafeDestroyNSSReference();

  /**
   * Thread that constructed this object.
   */
  nsCOMPtr<nsIThread> mControlThread;

  /**
   * Thread to which the actual input/output is delegated.
   */
  nsCOMPtr<nsIThread> mWorkerThread;

  /**
   * Stream that receives data from derived classes.  The received data will be
   * available to the worker thread through mPipeInputStream. This is an
   * instance of nsPipeOutputStream, not BackgroundFileSaverOutputStream.
   */
  nsCOMPtr<nsIAsyncOutputStream> mPipeOutputStream;

  /**
   * Used during initialization, determines if the pipe is created with an
   * infinite buffer.  An infinite buffer is required if the derived class
   * implements nsIStreamListener, because this interface requires all the
   * provided data to be consumed synchronously.
   */
  virtual bool HasInfiniteBuffer() = 0;

  /**
   * Used by derived classes if they need to be called back while copying.
   */
  virtual nsAsyncCopyProgressFun GetProgressCallback() = 0;

  /**
   * Stream used by the worker thread to read the data to be saved.
   */
  nsCOMPtr<nsIAsyncInputStream> mPipeInputStream;

private:
  friend class NotifyTargetChangeRunnable;

  /**
   * Matches the nsIBackgroundFileSaver::observer property.
   *
   * @remarks This is a strong reference so that JavaScript callers don't need
   *          to worry about keeping another reference to the observer.
   */
  nsCOMPtr<nsIBackgroundFileSaverObserver> mObserver;

  //////////////////////////////////////////////////////////////////////////////
  //// Shared state between control and worker threads

  /**
   * Protects the shared state between control and worker threads.  This mutex
   * is always locked for a very short time, never during input/output.
   */
  mozilla::Mutex mLock;

  /**
   * True if the worker thread is already waiting to process a change in state.
   */
  bool mWorkerThreadAttentionRequested;

  /**
   * True if the operation should finish as soon as possibile.
   */
  bool mFinishRequested;

  /**
   * True if the operation completed, with either success or failure.
   */
  bool mComplete;

  /**
   * Holds the current file saver status.  This is a success status while the
   * object is working correctly, and remains such if the operation completes
   * successfully.  This becomes an error status when an error occurs on the
   * worker thread, or when the operation is canceled.
   */
  nsresult mStatus;

  /**
   * True if we should append data to the initial target file, instead of
   * overwriting it.
   */
  bool mAppend;

  /**
   * This is set by the first SetTarget call on the control thread, and contains
   * the target file name that will be used by the worker thread, as soon as it
   * is possible to update mActualTarget and open the file.  This is null if no
   * target was ever assigned to this object.
   */
  nsCOMPtr<nsIFile> mInitialTarget;

  /**
   * This is set by the first SetTarget call on the control thread, and
   * indicates whether mInitialTarget should be kept as partially completed,
   * rather than deleted, if the operation fails or is canceled.
   */
  bool mInitialTargetKeepPartial;

  /**
   * This is set by subsequent SetTarget calls on the control thread, and
   * contains the new target file name to which the worker thread will move the
   * target file, as soon as it can be done.  This is null if SetTarget was
   * called only once, or no target was ever assigned to this object.
   *
   * The target file can be renamed multiple times, though only the most recent
   * rename is guaranteed to be processed by the worker thread.
   */
  nsCOMPtr<nsIFile> mRenamedTarget;

  /**
   * This is set by subsequent SetTarget calls on the control thread, and
   * indicates whether mRenamedTarget should be kept as partially completed,
   * rather than deleted, if the operation fails or is canceled.
   */
  bool mRenamedTargetKeepPartial;

  /**
   * While NS_AsyncCopy is in progress, allows canceling it.  Null otherwise.
   * This is read by both threads but only written by the worker thread.
   */
  nsCOMPtr<nsISupports> mAsyncCopyContext;

  /**
   * The SHA 256 hash in raw bytes of the downloaded file. This is written
   * by the worker thread but can be read on the main thread.
   */
  nsCString mSha256;

  /**
   * Whether or not to compute the hash. Must be set on the main thread before
   * setTarget is called.
   */
  bool mSha256Enabled;

  /**
   * Store the signature info.
   */
  nsCOMArray<nsIX509CertList> mSignatureInfo;

  /**
   * Whether or not to extract the signature. Must be set on the main thread
   * before setTarget is called.
   */
  bool mSignatureInfoEnabled;

  //////////////////////////////////////////////////////////////////////////////
  //// State handled exclusively by the worker thread

  /**
   * Current target file associated to the input and output streams.
   */
  nsCOMPtr<nsIFile> mActualTarget;

  /**
   * Indicates whether mActualTarget should be kept as partially completed,
   * rather than deleted, if the operation fails or is canceled.
   */
  bool mActualTargetKeepPartial;

  /**
   * Used to calculate the file hash. This keeps state across file renames and
   * is lazily initialized in ProcessStateChange.
   */
  UniquePK11Context mDigestContext;

  //////////////////////////////////////////////////////////////////////////////
  //// Private methods

  /**
   * Called when NS_AsyncCopy completes.
   *
   * @param aClosure
   *        Populated with a raw pointer to the BackgroundFileSaver object.
   * @param aStatus
   *        Success or failure status specified when the copy was interrupted.
   */
  static void AsyncCopyCallback(void *aClosure, nsresult aStatus);

  /**
   * Called on the control thread after state changes, to ensure that the worker
   * thread will process the state change appropriately.
   *
   * @param aShouldInterruptCopy
   *        If true, the current NS_AsyncCopy, if any, is canceled.
   */
  nsresult GetWorkerThreadAttention(bool aShouldInterruptCopy);

  /**
   * Event called on the worker thread to begin processing a state change.
   */
  nsresult ProcessAttention();

  /**
   * Called by ProcessAttention to execute the operations corresponding to the
   * state change.  If this results in an error, ProcessAttention will force the
   * entire operation to be aborted.
   */
  nsresult ProcessStateChange();

  /**
   * Returns true if completion conditions are met on the worker thread.  The
   * first time this happens, posts the completion event to the control thread.
   */
  bool CheckCompletion();

  /**
   * Event called on the control thread to indicate that file contents will now
   * be saved to the specified file.
   */
  nsresult NotifyTargetChange(nsIFile *aTarget);

  /**
   * Event called on the control thread to send the final notification.
   */
  nsresult NotifySaveComplete();

  /**
   * Verifies the signature of the binary at the specified file path and stores
   * the signature data in mSignatureInfo. We extract only X.509 certificates,
   * since that is what Google's Safebrowsing protocol specifies.
   */
  nsresult ExtractSignatureInfo(const nsAString& filePath);
};

////////////////////////////////////////////////////////////////////////////////
//// BackgroundFileSaverOutputStream

class BackgroundFileSaverOutputStream : public BackgroundFileSaver
                                      , public nsIAsyncOutputStream
                                      , public nsIOutputStreamCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
  NS_DECL_NSIASYNCOUTPUTSTREAM
  NS_DECL_NSIOUTPUTSTREAMCALLBACK

  BackgroundFileSaverOutputStream();

protected:
  virtual bool HasInfiniteBuffer() override;
  virtual nsAsyncCopyProgressFun GetProgressCallback() override;

private:
  ~BackgroundFileSaverOutputStream();

  /**
   * Original callback provided to our AsyncWait wrapper.
   */
  nsCOMPtr<nsIOutputStreamCallback> mAsyncWaitCallback;
};

////////////////////////////////////////////////////////////////////////////////
//// BackgroundFileSaverStreamListener. This class is instantiated by
// nsExternalHelperAppService, DownloadCore.jsm, and possibly others.

class BackgroundFileSaverStreamListener final : public BackgroundFileSaver
                                              , public nsIStreamListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  BackgroundFileSaverStreamListener();

protected:
  virtual bool HasInfiniteBuffer() override;
  virtual nsAsyncCopyProgressFun GetProgressCallback() override;

private:
  ~BackgroundFileSaverStreamListener();

  /**
   * Protects the state related to whether the request should be suspended.
   */
  mozilla::Mutex mSuspensionLock;

  /**
   * Whether we should suspend the request because we received too much data.
   */
  bool mReceivedTooMuchData;

  /**
   * Request for which we received too much data.  This is populated when
   * mReceivedTooMuchData becomes true for the first time.
   */
  nsCOMPtr<nsIRequest> mRequest;

  /**
   * Whether mRequest is currently suspended.
   */
  bool mRequestSuspended;

  /**
   * Called while NS_AsyncCopy is copying data.
   */
  static void AsyncCopyProgressCallback(void *aClosure, uint32_t aCount);

  /**
   * Called on the control thread to suspend or resume the request.
   */
  nsresult NotifySuspendOrResume();
};

// A wrapper around nsIOutputStream, so that we can compute hashes on the
// stream without copying and without polluting pristine NSS code with XPCOM
// interfaces.
class DigestOutputStream : public nsNSSShutDownObject,
                           public nsIOutputStream
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
  // Constructor. Neither parameter may be null. The caller owns both.
  DigestOutputStream(nsIOutputStream* outputStream, PK11Context* aContext);

  // We don't own any NSS objects here, so no need to clean up
  void virtualDestroyNSSReference() override { }

private:
  ~DigestOutputStream();

  // Calls to write are passed to this stream.
  nsCOMPtr<nsIOutputStream> mOutputStream;
  // Digest context used to compute the hash, owned by the caller.
  PK11Context* mDigestContext;

  // Don't accidentally copy construct.
  DigestOutputStream(const DigestOutputStream& d);
};

} // namespace net
} // namespace mozilla

#endif
