/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Main entry point to get references to all the back-end objects.
 */

import { Integration } from "resource://gre/modules/Integration.sys.mjs";

import {
  Download,
  DownloadError,
} from "resource://gre/modules/DownloadCore.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  DownloadCombinedList: "resource://gre/modules/DownloadList.sys.mjs",
  DownloadList: "resource://gre/modules/DownloadList.sys.mjs",
  DownloadSummary: "resource://gre/modules/DownloadList.sys.mjs",
});

Integration.downloads.defineESModuleGetter(
  lazy,
  "DownloadIntegration",
  "resource://gre/modules/DownloadIntegration.sys.mjs"
);

/**
 * This object is exposed directly to the consumers of this JavaScript module,
 * and provides the only entry point to get references to back-end objects.
 */
export const Downloads = {
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
   * @param properties
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
  async createDownload(properties) {
    return Download.fromSerializable(properties);
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
   * @param source
   *        String containing the URI for the download source.  Alternatively,
   *        may be an nsIURI or a DownloadSource object.
   * @param target
   *        String containing the path of the target file.  Alternatively, may
   *        be an nsIFile or a DownloadTarget object.
   * @param options
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
  async fetch(source, target, options) {
    const download = await this.createDownload({ source, target });

    if (options?.isPrivate) {
      download.source.isPrivate = options.isPrivate;
    }
    return download.start();
  },

  /**
   * Retrieves the specified type of DownloadList object.  There is one download
   * list for each type, and this method always retrieves a reference to the
   * same download list when called with the same argument.
   *
   * Calling this function may cause the list of public downloads to be reloaded
   * from the previous session, if it wasn't loaded already.
   *
   * @param type
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
  async getList(type) {
    if (!this._promiseListsInitialized) {
      this._promiseListsInitialized = (async () => {
        let publicList = new lazy.DownloadList();
        let privateList = new lazy.DownloadList();
        let combinedList = new lazy.DownloadCombinedList(
          publicList,
          privateList
        );

        try {
          await lazy.DownloadIntegration.addListObservers(publicList, false);
          await lazy.DownloadIntegration.addListObservers(privateList, true);
          await lazy.DownloadIntegration.initializePublicDownloadList(
            publicList
          );
        } catch (err) {
          console.error(err);
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

    await this._promiseListsInitialized;

    return this._lists[type];
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
   * @param type
   *        This can be Downloads.PUBLIC, Downloads.PRIVATE, or Downloads.ALL.
   *
   * @return {Promise}
   * @resolves The requested DownloadList or DownloadCombinedList object.
   * @rejects JavaScript exception.
   */
  async getSummary(type) {
    if (
      type != Downloads.PUBLIC &&
      type != Downloads.PRIVATE &&
      type != Downloads.ALL
    ) {
      throw new Error("Invalid type argument.");
    }

    if (!(type in this._summaries)) {
      this._summaries[type] = new lazy.DownloadSummary();
    }

    return this._summaries[type];
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
  getSystemDownloadsDirectory() {
    return lazy.DownloadIntegration.getSystemDownloadsDirectory();
  },

  /**
   * Returns the preferred downloads directory based on the user preferences
   * in the current profile asynchronously.
   *
   * @return {Promise}
   * @resolves The downloads directory string path.
   */
  getPreferredDownloadsDirectory() {
    return lazy.DownloadIntegration.getPreferredDownloadsDirectory();
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
  getTemporaryDownloadsDirectory() {
    return lazy.DownloadIntegration.getTemporaryDownloadsDirectory();
  },

  /**
   * Constructor for a DownloadError object.  When you catch an exception during
   * a download, you can use this to verify if "ex instanceof Downloads.Error",
   * before reading the exception properties with the error details.
   */
  Error: DownloadError,
};
