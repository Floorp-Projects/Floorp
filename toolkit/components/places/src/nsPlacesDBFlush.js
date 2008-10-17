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
Components.utils.import("resource://gre/modules/PlacesBackground.jsm");

////////////////////////////////////////////////////////////////////////////////
//// Constants

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const kPlacesBackgroundShutdown = "places-background-shutdown";

const kSyncPrefName = "syncDBTableIntervalInSecs";
const kDefaultSyncInterval = 120;

////////////////////////////////////////////////////////////////////////////////
//// nsPlacesDBFlush class

function nsPlacesDBFlush()
{
  //////////////////////////////////////////////////////////////////////////////
  //// Smart Getters

  this.__defineGetter__("_db", function() {
    delete this._db;
    return this._db = Cc["@mozilla.org/browser/nav-history-service;1"].
                      getService(Ci.nsPIPlacesDatabase).
                      DBConnection;
  });

  this.__defineGetter__("_bh", function() {
    delete this._bh;
    return this._bh = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                      getService(Ci.nsINavBookmarksService);
  });


  // Get our sync interval
  this._prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefService).
                getBranch("places.");
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
  this._bh.addObserver(this, false);

  this._prefs.QueryInterface(Ci.nsIPrefBranch2).addObserver("", this, false);

  let os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  os.addObserver(this, kPlacesBackgroundShutdown, false);

  // Create our timer to update everything
  this._timer = this._newTimer();
}

nsPlacesDBFlush.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function DBFlush_observe(aSubject, aTopic, aData)
  {
    if (aTopic == kPlacesBackgroundShutdown) {
      this._bh.removeObserver(this);
      this._timer.cancel();
      this._timer = null;
      this._syncAll();
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

    // We need to sync and restore our timer now.
    this._syncAll();
    this._timer = this._newTimer();
  },

  onItemAdded: function() this._syncMozPlaces(),

  onItemChanged: function DBFlush_onItemChanged(aItemId, aProperty,
                                                         aIsAnnotationProperty,
                                                         aValue)
  {
    if (aProperty == "uri")
      this._syncMozPlaces();
  },

  onItemRemoved: function() { },
  onItemVisited: function() { },
  onItemMoved: function() { },

  //////////////////////////////////////////////////////////////////////////////
  //// nsITimerCallback

  notify: function() this._syncAll(),

  //////////////////////////////////////////////////////////////////////////////
  //// nsPlacesDBFlush

  /**
   * Dispatches an event to the background thread to run _doSyncMozX("places").
   */
  _syncMozPlaces: function DBFlush_syncMozPlaces()
  {
    // No need to do extra work if we are in batch mode
    if (this._inBatchMode)
      return;

    let self = this;
    PlacesBackground.dispatch({
      run: function() self._doSyncMozX("places")
    }, Ci.nsIEventTarget.DISPATCH_NORMAL);
  },

  /**
   * Dispatches an event to the background thread to sync all temporary tables.
   */
  _syncAll: function DBFlush_syncAll()
  {
    let self = this;
    PlacesBackground.dispatch({
      run: function() {
        // We try to get a transaction, but if we can't don't worry
        let ourTransaction = false;
        try {
          this._db.beginTransaction();
          ourTransaction = true;
        }
        catch (e) { }

        try {
          // This needs to also sync moz_places in order to maintain data
          // integrity
          self._doSyncMozX("places");
          self._doSyncMozX("historyvisits");
        }
        catch (e) {
          if (ourTransaction)
            this._db.rollbackTransaction();
          throw e;
        }

        if (ourTransaction)
          this._db.commitTransaction();
      }
    }, Ci.nsIEventTarget.DISPATCH_NORMAL);
  },

  /**
   * Synchronizes the moz_{aName} and moz_{aName}_temp by copying all the data
   * from the temporary table into the permanent one.  It then deletes the data
   * in the temporary table.  All of this is done in a transaction that is
   * rolled back upon failure at any point.
   */
  _doSyncMozX: function DBFlush_doSyncMozX(aName)
  {
    // Delete all the data in the temp table.
    // We have triggers setup that ensure that the data is transfered over
   // upon deletion.
   this._db.executeSimpleSQL("DELETE FROM moz_" + aName + "_temp");
  },

  /**
   * Creates a new timer bases on this._timerInterval.
   *
   * @returns a REPEATING_SLACK nsITimer that runs every this._timerInterval.
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
  ])
};

////////////////////////////////////////////////////////////////////////////////
//// Module Registration

let components = [nsPlacesDBFlush];
function NSGetModule(compMgr, fileSpec)
{
  return XPCOMUtils.generateModule(components);
}
