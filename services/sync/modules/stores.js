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

const EXPORTED_SYMBOLS = ['Store', 'SnapshotStore', 'BookmarksStore',
			  'HistoryStore', 'CookieStore', 'PasswordStore', 'FormStore',
			  'TabStore'];

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
 * Data Stores
 * These can wrap, serialize items and apply commands
 */

function Store() {
  this._init();
}
Store.prototype = {
  _logName: "Store",
  _yieldDuringApply: true,

  __json: null,
  get _json() {
    if (!this.__json)
      this.__json = Cc["@mozilla.org/dom/json;1"].
        createInstance(Ci.nsIJSON);
    return this.__json;
  },

  _init: function Store__init() {
    this._log = Log4Moz.Service.getLogger("Service." + this._logName);
  },

  applyCommands: function Store_applyCommands(commandList) {
    let self = yield;
    let timer, listener;

    if (this._yieldDuringApply) {
      listener = new Utils.EventListener(self.cb);
      timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    }

    for (var i = 0; i < commandList.length; i++) {
      if (this._yieldDuringApply) {
        timer.initWithCallback(listener, 0, timer.TYPE_ONE_SHOT);
        yield; // Yield to main loop
      }
      var command = commandList[i];
      this._log.trace("Processing command: " + this._json.encode(command));
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
        this._log.error("unknown action in command: " + command["action"]);
        break;
      }
    }
    self.done();
  },

  // override these in derived objects
  wrap: function Store_wrap() {},
  wipe: function Store_wipe() {},
  resetGUIDs: function Store_resetGUIDs() {}
};

function SnapshotStore(name) {
  this._init(name);
}
SnapshotStore.prototype = {
  _logName: "SnapStore",

  _filename: null,
  get filename() {
    if (this._filename === null)
      throw "filename is null";
    return this._filename;
  },
  set filename(value) {
    this._filename = value + ".json";
  },

  __dirSvc: null,
  get _dirSvc() {
    if (!this.__dirSvc)
      this.__dirSvc = Cc["@mozilla.org/file/directory_service;1"].
        getService(Ci.nsIProperties);
    return this.__dirSvc;
  },

  // Last synced tree, version, and GUID (to detect if the store has
  // been completely replaced and invalidate the snapshot)

  _data: {},
  get data() { return this._data; },
  set data(value) { this._data = value; },

  _version: 0,
  get version() { return this._version; },
  set version(value) { this._version = value; },

  _GUID: null,
  get GUID() {
    if (!this._GUID) {
      let uuidgen = Cc["@mozilla.org/uuid-generator;1"].
        getService(Ci.nsIUUIDGenerator);
      this._GUID = uuidgen.generateUUID().toString().replace(/[{}]/g, '');
    }
    return this._GUID;
  },
  set GUID(GUID) {
    this._GUID = GUID;
  },

  _init: function SStore__init(name) {
    this.filename = name;
    this._log = Log4Moz.Service.getLogger("Service." + this._logName);
  },

  _createCommand: function SStore__createCommand(command) {
    this._data[command.GUID] = Utils.deepCopy(command.data);
  },

  _removeCommand: function SStore__removeCommand(command) {
    delete this._data[command.GUID];
  },

  _editCommand: function SStore__editCommand(command) {
    if ("GUID" in command.data) {
      // special-case guid changes
      let newGUID = command.data.GUID,
      oldGUID = command.GUID;

      this._data[newGUID] = this._data[oldGUID];
      delete this._data[oldGUID]

      for (let GUID in this._data) {
        if (this._data[GUID].parentGUID == oldGUID)
          this._data[GUID].parentGUID = newGUID;
      }
    }
    for (let prop in command.data) {
      if (prop == "GUID")
        continue;
      this._data[command.GUID][prop] = command.data[prop];
    }
  },

  save: function SStore_save() {
    this._log.info("Saving snapshot to disk");

    let file = this._dirSvc.get("ProfD", Ci.nsIFile);
    file.QueryInterface(Ci.nsILocalFile);

    file.append("weave");
    file.append("snapshots");
    file.append(this.filename);
    if (!file.exists())
      file.create(file.NORMAL_FILE_TYPE, PERMS_FILE);

    let out = {version: this.version,
               GUID: this.GUID,
               snapshot: this.data};
    out = this._json.encode(out);

    let [fos] = Utils.open(file, ">");
    fos.writeString(out);
    fos.close();
  },

  load: function SStore_load() {
    let file = this._dirSvc.get("ProfD", Ci.nsIFile);
    file.append("weave");
    file.append("snapshots");
    file.append(this.filename);

    if (!file.exists())
      return;

    try {
      let [is] = Utils.open(file, "<");
      let json = Utils.readStream(is);
      is.close();
      json = this._json.decode(json);

      if (json && 'snapshot' in json && 'version' in json && 'GUID' in json) {
        this._log.info("Read saved snapshot from disk");
        this.data = json.snapshot;
        this.version = json.version;
        this.GUID = json.GUID;
      }
    } catch (e) {
      this._log.warn("Could not parse saved snapshot; reverting to initial-sync state");
      this._log.debug("Exception: " + e);
    }
  },

  serialize: function SStore_serialize() {
    let json = this._json.encode(this.data);
    json = json.replace(/:{type/g, ":\n\t{type");
    json = json.replace(/}, /g, "},\n  ");
    json = json.replace(/, parentGUID/g, ",\n\t parentGUID");
    json = json.replace(/, index/g, ",\n\t index");
    json = json.replace(/, title/g, ",\n\t title");
    json = json.replace(/, URI/g, ",\n\t URI");
    json = json.replace(/, tags/g, ",\n\t tags");
    json = json.replace(/, keyword/g, ",\n\t keyword");
    return json;
  },

  wrap: function SStore_wrap() {
    return this.data;
  },

  wipe: function SStore_wipe() {
    this.data = {};
    this.version = -1;
    this.GUID = null;
    this.save();
  }
};
SnapshotStore.prototype.__proto__ = new Store();

function BookmarksStore() {
  this._init();
}
BookmarksStore.prototype = {
  _logName: "BStore",

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

  __ls: null,
  get _ls() {
    if (!this.__ls)
      this.__ls = Cc["@mozilla.org/browser/livemark-service;2"].
        getService(Ci.nsILivemarkService);
    return this.__ls;
  },

  __ms: null,
  get _ms() {
    if (!this.__ms)
      this.__ms = Cc["@mozilla.org/microsummary/service;1"].
        getService(Ci.nsIMicrosummaryService);
    return this.__ms;
  },

  __ts: null,
  get _ts() {
    if (!this.__ts)
      this.__ts = Cc["@mozilla.org/browser/tagging-service;1"].
                  getService(Ci.nsITaggingService);
    return this.__ts;
  },

  __ans: null,
  get _ans() {
    if (!this.__ans)
      this.__ans = Cc["@mozilla.org/browser/annotation-service;1"].
                   getService(Ci.nsIAnnotationService);
    return this.__ans;
  },

  _getItemIdForGUID: function BStore__getItemIdForGUID(GUID) {
    switch (GUID) {
    case "menu":
      return this._bms.bookmarksMenuFolder;
    case "toolbar":
      return this._bms.toolbarFolder;
    case "unfiled":
      return this._bms.unfiledBookmarksFolder;
    default:
      return this._bms.getItemIdForGUID(GUID);
    }
    return null;
  },

  _createCommand: function BStore__createCommand(command) {
    let newId;
    let parentId = this._getItemIdForGUID(command.data.parentGUID);

    if (parentId < 0) {
      this._log.warn("Creating node with unknown parent -> reparenting to root");
      parentId = this._bms.bookmarksMenuFolder;
    }

    switch (command.data.type) {
    case "query":
    case "bookmark":
    case "microsummary": {
      this._log.debug(" -> creating bookmark \"" + command.data.title + "\"");
      let URI = Utils.makeURI(command.data.URI);
      newId = this._bms.insertBookmark(parentId,
                                       URI,
                                       command.data.index,
                                       command.data.title);
      this._ts.untagURI(URI, null);
      this._ts.tagURI(URI, command.data.tags);
      this._bms.setKeywordForBookmark(newId, command.data.keyword);
      if (command.data.description) {
        this._ans.setItemAnnotation(newId, "bookmarkProperties/description",
                                    command.data.description, 0,
                                    this._ans.EXPIRE_NEVER);
      }

      if (command.data.type == "microsummary") {
        this._log.debug("   \-> is a microsummary");
        this._ans.setItemAnnotation(newId, "bookmarks/staticTitle",
                                    command.data.staticTitle || "", 0, this._ans.EXPIRE_NEVER);
        let genURI = Utils.makeURI(command.data.generatorURI);
        try {
          let micsum = this._ms.createMicrosummary(URI, genURI);
          this._ms.setMicrosummary(newId, micsum);
        }
        catch(ex) { /* ignore "missing local generator" exceptions */ }
      }
    } break;
    case "folder":
      this._log.debug(" -> creating folder \"" + command.data.title + "\"");
      newId = this._bms.createFolder(parentId,
                                     command.data.title,
                                     command.data.index);
      break;
    case "livemark":
      this._log.debug(" -> creating livemark \"" + command.data.title + "\"");
      newId = this._ls.createLivemark(parentId,
                                      command.data.title,
                                      Utils.makeURI(command.data.siteURI),
                                      Utils.makeURI(command.data.feedURI),
                                      command.data.index);
      break;
    case "mounted-share":
      this._log.debug(" -> creating share mountpoint \"" + command.data.title + "\"");
      newId = this._bms.createFolder(parentId,
                                     command.data.title,
                                     command.data.index);

      this._ans.setItemAnnotation(newId, "weave/mounted-share-id",
                                  command.data.mountId, 0, this._ans.EXPIRE_NEVER);
      break;
    case "separator":
      this._log.debug(" -> creating separator");
      newId = this._bms.insertSeparator(parentId, command.data.index);
      break;
    default:
      this._log.error("_createCommand: Unknown item type: " + command.data.type);
      break;
    }
    if (newId)
      this._bms.setItemGUID(newId, command.GUID);
  },

  _removeCommand: function BStore__removeCommand(command) {
    if (command.GUID == "menu" ||
        command.GUID == "toolbar" ||
        command.GUID == "unfiled") {
      this._log.warn("Attempted to remove root node (" + command.GUID +
                     ").  Skipping command.");
      return;
    }

    var itemId = this._bms.getItemIdForGUID(command.GUID);
    if (itemId < 0) {
      this._log.warn("Attempted to remove item " + command.GUID +
                     ", but it does not exist.  Skipping.");
      return;
    }
    var type = this._bms.getItemType(itemId);

    switch (type) {
    case this._bms.TYPE_BOOKMARK:
      this._log.debug("  -> removing bookmark " + command.GUID);
      this._bms.removeItem(itemId);
      break;
    case this._bms.TYPE_FOLDER:
      this._log.debug("  -> removing folder " + command.GUID);
      this._bms.removeFolder(itemId);
      break;
    case this._bms.TYPE_SEPARATOR:
      this._log.debug("  -> removing separator " + command.GUID);
      this._bms.removeItem(itemId);
      break;
    default:
      this._log.error("removeCommand: Unknown item type: " + type);
      break;
    }
  },

  _editCommand: function BStore__editCommand(command) {
    if (command.GUID == "menu" ||
        command.GUID == "toolbar" ||
        command.GUID == "unfiled") {
      this._log.warn("Attempted to edit root node (" + command.GUID +
                     ").  Skipping command.");
      return;
    }

    var itemId = this._bms.getItemIdForGUID(command.GUID);
    if (itemId < 0) {
      this._log.warn("Item for GUID " + command.GUID + " not found.  Skipping.");
      return;
    }

    for (let key in command.data) {
      switch (key) {
      case "type":
        // all commands have this to help in reconciliation, but it makes
        // no sense to edit it
        break;
      case "GUID":
        var existing = this._getItemIdForGUID(command.data.GUID);
        if (existing < 0)
          this._bms.setItemGUID(itemId, command.data.GUID);
        else
          this._log.warn("Can't change GUID " + command.GUID +
                         " to " + command.data.GUID + ": GUID already exists.");
        break;
      case "title":
        this._bms.setItemTitle(itemId, command.data.title);
        break;
      case "URI":
        this._bms.changeBookmarkURI(itemId, Utils.makeURI(command.data.URI));
        break;
      case "index":
        this._bms.moveItem(itemId, this._bms.getFolderIdForItem(itemId),
                           command.data.index);
        break;
      case "parentGUID": {
        let index = -1;
        if (command.data.index && command.data.index >= 0)
          index = command.data.index;
        this._bms.moveItem(
          itemId, this._getItemIdForGUID(command.data.parentGUID), index);
      } break;
      case "tags": {
        let tagsURI = this._bms.getBookmarkURI(itemId);
        this._ts.untagURI(tagsURI, null);
        this._ts.tagURI(tagsURI, command.data.tags);
      } break;
      case "keyword":
        this._bms.setKeywordForBookmark(itemId, command.data.keyword);
        break;
      case "description":
        if (command.data.description) {
          this._ans.setItemAnnotation(itemId, "bookmarkProperties/description",
                                      command.data.description, 0,
                                      this._ans.EXPIRE_NEVER);
        }
        break;
      case "generatorURI": {
        let micsumURI = Utils.makeURI(this._bms.getBookmarkURI(itemId));
        let genURI = Utils.makeURI(command.data.generatorURI);
        let micsum = this._ms.createMicrosummary(micsumURI, genURI);
        this._ms.setMicrosummary(itemId, micsum);
      } break;
      case "siteURI":
        this._ls.setSiteURI(itemId, Utils.makeURI(command.data.siteURI));
        break;
      case "feedURI":
        this._ls.setFeedURI(itemId, Utils.makeURI(command.data.feedURI));
        break;
      default:
        this._log.warn("Can't change item property: " + key);
        break;
      }
    }
  },

  _getNode: function BSS__getNode(folder) {
    let query = this._hsvc.getNewQuery();
    query.setFolders([folder], 1);
    return this._hsvc.executeQuery(query, this._hsvc.getNewQueryOptions()).root;
  },

  __wrap: function BSS___wrap(node, items, parentGUID, index, guidOverride) {
    let GUID, item;

    // we override the guid for the root items, "menu", "toolbar", etc.
    if (guidOverride) {
      GUID = guidOverride;
      item = {};
    } else {
      GUID = this._bms.getItemGUID(node.itemId);
      item = {parentGUID: parentGUID, index: index};
    }

    if (node.type == node.RESULT_TYPE_FOLDER) {
      if (this._ls.isLivemark(node.itemId)) {
        item.type = "livemark";
        let siteURI = this._ls.getSiteURI(node.itemId);
        let feedURI = this._ls.getFeedURI(node.itemId);
        item.siteURI = siteURI? siteURI.spec : "";
        item.feedURI = feedURI? feedURI.spec : "";

      } else if (this._ans.itemHasAnnotation(node.itemId,
                                             "weave/mounted-share-id")) {
        item.type = "mounted-share";
        item.title = node.title;
        item.mountId = this._ans.getItemAnnotation(node.itemId,
                                                   "weave/mounted-share-id");

      } else {
        item.type = "folder";
        node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
        node.containerOpen = true;
        for (var i = 0; i < node.childCount; i++) {
          this.__wrap(node.getChild(i), items, GUID, i);
        }
      }
      if (!guidOverride)
        item.title = node.title; // no titles for root nodes

    } else if (node.type == node.RESULT_TYPE_URI ||
               node.type == node.RESULT_TYPE_QUERY) {
      if (this._ms.hasMicrosummary(node.itemId)) {
        item.type = "microsummary";
        let micsum = this._ms.getMicrosummary(node.itemId);
        item.generatorURI = micsum.generator.uri.spec; // breaks local generators
        item.staticTitle = this._ans.getItemAnnotation(node.itemId, "bookmarks/staticTitle");
      } else if (node.type == node.RESULT_TYPE_QUERY) {
        item.type = "query";
        item.title = node.title;
      } else {
        item.type = "bookmark";
        item.title = node.title;
      }

      try {
        item.description =
          this._ans.getItemAnnotation(node.itemId, "bookmarkProperties/description");
      } catch (e) {
        item.description = undefined;
      }

      item.URI = node.uri;
      item.tags = this._ts.getTagsForURI(Utils.makeURI(node.uri), {});
      item.keyword = this._bms.getKeywordForBookmark(node.itemId);

    } else if (node.type == node.RESULT_TYPE_SEPARATOR) {
      item.type = "separator";

    } else {
      this._log.warn("Warning: unknown item type, cannot serialize: " + node.type);
      return;
    }

    items[GUID] = item;
  },

  // helper
  _wrap: function BStore__wrap(node, items, rootName) {
    return this.__wrap(node, items, null, null, rootName);
  },

  _wrapMount: function BStore__wrapMount(node, id) {
    if (node.type != node.RESULT_TYPE_FOLDER)
      throw "Trying to wrap a non-folder mounted share";

    let GUID = this._bms.getItemGUID(node.itemId);
    let ret = {rootGUID: GUID, userid: id, snapshot: {}};

    node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
    node.containerOpen = true;
    for (var i = 0; i < node.childCount; i++) {
      this.__wrap(node.getChild(i), ret.snapshot, GUID, i);
    }

    // remove any share mountpoints
    for (let guid in ret.snapshot) {
      if (ret.snapshot[guid].type == "mounted-share")
        delete ret.snapshot[guid];
    }

    return ret;
  },

  _resetGUIDs: function BSS__resetGUIDs(node) {
    if (this._ans.itemHasAnnotation(node.itemId, "placesInternal/GUID"))
      this._ans.removeItemAnnotation(node.itemId, "placesInternal/GUID");

    if (node.type == node.RESULT_TYPE_FOLDER &&
        !this._ls.isLivemark(node.itemId)) {
      node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      node.containerOpen = true;
      for (var i = 0; i < node.childCount; i++) {
        this._resetGUIDs(node.getChild(i));
      }
    }
  },

  findMounts: function BStore_findMounts() {
    let ret = [];
    let a = this._ans.getItemsWithAnnotation("weave/mounted-share-id", {});
    for (let i = 0; i < a.length; i++) {
      let id = this._ans.getItemAnnotation(a[i], "weave/mounted-share-id");
      ret.push(this._wrapMount(this._getNode(a[i]), id));
    }
    return ret;
  },

  wrap: function BStore_wrap() {
    var items = {};
    this._wrap(this._getNode(this._bms.bookmarksMenuFolder), items, "menu");
    this._wrap(this._getNode(this._bms.toolbarFolder), items, "toolbar");
    this._wrap(this._getNode(this._bms.unfiledBookmarksFolder), items, "unfiled");
    return items;
  },

  wipe: function BStore_wipe() {
    this._bms.removeFolderChildren(this._bms.bookmarksMenuFolder);
    this._bms.removeFolderChildren(this._bms.toolbarFolder);
    this._bms.removeFolderChildren(this._bms.unfiledBookmarksFolder);
  },

  resetGUIDs: function BStore_resetGUIDs() {
    this._resetGUIDs(this._getNode(this._bms.bookmarksMenuFolder));
    this._resetGUIDs(this._getNode(this._bms.toolbarFolder));
    this._resetGUIDs(this._getNode(this._bms.unfiledBookmarksFolder));
  }
};
BookmarksStore.prototype.__proto__ = new Store();

function HistoryStore() {
  this._init();
}
HistoryStore.prototype = {
  _logName: "HistStore",

  __hsvc: null,
  get _hsvc() {
    if (!this.__hsvc) {
      this.__hsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                    getService(Ci.nsINavHistoryService);
      this.__hsvc.QueryInterface(Ci.nsIGlobalHistory2);
      this.__hsvc.QueryInterface(Ci.nsIBrowserHistory);
    }
    return this.__hsvc;
  },

  _createCommand: function HistStore__createCommand(command) {
    this._log.debug("  -> creating history entry: " + command.GUID);
    try {
      let uri = Utils.makeURI(command.data.URI);
      this._hsvc.addVisit(uri, command.data.time, null,
                          this._hsvc.TRANSITION_TYPED, false, null);
      this._hsvc.setPageTitle(uri, command.data.title);
    } catch (e) {
      this._log.error("Exception caught: " + (e.message? e.message : e));
    }
  },

  _removeCommand: function HistStore__removeCommand(command) {
    this._log.trace("  -> NOT removing history entry: " + command.GUID);
    // we can't remove because we only sync the last 1000 items, not
    // the whole store.  So we don't know if remove commands were
    // generated due to the user removing an entry or because it
    // dropped past the 1000 item mark.
  },

  _editCommand: function HistStore__editCommand(command) {
    this._log.trace("  -> FIXME: NOT editing history entry: " + command.GUID);
    // FIXME: implement!
  },

  _historyRoot: function HistStore__historyRoot() {
    let query = this._hsvc.getNewQuery(),
        options = this._hsvc.getNewQueryOptions();

    query.minVisits = 1;
    options.maxResults = 1000;
    options.resultType = options.RESULTS_AS_VISIT; // FULL_VISIT does not work
    options.sortingMode = options.SORT_BY_DATE_DESCENDING;
    options.queryType = options.QUERY_TYPE_HISTORY;

    let root = this._hsvc.executeQuery(query, options).root;
    root.QueryInterface(Ci.nsINavHistoryQueryResultNode);
    return root;
  },

  wrap: function HistStore_wrap() {
    let root = this._historyRoot();
    root.containerOpen = true;
    let items = {};
    for (let i = 0; i < root.childCount; i++) {
      let item = root.getChild(i);
      let guid = item.time + ":" + item.uri
      items[guid] = {parentGUID: '',
			 title: item.title,
			 URI: item.uri,
			 time: item.time
			};
      // FIXME: sync transition type - requires FULL_VISITs
    }
    return items;
  },

  wipe: function HistStore_wipe() {
    this._hsvc.removeAllPages();
  }
};
HistoryStore.prototype.__proto__ = new Store();


function CookieStore( cookieManagerStub ) {
  /* If no argument is passed in, this store will query/write to the real
     Mozilla cookie manager component.  This is the normal way to use this
     class in production code.  But for unit-testing purposes, you can pass
     in a stub object that will be used in place of the cookieManager. */
  this._init();
  this._cookieManagerStub = cookieManagerStub;
}
CookieStore.prototype = {
  _logName: "CookieStore",


  // Documentation of the nsICookie interface says:
  // name 	ACString 	The name of the cookie. Read only.
  // value 	ACString 	The cookie value. Read only.
  // isDomain 	boolean 	True if the cookie is a domain cookie, false otherwise. Read only.
  // host 	AUTF8String 	The host (possibly fully qualified) of the cookie. Read only.
  // path 	AUTF8String 	The path pertaining to the cookie. Read only.
  // isSecure 	boolean 	True if the cookie was transmitted over ssl, false otherwise. Read only.
  // expires 	PRUint64 	Expiration time (local timezone) expressed as number of seconds since Jan 1, 1970. Read only.
  // status 	nsCookieStatus 	Holds the P3P status of cookie. Read only.
  // policy 	nsCookiePolicy 	Holds the site's compact policy value. Read only.
  // nsICookie2 deprecates expires, status, and policy, and adds:
  //rawHost 	AUTF8String 	The host (possibly fully qualified) of the cookie without a leading dot to represent if it is a domain cookie. Read only.
  //isSession 	boolean 	True if the cookie is a session cookie. Read only.
  //expiry 	PRInt64 	the actual expiry time of the cookie (where 0 does not represent a session cookie). Read only.
  //isHttpOnly 	boolean 	True if the cookie is an http only cookie. Read only.

  __cookieManager: null,
  get _cookieManager() {
    if ( this._cookieManagerStub != undefined ) {
      return this._cookieManagerStub;
    }
    // otherwise, use the real one
    if (!this.__cookieManager)
      this.__cookieManager = Cc["@mozilla.org/cookiemanager;1"].
                             getService(Ci.nsICookieManager2);
    // need the 2nd revision of the ICookieManager interface
    // because it supports add() and the 1st one doesn't.
    return this.__cookieManager
  },

  _createCommand: function CookieStore__createCommand(command) {
    /* we got a command to create a cookie in the local browser
       in order to sync with the server. */

    this._log.info("CookieStore got createCommand: " + command );
    // this assumes command.data fits the nsICookie2 interface
    if ( !command.data.isSession ) {
      // Add only persistent cookies ( not session cookies )
      this._cookieManager.add( command.data.host,
			       command.data.path,
			       command.data.name,
			       command.data.value,
			       command.data.isSecure,
			       command.data.isHttpOnly,
			       command.data.isSession,
			       command.data.expiry );
    }
  },

  _removeCommand: function CookieStore__removeCommand(command) {
    /* we got a command to remove a cookie from the local browser
       in order to sync with the server.
       command.data appears to be equivalent to what wrap() puts in
       the JSON dictionary. */

    this._log.info("CookieStore got removeCommand: " + command );

    /* I think it goes like this, according to
      http://developer.mozilla.org/en/docs/nsICookieManager
      the last argument is "always block cookies from this domain?"
      and the answer is "no". */
    this._cookieManager.remove( command.data.host,
				command.data.name,
				command.data.path,
				false );
  },

  _editCommand: function CookieStore__editCommand(command) {
    /* we got a command to change a cookie in the local browser
       in order to sync with the server. */
    this._log.info("CookieStore got editCommand: " + command );

    /* Look up the cookie that matches the one in the command: */
    var iter = this._cookieManager.enumerator;
    var matchingCookie = null;
    while (iter.hasMoreElements()){
      let cookie = iter.getNext();
      if (cookie.QueryInterface( Ci.nsICookie ) ){
        // see if host:path:name of cookie matches GUID given in command
	let key = cookie.host + ":" + cookie.path + ":" + cookie.name;
	if (key == command.GUID) {
	  matchingCookie = cookie;
	  break;
	}
      }
    }
    // Update values in the cookie:
    for (var key in command.data) {
      // Whatever values command.data has, use them
      matchingCookie[ key ] = command.data[ key ]
    }
    // Remove the old incorrect cookie from the manager:
    this._cookieManager.remove( matchingCookie.host,
				matchingCookie.name,
				matchingCookie.path,
				false );

    // Re-add the new updated cookie:
    if ( !command.data.isSession ) {
      /* ignore single-session cookies, add only persistent cookies.  */
      this._cookieManager.add( matchingCookie.host,
			       matchingCookie.path,
			       matchingCookie.name,
			       matchingCookie.value,
			       matchingCookie.isSecure,
			       matchingCookie.isHttpOnly,
			       matchingCookie.isSession,
			       matchingCookie.expiry );
    }

    // Also, there's an exception raised because
    // this._data[comand.GUID] is undefined
  },

  wrap: function CookieStore_wrap() {
    /* Return contents of this store, as JSON.
       A dictionary of cookies where the keys are GUIDs and the
       values are sub-dictionaries containing all cookie fields. */

    let items = {};
    var iter = this._cookieManager.enumerator;
    while (iter.hasMoreElements()){
      var cookie = iter.getNext();
      if (cookie.QueryInterface( Ci.nsICookie )){
	// String used to identify cookies is
	// host:path:name
	if ( cookie.isSession ) {
	  /* Skip session-only cookies, sync only persistent cookies. */
	  continue;
	}

	let key = cookie.host + ":" + cookie.path + ":" + cookie.name;
	items[ key ] = { parentGUID: '',
			 name: cookie.name,
			 value: cookie.value,
			 isDomain: cookie.isDomain,
			 host: cookie.host,
			 path: cookie.path,
			 isSecure: cookie.isSecure,
			 // nsICookie2 values:
			 rawHost: cookie.rawHost,
			 isSession: cookie.isSession,
			 expiry: cookie.expiry,
			 isHttpOnly: cookie.isHttpOnly }

	/* See http://developer.mozilla.org/en/docs/nsICookie
	   Note: not syncing "expires", "status", or "policy"
	   since they're deprecated. */

      }
    }
    return items;
  },

  wipe: function CookieStore_wipe() {
    /* Remove everything from the store.  Return nothing.
       TODO are the semantics of this just wiping out an internal
       buffer, or am I supposed to wipe out all cookies from
       the browser itself for reals? */
    this._cookieManager.removeAll()
  },

  resetGUIDs: function CookieStore_resetGUIDs() {
    /* called in the case where remote/local sync GUIDs do not
       match.  We do need to override this, but since we're deriving
       GUIDs from the cookie data itself and not generating them,
       there's basically no way they can get "out of sync" so there's
       nothing to do here. */
  }
};
CookieStore.prototype.__proto__ = new Store();

function PasswordStore() {
  this._init();
}
PasswordStore.prototype = {
  _logName: "PasswordStore",

  __loginManager : null,
  get _loginManager() {
    if (!this.__loginManager)
      this.__loginManager = Cc["@mozilla.org/login-manager;1"].
                            getService(Ci.nsILoginManager);
    return this.__loginManager;
  },

  __nsLoginInfo : null,
  get _nsLoginInfo() {
    if (!this.__nsLoginInfo)
      this.__nsLoginInfo = new Components.Constructor(
            "@mozilla.org/login-manager/loginInfo;1",
            Ci.nsILoginInfo, "init");
    return this.__nsLoginInfo;
  },


  _createCommand: function PasswordStore__createCommand(command) {
    this._log.info("PasswordStore got createCommand: " + command );

    var login = new this._nsLoginInfo(command.data.hostname,
                                      command.data.formSubmitURL,
                                      command.data.httpRealm,
                                      command.data.username,
                                      command.data.password,
                                      command.data.usernameField,
                                      command.data.passwordField);

    this._loginManager.addLogin(login);
  },

  _removeCommand: function PasswordStore__removeCommand(command) {
    this._log.info("PasswordStore got removeCommand: " + command );

    var login = new this._nsLoginInfo(command.data.hostname,
                                      command.data.formSubmitURL,
                                      command.data.httpRealm,
                                      command.data.username,
                                      command.data.password,
                                      command.data.usernameField,
                                      command.data.passwordField);

    this._loginManager.removeLogin(login);
  },

  _editCommand: function PasswordStore__editCommand(command) {
    this._log.info("PasswordStore got editCommand: " + command );
    throw "Password syncs are expected to only be create/remove!";
  },

  wrap: function PasswordStore_wrap() {
    /* Return contents of this store, as JSON. */
    var items = {};

    var logins = this._loginManager.getAllLogins({});

    for (var i = 0; i < logins.length; i++) {
      var login = logins[i];

      var key = this._hashLoginInfo(login);

      items[key] = { hostname      : login.hostname,
                     formSubmitURL : login.formSubmitURL,
                     httpRealm     : login.httpRealm,
                     username      : login.username,
                     password      : login.password,
                     usernameField : login.usernameField,
                     passwordField : login.passwordField };
    }

    return items;
  },

  wipe: function PasswordStore_wipe() {
    this._loginManager.removeAllLogins();
  },

  resetGUIDs: function PasswordStore_resetGUIDs() {
    // Not needed.
  }
};
PasswordStore.prototype.__proto__ = new Store();

function FormStore() {
  this._init();
}
FormStore.prototype = {
  _logName: "FormStore",

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

  __formHistory: null,
  get _formHistory() {
    if (!this.__formHistory)
      this.__formHistory = Cc["@mozilla.org/satchel/form-history;1"].
                           getService(Ci.nsIFormHistory2);
    return this.__formHistory;
  },

  _createCommand: function FormStore__createCommand(command) {
    this._log.info("FormStore got createCommand: " + command );
    this._formHistory.addEntry(command.data.name, command.data.value);
  },

  _removeCommand: function FormStore__removeCommand(command) {
    this._log.info("FormStore got removeCommand: " + command );
    this._formHistory.removeEntry(command.data.name, command.data.value);
  },

  _editCommand: function FormStore__editCommand(command) {
    this._log.info("FormStore got editCommand: " + command );
    this._log.warn("Form syncs are expected to only be create/remove!");
  },

  wrap: function FormStore_wrap() {
    var items = [];
    var stmnt = this._formDB.createStatement("SELECT * FROM moz_formhistory");

    while (stmnt.executeStep()) {
      var nam = stmnt.getUTF8String(1);
      var val = stmnt.getUTF8String(2);
      var key = Utils.sha1(nam + val);

      items[key] = { name: nam, value: val };
    }

    return items;
  },

  wipe: function FormStore_wipe() {
    this._formHistory.removeAllEntries();
  },

  resetGUIDs: function FormStore_resetGUIDs() {
    // Not needed.
  }
};
FormStore.prototype.__proto__ = new Store();

function TabStore() {
  this._virtualTabs = {};
  this._init();
}
TabStore.prototype = {
  __proto__: new Store(),

  _logName: "TabStore",

  get _sessionStore() {
    let sessionStore = Cc["@mozilla.org/browser/sessionstore;1"].
		       getService(Ci.nsISessionStore);
    this.__defineGetter__("_sessionStore", function() sessionStore);
    return this._sessionStore;
  },

  get _windowMediator() {
    let windowMediator = Cc["@mozilla.org/appshell/window-mediator;1"].
			 getService(Ci.nsIWindowMediator);
    this.__defineGetter__("_windowMediator", function() windowMediator);
    return this._windowMediator;
  },

  get _os() {
    let os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);
    this.__defineGetter__("_os", function() os);
    return this._os;
  },

  get _dirSvc() {
    let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                 getService(Ci.nsIProperties);
    this.__defineGetter__("_dirSvc", function() dirSvc);
    return this._dirSvc;
  },

  /**
   * A cache of "virtual" tabs from other devices synced to the server
   * that the user hasn't opened locally.  Unlike other stores, we don't
   * immediately apply create commands, which would be jarring to users.
   * Instead, we store them in this cache and prompt the user to pick
   * which ones she wants to open.
   *
   * We also persist this cache on disk and include it in the list of tabs
   * we generate in this.wrap to reduce ping-pong updates between clients
   * running simultaneously and to maintain a consistent state across restarts.
   */
  _virtualTabs: null,

  get virtualTabs() {
    // Make sure the list of virtual tabs is completely up-to-date (the user
    // might have independently opened some of these virtual tabs since the last
    // time we synced).
    let realTabs = this._wrapRealTabs();
    let virtualTabsChanged = false;
    for (let id in this._virtualTabs) {
      if (id in realTabs) {
        this._log.warn("get virtualTabs: both real and virtual tabs exist for "
                       + id + "; removing virtual one");
        delete this._virtualTabs[id];
        virtualTabsChanged = true;
      }
    }
    if (virtualTabsChanged)
      this._saveVirtualTabs();

    return this._virtualTabs;
  },

  set virtualTabs(newValue) {
    this._virtualTabs = newValue;
    this._saveVirtualTabs();
  },

  // The file in which we store the state of virtual tabs.
  get _file() {
    let file = this._dirSvc.get("ProfD", Ci.nsILocalFile);
    file.append("weave");
    file.append("store");
    file.append("tabs");
    file.append("virtual.json");
    this.__defineGetter__("_file", function() file);
    return this._file;
  },

  _saveVirtualTabs: function TabStore__saveVirtualTabs() {
    try {
      if (!this._file.exists())
        this._file.create(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
      let out = this._json.encode(this._virtualTabs);
      let [fos] = Utils.open(this._file, ">");
      fos.writeString(out);
      fos.close();
    }
    catch(ex) {
      this._log.warn("could not serialize virtual tabs to disk: " + ex);
    }
  },

  _restoreVirtualTabs: function TabStore__restoreVirtualTabs() {
    try {
      if (this._file.exists()) {
        let [is] = Utils.open(this._file, "<");
        let json = Utils.readStream(is);
        is.close();
        this._virtualTabs = this._json.decode(json);
      }
    }
    catch (ex) {
      this._log.warn("could not parse virtual tabs from disk: " + ex);
    }
  },

  _init: function TabStore__init() {
    this._restoreVirtualTabs();

    this.__proto__.__proto__._init();
  },

  /**
   * Apply commands generated by a diff during a sync operation.  This method
   * overrides the one in its superclass so it can save a copy of the latest set
   * of virtual tabs to disk so they can be restored on startup.
   */
  applyCommands: function TabStore_applyCommands(commandList) {
    let self = yield;

    this.__proto__.__proto__.applyCommands.async(this, self.cb, commandList);
    yield;

    this._saveVirtualTabs();

    self.done();
  },

  _createCommand: function TabStore__createCommand(command) {
    this._log.debug("_createCommand: " + command.GUID);

    if (command.GUID in this._virtualTabs || command.GUID in this._wrapRealTabs())
      throw "trying to create a tab that already exists; id: " + command.GUID;

    // Cache the tab and notify the UI to prompt the user to open it.
    this._virtualTabs[command.GUID] = command.data;
    this._os.notifyObservers(null, "weave:store:tabs:virtual:created", null);
  },

  _removeCommand: function TabStore__removeCommand(command) {
    this._log.debug("_removeCommand: " + command.GUID);

    // If this is a virtual tab, it's ok to remove it, since it was never really
    // added to this session in the first place.  But we don't remove it if it's
    // a real tab, since that would be unexpected, unpleasant, and unwanted.
    if (command.GUID in this._virtualTabs) {
      delete this._virtualTabs[command.GUID];
      this._os.notifyObservers(null, "weave:store:tabs:virtual:removed", null);
    }
  },

  _editCommand: function TabStore__editCommand(command) {
    this._log.debug("_editCommand: " + command.GUID);

    // We don't edit real tabs, because that isn't what the user would expect,
    // but it's ok to edit virtual tabs, so that if users do open them, they get
    // the most up-to-date version of them (and also to reduce sync churn).

    if (this._virtualTabs[command.GUID])
      this._virtualTabs[command.GUID] = command.data;
  },

  /**
   * Serialize the current state of tabs.
   *
   * Note: the state includes both tabs on this device and those on others.
   * We get the former from the session store.  The latter we retrieved from
   * the Weave server and stored in this._virtualTabs.  Including virtual tabs
   * in the serialized state prevents ping-pong deletes between two clients
   * running at the same time.
   */
  wrap: function TabStore_wrap() {
    let items;

    let virtualTabs = this._wrapVirtualTabs();
    let realTabs = this._wrapRealTabs();

    // Real tabs override virtual ones, which means ping-pong edits when two
    // clients have the same URL loaded with different history/attributes.
    // We could fix that by overriding real tabs with virtual ones, but then
    // we'd have stale tab metadata in same cases.
    items = virtualTabs;
    let virtualTabsChanged = false;
    for (let id in realTabs) {
      // Since virtual tabs can sometimes get out of sync with real tabs
      // (the user could have independently opened a new tab that exists
      // in the virtual tabs cache since the last time we updated the cache),
      // we sync them up in the process of merging them here.
      if (this._virtualTabs[id]) {
        this._log.warn("wrap: both real and virtual tabs exist for " + id +
                       "; removing virtual one");
        delete this._virtualTabs[id];
        virtualTabsChanged = true;
      }

      items[id] = realTabs[id];
    }
    if (virtualTabsChanged)
      this._saveVirtualTabs();

    return items;
  },

  _wrapVirtualTabs: function TabStore__wrapVirtualTabs() {
    let items = {};

    for (let id in this._virtualTabs) {
      let virtualTab = this._virtualTabs[id];

      // Copy the virtual tab without private properties (those that begin
      // with an underscore character) so that we don't sync data private to
      // this particular Weave client (like the _disposed flag).
      let item = {};
      for (let property in virtualTab)
        if (property[0] != "_")
          item[property] = virtualTab[property];

      items[id] = item;
    }

    return items;
  },

  _wrapRealTabs: function TabStore__wrapRealTabs() {
    let items = {};

    let session = this._json.decode(this._sessionStore.getBrowserState());

    for (let i = 0; i < session.windows.length; i++) {
      let window = session.windows[i];
      // For some reason, session store uses one-based array index references,
      // (f.e. in the "selectedWindow" and each tab's "index" properties), so we
      // convert them to and from JavaScript's zero-based indexes as needed.
      let windowID = i + 1;
      this._log.debug("_wrapRealTabs: window " + windowID);
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
        this._log.debug("_wrapRealTabs: tab " + tabID);

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
    // the user wants, but we'll clear the cache of virtual tabs.
    this._virtualTabs = {};
    this._saveVirtualTabs();
  },

  resetGUIDs: function TabStore_resetGUIDs() {
    // Not needed.
  }

};
