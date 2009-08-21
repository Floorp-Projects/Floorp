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
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
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

const EXPORTED_SYMBOLS = ['HistoryEngine'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/base_records/collection.js");
Cu.import("resource://weave/type_records/history.js");

function HistoryEngine() {
  this._init();
}
HistoryEngine.prototype = {
  __proto__: SyncEngine.prototype,
  name: "history",
  displayName: "History",
  logName: "History",
  _recordObj: HistoryRec,
  _storeObj: HistoryStore,
  _trackerObj: HistoryTracker,

  _sync: function HistoryEngine__sync() {
    Svc.History.runInBatchMode({
      runBatched: Utils.bind2(this, SyncEngine.prototype._sync)
    }, null);
  },

  // History reconciliation is simpler than the default one from SyncEngine,
  // because we have the advantage that we can use the URI as a definitive
  // check for local existence of incoming items.  The steps are as follows:
  //
  // 1) Check for the same item in the locally modified list.  In that case,
  //    local trumps remote.  This step is unchanged from our superclass.
  // 2) Check if the incoming item was deleted.  Skip if so.
  // 3) Apply new record/update.
  //
  // Note that we don't attempt to equalize the IDs, the history store does that
  // as part of update()
  _reconcile: function HistEngine__reconcile(item) {
    // Step 1: Check for conflicts
    //         If same as local record, do not upload
    if (item.id in this._tracker.changedIDs) {
      if (this._isEqual(item))
        this._tracker.removeChangedID(item.id);
      return false;
    }

    // Step 2: Check if the item is deleted - we don't support that (yet?)
    if (item.deleted)
      return false;

    // Step 3: Apply update/new record
    return true;
  },

  _syncFinish: function HistEngine__syncFinish(error) {
    this._log.debug("Finishing up sync");
    this._tracker.resetScore();

    // Only leave 1 week's worth of history on the server
    let coll = new Collection(this.engineURL, this._recordObj);
    coll.older = this.lastSync - 604800; // 1 week
    coll.full = 0;
    coll.delete();
  }
};

function HistoryStore() {
  this._init();
}
HistoryStore.prototype = {
  __proto__: Store.prototype,
  name: "history",
  _logName: "HistStore",

  get _hsvc() {
    let hsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
      getService(Ci.nsINavHistoryService);
    hsvc.QueryInterface(Ci.nsIGlobalHistory2);
    hsvc.QueryInterface(Ci.nsIBrowserHistory);
    hsvc.QueryInterface(Ci.nsPIPlacesDatabase);
    this.__defineGetter__("_hsvc", function() hsvc);
    return hsvc;
  },

  get _anno() {
    let anno = Cc["@mozilla.org/browser/annotation-service;1"].
      getService(Ci.nsIAnnotationService);
    this.__defineGetter__("_ans", function() anno);
    return anno;
  },

  get _db() {
    return this._hsvc.DBConnection;
  },

  get _visitStm() {
    this._log.trace("Creating SQL statement: _visitStm");
    let stm = this._db.createStatement(
      "SELECT visit_type type, visit_date date " +
      "FROM moz_historyvisits_view " +
      "WHERE place_id = (" +
        "SELECT id " +
        "FROM moz_places_view " +
        "WHERE url = :url) " +
      "ORDER BY date DESC LIMIT 10");
    this.__defineGetter__("_visitStm", function() stm);
    return stm;
  },

  get _urlStm() {
    this._log.trace("Creating SQL statement: _urlStm");
    let stm = this._db.createStatement(
      "SELECT url, title " +
      "FROM moz_places_view " +
      "WHERE id = (" +
        "SELECT place_id " +
        "FROM moz_annos " +
        "WHERE content = :guid AND anno_attribute_id = (" +
          "SELECT id " +
          "FROM moz_anno_attributes " +
          "WHERE name = 'weave/guid'))");
    this.__defineGetter__("_urlStm", function() stm);
    return stm;
  },

  // See bug 320831 for why we use SQL here
  _getVisits: function HistStore__getVisits(uri) {
    let visits = [];
    if (typeof(uri) != "string")
      uri = uri.spec;

    try {
      this._visitStm.params.url = uri;
      while (this._visitStm.step()) {
        visits.push({date: this._visitStm.row.date,
                     type: this._visitStm.row.type});
      }
    } finally {
      this._visitStm.reset();
    }

    return visits;
  },

  _getGUID: function HistStore__getGUID(uri) {
    if (typeof(uri) == "string")
      uri = Utils.makeURI(uri);
    try {
      return this._anno.getPageAnnotation(uri, "weave/guid");
    } catch (e) {
      // FIXME
      // if (e != NS_ERROR_NOT_AVAILABLE)
      // throw e;
    }
    let guid = Utils.makeGUID();
    this._anno.setPageAnnotation(uri, "weave/guid", guid, 0, this._anno.EXPIRE_WITH_HISTORY);
    return guid;
  },

  // See bug 468732 for why we use SQL here
  _findURLByGUID: function HistStore__findURLByGUID(guid) {
    try {
      this._urlStm.params.guid = guid;
      if (!this._urlStm.step())
        return null;

      return {url: this._urlStm.row.url,
              title: this._urlStm.row.title};
    } finally {
      this._urlStm.reset();
    }
  },

  changeItemID: function HStore_changeItemID(oldID, newID) {
    try {
      let uri = Utils.makeURI(this._findURLByGUID(oldID).url);
      this._anno.setPageAnnotation(uri, "weave/guid", newID, 0, 0);
    } catch (e) {
      this._log.debug("Could not change item ID: " + e);
    }
  },


  getAllIDs: function HistStore_getAllIDs() {
    let query = this._hsvc.getNewQuery(),
        options = this._hsvc.getNewQueryOptions();

    query.minVisits = 1;
    options.maxResults = 100;
    /*
    dump("SORT_BY_DATE_DESCENDING is " + options.SORT_BY_DATE_DESCENDING + "\n");
    dump("SORT_BY_ANNOTATIOn_DESCENDING is " + options.SORT_BY_ANNOTATION_DESCENDING + "\n");
    dump("BEFORE SETTING, options.sortingMode is " + options.sortingMode + "\n");

    for ( let z = -1; z < 20; z++) {
      try {
	options.sortingMode = z;
	dump("  -> Can set to " + z + " OK...\n");
      } catch (e) {
	dump("  -> Setting to " + z + " raises exception.\n");
      }
    }
    dump("AFTER SETTING, options.sortingMode is " + options.sortingMode + "\n");
     */
     // TODO the following line throws exception on Fennec; see above.
    options.sortingMode = options.SORT_BY_DATE_DESCENDING;
    options.queryType = options.QUERY_TYPE_HISTORY;

    let root = this._hsvc.executeQuery(query, options).root;
    root.QueryInterface(Ci.nsINavHistoryQueryResultNode);
    root.containerOpen = true;

    let items = {};
    for (let i = 0; i < root.childCount; i++) {
      let item = root.getChild(i);
      let guid = this._getGUID(item.uri);
      items[guid] = item.uri;
    }
    return items;
  },

  create: function HistStore_create(record) {
    this.update(record);
  },

  remove: function HistStore_remove(record) {
    //this._log.trace("  -> NOT removing history entry: " + record.id);
    // FIXME: implement!
  },

  update: function HistStore_update(record) {
    this._log.trace("  -> processing history entry: " + record.histUri);

    let uri = Utils.makeURI(record.histUri);
    if (!uri) {
      this._log.warn("Attempted to process invalid URI, skipping");
      throw "invalid URI in record";
    }
    let curvisits = [];
    if (this.urlExists(uri))
      curvisits = this._getVisits(record.histUri);

    let visit;
    while ((visit = record.visits.pop())) {
      if (curvisits.filter(function(i) i.date == visit.date).length)
        continue;
      this._log.debug("     visit " + visit.date);
      this._hsvc.addVisit(uri, visit.date, null, visit.type,
                          (visit.type == 5 || visit.type == 6), 0);
    }
    this._hsvc.setPageTitle(uri, record.title);

    // Equalize IDs
    let localId = this._getGUID(record.histUri);
    if (localId != record.id)
      this.changeItemID(localId, record.id);

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
    return url ? this._hsvc.isVisited(url) : false;
  },

  createRecord: function HistStore_createRecord(guid, cryptoMetaURL) {
    let foo = this._findURLByGUID(guid);
    let record = new HistoryRec();
    record.id = guid;
    if (foo) {
      record.histUri = foo.url;
      record.title = foo.title;
      record.visits = this._getVisits(record.histUri);
      record.encryption = cryptoMetaURL;
    }
    else
      record.deleted = true;
    this.cache.put(guid, record);
    return record;
  },

  wipe: function HistStore_wipe() {
    this._hsvc.removeAllPages();
  }
};

function HistoryTracker() {
  this._init();
}
HistoryTracker.prototype = {
  __proto__: Tracker.prototype,
  name: "history",
  _logName: "HistoryTracker",
  file: "history",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver]),

  get _hsvc() {
    let hsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
      getService(Ci.nsINavHistoryService);
    this.__defineGetter__("_hsvc", function() hsvc);
    return hsvc;
  },

  // FIXME: hack!
  get _store() {
    let store = new HistoryStore();
    this.__defineGetter__("_store", function() store);
    return store;
  },

  _init: function HT__init() {
    Tracker.prototype._init.call(this);
    this._hsvc.addObserver(this, false);
  },

  onBeginUpdateBatch: function HT_onBeginUpdateBatch() {},
  onEndUpdateBatch: function HT_onEndUpdateBatch() {},
  onPageChanged: function HT_onPageChanged() {},
  onTitleChanged: function HT_onTitleChanged() {},

  /* Every add or remove is worth 1 point.
   * Clearing the whole history is worth 50 points (see below)
   */
  _upScore: function BMT__upScore() {
    this._score += 1;
  },

  onVisit: function HT_onVisit(uri, vid, time, session, referrer, trans) {
    if (this.ignoreAll)
      return;
    this._log.trace("onVisit: " + uri.spec);
    if (this.addChangedID(this._store._getGUID(uri)))
      this._upScore();
  },
  onPageExpired: function HT_onPageExpired(uri, time, entry) {
  },
  onDeleteURI: function HT_onDeleteURI(uri) {
  },
  onClearHistory: function HT_onClearHistory() {
    this._log.trace("onClearHistory");
    this._score += 50;
  }
};
