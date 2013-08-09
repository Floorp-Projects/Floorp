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
XPCOMUtils.defineLazyModuleGetter(this, "DownloadUIHelper",
                                  "resource://gre/modules/DownloadUIHelper.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/commonjs/sdk/core/promise.js");
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
   *        This matches the serializable representation of a Download object.
   *        Some of the most common properties in this object include:
   *        {
   *          source: String containing the URI for the download source.
   *                  Alternatively, may be an nsIURI, a DownloadSource object,
   *                  or an object with the following properties:
   *          {
   *            url: String containing the URI for the download source.
   *            isPrivate: Indicates whether the download originated from a
   *                       private window.  If omitted, the download is public.
   *            referrer: String containing the referrer URI of the download
   *                      source.  Can be omitted or null if no referrer should
   *                      be sent or the download source is not HTTP.
   *          },
   *          target: String containing the path of the target file.
   *                  Alternatively, may be an nsIFile, a DownloadTarget object,
   *                  or an object with the following properties:
   *          {
   *            path: String containing the path of the target file.
   *          },
   *          saver: String representing the class of the download operation.
   *                 If omitted, defaults to "copy".  Alternatively, may be the
   *                 serializable representation of a DownloadSaver object.
   *        }
   *
   * @return {Promise}
   * @resolves The newly created Download object.
   * @rejects JavaScript exception.
   */
  createDownload: function D_createDownload(aProperties)
  {
    try {
      return Promise.resolve(Download.fromSerializable(aProperties));
    } catch (ex) {
      return Promise.reject(ex);
    }
  },

  /**
   * Downloads data from a remote network location to a local file.
   *
   * This download method does not provide user interface, or the ability to
   * cancel or restart the download programmatically.  For that, you should
   * obtain a reference to a Download object using the createDownload function.
   *
   * Since the download cannot be restarted, any partially downloaded data will
   * not be kept in case the download fails.
   *
   * @param aSource
   *        String containing the URI for the download source.  Alternatively,
   *        may be an nsIURI or a DownloadSource object.
   * @param aTarget
   *        String containing the path of the target file.  Alternatively, may
   *        be an nsIFile or a DownloadTarget object.
   * @param aOptions
   *        An optional object used to control the behavior of this function.
   *        You may pass an object with a subset of the following fields:
   *        {
   *          isPrivate: Indicates whether the download originated from a
   *                     private window.
   *        }
   *
   * @return {Promise}
   * @resolves When the download has finished successfully.
   * @rejects JavaScript exception if the download failed.
   */
  simpleDownload: function D_simpleDownload(aSource, aTarget, aOptions) {
    return this.createDownload({
      source: aSource,
      target: aTarget,
    }).then(function D_SD_onSuccess(aDownload) {
      if (aOptions && ("isPrivate" in aOptions)) {
        aDownload.source.isPrivate = aOptions.isPrivate;
      }
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
    if (!this._promisePublicDownloadList) {
      this._promisePublicDownloadList = Task.spawn(
        function task_D_getPublicDownloadList() {
          let list = new DownloadList(true);
          try {
            yield DownloadIntegration.addListObservers(list, false);
            yield DownloadIntegration.loadPersistent(list);
          } catch (ex) {
            Cu.reportError(ex);
          }
          throw new Task.Result(list);
        });
    }
    return this._promisePublicDownloadList;
  },

  /**
   * This promise is resolved with a reference to a DownloadList object that
   * represents persistent downloads.  This property is null before the list of
   * downloads is requested for the first time.
   */
  _promisePublicDownloadList: null,

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
    if (!this._promisePrivateDownloadList) {
      this._promisePrivateDownloadList = Task.spawn(
        function task_D_getPublicDownloadList() {
          let list = new DownloadList(false);
          try {
            yield DownloadIntegration.addListObservers(list, true);
          } catch (ex) {
            Cu.reportError(ex);
          }
          throw new Task.Result(list);
        });
    }
    return this._promisePrivateDownloadList;
  },

  /**
   * This promise is resolved with a reference to a DownloadList object that
   * represents private downloads. This property is null before the list of
   * downloads is requested for the first time.
   */
  _promisePrivateDownloadList: null,

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
