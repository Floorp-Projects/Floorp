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
  this._deferDone = Promise.defer();
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
   * false when a failed or canceled download is resumed.  This property remains
   * false while the download is paused.
   */
  done: false,

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
   * This can be set to a function that is called when other properties change.
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
  _deferDone: null,

  /**
   * Starts the download.
   *
   * @return {Promise}
   * @resolves When the download has finished successfully.
   * @rejects JavaScript exception if the download failed.
   */
  start: function D_start()
  {
    this._deferDone.resolve(Task.spawn(function task_D_start() {
      try {
        yield this.saver.execute();
        this.progress = 100;
      } finally {
        this.done = true;
        this._notifyChange();
      }
    }.bind(this)));

    return this.whenDone();
  },

  /**
   * Waits for the download to finish.
   *
   * @return {Promise}
   * @resolves When the download has finished successfully.
   * @rejects JavaScript exception if the download failed.
   */
  whenDone: function D_whenDone() {
    return this._deferDone.promise;
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
  }
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadCopySaver

/**
 * Saver object that simply copies the entire source file to the target.
 */
function DownloadCopySaver() { }

DownloadCopySaver.prototype = {
  __proto__: DownloadSaver,

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
            deferred.reject(new Components.Exception("Download failed.",
                                                     aStatus));
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
    } catch (ex) {
      // In case an error occurs while setting up the chain of objects for the
      // download, ensure that we release the resources of the background saver.
      deferred.reject(ex);
      backgroundFileSaver.finish(Cr.NS_ERROR_FAILURE);
    }
    return deferred.promise;
  },
};
