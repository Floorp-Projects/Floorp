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
 * The Original Code is Places.
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function BookmarksSyncService() { this._init(); }
BookmarksSyncService.prototype = {

  __bms: null,
  get _bms() {
    if (!this.__bms)
      this.__bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                   getService(Ci.nsINavBookmarksService);
    return this.__bms;
  },

  __hsvc: null,
  get _hsvc() {
    if (!this.__hsvc)
      this.__hsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                    getService(Ci.nsINavHistoryService);
    return this.__hsvc;
  },

  __ans: null,
  get _ans() {
    if (!this.__ans)
      this.__ans = Cc["@mozilla.org/browser/annotation-service;1"].
                   getService(Ci.nsIAnnotationService);
    return this.__ans;
  },

  __os: null,
  get _os() {
    if (!this.__os)
      this.__os = Cc["@mozilla.org/observer-service;1"]
        .getService(Ci.nsIObserverService);
    return this.__os;
  },

  // Timer object for yielding to the main loop
  _timer: null,

  // DAVCollection object
  _dav: null,

  // Last synced tree
  // FIXME: this should be serialized to disk
  _snapshot: {},
  _snapshotVersion: 0,

  get currentUser() {
    return this._dav.currentUser;
  },

  _init: function BSS__init() {
    let serverURL = 'https://dotmoz.mozilla.org/';
    try {
      let branch = Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);
      serverURL = branch.getCharPref("browser.places.sync.serverURL");
    }
    catch (ex) { /* use defaults */ }
    LOG("Bookmarks login server: " + serverURL);
    this._dav = new DAVCollection(serverURL);
  },

  _wrapNode: function BSS__wrapNode(node) {
    var items = {};
    this._wrapNodeInternal(node, items, null, null);
    return items;
  },

  _wrapNodeInternal: function BSS__wrapNodeInternal(node, items, parentGuid, index) {
    var guid = this._bms.getItemGUID(node.itemId);
    var item = {type: node.type,
                parentGuid: parentGuid,
                index: index};

    if (node.type == node.RESULT_TYPE_FOLDER) {
      item.title = node.title;

      node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      node.containerOpen = true;

      for (var i = 0; i < node.childCount; i++) {
        this._wrapNodeInternal(node.getChild(i), items, guid, i);
      }
    } else if (node.type == node.RESULT_TYPE_SEPARATOR) {
    } else if (node.type == node.RESULT_TYPE_URI) {
      // FIXME: need to verify that it's a bookmark, it could be a history result!
      item.title = node.title;
      item.uri = node.uri;
    } else {
      // what do we do?
    }

    items[guid] = item;
  },

  _getEdits: function BSS__getEdits(a, b) {
    // check the type separately, just in case
    if (a.type != b.type)
      return -1;

    let ret = {numProps: 0, props: {}};
    for (prop in a) {
      if (a[prop] != b[prop]) {
        ret.numProps++;
        ret.props[prop] = b[prop];
      }
    }

    // FIXME: prune out properties we don't care about

    return ret;
  },

  _nodeParents: function BSS__nodeParents(guid, tree) {
    return this._nodeParentsInt(guid, tree, []);
  },

  _nodeParentsInt: function BSS__nodeParentsInt(guid, tree, parents) {
    if (!tree[guid] || tree[guid].parentGuid == null)
      return parents;
    parents.push(tree[guid].parentGuid);
    return this._nodeParentsInt(tree[guid].parentGuid, tree, parents);
  },

  _detectUpdates: function BSS__detectUpdates(a, b) {
    let cmds = [];
    for (let guid in a) {
      if (guid in b) {
        let edits = this._getEdits(a[guid], b[guid]);
        if (edits == -1) // something went very wrong -- FIXME
          continue;
        if (edits.numProps == 0) // no changes - skip
          continue;
        let parents = this._nodeParents(guid, b);
        cmds.push({action: "edit", guid: guid,
                   depth: parents.length, parents: parents,
                   data: edits.props});
      } else {
        let parents = this._nodeParents(guid, a); // ???
        cmds.push({action: "remove", guid: guid,
                   depth: parents.length, parents: parents});
      }
    }
    for (let guid in b) {
      if (guid in a)
        continue;
      let parents = this._nodeParents(guid, b);
      cmds.push({action: "create", guid: guid,
                 depth: parents.length, parents: parents,
                 data: b[guid]});
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
    return cmds;
  },

  _conflicts: function BSS__conflicts(a, b) {
    if ((a.depth < b.depth) &&
        (b.parents.indexOf(a.guid) >= 0) &&
        a.action == "remove")
      return true;
    if ((a.guid == b.guid) && !this._deepEquals(a, b))
      return true;
// FIXME - how else can two commands conflict?
    return false;
  },

  // Bookmarks are allowed to be in a different index as long as they
  // are in the same folder.  Folders and separators must be at the
  // same index to qualify for 'likeness'.
  _commandLike: function BSS__commandLike(a, b) {
    if (!a || !b)
      return false;

    if (a.action != b.action)
      return false;

    // this check works because reconcile() fixes up the parent guids
    // as it runs, and the command list is sorted by depth
    if (a.parentGuid != b.parentGuid)
      return false;

    switch (a.data.type) {
    case 0:
      if (b.data.type == a.data.type &&
          b.data.uri == a.data.uri &&
          b.data.title == a.data.title)
        return true;
      return false;
    case 6:
      if (b.index == a.index &&
          b.data.type == a.data.type &&
          b.data.title == a.data.title)
        return true;
      return false;
    case 7:
      if (b.index == a.index &&
          b.data.type == a.data.type)
        return true;
      return false;
    default:
      LOG("_commandLike: Unknown item type: " + uneval(a));
      return false;
    }
  },

  _deepEquals: function BSS__commandEquals(a, b) {
    if (!a && !b)
      return true;
    if (!a || !b)
      return false;

    for (let key in a) {
      if (typeof(a[key]) == "object") {
        if (!typeof(b[key]) == "object")
          return false;
        if (!this._deepEquals(a[key], b[key]))
          return false;
      } else {
        if (a[key] != b[key])
          return false;
      }
    }
    return true;
  },

  // When we change the guid of a local item (because we detect it as
  // being the same item as a remote one), we need to fix any other
  // local items that have it as their parent
  _fixParents: function BSS__fixParents(list, oldGuid, newGuid) {
    for (let i = 0; i < list.length; i++) {
      if (!list[i])
        continue;
      if (list[i].parentGuid == oldGuid)
        list[i].parentGuid = newGuid;
      for (let j = 0; j < list[i].parents.length; j++) {
        if (list[i].parents[j] == oldGuid)
          list[i].parents[j] = newGuid;
      }
    }
  },

  // FIXME: todo: change the conflicts data structure to hold more
  // information about which specific command pars conflict; an what
  // commands would apply once those are resolved.  Perhaps something
  // like:

  // [[direct-conflictA, 2nd-degree-conflictA1, ...],
  //  [direct-conflictB, 2nd-degree-conflict-B1, ...], ...]

  // possible problem: a 2nd-degree conflict could show up in multiple
  // lists (that is, there are multiple direct conflicts that prevent
  // another command pair from cleanly applying). maybe:

  // [[[dcA1, dcB1], [dcA2. dcB2], ...],
  //  [2dcA1, 2dcA2, ...], [2dcB1, 2dcB2, ...]]

  _reconcile: function BSS__reconcile(onComplete, commandLists) {
    let generator = yield;
    this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    let handlers = this._handlersForGenerator(generator);
    let listener = new EventListener(handlers['complete']);

    let [listA, listB] = commandLists;
    let propagations = [[], []];
    let conflicts = [[], []];

    for (let i = 0; i < listA.length; i++) {

      // Yield to main loop
      this._timer.initWithCallback(listener, 0, this._timer.TYPE_ONE_SHOT);
      yield;

      for (let j = 0; j < listB.length; j++) {
        if (this._deepEquals(listA[i], listB[j])) {
          delete listA[i];
          delete listB[j];
        } else if (this._commandLike(listA[i], listB[j])) {
          this._fixParents(listA, listA[i].guid, listB[j].guid);
          listB[j].data = {guid: listB[j].guid};
          listB[j].guid = listA[i].guid;
          listB[j].action = "edit";
          delete listA[i];
        }
      }
    }

    listA = listA.filter(function(elt) { return elt });
    listB = listB.filter(function(elt) { return elt });

    for (let i = 0; i < listA.length; i++) {

      // Yield to main loop
      this._timer.initWithCallback(listener, 0, this._timer.TYPE_ONE_SHOT);
      yield;

      for (let j = 0; j < listB.length; j++) {
        if (this._conflicts(listA[i], listB[j]) ||
            this._conflicts(listB[j], listA[i])) {
          if (!conflicts[0].some(
            function(elt) { return elt.guid == listA[i].guid }))
            conflicts[0].push(listA[i]);
          if (!conflicts[1].some(
            function(elt) { return elt.guid == listB[j].guid }))
            conflicts[1].push(listB[j]);
        }
      }
    }
    for (let i = 0; i < listA.length; i++) {
      // need to check if a previous conflict might break this cmd
      if (!conflicts[0].some(
        function(elt) { return elt.guid == listA[i].guid }))
        propagations[1].push(listA[i]);
    }
    for (let j = 0; j < listB.length; j++) {
      // need to check if a previous conflict might break this cmd
      if (!conflicts[1].some(
        function(elt) { return elt.guid == listB[j].guid }))
        propagations[0].push(listB[j]);
    }

    this._timer = null;
    onComplete({propagations: propagations, conflicts: conflicts});
  },

  _applyCommandsToObj: function BSS__applyCommandsToObj(commands, obj) {
    for (let i = 0; i < commands.length; i++) {
      LOG("Applying cmd to obj: " + uneval(commands[i]));
      switch (commands[i].action) {
      case "create":
        obj[commands[i].guid] = eval(uneval(commands[i].data));
        break;
      case "edit":
        for (let prop in commands[i].data) {
          obj[commands[i].guid][prop] = commands[i].data[prop];
        }
        break;
      case "remove":
        delete obj[commands[i].guid];
        break;
      }
    }
    return obj;
  },

  // Applies commands to the places db
  _applyCommands: function BSS__applyCommands(commandList) {
    for (var i = 0; i < commandList.length; i++) {
      var command = commandList[i];
      LOG("Processing command: " + uneval(command));
      switch (command["action"]) {
      case "create":
        this._createCommand(command);
        break;
      case "remove":
        this._removeCommand(command);
        break;
      case "edit":
        this._editCommand(command);
        break;
      default:
        LOG("unknown action in command: " + command["action"]);
        break;
      }
    }
  },

  _createCommand: function BSS__createCommand(command) {
    let newId;
    let parentId = this._bms.getItemIdForGUID(command.data.parentGuid);

    if (parentId <= 0) {
      LOG("Warning: creating node with unknown parent -> reparenting to root");
      parentId = this._bms.bookmarksRoot;
    }

    LOG(" -> creating item");

    switch (command.data.type) {
    case 0:
      newId = this._bms.insertBookmark(parentId,
                                       makeURI(command.data.uri),
                                       command.data.index,
                                       command.data.title);
      break;
    case 6:
      newId = this._bms.createFolder(parentId,
                                     command.data.title,
                                     command.data.index);
      break;
    case 7:
      newId = this._bms.insertSeparator(parentId, command.data.index);
      break;
    default:
      LOG("_createCommand: Unknown item type: " + command.data.type);
      break;
    }
    if (newId)
      this._bms.setItemGUID(newId, command.guid);
  },

  _removeCommand: function BSS__removeCommand(command) {
    var itemId = this._bms.getItemIdForGUID(command.guid);
    var type = this._bms.getItemType(itemId);

    switch (type) {
    case this._bms.TYPE_BOOKMARK:
      // FIXME: check it's an actual bookmark?
      LOG("  -> removing bookmark " + command.guid);
      this._bms.removeItem(itemId);
      break;
    case this._bms.TYPE_FOLDER:
      LOG("  -> removing folder " + command.guid);
      this._bms.removeFolder(itemId);
      break;
    case this._bms.TYPE_SEPARATOR:
      LOG("  -> removing separator " + command.guid);
      this._bms.removeItem(itemId);
      break;
    default:
      LOG("removeCommand: Unknown item type: " + type);
      break;
    }
  },

  _editCommand: function BSS__editCommand(command) {
    var itemId = this._bms.getItemIdForGUID(command.guid);
    if (itemId == -1) {
      LOG("Warning: item for guid " + command.guid + " not found.  Skipping.");
      return;
    }

    var type = this._bms.getItemType(itemId);

    for (let key in command.data) {
      switch (key) {
      case "guid":
        var existing = this._bms.getItemIdForGUID(command.data.guid);
        if (existing == -1)
          this._bms.setItemGUID(itemId, command.data.guid);
        else
          LOG("Warning: can't change guid " + command.guid +
              " to " + command.data.guid + ": guid already exists.");
        break;
      case "title":
        this._bms.setItemTitle(itemId, command.data.title);
        break;
      case "uri":
        this._bms.changeBookmarkURI(itemId, makeURI(command.data.uri));
        break;
      case "index": // FIXME: what if we do this one before parentGuid ? that'd be wrong
        this._bms.moveItem(itemId, this._bms.getFolderIdForItem(itemId),
                           command.data.index);
        break;
      case "parentGuid":
        this._bms.moveItem(
          itemId, this._bms.getItemIdForGUID(command.data.parentGuid), -1);
        break;
      default:
        LOG("Warning: Can't change item property: " + key);
        break;
      }
    }
  },

  _getBookmarks: function BMS__getBookmarks(folder) {
    if (!folder)
      folder = this._bms.bookmarksRoot;
    var query = this._hsvc.getNewQuery();
    query.setFolders([folder], 1);
    return this._hsvc.executeQuery(query, this._hsvc.getNewQueryOptions()).root;
  },

  // 1) Fetch server deltas
  // 1.1) Construct current server status from snapshot + server deltas
  // 1.2) Generate single delta from snapshot -> current server status
  // 2) Generate local deltas from snapshot -> current client status
  // 3) Reconcile client/server deltas and generate new deltas for them.
  // 3.1) Apply local delta with server changes
  // 3.2) Append server delta to the delta file and upload

  _doSync: function BSS__doSync() {
    var generator = yield;
    var handlers = this._handlersForGenerator(generator);

    LOG("Beginning sync");

    try {
      //this._dav.lock(handlers);
      //var data = yield;
      var data;

      var localBookmarks = this._getBookmarks();
      var localJson = this._wrapNode(localBookmarks);
      LOG("local json: " + uneval(localJson));

      // 1) Fetch server deltas
      asyncRun(bind2(this, this._getServerData), handlers['complete'], localJson);
      var server = yield;

      LOG("server: " + uneval(server));
      if (server['status'] == 2) {
        LOG("Sync complete");
        return;
      } else if (server['status'] != 0 && server['status'] != 1) {
        LOG("Sync error");
        return;
      }

      LOG("Local snapshot version: " + this._snapshotVersion);
      LOG("Latest server version: " + server['version']);

      // 2) Generate local deltas from snapshot -> current client status

      LOG("Generating local updates");
      var localUpdates = this._detectUpdates(this._snapshot, localJson);
      LOG("updates: " + uneval(localUpdates));
      if (!(server['status'] == 1 || localUpdates.length > 0)) {
        LOG("Sync complete (1): no changes needed on client or server");
        return;
      }
	  
      // 3) Reconcile client/server deltas and generate new deltas for them.

      var propagations = [server['updates'], localUpdates];
      var conflicts = [[],[]];

      LOG("Reconciling updates");
      asyncRun(bind2(this, this._reconcile),
               handlers['complete'], [localUpdates, server.updates]);
      let ret = yield;
      propagations = ret.propagations;
      conflicts = ret.conflicts;

      LOG("Propagations: " + uneval(propagations) + "\n");
      LOG("Conflicts: " + uneval(conflicts) + "\n");
	  
      this._snapshotVersion = server['version'];

      if (!((propagations[0] && propagations[0].length) ||
            (propagations[1] && propagations[1].length) ||
            (conflicts &&
             (conflicts[0] && conflicts[0].length) ||
             (conflicts[1] && conflicts[1].length)))) {
        this._snapshot = this._wrapNode(localBookmarks);
        LOG("Sync complete (2): no changes needed on client or server");
        return;
      }

      if (conflicts && conflicts[0] && conflicts[0].length) {
        LOG("\nWARNING: Conflicts found, but we don't resolve conflicts yet!\n");
        LOG("Conflicts(1) " + uneval(conflicts[0]));
      }

      if (conflicts && conflicts[1] && conflicts[1].length) {
        LOG("\nWARNING: Conflicts found, but we don't resolve conflicts yet!\n");
        LOG("Conflicts(2) " + uneval(conflicts[1]));
      }

      // 3.1) Apply server changes to local store
      if (propagations[0] && propagations[0].length) {
        LOG("Applying changes locally");
        this._snapshot = this._wrapNode(localBookmarks);
        propagations[0] = this._applyCommandsToObj(this._snapshot, propagations[0]);
        this._applyCommands(propagations[0]);
        this._snapshot = this._wrapNode(localBookmarks);
      }

      // 3.2) Append server delta to the delta file and upload
      if (propagations[1] && propagations[1].length) {
        LOG("Uploading changes to server");
        this._snapshot = this._wrapNode(localBookmarks);
        this._snapshotVersion++;
        server['deltas'][this._snapshotVersion] = propagations[1];
        this._dav.PUT("bookmarks.delta", uneval(server['deltas']), handlers);
        data = yield;

        if (data.target.status >= 200 || data.target.status < 300)
          LOG("Successfully updated deltas on server");
        else
          LOG("Error: could not update deltas on server");
      }
      LOG("Sync complete");
    } finally {
      //this._dav.unlock(handlers);
      //data = yield;
    }
  },


  /* Get the deltas/combined updates from the server
   * Returns:
   *   status:
   *     -1: error
   *      0: no changes from server
   *      1: ok
   *      2: ok, initial sync
   *   version:
   *     the latest version on the server
   *   deltas:
   *     the individual deltas on the server
   *   updates:
   *     the relevant deltas (from our snapshot version to current),
   *     combined into a single set.
   */
  _getServerData: function BSS__getServerData(onComplete, localJson) {
    var generator = yield;
    var handlers = this._handlersForGenerator(generator);

    var ret = {status: -1, version: -1, deltas: null, updates: null};

    LOG("Getting bookmarks delta from server");
    this._dav.GET("bookmarks.delta", handlers);
    var data = yield;

    switch (data.target.status) {
    case 200:
      LOG("Got bookmarks delta from server");

      ret.deltas = eval(data.target.responseText);
      var tmp = eval(uneval(this._snapshot)); // fixme hack hack hack

      // FIXME: debug here for conditional below...
      LOG("[sync bowels] local version: " + this._snapshotVersion);
      for (var z in ret.deltas) {
        LOG("[sync bowels] remote version: " + z);
      }
      LOG("foo: " + uneval(ret.deltas[this._snapshotVersion + 1]));
      if (ret.deltas[this._snapshotVersion + 1])
        LOG("-> is true");
      else
        LOG("-> is false");

      if (ret.deltas[this._snapshotVersion + 1]) {
        // Merge the matching deltas into one, find highest version
        var keys = [];
        for (var v in ret.deltas) {
          if (v > this._snapshotVersion)
            keys.push(v);
          if (v > ret.version)
            ret.version = v;
        }
        keys = keys.sort();
        LOG("TMP: " + uneval(tmp));
        for (var i = 0; i < keys.length; i++) {
          tmp = this._applyCommandsToObj(ret.deltas[keys[i]], tmp);
          LOG("TMP: " + uneval(tmp));
        }
        ret.status = 1;
        ret["updates"] = this._detectUpdates(this._snapshot, tmp);

      } else if (ret.deltas[this._snapshotVersion]) {
        LOG("No changes from server");
        ret.status = 0;
        ret.version = this._snapshotVersion;
        ret.updates = [];

      } else {
        LOG("Server delta can't update from our snapshot version, getting full file");
        // generate updates from full local->remote snapshot diff
        asyncRun(bind2(this, this._getServerUpdatesFull),
                 handlers['complete'], localJson);
        data = yield;
        if (data.status == 2) {
          // we have a delta file but no snapshot on the server.  bad.
          // fixme?
          LOG("Error: Delta file on server, but snapshot file missing.  " +
              "New snapshot uploaded, may be inconsistent with deltas!");
        }

        var tmp = eval(uneval(this._snapshot)); // fixme hack hack hack
        tmp = this._applyCommandsToObj(data.updates, tmp);

        // fixme: this is duplicated from above, need to do some refactoring

        var keys = [];
        for (var v in ret.deltas) {
          if (v > this._snapshotVersion)
            keys.push(v);
          if (v > ret.version)
            ret.version = v;
        }
        keys = keys.sort();
        for (var i = 0; i < keys.length; i++) {
          tmp = this._applyCommandsToObj(ret.deltas[keys[i]], tmp);
        }

        ret.status = data.status;
        ret.updates = this._detectUpdates(this._snapshot, tmp);
        ret.version = data.version;
        var keys = [];
        for (var v in ret.deltas) {
          if (v > ret.version)
            ret.version = v;
        }
      }
      break;
    case 404:
      LOG("Server has no delta file.  Getting full bookmarks file from server");
      // generate updates from full local->remote snapshot diff
      asyncRun(bind2(this, this._getServerUpdatesFull), handlers['complete'], localJson);
      ret = yield;
      ret.deltas = {};
      break;
    default:
      LOG("Could not get bookmarks.delta: unknown HTTP status code " + data.target.status);
      break;
    }
    onComplete(ret);
  },

  _getServerUpdatesFull: function BSS__getServerUpdatesFull(onComplete, localJson) {
    var generator = yield;
    var handlers = this._handlersForGenerator(generator);

    var ret = {status: -1, version: -1, updates: null};

    this._dav.GET("bookmarks.json", handlers);
    data = yield;

    switch (data.target.status) {
    case 200:
      LOG("Got full bookmarks file from server");
      var tmp = eval(data.target.responseText);
      ret.status = 1;
      ret.updates = this._detectUpdates(this._snapshot, tmp.snapshot);
      ret.version = tmp.version;
      break;
    case 404:
      LOG("No bookmarks on server.  Starting initial sync to server");

      this._snapshot = localJson;
      this._snapshotVersion = 1;
      this._dav.PUT("bookmarks.json", uneval({version: 1, snapshot: this._snapshot}), handlers);
      data = yield;

      if (data.target.status >= 200 || data.target.status < 300) {
        LOG("Initial sync to server successful");
        ret.status = 2;
      } else {
        LOG("Initial sync to server failed");
      }
      break;
    default:
      LOG("Could not get bookmarks.json: unknown HTTP status code " + data.target.status);
      break;
    }
    onComplete(ret);
  },

  _handlersForGenerator: function BSS__handlersForGenerator(generator) {
    var h = {load: bind2(this, function(event) { continueGenerator(generator, event); }),
             error: bind2(this, function(event) { LOG("Request failed: " + uneval(event)); })};
    h['complete'] = h['load'];
    return h;
  },

  _onLogin: function BSS__onLogin(event) {
    LOG("Bookmarks sync server: " + this._dav.baseURL);
    this._os.notifyObservers(null, "bookmarks-sync:login", "");
  },

  _onLoginError: function BSS__onLoginError(event) {
    this._os.notifyObservers(null, "bookmarks-sync:login-error", "");
  },

  // Interfaces this component implements.
  interfaces: [Ci.nsIBookmarksSyncService, Ci.nsISupports],

  // XPCOM registration
  classDescription: "Bookmarks Sync Service",
  contractID: "@mozilla.org/places/sync-service;1",
  classID: Components.ID("{6efd73bf-6a5a-404f-9246-f70a1286a3d6}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBookmarksSyncService, Ci.nsISupports]),

  // nsISupports

  // nsIBookmarksSyncService

  sync: function BSS_sync() { asyncRun(bind2(this, this._doSync)); },

  login: function BSS_login() {
    this._dav.login({load: bind2(this, this._onLogin),
                     error: bind2(this, this._onLoginError)});
  },

  logout: function BSS_logout() {
    this._dav.logout();
    this._os.notifyObservers(null, "bookmarks-sync:logout", "");
  }
};

function makeFile(path) {
  var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
  file.initWithPath(path);
  return file;
}

function makeURI(uriString) {
  var ioservice = Cc["@mozilla.org/network/io-service;1"].
                  getService(Ci.nsIIOService);
  return ioservice.newURI(uriString, null, null);
}

function LOG(aText) {
  dump(aText + "\n");
  var consoleService = Cc["@mozilla.org/consoleservice;1"].
                       getService(Ci.nsIConsoleService);
  consoleService.logStringMessage(aText);
}

function bind2(object, method) {
  return function innerBind() { return method.apply(object, arguments); }
}

function asyncRun(func, handler, data) {
  var generator = func(handler, data);
  generator.next();
  generator.send(generator);
}

function continueGenerator(generator, data) {
  try { generator.send(data); }
  catch (e) {
    generator.close();
    if (e instanceof StopIteration)
      generator = null;
    else
	throw e;
  }
}

function EventListener(handler) {
  this._handler = handler;
}
EventListener.prototype = {
  QueryInterface: function(iid) {
    if (iid.Equals(Components.interfaces.nsITimerCallback) ||
        iid.Equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  // DOM event listener

  handleEvent: function EL_handleEvent(event) {
    this._handler(event);
  },

  // nsITimerCallback

  notify: function EL_notify(timer) {
    this._handler(timer);
  }
};

function DAVCollection(baseURL) {
  this._baseURL = baseURL;
  this._authProvider = new DummyAuthProvider();
}
DAVCollection.prototype = {
  _loggedIn: false,

  get baseURL() {
    return this._baseURL;
  },

  __base64: {},
  __vase64loaded: false,
  get _base64() {
    if (!this.__base64loaded) {
      let jsLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
        getService(Ci.mozIJSSubScriptLoader);
      jsLoader.loadSubScript("chrome://sync/content/base64.js", this.__base64);
      this.__base64loaded = true;
    }
    return this.__base64;
  },

  _authString: null,
  _currentUserPath: "nobody",

  _currentUser: "nobody@mozilla.com",
  get currentUser() {
    return this._currentUser;
  },

  _addHandler: function DC__addHandler(request, handlers, eventName) {
    if (handlers[eventName])
      request.addEventListener(eventName, new EventListener(handlers[eventName]), false);
  },
  _makeRequest: function DC__makeRequest(op, path, handlers, headers) {
    var request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance();
    request = request.QueryInterface(Ci.nsIDOMEventTarget);
  
    if (!handlers)
      handlers = {};
    this._addHandler(request, handlers, "load");
    this._addHandler(request, handlers, "error");
  
    request = request.QueryInterface(Ci.nsIXMLHttpRequest);
    request.open(op, this._baseURL + path, true);
  
    if (headers) {
      for (var key in headers) {
        request.setRequestHeader(key, headers[key]);
      }
    }

    request.channel.notificationCallbacks = this._authProvider;

    return request;
  },

  GET: function DC_GET(path, handlers, headers) {
    if (!headers)
      headers = {'Content-type': 'text/plain'};
    if (this._authString)
      headers['Authorization'] = this._authString;

    var request = this._makeRequest("GET", path, handlers, headers);
    request.send(null);
  },

  PUT: function DC_PUT(path, data, handlers, headers) {
    if (!headers)
      headers = {'Content-type': 'text/plain'};
    if (this._authString)
      headers['Authorization'] = this._authString;

    var request = this._makeRequest("PUT", path, handlers, headers);
    request.send(data);
  },

  // Login / Logout

  login: function DC_login(handlers) {
    this._loginHandlers = handlers;
    internalHandlers = {load: bind2(this, this._onLogin),
                        error: bind2(this, this._onLoginError)};

    try {
      let uri = makeURI(this._baseURL);
      let username = 'nobody@mozilla.com';
      let password;

      let branch = Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);
      username = branch.getCharPref("browser.places.sync.username");

      // fixme: make a request and get the realm
      let lm = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
      let logins = lm.findLogins({}, uri.hostPort, null,
                                 'Use your ldap username/password - dotmoz');
      LOG("Found " + logins.length + " logins");

      for (let i = 0; i < logins.length; i++) {
        if (logins[i].username == username) {
          password = logins[i].password;
          break;
        }
      }

      // FIXME: should we regen this each time to prevent it from staying in memory?
      this._authString = "Basic " +
        this._base64.Base64.encode(username + ":" + password);
      headers = {'Authorization': this._authString};
    } catch (e) {
      // try without the auth header?
      // fixme: may want to just fail here
    }

    let request = this._makeRequest("GET", "", internalHandlers, headers);
    request.send(null);
  },

  logout: function DC_logout() {
    this._authString = null;
  },

  _onLogin: function DC__onLogin(event) {
    //LOG("logged in (" + event.target.status + "):\n" +
    //    event.target.responseText + "\n");

    if (this._authProvider._authFailed || event.target.status >= 400) {
      this._onLoginError(event);
      return;
    }

    // fixme: hacktastic
    let hello = /Hello (.+)@mozilla.com/.exec(event.target.responseText)
    if (hello) {
      this._currentUserPath = hello[1];	
      this._currentUser = this._currentUserPath + "@mozilla.com";
      this._baseURL = "http://dotmoz.mozilla.org/~" +
        this._currentUserPath + "/";
    }

    this._loggedIn = true;

    if (this._loginHandlers && this._loginHandlers.load)
      this._loginHandlers.load(event);
  },
  _onLoginError: function DC__onLoginError(event) {
    LOG("login failed (" + event.target.status + "):\n" +
        event.target.responseText + "\n");

    this._loggedIn = false;

    if (this._loginHandlers && this._loginHandlers.error)
      this._loginHandlers.error(event);
  },

  // Locking

  // FIXME: make this function not reentrant
  lock: function DC_lock(handlers) {
    this._lockHandlers = handlers;
    internalHandlers = {load: bind2(this, this._onLock),
                        error: bind2(this, this._onLockError)};
    headers = {'Content-Type': 'text/xml; charset="utf-8"'};
    var request = this._makeRequest("LOCK", "", internalHandlers, headers);
    request.send("<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n" +
                 "<D:lockinfo xmlns:D=\"DAV:\">\n" +
                 "  <D:locktype><D:write/></D:locktype>\n" +
                 "  <D:lockscope><D:exclusive/></D:lockscope>\n" +
                 "</D:lockinfo>");
  },

  // FIXME: make this function not reentrant
  unlock: function DC_unlock(handlers) {
    this._lockHandlers = handlers;
    internalHandlers = {load: bind2(this, this._onUnlock),
                        error: bind2(this, this._onUnlockError)};
    headers = {'Lock-Token': "<" + this._token + ">"};
    var request = this._makeRequest("UNLOCK", "", internalHandlers, headers);
    request.send(null);
  },

  _onLock: function DC__onLock(event) {
    LOG("acquired lock (" + event.target.status + "):\n" + event.target.responseText + "\n");
    this._token = "woo";
    if (this._lockHandlers && this._lockHandlers.load)
      this._lockHandlers.load(event);
  },
  _onLockError: function DC__onLockError(event) {
    LOG("lock failed (" + event.target.status + "):\n" + event.target.responseText + "\n");
    if (this._lockHandlers && this._lockHandlers.error)
      this._lockHandlers.error(event);
  },

  _onUnlock: function DC__onUnlock(event) {
    LOG("removed lock (" + event.target.status + "):\n" + event.target.responseText + "\n");
    this._token = null;
    if (this._lockHandlers && this._lockHandlers.load)
      this._lockHandlers.load(event);
  },
  _onUnlockError: function DC__onUnlockError(event) {
    LOG("unlock failed (" + event.target.status + "):\n" + event.target.responseText + "\n");
    if (this._lockHandlers && this._lockHandlers.error)
      this._lockHandlers.error(event);
  }
};


// Taken from nsMicrosummaryService.js and massaged slightly
function DummyAuthProvider() {}
DummyAuthProvider.prototype = {
  // Implement notification callback interfaces so we can suppress UI
  // and abort loads for bad SSL certs and HTTP authorization requests.
  
  // Interfaces this component implements.
  interfaces: [Ci.nsIBadCertListener,
               Ci.nsIAuthPromptProvider,
               Ci.nsIAuthPrompt,
               Ci.nsIPrompt,
               Ci.nsIInterfaceRequestor,
               Ci.nsISupports],

  // Auth requests appear to succeed when we cancel them (since the server
  // redirects us to a "you're not authorized" page), so we have to set a flag
  // to let the load handler know to treat the load as a failure.
  get _authFailed()         { return this.__authFailed; },
  set _authFailed(newValue) { return this.__authFailed = newValue },

  // nsISupports

  QueryInterface: function DAP_QueryInterface(iid) {
    if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
      throw Cr.NS_ERROR_NO_INTERFACE;

    // nsIAuthPrompt and nsIPrompt need separate implementations because
    // their method signatures conflict.  The other interfaces we implement
    // within DummyAuthProvider itself.
    switch(iid) {
    case Ci.nsIAuthPrompt:
      return this.authPrompt;
    case Ci.nsIPrompt:
      return this.prompt;
    default:
      return this;
    }
  },

  // nsIInterfaceRequestor
  
  getInterface: function DAP_getInterface(iid) {
    return this.QueryInterface(iid);
  },

  // nsIBadCertListener

  // Suppress UI and abort secure loads from servers with bad SSL certificates.
  
  confirmUnknownIssuer: function DAP_confirmUnknownIssuer(socketInfo, cert, certAddType) {
    return false;
  },

  confirmMismatchDomain: function DAP_confirmMismatchDomain(socketInfo, targetURL, cert) {
    return false;
  },

  confirmCertExpired: function DAP_confirmCertExpired(socketInfo, cert) {
    return false;
  },

  notifyCrlNextupdate: function DAP_notifyCrlNextupdate(socketInfo, targetURL, cert) {
  },

  // nsIAuthPromptProvider
  
  getAuthPrompt: function(aPromptReason, aIID) {
    this._authFailed = true;
    throw Cr.NS_ERROR_NOT_AVAILABLE;
  },

  // HTTP always requests nsIAuthPromptProvider first, so it never needs
  // nsIAuthPrompt, but not all channels use nsIAuthPromptProvider, so we
  // implement nsIAuthPrompt too.

  // nsIAuthPrompt

  get authPrompt() {
    var resource = this;
    return {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIPrompt]),
      prompt: function(dialogTitle, text, passwordRealm, savePassword, defaultText, result) {
        resource._authFailed = true;
        return false;
      },
      promptUsernameAndPassword: function(dialogTitle, text, passwordRealm, savePassword, user, pwd) {
        resource._authFailed = true;
        return false;
      },
      promptPassword: function(dialogTitle, text, passwordRealm, savePassword, pwd) {
        resource._authFailed = true;
        return false;
      }
    };
  },

  // nsIPrompt

  get prompt() {
    var resource = this;
    return {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIPrompt]),
      alert: function(dialogTitle, text) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      alertCheck: function(dialogTitle, text, checkMessage, checkValue) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      confirm: function(dialogTitle, text) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      confirmCheck: function(dialogTitle, text, checkMessage, checkValue) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      confirmEx: function(dialogTitle, text, buttonFlags, button0Title, button1Title, button2Title, checkMsg, checkValue) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      prompt: function(dialogTitle, text, value, checkMsg, checkValue) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      promptPassword: function(dialogTitle, text, password, checkMsg, checkValue) {
        resource._authFailed = true;
        return false;
      },
      promptUsernameAndPassword: function(dialogTitle, text, username, password, checkMsg, checkValue) {
        resource._authFailed = true;
        return false;
      },
      select: function(dialogTitle, text, count, selectList, outSelection) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      }
    };
  }
};

function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule([BookmarksSyncService]);
}
