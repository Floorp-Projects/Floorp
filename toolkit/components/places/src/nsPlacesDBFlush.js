/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *   Marco Bonardo <mak77@bonardo.net>
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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

////////////////////////////////////////////////////////////////////////////////
//// Constants

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const kQuitApplication = "quit-application";
const kSyncFinished = "places-sync-finished";

const kSyncPrefName = "places.syncDBTableIntervalInSecs";
const kDefaultSyncInterval = 120;
const kExpireDaysPrefName = "browser.history_expire_days";
const kDefaultExpireDays = 90;

// The number of milliseconds in a day.
const kMSPerDay = 86400000;

// The max number of entries we will flush out when we expire.
const kMaxExpire = 24;

// Query Constants.  These describe the queries we use.
const kQuerySyncPlacesId = 0;
const kQuerySyncHistoryVisitsId = 1;
const kQuerySelectExpireVisitsId = 2;
const kQueryExpireVisitsId = 3;

////////////////////////////////////////////////////////////////////////////////
//// nsPlacesDBFlush class

function nsPlacesDBFlush()
{
  this._prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch);

  // Get our sync interval
  try {
    // We want to silently fail since getIntPref throws if it does not exist,
    // and use a default to fallback to.
    this._syncInterval = this._prefs.getIntPref(kSyncPrefName);
    if (this._syncInterval <= 0)
      this._syncInterval = kDefaultSyncInterval;
  }
  catch (e) {
    // The preference did not exist, so use the default.
    this._syncInterval = kDefaultSyncInterval;
  }

  // Get our maximum allowable age of visits in days.
  try {
    // We want to silently fail since getIntPref throws if it does not exist,
    // and use a default to fallback to.
    this._expireDays = this._prefs.getIntPref(kExpireDaysPrefName);
    if (this._expireDays <= 0)
      this._expireDays = kDefaultExpireDays;
  }
  catch (e) {
    // The preference did not exist, so use the default.
    this._expireDays = kDefaultExpireDays;
  }

  // Register observers
  this._os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);
  this._os.addObserver(this, kQuitApplication, false);

  let (pb2 = this._prefs.QueryInterface(Ci.nsIPrefBranch2)) {
    pb2.addObserver(kSyncPrefName, this, false);
    pb2.addObserver(kExpireDaysPrefName, this, false);
  }

  // Create our timer to update everything
  this._timer = this._newTimer();

  //////////////////////////////////////////////////////////////////////////////
  //// Smart Getters

  this.__defineGetter__("_db", function() {
    delete this._db;
    return this._db = Cc["@mozilla.org/browser/nav-history-service;1"].
                      getService(Ci.nsPIPlacesDatabase).
                      DBConnection;
  });

  this.__defineGetter__("_ios", function() {
    delete this._ios;
    return this._ios = Cc["@mozilla.org/network/io-service;1"].
                       getService(Ci.nsIIOService);
  });

  this.__defineGetter__("_hsn", function() {
    delete this._hsn;
    return this._hsn = Cc["@mozilla.org/browser/nav-history-service;1"].
                       getService(Ci.nsPIPlacesHistoryListenersNotifier);
  });

  this.__defineGetter__("_bs", function() {
    delete this._bs;
    return this._bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                      getService(Ci.nsINavBookmarksService);
  });
}

nsPlacesDBFlush.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function DBFlush_observe(aSubject, aTopic, aData)
  {
    if (aTopic == kQuitApplication) {
      this._os.removeObserver(this, kQuitApplication);
      let (pb2 = this._prefs.QueryInterface(Ci.nsIPrefBranch2)) {
        pb2.removeObserver(kSyncPrefName, this);
        pb2.removeObserver(kExpireDaysPrefName, this);
      }
      this._timer.cancel();
      this._timer = null;
      // Other components could still make changes to history at this point,
      // for example to clear private data on shutdown, so here we dispatch
      // an event to the main thread so that we will sync after
      // quit-application ensuring all data have been saved.
      let tm = Cc["@mozilla.org/thread-manager;1"].
          getService(Ci.nsIThreadManager);
      tm.mainThread.dispatch({
        _self: this,
        run: function() {
          let pip = Cc["@mozilla.org/browser/nav-history-service;1"].
                    getService(Ci.nsPIPlacesDatabase);
          pip.commitPendingChanges();
          this._self._flushWithQueries([kQuerySyncPlacesId, kQuerySyncHistoryVisitsId]);
          // Close the database connection, this was the last sync and we can't
          // ensure database coherence from now on.
          pip.finalizeInternalStatements();
          this._self._finalizeInternalStatements();
          this._self._db.close();
        }
      }, Ci.nsIThread.DISPATCH_NORMAL);

    }
    else if (aTopic == "nsPref:changed" && aData == kSyncPrefName) {
      // Get the new pref value, and then update our timer
      this._syncInterval = this._prefs.getIntPref(kSyncPrefName);
      if (this._syncInterval <= 0)
        this._syncInterval = kDefaultSyncInterval;

      // We may have canceled the timer already for batch updates, so we want to
      // exit early.
      if (!this._timer)
        return;

      this._timer.cancel();
      this._timer = this._newTimer();
    }
    else if (aTopic == "nsPref:changed" && aData == kExpireDaysPrefName) {
      // Get the new pref and store it.
      this._expireDays = this._prefs.getIntPref(kExpireDaysPrefName);
      if (this._expireDays <= 0)
        this._expireDays = kDefaultExpireDays;
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsINavBookmarkObserver

  onBeginUpdateBatch: function DBFlush_onBeginUpdateBatch()
  {
    this._inBatchMode = true;

    // We do not want to sync while we are doing batch work.
    this._timer.cancel();
    this._timer = null;
  },

  onEndUpdateBatch: function DBFlush_onEndUpdateBatch()
  {
    this._inBatchMode = false;

    // Restore our timer
    this._timer = this._newTimer();

    // We need to sync now
    this._flushWithQueries([kQuerySyncPlacesId, kQuerySyncHistoryVisitsId]);
  },

  onItemAdded: function(aItemId, aParentId, aIndex)
  {
    // Sync only if we added a TYPE_BOOKMARK item.  Note, we want to run the
    // least amount of queries as possible here for performance reasons.
    if (!this._inBatchMode &&
        this._bs.getItemType(aItemId) == this._bs.TYPE_BOOKMARK)
      this._flushWithQueries([kQuerySyncPlacesId]);
  },

  onItemChanged: function DBFlush_onItemChanged(aItemId, aProperty,
                                                aIsAnnotationProperty, aValue)
  {
    if (!this._inBatchMode && aProperty == "uri")
      this._flushWithQueries([kQuerySyncPlacesId]);
  },

  onBeforeItemRemoved: function() { },
  onItemRemoved: function() { },
  onItemVisited: function() { },
  onItemMoved: function() { },

  //////////////////////////////////////////////////////////////////////////////
  //// nsINavHistoryObserver

  // We currently only use the history observer to know when the history service
  // is activated.  At that point, we actually get initialized, and our timer
  // to sync history is added.

  // These methods share the name of the ones on nsINavBookmarkObserver, so
  // the implementations can be found above.
  //onBeginUpdateBatch: function() { },
  //onEndUpdateBatch: function() { },
  onVisit: function(aURI, aVisitID, aTime, aSessionID, aReferringID, aTransitionType) { },
  onTitleChanged: function(aURI, aPageTitle) { },
  onDeleteURI: function(aURI) { },
  onClearHistory: function() { },
  onPageChanged: function(aURI, aWhat, aValue) { },
  onPageExpired: function(aURI, aVisitTime, aWholeEntry) { },

  //////////////////////////////////////////////////////////////////////////////
  //// nsITimerCallback

  notify: function DBFlush_timerCallback()
  {
    let queries = [
      kQuerySelectExpireVisitsId,
      kQueryExpireVisitsId,
      kQuerySyncPlacesId,
      kQuerySyncHistoryVisitsId,
    ];
    this._flushWithQueries(queries);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageStatementCallback

  handleResult: function DBFlush_handleResult(aResultSet)
  {
    // The only results we'll ever get back is for notifying about expiration.
    if (!this._expiredResults)
      this._expiredResults = [];

    let row;
    while (row = aResultSet.getNextRow()) {
      if (row.getResultByName("hidden"))
        continue;

      this._expiredResults.push({
        uri: this._ios.newURI(row.getResultByName("url"), null, null),
        visitDate: row.getResultByName("visit_date"),
        wholeEntry: (row.getResultByName("visit_count") == 1)
      });
    }
  },

  handleError: function DBFlush_handleError(aError)
  {
    Cu.reportError("Async statement execution returned with '" +
                   aError.result + "', '" + aError.message + "'");
  },

  handleCompletion: function DBFlush_handleCompletion(aReason)
  {
    if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
      // Dispatch to history that we've finished expiring if we have results.
      if (this._expiredResults) {
        while (this._expiredResults.length) {
          let visit = this._expiredResults.shift();
          this._hsn.notifyOnPageExpired(visit.uri, visit.visitDate,
                                        visit.wholeEntry);
        }

        // And reset it...
        delete this._expiredResults;
      }

      // Dispatch a notification that sync has finished.
      this._os.notifyObservers(null, kSyncFinished, null);
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsPlacesDBFlush
  _syncInterval: kDefaultSyncInterval,

  /**
   * Execute async statements to flush tables with the specified queries.
   *
   * @param aQueryNames
   *        The names of the queries to use with this flush.
   */
  _flushWithQueries: function DBFlush_flushWithQueries(aQueryNames)
  {
    // No need to do extra work if we are in batch mode
    if (this._inBatchMode)
      return;

    let statements = [];
    for (let i = 0; i < aQueryNames.length; i++)
      statements.push(this._getQuery(aQueryNames[i]));

    // Execute sync statements async in a transaction
    this._db.executeAsync(statements, statements.length, this);
  },

  /**
   * Finalizes all of our mozIStorageStatements so we can properly close the
   * database.
   */
  _finalizeInternalStatements: function DBFlush_finalizeInternalStatements()
  {
    for each (let stmt in this._cachedStatements)
      if (stmt instanceof Ci.mozIStorageStatement)
        stmt.finalize();
  },

  /**
   * Generate the statement to synchronizes the moz_{aTableName} and
   * moz_{aTableName}_temp by copying all the data from the temporary table
   * into the permanent one.
   * Most of the work is done through triggers defined in nsPlacesTriggers.h,
   * they sync back to disk, then delete the data in the temporary table.
   *
   * @param aQueryType
   *        Type of the query to build statement for.
   */
  _cachedStatements: [],
  _getQuery: function DBFlush_getQuery(aQueryType)
  {
    // Statement creating can be expensive, so always cache if we can.
    if (aQueryType in this._cachedStatements) {
      let stmt = this._cachedStatements[aQueryType];

      // Bind the appropriate parameters.
      let params = stmt.params;
      switch (aQueryType) {
        case kQuerySyncHistoryVisitsId:
        case kQuerySyncPlacesId:
          params.transition_type = Ci.nsINavHistoryService.TRANSITION_EMBED;
          break;
        case kQuerySelectExpireVisitsId:
        case kQueryExpireVisitsId:
          params.visit_date = (Date.now() - (this._expireDays * kMSPerDay)) * 1000;
          params.max_expire = kMaxExpire;
          break;
      }

      return stmt;
    }

    switch(aQueryType) {
      case kQuerySyncHistoryVisitsId:
        // For history table we want to leave embed visits in memory, since
        // those are expired with current session, so we are filtering them out.
        this._cachedStatements[aQueryType] = this._db.createStatement(
          "DELETE FROM moz_historyvisits_temp " +
          "WHERE visit_type <> :transition_type"
        );
        break;

      case kQuerySyncPlacesId:
        // For places table we want to leave places associated with embed visits
        // in memory, they usually have hidden = 1 and at least an embed visit
        // in historyvisits_temp table.
        this._cachedStatements[aQueryType] = this._db.createStatement(
          "DELETE FROM moz_places_temp " +
          "WHERE id IN ( " +
            "SELECT id FROM moz_places_temp h " +
            "WHERE h.hidden <> 1 OR NOT EXISTS ( " +
              "SELECT id FROM moz_historyvisits_temp " +
              "WHERE place_id = h.id AND visit_type = :transition_type " +
              "LIMIT 1 " +
            ") " +
          ")"
        );
        break;

      case kQuerySelectExpireVisitsId:
        // Determine which entries will be flushed out from moz_historyvisits
        // when kQueryExpireVisitsId runs.
        this._cachedStatements[aQueryType] = this._db.createStatement(
          "SELECT h.url, v.visit_date, h.hidden, h.visit_count " +
          "FROM moz_places h " +
          "JOIN moz_historyvisits v ON h.id = v.place_id " +
          "WHERE v.visit_date < :visit_date " +
          "ORDER BY v.visit_date ASC " +
          "LIMIT :max_expire"
        );
        break;

      case kQueryExpireVisitsId:
        // Flush out entries from moz_historyvisits
        this._cachedStatements[aQueryType] = this._db.createStatement(
          "DELETE FROM moz_historyvisits " +
          "WHERE id IN ( " +
            "SELECT id " +
            "FROM moz_historyvisits " +
            "WHERE visit_date < :visit_date " +
            "ORDER BY visit_date ASC " +
            "LIMIT :max_expire " +
          ")"
        );
        break;

      default:
        throw "Unexpected statement!";
    }

    // We only bind our own parameters when we have a cached statement, so we
    // call ourself since we now have a cached statement.
    return this._getQuery(aQueryType);
  },

  /**
   * Creates a new timer based on this._syncInterval.
   *
   * @returns a REPEATING_SLACK nsITimer that runs every this._syncInterval.
   */
  _newTimer: function DBFlush_newTimer()
  {
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(this, this._syncInterval * 1000,
                           Ci.nsITimer.TYPE_REPEATING_SLACK);
    return timer;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  classDescription: "Used to synchronize the temporary and permanent tables of Places",
  classID: Components.ID("c1751cfc-e8f1-4ade-b0bb-f74edfb8ef6a"),
  contractID: "@mozilla.org/places/sync;1",

  // Registering in these categories makes us get initialized when either of
  // those listeners would be notified.
  _xpcom_categories: [
    { category: "bookmark-observers" },
    { category: "history-observers" },
  ],

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsINavBookmarkObserver,
    Ci.nsINavHistoryObserver,
    Ci.nsITimerCallback,
    Ci.mozIStorageStatementCallback,
  ])
};

////////////////////////////////////////////////////////////////////////////////
//// Module Registration

let components = [nsPlacesDBFlush];
function NSGetModule(compMgr, fileSpec)
{
  return XPCOMUtils.generateModule(components);
}
