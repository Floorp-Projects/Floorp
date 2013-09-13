/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file includes the following constructors and global objects:
 *
 * DownloadList
 * Represents a collection of Download objects that can be viewed and managed by
 * the user interface, and persisted across sessions.
 *
 * DownloadCombinedList
 * Provides a unified, unordered list combining public and private downloads.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "DownloadList",
  "DownloadCombinedList",
];

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/commonjs/sdk/core/promise.js");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

////////////////////////////////////////////////////////////////////////////////
//// DownloadList

/**
 * Represents a collection of Download objects that can be viewed and managed by
 * the user interface, and persisted across sessions.
 */
function DownloadList() {
  this._downloads = [];
  this._views = new Set();
}

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
    return Promise.resolve(Array.slice(this._downloads, 0));
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
   */
  add: function DL_add(aDownload) {
    this._downloads.push(aDownload);
    aDownload.onchange = this._change.bind(this, aDownload);
    this._notifyAllViews("onDownloadAdded", aDownload);
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
   */
  remove: function DL_remove(aDownload) {
    let index = this._downloads.indexOf(aDownload);
    if (index != -1) {
      this._downloads.splice(index, 1);
      aDownload.onchange = null;
      this._notifyAllViews("onDownloadRemoved", aDownload);
    }
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
   *        }
   *
   * @note The onDownloadAdded notifications are sent synchronously.  This
   *       allows for a complete initialization of the view used for detecting
   *       changes to downloads to be persisted, before other callers get a
   *       chance to modify them.
   */
  addView: function DL_addView(aView)
  {
    this._views.add(aView);

    if ("onDownloadAdded" in aView) {
      for (let download of this._downloads) {
        try {
          aView.onDownloadAdded(download);
        } catch (ex) {
          Cu.reportError(ex);
        }
      }
    }
  },

  /**
   * Removes a view that was previously added using addView.  The removed view
   * will not receive any more notifications after this method returns.
   *
   * @param aView
   *        The view object to remove.
   */
  removeView: function DL_removeView(aView)
  {
    this._views.delete(aView);
  },

  /**
   * Notifies all the views of a download addition, change, or removal.
   *
   * @param aMethodName
   *        String containing the name of the method to call on the view.
   * @param aDownload
   *        The Download object that changed.
   */
  _notifyAllViews: function (aMethodName, aDownload) {
    for (let view of this._views) {
      try {
        if (aMethodName in view) {
          view[aMethodName](aDownload);
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
    Task.spawn(function() {
      let list = yield this.getAll();
      for (let download of list) {
        // Remove downloads that have been canceled, even if the cancellation
        // operation hasn't completed yet so we don't check "stopped" here.
        // Failed downloads with partial data are also removed.
        if (download.stopped && (!download.hasPartialData || download.error) &&
            (!aFilterFn || aFilterFn(download))) {
          // Remove the download first, so that the views don't get the change
          // notifications that may occur during finalization.
          this.remove(download);
          // Ensure that the download is stopped and no partial data is kept.
          // This works even if the download state has changed meanwhile.  We
          // don't need to wait for the procedure to be complete before
          // processing the other downloads in the list.
          download.finalize(true).then(null, Cu.reportError);
        }
      }
    }.bind(this)).then(null, Cu.reportError);
  },
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadCombinedList

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
function DownloadCombinedList(aPublicList, aPrivateList)
{
  DownloadList.call(this);
  this._publicList = aPublicList;
  this._privateList = aPrivateList;
  aPublicList.addView(this);
  aPrivateList.addView(this);
}

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
   */
  add: function (aDownload)
  {
    if (aDownload.source.isPrivate) {
      this._privateList.add(aDownload);
    } else {
      this._publicList.add(aDownload);
    }
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
   */
  remove: function (aDownload)
  {
    if (aDownload.source.isPrivate) {
      this._privateList.remove(aDownload);
    } else {
      this._publicList.remove(aDownload);
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// DownloadList view

  onDownloadAdded: function (aDownload)
  {
    this._downloads.push(aDownload);
    this._notifyAllViews("onDownloadAdded", aDownload);
  },

  onDownloadChanged: function (aDownload)
  {
    this._notifyAllViews("onDownloadChanged", aDownload);
  },

  onDownloadRemoved: function (aDownload)
  {
    let index = this._downloads.indexOf(aDownload);
    if (index != -1) {
      this._downloads.splice(index, 1);
    }
    this._notifyAllViews("onDownloadRemoved", aDownload);
  },
};
