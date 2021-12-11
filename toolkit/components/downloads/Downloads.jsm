/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Main entry point to get references to all the back-end objects.
 */

"use strict";

var EXPORTED_SYMBOLS = ["Downloads"];

const { Integration } = ChromeUtils.import(
  "resource://gre/modules/Integration.jsm"
);
const { Download, DownloadError } = ChromeUtils.import(
  "resource://gre/modules/DownloadCore.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "DownloadCombinedList",
  "resource://gre/modules/DownloadList.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DownloadList",
  "resource://gre/modules/DownloadList.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DownloadSummary",
  "resource://gre/modules/DownloadList.jsm"
);

/* global DownloadIntegration */
Integration.downloads.defineModuleGetter(
  this,
  "DownloadIntegration",
  "resource://gre/modules/DownloadIntegration.jsm"
);

/**
 * This object is exposed directly to the consumers of this JavaScript module,
 * and provides the only entry point to get references to back-end objects.
 */
var Downloads = {
  /**
   * Work on downloads that were not started from a private browsing window.
   */
  get PUBLIC() {
    return "{Downloads.PUBLIC}";
  },
  /**
   * Work on downloads that were started from a private browsing window.
   */
  get PRIVATE() {
    return "{Downloads.PRIVATE}";
  },
  /**
   * Work on both Downloads.PRIVATE and Downloads.PUBLIC downloads.
   */
  get ALL() {
    return "{Downloads.ALL}";
  },

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
   *            referrerInfo: String or nsIReferrerInfo object represents the
   *                          referrerInfo of the download source.  Can be
   *                          omitted or null for example when the download
   *                          source is not HTTP.
   *            cookieJarSettings: The nsICookieJarSettings object represents
   *                               the cookieJarSetting of the download source.
   *                               Can be omitted or null if the download source
   *                               is not from a document.
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
  createDownload: function D_createDownload(aProperties) {
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
  fetch(aSource, aTarget, aOptions) {
    return this.createDownload({
      source: aSource,
      target: aTarget,
    }).then(function D_SD_onSuccess(aDownload) {
      if (aOptions && "isPrivate" in aOptions) {
        aDownload.source.isPrivate = aOptions.isPrivate;
      }
      return aDownload.start();
    });
  },

  /**
   * Retrieves the specified type of DownloadList object.  There is one download
   * list for each type, and this method always retrieves a reference to the
   * same download list when called with the same argument.
   *
   * Calling this function may cause the list of public downloads to be reloaded
   * from the previous session, if it wasn't loaded already.
   *
   * @param aType
   *        This can be Downloads.PUBLIC, Downloads.PRIVATE, or Downloads.ALL.
   *        Downloads added to the Downloads.PUBLIC and Downloads.PRIVATE lists
   *        are reflected in the Downloads.ALL list, and downloads added to the
   *        Downloads.ALL list are also added to either the Downloads.PUBLIC or
   *        the Downloads.PRIVATE list based on their properties.
   *
   * @return {Promise}
   * @resolves The requested DownloadList or DownloadCombinedList object.
   * @rejects JavaScript exception.
   */
  getList(aType) {
    if (!this._promiseListsInitialized) {
      this._promiseListsInitialized = (async () => {
        let publicList = new DownloadList();
        let privateList = new DownloadList();
        let combinedList = new DownloadCombinedList(publicList, privateList);

        try {
          await DownloadIntegration.addListObservers(publicList, false);
          await DownloadIntegration.addListObservers(privateList, true);
          await DownloadIntegration.initializePublicDownloadList(publicList);
        } catch (ex) {
          Cu.reportError(ex);
        }

        let publicSummary = await this.getSummary(Downloads.PUBLIC);
        let privateSummary = await this.getSummary(Downloads.PRIVATE);
        let combinedSummary = await this.getSummary(Downloads.ALL);

        await publicSummary.bindToList(publicList);
        await privateSummary.bindToList(privateList);
        await combinedSummary.bindToList(combinedList);

        this._lists[Downloads.PUBLIC] = publicList;
        this._lists[Downloads.PRIVATE] = privateList;
        this._lists[Downloads.ALL] = combinedList;
      })();
    }

    return this._promiseListsInitialized.then(() => this._lists[aType]);
  },

  /**
   * Promise resolved when the initialization of the download lists has
   * completed, or null if initialization has never been requested.
   */
  _promiseListsInitialized: null,

  /**
   * After initialization, this object is populated with one key for each type
   * of download list that can be returned (Downloads.PUBLIC, Downloads.PRIVATE,
   * or Downloads.ALL).  The values are the DownloadList objects.
   */
  _lists: {},

  /**
   * Retrieves the specified type of DownloadSummary object.  There is one
   * download summary for each type, and this method always retrieves a
   * reference to the same download summary when called with the same argument.
   *
   * Calling this function does not cause the list of public downloads to be
   * reloaded from the previous session.  The summary will behave as if no
   * downloads are present until the getList method is called.
   *
   * @param aType
   *        This can be Downloads.PUBLIC, Downloads.PRIVATE, or Downloads.ALL.
   *
   * @return {Promise}
   * @resolves The requested DownloadList or DownloadCombinedList object.
   * @rejects JavaScript exception.
   */
  getSummary(aType) {
    if (
      aType != Downloads.PUBLIC &&
      aType != Downloads.PRIVATE &&
      aType != Downloads.ALL
    ) {
      throw new Error("Invalid aType argument.");
    }

    if (!(aType in this._summaries)) {
      this._summaries[aType] = new DownloadSummary();
    }

    return Promise.resolve(this._summaries[aType]);
  },

  /**
   * This object is populated by the getSummary method with one key for each
   * type of object that can be returned (Downloads.PUBLIC, Downloads.PRIVATE,
   * or Downloads.ALL).  The values are the DownloadSummary objects.
   */
  _summaries: {},

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
   * @resolves The downloads directory string path.
   */
  getSystemDownloadsDirectory: function D_getSystemDownloadsDirectory() {
    return DownloadIntegration.getSystemDownloadsDirectory();
  },

  /**
   * Returns the preferred downloads directory based on the user preferences
   * in the current profile asynchronously.
   *
   * @return {Promise}
   * @resolves The downloads directory string path.
   */
  getPreferredDownloadsDirectory: function D_getPreferredDownloadsDirectory() {
    return DownloadIntegration.getPreferredDownloadsDirectory();
  },

  /**
   * Returns the temporary directory where downloads are placed before the
   * final location is chosen, or while the document is opened temporarily
   * with an external application. This may or may not be the system temporary
   * directory, based on the platform asynchronously.
   *
   * @return {Promise}
   * @resolves The downloads directory string path.
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
