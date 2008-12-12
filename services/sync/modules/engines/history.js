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

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/syncCores.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;

function HistoryEngine() {
  this._init();
}
HistoryEngine.prototype = {
  __proto__: SyncEngine.prototype,
  get _super() SyncEngine.prototype,

  get name() "history",
  get displayName() "History",
  get logName() "History",
  get serverPrefix() "user-data/history/",

  get _store() {
    let store = new HistoryStore();
    this.__defineGetter__("_store", function() store);
    return store;
  },

  get _tracker() {
    let tracker = new HistoryTracker();
    this.__defineGetter__("_tracker", function() tracker);
    return tracker;
  }
};

function HistoryStore() {
  this._init();
}
HistoryStore.prototype = {
  __proto__: Store.prototype,
  _logName: "HistStore",
  _lookup: null,

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

  get _histDB_31() {
    return this._hsvc.DBConnection;
  },

  get _histDB_30() {
    let file = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties).
      get("ProfD", Ci.nsIFile);
    file.append("places.sqlite");
    let stor = Cc["@mozilla.org/storage/service;1"].
      getService(Ci.mozIStorageService);
    let db = stor.openDatabase(file);
    this.__defineGetter__("_histDB_30", function() db);
    return db;
  },

  get _db() {
    // FIXME
    //if (fx3.0)
    //  return this._histDB_30;
    //else
      return this._histDB_31;
  },

  get _visitStm() {
    let stmt = this._db.createStatement(
      "SELECT visit_type AS type FROM moz_historyvisits WHERE place_id = :placeid");
    this.__defineGetter__("_visitStm", function() stmt);
    return stmt;
  },

  get _pidStm() {
    let stmt = this._db.createStatement(
      "SELECT id FROM moz_places WHERE url = :url");
    this.__defineGetter__("_pidStm", function() stmt);
    return stmt;
  },

  get _urlStm() {
    let stmt = this._db.createStatement(
      "SELECT url FROM moz_places WHERE id = ?1");
    this.__defineGetter__("_urlStm", function() stmt);
    return stmt;
  },

  get _findPidByAnnoStm() {
    let stmt = this._db.createStatement(
      "SELECT place_id FROM moz_annos WHERE type = ?1 AND content = ?2");
    this.__defineGetter__("_findPidByAnnoStm", function() stmt);
    return stmt;
  },

  create: function HistStore_create(record) {
    this._log.debug("  -> creating history entry: " + record.id);
    let uri = Utils.makeURI(record.cleartext.URI);
    let redirect = false;
    if (record.cleartext.transition == 5 || record.cleartext.transition == 6)
      redirect = true;

    this._hsvc.addVisit(uri, record.cleartext.time, null,
                        record.cleartext.transition, redirect, 0);
    this._hsvc.setPageTitle(uri, record.cleartext.title);
  },

  remove: function HistStore_remove(record) {
    this._log.trace("  -> NOT removing history entry: " + record.id);
    // FIXME: implement!
  },

  update: function HistStore_update(record) {
    this._log.trace("  -> FIXME: NOT editing history entry: " + record.id);
    // FIXME: implement!
  },

  // See bug 320831 for why we use SQL here
  _getVisitType: function HistStore__getVisitType(uri) {
    if (typeof(uri) != "string")
      uri = uri.spec;
    try {
      this._pidStm.params.url = uri;
      if (this._pidStm.step()) {
        let placeId = this._pidStm.row.id;
        this._visitStm.params.placeid = placeId;
        if (this._visitStm.step())
          return this._visitStm.row.type;
      }
    } finally {
      this._pidStm.reset();
      this._visitStm.reset();
    }
    return null;
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
    this._anno.setPageAnnotation(uri, "weave/guid", guid, 0, 0);
    return guid;
  },

  // See bug 468732 for why we use SQL here
  _findURLByGUID: function HistStore__findByGUID(guid) {
    this._findPidByAnnoStm.bindUTF8Parameter(0, "weave/guid");
    this._findPidByAnnoStm.bindUTF8Parameter(1, guid);
    if (this._findPidByAnnoStm.executeStep()) {
      let placeId = this._findPidByAnnoStm.getInt32(0);
      this._urlStm.bindInt32Parameter(0, placeId);
      if (this._urlStm.executeStep()) {
        return this._urlStm.getString(0);
      }
    }
    return null;
  },

  // XXX need a better way to query Places for all GUIDs
  getAllIDs: function BStore_getAllIDs() {
    let all = this.wrap();
    return all;
  },

  wrap: function HistStore_wrap() {
    let query = this._hsvc.getNewQuery(),
        options = this._hsvc.getNewQueryOptions();

    query.minVisits = 1;

    let root = this._hsvc.executeQuery(query, options).root;
    root.QueryInterface(Ci.nsINavHistoryQueryResultNode);
    root.containerOpen = true;

    let items = {};
    for (let i = 0; i < root.childCount; i++) {
      let item = root.getChild(i);
      let guid = this._getGUID(item.uri);
      let vType = this._getVisitType(item.uri);
      items[guid] = {title: item.title,
		     URI: item.uri,
		     transition: vType};
      // FIXME: get last 10 visit times (& transitions ?)
    }

    return items;
  },

  wrapItem: function HistStore_wrapItem(id) {
    // FIXME: use findURLByGUID! (not so important b/c of cache hints though)
    if (this._itemCache)
      return this._itemCache[id];
    let all = this._wrap();
    return all[id];
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
  _logName: "HistoryTracker",

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
    this.__proto__.__proto__._init.call(this);
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
    if (!this.enabled)
      return;
    this._score += 1;
  },

  onVisit: function HT_onVisit(uri, vid, time, session, referrer, trans) {
    this._log.trace("onVisit: " + itemId);
    this.addChangedID(this._store._getGUID(uri));
    this._upScore();
  },
  onPageExpired: function HT_onPageExpired(uri, time, entry) {
    this._log.trace("onPageExpired: " + itemId);
    this.addChangedID(this._store._getGUID(uri)); // XXX eek ?
    this._upScore();
  },
  onDeleteURI: function HT_onDeleteURI(uri) {
    this._log.trace("onDeleteURI: " + itemId);
    this.addChangedID(this._store._getGUID(uri)); // FIXME eek
    this._upScore();
  },
  onClearHistory: function HT_onClearHistory() {
    this._log.trace("onClearHistory");
    this._score += 50;
  }
};
