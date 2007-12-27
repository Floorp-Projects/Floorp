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

const EXPORTED_SYMBOLS = ['SyncCore', 'BookmarksSyncCore', 'HistorySyncCore'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");

Function.prototype.async = generatorAsync;

/*
 * SyncCore objects
 * Sync cores deal with diff creation and conflict resolution.
 * Tree data structures where all nodes have GUIDs only need to be
 * subclassed for each data type to implement commandLike and
 * itemExists.
 */

function SyncCore() {
  this._init();
}
SyncCore.prototype = {
  _logName: "Sync",

  _init: function SC__init() {
    this._log = Log4Moz.Service.getLogger("Service." + this._logName);
  },

  // FIXME: this won't work for deep objects, or objects with optional properties
  _getEdits: function SC__getEdits(a, b) {
    let ret = {numProps: 0, props: {}};
    for (prop in a) {
      if (!deepEquals(a[prop], b[prop])) {
        ret.numProps++;
        ret.props[prop] = b[prop];
      }
    }
    return ret;
  },

  _nodeParents: function SC__nodeParents(GUID, tree) {
    return this._nodeParentsInt(GUID, tree, []);
  },

  _nodeParentsInt: function SC__nodeParentsInt(GUID, tree, parents) {
    if (!tree[GUID] || !tree[GUID].parentGUID)
      return parents;
    parents.push(tree[GUID].parentGUID);
    return this._nodeParentsInt(tree[GUID].parentGUID, tree, parents);
  },

  _detectUpdates: function SC__detectUpdates(onComplete, a, b) {
    let [self, cont] = yield;
    let listener = new EventListener(cont);
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

    let cmds = [];

    try {
      for (let GUID in a) {

        timer.initWithCallback(listener, 0, timer.TYPE_ONE_SHOT);
        yield; // Yield to main loop

        if (GUID in b) {
          let edits = this._getEdits(a[GUID], b[GUID]);
          if (edits.numProps == 0) // no changes - skip
            continue;
          let parents = this._nodeParents(GUID, b);
          cmds.push({action: "edit", GUID: GUID,
                     depth: parents.length, parents: parents,
                     data: edits.props});
        } else {
          let parents = this._nodeParents(GUID, a); // ???
          cmds.push({action: "remove", GUID: GUID,
                     depth: parents.length, parents: parents});
        }
      }
      for (let GUID in b) {

        timer.initWithCallback(listener, 0, timer.TYPE_ONE_SHOT);
        yield; // Yield to main loop

        if (GUID in a)
          continue;
        let parents = this._nodeParents(GUID, b);
        cmds.push({action: "create", GUID: GUID,
                   depth: parents.length, parents: parents,
                   data: b[GUID]});
      }
      cmds.sort(function(a, b) {
        if (a.depth > b.depth)
          return 1;
        if (a.depth < b.depth)
          return -1;
        if (a.index > b.index)
          return -1;
        if (a.index < b.index)
          return 1;
        return 0; // should never happen, but not a big deal if it does
      });

    } catch (e) {
      this._log.error("Exception caught: " + (e.message? e.message : e));

    } finally {
      timer = null;
      generatorDone(this, self, onComplete, cmds);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  _commandLike: function SC__commandLike(a, b) {
    this._log.error("commandLike needs to be subclassed");

    // Check that neither command is null, and verify that the GUIDs
    // are different (otherwise we need to check for edits)
    if (!a || !b || a.GUID == b.GUID)
      return false;

    // Check that all other properties are the same
    // FIXME: could be optimized...
    for (let key in a) {
      if (key != "GUID" && !deepEquals(a[key], b[key]))
        return false;
    }
    for (let key in b) {
      if (key != "GUID" && !deepEquals(a[key], b[key]))
        return false;
    }
    return true;
  },

  // When we change the GUID of a local item (because we detect it as
  // being the same item as a remote one), we need to fix any other
  // local items that have it as their parent
  _fixParents: function SC__fixParents(list, oldGUID, newGUID) {
    for (let i = 0; i < list.length; i++) {
      if (!list[i])
        continue;
      if (list[i].data.parentGUID == oldGUID)
        list[i].data.parentGUID = newGUID;
      for (let j = 0; j < list[i].parents.length; j++) {
        if (list[i].parents[j] == oldGUID)
          list[i].parents[j] = newGUID;
      }
    }
  },

  _conflicts: function SC__conflicts(a, b) {
    if ((a.GUID == b.GUID) && !deepEquals(a, b))
      return true;
    return false;
  },

  _getPropagations: function SC__getPropagations(commands, conflicts, propagations) {
    for (let i = 0; i < commands.length; i++) {
      let alsoConflicts = function(elt) {
        return (elt.action == "create" || elt.action == "remove") &&
          commands[i].parents.indexOf(elt.GUID) >= 0;
      };
      if (conflicts.some(alsoConflicts))
        conflicts.push(commands[i]);

      let cmdConflicts = function(elt) {
        return elt.GUID == commands[i].GUID;
      };
      if (!conflicts.some(cmdConflicts))
        propagations.push(commands[i]);
    }
  },

  _itemExists: function SC__itemExists(GUID) {
    this._log.error("itemExists needs to be subclassed");
    return false;
  },

  _reconcile: function SC__reconcile(onComplete, listA, listB) {
    let [self, cont] = yield;
    let listener = new EventListener(cont);
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

    let propagations = [[], []];
    let conflicts = [[], []];
    let ret = {propagations: propagations, conflicts: conflicts};
    this._log.debug("Reconciling " + listA.length +
		    " against " + listB.length + "commands");

    try {
      let guidChanges = [];
      for (let i = 0; i < listA.length; i++) {
	let a = listA[i];
        timer.initWithCallback(listener, 0, timer.TYPE_ONE_SHOT);
        yield; // Yield to main loop

	//this._log.debug("comparing " + i + ", listB length: " + listB.length);

	let skip = false;
	listB = listB.filter(function(b) {
	  // fast path for when we already found a matching command
	  if (skip)
	    return true;

          if (deepEquals(a, b)) {
            delete listA[i]; // a
	    skip = true;
	    return false; // b

          } else if (this._commandLike(a, b)) {
            this._fixParents(listA, a.GUID, b.GUID);
	    guidChanges.push({action: "edit",
			      GUID: a.GUID,
			      data: {GUID: b.GUID}});
            delete listA[i]; // a
	    skip = true;
	    return false; // b, but we add it back from guidChanges
          }
  
          // watch out for create commands with GUIDs that already exist
          if (b.action == "create" && this._itemExists(b.GUID)) {
            this._log.error("Remote command has GUID that already exists " +
                            "locally. Dropping command.");
	    return false; // delete b
          }
	  return true; // keep b
        }, this);
      }
  
      listA = listA.filter(function(elt) { return elt });
      listB = guidChanges.concat(listB);
  
      for (let i = 0; i < listA.length; i++) {
        for (let j = 0; j < listB.length; j++) {

          timer.initWithCallback(listener, 0, timer.TYPE_ONE_SHOT);
          yield; // Yield to main loop
  
          if (this._conflicts(listA[i], listB[j]) ||
              this._conflicts(listB[j], listA[i])) {
            if (!conflicts[0].some(
              function(elt) { return elt.GUID == listA[i].GUID }))
              conflicts[0].push(listA[i]);
            if (!conflicts[1].some(
              function(elt) { return elt.GUID == listB[j].GUID }))
              conflicts[1].push(listB[j]);
          }
        }
      }
  
      this._getPropagations(listA, conflicts[0], propagations[1]);
  
      timer.initWithCallback(listener, 0, timer.TYPE_ONE_SHOT);
      yield; // Yield to main loop
  
      this._getPropagations(listB, conflicts[1], propagations[0]);
      ret = {propagations: propagations, conflicts: conflicts};

    } catch (e) {
      this._log.error("Exception caught: " + (e.message? e.message : e));

    } finally {
      timer = null;
      generatorDone(this, self, onComplete, ret);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  // Public methods

  detectUpdates: function SC_detectUpdates(onComplete, a, b) {
    return this._detectUpdates.async(this, onComplete, a, b);
  },

  reconcile: function SC_reconcile(onComplete, listA, listB) {
    return this._reconcile.async(this, onComplete, listA, listB);
  }
};

function BookmarksSyncCore() {
  this._init();
}
BookmarksSyncCore.prototype = {
  _logName: "BMSync",

  __bms: null,
  get _bms() {
    if (!this.__bms)
      this.__bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                   getService(Ci.nsINavBookmarksService);
    return this.__bms;
  },

  _itemExists: function BSC__itemExists(GUID) {
    return this._bms.getItemIdForGUID(GUID) >= 0;
  },

  _commandLike: function BSC_commandLike(a, b) {
    // Check that neither command is null, that their actions, types,
    // and parents are the same, and that they don't have the same
    // GUID.
    // Items with the same GUID do not qualify for 'likeness' because
    // we already consider them to be the same object, and therefore
    // we need to process any edits.
    // The parent GUID check works because reconcile() fixes up the
    // parent GUIDs as it runs, and the command list is sorted by
    // depth
    if (!a || !b ||
       a.action != b.action ||
       a.data.type != b.data.type ||
       a.data.parentGUID != b.data.parentGUID ||
       a.GUID == b.GUID)
      return false;

    // Bookmarks are allowed to be in a different index as long as
    // they are in the same folder.  Folders and separators must be at
    // the same index to qualify for 'likeness'.
    switch (a.data.type) {
    case "bookmark":
      if (a.data.URI == b.data.URI &&
          a.data.title == b.data.title)
        return true;
      return false;
    case "query":
      if (a.data.URI == b.data.URI &&
          a.data.title == b.data.title)
        return true;
      return false;
    case "microsummary":
      if (a.data.URI == b.data.URI &&
          a.data.generatorURI == b.data.generatorURI)
        return true;
      return false;
    case "folder":
      if (a.index == b.index &&
          a.data.title == b.data.title)
        return true;
      return false;
    case "livemark":
      if (a.data.title == b.data.title &&
          a.data.siteURI == b.data.siteURI &&
          a.data.feedURI == b.data.feedURI)
        return true;
      return false;
    case "separator":
      if (a.index == b.index)
        return true;
      return false;
    default:
      this._log.error("commandLike: Unknown item type: " + uneval(a));
      return false;
    }
  }
};
BookmarksSyncCore.prototype.__proto__ = new SyncCore();

function HistorySyncCore() {
  this._init();
}
HistorySyncCore.prototype = {
  _logName: "HistSync",

  _itemExists: function BSC__itemExists(GUID) {
    // we don't care about already-existing items; just try to re-add them
    return false;
  },

  _commandLike: function BSC_commandLike(a, b) {
    // History commands never qualify for likeness.  We will always
    // take the union of all client/server items.  We use the URL as
    // the GUID, so the same sites will map to the same item (same
    // GUID), without our intervention.
    return false;
  }
};
HistorySyncCore.prototype.__proto__ = new SyncCore();
