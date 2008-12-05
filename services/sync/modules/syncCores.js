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

const EXPORTED_SYMBOLS = ['SyncCore'];

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

  // Set this property in child objects!
  _store: null,

  _init: function SC__init() {
    this._log = Log4Moz.repository.getLogger("Service." + this._logName);
  },

  // FIXME: this won't work for deep objects, or objects with optional
  // properties
  _getEdits: function SC__getEdits(a, b) {
    let ret = {numProps: 0, props: {}};
    for (prop in a) {
      if (!Utils.deepEquals(a[prop], b[prop])) {
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
    if (!tree[GUID] || !tree[GUID].parentid)
      return parents;
    parents.push(tree[GUID].parentid);
    return this._nodeParentsInt(tree[GUID].parentid, tree, parents);
  },

  _detectUpdates: function SC__detectUpdates(a, b) {
    let self = yield;

    let cmds = [];

    try {
      for (let GUID in a) {

        Utils.makeTimerForCall(self.cb);
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

      for (GUID in b) {

        Utils.makeTimerForCall(self.cb);
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
      throw e;

    } finally {
      self.done(cmds);
    }
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
      if (key != "GUID" && !Utils.deepEquals(a[key], b[key]))
        return false;
    }
    for (key in b) {
      if (key != "GUID" && !Utils.deepEquals(a[key], b[key]))
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
      if (list[i].data && list[i].data.parentid &&
          list[i].data.parentid == oldGUID)
        list[i].data.parentid = newGUID;
      for (let j = 0; j < list[i].parents.length; j++) {
        if (list[i].parents[j] == oldGUID)
          list[i].parents[j] = newGUID;
      }
    }
  },

  _conflicts: function SC__conflicts(a, b) {
    if ((a.GUID == b.GUID) && !Utils.deepEquals(a, b))
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

  _reconcile: function SC__reconcile(listA, listB) {
    let self = yield;

    let propagations = [[], []];
    let conflicts = [[], []];
    let ret = {propagations: propagations, conflicts: conflicts};
    this._log.debug("Reconciling " + listA.length +
		    " against " + listB.length + " commands");

    let guidChanges = [];
    let edits = [];
    for (let i = 0; i < listA.length; i++) {
      let a = listA[i];

      yield Utils.makeTimerForCall(self.cb); // yield to UI

      let skip = false;
      listB = listB.filter(function(b) {
        // fast path for when we already found a matching command
        if (skip)
          return true;

        if (a.GUID == b.GUID) {
          // delete both commands
          // XXX this relies on the fact that we actually dump
          // outgoing commands and generate new ones by doing a fresh
          // diff after applying local changes
          skip = true;
          delete listA[i]; // delete a
          return false; // delete b

        } else if (this._commandLike(a, b)) {
          this._fixParents(listA, a.GUID, b.GUID);
          guidChanges.push({action: "edit",
      		      GUID: a.GUID,
      		      data: {GUID: b.GUID}});
          skip = true;
          delete listA[i]; // delete a
          return false; // delete b
        }
        return true; // keep b
      }, this);
    }

    listB = listB.filter(function(b) {
      // watch out for create commands with GUIDs that already exist
      if (b.action == "create" && this._store._itemExists(b.GUID)) {
        this._log.error("Remote command has GUID that already exists " +
                        "locally. Dropping command.");
        return false; // delete b
      }
      return true; // keep b
    }, this);

    listA = listA.filter(function(elt) { return elt }); // removes any holes
    listA = listA.concat(edits);
    listB = guidChanges.concat(listB);

    for (let i = 0; i < listA.length; i++) {
      for (let j = 0; j < listB.length; j++) {

        yield Utils.makeTimerForCall(self.cb); // yield to UI

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

    yield Utils.makeTimerForCall(self.cb); // yield to UI

    this._getPropagations(listB, conflicts[1], propagations[0]);
    ret = {propagations: propagations, conflicts: conflicts};

    self.done(ret);
  },

  // Public methods

  detectUpdates: function SC_detectUpdates(onComplete, a, b) {
    return this._detectUpdates.async(this, onComplete, a, b);
  },

  reconcile: function SC_reconcile(onComplete, listA, listB) {
    return this._reconcile.async(this, onComplete, listA, listB);
  }
};
