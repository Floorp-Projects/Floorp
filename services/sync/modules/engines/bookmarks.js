/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["BookmarksEngine", "PlacesItem", "Bookmark",
                         "BookmarkFolder", "BookmarkQuery",
                         "Livemark", "BookmarkSeparator"];

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BookmarkValidator",
                                  "resource://services-sync/bookmark_validator.js");
XPCOMUtils.defineLazyGetter(this, "PlacesBundle", () => {
  let bundleService = Cc["@mozilla.org/intl/stringbundle;1"]
                        .getService(Ci.nsIStringBundleService);
  return bundleService.createBundle("chrome://places/locale/places.properties");
});
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesSyncUtils",
                                  "resource://gre/modules/PlacesSyncUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesBackups",
                                  "resource://gre/modules/PlacesBackups.jsm");

const ANNOS_TO_TRACK = [PlacesSyncUtils.bookmarks.DESCRIPTION_ANNO,
                        PlacesSyncUtils.bookmarks.SIDEBAR_ANNO,
                        PlacesUtils.LMANNO_FEEDURI, PlacesUtils.LMANNO_SITEURI];

const SERVICE_NOT_SUPPORTED = "Service not supported on this platform";
const FOLDER_SORTINDEX = 1000000;
const {
  SOURCE_SYNC,
  SOURCE_IMPORT,
  SOURCE_IMPORT_REPLACE,
  SOURCE_SYNC_REPARENT_REMOVED_FOLDER_CHILDREN,
} = Ci.nsINavBookmarksService;

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
const IGNORED_SOURCES = [SOURCE_SYNC, SOURCE_IMPORT, SOURCE_IMPORT_REPLACE,
                         SOURCE_SYNC_REPARENT_REMOVED_FOLDER_CHILDREN];

function isSyncedRootNode(node) {
  return node.root == "bookmarksMenuFolder" ||
         node.root == "unfiledBookmarksFolder" ||
         node.root == "toolbarFolder" ||
         node.root == "mobileFolder";
}

// Returns the constructor for a bookmark record type.
function getTypeObject(type) {
  switch (type) {
    case "bookmark":
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
    let result = {
      kind: this.type,
      syncId: this.id,
      parentSyncId: this.parentid,
    };
    let dateAdded = PlacesSyncUtils.bookmarks.ratchetTimestampBackwards(
      this.dateAdded, +this.modified * 1000);
    if (dateAdded !== undefined) {
      result.dateAdded = dateAdded;
    }
    return result;
  },

  // Populates the record from a Sync bookmark object returned from
  // `PlacesSyncUtils.bookmarks.fetch`.
  fromSyncBookmark(item) {
    this.parentid = item.parentSyncId;
    this.parentName = item.parentTitle;
    if (item.dateAdded) {
      this.dateAdded = item.dateAdded;
    }
  },
};

Utils.deferGetSet(PlacesItem,
                  "cleartext",
                  ["hasDupe", "parentid", "parentName", "type", "dateAdded"]);

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
  allowSkippedRecord: false,

  emptyChangeset() {
    return new BookmarksChangeset();
  },


  _guidMapFailed: false,
  _buildGUIDMap: function _buildGUIDMap() {
    let store = this._store;
    let guidMap = {};
    let tree = Async.promiseSpinningly(PlacesUtils.promiseBookmarksTree(""));

    function* walkBookmarksTree(tree, parent = null) {
      if (tree) {
        // Skip root node
        if (parent) {
          yield [tree, parent];
        }
        if (tree.children) {
          for (let child of tree.children) {
            store._sleep(0); // avoid jank while looping.
            yield* walkBookmarksTree(child, tree);
          }
        }
      }
    }

    function* walkBookmarksRoots(tree) {
      for (let child of tree.children) {
        if (isSyncedRootNode(child)) {
          yield* walkBookmarksTree(child, tree);
        }
      }
    }

    for (let [node, parent] of walkBookmarksRoots(tree)) {
      let {guid, type: placeType} = node;
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
            key = "b" + node.uri + ":" + (node.title || "");
          }
          break;
        case PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER:
          // Folder
          key = "f" + (node.title || "");
          break;
        case PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR:
          // Separator
          key = "s" + node.index;
          break;
        default:
          this._log.error("Unknown place type: '" + placeType + "'");
          continue;
      }

      let parentName = parent.title || "";
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
          altKey = "b" + item.bmkUri + ":" + (item.title || "");
          break;
        }
        // No queryID? Fall through to the regular bookmark case.
      case "bookmark":
        key = "b" + item.bmkUri + ":" + (item.title || "");
        break;
      case "folder":
      case "livemark":
        key = "f" + (item.title || "");
        break;
      case "separator":
        key = "s" + item.pos;
        break;
      default:
        return undefined;
    }

    // Figure out if we have a map to use!
    // This will throw in some circumstances. That's fine.
    let guidMap = this._guidMap;

    // Give the GUID if we have the matching pair.
    let parentName = item.parentName || "";
    this._log.trace("Finding mapping: " + parentName + ", " + key);
    let parent = guidMap[parentName];

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
    if (newlyModified) {
      this._log.debug("Deleted pending items", newlyModified);
      this._modified.insert(newlyModified);
    }
  },

  _shouldReviveRemotelyDeletedRecord(item) {
    let modifiedTimestamp = this._modified.getModifiedTimestamp(item.id);
    if (!modifiedTimestamp) {
      // We only expect this to be called with items locally modified, so
      // something strange is going on - play it safe and don't revive it.
      this._log.error("_shouldReviveRemotelyDeletedRecord called on unmodified item: " + item.id);
      return false;
    }

    // In addition to preventing the deletion of this record (handled by the caller),
    // we use `touch` to mark the parent of this record for uploading next sync, in order
    // to ensure its children array is accurate. If `touch` returns new change records,
    // we revive the item and insert the changes into the current changeset.
    let newChanges = Async.promiseSpinningly(PlacesSyncUtils.bookmarks.touch(item.id));
    if (newChanges) {
      this._modified.insert(newChanges);
      return true;
    }
    return false;
  },

  _processIncoming(newitems) {
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
    if (this._modified.isTombstone(id)) {
      // If we already know a changed item is a tombstone, just create the
      // record without dipping into Places.
      return this._createTombstone(id);
    }
    // Create the record as usual, but mark it as having dupes if necessary.
    let record = SyncEngine.prototype._createRecord.call(this, id);
    let entry = this._mapDupe(record);
    if (entry != null && entry.hasDupe) {
      record.hasDupe = true;
    }
    if (record.deleted) {
      // Make sure deleted items are marked as tombstones. We do this here
      // in addition to the `isTombstone` call above because it's possible
      // a changed bookmark might be deleted during a sync (bug 1313967).
      this._modified.setTombstone(record.id);
    }
    return record;
  },

  buildWeakReuploadMap(idSet) {
    // We want to avoid uploading records which have changed, since that could
    // cause an inconsistent state on the server.
    //
    // Strictly speaking, it would be correct to just call getChangedIds() after
    // building the initial weak reupload map, however this is quite slow, since
    // we might end up doing createRecord() (which runs at least one, and
    // sometimes multiple database queries) for a potentially large number of
    // items.
    //
    // Since the call to getChangedIds is relatively cheap, we do it once before
    // building the weakReuploadMap (which is where the calls to createRecord()
    // occur) as an optimization, and once after for correctness, to handle the
    // unlikely case that a record was modified while we were building the map.
    let initialChanges = Async.promiseSpinningly(PlacesSyncUtils.bookmarks.getChangedIds());
    for (let changed of initialChanges) {
      idSet.delete(changed);
    }

    let map = SyncEngine.prototype.buildWeakReuploadMap.call(this, idSet);
    let changes = Async.promiseSpinningly(PlacesSyncUtils.bookmarks.getChangedIds());
    for (let id of changes) {
      map.delete(id);
    }
    return map;
  },

  _findDupe: function _findDupe(item) {
    this._log.trace("Finding dupe for " + item.id +
                    " (already duped: " + item.hasDupe + ").");

    // Don't bother finding a dupe if the incoming item has duplicates.
    if (item.hasDupe) {
      this._log.trace(item.id + " already a dupe: not finding one.");
      return null;
    }
    let mapped = this._mapDupe(item);
    this._log.debug(item.id + " mapped to " + mapped);
    // We must return a string, not an object, and the entries in the GUIDMap
    // are created via "new String()" making them an object.
    return mapped ? mapped.toString() : mapped;
  },

  pullAllChanges() {
    return this.pullNewChanges();
  },

  pullNewChanges() {
    return Async.promiseSpinningly(this._tracker.promiseChangedIDs());
  },

  trackRemainingChanges() {
    let changes = this._modified.changes;
    Async.promiseSpinningly(PlacesSyncUtils.bookmarks.pushChanges(changes));
  },

  _deleteId(id) {
    this._noteDeletedId(id);
  },

  _resetClient() {
    SyncEngine.prototype._resetClient.call(this);
    Async.promiseSpinningly(PlacesSyncUtils.bookmarks.reset());
  },

  // Called when _findDupe returns a dupe item and the engine has decided to
  // switch the existing item to the new incoming item.
  _switchItemToDupe(localDupeGUID, incomingItem) {
    let newChanges = Async.promiseSpinningly(PlacesSyncUtils.bookmarks.dedupe(
      localDupeGUID, incomingItem.id, incomingItem.parentid));
    this._modified.insert(newChanges);
  },

  // Cleans up the Places root, reading list items (ignored in bug 762118,
  // removed in bug 1155684), and pinned sites.
  _shouldDeleteRemotely(incomingItem) {
    return FORBIDDEN_INCOMING_IDS.includes(incomingItem.id) ||
           FORBIDDEN_INCOMING_PARENT_IDS.includes(incomingItem.parentid);
  },

  beforeRecordDiscard(record) {
    let isSpecial = PlacesSyncUtils.bookmarks.ROOTS.includes(record.id);
    if (isSpecial && record.children && !this._store._childrenToOrder[record.id]) {
      if (this._modified.getStatus(record.id) != PlacesUtils.bookmarks.SYNC_STATUS.NEW) {
        return;
      }
      this._log.debug("Recording children of " + record.id + " as " + JSON.stringify(record.children));
      this._store._childrenToOrder[record.id] = record.children;
    }
  },

  getValidator() {
    return new BookmarkValidator();
  }
};

function BookmarksStore(name, engine) {
  Store.call(this, name, engine);
  this._itemsToDelete = new Set();
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
      if (item.dateAdded != record.dateAdded) {
        this.engine._needWeakReupload.add(item.syncId);
      }
    }
  },

  remove: function BStore_remove(record) {
    this._log.trace(`Buffering removal of item "${record.id}".`);
    this._itemsToDelete.add(record.id);
  },

  update: function BStore_update(record) {
    let info = record.toSyncBookmark();
    let item = Async.promiseSpinningly(PlacesSyncUtils.bookmarks.update(info));
    if (item) {
      this._log.debug(`Updated ${item.kind} ${item.syncId} under ${
        item.parentSyncId}`, item);
      if (item.dateAdded != record.dateAdded) {
        this.engine._needWeakReupload.add(item.syncId);
      }
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
  //     - To do this, all deletion operations are buffered in `this._itemsToDelete`
  //       during a sync.
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
  //
  // See `PlacesSyncUtils.bookmarks.remove` for the implementation.

  deletePending: Task.async(function* deletePending() {
    let guidsToUpdate = yield PlacesSyncUtils.bookmarks.remove([...this._itemsToDelete]);
    this.clearPendingDeletions();
    return guidsToUpdate;
  }),

  clearPendingDeletions() {
    this._itemsToDelete.clear();
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
      let frecency = Async.promiseSpinningly(PlacesSyncUtils.history.fetchURLFrecency(record.bmkUri));
      if (frecency != -1)
        index += frecency;
    }

    return index;
  },

  wipe: function BStore_wipe() {
    this.clearPendingDeletions();
    Async.promiseSpinningly(Task.spawn(function* () {
      // Save a backup before clearing out all bookmarks.
      yield PlacesBackups.create(null, true);
      yield PlacesSyncUtils.bookmarks.wipe();
    }));
  }
};

// The bookmarks tracker is a special flower. Instead of listening for changes
// via observer notifications, it queries Places for the set of items that have
// changed since the last sync. Because it's a "pull-based" tracker, it ignores
// all concepts of "add a changed ID." However, it still registers an observer
// to bump the score, so that changed bookmarks are synced immediately.
function BookmarksTracker(name, engine) {
  this._batchDepth = 0;
  this._batchSawScoreIncrement = false;
  this._migratedOldEntries = false;
  Tracker.call(this, name, engine);
}
BookmarksTracker.prototype = {
  __proto__: Tracker.prototype,

  // `_ignore` checks the change source for each observer notification, so we
  // don't want to let the engine ignore all changes during a sync.
  get ignoreAll() {
    return false;
  },

  // Define an empty setter so that the engine doesn't throw a `TypeError`
  // setting a read-only property.
  set ignoreAll(value) {},

  // We never want to persist changed IDs, as the changes are already stored
  // in Places.
  persistChangedIDs: false,

  startTracking() {
    PlacesUtils.bookmarks.addObserver(this, true);
    Svc.Obs.add("bookmarks-restore-begin", this);
    Svc.Obs.add("bookmarks-restore-success", this);
    Svc.Obs.add("bookmarks-restore-failed", this);
  },

  stopTracking() {
    PlacesUtils.bookmarks.removeObserver(this);
    Svc.Obs.remove("bookmarks-restore-begin", this);
    Svc.Obs.remove("bookmarks-restore-success", this);
    Svc.Obs.remove("bookmarks-restore-failed", this);
  },

  // Ensure we aren't accidentally using the base persistence.
  addChangedID(id, when) {
    throw new Error("Don't add IDs to the bookmarks tracker");
  },

  removeChangedID(id) {
    throw new Error("Don't remove IDs from the bookmarks tracker");
  },

  // This method is called at various times, so we override with a no-op
  // instead of throwing.
  clearChangedIDs() {},

  promiseChangedIDs() {
    return PlacesSyncUtils.bookmarks.pullChanges();
  },

  get changedIDs() {
    throw new Error("Use promiseChangedIDs");
  },

  set changedIDs(obj) {
    throw new Error("Don't set initial changed bookmark IDs");
  },

  // Migrates tracker entries from the old JSON-based tracker to Places. This
  // is called the first time we start tracking changes.
  _migrateOldEntries: Task.async(function* () {
    let existingIDs = yield Utils.jsonLoad("changes/" + this.file, this);
    if (existingIDs === null) {
      // If the tracker file doesn't exist, we don't need to migrate, even if
      // the engine is enabled. It's possible we're upgrading before the first
      // sync. In the worst case, getting this wrong has the same effect as a
      // restore: we'll reupload everything to the server.
      this._log.debug("migrateOldEntries: Missing bookmarks tracker file; " +
                      "skipping migration");
      return null;
    }

    if (!this._needsMigration()) {
      // We have a tracker file, but bookmark syncing is disabled, or this is
      // the first sync. It's likely the tracker file is stale. Remove it and
      // skip migration.
      this._log.debug("migrateOldEntries: Bookmarks engine disabled or " +
                      "first sync; skipping migration");
      return Utils.jsonRemove("changes/" + this.file, this);
    }

    // At this point, we know the engine is enabled, we have a tracker file
    // (though it may be empty), and we've synced before.
    this._log.debug("migrateOldEntries: Migrating old tracker entries");
    let entries = [];
    for (let syncID in existingIDs) {
      let change = existingIDs[syncID];
      // Allow raw timestamps for backward-compatibility with changed IDs
      // persisted before bug 1274496.
      let timestamp = typeof change == "number" ? change : change.modified;
      entries.push({
        syncId: syncID,
        modified: timestamp * 1000,
      });
    }
    yield PlacesSyncUtils.bookmarks.migrateOldTrackerEntries(entries);
    return Utils.jsonRemove("changes/" + this.file, this);
  }),

  _needsMigration() {
    return this.engine && this.engineIsEnabled() && this.engine.lastSync > 0;
  },

  observe: function observe(subject, topic, data) {
    Tracker.prototype.observe.call(this, subject, topic, data);

    switch (topic) {
      case "weave:engine:start-tracking":
        if (!this._migratedOldEntries) {
          this._migratedOldEntries = true;
          Async.promiseSpinningly(this._migrateOldEntries());
        }
        break;
      case "bookmarks-restore-begin":
        this._log.debug("Ignoring changes from importing bookmarks.");
        break;
      case "bookmarks-restore-success":
        this._log.debug("Tracking all items on successful import.");

        this._log.debug("Restore succeeded: wiping server and other clients.");
        this.engine.service.resetClient([this.name]);
        this.engine.service.wipeServer([this.name]);
        this.engine.service.clientsEngine.sendCommand("wipeEngine", [this.name],
                                                      null, { reason: "bookmark-restore" });
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
    this._upScore();
  },

  onItemRemoved(itemId, parentId, index, type, uri,
                           guid, parentGuid, source) {
    if (IGNORED_SOURCES.includes(source)) {
      return;
    }

    this._log.trace("onItemRemoved: " + itemId);
    this._upScore();
  },

  _ensureMobileQuery: function _ensureMobileQuery() {
    Services.prefs.setBoolPref("browser.bookmarks.showMobileBookmarks", true);
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
    } else if (mobile.length == 0) {
      // Add the mobile bookmarks query if it doesn't exist
      let query = PlacesUtils.bookmarks.insertBookmark(all[0], queryURI, -1, title, /* guid */ null, SOURCE_SYNC);
      PlacesUtils.annotations.setItemAnnotation(query, ORGANIZERQUERY_ANNO, MOBILE_ANNO, 0,
                                  PlacesUtils.annotations.EXPIRE_NEVER, SOURCE_SYNC);
      PlacesUtils.annotations.setItemAnnotation(query, PlacesUtils.EXCLUDE_FROM_BACKUP_ANNO, 1, 0,
                                  PlacesUtils.annotations.EXPIRE_NEVER, SOURCE_SYNC);
    } else {
      // Make sure the existing query URL and title are correct
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
                    (", " + property + (isAnno ? " (anno)" : "")) +
                    (value ? (" = \"" + value + "\"") : ""));
    this._upScore();
  },

  onItemMoved: function BMT_onItemMoved(itemId, oldParent, oldIndex,
                                        newParent, newIndex, itemType,
                                        guid, oldParentGuid, newParentGuid,
                                        source) {
    if (IGNORED_SOURCES.includes(source)) {
      return;
    }

    this._log.trace("onItemMoved: " + itemId);
    this._upScore();
  },

  onBeginUpdateBatch() {
    ++this._batchDepth;
  },
  onEndUpdateBatch() {
    if (--this._batchDepth === 0 && this._batchSawScoreIncrement) {
      this.score += SCORE_INCREMENT_XLARGE;
      this._batchSawScoreIncrement = false;
    }
  },
  onItemVisited() {}
};

class BookmarksChangeset extends Changeset {
  constructor() {
    super();
    // Weak changes are part of the changeset, but don't bump the change
    // counter, and aren't persisted anywhere.
    this.weakChanges = {};
  }

  getStatus(id) {
    let change = this.changes[id];
    if (!change) {
      return PlacesUtils.bookmarks.SYNC_STATUS.UNKNOWN;
    }
    return change.status;
  }

  getModifiedTimestamp(id) {
    let change = this.changes[id];
    if (change) {
      // Pretend the change doesn't exist if we've already synced or
      // reconciled it.
      return change.synced ? Number.NaN : change.modified;
    }
    if (this.weakChanges[id]) {
      // For weak changes, we use a timestamp from long ago to ensure we always
      // prefer the remote version in case of conflicts.
      return 0;
    }
    return Number.NaN;
  }

  setWeak(id, { tombstone = false } = {}) {
    this.weakChanges[id] = { tombstone };
  }

  has(id) {
    let change = this.changes[id];
    if (change) {
      return !change.synced;
    }
    return !!this.weakChanges[id];
  }

  setTombstone(id) {
    let change = this.changes[id];
    if (change) {
      change.tombstone = true;
    }
    let weakChange = this.weakChanges[id];
    if (weakChange) {
      // Not strictly necessary, since we never persist weak changes, but may
      // be useful for bookkeeping.
      weakChange.tombstone = true;
    }
  }

  delete(id) {
    let change = this.changes[id];
    if (change) {
      // Mark the change as synced without removing it from the set. We do this
      // so that we can update Places in `trackRemainingChanges`.
      change.synced = true;
    }
    delete this.weakChanges[id];
  }

  changeID(oldID, newID) {
    super.changeID(oldID, newID);
    this.weakChanges[newID] = this.weakChanges[oldID];
    delete this.weakChanges[oldID];
  }

  ids() {
    let results = new Set();
    for (let id in this.changes) {
      results.add(id);
    }
    for (let id in this.weakChanges) {
      results.add(id);
    }
    return [...results];
  }

  clear() {
    super.clear();
    this.weakChanges = {};
  }

  isTombstone(id) {
    let change = this.changes[id];
    if (change) {
      return change.tombstone;
    }
    let weakChange = this.weakChanges[id];
    if (weakChange) {
      return weakChange.tombstone;
    }
    return false;
  }
}
