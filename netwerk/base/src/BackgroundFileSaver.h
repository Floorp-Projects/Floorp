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
#include "nsCOMPtr.h"
#include "nsIAsyncOutputStream.h"
#include "nsIBackgroundFileSaver.h"
#include "nsIStreamListener.h"
#include "nsStreamUtils.h"

class nsIAsyncInputStream;
class nsIThread;

namespace mozilla {
namespace net {

////////////////////////////////////////////////////////////////////////////////
//// BackgroundFileSaver

class BackgroundFileSaver : public nsIBackgroundFileSaver
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

protected:
  virtual ~BackgroundFileSaver();

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
   * available to the worker thread through mPipeInputStream.
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
   * Set by the control thread to the target file name that will be used by the
   * worker thread, as soon as it is possible to update mActualTarget and open
   * the file.  This is null if no target was ever assigned to this object.
   */
  nsCOMPtr<nsIFile> mAssignedTarget;

  /**
   * Indicates whether mAssignedTarget should be kept as partially completed,
   * rather than deleted, if the operation fails or is canceled.
   */
  bool mAssignedTargetKeepPartial;

  /**
   * While NS_AsyncCopy is in progress, allows canceling it.  Null otherwise.
   * This is read by both threads but only written by the worker thread.
   */
  nsCOMPtr<nsISupports> mAsyncCopyContext;

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
};

////////////////////////////////////////////////////////////////////////////////
//// BackgroundFileSaverOutputStream

class BackgroundFileSaverOutputStream : public BackgroundFileSaver
                                      , public nsIAsyncOutputStream
                                      , public nsIOutputStreamCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
  NS_DECL_NSIASYNCOUTPUTSTREAM
  NS_DECL_NSIOUTPUTSTREAMCALLBACK

  BackgroundFileSaverOutputStream();

protected:
  virtual bool HasInfiniteBuffer() MOZ_OVERRIDE;
  virtual nsAsyncCopyProgressFun GetProgressCallback() MOZ_OVERRIDE;

private:
  ~BackgroundFileSaverOutputStream();

  /**
   * Original callback provided to our AsyncWait wrapper.
   */
  nsCOMPtr<nsIOutputStreamCallback> mAsyncWaitCallback;
};

////////////////////////////////////////////////////////////////////////////////
//// BackgroundFileSaverStreamListener

class BackgroundFileSaverStreamListener : public BackgroundFileSaver
                                        , public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  BackgroundFileSaverStreamListener();

protected:
  virtual bool HasInfiniteBuffer() MOZ_OVERRIDE;
  virtual nsAsyncCopyProgressFun GetProgressCallback() MOZ_OVERRIDE;

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

} // namespace net
} // namespace mozilla

#endif
