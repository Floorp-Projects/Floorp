/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = [
  "BookmarksEngine",
  "PlacesItem",
  "Bookmark",
  "BookmarkFolder",
  "BookmarkQuery",
  "Livemark",
  "BookmarkSeparator",
  "BufferedBookmarksEngine",
];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Async } = ChromeUtils.import("resource://services-common/async.js");
const { SCORE_INCREMENT_XLARGE } = ChromeUtils.import(
  "resource://services-sync/constants.js"
);
const { Changeset, Store, SyncEngine, Tracker } = ChromeUtils.import(
  "resource://services-sync/engines.js"
);
const { CryptoWrapper } = ChromeUtils.import(
  "resource://services-sync/record.js"
);
const { Svc, Utils } = ChromeUtils.import("resource://services-sync/util.js");

XPCOMUtils.defineLazyModuleGetters(this, {
  BookmarkValidator: "resource://services-sync/bookmark_validator.js",
  LiveBookmarkMigrator: "resource:///modules/LiveBookmarkMigrator.jsm",
  Observers: "resource://services-common/observers.js",
  OS: "resource://gre/modules/osfile.jsm",
  PlacesBackups: "resource://gre/modules/PlacesBackups.jsm",
  PlacesDBUtils: "resource://gre/modules/PlacesDBUtils.jsm",
  PlacesSyncUtils: "resource://gre/modules/PlacesSyncUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Resource: "resource://services-sync/resource.js",
  SyncedBookmarksMirror: "resource://gre/modules/SyncedBookmarksMirror.jsm",
});

XPCOMUtils.defineLazyGetter(this, "PlacesBundle", () => {
  return Services.strings.createBundle(
    "chrome://places/locale/places.properties"
  );
});

XPCOMUtils.defineLazyGetter(this, "ANNOS_TO_TRACK", () => [
  PlacesUtils.LMANNO_FEEDURI,
  PlacesUtils.LMANNO_SITEURI,
]);

const PLACES_MAINTENANCE_INTERVAL_SECONDS = 4 * 60 * 60; // 4 hours.

const FOLDER_SORTINDEX = 1000000;

// Roots that should be deleted from the server, instead of applied locally.
// This matches `AndroidBrowserBookmarksRepositorySession::forbiddenGUID`,
// but allows tags because we don't want to reparent tag folders or tag items
// to "unfiled".
const FORBIDDEN_INCOMING_IDS = ["pinned", "places", "readinglist"];

// Items with these parents should be deleted from the server. We allow
// children of the Places root, to avoid orphaning left pane queries and other
// descendants of custom roots.
const FORBIDDEN_INCOMING_PARENT_IDS = ["pinned", "readinglist"];

// The tracker ignores changes made by import and restore, to avoid bumping the
// score and triggering syncs during the process, as well as changes made by
// Sync.
XPCOMUtils.defineLazyGetter(this, "IGNORED_SOURCES", () => [
  PlacesUtils.bookmarks.SOURCES.SYNC,
  PlacesUtils.bookmarks.SOURCES.IMPORT,
  PlacesUtils.bookmarks.SOURCES.RESTORE,
  PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP,
  PlacesUtils.bookmarks.SOURCES.SYNC_REPARENT_REMOVED_FOLDER_CHILDREN,
]);

// The validation telemetry version for the buffered engine. Version 1 is
// collected by `bookmark_validator.js`, and checks value as well as structure
// differences. Version 2 is collected by the buffered engine as part of
// building the remote tree, and checks structure differences only.
const BUFFERED_BOOKMARK_VALIDATOR_VERSION = 2;

// The maximum time that the buffered engine should wait before aborting a
// bookmark merge.
const BUFFERED_BOOKMARK_APPLY_TIMEOUT_MS = 5 * 60 * 60 * 1000; // 5 minutes

function isSyncedRootNode(node) {
  return (
    node.root == "bookmarksMenuFolder" ||
    node.root == "unfiledBookmarksFolder" ||
    node.root == "toolbarFolder" ||
    node.root == "mobileFolder"
  );
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

function PlacesItem(collection, id, type) {
  CryptoWrapper.call(this, collection, id);
  this.type = type || "item";
}
PlacesItem.prototype = {
  async decrypt(keyBundle) {
    // Do the normal CryptoWrapper decrypt, but change types before returning
    let clear = await CryptoWrapper.prototype.decrypt.call(this, keyBundle);

    // Convert the abstract places item to the actual object type
    if (!this.deleted) {
      this.__proto__ = this.getTypeObject(this.type).prototype;
    }

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
      recordId: this.id,
      parentRecordId: this.parentid,
    };
    let dateAdded = PlacesSyncUtils.bookmarks.ratchetTimestampBackwards(
      this.dateAdded,
      +this.modified * 1000
    );
    if (dateAdded > 0) {
      result.dateAdded = dateAdded;
    }
    return result;
  },

  // Populates the record from a Sync bookmark object returned from
  // `PlacesSyncUtils.bookmarks.fetch`.
  fromSyncBookmark(item) {
    this.parentid = item.parentRecordId;
    this.parentName = item.parentTitle;
    if (item.dateAdded) {
      this.dateAdded = item.dateAdded;
    }
  },
};

Utils.deferGetSet(PlacesItem, "cleartext", [
  "hasDupe",
  "parentid",
  "parentName",
  "type",
  "dateAdded",
]);

function Bookmark(collection, id, type) {
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
    info.tags = this.tags;
    info.keyword = this.keyword;
    return info;
  },

  fromSyncBookmark(item) {
    PlacesItem.prototype.fromSyncBookmark.call(this, item);
    this.title = item.title;
    this.bmkUri = item.url.href;
    this.description = item.description;
    this.tags = item.tags;
    this.keyword = item.keyword;
  },
};

Utils.deferGetSet(Bookmark, "cleartext", [
  "title",
  "bmkUri",
  "description",
  "tags",
  "keyword",
]);

function BookmarkQuery(collection, id) {
  Bookmark.call(this, collection, id, "query");
}
BookmarkQuery.prototype = {
  __proto__: Bookmark.prototype,
  _logName: "Sync.Record.BookmarkQuery",

  toSyncBookmark() {
    let info = Bookmark.prototype.toSyncBookmark.call(this);
    info.folder = this.folderName || undefined; // empty string -> undefined
    info.query = this.queryId;
    return info;
  },

  fromSyncBookmark(item) {
    Bookmark.prototype.fromSyncBookmark.call(this, item);
    this.folderName = item.folder || undefined; // empty string -> undefined
    this.queryId = item.query;
  },
};

Utils.deferGetSet(BookmarkQuery, "cleartext", ["folderName", "queryId"]);

function BookmarkFolder(collection, id, type) {
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
    this.children = item.childRecordIds;
  },
};

Utils.deferGetSet(BookmarkFolder, "cleartext", [
  "description",
  "title",
  "children",
]);

function Livemark(collection, id) {
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

function BookmarkSeparator(collection, id) {
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

/**
 * The rest of this file implements two different bookmarks engines and stores.
 * The `services.sync.engine.bookmarks.buffer` pref controls which one we use.
 * `BaseBookmarksEngine` and `BaseBookmarksStore` define a handful of methods
 * shared between the two implementations.
 *
 * `BookmarksEngine` and `BookmarksStore` pull locally changed IDs before
 * syncing, examine every incoming record, use the default record-level
 * reconciliation to resolve merge conflicts, and update records in Places
 * using public APIs. This is similar to how the other sync engines work.
 *
 * Unfortunately, this general approach doesn't serve bookmark sync well.
 * Bookmarks form a tree locally, but they're stored as opaque, encrypted, and
 * unordered records on the server. The records are interdependent, with a
 * set of constraints: each parent must know the IDs and order of its children,
 * and a child can't appear in multiple parents.
 *
 * This has two important implications.
 *
 * First, some changes require us to upload multiple records. For example,
 * moving a bookmark into a different folder uploads the bookmark, old folder,
 * and new folder.
 *
 * Second, conflict resolution, like adding a bookmark to a folder on one
 * device, and moving a different bookmark out of the same folder on a different
 * device, must account for the tree structure. Otherwise, we risk uploading an
 * incomplete tree, and confuse other devices that try to sync.
 *
 * Historically, the lack of durable change tracking and atomic uploads meant
 * that we'd miss these changes entirely, or leave the server in an inconsistent
 * state after a partial sync. Another device would then sync, download and
 * apply the partial state directly to Places, and upload its changes. This
 * could easily result in Sync scrambling bookmarks on both devices, and user
 * intervention to manually undo the damage would make things worse.
 *
 * `BufferedBookmarksEngine` and `BufferedBookmarksStore` mitigate this by
 * mirroring incoming bookmarks in a separate database, constructing trees from
 * the local and remote bookmarks, and merging the two trees into a single
 * consistent tree that accounts for every bookmark. For more information about
 * merging, please see the explanation above `SyncedBookmarksMirror`.
 */
function BaseBookmarksEngine(service) {
  SyncEngine.call(this, "Bookmarks", service);
}
BaseBookmarksEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _recordObj: PlacesItem,
  _trackerObj: BookmarksTracker,
  version: 2,
  _defaultSort: "index",

  syncPriority: 4,
  allowSkippedRecord: false,

  // Exposed so that the buffered engine can override to store the sync ID in
  // the mirror.
  _ensureCurrentSyncID(newSyncID) {
    return PlacesSyncUtils.bookmarks.ensureCurrentSyncId(newSyncID);
  },

  async ensureCurrentSyncID(newSyncID) {
    let shouldWipeRemote = await PlacesSyncUtils.bookmarks.shouldWipeRemote();
    if (!shouldWipeRemote) {
      this._log.debug(
        "Checking if server sync ID ${newSyncID} matches existing",
        { newSyncID }
      );
      await this._ensureCurrentSyncID(newSyncID);
      // Update the sync ID in prefs to allow downgrading to older Firefox
      // releases that don't store Sync metadata in Places. This can be removed
      // in bug 1443021.
      await super.ensureCurrentSyncID(newSyncID);
      return newSyncID;
    }
    // We didn't take the new sync ID because we need to wipe the server
    // and other clients after a restore. Send the command, wipe the
    // server, and reset our sync ID to reupload everything.
    this._log.debug(
      "Ignoring server sync ID ${newSyncID} after restore; " +
        "wiping server and resetting sync ID",
      { newSyncID }
    );
    await this.service.clientsEngine.sendCommand(
      "wipeEngine",
      [this.name],
      null,
      { reason: "bookmark-restore" }
    );
    let assignedSyncID = await this.resetSyncID();
    return assignedSyncID;
  },

  async resetSyncID() {
    await this._deleteServerCollection();
    return this.resetLocalSyncID();
  },

  async resetLocalSyncID() {
    let newSyncID = await PlacesSyncUtils.bookmarks.resetSyncId();
    this._log.debug("Assigned new sync ID ${newSyncID}", { newSyncID });
    await super.ensureCurrentSyncID(newSyncID); // Remove in bug 1443021.
    return newSyncID;
  },

  async _syncStartup() {
    await super._syncStartup();

    try {
      // For first syncs, back up the user's bookmarks and livemarks. Livemarks
      // are unsupported as of bug 1477671, and syncing deletes them locally and
      // remotely.
      let lastSync = await this.getLastSync();
      if (!lastSync) {
        this._log.debug("Bookmarks backup starting");
        await PlacesBackups.create(null, true);
        this._log.debug("Bookmarks backup done");

        this._log.debug("Livemarks backup starting");
        await LiveBookmarkMigrator.migrate();
        this._log.debug("Livemarks backup done");
      }
    } catch (ex) {
      // Failure to create a backup is somewhat bad, but probably not bad
      // enough to prevent syncing of bookmarks - so just log the error and
      // continue.
      this._log.warn(
        "Error while backing up bookmarks, but continuing with sync",
        ex
      );
    }
  },

  async _sync() {
    try {
      await super._sync();
      if (this._ranMaintenanceOnLastSync) {
        // If the last sync failed, we ran maintenance, and this sync succeeded,
        // maintenance likely fixed the issue.
        this._ranMaintenanceOnLastSync = false;
        this.service.recordTelemetryEvent("maintenance", "fix", "bookmarks");
      }
    } catch (ex) {
      if (
        Async.isShutdownException(ex) ||
        ex.status > 0 ||
        ex.name == "MergeConflictError" ||
        ex.name == "InterruptedError"
      ) {
        // Don't run maintenance on shutdown or HTTP errors, or if we aborted
        // the sync because the user changed their bookmarks during merging.
        throw ex;
      }
      // Run Places maintenance periodically to try to recover from corruption
      // that might have caused the sync to fail. We cap the interval because
      // persistent failures likely indicate a problem that won't be fixed by
      // running maintenance after every failed sync.
      let elapsedSinceMaintenance =
        Date.now() / 1000 -
        Services.prefs.getIntPref("places.database.lastMaintenance", 0);
      if (elapsedSinceMaintenance >= PLACES_MAINTENANCE_INTERVAL_SECONDS) {
        this._log.error(
          "Bookmark sync failed, ${elapsedSinceMaintenance}s " +
            "elapsed since last run; running Places maintenance",
          { elapsedSinceMaintenance }
        );
        await PlacesDBUtils.maintenanceOnIdle();
        this._ranMaintenanceOnLastSync = true;
        this.service.recordTelemetryEvent("maintenance", "run", "bookmarks");
      } else {
        this._ranMaintenanceOnLastSync = false;
      }
      throw ex;
    }
  },

  async _syncFinish() {
    await SyncEngine.prototype._syncFinish.call(this);
    await PlacesSyncUtils.bookmarks.ensureMobileQuery();
  },

  async _createRecord(id) {
    if (this._modified.isTombstone(id)) {
      // If we already know a changed item is a tombstone, just create the
      // record without dipping into Places.
      return this._createTombstone(id);
    }
    let record = await SyncEngine.prototype._createRecord.call(this, id);
    if (record.deleted) {
      // Make sure deleted items are marked as tombstones. We do this here
      // in addition to the `isTombstone` call above because it's possible
      // a changed bookmark might be deleted during a sync (bug 1313967).
      this._modified.setTombstone(record.id);
    }
    return record;
  },

  async pullAllChanges() {
    return this.pullNewChanges();
  },

  async trackRemainingChanges() {
    let changes = this._modified.changes;
    await PlacesSyncUtils.bookmarks.pushChanges(changes);
  },

  _deleteId(id) {
    this._noteDeletedId(id);
  },

  // The bookmarks engine rarely calls this method directly, except in tests or
  // when handling a `reset{All, Engine}` command from another client. We
  // usually reset local Sync metadata on a sync ID mismatch, which both engines
  // override with logic that lives in Places and the mirror.
  async _resetClient() {
    await super._resetClient();
    await PlacesSyncUtils.bookmarks.reset();
  },

  // Cleans up the Places root, reading list items (ignored in bug 762118,
  // removed in bug 1155684), and pinned sites.
  _shouldDeleteRemotely(incomingItem) {
    return (
      FORBIDDEN_INCOMING_IDS.includes(incomingItem.id) ||
      FORBIDDEN_INCOMING_PARENT_IDS.includes(incomingItem.parentid)
    );
  },
};

/**
 * The original bookmarks engine. Uses an in-memory GUID map for deduping, and
 * the default implementation for reconciling changes. Handles child ordering
 * and deletions at the end of a sync.
 */
function BookmarksEngine(service) {
  BaseBookmarksEngine.apply(this, arguments);
}

BookmarksEngine.prototype = {
  __proto__: BaseBookmarksEngine.prototype,
  _storeObj: BookmarksStore,

  async getSyncID() {
    return PlacesSyncUtils.bookmarks.getSyncId();
  },

  async getLastSync() {
    let lastSync = await PlacesSyncUtils.bookmarks.getLastSync();
    return lastSync;
  },

  async setLastSync(lastSync) {
    await PlacesSyncUtils.bookmarks.setLastSync(lastSync);
    await super.setLastSync(lastSync); // Remove in bug 1443021.
  },

  emptyChangeset() {
    return new BookmarksChangeset();
  },

  async _buildGUIDMap() {
    let guidMap = {};
    let tree = await PlacesUtils.promiseBookmarksTree("");

    function* walkBookmarksRoots(tree) {
      for (let child of tree.children) {
        if (isSyncedRootNode(child)) {
          yield* Utils.walkTree(child, tree);
        }
      }
    }

    await Async.yieldingForEach(walkBookmarksRoots(tree), ([node, parent]) => {
      let { guid, type: placeType } = node;
      guid = PlacesSyncUtils.bookmarks.guidToRecordId(guid);
      let key;
      switch (placeType) {
        case PlacesUtils.TYPE_X_MOZ_PLACE:
          // Bookmark
          key = "b" + node.uri + ":" + (node.title || "");
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
          return;
      }

      let parentName = parent.title || "";
      if (guidMap[parentName] == null) {
        guidMap[parentName] = {};
      }

      // If the entry already exists, remember that there are explicit dupes.
      let entry = {
        guid,
        hasDupe: guidMap[parentName][key] != null,
      };

      // Remember this item's GUID for its parent-name/key pair.
      guidMap[parentName][key] = entry;
      this._log.trace("Mapped: " + [parentName, key, entry, entry.hasDupe]);
    });

    return guidMap;
  },

  // Helper function to get a dupe GUID for an item.
  async _mapDupe(item) {
    // Figure out if we have something to key with.
    let key;
    switch (item.type) {
      case "query":
      // Fallthrough, treat the same as a bookmark.
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
    let guidMap = await this.getGuidMap();

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
      this._log.trace("Mapped dupe", dupe);
      return dupe;
    }

    this._log.trace("No dupe found for key " + key + ".");
    return undefined;
  },

  async _syncStartup() {
    await super._syncStartup();
    this._store._childrenToOrder = {};
    this._store.clearPendingDeletions();
  },

  async getGuidMap() {
    if (this._guidMap) {
      return this._guidMap;
    }
    try {
      return (this._guidMap = await this._buildGUIDMap());
    } catch (ex) {
      if (Async.isShutdownException(ex)) {
        throw ex;
      }
      this._log.warn(
        "Error while building GUID map, skipping all other incoming items",
        ex
      );
      // eslint-disable-next-line no-throw-literal
      throw { code: SyncEngine.prototype.eEngineAbortApplyIncoming, cause: ex };
    }
  },

  async _deletePending() {
    // Delete pending items -- See the comment above BookmarkStore's deletePending
    let newlyModified = await this._store.deletePending();
    if (newlyModified) {
      this._log.debug("Deleted pending items", newlyModified);
      this._modified.insert(newlyModified);
    }
  },

  async _shouldReviveRemotelyDeletedRecord(item) {
    let modifiedTimestamp = this._modified.getModifiedTimestamp(item.id);
    if (!modifiedTimestamp) {
      // We only expect this to be called with items locally modified, so
      // something strange is going on - play it safe and don't revive it.
      this._log.error(
        "_shouldReviveRemotelyDeletedRecord called on unmodified item: " +
          item.id
      );
      return false;
    }

    // In addition to preventing the deletion of this record (handled by the caller),
    // we use `touch` to mark the parent of this record for uploading next sync, in order
    // to ensure its children array is accurate. If `touch` returns new change records,
    // we revive the item and insert the changes into the current changeset.
    let newChanges = await PlacesSyncUtils.bookmarks.touch(item.id);
    if (newChanges) {
      this._modified.insert(newChanges);
      return true;
    }
    return false;
  },

  async _processIncoming(newitems) {
    try {
      await SyncEngine.prototype._processIncoming.call(this, newitems);
    } finally {
      await this._postProcessIncoming();
    }
  },

  // Applies pending tombstones, sets folder child order, and updates the sync
  // status of all `NEW` bookmarks to `NORMAL`.
  async _postProcessIncoming() {
    await this._deletePending();
    await this._orderChildren();
    let changes = this._modified.changes;
    await PlacesSyncUtils.bookmarks.markChangesAsSyncing(changes);
  },

  async _orderChildren() {
    await this._store._orderChildren();
    this._store._childrenToOrder = {};
  },

  async _syncCleanup() {
    await SyncEngine.prototype._syncCleanup.call(this);
    delete this._guidMap;
  },

  async _createRecord(id) {
    let record = await super._createRecord(id);
    if (record.deleted) {
      return record;
    }
    // Mark the record as having dupes if necessary.
    let entry = await this._mapDupe(record);
    if (entry != null && entry.hasDupe) {
      record.hasDupe = true;
    }
    return record;
  },

  async _findDupe(item) {
    this._log.trace(
      "Finding dupe for " + item.id + " (already duped: " + item.hasDupe + ")."
    );

    // Don't bother finding a dupe if the incoming item has duplicates.
    if (item.hasDupe) {
      this._log.trace(item.id + " already a dupe: not finding one.");
      return null;
    }
    let mapped = await this._mapDupe(item);
    this._log.debug(item.id + " mapped to", mapped);
    return mapped ? mapped.guid : null;
  },

  // Called when _findDupe returns a dupe item and the engine has decided to
  // switch the existing item to the new incoming item.
  async _switchItemToDupe(localDupeGUID, incomingItem) {
    let newChanges = await PlacesSyncUtils.bookmarks.dedupe(
      localDupeGUID,
      incomingItem.id,
      incomingItem.parentid
    );
    this._modified.insert(newChanges);
  },

  beforeRecordDiscard(localRecord, remoteRecord, remoteIsNewer) {
    if (localRecord.type != "folder" || remoteRecord.type != "folder") {
      return;
    }
    // Resolve child order conflicts by taking the chronologically newer list,
    // then appending any missing items from the older list. This preserves the
    // order of those missing items relative to each other, but not relative to
    // the items that appear in the newer list.
    let newRecord = remoteIsNewer ? remoteRecord : localRecord;
    let newChildren = new Set(newRecord.children);

    let oldChildren = (remoteIsNewer ? localRecord : remoteRecord).children;
    let missingChildren = oldChildren
      ? oldChildren.filter(child => !newChildren.has(child))
      : [];

    // Some of the children in `order` might have been deleted, or moved to
    // other folders. `PlacesSyncUtils.bookmarks.order` ignores them.
    let order = newRecord.children
      ? [...newRecord.children, ...missingChildren]
      : missingChildren;
    this._log.debug("Recording children of " + localRecord.id, order);
    this._store._childrenToOrder[localRecord.id] = order;
  },

  getValidator() {
    return new BookmarkValidator();
  },
};

/**
 * The buffered bookmarks engine uses a different store that stages downloaded
 * bookmarks in a separate database, instead of writing directly to Places. The
 * buffer handles reconciliation, so we stub out `_reconcile`, and wait to pull
 * changes until we're ready to upload.
 */
function BufferedBookmarksEngine() {
  BaseBookmarksEngine.apply(this, arguments);
}

BufferedBookmarksEngine.prototype = {
  __proto__: BaseBookmarksEngine.prototype,
  _storeObj: BufferedBookmarksStore,
  // Used to override the engine name in telemetry, so that we can distinguish
  // errors that happen when the buffered engine is enabled vs when the
  // non-buffered engine is enabled.
  overrideTelemetryName: "bookmarks-buffered",

  // Needed to ensure we don't miss items when resuming a sync that failed or
  // aborted early.
  _defaultSort: "oldest",

  // The time to wait before aborting a merge in `_processIncoming`. Exposed
  // for tests.
  _applyTimeout: BUFFERED_BOOKMARK_APPLY_TIMEOUT_MS,

  async _ensureCurrentSyncID(newSyncID) {
    await super._ensureCurrentSyncID(newSyncID);
    let buf = await this._store.ensureOpenMirror();
    await buf.ensureCurrentSyncId(newSyncID);
  },

  async getSyncID() {
    return PlacesSyncUtils.bookmarks.getSyncId();
  },

  async resetLocalSyncID() {
    let newSyncID = await super.resetLocalSyncID();
    let buf = await this._store.ensureOpenMirror();
    await buf.ensureCurrentSyncId(newSyncID);
    return newSyncID;
  },

  async getLastSync() {
    let mirror = await this._store.ensureOpenMirror();
    return mirror.getCollectionHighWaterMark();
  },

  async setLastSync(lastSync) {
    let mirror = await this._store.ensureOpenMirror();
    await mirror.setCollectionLastModified(lastSync);
    // Update the last sync time in Places so that reverting to the original
    // bookmarks engine doesn't download records we've already applied.
    await PlacesSyncUtils.bookmarks.setLastSync(lastSync);
    await super.setLastSync(lastSync); // Remove in bug 1443021.
  },

  emptyChangeset() {
    return new BufferedBookmarksChangeset();
  },

  async _processIncoming(newitems) {
    await super._processIncoming(newitems);
    let buf = await this._store.ensureOpenMirror();

    let watchdog = Async.watchdog();
    watchdog.start(this._applyTimeout);

    try {
      let recordsToUpload = await buf.apply({
        remoteTimeSeconds: Resource.serverTime,
        weakUpload: [...this._needWeakUpload.keys()],
        signal: watchdog.signal,
      });
      this._modified.replace(recordsToUpload);
    } finally {
      watchdog.stop();
      if (watchdog.abortReason) {
        this._log.warn(`Aborting bookmark merge: ${watchdog.abortReason}`);
      }
      this._needWeakUpload.clear();
    }
  },

  async _reconcile(item) {
    return true;
  },

  async _createRecord(id) {
    let record = await this._doCreateRecord(id);
    if (!record.deleted) {
      // Set hasDupe on all (non-deleted) records since we don't use it (e.g.
      // the buffered engine doesn't), and we want to minimize the risk of older
      // clients corrupting records. Note that the SyncedBookmarksMirror sets it
      // for all records that it created, but we would like to ensure that
      // weakly uploaded records are marked as hasDupe as well.
      record.hasDupe = true;
    }
    return record;
  },

  async _doCreateRecord(id) {
    if (this._needWeakUpload.has(id)) {
      return this._store.createRecord(id, this.name);
    }
    let change = this._modified.changes[id];
    if (!change) {
      this._log.error(
        "Creating record for item ${id} not in strong changeset",
        { id }
      );
      throw new TypeError("Can't create record for unchanged item");
    }
    let record = this._recordFromCleartext(id, change.cleartext);
    record.sortindex = await this._store._calculateIndex(record);
    return record;
  },

  _recordFromCleartext(id, cleartext) {
    let recordObj = getTypeObject(cleartext.type);
    if (!recordObj) {
      this._log.warn(
        "Creating record for item ${id} with unknown type ${type}",
        { id, type: cleartext.type }
      );
      recordObj = PlacesItem;
    }
    let record = new recordObj(this.name, id);
    record.cleartext = cleartext;
    return record;
  },

  async pullChanges() {
    return {};
  },

  /**
   * Writes successfully uploaded records back to the mirror, so that the
   * mirror matches the server. We update the mirror before updating Places,
   * which has implications for interrupted syncs.
   *
   * 1. Sync interrupted during upload; server doesn't support atomic uploads.
   *    We'll download and reapply everything that we uploaded before the
   *    interruption. All locally changed items retain their change counters.
   * 2. Sync interrupted during upload; atomic uploads enabled. The server
   *    discards the batch. All changed local items retain their change
   *    counters, so the next sync resumes cleanly.
   * 3. Sync interrupted during upload; outgoing records can't fit in a single
   *    batch. We'll download and reapply all records through the most recent
   *    committed batch. This is a variation of (1).
   * 4. Sync interrupted after we update the mirror, but before cleanup. The
   *    mirror matches the server, but locally changed items retain their change
   *    counters. Reuploading them on the next sync should be idempotent, though
   *    unnecessary. If another client makes a conflicting remote change before
   *    we sync again, we may incorrectly prefer the local state.
   * 5. Sync completes successfully. We'll update the mirror, and reset the
   *    change counters for all items.
   */
  async _onRecordsWritten(succeeded, failed, serverModifiedTime) {
    let records = [];
    for (let id of succeeded) {
      let change = this._modified.changes[id];
      if (!change) {
        // TODO (Bug 1433178): Write weakly uploaded records back to the mirror.
        this._log.info("Uploaded record not in strong changeset", id);
        continue;
      }
      if (!change.synced) {
        this._log.info("Record in strong changeset not uploaded", id);
        continue;
      }
      let cleartext = change.cleartext;
      if (!cleartext) {
        this._log.error(
          "Missing Sync record cleartext for ${id} in ${change}",
          { id, change }
        );
        throw new TypeError("Missing cleartext for uploaded Sync record");
      }
      let record = this._recordFromCleartext(id, cleartext);
      record.modified = serverModifiedTime;
      records.push(record);
    }
    let buf = await this._store.ensureOpenMirror();
    await buf.store(records, { needsMerge: false });
  },

  async _resetClient() {
    await super._resetClient();
    let buf = await this._store.ensureOpenMirror();
    await buf.reset();
  },

  async finalize() {
    await super.finalize();
    await this._store.finalize();
  },
};

/**
 * The only code shared between `BookmarksStore` and `BufferedBookmarksStore`
 * is for creating Sync records from Places items. Everything else is
 * different.
 */
function BaseBookmarksStore(name, engine) {
  Store.call(this, name, engine);
}

BaseBookmarksStore.prototype = {
  __proto__: Store.prototype,

  // Create a record starting from the weave id (places guid)
  async createRecord(id, collection) {
    let item = await PlacesSyncUtils.bookmarks.fetch(id);
    if (!item) {
      // deleted item
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

    record.sortindex = await this._calculateIndex(record);

    return record;
  },

  async _calculateIndex(record) {
    // Ensure folders have a very high sort index so they're not synced last.
    if (record.type == "folder") {
      return FOLDER_SORTINDEX;
    }

    // For anything directly under the toolbar, give it a boost of more than an
    // unvisited bookmark
    let index = 0;
    if (record.parentid == "toolbar") {
      index += 150;
    }

    // Add in the bookmark's frecency if we have something.
    if (record.bmkUri != null) {
      let frecency = await PlacesSyncUtils.history.fetchURLFrecency(
        record.bmkUri
      );
      if (frecency != -1) {
        index += frecency;
      }
    }

    return index;
  },

  async wipe() {
    // Save a backup before clearing out all bookmarks.
    await PlacesBackups.create(null, true);
    await PlacesSyncUtils.bookmarks.wipe();
  },
};

/**
 * The original store updates Places during the sync, using public methods.
 * `BookmarksStore` implements all abstract `Store` methods, and behaves like
 * the other stores.
 */
function BookmarksStore() {
  BaseBookmarksStore.apply(this, arguments);
  this._itemsToDelete = new Set();
}
BookmarksStore.prototype = {
  __proto__: BaseBookmarksStore.prototype,

  async itemExists(id) {
    return (await this.idForGUID(id)) > 0;
  },

  async applyIncoming(record) {
    this._log.debug("Applying record " + record.id);
    let isSpecial = PlacesSyncUtils.bookmarks.ROOTS.includes(record.id);

    if (record.deleted) {
      if (isSpecial) {
        this._log.warn("Ignoring deletion for special record " + record.id);
        return;
      }

      // Don't bother with pre and post-processing for deletions.
      await Store.prototype.applyIncoming.call(this, record);
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
    if (record.type == "query" && !record.bmkUri) {
      this._log.warn("Skipping malformed query bookmark: " + record.id);
      return;
    }

    // Figure out the local id of the parent GUID if available
    let parentGUID = record.parentid;
    if (!parentGUID) {
      throw new Error(
        `Record ${record.id} has invalid parentid: ${parentGUID}`
      );
    }
    this._log.debug("Remote parent is " + parentGUID);

    if (record.type == "livemark") {
      // Places no longer supports livemarks, so we replace new and updated
      // livemarks with tombstones, and insert new change records for the engine
      // to upload.
      let livemarkInfo = record.toSyncBookmark();
      let newChanges = await PlacesSyncUtils.bookmarks.removeLivemark(
        livemarkInfo
      );
      if (newChanges) {
        this.engine._modified.insert(newChanges);
        return;
      }
    }

    // Do the normal processing of incoming records
    await Store.prototype.applyIncoming.call(this, record);

    if (record.type == "folder" && record.children) {
      this._childrenToOrder[record.id] = record.children;
    }
  },

  async create(record) {
    let info = record.toSyncBookmark();
    // This can throw if we're inserting an invalid or incomplete bookmark.
    // That's fine; the exception will be caught by `applyIncomingBatch`
    // without aborting further processing.
    let item = await PlacesSyncUtils.bookmarks.insert(info);
    if (item) {
      this._log.trace(
        `Created ${item.kind} ${item.recordId} under ${item.parentRecordId}`,
        item
      );
      if (item.dateAdded != record.dateAdded) {
        this.engine.addForWeakUpload(item.recordId);
      }
    }
  },

  async remove(record) {
    this._log.trace(`Buffering removal of item "${record.id}".`);
    this._itemsToDelete.add(record.id);
  },

  async update(record) {
    let info = record.toSyncBookmark();
    let item = await PlacesSyncUtils.bookmarks.update(info);
    if (item) {
      this._log.trace(
        `Updated ${item.kind} ${item.recordId} under ${item.parentRecordId}`,
        item
      );
      if (item.dateAdded != record.dateAdded) {
        this.engine.addForWeakUpload(item.recordId);
      }
    }
  },

  async _orderChildren() {
    for (let id in this._childrenToOrder) {
      let children = this._childrenToOrder[id];
      try {
        await PlacesSyncUtils.bookmarks.order(id, children);
      } catch (ex) {
        this._log.debug(`Could not order children for ${id}`, ex);
      }
    }
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

  async deletePending() {
    let guidsToUpdate = await PlacesSyncUtils.bookmarks.remove([
      ...this._itemsToDelete,
    ]);
    this.clearPendingDeletions();
    return guidsToUpdate;
  },

  clearPendingDeletions() {
    this._itemsToDelete.clear();
  },

  async idForGUID(guid) {
    // guid might be a String object rather than a string.
    guid = PlacesSyncUtils.bookmarks.recordIdToGuid(guid.toString());

    try {
      return await PlacesUtils.promiseItemId(guid);
    } catch (ex) {
      return -1;
    }
  },

  async wipe() {
    this.clearPendingDeletions();
    await super.wipe();
  },
};

/**
 * The buffered store delegates to the mirror for staging and applying
 * records. Unlike `BookmarksStore`, `BufferedBookmarksStore` only
 * implements `applyIncoming`, and `createRecord` via `BaseBookmarksStore`.
 * These are the only two methods that `BufferedBookmarksEngine` calls during
 * download and upload.
 *
 * The other `Store` methods intentionally remain abstract, so you can't use
 * this store to create or update bookmarks in Places. All changes must go
 * through the mirror, which takes care of merging and producing a valid tree.
 */
function BufferedBookmarksStore() {
  BaseBookmarksStore.apply(this, arguments);
}

BufferedBookmarksStore.prototype = {
  __proto__: BaseBookmarksStore.prototype,
  _openMirrorPromise: null,

  // For tests.
  _batchChunkSize: 500,

  ensureOpenMirror() {
    if (!this._openMirrorPromise) {
      this._openMirrorPromise = this._openMirror().catch(err => {
        // We may have failed to open the mirror temporarily; for example, if
        // the database is locked. Clear the promise so that subsequent
        // `ensureOpenMirror` calls can try to open the mirror again.
        this._openMirrorPromise = null;
        throw err;
      });
    }
    return this._openMirrorPromise;
  },

  async _openMirror() {
    let mirrorPath = OS.Path.join(
      OS.Constants.Path.profileDir,
      "weave",
      "bookmarks.sqlite"
    );
    await OS.File.makeDir(OS.Path.dirname(mirrorPath), {
      from: OS.Constants.Path.profileDir,
    });

    return SyncedBookmarksMirror.open({
      path: mirrorPath,
      recordTelemetryEvent: (object, method, value, extra) => {
        this.engine.service.recordTelemetryEvent(object, method, value, extra);
      },
      recordStepTelemetry: (name, took, counts) => {
        Observers.notify(
          "weave:engine:sync:step",
          {
            name,
            took,
            counts,
          },
          this.name
        );
      },
      recordValidationTelemetry: (took, checked, problems) => {
        Observers.notify(
          "weave:engine:validate:finish",
          {
            version: BUFFERED_BOOKMARK_VALIDATOR_VERSION,
            took,
            checked,
            problems,
          },
          this.name
        );
      },
    });
  },

  async applyIncomingBatch(records) {
    let buf = await this.ensureOpenMirror();
    for (let [, chunk] of PlacesSyncUtils.chunkArray(
      records,
      this._batchChunkSize
    )) {
      await buf.store(chunk);
    }
    // Array of failed records.
    return [];
  },

  async applyIncoming(record) {
    let buf = await this.ensureOpenMirror();
    await buf.store([record]);
  },

  async finalize() {
    if (!this._openMirrorPromise) {
      return;
    }
    let buf = await this._openMirrorPromise;
    await buf.finalize();
  },
};

// The bookmarks tracker is a special flower. Instead of listening for changes
// via observer notifications, it queries Places for the set of items that have
// changed since the last sync. Because it's a "pull-based" tracker, it ignores
// all concepts of "add a changed ID." However, it still registers an observer
// to bump the score, so that changed bookmarks are synced immediately.
function BookmarksTracker(name, engine) {
  Tracker.call(this, name, engine);
  this._batchDepth = 0;
  this._batchSawScoreIncrement = false;
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

  onStart() {
    PlacesUtils.bookmarks.addObserver(this, true);
    this._placesListener = new PlacesWeakCallbackWrapper(
      this.handlePlacesEvents.bind(this)
    );
    PlacesUtils.observers.addListener(["bookmark-added"], this._placesListener);
    Svc.Obs.add("bookmarks-restore-begin", this);
    Svc.Obs.add("bookmarks-restore-success", this);
    Svc.Obs.add("bookmarks-restore-failed", this);
  },

  onStop() {
    PlacesUtils.bookmarks.removeObserver(this);
    PlacesUtils.observers.removeListener(
      ["bookmark-added"],
      this._placesListener
    );
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

  async getChangedIDs() {
    return PlacesSyncUtils.bookmarks.pullChanges();
  },

  set changedIDs(obj) {
    throw new Error("Don't set initial changed bookmark IDs");
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "bookmarks-restore-begin":
        this._log.debug("Ignoring changes from importing bookmarks.");
        break;
      case "bookmarks-restore-success":
        this._log.debug("Tracking all items on successful import.");

        if (data == "json") {
          this._log.debug(
            "Restore succeeded: wiping server and other clients."
          );
          // Trigger an immediate sync. `ensureCurrentSyncID` will notice we
          // restored, wipe the server and other clients, reset the sync ID, and
          // upload the restored tree.
          this.score += SCORE_INCREMENT_XLARGE;
        } else {
          // "html", "html-initial", or "json-append"
          this._log.debug("Import succeeded.");
        }
        break;
      case "bookmarks-restore-failed":
        this._log.debug("Tracking all items on failed import.");
        break;
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsINavBookmarkObserver,
    Ci.nsINavBookmarkObserver_MOZILLA_1_9_1_ADDITIONS,
    Ci.nsISupportsWeakReference,
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

  handlePlacesEvents(events) {
    for (let event of events) {
      if (IGNORED_SOURCES.includes(event.source)) {
        continue;
      }

      this._log.trace("'bookmark-added': " + event.id);
      this._upScore();
    }
  },

  onItemRemoved(itemId, parentId, index, type, uri, guid, parentGuid, source) {
    if (IGNORED_SOURCES.includes(source)) {
      return;
    }

    this._log.trace("onItemRemoved: " + itemId);
    this._upScore();
  },

  // This method is oddly structured, but the idea is to return as quickly as
  // possible -- this handler gets called *every time* a bookmark changes, for
  // *each change*.
  onItemChanged: function BMT_onItemChanged(
    itemId,
    property,
    isAnno,
    value,
    lastModified,
    itemType,
    parentId,
    guid,
    parentGuid,
    oldValue,
    source
  ) {
    if (IGNORED_SOURCES.includes(source)) {
      return;
    }

    if (isAnno && !ANNOS_TO_TRACK.includes(property)) {
      // Ignore annotations except for the ones that we sync.
      return;
    }

    // Ignore favicon changes to avoid unnecessary churn.
    if (property == "favicon") {
      return;
    }

    this._log.trace(
      "onItemChanged: " +
        itemId +
        (", " + property + (isAnno ? " (anno)" : "")) +
        (value ? ' = "' + value + '"' : "")
    );
    this._upScore();
  },

  onItemMoved: function BMT_onItemMoved(
    itemId,
    oldParent,
    oldIndex,
    newParent,
    newIndex,
    itemType,
    guid,
    oldParentGuid,
    newParentGuid,
    source
  ) {
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
  onItemVisited() {},
};

/**
 * A changeset that stores extra metadata in a change record for each ID. The
 * engine updates this metadata when uploading Sync records, and writes it back
 * to Places in `BaseBookmarksEngine#trackRemainingChanges`.
 *
 * The `synced` property on a change record means its corresponding item has
 * been uploaded, and we should pretend it doesn't exist in the changeset.
 */
class BufferedBookmarksChangeset extends Changeset {
  // Only `_reconcile` calls `getModifiedTimestamp` and `has`, and the buffered
  // engine does its own reconciliation.
  getModifiedTimestamp(id) {
    throw new Error("Don't use timestamps to resolve bookmark conflicts");
  }

  has(id) {
    throw new Error("Don't use the changeset to resolve bookmark conflicts");
  }

  delete(id) {
    let change = this.changes[id];
    if (change) {
      // Mark the change as synced without removing it from the set. We do this
      // so that we can update Places in `trackRemainingChanges`.
      change.synced = true;
    }
  }

  ids() {
    let results = new Set();
    for (let id in this.changes) {
      if (!this.changes[id].synced) {
        results.add(id);
      }
    }
    return [...results];
  }
}

class BookmarksChangeset extends BufferedBookmarksChangeset {
  getModifiedTimestamp(id) {
    let change = this.changes[id];
    if (change) {
      // Pretend the change doesn't exist if we've already synced or
      // reconciled it.
      return change.synced ? Number.NaN : change.modified;
    }
    return Number.NaN;
  }

  has(id) {
    let change = this.changes[id];
    if (change) {
      return !change.synced;
    }
    return false;
  }

  setTombstone(id) {
    let change = this.changes[id];
    if (change) {
      change.tombstone = true;
    }
  }

  isTombstone(id) {
    let change = this.changes[id];
    if (change) {
      return change.tombstone;
    }
    return false;
  }
}
