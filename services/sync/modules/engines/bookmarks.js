const EXPORTED_SYMBOLS = ['BookmarksEngine'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/dav.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/crypto.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/syncCores.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");

Function.prototype.async = Async.sugar;

function BookmarksEngine(pbeId) {
  this._init(pbeId);
}
BookmarksEngine.prototype = {
  get name() { return "bookmarks"; },
  get logName() { return "BmkEngine"; },
  get serverPrefix() { return "user-data/bookmarks/"; },

  __core: null,
  get _core() {
    if (!this.__core)
      this.__core = new BookmarksSyncCore();
    return this.__core;
  },

  __store: null,
  get _store() {
    if (!this.__store)
      this.__store = new BookmarksStore();
    return this.__store;
  },

  __tracker: null,
  get _tracker() {
    if (!this.__tracker)
      this.__tracker = new BookmarksTracker();
    return this.__tracker;
  },

  syncMounts: function BmkEngine_syncMounts(onComplete) {
    this._syncMounts.async(this, onComplete);
  },
  _syncMounts: function BmkEngine__syncMounts() {
    let self = yield;
    let mounts = this._store.findMounts();

    for (i = 0; i < mounts.length; i++) {
      try {
        this._syncOneMount.async(this, self.cb, mounts[i]);
        yield;
      } catch (e) {
        this._log.warn("Could not sync shared folder from " + mounts[i].userid);
        this._log.trace(Utils.stackTrace(e));
      }
    }
  },

  _syncOneMount: function BmkEngine__syncOneMount(mountData) {
    let self = yield;
    let user = mountData.userid;
    let prefix = DAV.defaultPrefix;
    let serverURL = Utils.prefs.getCharPref("serverURL");
    let snap = new SnapshotStore();

    this._log.debug("Syncing shared folder from user " + user);

    try {
      DAV.defaultPrefix = "user/" + user + "/";  //FIXME: very ugly!

      this._getSymKey.async(this, self.cb);
      yield;

      this._log.trace("Getting status file for " + user);
      DAV.GET(this.statusFile, self.cb);
      let resp = yield;
      Utils.ensureStatus(resp.status, "Could not download status file.");
      let status = this._json.decode(resp.responseText);

      this._log.trace("Downloading server snapshot for " + user);
      DAV.GET(this.snapshotFile, self.cb);
      resp = yield;
      Utils.ensureStatus(resp.status, "Could not download snapshot.");
      Crypto.PBEdecrypt.async(Crypto, self.cb, resp.responseText,
    			        this._engineId, status.snapEncryption);
      let data = yield;
      snap.data = this._json.decode(data);

      this._log.trace("Downloading server deltas for " + user);
      DAV.GET(this.deltasFile, self.cb);
      resp = yield;
      Utils.ensureStatus(resp.status, "Could not download deltas.");
      Crypto.PBEdecrypt.async(Crypto, self.cb, resp.responseText,
    			        this._engineId, status.deltasEncryption);
      data = yield;
      deltas = this._json.decode(data);
    }
    catch (e) { throw e; }
    finally { DAV.defaultPrefix = prefix; }

    // apply deltas to get current snapshot
    for (var i = 0; i < deltas.length; i++) {
      snap.applyCommands.async(snap, self.cb, deltas[i]);
      yield;
    }

    // prune tree / get what we want
    for (let guid in snap.data) {
      if (snap.data[guid].type != "bookmark")
        delete snap.data[guid];
      else
        snap.data[guid].parentGUID = mountData.rootGUID;
    }

    this._log.trace("Got bookmarks fror " + user + ", comparing with local copy");
    this._core.detectUpdates(self.cb, mountData.snapshot, snap.data);
    let diff = yield;

    // FIXME: should make sure all GUIDs here live under the mountpoint
    this._log.trace("Applying changes to folder from " + user);
    this._store.applyCommands.async(this._store, self.cb, diff);
    yield;

    this._log.trace("Shared folder from " + user + " successfully synced!");
  }
};
BookmarksEngine.prototype.__proto__ = new Engine();

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

  _getEdits: function BSC__getEdits(a, b) {
    // NOTE: we do not increment ret.numProps, as that would cause
    // edit commands to always get generated
    let ret = SyncCore.prototype._getEdits.call(this, a, b);
    ret.props.type = a.type;
    return ret;
  },

  // compares properties
  // returns true if the property is not set in either object
  // returns true if the property is set and equal in both objects
  // returns false otherwise
  _comp: function BSC__comp(a, b, prop) {
    return (!a.data[prop] && !b.data[prop]) ||
      (a.data[prop] && b.data[prop] && (a.data[prop] == b.data[prop]));
  },

  _commandLike: function BSC__commandLike(a, b) {
    // Check that neither command is null, that their actions, types,
    // and parents are the same, and that they don't have the same
    // GUID.
    // * Items with the same GUID do not qualify for 'likeness' because
    //   we already consider them to be the same object, and therefore
    //   we need to process any edits.
    // * Remove or edit commands don't qualify for likeness either,
    //   since remove or edit commands with different GUIDs are
    //   guaranteed to refer to two different items
    // * The parent GUID check works because reconcile() fixes up the
    //   parent GUIDs as it runs, and the command list is sorted by
    //   depth
    if (!a || !b ||
        a.action != b.action ||
        a.action != "create" ||
        a.data.type != b.data.type ||
        a.data.parentGUID != b.data.parentGUID ||
        a.GUID == b.GUID)
      return false;

    // Bookmarks and folders are allowed to be in a different index as long as
    // they are in the same folder.  Separators must be at
    // the same index to qualify for 'likeness'.
    switch (a.data.type) {
    case "bookmark":
      if (this._comp(a, b, 'URI') &&
          this._comp(a, b, 'title'))
        return true;
      return false;
    case "query":
      if (this._comp(a, b, 'URI') &&
          this._comp(a, b, 'title'))
        return true;
      return false;
    case "microsummary":
      if (this._comp(a, b, 'URI') &&
          this._comp(a, b, 'generatorURI'))
        return true;
      return false;
    case "folder":
      if (this._comp(a, b, 'title'))
        return true;
      return false;
    case "livemark":
      if (this._comp(a, b, 'title') &&
          this._comp(a, b, 'siteURI') &&
          this._comp(a, b, 'feedURI'))
        return true;
      return false;
    case "separator":
      if (this._comp(a, b, 'index'))
        return true;
      return false;
    default:
      let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
      this._log.error("commandLike: Unknown item type: " + json.encode(a));
      return false;
    }
  }
};
BookmarksSyncCore.prototype.__proto__ = new SyncCore();

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

      // This will throw if makeURI can't make an nsIURI object out of the
      // node.uri string (or return null if node.uri is null), in which case
      // we won't be able to get tags for the bookmark (but we'll still sync
      // the rest of the record).
      let uri;
      try {
        uri = Utils.makeURI(node.uri);
      }
      catch(e) {
        this._log.error("error parsing URI string <" + node.uri + "> " +
                        "for item " + node.itemId + " (" + node.title + "): " +
                        e);
      }

      if (uri)
        item.tags = this._ts.getTagsForURI(uri, {});

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
  onItemVisited: function BMT_onItemVisited() {

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
    this._log = Log4Moz.Service.getLogger("Service." + this._logName);
    this._score = 0;

    Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
    getService(Ci.nsINavBookmarksService).
    addObserver(this, false);
  }
}
BookmarksTracker.prototype.__proto__ = new Tracker();
