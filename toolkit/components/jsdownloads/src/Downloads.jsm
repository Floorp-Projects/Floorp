/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Main entry point to get references to all the back-end objects.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "Downloads",
];

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/DownloadCore.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadIntegration",
                                  "resource://gre/modules/DownloadIntegration.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadList",
                                  "resource://gre/modules/DownloadList.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadStore",
                                  "resource://gre/modules/DownloadStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadUIHelper",
                                  "resource://gre/modules/DownloadUIHelper.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/commonjs/sdk/core/promise.js");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

////////////////////////////////////////////////////////////////////////////////
//// Downloads

/**
 * This object is exposed directly to the consumers of this JavaScript module,
 * and provides the only entry point to get references to back-end objects.
 */
this.Downloads = {
  /**
   * Creates a new Download object.
   *
   * @param aProperties
   *        Provides the initial properties for the newly created download.
   *        {
   *          source: {
   *            uri: The nsIURI for the download source.
   *            isPrivate: Indicates whether the download originated from a
   *                       private window.
   *          },
   *          target: {
   *            file: The nsIFile for the download target.
   *          },
   *          saver: {
   *            type: String representing the class of download operation
   *                  handled by this saver object, for example "copy".
   *          },
   *        }
   *
   * @return {Promise}
   * @resolves The newly created Download object.
   * @rejects JavaScript exception.
   */
  createDownload: function D_createDownload(aProperties)
  {
    return Task.spawn(function task_D_createDownload() {
      let download = new Download();

      download.source = new DownloadSource();
      download.source.uri = aProperties.source.uri;
      if ("isPrivate" in aProperties.source) {
        download.source.isPrivate = aProperties.source.isPrivate;
      }
      if ("referrer" in aProperties.source) {
        download.source.referrer = aProperties.source.referrer;
      }
      download.target = new DownloadTarget();
      download.target.file = aProperties.target.file;

      // Support for different aProperties.saver values isn't implemented yet.
      download.saver = aProperties.saver.type == "legacy"
                       ? new DownloadLegacySaver()
                       : new DownloadCopySaver();
      download.saver.download = download;

      // This explicitly makes this function a generator for Task.jsm, so that
      // exceptions in the above calls can be reported asynchronously.
      yield;
      throw new Task.Result(download);
    });
  },

  /**
   * Downloads data from a remote network location to a local file.
   *
   * This download method does not provide user interface or the ability to
   * cancel the download programmatically.  For that, you should obtain a
   * reference to a Download object using the createDownload function.
   *
   * @param aSource
   *        The nsIURI or string containing the URI spec for the download
   *        source, or alternative DownloadSource.
   * @param aTarget
   *        The nsIFile or string containing the file path, or alternative
   *        DownloadTarget.
   * @param aOptions
   *        The object contains different additional options or null.
   *        {  isPrivate: Indicates whether the download originated from a
   *                      private window.
   *        }
   *
   * @return {Promise}
   * @resolves When the download has finished successfully.
   * @rejects JavaScript exception if the download failed.
   */
  simpleDownload: function D_simpleDownload(aSource, aTarget, aOptions) {
    // Wrap the arguments into simple objects resembling DownloadSource and
    // DownloadTarget, if they are not objects of that type already.
    if (aSource instanceof Ci.nsIURI) {
      aSource = { uri: aSource };
    } else if (typeof aSource == "string" ||
               (typeof aSource == "object" && "charAt" in aSource)) {
      aSource = { uri: NetUtil.newURI(aSource) };
    }

    if (aSource && aOptions && ("isPrivate" in aOptions)) {
      aSource.isPrivate = aOptions.isPrivate;
    }
    if (aTarget instanceof Ci.nsIFile) {
      aTarget = { file: aTarget };
    } else if (typeof aTarget == "string" ||
               (typeof aTarget == "object" && "charAt" in aTarget)) {
      aTarget = { file: new FileUtils.File(aTarget) };
    }

    // Create and start the actual download.
    return this.createDownload({
      source: aSource,
      target: aTarget,
      saver: { type: "copy" },
    }).then(function D_SD_onSuccess(aDownload) {
      return aDownload.start();
    });
  },

  /**
   * Retrieves the DownloadList object for downloads that were not started from
   * a private browsing window.
   *
   * Calling this function may cause the download list to be reloaded from the
   * previous session, if it wasn't loaded already.
   *
   * This method always retrieves a reference to the same download list.
   *
   * @return {Promise}
   * @resolves The DownloadList object for public downloads.
   * @rejects JavaScript exception.
   */
  getPublicDownloadList: function D_getPublicDownloadList()
  {
    if (!this._publicDownloadList) {
      this._publicDownloadList = new DownloadList(true);
    }
    return Promise.resolve(this._publicDownloadList);
  },
  _publicDownloadList: null,

  /**
   * Retrieves the DownloadList object for downloads that were started from
   * a private browsing window.
   *
   * This method always retrieves a reference to the same download list.
   *
   * @return {Promise}
   * @resolves The DownloadList object for private downloads.
   * @rejects JavaScript exception.
   */
  getPrivateDownloadList: function D_getPrivateDownloadList()
  {
    if (!this._privateDownloadList) {
      this._privateDownloadList = new DownloadList(false);
    }
    return Promise.resolve(this._privateDownloadList);
  },
  _privateDownloadList: null,

  /**
   * Returns the system downloads directory asynchronously.
   *   Mac OSX:
   *     User downloads directory
   *   XP/2K:
   *     My Documents/Downloads
   *   Vista and others:
   *     User downloads directory
   *   Linux:
   *     XDG user dir spec, with a fallback to Home/Downloads
   *   Android:
   *     standard downloads directory i.e. /sdcard
   *
   * @return {Promise}
   * @resolves The nsIFile of downloads directory.
   */
  getSystemDownloadsDirectory: function D_getSystemDownloadsDirectory() {
    return DownloadIntegration.getSystemDownloadsDirectory();
  },

  /**
   * Returns the preferred downloads directory based on the user preferences
   * in the current profile asynchronously.
   *
   * @return {Promise}
   * @resolves The nsIFile of downloads directory.
   */
  getUserDownloadsDirectory: function D_getUserDownloadsDirectory() {
    return DownloadIntegration.getUserDownloadsDirectory();
  },

  /**
   * Returns the temporary directory where downloads are placed before the
   * final location is chosen, or while the document is opened temporarily
   * with an external application. This may or may not be the system temporary
   * directory, based on the platform asynchronously.
   *
   * @return {Promise}
   * @resolves The nsIFile of downloads directory.
   */
  getTemporaryDownloadsDirectory: function D_getTemporaryDownloadsDirectory() {
    return DownloadIntegration.getTemporaryDownloadsDirectory();
  },

  /**
   * Constructor for a DownloadError object.  When you catch an exception during
   * a download, you can use this to verify if "ex instanceof Downloads.Error",
   * before reading the exception properties with the error details.
   */
  Error: DownloadError,
};
