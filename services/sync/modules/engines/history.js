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

const GUID_ANNO = "weave/guid";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/stores.js");
Cu.import("resource://services-sync/trackers.js");
Cu.import("resource://services-sync/type_records/history.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/log4moz.js");

function HistoryEngine() {
  SyncEngine.call(this, "History");
}
HistoryEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _recordObj: HistoryRec,
  _storeObj: HistoryStore,
  _trackerObj: HistoryTracker,

  _sync: Utils.batchSync("History", SyncEngine),

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
    this.__hsvc = null;
    this._stmts = [];
  }, this);
}
HistoryStore.prototype = {
  __proto__: Store.prototype,

  __hsvc: null,
  get _hsvc() {
    if (!this.__hsvc)
      this.__hsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                    getService(Ci.nsINavHistoryService).
                    QueryInterface(Ci.nsIGlobalHistory2).
                    QueryInterface(Ci.nsIBrowserHistory).
                    QueryInterface(Ci.nsPIPlacesDatabase);
    return this.__hsvc;
  },

  get _db() {
    return this._hsvc.DBConnection;
  },

  _stmts: {},
  _getStmt: function(query) {
    if (query in this._stmts)
      return this._stmts[query];

    this._log.trace("Creating SQL statement: " + query);
    return this._stmts[query] = Utils.createStatement(this._db, query);
  },

  get _haveTempTablesStm() {
    return this._getStmt(
      "SELECT name FROM sqlite_temp_master " +
      "WHERE name IN ('moz_places_temp', 'moz_historyvisits_temp')");
  },

  get _haveTempTables() {
    if (this.__haveTempTables == null)
      this.__haveTempTables = !!Utils.queryAsync(this._haveTempTablesStm,
                                                 ["name"]).length;
    return this.__haveTempTables;
  },

  get _addGUIDAnnotationNameStm() {
    let stmt = this._getStmt(
      "INSERT OR IGNORE INTO moz_anno_attributes (name) VALUES (:anno_name)");
    stmt.params.anno_name = GUID_ANNO;
    return stmt;
  },

  get _checkGUIDPageAnnotationStm() {
    let base =
      "SELECT h.id AS place_id, " +
        "(SELECT id FROM moz_anno_attributes WHERE name = :anno_name) AS name_id, " +
        "a.id AS anno_id, a.dateAdded AS anno_date ";
    let stmt;
    if (this._haveTempTables) {
      // Gecko <2.0
      stmt = this._getStmt(base +
        "FROM (SELECT id FROM moz_places_temp WHERE url = :page_url " +
              "UNION " +
              "SELECT id FROM moz_places WHERE url = :page_url) AS h " +
        "LEFT JOIN moz_annos a ON a.place_id = h.id " +
                             "AND a.anno_attribute_id = name_id");
    } else {
      // Gecko 2.0
      stmt = this._getStmt(base +
        "FROM moz_places h " + 
        "LEFT JOIN moz_annos a ON a.place_id = h.id " +
                             "AND a.anno_attribute_id = name_id " +
        "WHERE h.url = :page_url");
    }
    stmt.params.anno_name = GUID_ANNO;
    return stmt;
  },

  get _addPageAnnotationStm() {
    return this._getStmt(
    "INSERT OR REPLACE INTO moz_annos " +
      "(id, place_id, anno_attribute_id, mime_type, content, flags, " +
       "expiration, type, dateAdded, lastModified) " +
    "VALUES (:id, :place_id, :name_id, :mime_type, :content, :flags, " +
            ":expiration, :type, :date_added, :last_modified)");
  },

  // Some helper functions to handle GUIDs
  setGUID: function setGUID(uri, guid) {
    uri = uri.spec ? uri.spec : uri;

    if (arguments.length == 1)
      guid = Utils.makeGUID();

    // Ensure annotation name exists
    Utils.queryAsync(this._addGUIDAnnotationNameStm);

    let stmt = this._checkGUIDPageAnnotationStm;
    stmt.params.page_url = uri;
    let result = Utils.queryAsync(stmt, ["place_id", "name_id", "anno_id",
                                         "anno_date"])[0];
    if (!result) {
      let log = Log4Moz.repository.getLogger("Engine.History");
      log.warn("Couldn't annotate URI " + uri);
      return guid;
    }

    stmt = this._addPageAnnotationStm;
    if (result.anno_id) {
      stmt.params.id = result.anno_id;
      stmt.params.date_added = result.anno_date;
    } else {
      stmt.params.id = null;
      stmt.params.date_added = Date.now() * 1000;
    }
    stmt.params.place_id = result.place_id;
    stmt.params.name_id = result.name_id;
    stmt.params.content = guid;
    stmt.params.flags = 0;
    stmt.params.expiration = Ci.nsIAnnotationService.EXPIRE_WITH_HISTORY;
    stmt.params.type = Ci.nsIAnnotationService.TYPE_STRING;
    stmt.params.last_modified = Date.now() * 1000;
    Utils.queryAsync(stmt);

    return guid;
  },

  get _guidStm() {
    let base =
      "SELECT a.content AS guid " +
      "FROM moz_annos a " +
      "JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id ";
    let stm;
    if (this._haveTempTables) {
      // Gecko <2.0
      stm = this._getStmt(base +
        "JOIN ( " +
          "SELECT id FROM moz_places_temp WHERE url = :page_url " +
          "UNION " +
          "SELECT id FROM moz_places WHERE url = :page_url " +
        ") AS h ON h.id = a.place_id " +
        "WHERE n.name = :anno_name");
    } else {
      // Gecko 2.0
      stm = this._getStmt(base +
        "JOIN moz_places h ON h.id = a.place_id " +
        "WHERE n.name = :anno_name AND h.url = :page_url");
    }
    stm.params.anno_name = GUID_ANNO;
    return stm;
  },

  GUIDForUri: function GUIDForUri(uri, create) {
    let stm = this._guidStm;
    stm.params.page_url = uri.spec ? uri.spec : uri;

    // Use the existing GUID if it exists
    let result = Utils.queryAsync(stm, ["guid"])[0];
    if (result)
      return result.guid;

    // Give the uri a GUID if it doesn't have one
    if (create)
      return this.setGUID(uri);
  },

  get _visitStm() {
    // Gecko <2.0
    if (this._haveTempTables) {
      let where = 
        "WHERE place_id = IFNULL( " +
          "(SELECT id FROM moz_places_temp WHERE url = :url), " +
          "(SELECT id FROM moz_places WHERE url = :url) " +
        ") ";
      return this._getStmt(
        "SELECT visit_type type, visit_date date " +
        "FROM moz_historyvisits_temp " + where + "UNION " +
        "SELECT visit_type type, visit_date date " +
        "FROM moz_historyvisits " + where +
        "ORDER BY date DESC LIMIT 10 ");
    }
    // Gecko 2.0
    return this._getStmt(
      "SELECT visit_type type, visit_date date " +
      "FROM moz_historyvisits " +
      "WHERE place_id = (SELECT id FROM moz_places WHERE url = :url) " +
      "ORDER BY date DESC LIMIT 10");
  },

  get _urlStm() {
    let where =
      "WHERE id = (" +
        "SELECT place_id " +
        "FROM moz_annos " +
        "WHERE content = :guid AND anno_attribute_id = (" +
          "SELECT id " +
          "FROM moz_anno_attributes " +
          "WHERE name = '" + GUID_ANNO + "')) ";
    // Gecko <2.0
    if (this._haveTempTables)
      return this._getStmt(
        "SELECT url, title, frecency FROM moz_places_temp " + where +
        "UNION ALL " +
        "SELECT url, title, frecency FROM moz_places " + where + "LIMIT 1");
    // Gecko 2.0
    return this._getStmt(
      "SELECT url, title, frecency FROM moz_places " + where + "LIMIT 1");
  },

  get _allUrlStm() {
    // Gecko <2.0
    if (this._haveTempTables)
      return this._getStmt(
        "SELECT url, frecency FROM moz_places_temp " +
        "WHERE last_visit_date > :cutoff_date " +
        "UNION " +
        "SELECT url, frecency FROM moz_places " +
        "WHERE last_visit_date > :cutoff_date " +
        "ORDER BY 2 DESC " +
        "LIMIT :max_results");

    // Gecko 2.0
    return this._getStmt(
      "SELECT url " +
      "FROM moz_places " +
      "WHERE last_visit_date > :cutoff_date " +
      "ORDER BY frecency DESC " +
      "LIMIT :max_results");
  },

  // See bug 320831 for why we use SQL here
  _getVisits: function HistStore__getVisits(uri) {
    this._visitStm.params.url = uri;
    return Utils.queryAsync(this._visitStm, ["date", "type"]);
  },

  // See bug 468732 for why we use SQL here
  _findURLByGUID: function HistStore__findURLByGUID(guid) {
    this._urlStm.params.guid = guid;
    return Utils.queryAsync(this._urlStm, ["url", "title", "frecency"])[0];
  },

  changeItemID: function HStore_changeItemID(oldID, newID) {
    this.setGUID(this._findURLByGUID(oldID).url, newID);
  },


  getAllIDs: function HistStore_getAllIDs() {
    // Only get places visited within the last 30 days (30*24*60*60*1000ms)
    this._allUrlStm.params.cutoff_date = (Date.now() - 2592000000) * 1000;
    this._allUrlStm.params.max_results = 5000;

    let urls = Utils.queryAsync(this._allUrlStm, "url");
    let self = this;
    return urls.reduce(function(ids, item) {
      ids[self.GUIDForUri(item.url, true)] = item.url;
      return ids;
    }, {});
  },

  create: function HistStore_create(record) {
    // Add the url and set the GUID
    this.update(record);
    this.setGUID(record.histUri, record.id);
  },

  remove: function HistStore_remove(record) {
    let page = this._findURLByGUID(record.id);
    if (page == null) {
      this._log.debug("Page already removed: " + record.id);
      return;
    }

    let uri = Utils.makeURI(page.url);
    Svc.History.removePage(uri);
    this._log.trace("Removed page: " + [record.id, page.url, page.title]);
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

    // Add visits if there's no local visit with the same date
    for each (let {date, type} in record.visits)
      if (curvisits.every(function(cur) cur.date != date))
        Svc.History.addVisit(uri, date, null, type, type == 5 || type == 6, 0);

    this._hsvc.setPageTitle(uri, record.title);
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

  createRecord: function createRecord(guid, uri) {
    let foo = this._findURLByGUID(guid);
    let record = new HistoryRec(uri);
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
    this._hsvc.removeAllPages();
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
          Svc.History.addObserver(this, true);
          this._enabled = true;
        }
        break;
      case "weave:engine:stop-tracking":
        if (this._enabled) {
          Svc.History.removeObserver(this);
          this._enabled = false;
        }
        break;
    }
  },

  _GUIDForUri: function _GUIDForUri(uri, create) {
    // Isn't indirection fun...
    return Engines.get("history")._store.GUIDForUri(uri, create);
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavHistoryObserver,
    Ci.nsINavHistoryObserver_MOZILLA_1_9_1_ADDITIONS,
    Ci.nsISupportsWeakReference
  ]),

  onBeginUpdateBatch: function HT_onBeginUpdateBatch() {},
  onEndUpdateBatch: function HT_onEndUpdateBatch() {},
  onPageChanged: function HT_onPageChanged() {},
  onTitleChanged: function HT_onTitleChanged() {},

  /* Every add or remove is worth 1 point.
   * Clearing the whole history is worth 50 points (see below)
   */
  _upScore: function BMT__upScore() {
    this.score += 1;
  },

  onVisit: function HT_onVisit(uri, vid, time, session, referrer, trans) {
    if (this.ignoreAll)
      return;
    this._log.trace("onVisit: " + uri.spec);
    if (this.addChangedID(this._GUIDForUri(uri, true)))
      this._upScore();
  },
  onDeleteVisits: function onDeleteVisits() {
  },
  onPageExpired: function HT_onPageExpired(uri, time, entry) {
  },
  onBeforeDeleteURI: function onBeforeDeleteURI(uri) {
    if (this.ignoreAll)
      return;
    this._log.trace("onBeforeDeleteURI: " + uri.spec);
    if (this.addChangedID(this._GUIDForUri(uri, true)))
      this._upScore();
  },
  onDeleteURI: function HT_onDeleteURI(uri) {
  },
  onClearHistory: function HT_onClearHistory() {
    this._log.trace("onClearHistory");
    this.score += 500;
  }
};
