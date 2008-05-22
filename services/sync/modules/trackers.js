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

const EXPORTED_SYMBOLS = ['Tracker', 'BookmarksTracker'];

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
 
 /*
  * Tracker objects for each engine may need to subclass the
  * getScore routine, which returns the current 'score' for that
  * engine. How the engine decides to set the score is upto it,
  * as long as the value between 0 and 100 actually corresponds
  * to its urgency to sync.
  *
  * Here's an example BookmarksTracker. We don't subclass getScore
  * because the observer methods take care of updating _score which
  * getScore returns by default.
  */
  function BookmarksTracker() {
    this._init();
  }
  BookmarksTracker.prototype = {
    _logName: "BMTracker",
    
    /* We don't care about the first three */
    onBeginUpdateBatch: function BMT_onBeginUpdateBatch() {

    },
    onEndUpdateBatch: function BMT_onEndUpdateBatch() {

    },
    onItemVisited: function BMT_onItemChanged() {

    },
    /* Every add or remove is worth 4 points,
     * on the basis that adding or removing 20 bookmarks
     * means its time to sync?
     */
    onItemAdded: function BMT_onEndUpdateBatch() {
      this._score += 4;
    },
    onItemRemoved: function BMT_onItemRemoved() {
      this._score += 4;
    },
    /* Changes are worth 2 points? */
    onItemChanged: function BMT_onItemChanged() {
      this._score += 2;
    },
    
    _init: function BMT__init() {
      super._init();
      Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
      getService(Ci.nsINavBookmarksService).
      addObserver(this, false);
    }
  }
  BookmarksTracker.prototype.__proto__ = new Tracker();
  
