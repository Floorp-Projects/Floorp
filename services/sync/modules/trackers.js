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
 * Portions created by the Initial Developer are Copyright (C) 2008
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

const EXPORTED_SYMBOLS = ['Tracker'];

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
 * listening for changes to their particular data type.
 * 
 * There are two things they keep track of:
 * 1) A score, indicating how urgently the engine wants to sync
 * 2) A list of IDs for all the changed items that need to be synced
 * and updating their 'score', indicating how urgently they
 * want to sync.
 *
 */
function Tracker() {
  this._init();
}
Tracker.prototype = {
  _logName: "Tracker",
  file: "none",

  get _json() {
    let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
    this.__defineGetter__("_json", function() json);
    return json;
  },

  _init: function T__init() {
    this._log = Log4Moz.repository.getLogger(this._logName);
    this._score = 0;
    this.loadChangedIDs();
    this.enable();
  },

  get enabled() {
    return this._enabled;
  },

  enable: function T_enable() {
    this._enabled = true;
  },

  disable: function T_disable() {
    this._enabled = false;
  },

  /*
   * Score can be called as often as desired to decide which engines to sync
   *
   * Valid values for score:
   * -1: Do not sync unless the user specifically requests it (almost disabled)
   * 0: Nothing has changed
   * 100: Please sync me ASAP!
   * 
   * Setting it to other values should (but doesn't currently) throw an exception
   */
  get score() {
    if (this._score >= 100)
      return 100;
    else
      return this._score;
  },

  // Should be called by service everytime a sync has been done for an engine
  resetScore: function T_resetScore() {
    this._score = 0;
  },

  /*
   * Changed IDs are in an object (hash) to make it easy to check if
   * one is already set or not.
   * Note that it would be nice to make these methods asynchronous so
   * as to not block when writing to disk.  However, these will often
   * get called from observer callbacks, and so it is better to make
   * them synchronous.
   */

  get changedIDs() {
    let items = {};
    this.__defineGetter__("changedIDs", function() items);
    return items;
  },

  saveChangedIDs: function T_saveChangedIDs() {
    this._log.debug("Saving changed IDs to disk");

    let file = Utils.getProfileFile(
      {path: "weave/changes/" + this.file + ".json",
       autoCreate: true});
    let out = this._json.encode(this.changedIDs);
    let [fos] = Utils.open(file, ">");
    fos.writeString(out);
    fos.close();
  },

  loadChangedIDs: function T_loadChangedIDs() {
    let file = Utils.getProfileFile("weave/changes/" + this.file + ".json");
    if (!file.exists())
      return;

    this._log.debug("Loading previously changed IDs from disk");

    try {
      let [is] = Utils.open(file, "<");
      let json = Utils.readStream(is);
      is.close();

      let ids = this._json.decode(json);
      for (let id in ids) {
        this.changedIDs[id] = 1;
      }
    } catch (e) {
      this._log.warn("Could not load changed IDs from previous session");
      this._log.debug("Exception: " + e);
    }
  },

  addChangedID: function T_addChangedID(id) {
    if (!this.enabled)
      return;
    if (!id) {
      this._log.warn("Attempted to add undefined ID to tracker");
      return;
    }
    this._log.debug("Adding changed ID " + id);
    if (!this.changedIDs[id]) {
      this.changedIDs[id] = true;
      this.saveChangedIDs();
    }
  },

  removeChangedID: function T_removeChangedID(id) {
    if (!this.enabled)
      return;
    if (!id) {
      this._log.warn("Attempted to remove undefined ID from tracker");
      return;
    }
    this._log.debug("Removing changed ID " + id);
    if (this.changedIDs[id]) {
      delete this.changedIDs[id];
      this.saveChangedIDs();
    }
  },

  clearChangedIDs: function T_clearChangedIDs() {
    this._log.debug("Clearing changed ID list");
    for (let id in this.changedIDs) {
      delete this.changedIDs[id];
    }
    this.saveChangedIDs();
  }
};
