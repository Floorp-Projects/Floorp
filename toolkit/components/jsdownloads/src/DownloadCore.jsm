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
                                  "resource://gre/modules/commonjs/promise/core.js");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

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
      yield this.saver.execute();
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

    NetUtil.asyncFetch(this.download.source.uri, function (aInputStream, aResult) {
      if (!Components.isSuccessCode(aResult)) {
        deferred.reject(new Components.Exception("Download failed.", aResult));
        return;
      }

      let fileOutputStream = Cc["@mozilla.org/network/file-output-stream;1"]
                              .createInstance(Ci.nsIFileOutputStream);
      fileOutputStream.init(this.download.target.file, -1, -1, 0);

      NetUtil.asyncCopy(aInputStream, fileOutputStream, function (aResult) {
        if (!Components.isSuccessCode(aResult)) {
          deferred.reject(new Components.Exception("Download failed.", aResult));
          return;
        }

        deferred.resolve();
      }.bind(this));
    }.bind(this));

    return deferred.promise;
  },
};
