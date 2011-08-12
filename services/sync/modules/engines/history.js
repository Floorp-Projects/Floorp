/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Weave
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mills <thunder@mozilla.com>
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *   Richard Newman <rnewman@mozilla.com>
 *   Allison Naaktgeboren <ally@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const EXPORTED_SYMBOLS = ['HistoryEngine', 'HistoryRec'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

const HISTORY_TTL = 5184000; // 60 days
const TOPIC_UPDATEPLACES_COMPLETE = "places-updatePlaces-complete";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/async.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/log4moz.js");

function HistoryRec(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
HistoryRec.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Sync.Record.History",
  ttl: HISTORY_TTL
};

Utils.deferGetSet(HistoryRec, "cleartext", ["histUri", "title", "visits"]);


function HistoryEngine() {
  SyncEngine.call(this, "History");
}
HistoryEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _recordObj: HistoryRec,
  _storeObj: HistoryStore,
  _trackerObj: HistoryTracker,
  downloadLimit: MAX_HISTORY_DOWNLOAD,
  applyIncomingBatchSize: HISTORY_STORE_BATCH_SIZE,

  _findDupe: function _findDupe(item) {
    return this._store.GUIDForUri(item.histUri);
  }
};

function HistoryStore(name) {
  Store.call(this, name);

  // Explicitly nullify our references to our cached services so we don't leak
  Svc.Obs.add("places-shutdown", function() {
    for each ([query, stmt] in Iterator(this._stmts))
      stmt.finalize();
    this._stmts = [];
  }, this);
}
HistoryStore.prototype = {
  __proto__: Store.prototype,

  __asyncHistory: null,
  get _asyncHistory() {
    if (!this.__asyncHistory) {
      this.__asyncHistory = Cc["@mozilla.org/browser/history;1"]
                              .getService(Ci.mozIAsyncHistory);
    }
    return this.__asyncHistory;
  },

  _stmts: {},
  _getStmt: function(query) {
    if (query in this._stmts)
      return this._stmts[query];

    this._log.trace("Creating SQL statement: " + query);
    let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                        .DBConnection;
    return this._stmts[query] = db.createAsyncStatement(query);
  },

  get _setGUIDStm() {
    return this._getStmt(
      "UPDATE moz_places " +
      "SET guid = :guid " +
      "WHERE url = :page_url");
  },

  // Some helper functions to handle GUIDs
  setGUID: function setGUID(uri, guid) {
    uri = uri.spec ? uri.spec : uri;

    if (!guid)
      guid = Utils.makeGUID();

    let stmt = this._setGUIDStm;
    stmt.params.guid = guid;
    stmt.params.page_url = uri;
    Async.querySpinningly(stmt);
    return guid;
  },

  get _guidStm() {
    return this._getStmt(
      "SELECT guid " +
      "FROM moz_places " +
      "WHERE url = :page_url");
  },
  _guidCols: ["guid"],

  GUIDForUri: function GUIDForUri(uri, create) {
    let stm = this._guidStm;
    stm.params.page_url = uri.spec ? uri.spec : uri;

    // Use the existing GUID if it exists
    let result = Async.querySpinningly(stm, this._guidCols)[0];
    if (result && result.guid)
      return result.guid;

    // Give the uri a GUID if it doesn't have one
    if (create)
      return this.setGUID(uri);
  },

  get _visitStm() {
    return this._getStmt(
      "SELECT visit_type type, visit_date date " +
      "FROM moz_historyvisits " +
      "WHERE place_id = (SELECT id FROM moz_places WHERE url = :url) " +
      "ORDER BY date DESC LIMIT 10");
  },
  _visitCols: ["date", "type"],

  get _urlStm() {
    return this._getStmt(
      "SELECT url, title, frecency " +
      "FROM moz_places " +
      "WHERE guid = :guid");
  },
  _urlCols: ["url", "title", "frecency"],

  get _allUrlStm() {
    return this._getStmt(
      "SELECT url " +
      "FROM moz_places " +
      "WHERE last_visit_date > :cutoff_date " +
      "ORDER BY frecency DESC " +
      "LIMIT :max_results");
  },
  _allUrlCols: ["url"],

  // See bug 320831 for why we use SQL here
  _getVisits: function HistStore__getVisits(uri) {
    this._visitStm.params.url = uri;
    return Async.querySpinningly(this._visitStm, this._visitCols);
  },

  // See bug 468732 for why we use SQL here
  _findURLByGUID: function HistStore__findURLByGUID(guid) {
    this._urlStm.params.guid = guid;
    return Async.querySpinningly(this._urlStm, this._urlCols)[0];
  },

  changeItemID: function HStore_changeItemID(oldID, newID) {
    this.setGUID(this._findURLByGUID(oldID).url, newID);
  },


  getAllIDs: function HistStore_getAllIDs() {
    // Only get places visited within the last 30 days (30*24*60*60*1000ms)
    this._allUrlStm.params.cutoff_date = (Date.now() - 2592000000) * 1000;
    this._allUrlStm.params.max_results = MAX_HISTORY_UPLOAD;

    let urls = Async.querySpinningly(this._allUrlStm, this._allUrlCols);
    let self = this;
    return urls.reduce(function(ids, item) {
      ids[self.GUIDForUri(item.url, true)] = item.url;
      return ids;
    }, {});
  },

  applyIncomingBatch: function applyIncomingBatch(records) {
    let failed = [];

    // Convert incoming records to mozIPlaceInfo objects. Some records can be
    // ignored or handled directly, so we're rewriting the array in-place.
    let i, k;
    for (i = 0, k = 0; i < records.length; i++) {
      let record = records[k] = records[i];
      let shouldApply;

      // This is still synchronous I/O for now.
      try {
        if (record.deleted) {
          // Consider using nsIBrowserHistory::removePages() here.
          this.remove(record);
          // No further processing needed. Remove it from the list.
          shouldApply = false;
        } else {
          shouldApply = this._recordToPlaceInfo(record);
        }
      } catch(ex) {
        failed.push(record.id);
        shouldApply = false;
      }

      if (shouldApply) {
        k += 1;
      }
    }
    records.length = k; // truncate array

    // Nothing to do.
    if (!records.length) {
      return failed;
    }

    let cb = Async.makeSyncCallback();
    let updatePlacesCallback = { 
      handleResult: function handleResult() {},
      handleError: function handleError(resultCode, placeInfo) {
        failed.push(placeInfo.guid);
      }
    };
    let onComplete = function onComplete(subject, topic, data) {
      Svc.Obs.remove(TOPIC_UPDATEPLACES_COMPLETE, onComplete);
      cb();
    };
    Svc.Obs.add(TOPIC_UPDATEPLACES_COMPLETE, onComplete);
    this._asyncHistory.updatePlaces(records, updatePlacesCallback);
    Async.waitForSyncCallback(cb);
    return failed;
  },

  /**
   * Converts a Sync history record to a mozIPlaceInfo.
   * 
   * Throws if an invalid record is encountered (invalid URI, etc.),
   * returns true if the record is to be applied, false otherwise
   * (no visits to add, etc.),
   */
  _recordToPlaceInfo: function _recordToPlaceInfo(record) {
    // Sort out invalid URIs and ones Places just simply doesn't want.
    record.uri = Utils.makeURI(record.histUri);
    if (!record.uri) {
      this._log.warn("Attempted to process invalid URI, skipping.");
      throw "Invalid URI in record";
    }

    if (!Utils.checkGUID(record.id)) {
      this._log.warn("Encountered record with invalid GUID: " + record.id);
      return false;
    }
    record.guid = record.id;

    if (!PlacesUtils.history.canAddURI(record.uri)) {
      this._log.trace("Ignoring record " + record.id + " with URI "
                      + record.uri.spec + ": can't add this URI.");
      return false;
    }

    // We dupe visits by date and type. So an incoming visit that has
    // the same timestamp and type as a local one won't get applied.
    // To avoid creating new objects, we rewrite the query result so we
    // can simply check for containment below.
    let curVisits = this._getVisits(record.histUri);
    for (let i = 0; i < curVisits.length; i++) {
      curVisits[i] = curVisits[i].date + "," + curVisits[i].type;
    }

    // Walk through the visits, make sure we have sound data, and eliminate
    // dupes. The latter is done by rewriting the array in-place.
    let k;
    for (i = 0, k = 0; i < record.visits.length; i++) {
      let visit = record.visits[k] = record.visits[i];

      if (!visit.date || typeof visit.date != "number") {
        this._log.warn("Encountered record with invalid visit date: "
                       + visit.date);
        throw "Visit has no date!";
      }
      if (!visit.type || !(visit.type >= PlacesUtils.history.TRANSITION_LINK &&
                           visit.type <= PlacesUtils.history.TRANSITION_FRAMED_LINK)) {
        this._log.warn("Encountered record with invalid visit type: "
                       + visit.type);
        throw "Invalid visit type!";
      }
      // Dates need to be integers
      visit.date = Math.round(visit.date);

      if (curVisits.indexOf(visit.date + "," + visit.type) != -1) {
        // Visit is a dupe, don't increment 'k' so the element will be
        // overwritten.
        continue;
      }
      visit.visitDate = visit.date;
      visit.transitionType = visit.type;
      k += 1;
    }
    record.visits.length = k; // truncate array

    // No update if there aren't any visits to apply.
    // mozIAsyncHistory::updatePlaces() wants at least one visit.
    // In any case, the only thing we could change would be the title
    // and that shouldn't change without a visit.
    if (!record.visits.length) {
      this._log.trace("Ignoring record " + record.id + " with URI "
                      + record.uri.spec + ": no visits to add.");
      return false;
    }

    return true;
  },

  remove: function HistStore_remove(record) {
    let page = this._findURLByGUID(record.id);
    if (page == null) {
      this._log.debug("Page already removed: " + record.id);
      return;
    }

    let uri = Utils.makeURI(page.url);
    PlacesUtils.history.removePage(uri);
    this._log.trace("Removed page: " + [record.id, page.url, page.title]);
  },

  itemExists: function HistStore_itemExists(id) {
    if (this._findURLByGUID(id))
      return true;
    return false;
  },

  urlExists: function HistStore_urlExists(url) {
    if (typeof(url) == "string")
      url = Utils.makeURI(url);
    // Don't call isVisited on a null URL to work around crasher bug 492442.
    return url ? PlacesUtils.history.isVisited(url) : false;
  },

  createRecord: function createRecord(id, collection) {
    let foo = this._findURLByGUID(id);
    let record = new HistoryRec(collection, id);
    if (foo) {
      record.histUri = foo.url;
      record.title = foo.title;
      record.sortindex = foo.frecency;
      record.visits = this._getVisits(record.histUri);
    }
    else
      record.deleted = true;

    return record;
  },

  wipe: function HistStore_wipe() {
    PlacesUtils.history.removeAllPages();
  }
};

function HistoryTracker(name) {
  Tracker.call(this, name);
  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);
}
HistoryTracker.prototype = {
  __proto__: Tracker.prototype,

  _enabled: false,
  observe: function observe(subject, topic, data) {
    switch (topic) {
      case "weave:engine:start-tracking":
        if (!this._enabled) {
          PlacesUtils.history.addObserver(this, true);
          this._enabled = true;
        }
        break;
      case "weave:engine:stop-tracking":
        if (this._enabled) {
          PlacesUtils.history.removeObserver(this);
          this._enabled = false;
        }
        break;
    }
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavHistoryObserver,
    Ci.nsISupportsWeakReference
  ]),

  onBeginUpdateBatch: function HT_onBeginUpdateBatch() {},
  onEndUpdateBatch: function HT_onEndUpdateBatch() {},
  onPageChanged: function HT_onPageChanged() {},
  onTitleChanged: function HT_onTitleChanged() {},
  onDeleteVisits: function () {},
  onDeleteURI: function () {},

  /* Every add is worth 1 point.
   * OnBeforeDeleteURI will triggger a sync for MULTI-DEVICE (see below)
   * Clearing all history will trigger a sync for MULTI-DEVICE (see below)
   */
  _upScoreXLarge: function HT__upScoreXLarge() {
    this.score += SCORE_INCREMENT_XLARGE;
  },

  onVisit: function HT_onVisit(uri, vid, time, session, referrer, trans, guid) {
    if (this.ignoreAll)
      return;
    this._log.trace("onVisit: " + uri.spec);
    if (this.addChangedID(guid)) {
      this.score += SCORE_INCREMENT_SMALL;
    }
  },

  onBeforeDeleteURI: function onBeforeDeleteURI(uri, guid, reason) {
    if (this.ignoreAll || reason == Ci.nsINavHistoryObserver.REASON_EXPIRED)
      return;
    this._log.trace("onBeforeDeleteURI: " + uri.spec);
    if (this.addChangedID(guid)) {
      this._upScoreXLarge();
    }
  },

  onClearHistory: function HT_onClearHistory() {
    this._log.trace("onClearHistory");
    this._upScoreXLarge();
  }
};
