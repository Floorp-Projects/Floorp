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

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Observers: "resource://services-common/observers.js",
  OS: "resource://gre/modules/osfile.jsm",
  PlacesBackups: "resource://gre/modules/PlacesBackups.jsm",
  PlacesDBUtils: "resource://gre/modules/PlacesDBUtils.jsm",
  PlacesSyncUtils: "resource://gre/modules/PlacesSyncUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Resource: "resource://services-sync/resource.js",
  SyncedBookmarksMirror: "resource://gre/modules/SyncedBookmarksMirror.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "ANNOS_TO_TRACK", () => [
  lazy.PlacesUtils.LMANNO_FEEDURI,
  lazy.PlacesUtils.LMANNO_SITEURI,
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
XPCOMUtils.defineLazyGetter(lazy, "IGNORED_SOURCES", () => [
  lazy.PlacesUtils.bookmarks.SOURCES.SYNC,
  lazy.PlacesUtils.bookmarks.SOURCES.IMPORT,
  lazy.PlacesUtils.bookmarks.SOURCES.RESTORE,
  lazy.PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP,
  lazy.PlacesUtils.bookmarks.SOURCES.SYNC_REPARENT_REMOVED_FOLDER_CHILDREN,
]);

// The validation telemetry version for the engine. Version 1 is collected
// by `bookmark_validator.js`, and checks value as well as structure
// differences. Version 2 is collected by the engine as part of building the
// remote tree, and checks structure differences only.
const BOOKMARK_VALIDATOR_VERSION = 2;

// The maximum time that the engine should wait before aborting a bookmark
// merge.
const BOOKMARK_APPLY_TIMEOUT_MS = 5 * 60 * 60 * 1000; // 5 minutes

// The default frecency value to use when not known.
const FRECENCY_UNKNOWN = -1;

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
    let dateAdded = lazy.PlacesSyncUtils.bookmarks.ratchetTimestampBackwards(
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
 * The bookmarks engine uses a different store that stages downloaded bookmarks
 * in a separate database, instead of writing directly to Places. The buffer
 * handles reconciliation, so we stub out `_reconcile`, and wait to pull changes
 * until we're ready to upload.
 */
function BookmarksEngine(service) {
  SyncEngine.call(this, "Bookmarks", service);
}
BookmarksEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _recordObj: PlacesItem,
  _trackerObj: BookmarksTracker,
  _storeObj: BookmarksStore,
  version: 2,
  // Used to override the engine name in telemetry, so that we can distinguish
  // this engine from the old, now removed non-buffered engine.
  overrideTelemetryName: "bookmarks-buffered",

  // Needed to ensure we don't miss items when resuming a sync that failed or
  // aborted early.
  _defaultSort: "oldest",

  syncPriority: 4,
  allowSkippedRecord: false,

  async _ensureCurrentSyncID(newSyncID) {
    await lazy.PlacesSyncUtils.bookmarks.ensureCurrentSyncId(newSyncID);
    let buf = await this._store.ensureOpenMirror();
    await buf.ensureCurrentSyncId(newSyncID);
  },

  async ensureCurrentSyncID(newSyncID) {
    let shouldWipeRemote = await lazy.PlacesSyncUtils.bookmarks.shouldWipeRemote();
    if (!shouldWipeRemote) {
      this._log.debug(
        "Checking if server sync ID ${newSyncID} matches existing",
        { newSyncID }
      );
      await this._ensureCurrentSyncID(newSyncID);
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

  async getSyncID() {
    return lazy.PlacesSyncUtils.bookmarks.getSyncId();
  },

  async resetSyncID() {
    await this._deleteServerCollection();
    return this.resetLocalSyncID();
  },

  async resetLocalSyncID() {
    let newSyncID = await lazy.PlacesSyncUtils.bookmarks.resetSyncId();
    this._log.debug("Assigned new sync ID ${newSyncID}", { newSyncID });
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
    await lazy.PlacesSyncUtils.bookmarks.setLastSync(lastSync);
  },

  async _syncStartup() {
    await super._syncStartup();

    try {
      // For first syncs, back up the user's bookmarks.
      let lastSync = await this.getLastSync();
      if (!lastSync) {
        this._log.debug("Bookmarks backup starting");
        await lazy.PlacesBackups.create(null, true);
        this._log.debug("Bookmarks backup done");
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
        ex.name == "InterruptedError"
      ) {
        // Don't run maintenance on shutdown or HTTP errors, or if we aborted
        // the sync because the user changed their bookmarks during merging.
        throw ex;
      }
      if (ex.name == "MergeConflictError") {
        this._log.warn(
          "Bookmark syncing ran into a merge conflict error...will retry later"
        );
        return;
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
        await lazy.PlacesDBUtils.maintenanceOnIdle();
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
    await lazy.PlacesSyncUtils.bookmarks.ensureMobileQuery();
  },

  async pullAllChanges() {
    return this.pullNewChanges();
  },

  async trackRemainingChanges() {
    let changes = this._modified.changes;
    await lazy.PlacesSyncUtils.bookmarks.pushChanges(changes);
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
    await lazy.PlacesSyncUtils.bookmarks.reset();
    let buf = await this._store.ensureOpenMirror();
    await buf.reset();
  },

  // Cleans up the Places root, reading list items (ignored in bug 762118,
  // removed in bug 1155684), and pinned sites.
  _shouldDeleteRemotely(incomingItem) {
    return (
      FORBIDDEN_INCOMING_IDS.includes(incomingItem.id) ||
      FORBIDDEN_INCOMING_PARENT_IDS.includes(incomingItem.parentid)
    );
  },

  emptyChangeset() {
    return new BookmarksChangeset();
  },

  async _apply() {
    let buf = await this._store.ensureOpenMirror();
    let watchdog = this._newWatchdog();
    watchdog.start(BOOKMARK_APPLY_TIMEOUT_MS);

    try {
      let recordsToUpload = await buf.apply({
        remoteTimeSeconds: lazy.Resource.serverTime,
        signal: watchdog.signal,
      });
      this._modified.replace(recordsToUpload);
    } finally {
      watchdog.stop();
      if (watchdog.abortReason) {
        this._log.warn(`Aborting bookmark merge: ${watchdog.abortReason}`);
      }
    }
  },

  async _processIncoming(newitems) {
    await super._processIncoming(newitems);
    await this._apply();
  },

  async _reconcile(item) {
    return true;
  },

  async _createRecord(id) {
    let record = await this._doCreateRecord(id);
    if (!record.deleted) {
      // Set hasDupe on all (non-deleted) records since we don't use it and we
      // want to minimize the risk of older clients corrupting records. Note
      // that the SyncedBookmarksMirror sets it for all records that it created,
      // but we would like to ensure that weakly uploaded records are marked as
      // hasDupe as well.
      record.hasDupe = true;
    }
    return record;
  },

  async _doCreateRecord(id) {
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

  async finalize() {
    await super.finalize();
    await this._store.finalize();
  },
};

/**
 * The bookmarks store delegates to the mirror for staging and applying
 * records. Most `Store` methods intentionally remain abstract, so you can't use
 * this store to create or update bookmarks in Places. All changes must go
 * through the mirror, which takes care of merging and producing a valid tree.
 */
function BookmarksStore(name, engine) {
  Store.call(this, name, engine);
}

BookmarksStore.prototype = {
  __proto__: Store.prototype,

  _openMirrorPromise: null,

  // For tests.
  _batchChunkSize: 500,

  // Create a record starting from the weave id (places guid)
  async createRecord(id, collection) {
    let item = await lazy.PlacesSyncUtils.bookmarks.fetch(id);
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
      let frecency = FRECENCY_UNKNOWN;
      try {
        frecency = await lazy.PlacesSyncUtils.history.fetchURLFrecency(
          record.bmkUri
        );
      } catch (ex) {
        this._log.warn(
          `Failed to fetch frecency for ${record.id}; assuming default`,
          ex
        );
        this._log.trace("Record {id} has invalid URL ${bmkUri}", record);
      }
      if (frecency != FRECENCY_UNKNOWN) {
        index += frecency;
      }
    }

    return index;
  },

  async wipe() {
    // Save a backup before clearing out all bookmarks.
    await lazy.PlacesBackups.create(null, true);
    await lazy.PlacesSyncUtils.bookmarks.wipe();
  },

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
    let mirrorPath = lazy.OS.Path.join(
      lazy.OS.Constants.Path.profileDir,
      "weave",
      "bookmarks.sqlite"
    );
    await lazy.OS.File.makeDir(lazy.OS.Path.dirname(mirrorPath), {
      from: lazy.OS.Constants.Path.profileDir,
    });

    return lazy.SyncedBookmarksMirror.open({
      path: mirrorPath,
      recordStepTelemetry: (name, took, counts) => {
        lazy.Observers.notify(
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
        lazy.Observers.notify(
          "weave:engine:validate:finish",
          {
            version: BOOKMARK_VALIDATOR_VERSION,
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
    for (let chunk of lazy.PlacesUtils.chunkArray(
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
}
BookmarksTracker.prototype = {
  __proto__: Tracker.prototype,

  onStart() {
    lazy.PlacesUtils.bookmarks.addObserver(this, true);
    this._placesListener = new PlacesWeakCallbackWrapper(
      this.handlePlacesEvents.bind(this)
    );
    lazy.PlacesUtils.observers.addListener(
      [
        "bookmark-added",
        "bookmark-removed",
        "bookmark-moved",
        "bookmark-tags-changed",
        "bookmark-time-changed",
        "bookmark-title-changed",
        "bookmark-url-changed",
      ],
      this._placesListener
    );
    Svc.Obs.add("bookmarks-restore-begin", this);
    Svc.Obs.add("bookmarks-restore-success", this);
    Svc.Obs.add("bookmarks-restore-failed", this);
  },

  onStop() {
    lazy.PlacesUtils.bookmarks.removeObserver(this);
    lazy.PlacesUtils.observers.removeListener(
      [
        "bookmark-added",
        "bookmark-removed",
        "bookmark-moved",
        "bookmark-tags-changed",
        "bookmark-time-changed",
        "bookmark-title-changed",
        "bookmark-url-changed",
      ],
      this._placesListener
    );
    Svc.Obs.remove("bookmarks-restore-begin", this);
    Svc.Obs.remove("bookmarks-restore-success", this);
    Svc.Obs.remove("bookmarks-restore-failed", this);
  },

  async getChangedIDs() {
    return lazy.PlacesSyncUtils.bookmarks.pullChanges();
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
    "nsINavBookmarkObserver",
    "nsISupportsWeakReference",
  ]),

  /* Every add/remove/change will trigger a sync for MULTI_DEVICE */
  _upScore: function BMT__upScore() {
    this.score += SCORE_INCREMENT_XLARGE;
  },

  handlePlacesEvents(events) {
    for (let event of events) {
      switch (event.type) {
        case "bookmark-added":
        case "bookmark-removed":
        case "bookmark-moved":
        case "bookmark-guid-changed":
        case "bookmark-tags-changed":
        case "bookmark-time-changed":
        case "bookmark-title-changed":
        case "bookmark-url-changed":
          if (lazy.IGNORED_SOURCES.includes(event.source)) {
            continue;
          }

          this._log.trace(`'${event.type}': ${event.id}`);
          this._upScore();
          break;
        case "purge-caches":
          this._log.trace("purge-caches");
          this._upScore();
          break;
      }
    }
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
    if (lazy.IGNORED_SOURCES.includes(source)) {
      return;
    }

    if (isAnno && !lazy.ANNOS_TO_TRACK.includes(property)) {
      // Ignore annotations except for the ones that we sync.
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
};

/**
 * A changeset that stores extra metadata in a change record for each ID. The
 * engine updates this metadata when uploading Sync records, and writes it back
 * to Places in `BookmarksEngine#trackRemainingChanges`.
 *
 * The `synced` property on a change record means its corresponding item has
 * been uploaded, and we should pretend it doesn't exist in the changeset.
 */
class BookmarksChangeset extends Changeset {
  // Only `_reconcile` calls `getModifiedTimestamp` and `has`, and the engine
  // does its own reconciliation.
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
