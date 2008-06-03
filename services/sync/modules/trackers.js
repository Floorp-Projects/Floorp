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
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Anant Narayanan <anant@kix.in>
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

const EXPORTED_SYMBOLS = ['Tracker',
                          'FormsTracker', 'TabTracker'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;

/*
 * Trackers are associated with a single engine and deal with
 * listening for changes to their particular data type
 * and updating their 'score', indicating how urgently they
 * want to sync.
 *
 * 'score's range from 0 (Nothing's changed)
 * to 100 (I need to sync now!)
 * -1 is also a valid score
 * (don't sync me unless the user specifically requests it)
 *
 * Setting a score outside of this range will raise an exception.
 * Well not yet, but it will :)
 */
function Tracker() {
  this._init();
}
Tracker.prototype = {
  _logName: "Tracker",
  _score: 0,

  _init: function T__init() {
    this._log = Log4Moz.Service.getLogger("Service." + this._logName);
    this._score = 0;
  },

  /* Should be called by service periodically
   * before deciding which engines to sync
   */
  get score() {
    if (this._score >= 100)
      return 100;
    else
      return this._score;
  },

  /* Should be called by service everytime a sync
   * has been done for an engine
   */
  resetScore: function T_resetScore() {
    this._score = 0;
  }
};

function FormsTracker() {
  this._init();
}
FormsTracker.prototype = {
  _logName: "FormsTracker",

  __formDB: null,
  get _formDB() {
    if (!this.__formDB) {
      var file = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties).
      get("ProfD", Ci.nsIFile);
      file.append("formhistory.sqlite");
      var stor = Cc["@mozilla.org/storage/service;1"].
      getService(Ci.mozIStorageService);
      this.__formDB = stor.openDatabase(file);
    }

    return this.__formDB;
  },

  /* nsIFormSubmitObserver is not available in JS.
   * To calculate scores, we instead just count the changes in
   * the database since the last time we were asked.
   *
   * FIXME!: Buggy, because changes in a row doesn't result in
   * an increment of our score. A possible fix is to do a
   * SELECT for each fieldname and compare those instead of the
   * whole row count.
   *
   * Each change is worth 2 points. At some point, we may
   * want to differentiate between search-history rows and other
   * form items, and assign different scores.
   */
  _rowCount: 0,
  get score() {
    var stmnt = this._formDB.createStatement("SELECT COUNT(fieldname) FROM moz_formhistory");
    stmnt.executeStep();
    var count = stmnt.getInt32(0);
    stmnt.reset();

    this._score = Math.abs(this._rowCount - count) * 2;

    if (this._score >= 100)
      return 100;
    else
      return this._score;
  },

  resetScore: function FormsTracker_resetScore() {
    var stmnt = this._formDB.createStatement("SELECT COUNT(fieldname) FROM moz_formhistory");
    stmnt.executeStep();
    this._rowCount = stmnt.getInt32(0);
    stmnt.reset();
    this._score = 0;
  },

  _init: function FormsTracker__init() {
    this._log = Log4Moz.Service.getLogger("Service." + this._logName);
    this._score = 0;

    var stmnt = this._formDB.createStatement("SELECT COUNT(fieldname) FROM moz_formhistory");
    stmnt.executeStep();
    this._rowCount = stmnt.getInt32(0);
    stmnt.reset();
  }
}
FormsTracker.prototype.__proto__ = new Tracker();

function TabTracker(engine) {
  this._engine = engine;
  this._init();
}
TabTracker.prototype = {
  __proto__: new Tracker(),

  _logName: "TabTracker",

  _engine: null,

  get _json() {
    let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
    this.__defineGetter__("_json", function() json);
    return this._json;
  },

  /**
   * There are two ways we could calculate the score.  We could calculate it
   * incrementally by using the window mediator to watch for windows opening/
   * closing and FUEL (or some other API) to watch for tabs opening/closing
   * and changing location.
   *
   * Or we could calculate it on demand by comparing the state of tabs
   * according to the session store with the state according to the snapshot.
   *
   * It's hard to say which is better.  The incremental approach is less
   * accurate if it simply increments the score whenever there's a change,
   * but it might be more performant.  The on-demand approach is more accurate,
   * but it might be less performant depending on how often it's called.
   *
   * In this case we've decided to go with the on-demand approach, and we
   * calculate the score as the percent difference between the snapshot set
   * and the current tab set, where tabs that only exist in one set are
   * completely different, while tabs that exist in both sets but whose data
   * doesn't match (f.e. because of variations in history) are considered
   * "half different".
   *
   * So if the sets don't match at all, we return 100;
   * if they completely match, we return 0;
   * if half the tabs match, and their data is the same, we return 50;
   * and if half the tabs match, but their data is all different, we return 75.
   */
  get score() {
    // The snapshot data is a singleton that we can't modify, so we have to
    // copy its unique items to a new hash.
    let snapshotData = this._engine.snapshot.data;
    let a = {};

    // The wrapped current state is a unique instance we can munge all we want.
    let b = this._engine.store.wrap();

    // An array that counts the number of intersecting IDs between a and b
    // (represented as the length of c) and whether or not their values match
    // (represented by the boolean value of each item in c).
    let c = [];

    // Generate c and update a and b to contain only unique items.
    for (id in snapshotData) {
      if (id in b) {
        c.push(this._json.encode(snapshotData[id]) == this._json.encode(b[id]));
        delete b[id];
      }
      else {
        a[id] = snapshotData[id];
      }
    }

    let numShared = c.length;
    let numUnique = [true for (id in a)].length + [true for (id in b)].length;
    let numTotal = numShared + numUnique;

    // We're going to divide by the total later, so make sure we don't try
    // to divide by zero, even though we should never be in a state where there
    // are no tabs in either set.
    if (numTotal == 0)
      return 0;

    // The number of shared items whose data is different.
    let numChanged = c.filter(function(v) v).length;

    let fractionSimilar = (numShared - (numChanged / 2)) / numTotal;
    let fractionDissimilar = 1 - fractionSimilar;
    let percentDissimilar = Math.round(fractionDissimilar * 100);

    return percentDissimilar;
  },

  resetScore: function FormsTracker_resetScore() {
    // Not implemented, since we calculate the score on demand.
  }
}
