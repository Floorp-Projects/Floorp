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
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/PlacesBackups.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BookmarkValidator",
                                  "resource://services-sync/bookmark_validator.js");
XPCOMUtils.defineLazyGetter(this, "PlacesBundle", () => {
  let bundleService = Cc["@mozilla.org/intl/stringbundle;1"]
                        .getService(Ci.nsIStringBundleService);
  return bundleService.createBundle("chrome://places/locale/places.properties");
});

const ANNOS_TO_TRACK = [PlacesSyncUtils.bookmarks.DESCRIPTION_ANNO,
                        PlacesSyncUtils.bookmarks.SIDEBAR_ANNO,
                        PlacesUtils.LMANNO_FEEDURI, PlacesUtils.LMANNO_SITEURI];

const SERVICE_NOT_SUPPORTED = "Service not supported on this platform";
const FOLDER_SORTINDEX = 1000000;
const {
  SOURCE_SYNC,
  SOURCE_IMPORT,
  SOURCE_IMPORT_REPLACE,
} = Ci.nsINavBookmarksService;

const SQLITE_MAX_VARIABLE_NUMBER = 999;

const ORGANIZERQUERY_ANNO = "PlacesOrganizer/OrganizerQuery";
const ALLBOOKMARKS_ANNO = "AllBookmarks";
const MOBILE_ANNO = "MobileBookmarks";

// Roots that should be deleted from the server, instead of applied locally.
// This matches `AndroidBrowserBookmarksRepositorySession::forbiddenGUID`,
// but allows tags because we don't want to reparent tag folders or tag items
// to "unfiled".
const FORBIDDEN_INCOMING_IDS = ["pinned", "places", "readinglist"];

// Items with these parents should be deleted from the server. We allow
// children of the Places root, to avoid orphaning left pane queries and other
// descendants of custom roots.
const FORBIDDEN_INCOMING_PARENT_IDS = ["pinned", "readinglist"];

// The tracker ignores changes made by bookmark import and restore, and
// changes made by Sync. We don't need to exclude `SOURCE_IMPORT`, but both
// import and restore fire `bookmarks-restore-*` observer notifications, and
// the tracker doesn't currently distinguish between the two.
const IGNORED_SOURCES = [SOURCE_SYNC, SOURCE_IMPORT, SOURCE_IMPORT_REPLACE];

// Returns the constructor for a bookmark record type.
function getTypeObject(type) {
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
  return null;
}

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
    let recordObj = getTypeObject(type);
    if (!recordObj) {
      throw new Error("Unknown places item object type: " + type);
    }
    return recordObj;
  },

  __proto__: CryptoWrapper.prototype,
  _logName: "Sync.Record.PlacesItem",

  // Converts the record to a Sync bookmark object that can be passed to
  // `PlacesSyncUtils.bookmarks.{insert, update}`.
  toSyncBookmark() {
    return {
      kind: this.type,
      syncId: this.id,
      parentSyncId: this.parentid,
    };
  },

  // Populates the record from a Sync bookmark object returned from
  // `PlacesSyncUtils.bookmarks.fetch`.
  fromSyncBookmark(item) {
    this.parentid = item.parentSyncId;
    this.parentName = item.parentTitle;
  },
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

  toSyncBookmark() {
    let info = PlacesItem.prototype.toSyncBookmark.call(this);
    info.title = this.title;
    info.url = this.bmkUri;
    info.description = this.description;
    info.loadInSidebar = this.loadInSidebar;
    info.tags = this.tags;
    info.keyword = this.keyword;
    return info;
  },

  fromSyncBookmark(item) {
    PlacesItem.prototype.fromSyncBookmark.call(this, item);
    this.title = item.title;
    this.bmkUri = item.url.href;
    this.description = item.description;
    this.loadInSidebar = item.loadInSidebar;
    this.tags = item.tags;
    this.keyword = item.keyword;
  },
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

  toSyncBookmark() {
    let info = Bookmark.prototype.toSyncBookmark.call(this);
    info.folder = this.folderName;
    info.query = this.queryId;
    return info;
  },

  fromSyncBookmark(item) {
    Bookmark.prototype.fromSyncBookmark.call(this, item);
    this.folderName = item.folder;
    this.queryId = item.query;
  },
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

  toSyncBookmark() {
    let info = PlacesItem.prototype.toSyncBookmark.call(this);
    info.description = this.description;
    info.title = this.title;
    return info;
  },

  fromSyncBookmark(item) {
    PlacesItem.prototype.fromSyncBookmark.call(this, item);
    this.title = item.title;
    this.description = item.description;
    this.children = item.childSyncIds;
  },
};

Utils.deferGetSet(BookmarkFolder, "cleartext", ["description", "title",
                                                "children"]);

this.Livemark = function Livemark(collection, id) {
  BookmarkFolder.call(this, collection, id, "livemark");
}
Livemark.prototype = {
  __proto__: BookmarkFolder.prototype,
  _logName: "Sync.Record.Livemark",

  toSyncBookmark() {
    let info = BookmarkFolder.prototype.toSyncBookmark.call(this);
    info.feed = this.feedUri;
    info.site = this.siteUri;
    return info;
  },

  fromSyncBookmark(item) {
    BookmarkFolder.prototype.fromSyncBookmark.call(this, item);
    this.feedUri = item.feed.href;
    if (item.site) {
      this.siteUri = item.site.href;
    }
  },
};

Utils.deferGetSet(Livemark, "cleartext", ["siteUri", "feedUri"]);

this.BookmarkSeparator = function BookmarkSeparator(collection, id) {
  PlacesItem.call(this, collection, id, "separator");
}
BookmarkSeparator.prototype = {
  __proto__: PlacesItem.prototype,
  _logName: "Sync.Record.Separator",

  fromSyncBookmark(item) {
    PlacesItem.prototype.fromSyncBookmark.call(this, item);
    this.pos = item.index;
  },
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

    function* walkBookmarksRoots(tree, rootIDs) {
      for (let id of rootIDs) {
        let bookmarkRoot = tree.children.find(child => child.id === id);
        if (bookmarkRoot === null) {
          continue;
        }
        yield* walkBookmarksTree(bookmarkRoot, tree);
      }
    }

    let rootsToWalk = getChangeRootIds();

    for (let [node, parent] of walkBookmarksRoots(tree, rootsToWalk)) {
      let {guid, id, type: placeType} = node;
      guid = PlacesSyncUtils.bookmarks.guidToSyncId(guid);
      let key;
      switch (placeType) {
        case PlacesUtils.TYPE_X_MOZ_PLACE:
          // Bookmark
          let query = null;
          if (node.annos && node.uri.startsWith("place:")) {
            query = node.annos.find(({name}) =>
              name === PlacesSyncUtils.bookmarks.SMART_BOOKMARKS_ANNO);
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
    this._store.clearPendingDeletions();
  },

  _deletePending() {
    // Delete pending items -- See the comment above BookmarkStore's deletePending
    let newlyModified = Async.promiseSpinningly(this._store.deletePending());
    let now = this._tracker._now();
    this._log.debug("Deleted pending items", newlyModified);
    for (let modifiedSyncID of newlyModified) {
      if (!this._modified.has(modifiedSyncID)) {
        this._modified.set(modifiedSyncID, { timestamp: now, deleted: false });
      }
    }
  },

  // We avoid reviving folders since reviving them properly would require
  // reviving their children as well. Unfortunately, this is the wrong choice
  // in the case of a bookmark restore where wipeServer failed -- if the
  // server has the folder as deleted, we *would* want to reupload this folder.
  // This is mitigated by the fact that we move any undeleted children to the
  // grandparent when deleting the parent.
  _shouldReviveRemotelyDeletedRecord(item) {
    let kind = Async.promiseSpinningly(
      PlacesSyncUtils.bookmarks.getKindForSyncId(item.id));
    if (kind === PlacesSyncUtils.bookmarks.KINDS.FOLDER) {
      return false;
    }

    // In addition to preventing the deletion of this record (handled by the caller),
    // we need to mark the parent of this record for uploading next sync, in order
    // to ensure its children array is accurate.
    let modifiedTimestamp = this._modified.getModifiedTimestamp(item.id);
    if (!modifiedTimestamp) {
      // We only expect this to be called with items locally modified, so
      // something strange is going on - play it safe and don't revive it.
      this._log.error("_shouldReviveRemotelyDeletedRecord called on unmodified item: " + item.id);
      return false;
    }

    let localID = this._store.idForGUID(item.id);
    let localParentID = PlacesUtils.bookmarks.getFolderIdForItem(localID);
    let localParentSyncID = this._store.GUIDForId(localParentID);

    this._log.trace(`Reviving item "${item.id}" and marking parent ${localParentSyncID} as modified.`);

    if (!this._modified.has(localParentSyncID)) {
      this._modified.set(localParentSyncID, {
        timestamp: modifiedTimestamp,
        deleted: false
      });
    }
    return true
  },

  _processIncoming: function (newitems) {
    try {
      SyncEngine.prototype._processIncoming.call(this, newitems);
    } finally {
      try {
        this._deletePending();
      } finally {
        // Reorder children.
        this._store._orderChildren();
        delete this._store._childrenToOrder;
      }
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
  },

  pullAllChanges() {
    return new BookmarksChangeset(this._store.getAllIDs());
  },

  pullNewChanges() {
    let modifiedGUIDs = this._getModifiedGUIDs();
    if (!modifiedGUIDs.length) {
      return new BookmarksChangeset(this._tracker.changedIDs);
    }

    // We don't use `PlacesUtils.promiseDBConnection` here because
    // `getChangedIDs` might be called while we're in a batch, meaning we
    // won't see any changes until the batch finishes and the transaction
    // commits.
    let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                        .DBConnection;

    // Filter out tags, organizer queries, and other descendants that we're
    // not tracking. We chunk `modifiedGUIDs` because SQLite limits the number
    // of bound parameters per query.
    for (let startIndex = 0;
         startIndex < modifiedGUIDs.length;
         startIndex += SQLITE_MAX_VARIABLE_NUMBER) {

      let chunkLength = Math.min(SQLITE_MAX_VARIABLE_NUMBER,
                                 modifiedGUIDs.length - startIndex);

      let query = `
        WITH RECURSIVE
        modifiedGuids(guid) AS (
          VALUES ${new Array(chunkLength).fill("(?)").join(", ")}
        ),
        syncedItems(id) AS (
          VALUES ${getChangeRootIds().map(id => `(${id})`).join(", ")}
          UNION ALL
          SELECT b.id
          FROM moz_bookmarks b
          JOIN syncedItems s ON b.parent = s.id
        )
        SELECT b.guid
        FROM modifiedGuids m
        JOIN moz_bookmarks b ON b.guid = m.guid
        LEFT JOIN syncedItems s ON b.id = s.id
        WHERE s.id IS NULL
      `;

      let statement = db.createAsyncStatement(query);
      try {
        for (let i = 0; i < chunkLength; i++) {
          statement.bindByIndex(i, modifiedGUIDs[startIndex + i]);
        }
        let results = Async.querySpinningly(statement, ["guid"]);
        for (let { guid } of results) {
          let syncID = PlacesSyncUtils.bookmarks.guidToSyncId(guid);
          this._tracker.removeChangedID(syncID);
        }
      } finally {
        statement.finalize();
      }
    }

    return new BookmarksChangeset(this._tracker.changedIDs);
  },

  // Returns an array of Places GUIDs for all changed items. Ignores deletions,
  // which won't exist in the DB and shouldn't be removed from the tracker.
  _getModifiedGUIDs() {
    let guids = [];
    for (let syncID in this._tracker.changedIDs) {
      if (this._tracker.changedIDs[syncID].deleted === true) {
        // The `===` check also filters out old persisted timestamps,
        // which won't have a `deleted` property.
        continue;
      }
      let guid = PlacesSyncUtils.bookmarks.syncIdToGuid(syncID);
      guids.push(guid);
    }
    return guids;
  },

  // Called when _findDupe returns a dupe item and the engine has decided to
  // switch the existing item to the new incoming item.
  _switchItemToDupe(localDupeGUID, incomingItem) {
    // We unconditionally change the item's ID in case the engine knows of
    // an item but doesn't expose it through itemExists. If the API
    // contract were stronger, this could be changed.
    this._log.debug("Switching local ID to incoming: " + localDupeGUID + " -> " +
                    incomingItem.id);
    this._store.changeItemID(localDupeGUID, incomingItem.id);

    // And mark the parent as being modified. Given we de-dupe based on the
    // parent *name* it's possible the item having its GUID changed has a
    // different parent from the incoming record.
    // So we need to find the GUID of the local parent.
    let now = this._tracker._now();
    let localID = this._store.idForGUID(incomingItem.id);
    let localParentID = PlacesUtils.bookmarks.getFolderIdForItem(localID);
    let localParentGUID = this._store.GUIDForId(localParentID);
    this._modified.set(localParentGUID, { modified: now, deleted: false });

    // And we also add the parent as reflected in the incoming record as the
    // de-dupe process might have used an existing item in a different folder.
    // But only if the parent exists, otherwise we will upload a deleted item
    // when it might actually be valid, just unknown to us. Note that this
    // scenario will still leave us with inconsistent client and server states;
    // the incoming record on the server references a parent that isn't the
    // actual parent locally - see bug 1297955.
    if (localParentGUID != incomingItem.parentid) {
      let remoteParentID = this._store.idForGUID(incomingItem.parentid);
      if (remoteParentID > 0) {
        // The parent specified in the record does exist, so we are going to
        // attempt a move when we come to applying the record. Mark the parent
        // as being modified so we will later upload it with the new child
        // reference.
        this._modified.set(incomingItem.parentid, { modified: now, deleted: false });
      } else {
        // We aren't going to do a move as we don't have the parent (yet?).
        // When applying the record we will add our special PARENT_ANNO
        // annotation, so if it arrives in the future (either this Sync or a
        // later one) it will be reparented.
        this._log.debug(`Incoming duplicate item ${incomingItem.id} specifies ` +
                        `non-existing parent ${incomingItem.parentid}`);
      }
    }

    // The local, duplicate ID is always deleted on the server - but for
    // bookmarks it is a logical delete.
    // Simply adding this (now non-existing) ID to the tracker is enough.
    this._modified.set(localDupeGUID, { modified: now, deleted: true });
  },

  // Cleans up the Places root, reading list items (ignored in bug 762118,
  // removed in bug 1155684), and pinned sites.
  _shouldDeleteRemotely(incomingItem) {
    return FORBIDDEN_INCOMING_IDS.includes(incomingItem.id) ||
           FORBIDDEN_INCOMING_PARENT_IDS.includes(incomingItem.parentid);
  },

  getValidator() {
    return new BookmarkValidator();
  }
};

function BookmarksStore(name, engine) {
  Store.call(this, name, engine);
  this._foldersToDelete = new Set();
  this._atomsToDelete = new Set();
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
    return this.idForGUID(id) > 0;
  },

  applyIncoming: function BStore_applyIncoming(record) {
    this._log.debug("Applying record " + record.id);
    let isSpecial = PlacesSyncUtils.bookmarks.ROOTS.includes(record.id);

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
    this._log.debug("Remote parent is " + parentGUID);

    // Do the normal processing of incoming records
    Store.prototype.applyIncoming.call(this, record);

    if (record.type == "folder" && record.children) {
      this._childrenToOrder[record.id] = record.children;
    }
  },

  create: function BStore_create(record) {
    let info = record.toSyncBookmark();
    // This can throw if we're inserting an invalid or incomplete bookmark.
    // That's fine; the exception will be caught by `applyIncomingBatch`
    // without aborting further processing.
    let item = Async.promiseSpinningly(PlacesSyncUtils.bookmarks.insert(info));
    if (item) {
      this._log.debug(`Created ${item.kind} ${item.syncId} under ${
        item.parentSyncId}`, item);
    }
  },

  remove: function BStore_remove(record) {
    if (PlacesSyncUtils.bookmarks.isRootSyncID(record.id)) {
      this._log.warn("Refusing to remove special folder " + record.id);
      return;
    }
    let recordKind = Async.promiseSpinningly(
      PlacesSyncUtils.bookmarks.getKindForSyncId(record.id));
    let isFolder = recordKind === PlacesSyncUtils.bookmarks.KINDS.FOLDER;
    this._log.trace(`Buffering removal of item "${record.id}" of type "${recordKind}".`);
    if (isFolder) {
      this._foldersToDelete.add(record.id);
    } else {
      this._atomsToDelete.add(record.id);
    }
  },

  update: function BStore_update(record) {
    let info = record.toSyncBookmark();
    let item = Async.promiseSpinningly(PlacesSyncUtils.bookmarks.update(info));
    if (item) {
      this._log.debug(`Updated ${item.kind} ${item.syncId} under ${
        item.parentSyncId}`, item);
    }
  },

  _orderChildren: function _orderChildren() {
    let promises = Object.keys(this._childrenToOrder).map(syncID => {
      let children = this._childrenToOrder[syncID];
      return PlacesSyncUtils.bookmarks.order(syncID, children).catch(ex => {
        this._log.debug(`Could not order children for ${syncID}`, ex);
      });
    });
    Async.promiseSpinningly(Promise.all(promises));
  },

  // There's some complexity here around pending deletions. Our goals:
  //
  // - Don't delete any bookmarks a user has created but not explicitly deleted
  //   (This includes any bookmark that was not a child of the folder at the
  //   time the deletion was recorded, and also bookmarks restored from a backup).
  // - Don't undelete any bookmark without ensuring the server structure
  //   includes it (see `BookmarkEngine.prototype._shouldReviveRemotelyDeletedRecord`)
  //
  // This leads the following approach:
  //
  // - Additions, moves, and updates are processed before deletions.
  //     - To do this, all deletion operations are buffered during a sync. Folders
  //       we plan on deleting have their sync id's stored in `this._foldersToDelete`,
  //       and non-folders we plan on deleting have their sync id's stored in
  //       `this._atomsToDelete`.
  //     - The exception to this is the moves that occur to fix the order of bookmark
  //       children, which are performed after we process deletions.
  // - Non-folders are deleted before folder deletions, so that when we process
  //   folder deletions we know the correct state.
  // - Remote deletions always win for folders, but do not result in recursive
  //   deletion of children. This is a hack because we're not able to distinguish
  //   between value changes and structural changes to folders, and we don't even
  //   have the old server record to compare to. See `BookmarkEngine`'s
  //   `_shouldReviveRemotelyDeletedRecord` method.
  // - When a folder is deleted, its remaining children are moved in order to
  //   their closest living ancestor.  If this is interrupted (unlikely, but
  //   possible given that we don't perform this operation in a transaction),
  //   we revive the folder.
  // - Remote deletions can lose for non-folders, but only until we handle
  //   bookmark restores correctly (removing stale state from the server -- this
  //   is to say, if bug 1230011 is fixed, we should never revive bookmarks).

  deletePending: Task.async(function* deletePending() {
    yield this._deletePendingAtoms();
    let guidsToUpdate = yield this._deletePendingFolders();
    this.clearPendingDeletions();
    return guidsToUpdate;
  }),

  clearPendingDeletions() {
    this._foldersToDelete.clear();
    this._atomsToDelete.clear();
  },

  _deleteAtom: Task.async(function* _deleteAtom(syncID) {
    try {
      let info = yield PlacesSyncUtils.bookmarks.remove(syncID, {
        preventRemovalOfNonEmptyFolders: true
      });
      this._log.trace(`Removed item ${syncID} with type ${info.type}`);
    } catch (ex) {
      // Likely already removed.
      this._log.trace(`Error removing ${syncID}`, ex);
    }
  }),

  _deletePendingAtoms() {
    return Promise.all(
      [...this._atomsToDelete.values()]
        .map(syncID => this._deleteAtom(syncID)));
  },

  // Returns an array of sync ids that need updates.
  _deletePendingFolders: Task.async(function* _deletePendingFolders() {
    // To avoid data loss, we don't want to just delete the folder outright,
    // so we buffer folder deletions and process them at the end (now).
    //
    // At this point, any member in the folder that remains is either a folder
    // pending deletion (which we'll get to in this function), or an item that
    // should not be deleted. To avoid deleting these items, we first move them
    // to the parent of the folder we're about to delete.
    let needUpdate = new Set();
    for (let syncId of this._foldersToDelete) {
      let childSyncIds = yield PlacesSyncUtils.bookmarks.fetchChildSyncIds(syncId);
      if (!childSyncIds.length) {
        // No children -- just delete the folder.
        yield this._deleteAtom(syncId)
        continue;
      }
      // We could avoid some redundant work here by finding the nearest
      // grandparent who isn't present in `this._toDelete`...

      let grandparentSyncId = this.GUIDForId(
        PlacesUtils.bookmarks.getFolderIdForItem(
          this.idForGUID(PlacesSyncUtils.bookmarks.syncIdToGuid(syncId))));

      this._log.trace(`Moving ${childSyncIds.length} children of "${syncId}" to ` +
                      `grandparent "${grandparentSyncId}" before deletion.`);

      // Move children out of the parent and into the grandparent
      yield Promise.all(childSyncIds.map(child => PlacesSyncUtils.bookmarks.update({
        syncId: child,
        parentSyncId: grandparentSyncId
      })));

      // Delete the (now empty) parent
      try {
        yield PlacesSyncUtils.bookmarks.remove(syncId, {
          preventRemovalOfNonEmptyFolders: true
        });
      } catch (e) {
        // We failed, probably because someone added something to this folder
        // between when we got the children and now (or the database is corrupt,
        // or something else happened...) This is unlikely, but possible. To
        // avoid corruption in this case, we need to reupload the record to the
        // server.
        //
        // (Ideally this whole operation would be done in a transaction, and this
        // wouldn't be possible).
        needUpdate.add(syncId);
      }

      // Add children (for parentid) and grandparent (for children list) to set
      // of records needing an update, *unless* they're marked for deletion.
      if (!this._foldersToDelete.has(grandparentSyncId)) {
        needUpdate.add(grandparentSyncId);
      }
      for (let childSyncId of childSyncIds) {
        if (!this._foldersToDelete.has(childSyncId)) {
          needUpdate.add(childSyncId);
        }
      }
    }
    return [...needUpdate];
  }),

  changeItemID: function BStore_changeItemID(oldID, newID) {
    this._log.debug("Changing GUID " + oldID + " to " + newID);

    Async.promiseSpinningly(PlacesSyncUtils.bookmarks.changeGuid(oldID, newID));
  },

  // Create a record starting from the weave id (places guid)
  createRecord: function createRecord(id, collection) {
    let item = Async.promiseSpinningly(PlacesSyncUtils.bookmarks.fetch(id));
    if (!item) { // deleted item
      let record = new PlacesItem(collection, id);
      record.deleted = true;
      return record;
    }

    let recordObj = getTypeObject(item.kind);
    if (!recordObj) {
      this._log.warn("Unknown item type, cannot serialize: " + item.kind);
      recordObj = PlacesItem;
    }
    let record = new recordObj(collection, id);
    record.fromSyncBookmark(item);

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
    let guid = Async.promiseSpinningly(PlacesUtils.promiseItemGuid(id));
    return PlacesSyncUtils.bookmarks.guidToSyncId(guid);
  },

  idForGUID: function idForGUID(guid) {
    // guid might be a String object rather than a string.
    guid = PlacesSyncUtils.bookmarks.syncIdToGuid(guid.toString());

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

  getAllIDs: function BStore_getAllIDs() {
    let items = {};

    let query = `
      WITH RECURSIVE
      changeRootContents(id) AS (
        VALUES ${getChangeRootIds().map(id => `(${id})`).join(", ")}
        UNION ALL
        SELECT b.id
        FROM moz_bookmarks b
        JOIN changeRootContents c ON b.parent = c.id
      )
      SELECT guid
      FROM changeRootContents
      JOIN moz_bookmarks USING (id)
    `;

    let statement = this._getStmt(query);
    let results = Async.querySpinningly(statement, ["guid"]);
    for (let { guid } of results) {
      let syncID = PlacesSyncUtils.bookmarks.guidToSyncId(guid);
      items[syncID] = { modified: 0, deleted: false };
    }

    return items;
  },

  wipe: function BStore_wipe() {
    this.clearPendingDeletions();
    Async.promiseSpinningly(Task.spawn(function* () {
      // Save a backup before clearing out all bookmarks.
      yield PlacesBackups.create(null, true);
      yield PlacesUtils.bookmarks.eraseEverything({
        source: SOURCE_SYNC,
      });
    }));
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

  //`_ignore` checks the change source for each observer notification, so we
  // don't want to let the engine ignore all changes during a sync.
  get ignoreAll() {
    return false;
  },

  // Define an empty setter so that the engine doesn't throw a `TypeError`
  // setting a read-only property.
  set ignoreAll(value) {},

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
        break;
      case "bookmarks-restore-success":
        this._log.debug("Tracking all items on successful import.");

        this._log.debug("Restore succeeded: wiping server and other clients.");
        this.engine.service.resetClient([this.name]);
        this.engine.service.wipeServer([this.name]);
        this.engine.service.clientsEngine.sendCommand("wipeEngine", [this.name]);
        break;
      case "bookmarks-restore-failed":
        this._log.debug("Tracking all items on failed import.");
        break;
    }
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavBookmarkObserver,
    Ci.nsINavBookmarkObserver_MOZILLA_1_9_1_ADDITIONS,
    Ci.nsISupportsWeakReference
  ]),

  addChangedID(id, change) {
    if (!id) {
      this._log.warn("Attempted to add undefined ID to tracker");
      return false;
    }
    if (this._ignored.includes(id)) {
      return false;
    }
    let shouldSaveChange = false;
    let currentChange = this.changedIDs[id];
    if (currentChange) {
      if (typeof currentChange == "number") {
        // Allow raw timestamps for backward-compatibility with persisted
        // changed IDs. The new format uses tuples to track deleted items.
        shouldSaveChange = currentChange < change.modified;
      } else {
        shouldSaveChange = currentChange.modified < change.modified ||
                           currentChange.deleted != change.deleted;
      }
    } else {
      shouldSaveChange = true;
    }
    if (shouldSaveChange) {
      this._saveChangedID(id, change);
    }
    return true;
  },

  /**
   * Add a bookmark GUID to be uploaded and bump up the sync score.
   *
   * @param itemId
   *        The Places item ID of the bookmark to upload.
   * @param guid
   *        The Places GUID of the bookmark to upload.
   * @param isTombstone
   *        Whether we're uploading a tombstone for a removed bookmark.
   */
  _add: function BMT__add(itemId, guid, isTombstone = false) {
    let syncID = PlacesSyncUtils.bookmarks.guidToSyncId(guid);
    let info = { modified: Date.now() / 1000, deleted: isTombstone };
    if (this.addChangedID(syncID, info)) {
      this._upScore();
    }
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

  onItemAdded: function BMT_onItemAdded(itemId, folder, index,
                                        itemType, uri, title, dateAdded,
                                        guid, parentGuid, source) {
    if (IGNORED_SOURCES.includes(source)) {
      return;
    }

    this._log.trace("onItemAdded: " + itemId);
    this._add(itemId, guid);
    this._add(folder, parentGuid);
  },

  onItemRemoved: function (itemId, parentId, index, type, uri,
                           guid, parentGuid, source) {
    if (IGNORED_SOURCES.includes(source)) {
      return;
    }

    // Ignore changes to tags (folders under the tags folder).
    if (parentId == PlacesUtils.tagsFolderId) {
      return;
    }

    let grandParentId = -1;
    try {
      grandParentId = PlacesUtils.bookmarks.getFolderIdForItem(parentId);
    } catch (ex) {
      // `getFolderIdForItem` can throw if the item no longer exists, such as
      // when we've removed a subtree using `removeFolderChildren`.
      return;
    }

    // Ignore tag items (the actual instance of a tag for a bookmark).
    if (grandParentId == PlacesUtils.tagsFolderId) {
      return;
    }

    /**
     * The above checks are incomplete: we can still write tombstones for
     * items that we don't track, and upload extraneous roots.
     *
     * Consider the left pane root: it's a child of the Places root, and has
     * children and grandchildren. `PlacesUIUtils` can create, delete, and
     * recreate it as needed. We can't determine ancestors when the root or its
     * children are deleted, because they've already been removed from the
     * database when `onItemRemoved` is called. Likewise, we can't check their
     * "exclude from backup" annos, because they've *also* been removed.
     *
     * So, we end up writing tombstones for the left pane queries and left
     * pane root. For good measure, we'll also upload the Places root, because
     * it's the parent of the left pane root.
     *
     * As a workaround, we can track the parent GUID and reconstruct the item's
     * ancestry at sync time. This is complicated, and the previous behavior was
     * already wrong, so we'll wait for bug 1258127 to fix this generally.
     */
    this._log.trace("onItemRemoved: " + itemId);
    this._add(itemId, guid, /* isTombstone */ true);
    this._add(parentId, parentGuid);
  },

  _ensureMobileQuery: function _ensureMobileQuery() {
    let find = val =>
      PlacesUtils.annotations.getItemsWithAnnotation(ORGANIZERQUERY_ANNO, {}).filter(
        id => PlacesUtils.annotations.getItemAnnotation(id, ORGANIZERQUERY_ANNO) == val
      );

    // Don't continue if the Library isn't ready
    let all = find(ALLBOOKMARKS_ANNO);
    if (all.length == 0)
      return;

    let mobile = find(MOBILE_ANNO);
    let queryURI = Utils.makeURI("place:folder=" + PlacesUtils.mobileFolderId);
    let title = PlacesBundle.GetStringFromName("MobileBookmarksFolderTitle");

    // Don't add OR remove the mobile bookmarks if there's nothing.
    if (PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.mobileFolderId, 0) == -1) {
      if (mobile.length != 0)
        PlacesUtils.bookmarks.removeItem(mobile[0], SOURCE_SYNC);
    }
    // Add the mobile bookmarks query if it doesn't exist
    else if (mobile.length == 0) {
      let query = PlacesUtils.bookmarks.insertBookmark(all[0], queryURI, -1, title, /* guid */ null, SOURCE_SYNC);
      PlacesUtils.annotations.setItemAnnotation(query, ORGANIZERQUERY_ANNO, MOBILE_ANNO, 0,
                                  PlacesUtils.annotations.EXPIRE_NEVER, SOURCE_SYNC);
      PlacesUtils.annotations.setItemAnnotation(query, PlacesUtils.EXCLUDE_FROM_BACKUP_ANNO, 1, 0,
                                  PlacesUtils.annotations.EXPIRE_NEVER, SOURCE_SYNC);
    }
    // Make sure the existing query URL and title are correct
    else {
      if (!PlacesUtils.bookmarks.getBookmarkURI(mobile[0]).equals(queryURI)) {
        PlacesUtils.bookmarks.changeBookmarkURI(mobile[0], queryURI,
                                                SOURCE_SYNC);
      }
      let queryTitle = PlacesUtils.bookmarks.getItemTitle(mobile[0]);
      if (queryTitle != title) {
        PlacesUtils.bookmarks.setItemTitle(mobile[0], title, SOURCE_SYNC);
      }
      let rootTitle =
        PlacesUtils.bookmarks.getItemTitle(PlacesUtils.mobileFolderId);
      if (rootTitle != title) {
        PlacesUtils.bookmarks.setItemTitle(PlacesUtils.mobileFolderId, title,
                                           SOURCE_SYNC);
      }
    }
  },

  // This method is oddly structured, but the idea is to return as quickly as
  // possible -- this handler gets called *every time* a bookmark changes, for
  // *each change*.
  onItemChanged: function BMT_onItemChanged(itemId, property, isAnno, value,
                                            lastModified, itemType, parentId,
                                            guid, parentGuid, oldValue,
                                            source) {
    if (IGNORED_SOURCES.includes(source)) {
      return;
    }

    if (isAnno && (ANNOS_TO_TRACK.indexOf(property) == -1))
      // Ignore annotations except for the ones that we sync.
      return;

    // Ignore favicon changes to avoid unnecessary churn.
    if (property == "favicon")
      return;

    this._log.trace("onItemChanged: " + itemId +
                    (", " + property + (isAnno? " (anno)" : "")) +
                    (value ? (" = \"" + value + "\"") : ""));
    this._add(itemId, guid);
  },

  onItemMoved: function BMT_onItemMoved(itemId, oldParent, oldIndex,
                                        newParent, newIndex, itemType,
                                        guid, oldParentGuid, newParentGuid,
                                        source) {
    if (IGNORED_SOURCES.includes(source)) {
      return;
    }

    this._log.trace("onItemMoved: " + itemId);
    this._add(oldParent, oldParentGuid);
    if (oldParent != newParent) {
      this._add(itemId, guid);
      this._add(newParent, newParentGuid);
    }

    // Remove any position annotations now that the user moved the item
    PlacesUtils.annotations.removeItemAnnotation(itemId,
      PlacesSyncUtils.bookmarks.SYNC_PARENT_ANNO, SOURCE_SYNC);
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

// Returns an array of root IDs to recursively query for synced bookmarks.
// Items in other roots, including tags and organizer queries, will be
// ignored.
function getChangeRootIds() {
  return [
    PlacesUtils.bookmarksMenuFolderId,
    PlacesUtils.toolbarFolderId,
    PlacesUtils.unfiledBookmarksFolderId,
    PlacesUtils.mobileFolderId,
  ];
}

class BookmarksChangeset extends Changeset {
  getModifiedTimestamp(id) {
    let change = this.changes[id];
    return change ? change.modified : Number.NaN;
  }
}
