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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;

const PERMS_FILE      = 0644;
const PERMS_DIRECTORY = 0755;

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

  __console: null,
  get _console() {
    if (!this.__console)
      this.__console = Cc["@mozilla.org/consoleservice;1"]
        .getService(Ci.nsIConsoleService);
    return this.__console;
  },

  __dirSvc: null,
  get _dirSvc() {
    if (!this.__dirSvc)
      this.__dirSvc = Cc["@mozilla.org/file/directory_service;1"].
        getService(Ci.nsIProperties);
    return this.__dirSvc;
  },

  // File output stream to a log file
  _log: null,

  // Timer object for yielding to the main loop
  _timer: null,

  // DAVCollection object
  _dav: null,

  // Last synced tree and version
  _snapshot: {},
  _snapshotVersion: 0,

  get currentUser() {
    return this._dav.currentUser;
  },

  _init: function BSS__init() {
    this._initLog();
    let serverURL = 'https://dotmoz.mozilla.org/';
    try {
      let branch = Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);
      serverURL = branch.getCharPref("browser.places.sync.serverURL");
    }
    catch (ex) { /* use defaults */ }
    this.notice("Bookmarks login server: " + serverURL);
    this._dav = new DAVCollection(serverURL);
    this._readSnapshot();
  },

  _initLog: function BSS__initLog() {
    let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
    getService(Ci.nsIProperties);

    let file = dirSvc.get("ProfD", Ci.nsIFile);
    file.append("bm-sync.log");
    file.QueryInterface(Ci.nsILocalFile);

    if (file.exists())
      file.remove(false);
    file.create(file.NORMAL_FILE_TYPE, PERMS_FILE);

    this._log = Cc["@mozilla.org/network/file-output-stream;1"]
      .createInstance(Ci.nsIFileOutputStream);
    let flags = MODE_WRONLY | MODE_CREATE | MODE_APPEND;
    this._log.init(file, flags, PERMS_FILE, 0);

    let str = "Bookmarks Sync Log\n------------------\n\n";
    this._log.write(str, str.length);
    this._log.flush();
  },

  _saveSnapshot: function BSS__saveSnapshot() {
    let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties);

    let file = dirSvc.get("ProfD", Ci.nsIFile);
    file.append("bm-sync-snapshot.json");
    file.QueryInterface(Ci.nsILocalFile);

    if (!file.exists())
      file.create(file.NORMAL_FILE_TYPE, PERMS_FILE);

    let fos = Cc["@mozilla.org/network/file-output-stream;1"].
      createInstance(Ci.nsIFileOutputStream);
    let flags = MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE;
    fos.init(file, flags, PERMS_FILE, 0);

    let out = {version: this._snapshotVersion, snapshot: this._snapshot};
    out = uneval(out);
    fos.write(out, out.length);
    fos.close();
  },

  _readSnapshot: function BSS__readSnapshot() {
    let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties);

    let file = dirSvc.get("ProfD", Ci.nsIFile);
    file.append("bm-sync-snapshot.json");

    if (!file.exists())
      return;

    let fis = Cc["@mozilla.org/network/file-input-stream;1"].
      createInstance(Ci.nsIFileInputStream);
    fis.init(file, MODE_RDONLY, PERMS_FILE, 0);
    fis.QueryInterface(Ci.nsILineInputStream);

    let json = "";
    while (fis.available()) {
      let ret = {};
      fis.readLine(ret);
      json += ret.value;
    }
    fis.close();
    json = eval(json);

    if (json.snapshot && json.version) {
      this._snapshot = json.snapshot;
      this._snapshotVersion = json.version;
    }
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
    if (!tree[guid] || !tree[guid].parentGuid)
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
      this.notice("_commandLike: Unknown item type: " + uneval(a));
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

      this._timer.initWithCallback(listener, 0, this._timer.TYPE_ONE_SHOT);
      yield; // Yield to main loop

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

      this._timer.initWithCallback(listener, 0, this._timer.TYPE_ONE_SHOT);
      yield; // Yield to main loop

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
    let ret = {propagations: propagations, conflicts: conflicts};
    this._generatorDone(onComplete, ret);

    // shutdown protocol
    let cookie = yield;
    if (cookie != "generator shutdown")
      this.notice("_reconcile: Error: generator not properly shut down.")
  },

  _applyCommandsToObj: function BSS__applyCommandsToObj(commands, obj) {
    for (let i = 0; i < commands.length; i++) {
      this.notice("Applying cmd to obj: " + uneval(commands[i]));
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
      this.notice("Processing command: " + uneval(command));
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
        this.notice("unknown action in command: " + command["action"]);
        break;
      }
    }
  },

  _createCommand: function BSS__createCommand(command) {
    let newId;
    let parentId = this._bms.getItemIdForGUID(command.data.parentGuid);

    if (parentId <= 0) {
      this.notice("Warning: creating node with unknown parent -> reparenting to root");
      parentId = this._bms.bookmarksRoot;
    }

    this.notice(" -> creating item");

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
      this.notice("_createCommand: Unknown item type: " + command.data.type);
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
      this.notice("  -> removing bookmark " + command.guid);
      this._bms.removeItem(itemId);
      break;
    case this._bms.TYPE_FOLDER:
      this.notice("  -> removing folder " + command.guid);
      this._bms.removeFolder(itemId);
      break;
    case this._bms.TYPE_SEPARATOR:
      this.notice("  -> removing separator " + command.guid);
      this._bms.removeItem(itemId);
      break;
    default:
      this.notice("removeCommand: Unknown item type: " + type);
      break;
    }
  },

  _editCommand: function BSS__editCommand(command) {
    var itemId = this._bms.getItemIdForGUID(command.guid);
    if (itemId == -1) {
      this.notice("Warning: item for guid " + command.guid + " not found.  Skipping.");
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
          this.notice("Warning: can't change guid " + command.guid +
              " to " + command.data.guid + ": guid already exists.");
        break;
      case "title":
        this._bms.setItemTitle(itemId, command.data.title);
        break;
      case "uri":
        this._bms.changeBookmarkURI(itemId, makeURI(command.data.uri));
        break;
      case "index":
        this._bms.moveItem(itemId, this._bms.getFolderIdForItem(itemId),
                           command.data.index);
        break;
      case "parentGuid":
        let index = -1;
        if (command.data.index && command.data.index >= 0)
          index = command.data.index;
        this._bms.moveItem(
          itemId, this._bms.getItemIdForGUID(command.data.parentGuid), index);
        break;
      default:
        this.notice("Warning: Can't change item property: " + key);
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

  // FIXME: these print functions need some love...
  _mungeJSON: function BSS__mungeJSON(json) {
    json.replace(":{type", ":\n\t{type");
    json.replace(", ", ",\n\t");
    return json;
  },

  _printNodes: function BSS__printNodes(nodes) {
    let nodeList = [];
    for (let guid in nodes) {
      switch (nodes[guid].type) {
      case 0:
        nodeList.push(nodes[guid].parentGuid + " b " + guid + "\n\t" +
                      nodes[guid].title + nodes[guid].uri);
        break;
      case 6:
        nodeList.push(nodes[guid].parentGuid + " f " + guid + "\n\t" +
                      nodes[guid].title);
        break;
      case 7:
        nodeList.push(nodes[guid].parentGuid + " s " + guid);
        break;
      default:
        nodeList.push("error: unknown item type!\n" + uneval(nodes[guid]));
        break;
      }
    }
    nodeList.sort();
    return nodeList.join("\n");
  },

  _printCommands: function BSS__printCommands(commands) {
    let ret = [];
    for (let i = 0; i < commands.length; i++) {
      switch (commands[i].action) {
      case "create":
        ret.push("create");
        break;
      case "edit":
        ret.push();
        break;
      case "remove":
        ret.push();
        break;
      default:
        ret.push("error: unknown command action!\n" + uneval(commands[i]));
        break;
      }
    }
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

    this._os.notifyObservers(null, "bookmarks-sync:start", "");
    this.notice("Beginning sync");

    try {
      //this._dav.lock(handlers);
      //var data = yield;
      var data;

      var localBookmarks = this._getBookmarks();
      var localJson = this._wrapNode(localBookmarks);
      this.notice("local json:\n" + this._mungeJSON(uneval(localJson)));

      // 1) Fetch server deltas
      let gsd_gen = this._getServerData(handlers['complete'], localJson);
      gsd_gen.next(); // must initialize before sending
      gsd_gen.send(gsd_gen);
      let server = yield;

      this.notice("server: " + uneval(server));
      if (server['status'] == 2) {
        this._os.notifyObservers(null, "bookmarks-sync:end", "");
        this.notice("Sync complete");
        return;
      } else if (server['status'] != 0 && server['status'] != 1) {
        this._os.notifyObservers(null, "bookmarks-sync:end", "");
        this.notice("Sync error");
        return;
      }

      this.notice("Local snapshot version: " + this._snapshotVersion);
      this.notice("Latest server version: " + server['version']);

      // 2) Generate local deltas from snapshot -> current client status

      this.notice("Generating local updates");
      var localUpdates = this._detectUpdates(this._snapshot, localJson);
      this.notice("updates: " + uneval(localUpdates));
      if (!(server['status'] == 1 || localUpdates.length > 0)) {
        this._os.notifyObservers(null, "bookmarks-sync:end", "");
        this.notice("Sync complete (1): no changes needed on client or server");
        return;
      }
	  
      // 3) Reconcile client/server deltas and generate new deltas for them.

      this.notice("Reconciling updates");
      let callback = function(retval) { continueGenerator(generator, retval); };
      let rec_gen = this._reconcile(callback, [localUpdates, server.updates]);
      rec_gen.next(); // must initialize before sending
      rec_gen.send(rec_gen);
      let ret = yield;
      // FIXME: Need to come up with a closing protocol for generators
      rec_gen.close();

      let clientChanges = [];
      let serverChanges = [];
      let clientConflicts = [];
      let serverConflicts = [];

      if (ret.propagations[0])
        clientChanges = ret.propagations[0];
      if (ret.propagations[1])
        serverChanges = ret.propagations[1];

      if (ret.conflicts && ret.conflicts[0])
        clientConflicts = ret.conflicts[0];
      if (ret.conflicts && ret.conflicts[1])
        serverConflicts = ret.conflicts[1];

      this.notice("Changes for client: " + uneval(clientChanges));
      this.notice("Changes for server: " + uneval(serverChanges));
      this.notice("Client conflicts: " + uneval(clientConflicts));
      this.notice("Server conflicts: " + uneval(serverConflicts));

      if (!(clientChanges.length || serverChanges.length ||
            clientConflicts.length || serverConflicts.length)) {
        this._os.notifyObservers(null, "bookmarks-sync:end", "");
        this.notice("Sync complete (2): no changes needed on client or server");
        this._snapshot = this._wrapNode(localBookmarks);
        this._snapshotVersion = server['version'];
        this._saveSnapshot();
        return;
      }

      if (clientConflicts.length || serverConflicts.length) {
        this.notice("\nWARNING: Conflicts found, but we don't resolve conflicts yet!\n");
      }

      // 3.1) Apply server changes to local store
      if (clientChanges.length) {
        this.notice("Applying changes locally");
        // Note that we need to need to apply client changes to the
        // current tree, not the saved snapshot
        this._snapshot = this._applyCommandsToObj(clientChanges,
                                                  this._wrapNode(localBookmarks));
        this._snapshotVersion = server['version'];
        this._applyCommands(clientChanges);

        let newSnapshot = this._wrapNode(localBookmarks);
        let diff = this._detectUpdates(this._snapshot, newSnapshot);
        if (diff.length != 0) {
          this.notice("Error: commands did not apply correctly.  Diff:\n" +
                      uneval(diff));
          this._snapshot = this._wrapNode(localBookmarks);
          // FIXME: What else can we do?
        }
        this._saveSnapshot();
      }

      // 3.2) Append server delta to the delta file and upload
      if (serverChanges.length) {
        this.notice("Uploading changes to server");
        this._snapshot = this._wrapNode(localBookmarks);
        this._snapshotVersion = server['version'] + 1;
        server['deltas'][this._snapshotVersion] = serverChanges;
        this._dav.PUT("bookmarks.delta", uneval(server['deltas']), handlers);
        data = yield;

        if (data.target.status >= 200 || data.target.status < 300) {
          this.notice("Successfully updated deltas on server");
          this._saveSnapshot();
        } else {
          // FIXME: revert snapshot here? - can't, we already applied
          // updates locally! - need to save and retry
          this.notice("Error: could not update deltas on server");
        }
      }
      this._os.notifyObservers(null, "bookmarks-sync:end", "");
      this.notice("Sync complete");
    } finally {
      //this._dav.unlock(handlers);
      //data = yield;
      this._os.notifyObservers(null, "bookmarks-sync:end", "");
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

    this.notice("Getting bookmarks delta from server");
    this._dav.GET("bookmarks.delta", handlers);
    var data = yield;

    switch (data.target.status) {
    case 200:
      this.notice("Got bookmarks delta from server");

      ret.deltas = eval(data.target.responseText);
      var tmp = eval(uneval(this._snapshot)); // fixme hack hack hack

      // FIXME: debug here for conditional below...
      /*
      this.notice("[sync bowels] local version: " + this._snapshotVersion);
      for (var z in ret.deltas) {
        this.notice("[sync bowels] remote version: " + z);
      }
      this.notice("foo: " + uneval(ret.deltas[this._snapshotVersion + 1]));
      if (ret.deltas[this._snapshotVersion + 1])
        this.notice("-> is true");
      else
        this.notice("-> is false");
      */

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
        //this.notice("TMP: " + uneval(tmp));
        for (var i = 0; i < keys.length; i++) {
          tmp = this._applyCommandsToObj(ret.deltas[keys[i]], tmp);
          //this.notice("TMP: " + uneval(tmp));
        }
        ret.status = 1;
        ret["updates"] = this._detectUpdates(this._snapshot, tmp);

      } else if (ret.deltas[this._snapshotVersion]) {
        this.notice("No changes from server");
        ret.status = 0;
        ret.version = this._snapshotVersion;
        ret.updates = [];

      } else {
        this.notice("Server delta can't update from our snapshot version, getting full file");
        // generate updates from full local->remote snapshot diff
        let gsdf_gen = this._getServerUpdatesFull(handlers['complete'],
                                                  localJson);
        gsdf_gen.next(); // must initialize before sending
        gsdf_gen.send(gsdf_gen);
        data = yield;
        if (data.status == 2) {
          // we have a delta file but no snapshot on the server.  bad.
          // fixme?
          this.notice("Error: Delta file on server, but snapshot file missing.  " +
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
      this.notice("Server has no delta file.  Getting full bookmarks file from server");
      // generate updates from full local->remote snapshot diff
      let gsdf_gen = this._getServerUpdatesFull(handlers['complete'], localJson);
      gsdf_gen.next(); // must initialize before sending
      gsdf_gen.send(gsdf_gen);
      ret = yield;
      ret.deltas = {};
      break;
    default:
      this.notice("Could not get bookmarks.delta: unknown HTTP status code " + data.target.status);
      break;
    }
    this._generatorDone(onComplete, ret)
  },

  _getServerUpdatesFull: function BSS__getServerUpdatesFull(onComplete, localJson) {
    let generator = yield;
    let handlers = this._handlersForGenerator(generator);

    var ret = {status: -1, version: -1, updates: null};

    this._dav.GET("bookmarks.json", handlers);
    data = yield;

    switch (data.target.status) {
    case 200:
      this.notice("Got full bookmarks file from server");
      var tmp = eval(data.target.responseText);
      ret.status = 1;
      ret.updates = this._detectUpdates(this._snapshot, tmp.snapshot);
      ret.version = tmp.version;
      break;
    case 404:
      this.notice("No bookmarks on server.  Starting initial sync to server");

      this._snapshot = localJson;
      this._snapshotVersion = 1;
      this._dav.PUT("bookmarks.json", uneval({version: 1, snapshot: this._snapshot}), handlers);
      data = yield;

      if (data.target.status >= 200 || data.target.status < 300) {
        this.notice("Initial sync to server successful");
        ret.status = 2;
      } else {
        this.notice("Initial sync to server failed");
      }
      break;
    default:
      this.notice("Could not get bookmarks.json: unknown HTTP status code " + data.target.status);
      break;
    }
    this._generatorDone(onComplete, ret);
  },

  _handlersForGenerator: function BSS__handlersForGenerator(generator) {
    var h = {load: bind2(this, function(data) { continueGenerator(generator, data); }),
             error: bind2(this, function(data) { this.notice("Request failed: " + uneval(data)); })};
    h['complete'] = h['load'];
    return h;
  },

  // generators called using asyncRun can't simply call the callback
  // with the return value, since that would cause the calling
  // function to end up running (after the yield) from inside the
  // generator.  Instead, generators can call this method which sets
  // up a timer to call the callback from a timer (and cleans up the
  // timer to avoid leaks).
  _generatorDone: function BSS__generatorDone(callback, retval) {
    if (this._timer)
      throw "Called generatorDone when there is a timer already set."

    let cb = bind2(this, function(event) {
      this._timer = null;
      callback(retval);
    });

    this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._timer.initWithCallback(new EventListener(cb),
                                 0, this._timer.TYPE_ONE_SHOT);
  },

  _onLogin: function BSS__onLogin(event) {
    this.notice("Bookmarks sync server: " + this._dav.baseURL);
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

  sync: function BSS_sync() {
    let sync_gen = this._doSync();
    sync_gen.next(); // must initialize before sending
    sync_gen.send(sync_gen);
  },

  login: function BSS_login() {
    this._dav.login({load: bind2(this, this._onLogin),
                     error: bind2(this, this._onLoginError)});
  },

  logout: function BSS_logout() {
    this._dav.logout();
    this._os.notifyObservers(null, "bookmarks-sync:logout", "");
  },

  log: function BSS_log(line) {
  },

  notice: function BSS_notice(line) {
    let fullLine = line + "\n";
    dump(fullLine);
    this._console.logStringMessage(line);
    this._log.write(fullLine, fullLine.length);
    this._log.flush();
  },

  error: function BSS_error(line) {
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

function bind2(object, method) {
  return function innerBind() { return method.apply(object, arguments); }
}

function continueGenerator(generator, data) {
  try { generator.send(data); }
  catch (e) {
    //notice("continueGenerator exception! - " + e);
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
      //notice("Found " + logins.length + " logins");

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
    //notice("logged in (" + event.target.status + "):\n" +
    //    event.target.responseText + "\n");

    if (this._authProvider._authFailed || event.target.status >= 400) {
      this._onLoginError(event);
      return;
    }

    // fixme: hacktastic
//    let hello = /Hello (.+)@mozilla.com/.exec(event.target.responseText)
//    let hello = /Index of/.exec(event.target.responseText)
//    if (hello) {
//      this._currentUserPath = hello[1];	
//      this._currentUser = this._currentUserPath + "@mozilla.com";
//      this._baseURL = "http://dotmoz.mozilla.org/~" +
//        this._currentUserPath + "/";
//    }

      // XXX we need to refine some server response codes
    let branch = Cc["@mozilla.org/preferences-service;1"].
      getService(Ci.nsIPrefBranch);
    this._currentUser = branch.getCharPref("browser.places.sync.username");

    // XXX
    let path = this._currentUser.split("@");
    this._currentUserPath = path[0];

    let serverURL = branch.getCharPref("browser.places.sync.serverURL");
    this._baseURL = serverURL + this._currentUserPath + "/";
    
    this._loggedIn = true;

    if (this._loginHandlers && this._loginHandlers.load)
      this._loginHandlers.load(event);
  },
  _onLoginError: function DC__onLoginError(event) {
    //notice("login failed (" + event.target.status + "):\n" +
    //    event.target.responseText + "\n");

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
    //notice("acquired lock (" + event.target.status + "):\n" + event.target.responseText + "\n");
    this._token = "woo";
    if (this._lockHandlers && this._lockHandlers.load)
      this._lockHandlers.load(event);
  },
  _onLockError: function DC__onLockError(event) {
    //notice("lock failed (" + event.target.status + "):\n" + event.target.responseText + "\n");
    if (this._lockHandlers && this._lockHandlers.error)
      this._lockHandlers.error(event);
  },

  _onUnlock: function DC__onUnlock(event) {
    //notice("removed lock (" + event.target.status + "):\n" + event.target.responseText + "\n");
    this._token = null;
    if (this._lockHandlers && this._lockHandlers.load)
      this._lockHandlers.load(event);
  },
  _onUnlockError: function DC__onUnlockError(event) {
    //notice("unlock failed (" + event.target.status + "):\n" + event.target.responseText + "\n");
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
