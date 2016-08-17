/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ['BookmarksEngine', "PlacesItem", "Bookmark",
                         "BookmarkFolder", "BookmarkQuery",
                         "Livemark", "BookmarkSeparator"];

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/PlacesSyncUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/bookmark_utils.js");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/PlacesBackups.jsm");

const ANNOS_TO_TRACK = [BookmarkAnnos.DESCRIPTION_ANNO, BookmarkAnnos.SIDEBAR_ANNO,
                        PlacesUtils.LMANNO_FEEDURI, PlacesUtils.LMANNO_SITEURI];

const SERVICE_NOT_SUPPORTED = "Service not supported on this platform";
const FOLDER_SORTINDEX = 1000000;
const { SOURCE_SYNC } = Ci.nsINavBookmarksService;

// Maps Sync record property names to `PlacesSyncUtils` bookmark properties.
const RECORD_PROPS_TO_BOOKMARK_PROPS = {
  title: "title",
  bmkUri: "url",
  tags: "tags",
  keyword: "keyword",
  description: "description",
  loadInSidebar: "loadInSidebar",
  queryId: "query",
  siteUri: "site",
  feedUri: "feed",
};

this.PlacesItem = function PlacesItem(collection, id, type) {
  CryptoWrapper.call(this, collection, id);
  this.type = type || "item";
}
PlacesItem.prototype = {
  decrypt: function PlacesItem_decrypt(keyBundle) {
    // Do the normal CryptoWrapper decrypt, but change types before returning
    let clear = CryptoWrapper.prototype.decrypt.call(this, keyBundle);

    // Convert the abstract places item to the actual object type
    if (!this.deleted)
      this.__proto__ = this.getTypeObject(this.type).prototype;

    return clear;
  },

  getTypeObject: function PlacesItem_getTypeObject(type) {
    switch (type) {
      case "bookmark":
      case "microsummary":
        return Bookmark;
      case "query":
        return BookmarkQuery;
      case "folder":
        return BookmarkFolder;
      case "livemark":
        return Livemark;
      case "separator":
        return BookmarkSeparator;
      case "item":
        return PlacesItem;
    }
    throw "Unknown places item object type: " + type;
  },

  __proto__: CryptoWrapper.prototype,
  _logName: "Sync.Record.PlacesItem",
};

Utils.deferGetSet(PlacesItem,
                  "cleartext",
                  ["hasDupe", "parentid", "parentName", "type"]);

this.Bookmark = function Bookmark(collection, id, type) {
  PlacesItem.call(this, collection, id, type || "bookmark");
}
Bookmark.prototype = {
  __proto__: PlacesItem.prototype,
  _logName: "Sync.Record.Bookmark",
};

Utils.deferGetSet(Bookmark,
                  "cleartext",
                  ["title", "bmkUri", "description",
                   "loadInSidebar", "tags", "keyword"]);

this.BookmarkQuery = function BookmarkQuery(collection, id) {
  Bookmark.call(this, collection, id, "query");
}
BookmarkQuery.prototype = {
  __proto__: Bookmark.prototype,
  _logName: "Sync.Record.BookmarkQuery",
};

Utils.deferGetSet(BookmarkQuery,
                  "cleartext",
                  ["folderName", "queryId"]);

this.BookmarkFolder = function BookmarkFolder(collection, id, type) {
  PlacesItem.call(this, collection, id, type || "folder");
}
BookmarkFolder.prototype = {
  __proto__: PlacesItem.prototype,
  _logName: "Sync.Record.Folder",
};

Utils.deferGetSet(BookmarkFolder, "cleartext", ["description", "title",
                                                "children"]);

this.Livemark = function Livemark(collection, id) {
  BookmarkFolder.call(this, collection, id, "livemark");
}
Livemark.prototype = {
  __proto__: BookmarkFolder.prototype,
  _logName: "Sync.Record.Livemark",
};

Utils.deferGetSet(Livemark, "cleartext", ["siteUri", "feedUri"]);

this.BookmarkSeparator = function BookmarkSeparator(collection, id) {
  PlacesItem.call(this, collection, id, "separator");
}
BookmarkSeparator.prototype = {
  __proto__: PlacesItem.prototype,
  _logName: "Sync.Record.Separator",
};

Utils.deferGetSet(BookmarkSeparator, "cleartext", "pos");

this.BookmarksEngine = function BookmarksEngine(service) {
  SyncEngine.call(this, "Bookmarks", service);
}
BookmarksEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _recordObj: PlacesItem,
  _storeObj: BookmarksStore,
  _trackerObj: BookmarksTracker,
  version: 2,
  _defaultSort: "index",

  syncPriority: 4,

  // A diagnostic helper to get the string value for a bookmark's URL given
  // its ID. Always returns a string - on error will return a string in the
  // form of "<description of error>" as this is purely for, eg, logging.
  // (This means hitting the DB directly and we don't bother using a cached
  // statement - we should rarely hit this.)
  _getStringUrlForId(id) {
    let url;
    try {
      let stmt = this._store._getStmt(`
            SELECT h.url
            FROM moz_places h
            JOIN moz_bookmarks b ON h.id = b.fk
            WHERE b.id = :id`);
      stmt.params.id = id;
      let rows = Async.querySpinningly(stmt, ["url"]);
      url = rows.length == 0 ? "<not found>" : rows[0].url;
    } catch (ex) {
      if (Async.isShutdownException(ex)) {
        throw ex;
      }
      if (ex instanceof Ci.mozIStorageError) {
        url = `<failed: Storage error: ${ex.message} (${ex.result})>`;
      } else {
        url = `<failed: ${ex.toString()}>`;
      }
    }
    return url;
  },

  _guidMapFailed: false,
  _buildGUIDMap: function _buildGUIDMap() {
    let guidMap = {};
    let tree = Async.promiseSpinningly(PlacesUtils.promiseBookmarksTree("", {
      includeItemIds: true
    }));
    function* walkBookmarksTree(tree, parent=null) {
      if (tree) {
        // Skip root node
        if (parent) {
          yield [tree, parent];
        }
        if (tree.children) {
          for (let child of tree.children) {
            yield* walkBookmarksTree(child, tree);
          }
        }
      }
    }

    function* walkBookmarksRoots(tree, rootGUIDs) {
      for (let guid of rootGUIDs) {
        let id = BookmarkSpecialIds.specialIdForGUID(guid, false);
        let bookmarkRoot = id === null ? null :
          tree.children.find(child => child.id === id);
        if (bookmarkRoot === null) {
          continue;
        }
        yield* walkBookmarksTree(bookmarkRoot, tree);
      }
    }

    let rootsToWalk = BookmarkSpecialIds.guids.filter(guid =>
      guid !== 'places' && guid !== 'tags');

    for (let [node, parent] of walkBookmarksRoots(tree, rootsToWalk)) {
      let {guid, id, type: placeType} = node;
      guid = BookmarkSpecialIds.specialGUIDForId(id) || guid;
      let key;
      switch (placeType) {
        case PlacesUtils.TYPE_X_MOZ_PLACE:
          // Bookmark
          let query = null;
          if (node.annos && node.uri.startsWith("place:")) {
            query = node.annos.find(({name}) => name === BookmarkAnnos.SMART_BOOKMARKS_ANNO);
          }
          if (query && query.value) {
            key = "q" + query.value;
          } else {
            key = "b" + node.uri + ":" + node.title;
          }
          break;
        case PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER:
          // Folder
          key = "f" + node.title;
          break;
        case PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR:
          // Separator
          key = "s" + node.index;
          break;
        default:
          this._log.error("Unknown place type: '"+placeType+"'");
          continue;
      }

      let parentName = parent.title;
      if (guidMap[parentName] == null)
        guidMap[parentName] = {};

      // If the entry already exists, remember that there are explicit dupes.
      let entry = new String(guid);
      entry.hasDupe = guidMap[parentName][key] != null;

      // Remember this item's GUID for its parent-name/key pair.
      guidMap[parentName][key] = entry;
      this._log.trace("Mapped: " + [parentName, key, entry, entry.hasDupe]);
    }

    return guidMap;
  },

  // Helper function to get a dupe GUID for an item.
  _mapDupe: function _mapDupe(item) {
    // Figure out if we have something to key with.
    let key;
    let altKey;
    switch (item.type) {
      case "query":
        // Prior to Bug 610501, records didn't carry their Smart Bookmark
        // anno, so we won't be able to dupe them correctly. This altKey
        // hack should get them to dupe correctly.
        if (item.queryId) {
          key = "q" + item.queryId;
          altKey = "b" + item.bmkUri + ":" + item.title;
          break;
        }
        // No queryID? Fall through to the regular bookmark case.
      case "bookmark":
      case "microsummary":
        key = "b" + item.bmkUri + ":" + item.title;
        break;
      case "folder":
      case "livemark":
        key = "f" + item.title;
        break;
      case "separator":
        key = "s" + item.pos;
        break;
      default:
        return;
    }

    // Figure out if we have a map to use!
    // This will throw in some circumstances. That's fine.
    let guidMap = this._guidMap;

    // Give the GUID if we have the matching pair.
    this._log.trace("Finding mapping: " + item.parentName + ", " + key);
    let parent = guidMap[item.parentName];

    if (!parent) {
      this._log.trace("No parent => no dupe.");
      return undefined;
    }

    let dupe = parent[key];

    if (dupe) {
      this._log.trace("Mapped dupe: " + dupe);
      return dupe;
    }

    if (altKey) {
      dupe = parent[altKey];
      if (dupe) {
        this._log.trace("Mapped dupe using altKey " + altKey + ": " + dupe);
        return dupe;
      }
    }

    this._log.trace("No dupe found for key " + key + "/" + altKey + ".");
    return undefined;
  },

  _syncStartup: function _syncStart() {
    SyncEngine.prototype._syncStartup.call(this);

    let cb = Async.makeSpinningCallback();
    Task.spawn(function* () {
      // For first-syncs, make a backup for the user to restore
      if (this.lastSync == 0) {
        this._log.debug("Bookmarks backup starting.");
        yield PlacesBackups.create(null, true);
        this._log.debug("Bookmarks backup done.");
      }
    }.bind(this)).then(
      cb, ex => {
        // Failure to create a backup is somewhat bad, but probably not bad
        // enough to prevent syncing of bookmarks - so just log the error and
        // continue.
        this._log.warn("Error while backing up bookmarks, but continuing with sync", ex);
        cb();
      }
    );

    cb.wait();

    this.__defineGetter__("_guidMap", function() {
      // Create a mapping of folder titles and separator positions to GUID.
      // We do this lazily so that we don't do any work unless we reconcile
      // incoming items.
      let guidMap;
      try {
        guidMap = this._buildGUIDMap();
      } catch (ex) {
        if (Async.isShutdownException(ex)) {
          throw ex;
        }
        this._log.warn("Error while building GUID map, skipping all other incoming items", ex);
        throw {code: Engine.prototype.eEngineAbortApplyIncoming,
               cause: ex};
      }
      delete this._guidMap;
      return this._guidMap = guidMap;
    });

    this._store._childrenToOrder = {};
  },

  _processIncoming: function (newitems) {
    try {
      SyncEngine.prototype._processIncoming.call(this, newitems);
    } finally {
      // Reorder children.
      this._tracker.ignoreAll = true;
      this._store._orderChildren();
      this._tracker.ignoreAll = false;
      delete this._store._childrenToOrder;
    }
  },

  _syncFinish: function _syncFinish() {
    SyncEngine.prototype._syncFinish.call(this);
    this._tracker._ensureMobileQuery();
  },

  _syncCleanup: function _syncCleanup() {
    SyncEngine.prototype._syncCleanup.call(this);
    delete this._guidMap;
  },

  _createRecord: function _createRecord(id) {
    // Create the record as usual, but mark it as having dupes if necessary.
    let record = SyncEngine.prototype._createRecord.call(this, id);
    let entry = this._mapDupe(record);
    if (entry != null && entry.hasDupe) {
      record.hasDupe = true;
    }
    return record;
  },

  _findDupe: function _findDupe(item) {
    this._log.trace("Finding dupe for " + item.id +
                    " (already duped: " + item.hasDupe + ").");

    // Don't bother finding a dupe if the incoming item has duplicates.
    if (item.hasDupe) {
      this._log.trace(item.id + " already a dupe: not finding one.");
      return;
    }
    let mapped = this._mapDupe(item);
    this._log.debug(item.id + " mapped to " + mapped);
    // We must return a string, not an object, and the entries in the GUIDMap
    // are created via "new String()" making them an object.
    return mapped ? mapped.toString() : mapped;
  }
};

function BookmarksStore(name, engine) {
  Store.call(this, name, engine);

  // Explicitly nullify our references to our cached services so we don't leak
  Svc.Obs.add("places-shutdown", function() {
    for (let query in this._stmts) {
      let stmt = this._stmts[query];
      stmt.finalize();
    }
    this._stmts = {};
  }, this);
}
BookmarksStore.prototype = {
  __proto__: Store.prototype,

  itemExists: function BStore_itemExists(id) {
    return this.idForGUID(id, true) > 0;
  },

  applyIncoming: function BStore_applyIncoming(record) {
    this._log.debug("Applying record " + record.id);
    let isSpecial = record.id in BookmarkSpecialIds;

    if (record.deleted) {
      if (isSpecial) {
        this._log.warn("Ignoring deletion for special record " + record.id);
        return;
      }

      // Don't bother with pre and post-processing for deletions.
      Store.prototype.applyIncoming.call(this, record);
      return;
    }

    // For special folders we're only interested in child ordering.
    if (isSpecial && record.children) {
      this._log.debug("Processing special node: " + record.id);
      // Reorder children later
      this._childrenToOrder[record.id] = record.children;
      return;
    }

    // Skip malformed records. (Bug 806460.)
    if (record.type == "query" &&
        !record.bmkUri) {
      this._log.warn("Skipping malformed query bookmark: " + record.id);
      return;
    }

    // Figure out the local id of the parent GUID if available
    let parentGUID = record.parentid;
    if (!parentGUID) {
      throw "Record " + record.id + " has invalid parentid: " + parentGUID;
    }
    this._log.debug("Local parent is " + parentGUID);

    // Do the normal processing of incoming records
    Store.prototype.applyIncoming.call(this, record);

    if (record.type == "folder" && record.children) {
      this._childrenToOrder[record.id] = record.children;
    }
  },

  create: function BStore_create(record) {
    switch (record.type) {
    case "bookmark":
    case "query":
    case "microsummary": {
      let info = {
        kind: record.type,
        url: record.bmkUri,
        parentGuid: BookmarkSpecialIds.syncIDToPlacesGUID(record.parentid),
        title: record.title,
        guid: BookmarkSpecialIds.syncIDToPlacesGUID(record.id),
        tags: record.tags,
        keyword: record.keyword,
      };
      if (record.loadInSidebar) {
        info.loadInSidebar = record.loadInSidebar;
      }
      if (record.queryId) {
        info.query = record.queryId;
      }
      if (record.folderName) {
        info.folder = record.folderName;
      }
      if (record.description) {
        info.description = record.description;
      }

      let bmk = Async.promiseSpinningly(PlacesSyncUtils.bookmarks.insert(info));
      this._log.debug("created bookmark " + bmk.guid + " under " + bmk.parentGuid
                      + " as " + bmk.title + " " + bmk.url.href);

    } break;
    case "folder": {
      let info = {
        kind: PlacesSyncUtils.bookmarks.KINDS.FOLDER,
        parentGuid: BookmarkSpecialIds.syncIDToPlacesGUID(record.parentid),
        guid: BookmarkSpecialIds.syncIDToPlacesGUID(record.id),
        title: record.title,
      };
      if (record.description) {
        info.description = record.description;
      }

      let folder = Async.promiseSpinningly(PlacesSyncUtils.bookmarks.insert(info));
      this._log.debug("created folder " + folder.guid + " under " + folder.parentGuid
                      + " as " + folder.title);

      // record.children will be dealt with in _orderChildren.
    } break;
    case "livemark":
      if (!record.feedUri) {
        this._log.debug("No feed URI: skipping livemark record " + record.id);
        return;
      }
      let info = {
        kind: PlacesSyncUtils.bookmarks.KINDS.LIVEMARK,
        title: record.title,
        parentGuid: BookmarkSpecialIds.syncIDToPlacesGUID(record.parentid),
        feed: record.feedUri,
        site: record.siteUri,
        guid: BookmarkSpecialIds.syncIDToPlacesGUID(record.id),
      };
      let livemark = Async.promiseSpinningly(PlacesSyncUtils.bookmarks.insert(info));
      if (livemark) {
        this._log.debug("Created livemark " + livemark.id + " under " +
                        livemark.parentId + " as " + livemark.title +
                        ", " + livemark.siteURI.spec + ", " +
                        livemark.feedURI.spec + ", GUID " +
                        livemark.guid);
      }
      break;
    case "separator": {
      let separator = Async.promiseSpinningly(PlacesSyncUtils.bookmarks.insert({
        kind: PlacesSyncUtils.bookmarks.KINDS.SEPARATOR,
        parentGuid: BookmarkSpecialIds.syncIDToPlacesGUID(record.parentid),
        guid: BookmarkSpecialIds.syncIDToPlacesGUID(record.id),
      }));
      this._log.debug("created separator " + separator.guid + " under " + separator.parentGuid);
    } break;
    case "item":
      this._log.debug(" -> got a generic places item.. do nothing?");
      return;
    default:
      this._log.error("_create: Unknown item type: " + record.type);
      return;
    }
  },

  remove: function BStore_remove(record) {
    if (BookmarkSpecialIds.isSpecialGUID(record.id)) {
      this._log.warn("Refusing to remove special folder " + record.id);
      return;
    }

    let guid = BookmarkSpecialIds.syncIDToPlacesGUID(record.id);
    try {
      let info = Async.promiseSpinningly(PlacesSyncUtils.bookmarks.remove(guid));
      this._log.debug(`Removed item ${guid} with type ${info.type}`);
    } catch (ex) {
      // Likely already removed.
      this._log.debug(`Error removing ${record.id}`, ex);
    }
  },

  update: function BStore_update(record) {
    let info = {
      parentGuid: BookmarkSpecialIds.syncIDToPlacesGUID(record.parentid),
      guid: BookmarkSpecialIds.syncIDToPlacesGUID(record.id),
      kind: record.type,
    };

    for (let prop of Object.keys(RECORD_PROPS_TO_BOOKMARK_PROPS)) {
      let bmkProp = RECORD_PROPS_TO_BOOKMARK_PROPS[prop];
      if (prop in record.cleartext) {
        info[bmkProp] = record.cleartext[prop];
      }
    }

    let bmk = Async.promiseSpinningly(PlacesSyncUtils.bookmarks.update(info));
    this._log.debug("updated bookmark " + bmk.guid + " under " + bmk.parentGuid);
  },

  _orderChildren: function _orderChildren() {
    let promises = Object.keys(this._childrenToOrder).map(syncID => {
      let children = this._childrenToOrder[syncID];
      if (!children.length) {
        return Promise.resolve();
      }
      let guid = BookmarkSpecialIds.syncIDToPlacesGUID(syncID);
      let childGUIDs = children.map(syncID => BookmarkSpecialIds.syncIDToPlacesGUID(syncID));
      return PlacesSyncUtils.bookmarks.order(guid, childGUIDs).catch(ex => {
        this._log.debug(`Could not order children for ${guid}: ${ex}`);
      });
    });
    Async.promiseSpinningly(Promise.all(promises));
  },

  changeItemID: function BStore_changeItemID(oldID, newID) {
    this._log.debug("Changing GUID " + oldID + " to " + newID);

    Async.promiseSpinningly(PlacesSyncUtils.bookmarks.changeGuid(
      BookmarkSpecialIds.syncIDToPlacesGUID(oldID),
      BookmarkSpecialIds.syncIDToPlacesGUID(newID)
    ));
  },

  _getNode: function BStore__getNode(folder) {
    let query = PlacesUtils.history.getNewQuery();
    query.setFolders([folder], 1);
    return PlacesUtils.history.executeQuery(
      query, PlacesUtils.history.getNewQueryOptions()).root;
  },

  _getTags: function BStore__getTags(uri) {
    try {
      if (typeof(uri) == "string")
        uri = Utils.makeURI(uri);
    } catch(e) {
      this._log.warn("Could not parse URI \"" + uri + "\": " + e);
    }
    return PlacesUtils.tagging.getTagsForURI(uri, {});
  },

  _getDescription: function BStore__getDescription(id) {
    try {
      return PlacesUtils.annotations.getItemAnnotation(id, BookmarkAnnos.DESCRIPTION_ANNO);
    } catch (e) {
      return null;
    }
  },

  _isLoadInSidebar: function BStore__isLoadInSidebar(id) {
    return PlacesUtils.annotations.itemHasAnnotation(id, BookmarkAnnos.SIDEBAR_ANNO);
  },

  // Create a record starting from the weave id (places guid)
  createRecord: function createRecord(id, collection) {
    let placeId = this.idForGUID(id);
    let record;
    if (placeId <= 0) { // deleted item
      record = new PlacesItem(collection, id);
      record.deleted = true;
      return record;
    }

    let parent = PlacesUtils.bookmarks.getFolderIdForItem(placeId);
    switch (PlacesUtils.bookmarks.getItemType(placeId)) {
    case PlacesUtils.bookmarks.TYPE_BOOKMARK:
      let bmkUri = PlacesUtils.bookmarks.getBookmarkURI(placeId).spec;
      if (bmkUri.indexOf("place:") == 0) {
        record = new BookmarkQuery(collection, id);

        // Get the actual tag name instead of the local itemId
        let folder = bmkUri.match(/[:&]folder=(\d+)/);
        try {
          // There might not be the tag yet when creating on a new client
          if (folder != null) {
            folder = folder[1];
            record.folderName = PlacesUtils.bookmarks.getItemTitle(folder);
            this._log.trace("query id: " + folder + " = " + record.folderName);
          }
        }
        catch(ex) {}

        // Persist the Smart Bookmark anno, if found.
        try {
          let anno = PlacesUtils.annotations.getItemAnnotation(placeId, BookmarkAnnos.SMART_BOOKMARKS_ANNO);
          if (anno != null) {
            this._log.trace("query anno: " + BookmarkAnnos.SMART_BOOKMARKS_ANNO +
                            " = " + anno);
            record.queryId = anno;
          }
        }
        catch(ex) {}
      }
      else {
        record = new Bookmark(collection, id);
      }
      record.title = PlacesUtils.bookmarks.getItemTitle(placeId);

      record.parentName = PlacesUtils.bookmarks.getItemTitle(parent);
      record.bmkUri = bmkUri;
      record.tags = this._getTags(record.bmkUri);
      record.keyword = PlacesUtils.bookmarks.getKeywordForBookmark(placeId);
      record.description = this._getDescription(placeId);
      record.loadInSidebar = this._isLoadInSidebar(placeId);
      break;

    case PlacesUtils.bookmarks.TYPE_FOLDER:
      if (PlacesUtils.annotations
                     .itemHasAnnotation(placeId, PlacesUtils.LMANNO_FEEDURI)) {
        record = new Livemark(collection, id);
        let as = PlacesUtils.annotations;
        record.feedUri = as.getItemAnnotation(placeId, PlacesUtils.LMANNO_FEEDURI);
        try {
          record.siteUri = as.getItemAnnotation(placeId, PlacesUtils.LMANNO_SITEURI);
        } catch (ex) {}
      } else {
        record = new BookmarkFolder(collection, id);
      }

      if (parent > 0)
        record.parentName = PlacesUtils.bookmarks.getItemTitle(parent);
      record.title = PlacesUtils.bookmarks.getItemTitle(placeId);
      record.description = this._getDescription(placeId);
      record.children = Async.promiseSpinningly(
        PlacesSyncUtils.bookmarks.fetchChildGuids(
          BookmarkSpecialIds.syncIDToPlacesGUID(id)));
      break;

    case PlacesUtils.bookmarks.TYPE_SEPARATOR:
      record = new BookmarkSeparator(collection, id);
      if (parent > 0)
        record.parentName = PlacesUtils.bookmarks.getItemTitle(parent);
      // Create a positioning identifier for the separator, used by _mapDupe
      record.pos = PlacesUtils.bookmarks.getItemIndex(placeId);
      break;

    default:
      record = new PlacesItem(collection, id);
      this._log.warn("Unknown item type, cannot serialize: " +
                     PlacesUtils.bookmarks.getItemType(placeId));
    }

    record.parentid = this.GUIDForId(parent);
    record.sortindex = this._calculateIndex(record);

    return record;
  },

  _stmts: {},
  _getStmt: function(query) {
    if (query in this._stmts) {
      return this._stmts[query];
    }

    this._log.trace("Creating SQL statement: " + query);
    let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                        .DBConnection;
    return this._stmts[query] = db.createAsyncStatement(query);
  },

  get _frecencyStm() {
    return this._getStmt(
        "SELECT frecency " +
        "FROM moz_places " +
        "WHERE url_hash = hash(:url) AND url = :url " +
        "LIMIT 1");
  },
  _frecencyCols: ["frecency"],

  GUIDForId: function GUIDForId(id) {
    let special = BookmarkSpecialIds.specialGUIDForId(id);
    if (special)
      return special;

    return Async.promiseSpinningly(
      PlacesSyncUtils.bookmarks.ensureGuidForId(id));
  },

  // noCreate is provided as an optional argument to prevent the creation of
  // non-existent special records, such as "mobile".
  idForGUID: function idForGUID(guid, noCreate) {
    // guid might be a String object rather than a string.
    guid = guid.toString();

    if (BookmarkSpecialIds.isSpecialGUID(guid))
      return BookmarkSpecialIds.specialIdForGUID(guid, !noCreate);

    return Async.promiseSpinningly(PlacesUtils.promiseItemId(guid).catch(
      ex => -1));
  },

  _calculateIndex: function _calculateIndex(record) {
    // Ensure folders have a very high sort index so they're not synced last.
    if (record.type == "folder")
      return FOLDER_SORTINDEX;

    // For anything directly under the toolbar, give it a boost of more than an
    // unvisited bookmark
    let index = 0;
    if (record.parentid == "toolbar")
      index += 150;

    // Add in the bookmark's frecency if we have something.
    if (record.bmkUri != null) {
      this._frecencyStm.params.url = record.bmkUri;
      let result = Async.querySpinningly(this._frecencyStm, this._frecencyCols);
      if (result.length)
        index += result[0].frecency;
    }

    return index;
  },

  _getChildren: function BStore_getChildren(guid, items) {
    let node = guid; // the recursion case
    if (typeof(node) == "string") { // callers will give us the guid as the first arg
      let nodeID = this.idForGUID(guid, true);
      if (!nodeID) {
        this._log.debug("No node for GUID " + guid + "; returning no children.");
        return items;
      }
      node = this._getNode(nodeID);
    }

    if (node.type == node.RESULT_TYPE_FOLDER) {
      node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      node.containerOpen = true;
      try {
        // Remember all the children GUIDs and recursively get more
        for (let i = 0; i < node.childCount; i++) {
          let child = node.getChild(i);
          items[this.GUIDForId(child.itemId)] = true;
          this._getChildren(child, items);
        }
      }
      finally {
        node.containerOpen = false;
      }
    }

    return items;
  },

  getAllIDs: function BStore_getAllIDs() {
    let items = {"menu": true,
                 "toolbar": true,
                 "unfiled": true,
                };
    // We also want "mobile" but only if a local mobile folder already exists
    // (otherwise we'll later end up creating it, which we want to avoid until
    // we actually need it.)
    if (BookmarkSpecialIds.findMobileRoot(false)) {
      items["mobile"] = true;
    }
    for (let guid of BookmarkSpecialIds.guids) {
      if (guid != "places" && guid != "tags")
        this._getChildren(guid, items);
    }
    return items;
  },

  wipe: function BStore_wipe() {
    let cb = Async.makeSpinningCallback();
    Task.spawn(function* () {
      // Save a backup before clearing out all bookmarks.
      yield PlacesBackups.create(null, true);
      // Instead of exposing a "clear folder" method, we should instead have
      // `PlacesSyncUtils` clear all roots. `eraseEverything` comes close,
      // but doesn't clear the mobile root. The fix is to move mobile root
      // and query handling into Places.
      for (let syncID of BookmarkSpecialIds.guids) {
        if (syncID == "places") {
          continue;
        }
        if (syncID == "mobile" && !BookmarkSpecialIds.findMobileRoot(false)) {
          // `syncIDToPlacesGUID` will create the mobile root as a side effect,
          // which we don't want it to do if we're clearing bookmarks.
          continue;
        }
        let guid = BookmarkSpecialIds.syncIDToPlacesGUID(syncID);
        yield PlacesSyncUtils.bookmarks.clear(guid);
      }
      cb();
    });
    cb.wait();
  }
};

function BookmarksTracker(name, engine) {
  this._batchDepth = 0;
  this._batchSawScoreIncrement = false;
  Tracker.call(this, name, engine);

  Svc.Obs.add("places-shutdown", this);
}
BookmarksTracker.prototype = {
  __proto__: Tracker.prototype,

  startTracking: function() {
    PlacesUtils.bookmarks.addObserver(this, true);
    Svc.Obs.add("bookmarks-restore-begin", this);
    Svc.Obs.add("bookmarks-restore-success", this);
    Svc.Obs.add("bookmarks-restore-failed", this);
  },

  stopTracking: function() {
    PlacesUtils.bookmarks.removeObserver(this);
    Svc.Obs.remove("bookmarks-restore-begin", this);
    Svc.Obs.remove("bookmarks-restore-success", this);
    Svc.Obs.remove("bookmarks-restore-failed", this);
  },

  observe: function observe(subject, topic, data) {
    Tracker.prototype.observe.call(this, subject, topic, data);

    switch (topic) {
      case "bookmarks-restore-begin":
        this._log.debug("Ignoring changes from importing bookmarks.");
        this.ignoreAll = true;
        break;
      case "bookmarks-restore-success":
        this._log.debug("Tracking all items on successful import.");
        this.ignoreAll = false;

        this._log.debug("Restore succeeded: wiping server and other clients.");
        this.engine.service.resetClient([this.name]);
        this.engine.service.wipeServer([this.name]);
        this.engine.service.clientsEngine.sendCommand("wipeEngine", [this.name]);
        break;
      case "bookmarks-restore-failed":
        this._log.debug("Tracking all items on failed import.");
        this.ignoreAll = false;
        break;
    }
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavBookmarkObserver,
    Ci.nsINavBookmarkObserver_MOZILLA_1_9_1_ADDITIONS,
    Ci.nsISupportsWeakReference
  ]),

  /**
   * Add a bookmark GUID to be uploaded and bump up the sync score.
   *
   * @param itemGuid
   *        GUID of the bookmark to upload.
   */
  _add: function BMT__add(itemId, guid) {
    guid = BookmarkSpecialIds.specialGUIDForId(itemId) || guid;
    if (this.addChangedID(guid))
      this._upScore();
  },

  /* Every add/remove/change will trigger a sync for MULTI_DEVICE (except in
     a batch operation, where we do it at the end of the batch) */
  _upScore: function BMT__upScore() {
    if (this._batchDepth == 0) {
      this.score += SCORE_INCREMENT_XLARGE;
    } else {
      this._batchSawScoreIncrement = true;
    }
  },

  /**
   * Determine if a change should be ignored.
   *
   * @param itemId
   *        Item under consideration to ignore
   * @param folder (optional)
   *        Folder of the item being changed
   */
  _ignore: function BMT__ignore(itemId, folder, guid) {
    // Ignore unconditionally if the engine tells us to.
    if (this.ignoreAll)
      return true;

    // Get the folder id if we weren't given one.
    if (folder == null) {
      try {
        folder = PlacesUtils.bookmarks.getFolderIdForItem(itemId);
      } catch (ex) {
        this._log.debug("getFolderIdForItem(" + itemId +
                        ") threw; calling _ensureMobileQuery.");
        // I'm guessing that gFIFI can throw, and perhaps that's why
        // _ensureMobileQuery is here at all. Try not to call it.
        this._ensureMobileQuery();
        folder = PlacesUtils.bookmarks.getFolderIdForItem(itemId);
      }
    }

    // Ignore changes to tags (folders under the tags folder).
    let tags = BookmarkSpecialIds.tags;
    if (folder == tags)
      return true;

    // Ignore tag items (the actual instance of a tag for a bookmark).
    if (PlacesUtils.bookmarks.getFolderIdForItem(folder) == tags)
      return true;

    // Make sure to remove items that have the exclude annotation.
    if (PlacesUtils.annotations.itemHasAnnotation(itemId, BookmarkAnnos.EXCLUDEBACKUP_ANNO)) {
      this.removeChangedID(guid);
      return true;
    }

    return false;
  },

  onItemAdded: function BMT_onItemAdded(itemId, folder, index,
                                        itemType, uri, title, dateAdded,
                                        guid, parentGuid) {
    if (this._ignore(itemId, folder, guid))
      return;

    this._log.trace("onItemAdded: " + itemId);
    this._add(itemId, guid);
    this._add(folder, parentGuid);
  },

  onItemRemoved: function (itemId, parentId, index, type, uri,
                           guid, parentGuid) {
    if (this._ignore(itemId, parentId, guid)) {
      return;
    }

    this._log.trace("onItemRemoved: " + itemId);
    this._add(itemId, guid);
    this._add(parentId, parentGuid);
  },

  _ensureMobileQuery: function _ensureMobileQuery() {
    let find = val =>
      PlacesUtils.annotations.getItemsWithAnnotation(BookmarkAnnos.ORGANIZERQUERY_ANNO, {}).filter(
        id => PlacesUtils.annotations.getItemAnnotation(id, BookmarkAnnos.ORGANIZERQUERY_ANNO) == val
      );

    // Don't continue if the Library isn't ready
    let all = find(BookmarkAnnos.ALLBOOKMARKS_ANNO);
    if (all.length == 0)
      return;

    // Disable handling of notifications while changing the mobile query
    this.ignoreAll = true;

    let mobile = find(BookmarkAnnos.MOBILE_ANNO);
    let queryURI = Utils.makeURI("place:folder=" + BookmarkSpecialIds.mobile);
    let title = Str.sync.get("mobile.label");

    // Don't add OR remove the mobile bookmarks if there's nothing.
    if (PlacesUtils.bookmarks.getIdForItemAt(BookmarkSpecialIds.mobile, 0) == -1) {
      if (mobile.length != 0)
        PlacesUtils.bookmarks.removeItem(mobile[0], SOURCE_SYNC);
    }
    // Add the mobile bookmarks query if it doesn't exist
    else if (mobile.length == 0) {
      let query = PlacesUtils.bookmarks.insertBookmark(all[0], queryURI, -1, title, /* guid */ null, SOURCE_SYNC);
      PlacesUtils.annotations.setItemAnnotation(query, BookmarkAnnos.ORGANIZERQUERY_ANNO, BookmarkAnnos.MOBILE_ANNO, 0,
                                  PlacesUtils.annotations.EXPIRE_NEVER, SOURCE_SYNC);
      PlacesUtils.annotations.setItemAnnotation(query, BookmarkAnnos.EXCLUDEBACKUP_ANNO, 1, 0,
                                  PlacesUtils.annotations.EXPIRE_NEVER, SOURCE_SYNC);
    }
    // Make sure the existing title is correct
    else if (PlacesUtils.bookmarks.getItemTitle(mobile[0]) != title) {
      PlacesUtils.bookmarks.setItemTitle(mobile[0], title, SOURCE_SYNC);
    }

    this.ignoreAll = false;
  },

  // This method is oddly structured, but the idea is to return as quickly as
  // possible -- this handler gets called *every time* a bookmark changes, for
  // *each change*.
  onItemChanged: function BMT_onItemChanged(itemId, property, isAnno, value,
                                            lastModified, itemType, parentId,
                                            guid, parentGuid) {
    // Quicker checks first.
    if (this.ignoreAll)
      return;

    if (isAnno && (ANNOS_TO_TRACK.indexOf(property) == -1))
      // Ignore annotations except for the ones that we sync.
      return;

    // Ignore favicon changes to avoid unnecessary churn.
    if (property == "favicon")
      return;

    if (this._ignore(itemId, parentId, guid))
      return;

    this._log.trace("onItemChanged: " + itemId +
                    (", " + property + (isAnno? " (anno)" : "")) +
                    (value ? (" = \"" + value + "\"") : ""));
    this._add(itemId, guid);
  },

  onItemMoved: function BMT_onItemMoved(itemId, oldParent, oldIndex,
                                        newParent, newIndex, itemType,
                                        guid, oldParentGuid, newParentGuid) {
    if (this._ignore(itemId, newParent, guid))
      return;

    this._log.trace("onItemMoved: " + itemId);
    this._add(oldParent, oldParentGuid);
    if (oldParent != newParent) {
      this._add(itemId, guid);
      this._add(newParent, newParentGuid);
    }

    // Remove any position annotations now that the user moved the item
    PlacesUtils.annotations.removeItemAnnotation(itemId, BookmarkAnnos.PARENT_ANNO, SOURCE_SYNC);
  },

  onBeginUpdateBatch: function () {
    ++this._batchDepth;
  },
  onEndUpdateBatch: function () {
    if (--this._batchDepth === 0 && this._batchSawScoreIncrement) {
      this.score += SCORE_INCREMENT_XLARGE;
      this._batchSawScoreIncrement = false;
    }
  },
  onItemVisited: function () {}
};
