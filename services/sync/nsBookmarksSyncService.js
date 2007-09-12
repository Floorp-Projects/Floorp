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

  // DAVCollection object
  _dav: null,

  // sync.js
  _sync: {},

  // PlacesUtils
  _utils: {},

  // Last synced tree
  // FIXME: this should be serialized to disk
  _snapshot: {},
  _snapshotVersion: 0,

  _init: function BSS__init() {

    var serverUrl = "http://sync.server.url/";
    try {
      var branch = Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);
      serverUrl = branch.getCharPref("browser.places.sync.serverUrl");
    }
    catch (ex) { /* use defaults */ }
    LOG("Bookmarks sync server: " + serverUrl);
    this._dav = new DAVCollection(serverUrl);

    var jsLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
      getService(Ci.mozIJSSubScriptLoader);
    jsLoader.loadSubScript("chrome://sync/content/sync-engine.js", this._sync);
    jsLoader.loadSubScript("chrome://browser/content/places/utils.js", this._utils);
  },

  _applyCommands: function BSS__applyCommands(node, commandList) {
    for (var i = 0; i < commandList.length; i++) {
      var command = commandList[i];
      LOG("Processing command: " + uneval(command));
      switch (command["action"]) {
      case "create":
        this._createCommand(node, this._snapshot, command);
        break;
      case "remove":
        this._removeCommand(node, command);
        break;
      case "edit":
        this._editCommand(node, command);
        break;
      default:
        LOG("unknown action in command: " + command["action"]);
        break;
      }
    }
  },

  _nodeFromPath: function BSS__nodeFromPath (aNodeRoot, aPath) {
    var node = aNodeRoot;
    for (var i = 0; i < aPath.length; i = i + 2) {
      if (aPath[i] != "children")
        break;
      node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      var openState = node.containerOpen;
      node.containerOpen = true;
      node = node.getChild(aPath[i + 1]);
      //node.containerOpen = openState; // fixme?
    }
    return node;
  },

  _createCommand: function BSS__createCommand(aNode, aJsonNode, aCommand) {
    var path = aCommand["path"];
    if (path[path.length - 2] != "children")
      path = aCommand["path"].slice(0, path.length - 1);

    var json = this._sync.pathToReference(aJsonNode, path);
    if (json["_done"] == true)
      return;
    json["_done"] = true;

    var index = path[path.length - 1];
    var node = this._nodeFromPath(aNode, path.slice(0, path.length - 2));

    switch (json["type"]) {
    case 0:
      LOG("  -> creating a bookmark: '" + json["title"] + "' -> " + json["uri"]);
      this._bms.insertBookmark(node.itemId, makeURI(json["uri"]), index, json["title"]);
      break;
    case 6:
      LOG("  -> creating a folder: '" + json["title"] + "'");
      this._bms.createFolder(node.itemId, json["title"], index);
      break;
    case 7:
      LOG("  -> creating a separator");
      this._bms.insertSeparator(node.itemId, index);
      break;
    default:
      LOG("createCommand: Unknown item type: " + json["type"]);
      break;
    }
  },

  _removeCommand: function BSS__removeCommand(node, command) {
    if (command["path"].length == 0) {
      LOG("removing item");
      switch (node.type) {
      case node.RESULT_TYPE_URI:
        // FIXME: check it's an actual bookmark?
        this._bms.removeItem(node.itemId);
        break;
      case node.RESULT_TYPE_FOLDER:
        this._bms.removeFolder(node.itemId);
        break;
      case node.RESULT_TYPE_SEPARATOR:
        this._bms.removeItem(node.itemId);
        break;
      default:
        LOG("removeCommand: Unknown item type: " + node.type);
        break;
      }
    } else if (command["path"].shift() == "children") {
      if (command["path"].length == 0) {
        LOG("invalid command?");
        return;
      }

      var index = command["path"].shift();

      node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      var openState = node.containerOpen;
      node.containerOpen = true;
      this._removeCommand(node.getChild(index), command);
      node.containerOpen = openState;
    }
  },

  _editCommand: function BSS__editCommand(node, command) {
    switch (command["path"].shift()) {
    case "children":
      node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      var openState = node.containerOpen;
      node.containerOpen = true;
      this._editCommand(node.getChild(command["path"].shift()), command);
      node.containerOpen = openState;
      break;
    case "type":
      LOG("Can't change item type!"); // FIXME: is this valid?
      break;
    case "title":
      this._bms.setItemTitle(node.itemId, command["value"]);
      break;
    case "uri":
      this._bms.changeBookmarkURI(node.itemId, makeURI(command["value"]));
      break;
    }
  },

  // FIXME - hack to make sure we have Commands, not just eval'ed hashes
  _sanitizeCommands: function BSS__sanitizeCommands(hashes) {
    var commands = [];
    for (var i = 0; i < hashes.length; i++) {
      commands.push(new this._sync.Command(hashes[i]["action"],
                                           hashes[i]["path"],
                                           hashes[i]["value"]));
    }
    return commands;
  },

  _getLocalBookmarks: function BMS__getLocalBookmarks() {
    var query = this._hsvc.getNewQuery();
    query.setFolders([this._bms.bookmarksRoot], 1);
    return this._hsvc.executeQuery(query, this._hsvc.getNewQueryOptions()).root;
  },

  // FIXME: temp version here because we can't yet get to PlacesUtils.wrapNode
  _wrapNode: function BSS__wrapNode(node) {
    //var guid = this._bms.getItemGuid(node.itemId);
    var item = {"type": node.type}; //,
    //                "guid": guid};

    if (node.type == node.RESULT_TYPE_FOLDER) {
      node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      var openState = node.containerOpen;
      node.containerOpen = true;
      var children = [];
      for (var i = 0; i < node.childCount; i++) {
        children.push(this._wrapNode(node.getChild(i)));
      }
      item["children"] = children;
      item["title"] = node.title;
      node.containerOpen = openState;
    } else if (node.type == node.RESULT_TYPE_SEPARATOR) {
    } else if (node.type == node.RESULT_TYPE_URI) {
      // FIXME: need to verify that it's a bookmark, it could be a history result!
      item["title"] = node.title;
      item["uri"] = node.uri;
    } else {
      // what do we do?
    }

    return item;
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

      var localBookmarks = this._getLocalBookmarks();
      var localJson = this._wrapNode(localBookmarks);

      // 1) Fetch server deltas
      asyncRun(bind2(this, this._getServerData), handlers['complete'], localJson);
      var server = yield;

      if (server['status'] == 2) {
        LOG("Sync complete");
        return;
      } else if (server['status'] != 0 && server['status'] != 1) {
        LOG("Sync error");
        return;
      }

      // 2) Generate local deltas from snapshot -> current client status
      LOG("Generating local updates");
      var localUpdates = this._sanitizeCommands(this._sync.detectUpdates(this._snapshot, localJson));

      // 3) Reconcile client/server deltas and generate new deltas for them.

      if (!(server['status'] == 1 || localUpdates.length > 0)) {
        LOG("Sync complete: no changes needed on client or server");
        return;
      }

      var propagations = [server['updates'], localUpdates];

      if (server['status'] == 1 && localUpdates.length > 0) {
        LOG("Reconciling updates");
        var propagations = this._sync.reconcile([localUpdates, server['updates']]);
      }
      LOG("Local:" + uneval(propagations[0]));
      LOG("To server:" + uneval(propagations[1]));

      LOG("Local snapshot version: " + this._snapshotVersion);
      LOG("Latest server version: " + server['version']);
      this._snapshotVersion = server['version'];

      if (!(propagations[0].length || propagations[1].length)) {
        this._snapshot = this._wrapNode(localBookmarks);
        LOG("Sync complete: no changes needed on client or server");
        return;
      }

      // 3.1) Apply server changes to local store
      if (propagations[0].length) {
        LOG("Applying changes locally");
        localBookmarks = this._getLocalBookmarks(); // fixme: wtf
        this._snapshot = this._wrapNode(localBookmarks);
        // applyCommands changes the imput commands, so we eval(uneval()) them to make a copy :-/
        this._sync.applyCommands(this._snapshot, eval(uneval(propagations[0])));
        this._applyCommands(localBookmarks, propagations[0]);
        this._snapshot = this._wrapNode(localBookmarks);
      }

      // 3.2) Append server delta to the delta file and upload
      if (propagations[1].length) {
        LOG("Uploading changes to server");
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
        for (var i = 0; i < keys.length; i++) {
          this._sync.applyCommands(tmp, this._sanitizeCommands(ret.deltas[keys[i]]));
        }
        ret.status = 1;
        ret.updates = this._sync.detectUpdates(this._snapshot, tmp);

      } else if (ret.deltas[this._snapshotVersion]) {
        LOG("No changes from server");
        ret.status = 0;
        ret.version = this._snapshotVersion;
        ret.updates = [];

      } else {
        LOG("Server delta can't update from our snapshot version, getting full file");
        // generate updates from full local->remote snapshot diff
        asyncRun(bind2(this, this._getServerUpdatesFull), handlers['complete'], localJson);
        data = yield;
        if (data.status == 2) {
          // we have a delta file but no snapshot on the server.  bad.
          // fixme?
          LOG("Error: Delta file on server, but snapshot file missing.  New snapshot uploaded, may be inconsistent with deltas!");
        }

        var tmp = eval(uneval(this._snapshot)); // fixme hack hack hack
        this._sync.applyCommands(tmp, this._sanitizeCommands(data.updates));

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
          this._sync.applyCommands(tmp, this._sanitizeCommands(ret.deltas[keys[i]]));
        }

        ret.status = data.status;
        ret.updates = this._sync.detectUpdates(this._snapshot, tmp);
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
      ret.updates = this._sync.detectUpdates(this._snapshot, tmp.snapshot);
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
    var h = {load: bind2(this, function(event) { handleEvent(generator, event); }),
             error: bind2(this, function(event) { LOG("Request failed: " + uneval(event)); })};
    h['complete'] = h['load'];
    return h;
  },

  // Interfaces this component implements.
  interfaces: [Ci.nsIBookmarksSyncService, Ci.nsISupports],

  // nsISupports

  // XPCOM registration
  classDescription: "Bookmarks Sync Service",
  contractID: "@mozilla.org/places/sync-service;1",
  classID: Components.ID("{6efd73bf-6a5a-404f-9246-f70a1286a3d6}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBookmarksSyncService, Ci.nsISupports]),

  // nsIBookmarksSyncService

  sync: function BSS_sync() { asyncRun(bind2(this, this._doSync)); }
};

function asyncRun(func, handler, data) {
  var generator = func(handler, data);
  generator.next();
  generator.send(generator);
}

function handleEvent(generator, data) {
  try { generator.send(data); }
  catch (e) {
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
  handleEvent: function EL_handleEvent(event) {
    this._handler(event);
  }
};

function DAVCollection(baseUrl) {
  this._baseUrl = baseUrl;
}
DAVCollection.prototype = {
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
    request.open(op, this._baseUrl + path, true);
  
    if (headers) {
      for (var key in headers) {
        request.setRequestHeader(key, headers[key]);
      }
    }

    return request;
  },
  GET: function DC_GET(path, handlers, headers) {
    if (!headers)
      headers = {'Content-type': 'text/plain'};
    var request = this._makeRequest("GET", path, handlers, headers);
    request.send(null);
  },
  PUT: function DC_PUT(path, data, handlers, headers) {
    if (!headers)
      headers = {'Content-type': 'text/plain'};
    var request = this._makeRequest("PUT", path, handlers, headers);
    request.send(data);
  },
  _runLockHandler: function DC__runLockHandler(name, event) {
    if (this._lockHandlers && this._lockHandlers[name])
      this._lockHandlers[name](event);
  },
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
  _onLock: function DC__onLock(event) {
    LOG("acquired lock (" + event.target.status + "):\n" + event.target.responseText + "\n");
    this._token = "woo";
    this._runLockHandler("load", event);
  },
  _onLockError: function DC__onLockError(event) {
    LOG("lock failed (" + event.target.status + "):\n" + event.target.responseText + "\n");
    this._runLockHandler("error", event);
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
  _onUnlock: function DC__onUnlock(event) {
    LOG("removed lock (" + event.target.status + "):\n" + event.target.responseText + "\n");
    this._token = null;
    this._runLockHandler("load", event);
  },
  _onUnlockError: function DC__onUnlockError(event) {
    LOG("unlock failed (" + event.target.status + "):\n" + event.target.responseText + "\n");
    this._runLockHandler("error", event);
  },
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

function LOG(aText) {
  dump(aText + "\n");
  var consoleService = Cc["@mozilla.org/consoleservice;1"].
                       getService(Ci.nsIConsoleService);
  consoleService.logStringMessage(aText);
}

function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule([BookmarksSyncService]);
}
