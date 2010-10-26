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

////////////////////////////////////////////////////////////////////////////////
//// Constants

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

// A database flush should be the last operation during the shutdown process.
const kTopicShutdown = "places-connection-closing";
const kSyncFinished = "places-sync-finished";
const kDebugStopSync = "places-debug-stop-sync";
const kDebugStartSync = "places-debug-start-sync";

const kSyncPrefName = "places.syncDBTableIntervalInSecs";
const kDefaultSyncInterval = 120;

// Query Constants.  These describe the queries we use.
const kQuerySyncPlacesId = 0;
const kQuerySyncHistoryVisitsId = 1;

////////////////////////////////////////////////////////////////////////////////
//// Modules

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");

////////////////////////////////////////////////////////////////////////////////
//// nsPlacesDBFlush class

function nsPlacesDBFlush()
{
  // Get our sync interval
  try {
    // We want to silently fail since getIntPref throws if it does not exist,
    // and use a default to fallback to.
    this._syncInterval = Services.prefs.getIntPref(kSyncPrefName);
    if (this._syncInterval <= 0)
      this._syncInterval = kDefaultSyncInterval;
  }
  catch (e) {
    // The preference did not exist, so use the default.
    this._syncInterval = kDefaultSyncInterval;
  }

  // Register observers
  Services.obs.addObserver(this, kTopicShutdown, false);
  Services.obs.addObserver(this, kDebugStopSync, false);
  Services.obs.addObserver(this, kDebugStartSync, false);

  let (pb2 = Services.prefs.QueryInterface(Ci.nsIPrefBranch2))
    pb2.addObserver(kSyncPrefName, this, false);

  // Create our timer to update everything
  this._timer = this._newTimer();

  //////////////////////////////////////////////////////////////////////////////
  //// Smart Getters

  XPCOMUtils.defineLazyGetter(this, "_db", function() {
    return PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                              .DBConnection;
  });
}

nsPlacesDBFlush.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function DBFlush_observe(aSubject, aTopic, aData)
  {
    if (aTopic == kTopicShutdown) {
      Services.obs.removeObserver(this, kTopicShutdown);
      Services.obs.removeObserver(this, kDebugStopSync);
      Services.obs.removeObserver(this, kDebugStartSync);

      let (pb2 = Services.prefs.QueryInterface(Ci.nsIPrefBranch2))
        pb2.removeObserver(kSyncPrefName, this);

      if (this._timer) {
        this._timer.cancel();
        this._timer = null;
      }

      // Flush any remaining change to disk tables.
      this._flushWithQueries([kQuerySyncPlacesId, kQuerySyncHistoryVisitsId]);
      this._finalizeInternalStatements();
    }
    else if (aTopic == "nsPref:changed" && aData == kSyncPrefName) {
      // Get the new pref value, and then update our timer
      this._syncInterval = Services.prefs.getIntPref(kSyncPrefName);
      if (this._syncInterval <= 0)
        this._syncInterval = kDefaultSyncInterval;

      // We may have canceled the timer already for batch updates, so we want to
      // exit early.
      if (!this._timer)
        return;

      this._timer.cancel();
      this._timer = this._newTimer();
    }
    else if (aTopic == kDebugStopSync) {
      this._syncStopped = true;
    }
    else if (aTopic == kDebugStartSync) {
      if (_syncStopped in this)
        delete this._syncStopped;
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

  onItemAdded: function(aItemId, aParentId, aIndex, aItemType, aURI)
  {
    // Sync only if we added a TYPE_BOOKMARK item.  Note, we want to run the
    // least amount of queries as possible here for performance reasons.
    if (!this._inBatchMode && aItemType == PlacesUtils.bookmarks.TYPE_BOOKMARK)
      this._flushWithQueries([kQuerySyncPlacesId]);
  },

  onItemChanged: function DBFlush_onItemChanged(aItemId, aProperty,
                                                aIsAnnotationProperty,
                                                aNewValue, aLastModified,
                                                aItemType)
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
  onVisit: function() { },
  onTitleChanged: function() { },
  onBeforeDeleteURI: function() { },
  onDeleteURI: function() { },
  onClearHistory: function() { },
  onPageChanged: function() { },
  onDeleteVisits: function() { },

  //////////////////////////////////////////////////////////////////////////////
  //// nsITimerCallback

  notify: function DBFlush_timerCallback()
  {
    let queries = [
      kQuerySyncPlacesId,
      kQuerySyncHistoryVisitsId,
    ];
    this._flushWithQueries(queries);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageStatementCallback

  handleResult: function DBFlush_handleResult(aResultSet)
  {
  },

  handleError: function DBFlush_handleError(aError)
  {
    Cu.reportError("Async statement execution returned with '" +
                   aError.result + "', '" + aError.message + "'");
  },

  handleCompletion: function DBFlush_handleCompletion(aReason)
  {
    if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
      // Dispatch a notification that sync has finished.
      Services.obs.notifyObservers(null, kSyncFinished, null);
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
    if (this._inBatchMode || this._syncStopped)
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
    this._cachedStatements.forEach(function(stmt) {
      if (stmt instanceof Ci.mozIStorageAsyncStatement)
        stmt.finalize();
    });
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
      }

      return stmt;
    }

    switch(aQueryType) {
      case kQuerySyncHistoryVisitsId:
        // For history table we want to leave embed visits in memory, since
        // those are expired with current session, so we are filtering them out.
        // Notice that instead we _want_ to sync framed_link visits, since those
        // come from user's actions.
        this._cachedStatements[aQueryType] = this._db.createAsyncStatement(
          "DELETE FROM moz_historyvisits_temp " +
          "WHERE visit_type <> :transition_type"
        );
        break;

      case kQuerySyncPlacesId:
        // For places table we want to leave places associated with embed visits
        // in memory, they usually have hidden = 1 and at least an embed visit
        // in historyvisits_temp table.
        this._cachedStatements[aQueryType] = this._db.createAsyncStatement(
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

  classID: Components.ID("c1751cfc-e8f1-4ade-b0bb-f74edfb8ef6a"),

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
var NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
