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
 *  Myk Melez <myk@mozilla.org>
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

const EXPORTED_SYMBOLS = ['TabEngine'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/syncCores.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/constants.js");

Function.prototype.async = Async.sugar;

function TabEngine() {
  this._init();
}
TabEngine.prototype = {
  __proto__: SyncEngine.prototype,
  name: "tabs",
  displayName: "Tabs",
  logName: "Tabs",
  _storeObj: TabStore,
  _trackerObj: TabTracker,
  _recordObj: ClientTabsRecord

  get virtualTabs() {
    let virtualTabs = {};
    let realTabs = this._store.wrap();

    for (let profileId in this._file.data) {
      let tabset = this._file.data[profileId];
      for (let guid in tabset) {
        if (!(guid in realTabs) && !(guid in virtualTabs)) {
          virtualTabs[guid] = tabset[guid];
          virtualTabs[guid].profileId = profileId;
        }
      }
    }
    return virtualTabs;
  }
};

function TabStore() {
  this._TabStore_init();
}
TabStore.prototype = {
  __proto__: new Store(),
  _logName: "Tabs.Store",

  get _sessionStore() {
    let sessionStore = Cc["@mozilla.org/browser/sessionstore;1"].
		       getService(Ci.nsISessionStore);
    this.__defineGetter__("_sessionStore", function() sessionStore);
    return this._sessionStore;
  },

  _createCommand: function TabStore__createCommand(command) {
    this._log.debug("_createCommand: " + command.GUID);

    if (command.GUID in this._virtualTabs || command.GUID in this._wrapRealTabs())
      throw "trying to create a tab that already exists; id: " + command.GUID;

    // Don't do anything if the command isn't valid (i.e. it doesn't contain
    // the minimum information about the tab that is necessary to recreate it).
    if (!this.validateVirtualTab(command.data)) {
      this._log.warn("could not create command " + command.GUID + "; invalid");
      return;
    }

    // Cache the tab and notify the UI to prompt the user to open it.
    this._virtualTabs[command.GUID] = command.data;
    this._os.notifyObservers(null, "weave:store:tabs:virtual:created", null);
  },

  /**
   * Serialize the current state of tabs.
   */
  wrap: function TabStore_wrap() {
    let items = {};

    let session = this._json.decode(this._sessionStore.getBrowserState());

    for (let i = 0; i < session.windows.length; i++) {
      let window = session.windows[i];
      // For some reason, session store uses one-based array index references,
      // (f.e. in the "selectedWindow" and each tab's "index" properties), so we
      // convert them to and from JavaScript's zero-based indexes as needed.
      let windowID = i + 1;

      for (let j = 0; j < window.tabs.length; j++) {
        let tab = window.tabs[j];

	// The session history entry for the page currently loaded in the tab.
	// We use the URL of the current page as the ID for the tab.
	let currentEntry = tab.entries[tab.index - 1];
	if (!currentEntry || !currentEntry.url) {
	  this._log.warn("_wrapRealTabs: no current entry or no URL, can't " +
                         "identify " + this._json.encode(tab));
	  continue;
	}

	let tabID = currentEntry.url;

        // Only sync up to 10 back-button entries, otherwise we can end up with
        // some insanely large snapshots.
        tab.entries = tab.entries.slice(tab.entries.length - 10);

        // The ID property of each entry in the tab, which I think contains
        // nsISHEntry::ID, changes every time session store restores the tab,
        // so we can't sync them, or we would generate edit commands on every
        // restart (even though nothing has actually changed).
        for (let k = 0; k < tab.entries.length; k++) {
            delete tab.entries[k].ID;
        }

	items[tabID] = {
          // Identify this item as a tab in case we start serializing windows
          // in the future.
	  type: "tab",

          // The position of this tab relative to other tabs in the window.
          // For consistency with session store data, we make this one-based.
          position: j + 1,

	  windowID: windowID,

	  state: tab
	};
      }
    }

    return items;
  },

  wipe: function TabStore_wipe() {
    // We're not going to close tabs, since that's probably not what
    // the user wants
  }
};

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
    let numChanged = c.filter(function(v) !v).length;

    let fractionSimilar = (numShared - (numChanged / 2)) / numTotal;
    let fractionDissimilar = 1 - fractionSimilar;
    let percentDissimilar = Math.round(fractionDissimilar * 100);

    return percentDissimilar;
  },

  resetScore: function FormsTracker_resetScore() {
    // Not implemented, since we calculate the score on demand.
  }
}
