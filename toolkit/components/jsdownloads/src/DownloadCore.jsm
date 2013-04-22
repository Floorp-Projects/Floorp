/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file includes the following constructors and global objects:
 *
 * Download
 * Represents a single download, with associated state and actions.  This object
 * is transient, though it can be included in a DownloadList so that it can be
 * managed by the user interface and persisted across sessions.
 *
 * DownloadSource
 * Represents the source of a download, for example a document or an URI.
 *
 * DownloadTarget
 * Represents the target of a download, for example a file in the global
 * downloads directory, or a file in the system temporary directory.
 *
 * DownloadError
 * Provides detailed information about a download failure.
 *
 * DownloadSaver
 * Template for an object that actually transfers the data for the download.
 *
 * DownloadCopySaver
 * Saver object that simply copies the entire source file to the target.
 *
 * DownloadLegacySaver
 * Saver object that integrates with the legacy nsITransfer interface.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "Download",
  "DownloadSource",
  "DownloadTarget",
  "DownloadError",
  "DownloadSaver",
  "DownloadCopySaver",
  "DownloadLegacySaver",
];

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm")
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/commonjs/sdk/core/promise.js");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

const BackgroundFileSaverStreamListener = Components.Constructor(
      "@mozilla.org/network/background-file-saver;1?mode=streamlistener",
      "nsIBackgroundFileSaver");

////////////////////////////////////////////////////////////////////////////////
//// Download

/**
 * Represents a single download, with associated state and actions.  This object
 * is transient, though it can be included in a DownloadList so that it can be
 * managed by the user interface and persisted across sessions.
 */
function Download()
{
  this._deferSucceeded = Promise.defer();
}

Download.prototype = {
  /**
   * DownloadSource object associated with this download.
   */
  source: null,

  /**
   * DownloadTarget object associated with this download.
   */
  target: null,

  /**
   * DownloadSaver object associated with this download.
   */
  saver: null,

  /**
   * Indicates that the download never started, has been completed successfully,
   * failed, or has been canceled.  This property becomes false when a download
   * is started for the first time, or when a failed or canceled download is
   * restarted.
   */
  stopped: true,

  /**
   * Indicates that the download has been completed successfully.
   */
  succeeded: false,

  /**
   * Indicates that the download has been canceled.  This property can become
   * true, then it can be reset to false when a canceled download is restarted.
   *
   * This property becomes true as soon as the "cancel" method is called, though
   * the "stopped" property might remain false until the cancellation request
   * has been processed.  Temporary files or part files may still exist even if
   * they are expected to be deleted, until the "stopped" property becomes true.
   */
  canceled: false,

  /**
   * When the download fails, this is set to a DownloadError instance indicating
   * the cause of the failure.  If the download has been completed successfully
   * or has been canceled, this property is null.  This property is reset to
   * null when a failed download is restarted.
   */
  error: null,

  /**
   * Indicates whether this download's "progress" property is able to report
   * partial progress while the download proceeds, and whether the value in
   * totalBytes is relevant.  This depends on the saver and the download source.
   */
  hasProgress: false,

  /**
   * Progress percent, from 0 to 100.  Intermediate values are reported only if
   * hasProgress is true.
   *
   * @note You shouldn't rely on this property being equal to 100 to determine
   *       whether the download is completed.  You should use the individual
   *       state properties instead.
   */
  progress: 0,

  /**
   * When hasProgress is true, indicates the total number of bytes to be
   * transferred before the download finishes, that can be zero for empty files.
   *
   * When hasProgress is false, this property is always zero.
   */
  totalBytes: 0,

  /**
   * Number of bytes currently transferred.  This value starts at zero, and may
   * be updated regardless of the value of hasProgress.
   *
   * @note You shouldn't rely on this property being equal to totalBytes to
   *       determine whether the download is completed.  You should use the
   *       individual state properties instead.
   */
  currentBytes: 0,

  /**
   * This can be set to a function that is called after other properties change.
   */
  onchange: null,

  /**
   * Raises the onchange notification.
   */
  _notifyChange: function D_notifyChange() {
    try {
      if (this.onchange) {
        this.onchange();
      }
    } catch (ex) {
      Cu.reportError(ex);
    }
  },

  /**
   * The download may be stopped and restarted multiple times before it
   * completes successfully. This may happen if any of the download attempts is
   * canceled or fails.
   *
   * This property contains a promise that is linked to the current attempt, or
   * null if the download is either stopped or in the process of being canceled.
   * If the download restarts, this property is replaced with a new promise.
   *
   * The promise is resolved if the attempt it represents finishes successfully,
   * and rejected if the attempt fails.
   */
  _currentAttempt: null,

  /**
   * Starts the download for the first time, or restarts a download that failed
   * or has been canceled.
   *
   * Calling this method when the download has been completed successfully has
   * no effect, and the method returns a resolved promise.  If the download is
   * in progress, the method returns the same promise as the previous call.
   *
   * If the "cancel" method was called but the cancellation process has not
   * finished yet, this method waits for the cancellation to finish, then
   * restarts the download immediately.
   *
   * @note If you need to start a new download from the same source, rather than
   *       restarting a failed or canceled one, you should create a separate
   *       Download object with the same source as the current one.
   *
   * @return {Promise}
   * @resolves When the download has finished successfully.
   * @rejects JavaScript exception if the download failed.
   */
  start: function D_start()
  {
    // If the download succeeded, it's the final state, we have nothing to do.
    if (this.succeeded) {
      return Promise.resolve();
    }

    // If the download already started and hasn't failed or hasn't been
    // canceled, return the same promise as the previous call, allowing the
    // caller to wait for the current attempt to finish.
    if (this._currentAttempt) {
      return this._currentAttempt;
    }

    // Initialize all the status properties for a new or restarted download.
    this.stopped = false;
    this.canceled = false;
    this.error = null;
    this.hasProgress = false;
    this.progress = 0;
    this.totalBytes = 0;
    this.currentBytes = 0;

    // Create a new deferred object and an associated promise before starting
    // the actual download.  We store it on the download as the current attempt.
    let deferAttempt = Promise.defer();
    let currentAttempt = deferAttempt.promise;
    this._currentAttempt = currentAttempt;

    // This function propagates progress from the DownloadSaver object, unless
    // it comes in late from a download attempt that was replaced by a new one.
    function DS_setProgressBytes(aCurrentBytes, aTotalBytes)
    {
      if (this._currentAttempt == currentAttempt || !this._currentAttempt) {
        this._setBytes(aCurrentBytes, aTotalBytes);
      }
    }

    // Now that we stored the promise in the download object, we can start the
    // task that will actually execute the download.
    deferAttempt.resolve(Task.spawn(function task_D_start() {
      // Wait upon any pending cancellation request.
      if (this._promiseCanceled) {
        yield this._promiseCanceled;
      }

      try {
        // Execute the actual download through the saver object.
        yield this.saver.execute(DS_setProgressBytes.bind(this));

        // Update the status properties for a successful download.
        this.progress = 100;
        this.succeeded = true;
      } catch (ex) {
        // Fail with a generic status code on cancellation, so that the caller
        // is forced to actually check the status properties to see if the
        // download was canceled or failed because of other reasons.
        if (this._promiseCanceled) {
          throw new DownloadError(Cr.NS_ERROR_FAILURE, "Download canceled.");
        }

        // Update the download error, unless a new attempt already started. The
        // change in the status property is notified in the finally block.
        if (this._currentAttempt == currentAttempt || !this._currentAttempt) {
          this.error = ex;
        }
        throw ex;
      } finally {
        // Any cancellation request has now been processed.
        this._promiseCanceled = null;

        // Update the status properties, unless a new attempt already started.
        if (this._currentAttempt == currentAttempt || !this._currentAttempt) {
          this._currentAttempt = null;
          this.stopped = true;
          this._notifyChange();
          if (this.succeeded) {
            this._deferSucceeded.resolve();
          }
        }
      }
    }.bind(this)));

    // Notify the new download state before returning.
    this._notifyChange();
    return this._currentAttempt;
  },

  /**
   * When a request to cancel the download is received, contains a promise that
   * will be resolved when the cancellation request is processed.  When the
   * request is processed, this property becomes null again.
   */
  _promiseCanceled: null,

  /**
   * Cancels the download.
   *
   * The cancellation request is asynchronous.  Until the cancellation process
   * finishes, temporary files or part files may still exist even if they are
   * expected to be deleted.
   *
   * In case the download completes successfully before the cancellation request
   * could be processed, this method has no effect, and it returns a resolved
   * promise.  You should check the properties of the download at the time the
   * returned promise is resolved to determine if the download was cancelled.
   *
   * Calling this method when the download has been completed successfully,
   * failed, or has been canceled has no effect, and the method returns a
   * resolved promise.  This behavior is designed for the case where the call
   * to "cancel" happens asynchronously, and is consistent with the case where
   * the cancellation request could not be processed in time.
   *
   * @return {Promise}
   * @resolves When the cancellation process has finished.
   * @rejects Never.
   */
  cancel: function D_cancel()
  {
    // If the download is currently stopped, we have nothing to do.
    if (this.stopped) {
      return Promise.resolve();
    }

    if (!this._promiseCanceled) {
      // Start a new cancellation request.
      let deferCanceled = Promise.defer();
      this._currentAttempt.then(function () deferCanceled.resolve(),
                                function () deferCanceled.resolve());
      this._promiseCanceled = deferCanceled.promise;

      // The download can already be restarted.
      this._currentAttempt = null;

      // Notify that the cancellation request was received.
      this.canceled = true;
      this._notifyChange();

      // Execute the actual cancellation through the saver object.
      this.saver.cancel();
    }

    return this._promiseCanceled;
  },

  /**
   * This deferred object contains a promise that is resolved as soon as this
   * download finishes successfully, and is never rejected.  This property is
   * initialized when the download is created, and never changes.
   */
  _deferSucceeded: null,

  /**
   * Returns a promise that is resolved as soon as this download finishes
   * successfully, even if the download was stopped and restarted meanwhile.
   *
   * You can use this property for scheduling download completion actions in the
   * current session, for downloads that are controlled interactively.  If the
   * download is not controlled interactively, you should use the promise
   * returned by the "start" method instead, to check for success or failure.
   *
   * @return {Promise}
   * @resolves When the download has finished successfully.
   * @rejects Never.
   */
  whenSucceeded: function D_whenSucceeded()
  {
    return this._deferSucceeded.promise;
  },

  /**
   * Updates progress notifications based on the number of bytes transferred.
   *
   * @param aCurrentBytes
   *        Number of bytes transferred until now.
   * @param aTotalBytes
   *        Total number of bytes to be transferred, or -1 if unknown.
   */
  _setBytes: function D_setBytes(aCurrentBytes, aTotalBytes) {
    this.currentBytes = aCurrentBytes;
    if (aTotalBytes != -1) {
      this.hasProgress = true;
      this.totalBytes = aTotalBytes;
      if (aTotalBytes > 0) {
        this.progress = Math.floor(aCurrentBytes / aTotalBytes * 100);
      }
    }
    this._notifyChange();
  },
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadSource

/**
 * Represents the source of a download, for example a document or an URI.
 */
function DownloadSource() { }

DownloadSource.prototype = {
  /**
   * The nsIURI for the download source.
   */
  uri: null,
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadTarget

/**
 * Represents the target of a download, for example a file in the global
 * downloads directory, or a file in the system temporary directory.
 */
function DownloadTarget() { }

DownloadTarget.prototype = {
  /**
   * The nsIFile for the download target.
   */
  file: null,
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadError

/**
 * Provides detailed information about a download failure.
 *
 * @param aResult
 *        The result code associated with the error.
 * @param aMessage
 *        The message to be displayed, or null to use the message associated
 *        with the result code.
 * @param aInferCause
 *        If true, attempts to determine if the cause of the download is a
 *        network failure or a local file failure, based on a set of known
 *        values of the result code.  This is useful when the error is received
 *        by a component that handles both aspects of the download.
 */
function DownloadError(aResult, aMessage, aInferCause)
{
  const NS_ERROR_MODULE_BASE_OFFSET = 0x45;
  const NS_ERROR_MODULE_NETWORK = 6;
  const NS_ERROR_MODULE_FILES = 13;

  // Set the error name used by the Error object prototype first.
  this.name = "DownloadError";
  this.result = aResult || Cr.NS_ERROR_FAILURE;
  if (aMessage) {
    this.message = aMessage;
  } else {
    let exception = new Components.Exception(this.result);
    this.message = exception.toString();
  }
  if (aInferCause) {
    let module = ((aResult & 0x7FFF0000) >> 16) - NS_ERROR_MODULE_BASE_OFFSET;
    this.becauseSourceFailed = (module == NS_ERROR_MODULE_NETWORK);
    this.becauseTargetFailed = (module == NS_ERROR_MODULE_FILES);
  }
}

DownloadError.prototype = {
  __proto__: Error.prototype,

  /**
   * The result code associated with this error.
   */
  result: false,

  /**
   * Indicates an error occurred while reading from the remote location.
   */
  becauseSourceFailed: false,

  /**
   * Indicates an error occurred while writing to the local target.
   */
  becauseTargetFailed: false,
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadSaver

/**
 * Template for an object that actually transfers the data for the download.
 */
function DownloadSaver() { }

DownloadSaver.prototype = {
  /**
   * Download object for raising notifications and reading properties.
   */
  download: null,

  /**
   * Executes the download.
   *
   * @param aSetProgressBytesFn
   *        This function may be called by the saver to report progress. It
   *        takes two arguments: the first is the number of bytes transferred
   *        until now, the second is the total number of bytes to be
   *        transferred, or -1 if unknown.
   *
   * @return {Promise}
   * @resolves When the download has finished successfully.
   * @rejects JavaScript exception if the download failed.
   */
  execute: function DS_execute(aSetProgressBytesFn)
  {
    throw new Error("Not implemented.");
  },

  /**
   * Cancels the download.
   */
  cancel: function DS_cancel()
  {
    throw new Error("Not implemented.");
  },
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadCopySaver

/**
 * Saver object that simply copies the entire source file to the target.
 */
function DownloadCopySaver() { }

DownloadCopySaver.prototype = {
  __proto__: DownloadSaver.prototype,

  /**
   * BackgroundFileSaver object currently handling the download.
   */
  _backgroundFileSaver: null,

  /**
   * Implements "DownloadSaver.execute".
   */
  execute: function DCS_execute(aSetProgressBytesFn)
  {
    let deferred = Promise.defer();
    let download = this.download;

    // Create the object that will save the file in a background thread.
    let backgroundFileSaver = new BackgroundFileSaverStreamListener();
    try {
      // When the operation completes, reflect the status in the promise
      // returned by this download execution function.
      backgroundFileSaver.observer = {
        onTargetChange: function () { },
        onSaveComplete: function DCSE_onSaveComplete(aSaver, aStatus)
        {
          // Free the reference cycle, in order to release resources earlier.
          backgroundFileSaver.observer = null;
          this._backgroundFileSaver = null;

          // Send notifications now that we can restart the download if needed.
          if (Components.isSuccessCode(aStatus)) {
            deferred.resolve();
          } else {
            // Infer the origin of the error from the failure code, because
            // BackgroundFileSaver does not provide more specific data.
            deferred.reject(new DownloadError(aStatus, null, true));
          }
        },
      };

      // Set the target file, that will be deleted if the download fails.
      backgroundFileSaver.setTarget(download.target.file, false);

      // Create a channel from the source, and listen to progress notifications.
      // TODO: Handle downloads initiated from private browsing windows.
      let channel = NetUtil.newChannel(download.source.uri);
      channel.notificationCallbacks = {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsIInterfaceRequestor]),
        getInterface: XPCOMUtils.generateQI([Ci.nsIProgressEventSink]),
        onProgress: function DCSE_onProgress(aRequest, aContext, aProgress,
                                             aProgressMax)
        {
          aSetProgressBytesFn(aProgress, aProgressMax);
        },
        onStatus: function () { },
      };

      // Open the channel, directing output to the background file saver.
      backgroundFileSaver.QueryInterface(Ci.nsIStreamListener);
      channel.asyncOpen({
        onStartRequest: function DCSE_onStartRequest(aRequest, aContext)
        {
          backgroundFileSaver.onStartRequest(aRequest, aContext);

          // Ensure we report the value of "Content-Length", if available, even
          // if the download doesn't generate any progress events later.
          if (aRequest instanceof Ci.nsIChannel &&
              aRequest.contentLength >= 0) {
            aSetProgressBytesFn(0, aRequest.contentLength);
          }
        },
        onStopRequest: function DCSE_onStopRequest(aRequest, aContext,
                                                   aStatusCode)
        {
          try {
            backgroundFileSaver.onStopRequest(aRequest, aContext, aStatusCode);
          } finally {
            // If the data transfer completed successfully, indicate to the
            // background file saver that the operation can finish.  If the
            // data transfer failed, the saver has been already stopped.
            if (Components.isSuccessCode(aStatusCode)) {
              backgroundFileSaver.finish(Cr.NS_OK);
            }
          }
        },
        onDataAvailable: function DCSE_onDataAvailable(aRequest, aContext,
                                                       aInputStream, aOffset,
                                                       aCount)
        {
          backgroundFileSaver.onDataAvailable(aRequest, aContext, aInputStream,
                                              aOffset, aCount);
        },
      }, null);

      // If the operation succeeded, store the object to allow cancellation.
      this._backgroundFileSaver = backgroundFileSaver;
    } catch (ex) {
      // In case an error occurs while setting up the chain of objects for the
      // download, ensure that we release the resources of the background saver.
      deferred.reject(ex);
      backgroundFileSaver.finish(Cr.NS_ERROR_FAILURE);
    }
    return deferred.promise;
  },

  /**
   * Implements "DownloadSaver.cancel".
   */
  cancel: function DCS_cancel()
  {
    if (this._backgroundFileSaver) {
      this._backgroundFileSaver.finish(Cr.NS_ERROR_FAILURE);
      this._backgroundFileSaver = null;
    }
  },
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadLegacySaver

/**
 * Saver object that integrates with the legacy nsITransfer interface.
 *
 * For more background on the process, see the DownloadLegacyTransfer object.
 */
function DownloadLegacySaver()
{
  this.deferExecuted = Promise.defer();
  this.deferCanceled = Promise.defer();
}

DownloadLegacySaver.prototype = {
  __proto__: DownloadSaver.prototype,

  /**
   * nsIRequest object associated to the status and progress updates we
   * received.  This object is null before we receive the first status and
   * progress update, and is also reset to null when the download is stopped.
   */
  request: null,

  /**
   * This deferred object contains a promise that is resolved as soon as this
   * download finishes successfully, and is rejected in case the download is
   * canceled or receives a failure notification through nsITransfer.
   */
  deferExecuted: null,

  /**
   * This deferred object contains a promise that is resolved if the download
   * receives a cancellation request through the "cancel" method, and is never
   * rejected.  The nsITransfer implementation will register a handler that
   * actually causes the download cancellation.
   */
  deferCanceled: null,

  /**
   * This is populated with the value of the aSetProgressBytesFn argument of the
   * "execute" method, and is null before the method is called.
   */
  setProgressBytesFn: null,

  /**
   * Called by the nsITransfer implementation while the download progresses.
   *
   * @param aCurrentBytes
   *        Number of bytes transferred until now.
   * @param aTotalBytes
   *        Total number of bytes to be transferred, or -1 if unknown.
   */
  onProgressBytes: function DLS_onProgressBytes(aCurrentBytes, aTotalBytes)
  {
    // Ignore progress notifications until we are ready to process them.
    if (!this.setProgressBytesFn) {
      return;
    }

    this.progressWasNotified = true;
    this.setProgressBytesFn(aCurrentBytes, aTotalBytes);
  },

  /**
   * Whether the onProgressBytes function has been called at least once.
   */
  progressWasNotified: false,

  /**
   * Called by the nsITransfer implementation when the request has finished.
   *
   * @param aRequest
   *        nsIRequest associated to the status update.
   * @param aStatus
   *        Status code received by the nsITransfer implementation.
   */
  onTransferFinished: function DLS_onTransferFinished(aRequest, aStatus)
  {
    // Store a reference to the request, used when handling completion.
    this.request = aRequest;

    if (Components.isSuccessCode(aStatus)) {
      this.deferExecuted.resolve();
    } else {
      // Infer the origin of the error from the failure code, because more
      // specific data is not available through the nsITransfer implementation.
      this.deferExecuted.reject(new DownloadError(aStatus, null, true));
    }
  },

  /**
   * Implements "DownloadSaver.execute".
   */
  execute: function DLS_execute(aSetProgressBytesFn)
  {
    this.setProgressBytesFn = aSetProgressBytesFn;

    return Task.spawn(function task_DLS_execute() {
      try {
        // Wait for the component that executes the download to finish.
        yield this.deferExecuted.promise;

        // At this point, the "request" property has been populated.  Ensure we
        // report the value of "Content-Length", if available, even if the
        // download didn't generate any progress events.
        if (!this.progressWasNotified &&
            this.request instanceof Ci.nsIChannel &&
            this.request.contentLength >= 0) {
          aSetProgressBytesFn(0, this.request.contentLength);
        }

        // The download implementation may not have created the target file if
        // no data was received from the source.  In this case, ensure that an
        // empty file is created as expected.
        try {
          // This atomic operation is more efficient than an existence check.
          let file = yield OS.File.open(this.download.target.file.path,
                                        { create: true });
          yield file.close();
        } catch (ex if ex instanceof OS.File.Error && ex.becauseExists) { }
      } finally {
        // We don't need the reference to the request anymore.
        this.request = null;
      }
    }.bind(this));
  },

  /**
   * Implements "DownloadSaver.cancel".
   */
  cancel: function DLS_cancel()
  {
    // Synchronously cancel the operation as soon as the object is connected.
    this.deferCanceled.resolve();

    // We don't necessarily receive status notifications after we call "cancel",
    // but cancellation through nsICancelable should be synchronous, thus force
    // the rejection of the execution promise immediately.
    this.deferExecuted.reject(new DownloadError(Cr.NS_ERROR_FAILURE,
                                                "Download canceled."));
  },
};
