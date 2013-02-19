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
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "Download",
  "DownloadSource",
  "DownloadTarget",
  "DownloadError",
  "DownloadSaver",
  "DownloadCopySaver",
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
  this._deferStopped = Promise.defer();
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
   * Becomes true when the download has been completed successfully, failed, or
   * has been canceled.  This property can become true, then it can be reset to
   * false when a failed or canceled download is restarted.
   */
  stopped: false,

  /**
   * Indicates that the download has been canceled.  This property can become
   * true, then it can be reset to false when a canceled download is restarted.
   */
  canceled: false,

  /**
   * When the download fails, this is set to a DownloadError instance indicating
   * the cause of the failure.  If the download has been completed successfully
   * or has been canceled, this property is null.
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
   * This deferred object is resolved when this download finishes successfully,
   * and rejected if this download fails.
   */
  _deferStopped: null,

  /**
   * Starts the download.
   *
   * @return {Promise}
   * @resolves When the download has finished successfully.
   * @rejects JavaScript exception if the download failed.
   */
  start: function D_start()
  {
    this._deferStopped.resolve(Task.spawn(function task_D_start() {
      try {
        yield this.saver.execute();
        this.progress = 100;
      } catch (ex) {
        if (this.canceled) {
          throw new DownloadError(Cr.NS_ERROR_FAILURE, "Download canceled.");
        }
        this.error = ex;
        throw ex;
      } finally {
        this.stopped = true;
        this._notifyChange();
      }
    }.bind(this)));

    return this._deferStopped.promise;
  },

  /**
   * Cancels the download.
   */
  cancel: function D_cancel()
  {
    if (this.stopped || this.canceled) {
      return;
    }

    this.canceled = true;
    this.saver.cancel();
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
   * @return {Promise}
   * @resolves When the download has finished successfully.
   * @rejects JavaScript exception if the download failed.
   */
  execute: function DS_execute()
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
  execute: function DCS_execute()
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
          if (Components.isSuccessCode(aStatus)) {
            deferred.resolve();
          } else {
            // Infer the origin of the error from the failure code, because
            // BackgroundFileSaver does not provide more specific data.
            deferred.reject(new DownloadError(aStatus, null, true));
          }

          // Free the reference cycle, in order to release resources earlier.
          backgroundFileSaver.observer = null;
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
          download._setBytes(aProgress, aProgressMax);
        },
        onStatus: function () { },
      };

      // Open the channel, directing output to the background file saver.
      backgroundFileSaver.QueryInterface(Ci.nsIStreamListener);
      channel.asyncOpen({
        onStartRequest: function DCSE_onStartRequest(aRequest, aContext)
        {
          backgroundFileSaver.onStartRequest(aRequest, aContext);
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
