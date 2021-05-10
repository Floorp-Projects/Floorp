/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides collections of Download objects and aggregate views on them.
 */

"use strict";

var EXPORTED_SYMBOLS = [
  "DownloadList",
  "DownloadCombinedList",
  "DownloadSummary",
];

/**
 * Represents a collection of Download objects that can be viewed and managed by
 * the user interface, and persisted across sessions.
 */
var DownloadList = function() {
  this._downloads = [];
  this._views = new Set();
};

DownloadList.prototype = {
  /**
   * Array of Download objects currently in the list.
   */
  _downloads: null,

  /**
   * Retrieves a snapshot of the downloads that are currently in the list.  The
   * returned array does not change when downloads are added or removed, though
   * the Download objects it contains are still updated in real time.
   *
   * @return {Promise}
   * @resolves An array of Download objects.
   * @rejects JavaScript exception.
   */
  getAll: function DL_getAll() {
    return Promise.resolve(Array.from(this._downloads));
  },

  /**
   * Adds a new download to the end of the items list.
   *
   * @note When a download is added to the list, its "onchange" event is
   *       registered by the list, thus it cannot be used to monitor the
   *       download.  To receive change notifications for downloads that are
   *       added to the list, use the addView method to register for
   *       onDownloadChanged notifications.
   *
   * @param aDownload
   *        The Download object to add.
   *
   * @return {Promise}
   * @resolves When the download has been added.
   * @rejects JavaScript exception.
   */
  add: function DL_add(aDownload) {
    this._downloads.push(aDownload);
    aDownload.onchange = this._change.bind(this, aDownload);
    this._notifyAllViews("onDownloadAdded", aDownload);

    return Promise.resolve();
  },

  /**
   * Removes a download from the list.  If the download was already removed,
   * this method has no effect.
   *
   * This method does not change the state of the download, to allow adding it
   * to another list, or control it directly.  If you want to dispose of the
   * download object, you should cancel it afterwards, and remove any partially
   * downloaded data if needed.
   *
   * @param aDownload
   *        The Download object to remove.
   *
   * @return {Promise}
   * @resolves When the download has been removed.
   * @rejects JavaScript exception.
   */
  remove: function DL_remove(aDownload) {
    let index = this._downloads.indexOf(aDownload);
    if (index != -1) {
      this._downloads.splice(index, 1);
      aDownload.onchange = null;
      this._notifyAllViews("onDownloadRemoved", aDownload);
    }

    return Promise.resolve();
  },

  /**
   * This function is called when "onchange" events of downloads occur.
   *
   * @param aDownload
   *        The Download object that changed.
   */
  _change: function DL_change(aDownload) {
    this._notifyAllViews("onDownloadChanged", aDownload);
  },

  /**
   * Set of currently registered views.
   */
  _views: null,

  /**
   * Adds a view that will be notified of changes to downloads.  The newly added
   * view will receive onDownloadAdded notifications for all the downloads that
   * are already in the list.
   *
   * @param aView
   *        The view object to add.  The following methods may be defined:
   *        {
   *          onDownloadAdded: function (aDownload) {
   *            // Called after aDownload is added to the end of the list.
   *          },
   *          onDownloadChanged: function (aDownload) {
   *            // Called after the properties of aDownload change.
   *          },
   *          onDownloadRemoved: function (aDownload) {
   *            // Called after aDownload is removed from the list.
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
  addView: function DL_addView(aView) {
    this._views.add(aView);

    if ("onDownloadAdded" in aView) {
      this._notifyAllViews("onDownloadBatchStarting");
      for (let download of this._downloads) {
        try {
          aView.onDownloadAdded(download);
        } catch (ex) {
          Cu.reportError(ex);
        }
      }
      this._notifyAllViews("onDownloadBatchEnded");
    }

    return Promise.resolve();
  },

  /**
   * Removes a view that was previously added using addView.
   *
   * @param aView
   *        The view object to remove.
   *
   * @return {Promise}
   * @resolves When the view has been removed.  At this point, the removed view
   *           will not receive any more notifications.
   * @rejects JavaScript exception.
   */
  removeView: function DL_removeView(aView) {
    this._views.delete(aView);

    return Promise.resolve();
  },

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
        Cu.reportError(ex);
      }
    }
  },

  /**
   * Removes downloads from the list that have finished, have failed, or have
   * been canceled without keeping partial data.  A filter function may be
   * specified to remove only a subset of those downloads.
   *
   * This method finalizes each removed download, ensuring that any partially
   * downloaded data associated with it is also removed.
   *
   * @param aFilterFn
   *        The filter function is called with each download as its only
   *        argument, and should return true to remove the download and false
   *        to keep it.  This parameter may be null or omitted to have no
   *        additional filter.
   */
  removeFinished: function DL_removeFinished(aFilterFn) {
    (async () => {
      let list = await this.getAll();
      for (let download of list) {
        // Remove downloads that have been canceled, even if the cancellation
        // operation hasn't completed yet so we don't check "stopped" here.
        // Failed downloads with partial data are also removed.
        if (
          download.stopped &&
          (!download.hasPartialData || download.error) &&
          (!aFilterFn || aFilterFn(download))
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
          download.finalize(removePartialData).catch(Cu.reportError);
        }
      }
    })().catch(Cu.reportError);
  },
};

/**
 * Provides a unified, unordered list combining public and private downloads.
 *
 * Download objects added to this list are also added to one of the two
 * underlying lists, based on their "source.isPrivate" property.  Views on this
 * list will receive notifications for both public and private downloads.
 *
 * @param aPublicList
 *        Underlying DownloadList containing public downloads.
 * @param aPrivateList
 *        Underlying DownloadList containing private downloads.
 */
var DownloadCombinedList = function(aPublicList, aPrivateList) {
  DownloadList.call(this);
  this._publicList = aPublicList;
  this._privateList = aPrivateList;
  aPublicList.addView(this).catch(Cu.reportError);
  aPrivateList.addView(this).catch(Cu.reportError);
};

DownloadCombinedList.prototype = {
  __proto__: DownloadList.prototype,

  /**
   * Underlying DownloadList containing public downloads.
   */
  _publicList: null,

  /**
   * Underlying DownloadList containing private downloads.
   */
  _privateList: null,

  /**
   * Adds a new download to the end of the items list.
   *
   * @note When a download is added to the list, its "onchange" event is
   *       registered by the list, thus it cannot be used to monitor the
   *       download.  To receive change notifications for downloads that are
   *       added to the list, use the addView method to register for
   *       onDownloadChanged notifications.
   *
   * @param aDownload
   *        The Download object to add.
   *
   * @return {Promise}
   * @resolves When the download has been added.
   * @rejects JavaScript exception.
   */
  add(aDownload) {
    if (aDownload.source.isPrivate) {
      return this._privateList.add(aDownload);
    }
    return this._publicList.add(aDownload);
  },

  /**
   * Removes a download from the list.  If the download was already removed,
   * this method has no effect.
   *
   * This method does not change the state of the download, to allow adding it
   * to another list, or control it directly.  If you want to dispose of the
   * download object, you should cancel it afterwards, and remove any partially
   * downloaded data if needed.
   *
   * @param aDownload
   *        The Download object to remove.
   *
   * @return {Promise}
   * @resolves When the download has been removed.
   * @rejects JavaScript exception.
   */
  remove(aDownload) {
    if (aDownload.source.isPrivate) {
      return this._privateList.remove(aDownload);
    }
    return this._publicList.remove(aDownload);
  },

  // DownloadList callback
  onDownloadAdded(aDownload) {
    this._downloads.push(aDownload);
    this._notifyAllViews("onDownloadAdded", aDownload);
  },

  // DownloadList callback
  onDownloadChanged(aDownload) {
    this._notifyAllViews("onDownloadChanged", aDownload);
  },

  // DownloadList callback
  onDownloadRemoved(aDownload) {
    let index = this._downloads.indexOf(aDownload);
    if (index != -1) {
      this._downloads.splice(index, 1);
    }
    this._notifyAllViews("onDownloadRemoved", aDownload);
  },
};

/**
 * Provides an aggregated view on the contents of a DownloadList.
 */
var DownloadSummary = function() {
  this._downloads = [];
  this._views = new Set();
};

DownloadSummary.prototype = {
  /**
   * Array of Download objects that are currently part of the summary.
   */
  _downloads: null,

  /**
   * Underlying DownloadList whose contents should be summarized.
   */
  _list: null,

  /**
   * This method may be called once to bind this object to a DownloadList.
   *
   * Views on the summarized data can be registered before this object is bound
   * to an actual list.  This allows the summary to be used without requiring
   * the initialization of the DownloadList first.
   *
   * @param aList
   *        Underlying DownloadList whose contents should be summarized.
   *
   * @return {Promise}
   * @resolves When the view on the underlying list has been registered.
   * @rejects JavaScript exception.
   */
  bindToList(aList) {
    if (this._list) {
      throw new Error("bindToList may be called only once.");
    }

    return aList.addView(this).then(() => {
      // Set the list reference only after addView has returned, so that we don't
      // send a notification to our views for each download that is added.
      this._list = aList;
      this._onListChanged();
    });
  },

  /**
   * Set of currently registered views.
   */
  _views: null,

  /**
   * Adds a view that will be notified of changes to the summary.  The newly
   * added view will receive an initial onSummaryChanged notification.
   *
   * @param aView
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
  addView(aView) {
    this._views.add(aView);

    if ("onSummaryChanged" in aView) {
      try {
        aView.onSummaryChanged();
      } catch (ex) {
        Cu.reportError(ex);
      }
    }

    return Promise.resolve();
  },

  /**
   * Removes a view that was previously added using addView.
   *
   * @param aView
   *        The view object to remove.
   *
   * @return {Promise}
   * @resolves When the view has been removed.  At this point, the removed view
   *           will not receive any more notifications.
   * @rejects JavaScript exception.
   */
  removeView(aView) {
    this._views.delete(aView);

    return Promise.resolve();
  },

  /**
   * Indicates whether all the downloads are currently stopped.
   */
  allHaveStopped: true,

  /**
   * Indicates whether whether all downloads have an unknown final size.
   */
  allUnknownSize: true,

  /**
   * Indicates the total number of bytes to be transferred before completing all
   * the downloads that are currently in progress.
   *
   * For downloads that do not have a known final size, the number of bytes
   * currently transferred is reported as part of this property.
   *
   * This is zero if no downloads are currently in progress.
   */
  progressTotalBytes: 0,

  /**
   * Number of bytes currently transferred as part of all the downloads that are
   * currently in progress.
   *
   * This is zero if no downloads are currently in progress.
   */
  progressCurrentBytes: 0,

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
        Cu.reportError(ex);
      }
    }
  },

  // DownloadList callback
  onDownloadAdded(aDownload) {
    this._downloads.push(aDownload);
    if (this._list) {
      this._onListChanged();
    }
  },

  // DownloadList callback
  onDownloadChanged(aDownload) {
    this._onListChanged();
  },

  // DownloadList callback
  onDownloadRemoved(aDownload) {
    let index = this._downloads.indexOf(aDownload);
    if (index != -1) {
      this._downloads.splice(index, 1);
    }
    this._onListChanged();
  },
};
