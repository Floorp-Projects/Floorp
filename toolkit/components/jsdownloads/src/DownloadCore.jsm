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

XPCOMUtils.defineLazyModuleGetter(this, "DownloadIntegration",
                                  "resource://gre/modules/DownloadIntegration.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
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

/**
 * Returns true if the given value is a primitive string or a String object.
 */
function isString(aValue) {
  // We cannot use the "instanceof" operator reliably across module boundaries.
  return (typeof aValue == "string") ||
         (typeof aValue == "object" && "charAt" in aValue);
}

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
   * Indicates the start time of the download.  When the download starts,
   * this property is set to a valid Date object.  The default value is null
   * before the download starts.
   */
  startTime: null,

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
   * Indicates whether, at this time, there is any partially downloaded data
   * that can be used when restarting a failed or canceled download.
   *
   * This property is relevant while the download is in progress, and also if it
   * failed or has been canceled.  If the download has been completed
   * successfully, this property is not relevant anymore.
   *
   * Whether partial data can actually be retained depends on the saver and the
   * download source, and may not be known before the download is started.
   */
  hasPartialData: false,

  /**
   * This can be set to a function that is called after other properties change.
   */
  onchange: null,

  /**
   * This tells if the user has chosen to open/run the downloaded file after
   * download has completed.
   */
  launchWhenSucceeded: false,

  /**
   * This represents the MIME type of the download.
   */
  contentType: null,

  /**
   * This indicates the path of the application to be used to launch the file,
   * or null if the file should be launched with the default application.
   */
  launcherPath: null,

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

    // While shutting down or disposing of this object, we prevent the download
    // from returning to be in progress.
    if (this._finalized) {
      return Promise.reject(new DownloadError(Cr.NS_ERROR_FAILURE,
                                "Cannot start after finalization."));
    }

    // Initialize all the status properties for a new or restarted download.
    this.stopped = false;
    this.canceled = false;
    this.error = null;
    this.hasProgress = false;
    this.progress = 0;
    this.totalBytes = 0;
    this.currentBytes = 0;
    this.startTime = new Date();

    // Create a new deferred object and an associated promise before starting
    // the actual download.  We store it on the download as the current attempt.
    let deferAttempt = Promise.defer();
    let currentAttempt = deferAttempt.promise;
    this._currentAttempt = currentAttempt;

    // This function propagates progress from the DownloadSaver object, unless
    // it comes in late from a download attempt that was replaced by a new one.
    function DS_setProgressBytes(aCurrentBytes, aTotalBytes, aHasPartialData)
    {
      if (this._currentAttempt == currentAttempt || !this._currentAttempt) {
        this._setBytes(aCurrentBytes, aTotalBytes, aHasPartialData);
      }
    }

    // This function propagates download properties from the DownloadSaver
    // object, unless it comes in late from a download attempt that was
    // replaced by a new one.
    function DS_setProperties(aOptions)
    {
      if (this._currentAttempt && this._currentAttempt != currentAttempt) {
        return;
      }

      let changeMade = false;

      if ("contentType" in aOptions &&
          this.contentType != aOptions.contentType) {
        this.contentType = aOptions.contentType;
        changeMade = true;
      }

      if (changeMade) {
        this._notifyChange();
      }
    }

    // Now that we stored the promise in the download object, we can start the
    // task that will actually execute the download.
    deferAttempt.resolve(Task.spawn(function task_D_start() {
      // Wait upon any pending operation before restarting.
      if (this._promiseCanceled) {
        yield this._promiseCanceled;
      }
      if (this._promiseRemovePartialData) {
        try {
          yield this._promiseRemovePartialData;
        } catch (ex) {
          // Ignore any errors, which are already reported by the original
          // caller of the removePartialData method.
        }
      }

      // Disallow download if parental controls service restricts it.
      if (yield DownloadIntegration.shouldBlockForParentalControls(this)) {
        let error = new DownloadError(Cr.NS_ERROR_FAILURE, "Download blocked.");
        error.becauseBlocked = true;
        error.becauseBlockedByParentalControls = true;
        throw error;
      }

      try {
        // Execute the actual download through the saver object.
        yield this.saver.execute(DS_setProgressBytes.bind(this),
                                 DS_setProperties.bind(this));

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

            if (this.launchWhenSucceeded) {
              this.launch().then(null, Cu.reportError);
            }
          }
        }
      }
    }.bind(this)));

    // Notify the new download state before returning.
    this._notifyChange();
    return this._currentAttempt;
  },

  /*
   * Launches the file after download has completed. This can open
   * the file with the default application for the target MIME type
   * or file extension, or with a custom application if launcherPath
   * is set.
   *
   * @return {Promise}
   * @resolves When the instruction to launch the file has been
   *           successfully given to the operating system. Note that
   *           the OS might still take a while until the file is actually
   *           launched.
   * @rejects  JavaScript exception if there was an error trying to launch
   *           the file.
   */
  launch: function() {
    if (!this.succeeded) {
      return Promise.reject(
        new Error("launch can only be called if the download succeeded")
      );
    }

    return DownloadIntegration.launchDownload(this);
  },

  /*
   * Shows the folder containing the target file, or where the target file
   * will be saved. This may be called at any time, even if the download
   * failed or is currently in progress.
   *
   * @return {Promise}
   * @resolves When the instruction to open the containing folder has been
   *           successfully given to the operating system. Note that
   *           the OS might still take a while until the folder is actually
   *           opened.
   * @rejects  JavaScript exception if there was an error trying to open
   *           the containing folder.
   */
  showContainingDirectory: function D_showContainingDirectory() {
    return DownloadIntegration.showContainingDirectory(this.target.path);
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
   * Indicates whether any partially downloaded data should be retained, to use
   * when restarting a failed or canceled download.  The default is false.
   *
   * Whether partial data can actually be retained depends on the saver and the
   * download source, and may not be known before the download is started.
   *
   * To have any effect, this property must be set before starting the download.
   * Resetting this property to false after the download has already started
   * will not remove any partial data.
   *
   * If this property is set to true, care should be taken that partial data is
   * removed before the reference to the download is discarded.  This can be
   * done using the removePartialData or the "finalize" methods.
   */
  tryToKeepPartialData: false,

  /**
   * When a request to remove partially downloaded data is received, contains a
   * promise that will be resolved when the removal request is processed.  When
   * the request is processed, this property becomes null again.
   */
  _promiseRemovePartialData: null,

  /**
   * Removes any partial data kept as part of a canceled or failed download.
   *
   * If the download is not canceled or failed, this method has no effect, and
   * it returns a resolved promise.  If the "cancel" method was called but the
   * cancellation process has not finished yet, this method waits for the
   * cancellation to finish, then removes the partial data.
   *
   * After this method has been called, if the tryToKeepPartialData property is
   * still true when the download is restarted, partial data will be retained
   * during the new download attempt.
   *
   * @return {Promise}
   * @resolves When the partial data has been successfully removed.
   * @rejects JavaScript exception if the operation could not be completed.
   */
  removePartialData: function ()
  {
    if (!this.canceled && !this.error) {
      return Promise.resolve();
    }

    let promiseRemovePartialData = this._promiseRemovePartialData;

    if (!promiseRemovePartialData) {
      let deferRemovePartialData = Promise.defer();
      promiseRemovePartialData = deferRemovePartialData.promise;
      this._promiseRemovePartialData = promiseRemovePartialData;

      deferRemovePartialData.resolve(
        Task.spawn(function task_D_removePartialData() {
          try {
            // Wait upon any pending cancellation request.
            if (this._promiseCanceled) {
              yield this._promiseCanceled;
            }
            // Ask the saver object to remove any partial data.
            yield this.saver.removePartialData();
            // For completeness, clear the number of bytes transferred.
            if (this.currentBytes != 0 || this.hasPartialData) {
              this.currentBytes = 0;
              this.hasPartialData = false;
              this._notifyChange();
            }
          } finally {
            this._promiseRemovePartialData = null;
          }
        }.bind(this)));
    }

    return promiseRemovePartialData;
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
   * True if the "finalize" method has been called.  This prevents the download
   * from starting again after having been stopped.
   */
  _finalized: false,

  /**
   * Ensures that the download is stopped, and optionally removes any partial
   * data kept as part of a canceled or failed download.  After this method has
   * been called, the download cannot be started again.
   *
   * This method should be used in place of "cancel" and removePartialData while
   * shutting down or disposing of the download object, to prevent other callers
   * from interfering with the operation.  This is required because cancellation
   * and other operations are asynchronous.
   *
   * @param aRemovePartialData
   *        Whether any partially downloaded data should be removed after the
   *        download has been stopped.
   *
   * @return {Promise}
   * @resolves When the operation has finished successfully.
   * @rejects JavaScript exception if an error occurred while removing the
   *          partially downloaded data.
   */
  finalize: function (aRemovePartialData)
  {
    // Prevents the download from starting again after having been stopped.
    this._finalized = true;

    if (aRemovePartialData) {
      // Cancel the download, in case it is currently in progress, then remove
      // any partially downloaded data.  The removal operation waits for
      // cancellation to be completed before resolving the promise it returns.
      this.cancel();
      return this.removePartialData();
    } else {
      // Just cancel the download, in case it is currently in progress.
      return this.cancel();
    }
  },

  /**
   * Updates progress notifications based on the number of bytes transferred.
   *
   * @param aCurrentBytes
   *        Number of bytes transferred until now.
   * @param aTotalBytes
   *        Total number of bytes to be transferred, or -1 if unknown.
   * @param aHasPartialData
   *        Indicates whether the partially downloaded data can be used when
   *        restarting the download if it fails or is canceled.
   */
  _setBytes: function D_setBytes(aCurrentBytes, aTotalBytes, aHasPartialData) {
    this.currentBytes = aCurrentBytes;
    this.hasPartialData = aHasPartialData;
    if (aTotalBytes != -1) {
      this.hasProgress = true;
      this.totalBytes = aTotalBytes;
      if (aTotalBytes > 0) {
        this.progress = Math.floor(aCurrentBytes / aTotalBytes * 100);
      }
    }
    this._notifyChange();
  },

  /**
   * Returns a static representation of the current object state.
   *
   * @return A JavaScript object that can be serialized to JSON.
   */
  toSerializable: function ()
  {
    let serializable = {
      source: this.source.toSerializable(),
      target: this.target.toSerializable(),
    };

    // Simplify the representation for the most common saver type.  If the saver
    // is an object instead of a simple string, we can't simplify it because we
    // need to persist all its properties, not only "type".  This may happen for
    // savers of type "copy" as well as other types.
    let saver = this.saver.toSerializable();
    if (saver !== "copy") {
      serializable.saver = saver;
    }

    if (this.launcherPath) {
      serializable.launcherPath = this.launcherPath;
    }

    if (this.launchWhenSucceeded) {
      serializable.launchWhenSucceeded = true;
    }

    if (this.contentType) {
      serializable.contentType = this.contentType;
    }

    return serializable;
  },
};

/**
 * Creates a new Download object from a serializable representation.  This
 * function is used by the createDownload method of Downloads.jsm when a new
 * Download object is requested, thus some properties may refer to live objects
 * in place of their serializable representations.
 *
 * @param aSerializable
 *        An object with the following fields:
 *        {
 *          source: DownloadSource object, or its serializable representation.
 *                  See DownloadSource.fromSerializable for details.
 *          target: DownloadTarget object, or its serializable representation.
 *                  See DownloadTarget.fromSerializable for details.
 *          saver: Serializable representation of a DownloadSaver object.  See
 *                 DownloadSaver.fromSerializable for details.  If omitted,
 *                 defaults to "copy".
 *        }
 *
 * @return The newly created Download object.
 */
Download.fromSerializable = function (aSerializable) {
  let download = new Download();
  if (aSerializable.source instanceof DownloadSource) {
    download.source = aSerializable.source;
  } else {
    download.source = DownloadSource.fromSerializable(aSerializable.source);
  }
  if (aSerializable.target instanceof DownloadTarget) {
    download.target = aSerializable.target;
  } else {
    download.target = DownloadTarget.fromSerializable(aSerializable.target);
  }
  if ("saver" in aSerializable) {
    download.saver = DownloadSaver.fromSerializable(aSerializable.saver);
  } else {
    download.saver = DownloadSaver.fromSerializable("copy");
  }
  download.saver.download = download;

  if ("launchWhenSucceeded" in aSerializable) {
    download.launchWhenSucceeded = !!aSerializable.launchWhenSucceeded;
  }

  if ("contentType" in aSerializable) {
    download.contentType = aSerializable.contentType;
  }

  if ("launcherPath" in aSerializable) {
    download.launcherPath = aSerializable.launcherPath;
  }

  return download;
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadSource

/**
 * Represents the source of a download, for example a document or an URI.
 */
function DownloadSource() { }

DownloadSource.prototype = {
  /**
   * String containing the URI for the download source.
   */
  url: null,

  /**
   * Indicates whether the download originated from a private window.  This
   * determines the context of the network request that is made to retrieve the
   * resource.
   */
  isPrivate: false,

  /**
   * String containing the referrer URI of the download source, or null if no
   * referrer should be sent or the download source is not HTTP.
   */
  referrer: null,

  /**
   * Returns a static representation of the current object state.
   *
   * @return A JavaScript object that can be serialized to JSON.
   */
  toSerializable: function ()
  {
    // Simplify the representation if we don't have other details.
    if (!this.isPrivate && !this.referrer) {
      return this.url;
    }

    let serializable = { url: this.url };
    if (this.isPrivate) {
      serializable.isPrivate = true;
    }
    if (this.referrer) {
      serializable.referrer = this.referrer;
    }
    return serializable;
  },
};

/**
 * Creates a new DownloadSource object from its serializable representation.
 *
 * @param aSerializable
 *        Serializable representation of a DownloadSource object.  This may be a
 *        string containing the URI for the download source, an nsIURI, or an
 *        object with the following properties:
 *        {
 *          url: String containing the URI for the download source.
 *          isPrivate: Indicates whether the download originated from a private
 *                     window.  If omitted, the download is public.
 *          referrer: String containing the referrer URI of the download source.
 *                    Can be omitted or null if no referrer should be sent or
 *                    the download source is not HTTP.
 *        }
 *
 * @return The newly created DownloadSource object.
 */
DownloadSource.fromSerializable = function (aSerializable) {
  let source = new DownloadSource();
  if (isString(aSerializable)) {
    // Convert String objects to primitive strings at this point.
    source.url = aSerializable.toString();
  } else if (aSerializable instanceof Ci.nsIURI) {
    source.url = aSerializable.spec;
  } else {
    // Convert String objects to primitive strings at this point.
    source.url = aSerializable.url.toString();
    if ("isPrivate" in aSerializable) {
      source.isPrivate = aSerializable.isPrivate;
    }
    if ("referrer" in aSerializable) {
      source.referrer = aSerializable.referrer;
    }
  }
  return source;
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
   * String containing the path of the target file.
   */
  path: null,

  /**
   * String containing the path of the ".part" file containing the data
   * downloaded so far, or null to disable the use of a ".part" file to keep
   * partially downloaded data.
   */
  partFilePath: null,

  /**
   * Returns a static representation of the current object state.
   *
   * @return A JavaScript object that can be serialized to JSON.
   */
  toSerializable: function ()
  {
    // Simplify the representation if we don't have other details.
    if (!this.partFilePath) {
      return this.path;
    }

    return { path: this.path,
             partFilePath: this.partFilePath };
  },
};

/**
 * Creates a new DownloadTarget object from its serializable representation.
 *
 * @param aSerializable
 *        Serializable representation of a DownloadTarget object.  This may be a
 *        string containing the path of the target file, an nsIFile, or an
 *        object with the following properties:
 *        {
 *          path: String containing the path of the target file.
 *        }
 *
 * @return The newly created DownloadTarget object.
 */
DownloadTarget.fromSerializable = function (aSerializable) {
  let target = new DownloadTarget();
  if (isString(aSerializable)) {
    // Convert String objects to primitive strings at this point.
    target.path = aSerializable.toString();
  } else if (aSerializable instanceof Ci.nsIFile) {
    // Read the "path" property of nsIFile after checking the object type.
    target.path = aSerializable.path;
  } else {
    // Read the "path" property of the serializable DownloadTarget
    // representation, converting String objects to primitive strings.
    target.path = aSerializable.path.toString();
    if ("partFilePath" in aSerializable) {
      target.partFilePath = aSerializable.partFilePath;
    }
  }
  return target;
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
  this.stack = new Error().stack;
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

  /**
   * Indicates the download failed because it was blocked.  If the reason for
   * blocking is known, the corresponding property will be also set.
   */
  becauseBlocked: false,

  /**
   * Indicates the download was blocked because downloads are globally
   * disallowed by the Parental Controls or Family Safety features on Windows.
   */
  becauseBlockedByParentalControls: false,
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
   *
   * If the tryToKeepPartialData property of the download object is false, the
   * saver should never try to keep partially downloaded data if the download
   * fails.
   */
  download: null,

  /**
   * Executes the download.
   *
   * @param aSetProgressBytesFn
   *        This function may be called by the saver to report progress. It
   *        takes three arguments: the first is the number of bytes transferred
   *        until now, the second is the total number of bytes to be
   *        transferred (or -1 if unknown), the third indicates whether the
   *        partially downloaded data can be used when restarting the download
   *        if it fails or is canceled.
   * @param aSetPropertiesFn
   *        This function may be called by the saver to report information
   *        about new download properties discovered by the saver during the
   *        download process. It takes an object where the keys represents
   *        the names of the properties to set, and the value represents the
   *        value to set.
   *
   * @return {Promise}
   * @resolves When the download has finished successfully.
   * @rejects JavaScript exception if the download failed.
   */
  execute: function DS_execute(aSetProgressBytesFn, aSetPropertiesFn)
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

  /**
   * Removes any partial data kept as part of a canceled or failed download.
   *
   * This method is never called until the promise returned by "execute" is
   * either resolved or rejected, and the "execute" method is not called again
   * until the promise returned by this method is resolved or rejected.
   *
   * @return {Promise}
   * @resolves When the operation has finished successfully.
   * @rejects JavaScript exception.
   */
  removePartialData: function DS_removePartialData()
  {
    return Promise.resolve();
  },

  /**
   * Returns a static representation of the current object state.
   *
   * @return A JavaScript object that can be serialized to JSON.
   */
  toSerializable: function ()
  {
    throw new Error("Not implemented.");
  },
};

/**
 * Creates a new DownloadSaver object from its serializable representation.
 *
 * @param aSerializable
 *        Serializable representation of a DownloadSaver object.  If no initial
 *        state information for the saver object is needed, can be a string
 *        representing the class of the download operation, for example "copy".
 *
 * @return The newly created DownloadSaver object.
 */
DownloadSaver.fromSerializable = function (aSerializable) {
  let serializable = isString(aSerializable) ? { type: aSerializable }
                                             : aSerializable;
  let saver;
  switch (serializable.type) {
    case "copy":
      saver = DownloadCopySaver.fromSerializable(serializable);
      break;
    case "legacy":
      saver = DownloadLegacySaver.fromSerializable(serializable);
      break;
    default:
      throw new Error("Unrecoginzed download saver type.");
  }
  return saver;
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
   * Indicates whether the "cancel" method has been called.  This is used to
   * prevent the request from starting in case the operation is canceled before
   * the BackgroundFileSaver instance has been created.
   */
  _canceled: false,

  /**
   * Implements "DownloadSaver.execute".
   */
  execute: function DCS_execute(aSetProgressBytesFn, aSetPropertiesFn)
  {
    let copySaver = this;

    this._canceled = false;

    let download = this.download;
    let targetPath = download.target.path;
    let partFilePath = download.target.partFilePath;
    let keepPartialData = download.tryToKeepPartialData;

    return Task.spawn(function task_DCS_execute() {
      // To reduce the chance that other downloads reuse the same final target
      // file name, we should create a placeholder as soon as possible, before
      // starting the network request.  The placeholder is also required in case
      // we are using a ".part" file instead of the final target while the
      // download is in progress.
      try {
        // If the file already exists, don't delete its contents yet.
        let file = yield OS.File.open(targetPath, { write: true });
        yield file.close();
      } catch (ex if ex instanceof OS.File.Error) {
        // Throw a DownloadError indicating that the operation failed because of
        // the target file.  We cannot translate this into a specific result
        // code, but we preserve the original message using the toString method.
        let error = new DownloadError(Cr.NS_ERROR_FAILURE, ex.toString());
        error.becauseTargetFailed = true;
        throw error;
      }

      try {
        let deferSaveComplete = Promise.defer();

        if (this._canceled) {
          // Don't create the BackgroundFileSaver object if we have been
          // canceled meanwhile.
          throw new DownloadError(Cr.NS_ERROR_FAILURE, "Saver canceled.");
        }

        // Create the object that will save the file in a background thread.
        let backgroundFileSaver = new BackgroundFileSaverStreamListener();
        try {
          // When the operation completes, reflect the status in the promise
          // returned by this download execution function.
          backgroundFileSaver.observer = {
            onTargetChange: function () { },
            onSaveComplete: function DCSE_onSaveComplete(aSaver, aStatus)
            {
              // Free the reference cycle, to release resources earlier.
              backgroundFileSaver.observer = null;
              this._backgroundFileSaver = null;

              // Send notifications now that we can restart if needed.
              if (Components.isSuccessCode(aStatus)) {
                deferSaveComplete.resolve();
              } else {
                // Infer the origin of the error from the failure code, because
                // BackgroundFileSaver does not provide more specific data.
                deferSaveComplete.reject(new DownloadError(aStatus, null,
                                                           true));
              }
            },
          };

          // Create a channel from the source, and listen to progress
          // notifications.
          let channel = NetUtil.newChannel(NetUtil.newURI(download.source.url));
          if (channel instanceof Ci.nsIPrivateBrowsingChannel) {
            channel.setPrivate(download.source.isPrivate);
          }
          if (channel instanceof Ci.nsIHttpChannel &&
              download.source.referrer) {
            channel.referrer = NetUtil.newURI(download.source.referrer);
          }

          // If we have data that we can use to resume the download from where
          // it stopped, try to use it.
          let resumeAttempted = false;
          let resumeFromBytes = 0;
          if (channel instanceof Ci.nsIResumableChannel && this.entityID &&
              partFilePath && keepPartialData) {
            try {
              let stat = yield OS.File.stat(partFilePath);
              channel.resumeAt(stat.size, this.entityID);
              resumeAttempted = true;
              resumeFromBytes = stat.size;
            } catch (ex if ex instanceof OS.File.Error &&
                           ex.becauseNoSuchFile) { }
          }

          channel.notificationCallbacks = {
            QueryInterface: XPCOMUtils.generateQI([Ci.nsIInterfaceRequestor]),
            getInterface: XPCOMUtils.generateQI([Ci.nsIProgressEventSink]),
            onProgress: function DCSE_onProgress(aRequest, aContext, aProgress,
                                                 aProgressMax)
            {
              let currentBytes = resumeFromBytes + aProgress;
              let totalBytes = aProgressMax == -1 ? -1 : (resumeFromBytes +
                                                          aProgressMax);
              aSetProgressBytesFn(currentBytes, totalBytes, aProgress > 0 &&
                                  partFilePath && keepPartialData);
            },
            onStatus: function () { },
          };

          // Open the channel, directing output to the background file saver.
          backgroundFileSaver.QueryInterface(Ci.nsIStreamListener);
          channel.asyncOpen({
            onStartRequest: function (aRequest, aContext) {
              backgroundFileSaver.onStartRequest(aRequest, aContext);

              aSetPropertiesFn({ contentType: channel.contentType });

              // Ensure we report the value of "Content-Length", if available,
              // even if the download doesn't generate any progress events
              // later.
              if (channel.contentLength >= 0) {
                aSetProgressBytesFn(0, channel.contentLength);
              }

              if (keepPartialData) {
                // If the source is not resumable, don't keep partial data even
                // if we were asked to try and do it.
                if (aRequest instanceof Ci.nsIResumableChannel) {
                  try {
                    // If reading the ID succeeds, the source is resumable.
                    this.entityID = aRequest.entityID;
                  } catch (ex if ex instanceof Components.Exception &&
                                 ex.result == Cr.NS_ERROR_NOT_RESUMABLE) {
                    keepPartialData = false;
                  }
                } else {
                  keepPartialData = false;
                }
              }

              if (partFilePath) {
                // If we actually resumed a request, append to the partial data.
                if (resumeAttempted) {
                  // TODO: Handle Cr.NS_ERROR_ENTITY_CHANGED
                  backgroundFileSaver.enableAppend();
                }

                // Use a part file, determining if we should keep it on failure.
                backgroundFileSaver.setTarget(new FileUtils.File(partFilePath),
                                              keepPartialData);
              } else {
                // Set the final target file, and delete it on failure.
                backgroundFileSaver.setTarget(new FileUtils.File(targetPath),
                                              false);
              }
            }.bind(copySaver),

            onStopRequest: function (aRequest, aContext, aStatusCode) {
              try {
                backgroundFileSaver.onStopRequest(aRequest, aContext,
                                                  aStatusCode);
              } finally {
                // If the data transfer completed successfully, indicate to the
                // background file saver that the operation can finish.  If the
                // data transfer failed, the saver has been already stopped.
                if (Components.isSuccessCode(aStatusCode)) {
                  if (partFilePath) {
                    // Move to the final target if we were using a part file.
                    backgroundFileSaver.setTarget(
                                        new FileUtils.File(targetPath), false);
                  }
                  backgroundFileSaver.finish(Cr.NS_OK);
                }
              }
            }.bind(copySaver),

            onDataAvailable: function (aRequest, aContext, aInputStream,
                                       aOffset, aCount) {
              backgroundFileSaver.onDataAvailable(aRequest, aContext,
                                                  aInputStream, aOffset,
                                                  aCount);
            }.bind(copySaver),
          }, null);

          // If the operation succeeded, store the object to allow cancellation.
          this._backgroundFileSaver = backgroundFileSaver;
        } catch (ex) {
          // In case an error occurs while setting up the chain of objects for
          // the download, ensure that we release the resources of the saver.
          backgroundFileSaver.finish(Cr.NS_ERROR_FAILURE);
          throw ex;
        }

        // We will wait on this promise in case no error occurred while setting
        // up the chain of objects for the download.
        yield deferSaveComplete.promise;
      } catch (ex) {
        // Ensure we always remove the placeholder for the final target file on
        // failure, independently of which code path failed.  In some cases, the
        // background file saver may have already removed the file.
        try {
          yield OS.File.remove(targetPath);
        } catch (e2 if e2 instanceof OS.File.Error && e2.becauseNoSuchFile) { }
        throw ex;
      }
    }.bind(this));
  },

  /**
   * Implements "DownloadSaver.cancel".
   */
  cancel: function DCS_cancel()
  {
    this._canceled = true;
    if (this._backgroundFileSaver) {
      this._backgroundFileSaver.finish(Cr.NS_ERROR_FAILURE);
      this._backgroundFileSaver = null;
    }
  },

  /**
   * Implements "DownloadSaver.removePartialData".
   */
  removePartialData: function ()
  {
    return Task.spawn(function task_DCS_removePartialData() {
      if (this.download.target.partFilePath) {
        try {
          yield OS.File.remove(this.download.target.partFilePath);
        } catch (ex if ex instanceof OS.File.Error && ex.becauseNoSuchFile) { }
      }
    }.bind(this));
  },

  /**
   * Implements "DownloadSaver.toSerializable".
   */
  toSerializable: function ()
  {
    // Simplify the representation since we don't have other details for now.
    return "copy";
  },
};

/**
 * Creates a new DownloadCopySaver object, with its initial state derived from
 * its serializable representation.
 *
 * @param aSerializable
 *        Serializable representation of a DownloadCopySaver object.
 *
 * @return The newly created DownloadCopySaver object.
 */
DownloadCopySaver.fromSerializable = function (aSerializable) {
  // We don't have other state details for now.
  return new DownloadCopySaver();
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

    let hasPartFile = !!this.download.target.partFilePath;

    this.progressWasNotified = true;
    this.setProgressBytesFn(aCurrentBytes, aTotalBytes,
                            aCurrentBytes > 0 && hasPartFile);
  },

  /**
   * Whether the onProgressBytes function has been called at least once.
   */
  progressWasNotified: false,

  /**
   * Called by the nsITransfer implementation when the request has started.
   *
   * @param aRequest
   *        nsIRequest associated to the status update.
   */
  onTransferStarted: function (aRequest)
  {
    // Store the entity ID to use for resuming if required.
    if (this.download.tryToKeepPartialData &&
        aRequest instanceof Ci.nsIResumableChannel) {
      try {
        // If reading the ID succeeds, the source is resumable.
        this.entityID = aRequest.entityID;
      } catch (ex if ex instanceof Components.Exception &&
                     ex.result == Cr.NS_ERROR_NOT_RESUMABLE) { }
    }
  },

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
   * When the first execution of the download finished, it can be restarted by
   * using a DownloadCopySaver object instead of the original legacy component
   * that executed the download.
   */
  firstExecutionFinished: false,

  /**
   * In case the download is restarted after the first execution finished, this
   * property contains a reference to the DownloadCopySaver that is executing
   * the new download attempt.
   */
  copySaver: null,

  /**
   * Implements "DownloadSaver.execute".
   */
  execute: function DLS_execute(aSetProgressBytesFn)
  {
    // Check if this is not the first execution of the download.  The Download
    // object guarantees that this function is not re-entered during execution.
    if (this.firstExecutionFinished) {
      if (!this.copySaver) {
        this.copySaver = new DownloadCopySaver();
        this.copySaver.download = this.download;
        this.copySaver.entityID = this.entityID;
      }
      return this.copySaver.execute.apply(this.copySaver, arguments);
    }

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

        // If the component executing the download provides the path of a
        // ".part" file, it means that it expects the listener to move the file
        // to its final target path when the download succeeds.  In this case,
        // an empty ".part" file is created even if no data was received from
        // the source.
        if (this.download.target.partFilePath) {
          yield OS.File.move(this.download.target.partFilePath,
                             this.download.target.path);
        } else {
          // The download implementation may not have created the target file if
          // no data was received from the source.  In this case, ensure that an
          // empty file is created as expected.
          try {
            // This atomic operation is more efficient than an existence check.
            let file = yield OS.File.open(this.download.target.path,
                                          { create: true });
            yield file.close();
          } catch (ex if ex instanceof OS.File.Error && ex.becauseExists) { }
        }
      } finally {
        // We don't need the reference to the request anymore.
        this.request = null;
        // Allow the download to restart through a DownloadCopySaver.
        this.firstExecutionFinished = true;
      }
    }.bind(this));
  },

  /**
   * Implements "DownloadSaver.cancel".
   */
  cancel: function DLS_cancel()
  {
    // We may be using a DownloadCopySaver to handle resuming.
    if (this.copySaver) {
      return this.copySaver.cancel.apply(this.copySaver, arguments);
    }

    // Synchronously cancel the operation as soon as the object is connected.
    this.deferCanceled.resolve();

    // We don't necessarily receive status notifications after we call "cancel",
    // but cancellation through nsICancelable should be synchronous, thus force
    // the rejection of the execution promise immediately.
    this.deferExecuted.reject(new DownloadError(Cr.NS_ERROR_FAILURE,
                                                "Download canceled."));
  },

  /**
   * Implements "DownloadSaver.removePartialData".
   */
  removePartialData: function ()
  {
    // DownloadCopySaver and DownloadLeagcySaver use the same logic for removing
    // partially downloaded data, though this implementation isn't shared by
    // other saver types, thus it isn't found on their shared prototype.
    return DownloadCopySaver.prototype.removePartialData.call(this);
  },

  /**
   * Implements "DownloadSaver.toSerializable".
   */
  toSerializable: function ()
  {
    // This object depends on legacy components that are created externally,
    // thus it cannot be rebuilt during deserialization.  To support resuming
    // across different browser sessions, this object is transformed into a
    // DownloadCopySaver for the purpose of serialization.
    return "copy";
  },
};

/**
 * Returns a new DownloadLegacySaver object.  This saver type has a
 * deserializable form only when creating a new object in memory, because it
 * cannot be serialized to disk.
 */
DownloadLegacySaver.fromSerializable = function () {
  return new DownloadLegacySaver();
};
