/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides collections of Download objects and aggregate views on them.
 */

const FILE_EXTENSIONS = [
  "aac",
  "adt",
  "adts",
  "accdb",
  "accde",
  "accdr",
  "accdt",
  "aif",
  "aifc",
  "aiff",
  "apng",
  "aspx",
  "avi",
  "avif",
  "bat",
  "bin",
  "bmp",
  "cab",
  "cda",
  "csv",
  "dif",
  "dll",
  "doc",
  "docm",
  "docx",
  "dot",
  "dotx",
  "eml",
  "eps",
  "exe",
  "flac",
  "flv",
  "gif",
  "htm",
  "html",
  "ico",
  "ini",
  "iso",
  "jar",
  "jfif",
  "jpg",
  "jpeg",
  "json",
  "m4a",
  "mdb",
  "mid",
  "midi",
  "mov",
  "mp3",
  "mp4",
  "mpeg",
  "mpg",
  "msi",
  "mui",
  "oga",
  "ogg",
  "ogv",
  "opus",
  "pdf",
  "pjpeg",
  "pjp",
  "png",
  "pot",
  "potm",
  "potx",
  "ppam",
  "pps",
  "ppsm",
  "ppsx",
  "ppt",
  "pptm",
  "pptx",
  "psd",
  "pst",
  "pub",
  "rar",
  "rdf",
  "rtf",
  "shtml",
  "sldm",
  "sldx",
  "svg",
  "swf",
  "sys",
  "tif",
  "tiff",
  "tmp",
  "txt",
  "vob",
  "vsd",
  "vsdm",
  "vsdx",
  "vss",
  "vssm",
  "vst",
  "vstm",
  "vstx",
  "wav",
  "wbk",
  "webm",
  "webp",
  "wks",
  "wma",
  "wmd",
  "wmv",
  "wmz",
  "wms",
  "wpd",
  "wp5",
  "xht",
  "xhtml",
  "xla",
  "xlam",
  "xll",
  "xlm",
  "xls",
  "xlsm",
  "xlsx",
  "xlt",
  "xltm",
  "xltx",
  "xml",
  "zip",
];

const TELEMETRY_EVENT_CATEGORY = "downloads";

/**
 * Represents a collection of Download objects that can be viewed and managed by
 * the user interface, and persisted across sessions.
 */
export class DownloadList {
  constructor() {
    /**
     * Array of Download objects currently in the list.
     */
    this._downloads = [];

    /**
     * Set of currently registered views.
     */
    this._views = new Set();
  }

  /**
   * Retrieves a snapshot of the downloads that are currently in the list.  The
   * returned array does not change when downloads are added or removed, though
   * the Download objects it contains are still updated in real time.
   *
   * @return {Promise}
   * @resolves An array of Download objects.
   * @rejects JavaScript exception.
   */
  async getAll() {
    return Array.from(this._downloads);
  }

  /**
   * Adds a new download to the end of the items list.
   *
   * @note When a download is added to the list, its "onchange" event is
   *       registered by the list, thus it cannot be used to monitor the
   *       download.  To receive change notifications for downloads that are
   *       added to the list, use the addView method to register for
   *       onDownloadChanged notifications.
   *
   * @param download
   *        The Download object to add.
   *
   * @return {Promise}
   * @resolves When the download has been added.
   * @rejects JavaScript exception.
   */
  async add(download) {
    this._downloads.push(download);
    download.onchange = this._change.bind(this, download);
    this._notifyAllViews("onDownloadAdded", download);
  }

  /**
   * Removes a download from the list.  If the download was already removed,
   * this method has no effect.
   *
   * This method does not change the state of the download, to allow adding it
   * to another list, or control it directly.  If you want to dispose of the
   * download object, you should cancel it afterwards, and remove any partially
   * downloaded data if needed.
   *
   * @param download
   *        The Download object to remove.
   *
   * @return {Promise}
   * @resolves When the download has been removed.
   * @rejects JavaScript exception.
   */
  async remove(download) {
    let index = this._downloads.indexOf(download);
    if (index != -1) {
      this._downloads.splice(index, 1);
      download.onchange = null;
      this._notifyAllViews("onDownloadRemoved", download);
    }
  }

  /**
   * This function is called when "onchange" events of downloads occur.
   *
   * @param download
   *        The Download object that changed.
   */
  _change(download) {
    this._notifyAllViews("onDownloadChanged", download);
  }

  /**
   * Adds a view that will be notified of changes to downloads.  The newly added
   * view will receive onDownloadAdded notifications for all the downloads that
   * are already in the list.
   *
   * @param view
   *        The view object to add.  The following methods may be defined:
   *        {
   *          onDownloadAdded: function (download) {
   *            // Called after download is added to the end of the list.
   *          },
   *          onDownloadChanged: function (download) {
   *            // Called after the properties of download change.
   *          },
   *          onDownloadRemoved: function (download) {
   *            // Called after download is removed from the list.
   *          },
   *          onDownloadBatchStarting: function () {
   *            // Called before multiple changes are made at the same time.
   *          },
   *          onDownloadBatchEnded: function () {
   *            // Called after all the changes have been made.
   *          },
   *        }
   *
   * @return {Promise}
   * @resolves When the view has been registered and all the onDownloadAdded
   *           notifications for the existing downloads have been sent.
   * @rejects JavaScript exception.
   */
  async addView(view) {
    this._views.add(view);

    if ("onDownloadAdded" in view) {
      this._notifyAllViews("onDownloadBatchStarting");
      for (let download of this._downloads) {
        try {
          view.onDownloadAdded(download);
        } catch (ex) {
          console.error(ex);
        }
      }
      this._notifyAllViews("onDownloadBatchEnded");
    }
  }

  /**
   * Removes a view that was previously added using addView.
   *
   * @param view
   *        The view object to remove.
   *
   * @return {Promise}
   * @resolves When the view has been removed.  At this point, the removed view
   *           will not receive any more notifications.
   * @rejects JavaScript exception.
   */
  async removeView(view) {
    this._views.delete(view);
  }

  /**
   * Notifies all the views of a download addition, change, removal, or other
   * event. The additional arguments are passed to the called method.
   *
   * @param methodName
   *        String containing the name of the method to call on the view.
   */
  _notifyAllViews(methodName, ...args) {
    for (let view of this._views) {
      try {
        if (methodName in view) {
          view[methodName](...args);
        }
      } catch (ex) {
        console.error(ex);
      }
    }
  }

  /**
   * Removes downloads from the list that have finished, have failed, or have
   * been canceled without keeping partial data.  A filter function may be
   * specified to remove only a subset of those downloads.
   *
   * This method finalizes each removed download, ensuring that any partially
   * downloaded data associated with it is also removed.
   *
   * @param filterFn
   *        The filter function is called with each download as its only
   *        argument, and should return true to remove the download and false
   *        to keep it.  This parameter may be null or omitted to have no
   *        additional filter.
   */
  removeFinished(filterFn) {
    (async () => {
      let list = await this.getAll();
      for (let download of list) {
        // Remove downloads that have been canceled, even if the cancellation
        // operation hasn't completed yet so we don't check "stopped" here.
        // Failed downloads with partial data are also removed.
        if (
          download.stopped &&
          (!download.hasPartialData || download.error) &&
          (!filterFn || filterFn(download))
        ) {
          // Remove the download first, so that the views don't get the change
          // notifications that may occur during finalization.
          await this.remove(download);
          // Find if a file with the same path is also downloading.
          let sameFileIsDownloading = false;
          for (let otherDownload of await this.getAll()) {
            if (
              download !== otherDownload &&
              download.target.path == otherDownload.target.path &&
              !otherDownload.error
            ) {
              sameFileIsDownloading = true;
            }
          }
          // Ensure that the download is stopped and no partial data is kept.
          // This works even if the download state has changed meanwhile.  We
          // don't need to wait for the procedure to be complete before
          // processing the other downloads in the list.
          let removePartialData = !sameFileIsDownloading;
          download.finalize(removePartialData).catch(console.error);
        }
      }
    })().catch(console.error);
  }
}

/**
 * Provides a unified, unordered list combining public and private downloads.
 *
 * Download objects added to this list are also added to one of the two
 * underlying lists, based on their "source.isPrivate" property.  Views on this
 * list will receive notifications for both public and private downloads.
 *
 * @param publicList
 *        Underlying DownloadList containing public downloads.
 * @param privateList
 *        Underlying DownloadList containing private downloads.
 */
export class DownloadCombinedList extends DownloadList {
  constructor(publicList, privateList) {
    super();

    /**
     * Underlying DownloadList containing public downloads.
     */
    this._publicList = publicList;

    /**
     * Underlying DownloadList containing private downloads.
     */
    this._privateList = privateList;

    publicList.addView(this).catch(console.error);
    privateList.addView(this).catch(console.error);
  }

  /**
   * Adds a new download to the end of the items list.
   *
   * @note When a download is added to the list, its "onchange" event is
   *       registered by the list, thus it cannot be used to monitor the
   *       download.  To receive change notifications for downloads that are
   *       added to the list, use the addView method to register for
   *       onDownloadChanged notifications.
   *
   * @param download
   *        The Download object to add.
   *
   * @return {Promise}
   * @resolves When the download has been added.
   * @rejects JavaScript exception.
   */
  add(download) {
    let extension = download.target.path.split(".").pop();

    if (!FILE_EXTENSIONS.includes(extension)) {
      extension = "other";
    }

    try {
      Services.telemetry.recordEvent(
        TELEMETRY_EVENT_CATEGORY,
        "added",
        "fileExtension",
        extension,
        {}
      );
    } catch (ex) {
      console.error(
        "DownloadsCommon: error recording telemetry event.",
        ex.message
      );
    }

    if (download.source.isPrivate) {
      return this._privateList.add(download);
    }

    return this._publicList.add(download);
  }

  /**
   * Removes a download from the list.  If the download was already removed,
   * this method has no effect.
   *
   * This method does not change the state of the download, to allow adding it
   * to another list, or control it directly.  If you want to dispose of the
   * download object, you should cancel it afterwards, and remove any partially
   * downloaded data if needed.
   *
   * @param download
   *        The Download object to remove.
   *
   * @return {Promise}
   * @resolves When the download has been removed.
   * @rejects JavaScript exception.
   */
  remove(download) {
    if (download.source.isPrivate) {
      return this._privateList.remove(download);
    }
    return this._publicList.remove(download);
  }

  // DownloadList callback
  onDownloadAdded(download) {
    this._downloads.push(download);
    this._notifyAllViews("onDownloadAdded", download);
  }

  // DownloadList callback
  onDownloadChanged(download) {
    this._notifyAllViews("onDownloadChanged", download);
  }

  // DownloadList callback
  onDownloadRemoved(download) {
    let index = this._downloads.indexOf(download);
    if (index != -1) {
      this._downloads.splice(index, 1);
    }
    this._notifyAllViews("onDownloadRemoved", download);
  }
}
/**
 * Provides an aggregated view on the contents of a DownloadList.
 */
export class DownloadSummary {
  constructor() {
    /**
     * Array of Download objects that are currently part of the summary.
     */
    this._downloads = [];

    /**
     * Set of currently registered views.
     */
    this._views = new Set();
  }

  /**
   * Underlying DownloadList whose contents should be summarized.
   */
  _list = null;

  /**
   * Indicates whether all the downloads are currently stopped.
   */
  allHaveStopped = true;

  /**
   * Indicates whether whether all downloads have an unknown final size.
   */
  allUnknownSize = true;

  /**
   * Indicates the total number of bytes to be transferred before completing all
   * the downloads that are currently in progress.
   *
   * For downloads that do not have a known final size, the number of bytes
   * currently transferred is reported as part of this property.
   *
   * This is zero if no downloads are currently in progress.
   */
  progressTotalBytes = 0;

  /**
   * Number of bytes currently transferred as part of all the downloads that are
   * currently in progress.
   *
   * This is zero if no downloads are currently in progress.
   */
  progressCurrentBytes = 0;

  /**
   * This method may be called once to bind this object to a DownloadList.
   *
   * Views on the summarized data can be registered before this object is bound
   * to an actual list.  This allows the summary to be used without requiring
   * the initialization of the DownloadList first.
   *
   * @param list
   *        Underlying DownloadList whose contents should be summarized.
   *
   * @return {Promise}
   * @resolves When the view on the underlying list has been registered.
   * @rejects JavaScript exception.
   */
  async bindToList(list) {
    if (this._list) {
      throw new Error("bindToList may be called only once.");
    }

    await list.addView(this);
    // Set the list reference only after addView has returned, so that we don't
    // send a notification to our views for each download that is added.
    this._list = list;
    this._onListChanged();
  }

  /**
   * Adds a view that will be notified of changes to the summary.  The newly
   * added view will receive an initial onSummaryChanged notification.
   *
   * @param view
   *        The view object to add.  The following methods may be defined:
   *        {
   *          onSummaryChanged: function () {
   *            // Called after any property of the summary has changed.
   *          },
   *        }
   *
   * @return {Promise}
   * @resolves When the view has been registered and the onSummaryChanged
   *           notification has been sent.
   * @rejects JavaScript exception.
   */
  async addView(view) {
    this._views.add(view);

    if ("onSummaryChanged" in view) {
      try {
        view.onSummaryChanged();
      } catch (ex) {
        console.error(ex);
      }
    }
  }

  /**
   * Removes a view that was previously added using addView.
   *
   * @param view
   *        The view object to remove.
   *
   * @return {Promise}
   * @resolves When the view has been removed.  At this point, the removed view
   *           will not receive any more notifications.
   * @rejects JavaScript exception.
   */
  async removeView(view) {
    this._views.delete(view);
  }

  /**
   * This function is called when any change in the list of downloads occurs,
   * and will recalculate the summary and notify the views in case the
   * aggregated properties are different.
   */
  _onListChanged() {
    let allHaveStopped = true;
    let allUnknownSize = true;
    let progressTotalBytes = 0;
    let progressCurrentBytes = 0;

    // Recalculate the aggregated state.  See the description of the individual
    // properties for an explanation of the summarization logic.
    for (let download of this._downloads) {
      if (!download.stopped) {
        allHaveStopped = false;
        if (download.hasProgress) {
          allUnknownSize = false;
          progressTotalBytes += download.totalBytes;
        } else {
          progressTotalBytes += download.currentBytes;
        }
        progressCurrentBytes += download.currentBytes;
      }
    }

    // Exit now if the properties did not change.
    if (
      this.allHaveStopped == allHaveStopped &&
      this.allUnknownSize == allUnknownSize &&
      this.progressTotalBytes == progressTotalBytes &&
      this.progressCurrentBytes == progressCurrentBytes
    ) {
      return;
    }

    this.allHaveStopped = allHaveStopped;
    this.allUnknownSize = allUnknownSize;
    this.progressTotalBytes = progressTotalBytes;
    this.progressCurrentBytes = progressCurrentBytes;

    // Notify all the views that our properties changed.
    for (let view of this._views) {
      try {
        if ("onSummaryChanged" in view) {
          view.onSummaryChanged();
        }
      } catch (ex) {
        console.error(ex);
      }
    }
  }

  // DownloadList callback
  onDownloadAdded(download) {
    this._downloads.push(download);
    if (this._list) {
      this._onListChanged();
    }
  }

  // DownloadList callback
  onDownloadChanged(download) {
    this._onListChanged();
  }

  // DownloadList callback
  onDownloadRemoved(download) {
    let index = this._downloads.indexOf(download);
    if (index != -1) {
      this._downloads.splice(index, 1);
    }
    this._onListChanged();
  }
}
