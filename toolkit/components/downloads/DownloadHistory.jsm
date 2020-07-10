/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides access to downloads from previous sessions on platforms that store
 * them in a different location than session downloads.
 *
 * This module works with objects that are compatible with Download, while using
 * the Places interfaces internally. Some of the Places objects may also be
 * exposed to allow the consumers to integrate with history view commands.
 */

"use strict";

var EXPORTED_SYMBOLS = ["DownloadHistory"];

const { DownloadList } = ChromeUtils.import(
  "resource://gre/modules/DownloadList.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Downloads: "resource://gre/modules/Downloads.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

// Places query used to retrieve all history downloads for the related list.
const HISTORY_PLACES_QUERY =
  "place:transition=" +
  Ci.nsINavHistoryService.TRANSITION_DOWNLOAD +
  "&sort=" +
  Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING;

const DESTINATIONFILEURI_ANNO = "downloads/destinationFileURI";
const METADATA_ANNO = "downloads/metaData";

const METADATA_STATE_FINISHED = 1;
const METADATA_STATE_FAILED = 2;
const METADATA_STATE_CANCELED = 3;
const METADATA_STATE_PAUSED = 4;
const METADATA_STATE_BLOCKED_PARENTAL = 6;
const METADATA_STATE_DIRTY = 8;

/**
 * Provides methods to retrieve downloads from previous sessions and store
 * downloads for future sessions.
 */
var DownloadHistory = {
  /**
   * Retrieves the main DownloadHistoryList object which provides a unified view
   * on downloads from both previous browsing sessions and this session.
   *
   * @param type
   *        Determines which type of downloads from this session should be
   *        included in the list. This is Downloads.PUBLIC by default, but can
   *        also be Downloads.PRIVATE or Downloads.ALL.
   * @param maxHistoryResults
   *        Optional number that limits the amount of results the history query
   *        may return.
   *
   * @return {Promise}
   * @resolves The requested DownloadHistoryList object.
   * @rejects JavaScript exception.
   */
  async getList({ type = Downloads.PUBLIC, maxHistoryResults } = {}) {
    await DownloadCache.ensureInitialized();

    let key = `${type}|${maxHistoryResults ? maxHistoryResults : -1}`;
    if (!this._listPromises[key]) {
      this._listPromises[key] = Downloads.getList(type).then(list => {
        // When the amount of history downloads is capped, we request the list in
        // descending order, to make sure that the list can apply the limit.
        let query =
          HISTORY_PLACES_QUERY +
          (maxHistoryResults ? "&maxResults=" + maxHistoryResults : "");
        return new DownloadHistoryList(list, query);
      });
    }

    return this._listPromises[key];
  },

  /**
   * This object is populated with one key for each type of download list that
   * can be returned by the getList method. The values are promises that resolve
   * to DownloadHistoryList objects.
   */
  _listPromises: {},

  async addDownloadToHistory(download) {
    if (
      download.source.isPrivate ||
      !PlacesUtils.history.canAddURI(PlacesUtils.toURI(download.source.url))
    ) {
      return;
    }

    await DownloadCache.addDownload(download);

    await this._updateHistoryListData(download.source.url);
  },

  /**
   * Stores new detailed metadata for the given download in history. This is
   * normally called after a download finishes, fails, or is canceled.
   *
   * Failed or canceled downloads with partial data are not stored as paused,
   * because the information from the session download is required for resuming.
   *
   * @param download
   *        Download object whose metadata should be updated. If the object
   *        represents a private download, the call has no effect.
   */
  async updateMetaData(download) {
    if (download.source.isPrivate || !download.stopped) {
      return;
    }

    let state = METADATA_STATE_CANCELED;
    if (download.succeeded) {
      state = METADATA_STATE_FINISHED;
    } else if (download.error) {
      if (download.error.becauseBlockedByParentalControls) {
        state = METADATA_STATE_BLOCKED_PARENTAL;
      } else if (download.error.becauseBlockedByReputationCheck) {
        state = METADATA_STATE_DIRTY;
      } else {
        state = METADATA_STATE_FAILED;
      }
    }

    let metaData = { state, endTime: download.endTime };
    if (download.succeeded) {
      metaData.fileSize = download.target.size;
    }

    // The verdict may still be present even if the download succeeded.
    if (download.error && download.error.reputationCheckVerdict) {
      metaData.reputationCheckVerdict = download.error.reputationCheckVerdict;
    }

    // This should be executed before any async parts, to ensure the cache is
    // updated before any notifications are activated.
    await DownloadCache.setMetadata(download.source.url, metaData);

    await this._updateHistoryListData(download.source.url);
  },

  async _updateHistoryListData(sourceUrl) {
    for (let key of Object.getOwnPropertyNames(this._listPromises)) {
      let downloadHistoryList = await this._listPromises[key];
      downloadHistoryList.updateForMetaDataChange(
        sourceUrl,
        DownloadCache.get(sourceUrl)
      );
    }
  },
};

/**
 * This cache exists:
 * - in order to optimize the load of DownloadsHistoryList, when Places
 *   annotations for history downloads must be read. In fact, annotations are
 *   stored in a single table, and reading all of them at once is much more
 *   efficient than an individual query.
 * - to avoid needing to do asynchronous reading of the database during download
 *   list updates, which are designed to be synchronous (to improve UI
 *   responsiveness).
 *
 * The cache is initialized the first time DownloadHistory.getList is called, or
 * when data is added.
 */
var DownloadCache = {
  _data: new Map(),
  _initializePromise: null,

  /**
   * Initializes the cache, loading the data from the places database.
   *
   * @return {Promise} Returns a promise that is resolved once the
   *                   initialization is complete.
   */
  ensureInitialized() {
    if (this._initializePromise) {
      return this._initializePromise;
    }
    this._initializePromise = (async () => {
      PlacesUtils.history.addObserver(this, true);

      let pageAnnos = await PlacesUtils.history.fetchAnnotatedPages([
        METADATA_ANNO,
        DESTINATIONFILEURI_ANNO,
      ]);

      let metaDataPages = pageAnnos.get(METADATA_ANNO);
      if (metaDataPages) {
        for (let { uri, content } of metaDataPages) {
          try {
            this._data.set(uri.href, JSON.parse(content));
          } catch (ex) {
            // Do nothing - JSON.parse could throw.
          }
        }
      }

      let destinationFilePages = pageAnnos.get(DESTINATIONFILEURI_ANNO);
      if (destinationFilePages) {
        for (let { uri, content } of destinationFilePages) {
          let newData = this.get(uri.href);
          newData.targetFileSpec = content;
          this._data.set(uri.href, newData);
        }
      }
    })();

    return this._initializePromise;
  },

  /**
   * This returns an object containing the meta data for the supplied URL.
   *
   * @param {String} url The url to get the meta data for.
   * @return {Object|null} Returns an empty object if there is no meta data found, or
   *                       an object containing the meta data. The meta data
   *                       will look like:
   *
   * { targetFileSpec, state, endTime, fileSize, ... }
   *
   * The targetFileSpec property is the value of "downloads/destinationFileURI",
   * while the other properties are taken from "downloads/metaData". Any of the
   * properties may be missing from the object.
   */
  get(url) {
    return this._data.get(url) || {};
  },

  /**
   * Adds a download to the cache and the places database.
   *
   * @param {Download} download The download to add to the database and cache.
   */
  async addDownload(download) {
    await this.ensureInitialized();

    let targetFile = new FileUtils.File(download.target.path);
    let targetUri = Services.io.newFileURI(targetFile);

    // This should be executed before any async parts, to ensure the cache is
    // updated before any notifications are activated.
    // Note: this intentionally overwrites any metadata as this is
    // the start of a new download.
    this._data.set(download.source.url, { targetFileSpec: targetUri.spec });

    let originalPageInfo = await PlacesUtils.history.fetch(download.source.url);

    let pageInfo = await PlacesUtils.history.insert({
      url: download.source.url,
      // In case we are downloading a file that does not correspond to a web
      // page for which the title is present, we populate the otherwise empty
      // history title with the name of the destination file, to allow it to be
      // visible and searchable in history results.
      title:
        (originalPageInfo && originalPageInfo.title) || targetFile.leafName,
      visits: [
        {
          // The start time is always available when we reach this point.
          date: download.startTime,
          transition: PlacesUtils.history.TRANSITIONS.DOWNLOAD,
          referrer: download.source.referrerInfo
            ? download.source.referrerInfo.originalReferrer
            : null,
        },
      ],
    });

    await PlacesUtils.history.update({
      annotations: new Map([["downloads/destinationFileURI", targetUri.spec]]),
      // XXX Bug 1479445: We shouldn't have to supply both guid and url here,
      // but currently we do.
      guid: pageInfo.guid,
      url: pageInfo.url,
    });
  },

  /**
   * Sets the metadata for a given url. If the cache already contains meta data
   * for the given url, it will be overwritten (note: the targetFileSpec will be
   * maintained).
   *
   * @param {String} url The url to set the meta data for.
   * @param {Object} metadata The new metaData to save in the cache.
   */
  async setMetadata(url, metadata) {
    await this.ensureInitialized();

    // This should be executed before any async parts, to ensure the cache is
    // updated before any notifications are activated.
    let existingData = this.get(url);
    let newData = { ...metadata };
    if ("targetFileSpec" in existingData) {
      newData.targetFileSpec = existingData.targetFileSpec;
    }
    this._data.set(url, newData);

    try {
      await PlacesUtils.history.update({
        annotations: new Map([[METADATA_ANNO, JSON.stringify(metadata)]]),
        url,
      });
    } catch (ex) {
      Cu.reportError(ex);
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsINavHistoryObserver",
    "nsISupportsWeakReference",
  ]),

  // nsINavHistoryObserver
  onDeleteURI(uri) {
    this._data.delete(uri.spec);
  },
  onClearHistory() {
    this._data.clear();
  },
  onBeginUpdateBatch() {},
  onEndUpdateBatch() {},
  onTitleChanged() {},
  onFrecencyChanged() {},
  onManyFrecenciesChanged() {},
  onPageChanged() {},
  onDeleteVisits() {},
};

/**
 * Represents a download from the browser history. This object implements part
 * of the interface of the Download object.
 *
 * While Download objects are shared between the public DownloadList and all the
 * DownloadHistoryList instances, multiple HistoryDownload objects referring to
 * the same item can be created for different DownloadHistoryList instances.
 *
 * @param placesNode
 *        The Places node from which the history download should be initialized.
 */
function HistoryDownload(placesNode) {
  this.placesNode = placesNode;

  // History downloads should get the referrer from Places (bug 829201).
  this.source = {
    url: placesNode.uri,
    isPrivate: false,
  };
  this.target = {
    path: undefined,
    exists: false,
    size: undefined,
  };

  // In case this download cannot obtain its end time from the Places metadata,
  // use the time from the Places node, that is the start time of the download.
  this.endTime = placesNode.time / 1000;
}

HistoryDownload.prototype = {
  /**
   * DownloadSlot containing this history download.
   */
  slot: null,

  /**
   * Pushes information from Places metadata into this object.
   */
  updateFromMetaData(metaData) {
    try {
      this.target.path = Cc["@mozilla.org/network/protocol;1?name=file"]
        .getService(Ci.nsIFileProtocolHandler)
        .getFileFromURLSpec(metaData.targetFileSpec).path;
    } catch (ex) {
      this.target.path = undefined;
    }

    if ("state" in metaData) {
      this.succeeded = metaData.state == METADATA_STATE_FINISHED;
      this.canceled =
        metaData.state == METADATA_STATE_CANCELED ||
        metaData.state == METADATA_STATE_PAUSED;
      this.endTime = metaData.endTime;

      // Recreate partial error information from the state saved in history.
      if (metaData.state == METADATA_STATE_FAILED) {
        this.error = { message: "History download failed." };
      } else if (metaData.state == METADATA_STATE_BLOCKED_PARENTAL) {
        this.error = { becauseBlockedByParentalControls: true };
      } else if (metaData.state == METADATA_STATE_DIRTY) {
        this.error = {
          becauseBlockedByReputationCheck: true,
          reputationCheckVerdict: metaData.reputationCheckVerdict || "",
        };
      } else {
        this.error = null;
      }

      // Normal history downloads are assumed to exist until the user interface
      // is refreshed, at which point these values may be updated.
      this.target.exists = true;
      this.target.size = metaData.fileSize;
    } else {
      // Metadata might be missing from a download that has started but hasn't
      // stopped already. Normally, this state is overridden with the one from
      // the corresponding in-progress session download. But if the browser is
      // terminated abruptly and additionally the file with information about
      // in-progress downloads is lost, we may end up using this state. We use
      // the failed state to allow the download to be restarted.
      //
      // On the other hand, if the download is missing the target file
      // annotation as well, it is just a very old one, and we can assume it
      // succeeded.
      this.succeeded = !this.target.path;
      this.error = this.target.path ? { message: "Unstarted download." } : null;
      this.canceled = false;

      // These properties may be updated if the user interface is refreshed.
      this.target.exists = false;
      this.target.size = undefined;
    }
  },

  /**
   * History downloads are never in progress.
   */
  stopped: true,

  /**
   * No percentage indication is shown for history downloads.
   */
  hasProgress: false,

  /**
   * History downloads cannot be restarted using their partial data, even if
   * they are indicated as paused in their Places metadata. The only way is to
   * use the information from a persisted session download, that will be shown
   * instead of the history download. In case this session download is not
   * available, we show the history download as canceled, not paused.
   */
  hasPartialData: false,

  /**
   * This method may be called when deleting a history download.
   */
  async finalize() {},

  /**
   * This method mimicks the "refresh" method of session downloads.
   */
  async refresh() {
    try {
      this.target.size = (await OS.File.stat(this.target.path)).size;
      this.target.exists = true;
    } catch (ex) {
      // We keep the known file size from the metadata, if any.
      this.target.exists = false;
    }

    this.slot.list._notifyAllViews("onDownloadChanged", this);
  },
};

/**
 * Represents one item in the list of public session and history downloads.
 *
 * The object may contain a session download, a history download, or both. When
 * both a history and a session download are present, the session download gets
 * priority and its information is accessed.
 *
 * @param list
 *        The DownloadHistoryList that owns this DownloadSlot object.
 */
function DownloadSlot(list) {
  this.list = list;
}

DownloadSlot.prototype = {
  list: null,

  /**
   * Download object representing the session download contained in this slot.
   */
  sessionDownload: null,

  /**
   * HistoryDownload object contained in this slot.
   */
  get historyDownload() {
    return this._historyDownload;
  },
  set historyDownload(historyDownload) {
    this._historyDownload = historyDownload;
    if (historyDownload) {
      historyDownload.slot = this;
    }
  },
  _historyDownload: null,

  /**
   * Returns the Download or HistoryDownload object for displaying information
   * and executing commands in the user interface.
   */
  get download() {
    return this.sessionDownload || this.historyDownload;
  },
};

/**
 * Represents an ordered collection of DownloadSlot objects containing a merged
 * view on session downloads and history downloads. Views on this list will
 * receive notifications for changes to both types of downloads.
 *
 * Downloads in this list are sorted from oldest to newest, with all session
 * downloads after all the history downloads. When a new history download is
 * added and the list also contains session downloads, the insertBefore option
 * of the onDownloadAdded notification refers to the first session download.
 *
 * The list of downloads cannot be modified using the DownloadList methods.
 *
 * @param publicList
 *        Underlying DownloadList containing public downloads.
 * @param place
 *        Places query used to retrieve history downloads.
 */
var DownloadHistoryList = function(publicList, place) {
  DownloadList.call(this);

  // While "this._slots" contains all the data in order, the other properties
  // provide fast access for the most common operations.
  this._slots = [];
  this._slotsForUrl = new Map();
  this._slotForDownload = new WeakMap();

  // Start the asynchronous queries to retrieve history and session downloads.
  publicList.addView(this).catch(Cu.reportError);
  let query = {},
    options = {};
  PlacesUtils.history.queryStringToQuery(place, query, options);

  // NB: The addObserver call sets our nsINavHistoryResultObserver.result.
  let result = PlacesUtils.history.executeQuery(query.value, options.value);
  result.addObserver(this);

  // Our history result observer is long lived for fast shared views, so free
  // the reference on shutdown to prevent leaks.
  Services.obs.addObserver(() => {
    this.result = null;
  }, "quit-application-granted");
};

DownloadHistoryList.prototype = {
  __proto__: DownloadList.prototype,

  /**
   * This is set when executing the Places query.
   */
  get result() {
    return this._result;
  },
  set result(result) {
    if (this._result == result) {
      return;
    }

    if (this._result) {
      this._result.removeObserver(this);
      this._result.root.containerOpen = false;
    }

    this._result = result;

    if (this._result) {
      this._result.root.containerOpen = true;
    }
  },
  _result: null,

  /**
   * Updates the download history item when the meta data or destination file
   * changes.
   *
   * @param {String} sourceUrl The sourceUrl which was updated.
   * @param {Object} metaData The new meta data for the sourceUrl.
   */
  updateForMetaDataChange(sourceUrl, metaData) {
    let slotsForUrl = this._slotsForUrl.get(sourceUrl);
    if (!slotsForUrl) {
      return;
    }

    for (let slot of slotsForUrl) {
      if (slot.sessionDownload) {
        // The visible data doesn't change, so we don't have to notify views.
        return;
      }
      slot.historyDownload.updateFromMetaData(metaData);
      this._notifyAllViews("onDownloadChanged", slot.download);
    }
  },

  /**
   * Index of the first slot that contains a session download. This is equal to
   * the length of the list when there are no session downloads.
   */
  _firstSessionSlotIndex: 0,

  _insertSlot({ slot, index, slotsForUrl }) {
    // Add the slot to the ordered array.
    this._slots.splice(index, 0, slot);
    this._downloads.splice(index, 0, slot.download);
    if (!slot.sessionDownload) {
      this._firstSessionSlotIndex++;
    }

    // Add the slot to the fast access maps.
    slotsForUrl.add(slot);
    this._slotsForUrl.set(slot.download.source.url, slotsForUrl);

    // Add the associated view items.
    this._notifyAllViews("onDownloadAdded", slot.download, {
      insertBefore: this._downloads[index + 1],
    });
  },

  _removeSlot({ slot, slotsForUrl }) {
    // Remove the slot from the ordered array.
    let index = this._slots.indexOf(slot);
    this._slots.splice(index, 1);
    this._downloads.splice(index, 1);
    if (this._firstSessionSlotIndex > index) {
      this._firstSessionSlotIndex--;
    }

    // Remove the slot from the fast access maps.
    slotsForUrl.delete(slot);
    if (slotsForUrl.size == 0) {
      this._slotsForUrl.delete(slot.download.source.url);
    }

    // Remove the associated view items.
    this._notifyAllViews("onDownloadRemoved", slot.download);
  },

  /**
   * Ensures that the information about a history download is stored in at least
   * one slot, adding a new one at the end of the list if necessary.
   *
   * A reference to the same Places node will be stored in the HistoryDownload
   * object for all the DownloadSlot objects associated with the source URL.
   *
   * @param placesNode
   *        The Places node that represents the history download.
   */
  _insertPlacesNode(placesNode) {
    let slotsForUrl = this._slotsForUrl.get(placesNode.uri) || new Set();

    // If there are existing slots associated with this URL, we only have to
    // ensure that the Places node reference is kept updated in case the more
    // recent Places notification contained a different node object.
    if (slotsForUrl.size > 0) {
      for (let slot of slotsForUrl) {
        if (!slot.historyDownload) {
          slot.historyDownload = new HistoryDownload(placesNode);
        } else {
          slot.historyDownload.placesNode = placesNode;
        }
      }
      return;
    }

    // If there are no existing slots for this URL, we have to create a new one.
    // Since the history download is visible in the slot, we also have to update
    // the object using the Places metadata.
    let historyDownload = new HistoryDownload(placesNode);
    historyDownload.updateFromMetaData(DownloadCache.get(placesNode.uri));
    let slot = new DownloadSlot(this);
    slot.historyDownload = historyDownload;
    this._insertSlot({ slot, slotsForUrl, index: this._firstSessionSlotIndex });
  },

  // nsINavHistoryResultObserver
  containerStateChanged(node, oldState, newState) {
    this.invalidateContainer(node);
  },

  // nsINavHistoryResultObserver
  invalidateContainer(container) {
    this._notifyAllViews("onDownloadBatchStarting");

    // Remove all the current slots containing only history downloads.
    for (let index = this._slots.length - 1; index >= 0; index--) {
      let slot = this._slots[index];
      if (slot.sessionDownload) {
        // The visible data doesn't change, so we don't have to notify views.
        slot.historyDownload = null;
      } else {
        let slotsForUrl = this._slotsForUrl.get(slot.download.source.url);
        this._removeSlot({ slot, slotsForUrl });
      }
    }

    // Add new slots or reuse existing ones for history downloads.
    for (let index = container.childCount - 1; index >= 0; --index) {
      try {
        this._insertPlacesNode(container.getChild(index));
      } catch (ex) {
        Cu.reportError(ex);
      }
    }

    this._notifyAllViews("onDownloadBatchEnded");
  },

  // nsINavHistoryResultObserver
  nodeInserted(parent, placesNode) {
    this._insertPlacesNode(placesNode);
  },

  // nsINavHistoryResultObserver
  nodeRemoved(parent, placesNode, aOldIndex) {
    let slotsForUrl = this._slotsForUrl.get(placesNode.uri);
    for (let slot of slotsForUrl) {
      if (slot.sessionDownload) {
        // The visible data doesn't change, so we don't have to notify views.
        slot.historyDownload = null;
      } else {
        this._removeSlot({ slot, slotsForUrl });
      }
    }
  },

  // nsINavHistoryResultObserver
  nodeIconChanged() {},
  nodeTitleChanged() {},
  nodeKeywordChanged() {},
  nodeDateAddedChanged() {},
  nodeLastModifiedChanged() {},
  nodeHistoryDetailsChanged() {},
  nodeTagsChanged() {},
  sortingChanged() {},
  nodeMoved() {},
  nodeURIChanged() {},
  batching() {},

  // DownloadList callback
  onDownloadAdded(download) {
    let url = download.source.url;
    let slotsForUrl = this._slotsForUrl.get(url) || new Set();

    // For every source URL, there can be at most one slot containing a history
    // download without an associated session download. If we find one, then we
    // can reuse it for the current session download, although we have to move
    // it together with the other session downloads.
    let slot = [...slotsForUrl][0];
    if (slot && !slot.sessionDownload) {
      // Remove the slot because we have to change its position.
      this._removeSlot({ slot, slotsForUrl });
    } else {
      slot = new DownloadSlot(this);
    }
    slot.sessionDownload = download;
    this._insertSlot({ slot, slotsForUrl, index: this._slots.length });
    this._slotForDownload.set(download, slot);
  },

  // DownloadList callback
  onDownloadChanged(download) {
    let slot = this._slotForDownload.get(download);
    this._notifyAllViews("onDownloadChanged", slot.download);
  },

  // DownloadList callback
  onDownloadRemoved(download) {
    let url = download.source.url;
    let slotsForUrl = this._slotsForUrl.get(url);
    let slot = this._slotForDownload.get(download);
    this._removeSlot({ slot, slotsForUrl });

    this._slotForDownload.delete(download);

    // If there was only one slot for this source URL and it also contained a
    // history download, we should resurrect it in the correct area of the list.
    if (slotsForUrl.size == 0 && slot.historyDownload) {
      // We have one download slot containing both a session download and a
      // history download, and we are now removing the session download.
      // Previously, we did not use the Places metadata because it was obscured
      // by the session download. Since this is no longer the case, we have to
      // read the latest metadata before resurrecting the history download.
      slot.historyDownload.updateFromMetaData(DownloadCache.get(url));
      slot.sessionDownload = null;
      // Place the resurrected history slot after all the session slots.
      this._insertSlot({
        slot,
        slotsForUrl,
        index: this._firstSessionSlotIndex,
      });
    }
  },

  // DownloadList
  add() {
    throw new Error("Not implemented.");
  },

  // DownloadList
  remove() {
    throw new Error("Not implemented.");
  },

  // DownloadList
  removeFinished() {
    throw new Error("Not implemented.");
  },
};
