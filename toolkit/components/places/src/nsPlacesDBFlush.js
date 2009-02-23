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

const kSyncPrefName = "syncDBTableIntervalInSecs";
const kDefaultSyncInterval = 120;

////////////////////////////////////////////////////////////////////////////////
//// nsPlacesDBFlush class

function nsPlacesDBFlush()
{
  this._prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefService).
                getBranch("places.");

  // Get our sync interval
  try {
    // We want to silently fail if the preference does not exist, and use a
    // default to fallback to.
    this._syncInterval = this._prefs.getIntPref(kSyncPrefName);
    if (this._syncInterval <= 0)
      this._syncInterval = kDefaultSyncInterval;
  }
  catch (e) {
    // The preference did not exist, so default to two minutes.
    this._syncInterval = kDefaultSyncInterval;
  }

  // Register observers
  this._bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
             getService(Ci.nsINavBookmarksService);
  this._bs.addObserver(this, false);

  this._os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);
  this._os.addObserver(this, kQuitApplication, false);

  this._prefs.QueryInterface(Ci.nsIPrefBranch2)
             .addObserver("", this, false);

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

}

nsPlacesDBFlush.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function DBFlush_observe(aSubject, aTopic, aData)
  {
    if (aTopic == kQuitApplication) {
      this._bs.removeObserver(this);
      this._os.removeObserver(this, kQuitApplication);
      this._prefs.QueryInterface(Ci.nsIPrefBranch2).removeObserver("", this);
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
          this._self._syncTables(["places", "historyvisits"]);
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
      this._syncInterval = aSubject.getIntPref(kSyncPrefName);
      if (this._syncInterval <= 0)
        this._syncInterval = kDefaultSyncInterval;

      // We may have canceled the timer already for batch updates, so we want to
      // exit early.
      if (!this._timer)
        return;

      this._timer.cancel();
      this._timer = this._newTimer();
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
    this._syncTables(["places", "historyvisits"]);
  },

  onItemAdded: function(aItemId, aParentId, aIndex)
  {
    // Sync only if we added a TYPE_BOOKMARK item
    if (!this._inBatchMode &&
        this._bs.getItemType(aItemId) == this._bs.TYPE_BOOKMARK)
      this._syncTables(["places"]);
  },

  onItemChanged: function DBFlush_onItemChanged(aItemId, aProperty,
                                                         aIsAnnotationProperty,
                                                         aValue)
  {
    if (!this._inBatchMode && aProperty == "uri")
      this._syncTables(["places"]);
  },

  onItemRemoved: function() { },
  onItemVisited: function() { },
  onItemMoved: function() { },

  //////////////////////////////////////////////////////////////////////////////
  //// nsITimerCallback

  notify: function() this._syncTables(["places", "historyvisits"]),

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageStatementCallback

  handleError: function DBFlush_handleError(aError)
  {
    Cu.reportError("Async statement execution returned with '" +
                   aError.result + "', '" + aError.message + "'");
  },

  handleCompletion: function DBFlush_handleCompletion(aReason)
  {
    if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
      // Dispatch a notification that sync has finished.
      this._os.notifyObservers(null, kSyncFinished, null);
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsPlacesDBFlush
  _syncInterval: kDefaultSyncInterval,

  /**
   * Execute async statements to sync temporary places table.
   * @param aTableNames
   *        array of table names that should be synced, as moz_{TableName}_temp.
   */
  _syncTables: function DBFlush_syncTables(aTableNames)
  {
    // No need to do extra work if we are in batch mode
    if (this._inBatchMode)
      return;

    let statements = [];
    for (let i = 0; i < aTableNames.length; i++)
      statements.push(this._getSyncTableStatement(aTableNames[i]));

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
   * @param aTableName
   *        name of the table to build statement for, as moz_{TableName}_temp.
   */
  _cachedStatements: {},
  _getSyncTableStatement: function DBFlush_getSyncTableStatement(aTableName)
  {
    // Statement creating can be expensive, so always cache if we can.
    if (aTableName in this._cachedStatements)
      return this._cachedStatements[aTableName];

    // Delete all the data in the temp table.
    // We have triggers setup that ensure that the data is transferred over
    // upon deletion.
    let condition = "";
    switch(aTableName) {
      case "historyvisits":
        // For history table we want to leave embed visits in memory, since
        // those are expired with current session, so we are filtering them out.
        condition = "WHERE visit_type <> " + Ci.nsINavHistoryService.TRANSITION_EMBED;
        break;
      case "places":
        // For places table we want to leave places associated with embed visits
        // in memory, they usually have hidden = 1 and at least an embed visit
        // in historyvisits_temp table.
        condition = "WHERE id IN (SELECT id FROM moz_places_temp h " +
                                  "WHERE h.hidden <> 1 OR NOT EXISTS ( " +
                                    "SELECT id FROM moz_historyvisits_temp " +
                                    "WHERE place_id = h.id AND visit_type = " +
                                    Ci.nsINavHistoryService.TRANSITION_EMBED +
                                    " LIMIT 1) " +
                                  ")";
        break;
    }

    let sql = "DELETE FROM moz_" + aTableName + "_temp " + condition;
    return this._cachedStatements[aTableName] = this._db.createStatement(sql);
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
  _xpcom_categories: [{
    category: "profile-after-change",
  }],

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsINavBookmarkObserver,
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
