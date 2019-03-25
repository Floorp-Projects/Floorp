/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This file implements a mirror and two-way merger for synced bookmarks. The
 * mirror matches the complete tree stored on the Sync server, and stages new
 * bookmarks changed on the server since the last sync. The merger walks the
 * local tree in Places and the mirrored remote tree, produces a new merged
 * tree, then updates the local tree to reflect the merged tree.
 *
 * Let's start with an overview of the different classes, and how they fit
 * together.
 *
 * - `SyncedBookmarksMirror` sets up the database, validates and upserts new
 *   incoming records, attaches to Places, and applies the changed records.
 *   During application, we fetch the local and remote bookmark trees, merge
 *   them, and update Places to match. Merging and application happen in a
 *   single transaction, so applying the merged tree won't collide with local
 *   changes. A failure at this point aborts the merge and leaves Places
 *   unchanged.
 *
 * - A `BookmarkTree` is a fully rooted tree that also notes deletions. A
 *   `BookmarkNode` represents a local item in Places, or a remote item in the
 *   mirror.
 *
 * - A `MergedBookmarkNode` holds a local node, a remote node, and a
 *   `MergeState` that indicates which node to prefer when updating Places and
 *   the server to match the merged tree.
 *
 * - `BookmarkObserverRecorder` records all changes made to Places during the
 *   merge, then dispatches `nsINavBookmarkObserver` notifications. Places uses
 *   these notifications to update the UI and internal caches. We can't dispatch
 *   during the merge because observers won't see the changes until the merge
 *   transaction commits and the database is consistent again.
 *
 * - After application, we flag all applied incoming items as merged, create
 *   Sync records for the locally new and updated items in Places, and upload
 *   the records to the server. At this point, all outgoing items are flagged as
 *   changed in Places, so the next sync can resume cleanly if the upload is
 *   interrupted or fails.
 *
 * - Once upload succeeds, we update the mirror with the uploaded records, so
 *   that the mirror matches the server again. An interruption or error here
 *   will leave the uploaded items flagged as changed in Places, so we'll merge
 *   them again on the next sync. This is redundant work, but shouldn't cause
 *   issues.
 */

var EXPORTED_SYMBOLS = ["SyncedBookmarksMirror"];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

XPCOMUtils.defineLazyModuleGetters(this, {
  Async: "resource://services-common/async.js",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  Log: "resource://gre/modules/Log.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PlacesSyncUtils: "resource://gre/modules/PlacesSyncUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Sqlite: "resource://gre/modules/Sqlite.jsm",
});

XPCOMUtils.defineLazyGetter(this, "MirrorLog", () =>
  Log.repository.getLogger("Sync.Engine.Bookmarks.Mirror")
);

const SyncedBookmarksMerger = Components.Constructor(
  "@mozilla.org/browser/synced-bookmarks-merger;1",
  "mozISyncedBookmarksMerger");

/**
 * A common table expression for all local items in Places, to be included in a
 * `WITH RECURSIVE` clause. We start at the roots, excluding tags (bug 424160),
 * and work our way down.
 *
 * Note that syncable items (`isSyncable`) descend from the four syncable roots.
 * Any other roots and their descendants, like the left pane root, left pane
 * queries, and custom roots, are non-syncable.
 *
 * Newer Desktops should never reupload non-syncable items (bug 1274496), and
 * should have removed them in Places migrations (bug 1310295). However, these
 * items might be orphaned in "unfiled", in which case they're seen as syncable
 * locally. If the server has the missing parents and roots, we'll determine
 * that the items are non-syncable when merging, remove them from Places, and
 * upload tombstones to the server.
 */
XPCOMUtils.defineLazyGetter(this, "LocalItemsSQLFragment", () => `
  localItems(id, guid, parentId, parentGuid, position, type, title,
             parentTitle, placeId, dateAdded, lastModified, syncChangeCounter,
             isSyncable, level) AS (
    SELECT b.id, b.guid, p.id, p.guid, b.position, b.type, b.title, p.title,
           b.fk, b.dateAdded, b.lastModified, b.syncChangeCounter,
           b.guid IN (${PlacesUtils.bookmarks.userContentRoots.map(v =>
             `'${v}'`
           ).join(",")}), 0
    FROM moz_bookmarks b
    JOIN moz_bookmarks p ON p.id = b.parent
    WHERE b.guid <> '${PlacesUtils.bookmarks.tagsGuid}' AND
          p.guid = '${PlacesUtils.bookmarks.rootGuid}'
    UNION ALL
    SELECT b.id, b.guid, s.id, s.guid, b.position, b.type, b.title, s.title,
           b.fk, b.dateAdded, b.lastModified, b.syncChangeCounter,
           s.isSyncable, s.level + 1
    FROM moz_bookmarks b
    JOIN localItems s ON s.id = b.parent
    WHERE b.guid <> '${PlacesUtils.bookmarks.rootGuid}'
  )
`);

// These can be removed once they're exposed in a central location (bug
// 1375896).
const DB_URL_LENGTH_MAX = 65536;
const DB_TITLE_LENGTH_MAX = 4096;

// The current mirror database schema version. Bump for migrations, then add
// migration code to `migrateMirrorSchema`.
const MIRROR_SCHEMA_VERSION = 3;

const DEFAULT_MAX_FRECENCIES_TO_RECALCULATE = 400;

// Use a shared jankYielder in these functions
XPCOMUtils.defineLazyGetter(this, "maybeYield", () => Async.jankYielder());
function yieldingIterator(collection) {
  return Async.yieldingIterator(collection, maybeYield);
}

/** Adapts a `Log.jsm` logger to a `mozISyncedBookmarksMirrorLogger`. */
class MirrorLoggerAdapter {
  constructor(log) {
    this.log = log;
  }

  get maxLevel() {
    let level = this.log.level;
    if (level <= Log.Level.All) {
      return Ci.mozISyncedBookmarksMirrorLogger.LEVEL_TRACE;
    }
    if (level <= Log.Level.Info) {
      return Ci.mozISyncedBookmarksMirrorLogger.LEVEL_DEBUG;
    }
    if (level <= Log.Level.Warn) {
      return Ci.mozISyncedBookmarksMirrorLogger.LEVEL_WARN;
    }
    if (level <= Log.Level.Error) {
      return Ci.mozISyncedBookmarksMirrorLogger.LEVEL_ERROR;
    }
    return Ci.mozISyncedBookmarksMirrorLogger.LEVEL_OFF;
  }

  trace(message) {
    this.log.trace(message);
  }

  debug(message) {
    this.log.debug(message);
  }

  warn(message) {
    this.log.warn(message);
  }

  error(message) {
    this.log.error(message);
  }
}

/**
 * A mirror maintains a copy of the complete tree as stored on the Sync server.
 * It is persistent.
 *
 * The mirror schema is a hybrid of how Sync and Places represent bookmarks.
 * The `items` table contains item attributes (title, kind, URL, etc.), while
 * the `structure` table stores parent-child relationships and position.
 * This is similar to how iOS encodes "value" and "structure" state,
 * though we handle these differently when merging. See `BookmarkMerger` for
 * details.
 *
 * There's no guarantee that the remote state is consistent. We might be missing
 * parents or children, or a bookmark and its parent might disagree about where
 * it belongs. This means we need a strategy to handle missing parents and
 * children.
 *
 * We treat the `children` of the last parent we see as canonical, and ignore
 * the child's `parentid` entirely. We also ignore missing children, and
 * temporarily reparent bookmarks with missing parents to "unfiled". When we
 * eventually see the missing items, either during a later sync or as part of
 * repair, we'll fill in the mirror's gaps and fix up the local tree.
 *
 * During merging, we won't intentionally try to fix inconsistencies on the
 * server, and opt to build as complete a tree as we can from the remote state,
 * even if we diverge from what's in the mirror. See bug 1433512 for context.
 *
 * If a sync is interrupted, we resume downloading from the server collection
 * last modified time, or the server last modified time of the most recent
 * record if newer. New incoming records always replace existing records in the
 * mirror.
 *
 * We delete the mirror database on client reset, including when the sync ID
 * changes on the server, and when the user is node reassigned, disables the
 * bookmarks engine, or signs out.
 */
class SyncedBookmarksMirror {
  constructor(db, { recordTelemetryEvent, finalizeAt =
                      AsyncShutdown.profileBeforeChange } = {}) {
    this.db = db;
    this.recordTelemetryEvent = recordTelemetryEvent;

    this.merger = new SyncedBookmarksMerger();
    this.merger.db = db.unsafeRawConnection.QueryInterface(
      Ci.mozIStorageConnection);
    this.merger.logger = new MirrorLoggerAdapter(MirrorLog);

    // Automatically close the database connection on shutdown.
    this.finalizeAt = finalizeAt;
    this.finalizeBound = () => this.finalize();
    this.finalizeAt.addBlocker("SyncedBookmarksMirror: finalize",
                               this.finalizeBound);
  }

  /**
   * Sets up the mirror database connection and upgrades the mirror to the
   * newest schema version. Automatically recreates the mirror if it's corrupt;
   * throws on failure.
   *
   * @param  {String} options.path
   *         The path to the mirror database file, either absolute or relative
   *         to the profile path.
   * @param  {Function} options.recordTelemetryEvent
   *         A function with the signature `(object: String, method: String,
   *         value: String?, extra: Object?)`, used to emit telemetry events.
   * @param  {AsyncShutdown.Barrier} [options.finalizeAt]
   *         A shutdown phase, barrier, or barrier client that should
   *         automatically finalize the mirror when triggered. Exposed for
   *         testing.
   * @return {SyncedBookmarksMirror}
   *         A mirror ready for use.
   */
  static async open(options) {
    let db = await Sqlite.cloneStorageConnection({
      connection: PlacesUtils.history.DBConnection,
      readOnly: false,
    });
    let path = OS.Path.join(OS.Constants.Path.profileDir, options.path);
    let whyFailed = "initialize";
    try {
      await db.execute(`PRAGMA foreign_keys = ON`);
      try {
        await attachAndInitMirrorDatabase(db, path);
      } catch (ex) {
        if (isDatabaseCorrupt(ex)) {
          MirrorLog.warn("Error attaching mirror to Places; removing and " +
                         "recreating mirror", ex);
          options.recordTelemetryEvent("mirror", "open", "retry",
                                       { why: "corrupt" });

          whyFailed = "remove";
          await OS.File.remove(path);

          whyFailed = "replace";
          await attachAndInitMirrorDatabase(db, path);
        } else {
          MirrorLog.error("Unrecoverable error attaching mirror to Places", ex);
          throw ex;
        }
      }
      try {
        let info = await OS.File.stat(path);
        let size = Math.floor(info.size / 1024);
        options.recordTelemetryEvent("mirror", "open", "success", { size });
      } catch (ex) {
        MirrorLog.warn("Error recording stats for mirror database size", ex);
      }
    } catch (ex) {
      options.recordTelemetryEvent("mirror", "open", "error",
                                   { why: whyFailed });
      await db.close();
      throw ex;
    }
    return new SyncedBookmarksMirror(db, options);
  }

  /**
   * Returns the newer of the bookmarks collection last modified time, or the
   * server modified time of the newest record. The bookmarks engine uses this
   * timestamp as the "high water mark" for all downloaded records. Each sync
   * downloads and stores records that are strictly newer than this time.
   *
   * @return {Number}
   *         The high water mark time, in seconds.
   */
  async getCollectionHighWaterMark() {
    // The first case, where we have records with server modified times newer
    // than the collection last modified time, occurs when a sync is interrupted
    // before we call `setCollectionLastModified`. We subtract one second, the
    // maximum time precision guaranteed by the server, so that we don't miss
    // other records with the same time as the newest one we downloaded.
    let rows = await this.db.executeCached(`
      SELECT MAX(
        IFNULL((SELECT MAX(serverModified) - 1000 FROM items), 0),
        IFNULL((SELECT CAST(value AS INTEGER) FROM meta
                WHERE key = :modifiedKey), 0)
      ) AS highWaterMark`,
      { modifiedKey: SyncedBookmarksMirror.META_KEY.LAST_MODIFIED });
    let highWaterMark = rows[0].getResultByName("highWaterMark");
    return highWaterMark / 1000;
  }

  /**
   * Updates the bookmarks collection last modified time. Note that this may
   * be newer than the modified time of the most recent record.
   *
   * @param {Number|String} lastModifiedSeconds
   *        The collection last modified time, in seconds.
   */
  async setCollectionLastModified(lastModifiedSeconds) {
    let lastModified = Math.floor(lastModifiedSeconds * 1000);
    if (!Number.isInteger(lastModified)) {
      throw new TypeError("Invalid collection last modified time");
    }
    await this.db.executeBeforeShutdown(
      "SyncedBookmarksMirror: setCollectionLastModified",
      db => db.executeCached(`
        REPLACE INTO meta(key, value)
        VALUES(:modifiedKey, :lastModified)`,
        { modifiedKey: SyncedBookmarksMirror.META_KEY.LAST_MODIFIED,
          lastModified })
    );
  }

  /**
   * Returns the bookmarks collection sync ID. This corresponds to
   * `PlacesSyncUtils.bookmarks.getSyncId`.
   *
   * @return {String}
   *         The sync ID, or `""` if one isn't set.
   */
  async getSyncId() {
    let rows = await this.db.executeCached(`
      SELECT value FROM meta WHERE key = :syncIdKey`,
      { syncIdKey: SyncedBookmarksMirror.META_KEY.SYNC_ID });
    return rows.length ? rows[0].getResultByName("value") : "";
  }

  /**
   * Ensures that the sync ID in the mirror is up-to-date with the server and
   * Places, and discards the mirror on mismatch.
   *
   * The bookmarks engine store the same sync ID in Places and the mirror to
   * "tie" the two together. This allows Sync to do the right thing if the
   * database files are copied between profiles connected to different accounts.
   *
   * See `PlacesSyncUtils.bookmarks.ensureCurrentSyncId` for an explanation of
   * how Places handles sync ID mismatches.
   *
   * @param {String} newSyncId
   *        The server's sync ID.
   */
  async ensureCurrentSyncId(newSyncId) {
    if (!newSyncId || typeof newSyncId != "string") {
      throw new TypeError("Invalid new bookmarks sync ID");
    }
    let existingSyncId = await this.getSyncId();
    if (existingSyncId == newSyncId) {
      MirrorLog.trace("Sync ID up-to-date in mirror", { existingSyncId });
      return;
    }
    MirrorLog.info("Sync ID changed from ${existingSyncId} to " +
                   "${newSyncId}; resetting mirror",
                   { existingSyncId, newSyncId });
    await this.db.executeBeforeShutdown(
      "SyncedBookmarksMirror: ensureCurrentSyncId",
      db => db.executeTransaction(async function() {
        await resetMirror(db);
        await db.execute(`
          REPLACE INTO meta(key, value)
          VALUES(:syncIdKey, :newSyncId)`,
          { syncIdKey: SyncedBookmarksMirror.META_KEY.SYNC_ID, newSyncId });
      })
    );
  }

  /**
   * Stores incoming or uploaded Sync records in the mirror. Rejects if any
   * records are invalid.
   *
   * @param {PlacesItem[]} records
   *        Sync records to store in the mirror.
   * @param {Boolean} [options.needsMerge]
   *        Indicates if the records were changed remotely since the last sync,
   *        and should be merged into the local tree. This option is set to
   *        `true` for incoming records, and `false` for successfully uploaded
   *        records. Tests can also pass `false` to set up an existing mirror.
   */
  async store(records, { needsMerge = true } = {}) {
    let options = { needsMerge };
    await this.db.executeBeforeShutdown(
      "SyncedBookmarksMirror: store",
      db => db.executeTransaction(async () => {
        for await (let record of yieldingIterator(records)) {
          let guid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.id);
          if (guid == PlacesUtils.bookmarks.rootGuid) {
            // The engine should hard DELETE Places roots from the server.
            throw new TypeError("Can't store Places root");
          }
          MirrorLog.trace(`Storing in mirror: ${record.cleartextToString()}`);
          switch (record.type) {
            case "bookmark":
              await this.storeRemoteBookmark(record, options);
              continue;

            case "query":
              await this.storeRemoteQuery(record, options);
              continue;

            case "folder":
              await this.storeRemoteFolder(record, options);
              continue;

            case "livemark":
              await this.storeRemoteLivemark(record, options);
              continue;

            case "separator":
              await this.storeRemoteSeparator(record, options);
              continue;

            default:
              if (record.deleted) {
                await this.storeRemoteTombstone(record, options);
                continue;
              }
          }
          MirrorLog.warn("Ignoring record with unknown type", record.type);
        }
      }
    ));
  }

  /**
   * Builds a complete merged tree from the local and remote trees, resolves
   * value and structure conflicts, dedupes local items, applies the merged
   * tree back to Places, and notifies observers about the changes.
   *
   * Merging and application happen in a transaction, meaning code that uses the
   * main Places connection, including the UI, will fail to write to the
   * database until the transaction commits. Asynchronous consumers will retry
   * on `SQLITE_BUSY`; synchronous consumers will fail after waiting for 100ms.
   * See bug 1305563, comment 122 for details.
   *
   * @param  {Number} [options.localTimeSeconds]
   *         The current local time, in seconds.
   * @param  {Number} [options.remoteTimeSeconds]
   *         The current server time, in seconds.
   * @param  {String[]} [options.weakUpload]
   *         GUIDs of bookmarks to weakly upload.
   * @param  {Number} [options.maxFrecenciesToRecalculate]
   *         The maximum number of bookmark URL frecencies to recalculate after
   *         this merge. Frecency calculation blocks other Places writes, so we
   *         limit the number of URLs we process at once. We'll process either
   *         the next set of URLs after the next merge, or all remaining URLs
   *         when Places automatically fixes invalid frecencies on idle;
   *         whichever comes first.
   * @return {Object.<String, BookmarkChangeRecord>}
   *         A changeset containing locally changed and reconciled records to
   *         upload to the server, and to store in the mirror once upload
   *         succeeds.
   */
  async apply({ localTimeSeconds = Date.now() / 1000,
                remoteTimeSeconds = 0,
                weakUpload = [],
                maxFrecenciesToRecalculate =
                  DEFAULT_MAX_FRECENCIES_TO_RECALCULATE } = {}) {
    // We intentionally don't use `executeBeforeShutdown` in this function,
    // since merging can take a while for large trees, and we don't want to
    // block shutdown. Since all new items are in the mirror, we'll just try
    // to merge again on the next sync.

    let observersToNotify = new BookmarkObserverRecorder(this.db,
      { maxFrecenciesToRecalculate });

    let hasChanges = weakUpload.length > 0 || (await this.hasChanges());
    if (!hasChanges) {
      MirrorLog.debug("No changes detected in both mirror and Places");
      await observersToNotify.updateFrecencies();
      return {};
    }

    if (!(await this.validLocalRoots())) {
      throw new SyncedBookmarksMirror.ConsistencyError(
        "Local tree has misparented root");
    }

    // The flow ID is used to correlate telemetry events for each sync.
    let flowID = PlacesUtils.history.makeGuid();

    let changeRecords;
    try {
      changeRecords = await this.tryApply(flowID, localTimeSeconds,
                                          remoteTimeSeconds,
                                          observersToNotify,
                                          weakUpload);
    } catch (ex) {
      // Include the error message in the event payload, since we can't
      // easily correlate event telemetry to engine errors in the Sync ping.
      let why = (typeof ex.message == "string" ? ex.message :
                 String(ex)).slice(0, 85);
      this.recordTelemetryEvent("mirror", "apply", "error", { flowID, why });
      throw ex;
    }

    return changeRecords;
  }

  async tryApply(flowID, localTimeSeconds, remoteTimeSeconds, observersToNotify,
                 weakUpload) {
    let { missingParents, missingChildren, parentsWithGaps } =
      await this.fetchRemoteOrphans();
    if (missingParents.length) {
      MirrorLog.warn("Temporarily reparenting remote items with missing " +
                     "parents to unfiled", missingParents);
    }
    if (missingChildren.length) {
      MirrorLog.warn("Remote tree missing items", missingChildren);
    }
    if (parentsWithGaps.length) {
      MirrorLog.warn("Remote tree has parents with gaps in positions",
                     parentsWithGaps);
    }

    let { missingLocal, missingRemote, wrongSyncStatus } =
      await this.fetchSyncStatusMismatches();
    if (missingLocal.length) {
      MirrorLog.warn("Remote tree has merged items that don't exist locally",
                     missingLocal);
    }
    if (missingRemote.length) {
      MirrorLog.warn("Local tree has synced items that don't exist remotely",
                     missingRemote);
    }
    if (wrongSyncStatus.length) {
      MirrorLog.warn("Local tree has wrong sync statuses for items that " +
                     "exist remotely", wrongSyncStatus);
    }

    this.recordTelemetryEvent("mirror", "apply", "problems", {
      flowID,
      missingParents: missingParents.length,
      missingChildren: missingChildren.length,
      parentsWithGaps: parentsWithGaps.length,
      missingLocal: missingLocal.length,
      missingRemote: missingRemote.length,
      wrongSyncStatus: wrongSyncStatus.length,
    });

    MirrorLog.info("Merging bookmarks in Rust");
    let result = await new Promise((resolve, reject) => {
      let callback = {
        handleResult(result) {
          resolve(result);
        },
        handleError(code, message) {
          if (code == Cr.NS_ERROR_STORAGE_BUSY) {
            reject(new SyncedBookmarksMirror.MergeConflictError(
              "Local tree changed during merge"));
          } else {
            reject(new SyncedBookmarksMirror.MergeError(message));
          }
        },
      };
      this.merger.merge(localTimeSeconds, remoteTimeSeconds, weakUpload,
                        callback);
    });
    let telem = result.QueryInterface(Ci.nsIPropertyBag);
    const telemPropToEventValue = [
      ["fetchLocalTreeTime", "fetchLocalTree"],
      ["fetchNewLocalContentsTime", "fetchNewLocalContents"],
      ["fetchRemoteTreeTime", "fetchRemoteTree"],
      ["fetchNewRemoteContentsTime", "fetchNewRemoteContents"],
    ];
    for (let [prop, value] of telemPropToEventValue) {
      this.recordTelemetryEvent("mirror", "apply", value,
        { flowID, time: telem.getProperty(prop) });
    }
    this.recordTelemetryEvent("mirror", "apply", "merge",
      { flowID, time: telem.getProperty("mergeTime"),
        nodes: telem.getProperty("mergedNodesCount"),
        deletions: telem.getProperty("mergedDeletionsCount"),
        dupes: telem.getProperty("dupesCount") });
    this.recordTelemetryEvent("mirror", "merge", "structure", {
      remoteRevives: telem.getProperty("remoteRevivesCount"),
      localDeletes: telem.getProperty("localDeletesCount"),
      localRevives: telem.getProperty("localRevivesCount"),
      remoteDeletes: telem.getProperty("remoteDeletesCount"),
    });

    // At this point, the database is consistent, so we can notify observers and
    // inflate records for outgoing items.

    await withTiming(
      "Notifying Places observers",
      async () => {
        try {
          // Note that we don't use a transaction when fetching info for
          // observers, so it's possible we might notify with stale info if the
          // main connection changes Places between the time we finish merging,
          // and the time we notify observers.
          await observersToNotify.notifyAll();
        } catch (ex) {
          MirrorLog.warn("Error notifying Places observers", ex);
        } finally {
          await this.db.executeTransaction(async () => {
            await this.db.execute(`DELETE FROM itemsAdded`);
            await this.db.execute(`DELETE FROM guidsChanged`);
            await this.db.execute(`DELETE FROM itemsChanged`);
            await this.db.execute(`DELETE FROM itemsRemoved`);
            await this.db.execute(`DELETE FROM itemsMoved`);
          });
        }
      },
      time => this.recordTelemetryEvent("mirror", "apply",
        "notifyObservers", { flowID, time })
    );

    return withTiming(
      "Fetching records for local items to upload",
      async () => {
        try {
          let changeRecords = await this.fetchLocalChangeRecords();
          return changeRecords;
        } finally {
          await this.db.execute(`DELETE FROM itemsToUpload`);
        }
      },
      (time, records) => this.recordTelemetryEvent("mirror", "apply",
        "fetchLocalChangeRecords", { flowID,
          count: Object.keys(records).length })
    );
  }

  /**
   * Discards the mirror contents. This is called when the user is node
   * reassigned, disables the bookmarks engine, or signs out.
   */
  async reset() {
    await this.db.executeBeforeShutdown(
      "SyncedBookmarksMirror: reset",
      db => db.executeTransaction(() => resetMirror(db))
    );
  }

  /**
   * Fetches the GUIDs of all items in the remote tree that need to be merged
   * into the local tree.
   *
   * @return {String[]}
   *         Remotely changed GUIDs that need to be merged into Places.
   */
  async fetchUnmergedGuids() {
    let rows = await this.db.execute(`SELECT guid FROM items WHERE needsMerge`);
    return rows.map(row => row.getResultByName("guid"));
  }

  async storeRemoteBookmark(record, { needsMerge }) {
    let guid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.id);

    let url = validateURL(record.bmkUri);
    if (url) {
      await this.maybeStoreRemoteURL(url);
    }

    let parentGuid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.parentid);
    let serverModified = determineServerModified(record);
    let dateAdded = determineDateAdded(record);
    let title = validateTitle(record.title);
    let keyword = validateKeyword(record.keyword);
    let validity = url ?
      Ci.mozISyncedBookmarksMerger.VALIDITY_VALID :
      Ci.mozISyncedBookmarksMerger.VALIDITY_REPLACE;

    await this.db.executeCached(`
      REPLACE INTO items(guid, parentGuid, serverModified, needsMerge, kind,
                         dateAdded, title, keyword, validity,
                         urlId)
      VALUES(:guid, :parentGuid, :serverModified, :needsMerge, :kind,
             :dateAdded, NULLIF(:title, ""), :keyword, :validity,
             (SELECT id FROM urls
              WHERE hash = hash(:url) AND
                    url = :url))`,
      { guid, parentGuid, serverModified, needsMerge,
        kind: Ci.mozISyncedBookmarksMerger.KIND_BOOKMARK, dateAdded, title,
        keyword, url: url ? url.href : null, validity });

    let tags = record.tags;
    if (tags && Array.isArray(tags)) {
      for (let rawTag of tags) {
        let tag = validateTag(rawTag);
        if (!tag) {
          continue;
        }
        await this.db.executeCached(`
          INSERT INTO tags(itemId, tag)
          SELECT id, :tag FROM items
          WHERE guid = :guid`,
          { tag, guid });
      }
    }
  }

  async storeRemoteQuery(record, { needsMerge }) {
    let guid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.id);

    let validity = Ci.mozISyncedBookmarksMerger.VALIDITY_VALID;

    let url = validateURL(record.bmkUri);
    if (url) {
      // The query has a valid URL. Determine if we need to rewrite and reupload
      // it.
      let params = new URLSearchParams(url.pathname);
      let type = +params.get("type");
      if (type == Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_CONTENTS) {
        // Legacy tag queries with this type use a `place:` URL with a `folder`
        // param that points to the tag folder ID. Rewrite the query to directly
        // reference the tag stored in its `folderName`, then flag the rewritten
        // query for reupload.
        let tagFolderName = validateTag(record.folderName);
        if (tagFolderName) {
          url.href = `place:tag=${tagFolderName}`;
          validity = Ci.mozISyncedBookmarksMerger.VALIDITY_REUPLOAD;
        } else {
          // The tag folder name is invalid, so replace or delete the remote
          // copy.
          url = null;
          validity = Ci.mozISyncedBookmarksMerger.VALIDITY_REPLACE;
        }
      } else {
        let folder = params.get("folder");
        if (folder && !params.has("excludeItems")) {
          // We don't sync enough information to rewrite other queries with a
          // `folder` param (bug 1377175). Referencing a nonexistent folder ID
          // causes the query to return all items in the database, so we add
          // `excludeItems=1` to stop it from doing so. We also flag the
          // rewritten query for reupload.
          url.href = `${url.href}&excludeItems=1`;
          validity = Ci.mozISyncedBookmarksMerger.VALIDITY_REUPLOAD;
        }
      }

      // Other queries are implicitly valid, and don't need to be reuploaded
      // or replaced.

      await this.maybeStoreRemoteURL(url);
    } else {
      // If the query doesn't have a valid URL, we must replace the remote copy
      // with either a valid local copy, or a tombstone if the query doesn't
      // exist locally.
      validity = Ci.mozISyncedBookmarksMerger.VALIDITY_REPLACE;
    }

    let parentGuid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.parentid);
    let serverModified = determineServerModified(record);
    let dateAdded = determineDateAdded(record);
    let title = validateTitle(record.title);

    await this.db.executeCached(`
      REPLACE INTO items(guid, parentGuid, serverModified, needsMerge, kind,
                         dateAdded, title,
                         urlId,
                         validity)
      VALUES(:guid, :parentGuid, :serverModified, :needsMerge, :kind,
             :dateAdded, NULLIF(:title, ""),
             (SELECT id FROM urls
              WHERE hash = hash(:url) AND
                    url = :url),
             :validity)`,
      { guid, parentGuid, serverModified, needsMerge,
        kind: Ci.mozISyncedBookmarksMerger.KIND_QUERY, dateAdded, title,
        url: url ? url.href : null, validity });
  }

  async storeRemoteFolder(record, { needsMerge }) {
    let guid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.id);
    let parentGuid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.parentid);
    let serverModified = determineServerModified(record);
    let dateAdded = determineDateAdded(record);
    let title = validateTitle(record.title);

    await this.db.executeCached(`
      REPLACE INTO items(guid, parentGuid, serverModified, needsMerge, kind,
                         dateAdded, title)
      VALUES(:guid, :parentGuid, :serverModified, :needsMerge, :kind,
             :dateAdded, NULLIF(:title, ""))`,
      { guid, parentGuid, serverModified, needsMerge,
        kind: Ci.mozISyncedBookmarksMerger.KIND_FOLDER, dateAdded, title });

    let children = record.children;
    if (children && Array.isArray(children)) {
      for (let position = 0; position < children.length; ++position) {
        await maybeYield();
        let childRecordId = children[position];
        let childGuid = PlacesSyncUtils.bookmarks.recordIdToGuid(childRecordId);
        await this.db.executeCached(`
          REPLACE INTO structure(guid, parentGuid, position)
          VALUES(:childGuid, :parentGuid, :position)`,
          { childGuid, parentGuid: guid, position });
      }
    }
  }

  async storeRemoteLivemark(record, { needsMerge }) {
    let guid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.id);
    let parentGuid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.parentid);
    let serverModified = determineServerModified(record);
    let feedURL = validateURL(record.feedUri);
    let dateAdded = determineDateAdded(record);
    let title = validateTitle(record.title);
    let siteURL = validateURL(record.siteUri);

    let validity = feedURL ?
      Ci.mozISyncedBookmarksMerger.VALIDITY_VALID :
      Ci.mozISyncedBookmarksMerger.VALIDITY_REPLACE;

    await this.db.executeCached(`
      REPLACE INTO items(guid, parentGuid, serverModified, needsMerge, kind,
                         dateAdded, title, feedURL, siteURL, validity)
      VALUES(:guid, :parentGuid, :serverModified, :needsMerge, :kind,
             :dateAdded, NULLIF(:title, ""), :feedURL, :siteURL, :validity)`,
      { guid, parentGuid, serverModified, needsMerge,
        kind: Ci.mozISyncedBookmarksMerger.KIND_LIVEMARK,
        dateAdded, title, feedURL: feedURL ? feedURL.href : null,
        siteURL: siteURL ? siteURL.href : null, validity });
  }

  async storeRemoteSeparator(record, { needsMerge }) {
    let guid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.id);
    let parentGuid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.parentid);
    let serverModified = determineServerModified(record);
    let dateAdded = determineDateAdded(record);

    await this.db.executeCached(`
      REPLACE INTO items(guid, parentGuid, serverModified, needsMerge, kind,
                         dateAdded)
      VALUES(:guid, :parentGuid, :serverModified, :needsMerge, :kind,
             :dateAdded)`,
      { guid, parentGuid, serverModified, needsMerge,
        kind: Ci.mozISyncedBookmarksMerger.KIND_SEPARATOR, dateAdded });
  }

  async storeRemoteTombstone(record, { needsMerge }) {
    let guid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.id);
    let serverModified = determineServerModified(record);

    await this.db.executeCached(`
      REPLACE INTO items(guid, serverModified, needsMerge, isDeleted)
      VALUES(:guid, :serverModified, :needsMerge, 1)`,
      { guid, serverModified, needsMerge });
  }

  async maybeStoreRemoteURL(url) {
    await this.db.executeCached(`
      INSERT OR IGNORE INTO urls(guid, url, hash, revHost)
      VALUES(IFNULL((SELECT guid FROM urls
                     WHERE hash = hash(:url) AND
                                  url = :url),
                    GENERATE_GUID()), :url, hash(:url), :revHost)`,
      { url: url.href, revHost: PlacesUtils.getReversedHost(url) });
  }

  async fetchRemoteOrphans() {
    let infos = {
      missingParents: [],
      missingChildren: [],
      parentsWithGaps: [],
    };

    let orphanRows = await this.db.execute(`
      SELECT v.guid AS guid, 1 AS missingParent, 0 AS missingChild,
             0 AS parentWithGaps
      FROM items v
      LEFT JOIN structure s ON s.guid = v.guid
      WHERE NOT v.isDeleted AND
            s.guid IS NULL
      UNION ALL
      SELECT s.guid AS guid, 0 AS missingParent, 1 AS missingChild,
             0 AS parentsWithGaps
      FROM structure s
      LEFT JOIN items v ON v.guid = s.guid
      WHERE v.guid IS NULL
      UNION ALL
      SELECT s.parentGuid AS guid, 0 AS missingParent, 0 AS missingChild,
             1 AS parentWithGaps
      FROM structure s
      WHERE s.guid <> :rootGuid
      GROUP BY s.parentGuid
      HAVING (sum(DISTINCT position + 1) -
                  (count(*) * (count(*) + 1) / 2)) <> 0
      ORDER BY guid`,
      { rootGuid: PlacesUtils.bookmarks.rootGuid });

    for await (let row of yieldingIterator(orphanRows)) {
      let guid = row.getResultByName("guid");
      let missingParent = row.getResultByName("missingParent");
      if (missingParent) {
        infos.missingParents.push(guid);
      }
      let missingChild = row.getResultByName("missingChild");
      if (missingChild) {
        infos.missingChildren.push(guid);
      }
      let parentWithGaps = row.getResultByName("parentWithGaps");
      if (parentWithGaps) {
        infos.parentsWithGaps.push(guid);
      }
    }

    return infos;
  }

  /**
   * Checks the sync statuses of all items for consistency. All merged items in
   * the remote tree should exist as either items or tombstones in the local
   * tree, and all NORMAL items and tombstones in the local tree should exist
   * in the remote tree, if the mirror has any merged items.
   *
   * @return {Object.<String, String[]>}
   *         An object containing GUIDs for each problem type:
   *           - `missingLocal`: Merged items in the remote tree that aren't
   *             mentioned in the local tree.
   *           - `missingRemote`: NORMAL items in the local tree that aren't
   *             mentioned in the remote tree.
   */
  async fetchSyncStatusMismatches() {
    let infos = {
      missingLocal: [],
      missingRemote: [],
      wrongSyncStatus: [],
    };

    let problemRows = await this.db.execute(`
      SELECT v.guid, 1 AS missingLocal, 0 AS missingRemote, 0 AS wrongSyncStatus
      FROM items v
      LEFT JOIN moz_bookmarks b ON b.guid = v.guid
      LEFT JOIN moz_bookmarks_deleted d ON d.guid = v.guid
      WHERE NOT v.needsMerge AND
            NOT v.isDeleted AND
            b.guid IS NULL AND
            d.guid IS NULL
      UNION ALL
      SELECT b.guid, 0 AS missingLocal, 1 AS missingRemote, 0 AS wrongSyncStatus
      FROM moz_bookmarks b
      LEFT JOIN items v ON v.guid = b.guid
      WHERE EXISTS(SELECT 1 FROM items
                   WHERE NOT needsMerge AND
                         guid <> :rootGuid) AND
            b.syncStatus = :syncStatus AND
            v.guid IS NULL
      UNION ALL
      SELECT d.guid, 0 AS missingLocal, 1 AS missingRemote, 0 AS wrongSyncStatus
      FROM moz_bookmarks_deleted d
      LEFT JOIN items v ON v.guid = d.guid
      WHERE EXISTS(SELECT 1 FROM items
                   WHERE NOT needsMerge AND
                         guid <> :rootGuid) AND
            v.guid IS NULL
      UNION ALL
      SELECT b.guid, 0 AS missingLocal, 0 AS missingRemote, 1 AS wrongSyncStatus
      FROM moz_bookmarks b
      JOIN items v ON v.guid = b.guid
      WHERE EXISTS(SELECT 1 FROM items
                   WHERE NOT needsMerge AND
                         guid <> :rootGuid) AND
            b.guid <> :rootGuid AND
            b.syncStatus <> :syncStatus`,
      { syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
        rootGuid: PlacesUtils.bookmarks.rootGuid });

    for await (let row of yieldingIterator(problemRows)) {
      let guid = row.getResultByName("guid");
      let missingLocal = row.getResultByName("missingLocal");
      if (missingLocal) {
        infos.missingLocal.push(guid);
      }
      let missingRemote = row.getResultByName("missingRemote");
      if (missingRemote) {
        infos.missingRemote.push(guid);
      }
      let wrongSyncStatus = row.getResultByName("wrongSyncStatus");
      if (wrongSyncStatus) {
        infos.wrongSyncStatus.push(guid);
      }
    }

    return infos;
  }

  /*
   * Checks if Places or mirror have any unsynced/unmerged changes.
   *
   * @return {Boolean}
   *         `true` if something has changed.
   */
  async hasChanges() {
    // In the first subquery, we check incoming items with needsMerge = true
    // except the tombstones who don't correspond to any local bookmark because
    // we don't store them yet, hence never "merged" (see bug 1343103).
    let rows = await this.db.execute(`
      SELECT
      EXISTS (
       SELECT 1
       FROM items v
       LEFT JOIN moz_bookmarks b ON v.guid = b.guid
       WHERE v.needsMerge AND
       (NOT v.isDeleted OR b.guid NOT NULL)
      ) OR EXISTS (
       WITH RECURSIVE
       ${LocalItemsSQLFragment}
       SELECT 1
       FROM localItems
       WHERE syncChangeCounter > 0
      ) OR EXISTS (
       SELECT 1
       FROM moz_bookmarks_deleted
      )
      AS hasChanges
    `);
    return !!rows[0].getResultByName("hasChanges");
  }

  /**
   * Ensures that all local roots are parented correctly. Misparented roots
   * (bug 1453994, bug 1472127) might produce an invalid tree, so we check
   * before merging, and rely on Places to reparent any invalid roots after
   * the next restart or maintenance run.
   *
   * @return {Boolean}
   *         `true` if the Places root, and four syncable roots, are parented
   *         correctly.
   */
  async validLocalRoots() {
    let rows = await this.db.execute(`
      SELECT EXISTS(SELECT 1 FROM moz_bookmarks
                    WHERE guid = '${PlacesUtils.bookmarks.rootGuid}' AND
                          parent = 0) AND
             (SELECT COUNT(*) FROM moz_bookmarks b
              JOIN moz_bookmarks p ON p.id = b.parent
              WHERE b.guid IN (${PlacesUtils.bookmarks.userContentRoots.map(
                v => `'${v}'`)}) AND
                    p.guid = '${PlacesUtils.bookmarks.rootGuid}') =
             ${PlacesUtils.bookmarks.userContentRoots.length} AS areValid`);
    return !!rows[0].getResultByName("areValid");
  }

  /**
   * Inflates Sync records for all staged outgoing items.
   *
   * @return {Object.<String, BookmarkChangeRecord>}
   *         A changeset containing Sync record cleartexts for outgoing items
   *         and tombstones, keyed by their Sync record IDs.
   */
  async fetchLocalChangeRecords() {
    let changeRecords = {};
    let childRecordIdsByLocalParentId = new Map();
    let tagsByLocalId = new Map();

    let childGuidRows = await this.db.execute(`
      SELECT parentId, guid FROM structureToUpload
      ORDER BY parentId, position`);

    for await (let row of yieldingIterator(childGuidRows)) {
      let localParentId = row.getResultByName("parentId");
      let childRecordId = PlacesSyncUtils.bookmarks.guidToRecordId(
        row.getResultByName("guid"));
      let childRecordIds = childRecordIdsByLocalParentId.get(localParentId);
      if (childRecordIds) {
        childRecordIds.push(childRecordId);
      } else {
        childRecordIdsByLocalParentId.set(localParentId, [childRecordId]);
      }
    }

    let tagRows = await this.db.execute(`
      SELECT id, tag FROM tagsToUpload`);

    for await (let row of yieldingIterator(tagRows)) {
      let localId = row.getResultByName("id");
      let tag = row.getResultByName("tag");
      let tags = tagsByLocalId.get(localId);
      if (tags) {
        tags.push(tag);
      } else {
        tagsByLocalId.set(localId, [tag]);
      }
    }

    let itemRows = await this.db.execute(`
      SELECT id, syncChangeCounter, guid, isDeleted, type, isQuery,
             tagFolderName, keyword, url, IFNULL(title, "") AS title,
             position, parentGuid,
             IFNULL(parentTitle, "") AS parentTitle, dateAdded
      FROM itemsToUpload`);

    for await (let row of yieldingIterator(itemRows)) {
      let syncChangeCounter = row.getResultByName("syncChangeCounter");

      let guid = row.getResultByName("guid");
      let recordId = PlacesSyncUtils.bookmarks.guidToRecordId(guid);

      // Tombstones don't carry additional properties.
      let isDeleted = row.getResultByName("isDeleted");
      if (isDeleted) {
        changeRecords[recordId] = new BookmarkChangeRecord(syncChangeCounter, {
          id: recordId,
          deleted: true,
        });
        continue;
      }

      let parentGuid = row.getResultByName("parentGuid");
      let parentRecordId = PlacesSyncUtils.bookmarks.guidToRecordId(parentGuid);

      let type = row.getResultByName("type");
      switch (type) {
        case PlacesUtils.bookmarks.TYPE_BOOKMARK: {
          let isQuery = row.getResultByName("isQuery");
          if (isQuery) {
            let queryCleartext = {
              id: recordId,
              type: "query",
              // We ignore `parentid` and use the parent's `children`, but older
              // Desktops and Android use `parentid` as the canonical parent.
              // iOS is stricter and requires both `children` and `parentid` to
              // match.
              parentid: parentRecordId,
              // Older Desktops use `hasDupe` (along with `parentName` for
              // deduping), if hasDupe is true, then they won't attempt deduping
              // (since they believe that a duplicate for this record should
              // exist). We set it to true to prevent them from applying their
              // deduping logic.
              hasDupe: true,
              parentName: row.getResultByName("parentTitle"),
              // Omit `dateAdded` from the record if it's not set locally.
              dateAdded: row.getResultByName("dateAdded") || undefined,
              bmkUri: row.getResultByName("url"),
              title: row.getResultByName("title"),
              // folderName should never be an empty string or null
              folderName: row.getResultByName("tagFolderName") || undefined,
            };
            changeRecords[recordId] = new BookmarkChangeRecord(
              syncChangeCounter, queryCleartext);
            continue;
          }

          let bookmarkCleartext = {
            id: recordId,
            type: "bookmark",
            parentid: parentRecordId,
            hasDupe: true,
            parentName: row.getResultByName("parentTitle"),
            dateAdded: row.getResultByName("dateAdded") || undefined,
            bmkUri: row.getResultByName("url"),
            title: row.getResultByName("title"),
          };
          let keyword = row.getResultByName("keyword");
          if (keyword) {
            bookmarkCleartext.keyword = keyword;
          }
          let localId = row.getResultByName("id");
          let tags = tagsByLocalId.get(localId);
          if (tags) {
            bookmarkCleartext.tags = tags;
          }
          changeRecords[recordId] = new BookmarkChangeRecord(
            syncChangeCounter, bookmarkCleartext);
          continue;
        }

        case PlacesUtils.bookmarks.TYPE_FOLDER: {
          let folderCleartext = {
            id: recordId,
            type: "folder",
            parentid: parentRecordId,
            hasDupe: true,
            parentName: row.getResultByName("parentTitle"),
            dateAdded: row.getResultByName("dateAdded") || undefined,
            title: row.getResultByName("title"),
          };
          let localId = row.getResultByName("id");
          let childRecordIds = childRecordIdsByLocalParentId.get(localId);
          folderCleartext.children = childRecordIds || [];
          changeRecords[recordId] = new BookmarkChangeRecord(
            syncChangeCounter, folderCleartext);
          continue;
        }

        case PlacesUtils.bookmarks.TYPE_SEPARATOR: {
          let separatorCleartext = {
            id: recordId,
            type: "separator",
            parentid: parentRecordId,
            hasDupe: true,
            parentName: row.getResultByName("parentTitle"),
            dateAdded: row.getResultByName("dateAdded") || undefined,
            // Older Desktops use `pos` for deduping.
            pos: row.getResultByName("position"),
          };
          changeRecords[recordId] = new BookmarkChangeRecord(
            syncChangeCounter, separatorCleartext);
          continue;
        }

        default:
          throw new TypeError("Can't create record for unknown Places item");
      }
    }

    return changeRecords;
  }

  /**
   * Closes the mirror database connection. This is called automatically on
   * shutdown, but may also be called explicitly when the mirror is no longer
   * needed.
   */
  finalize() {
    if (!this.finalizePromise) {
      this.finalizePromise = (async () => {
        this.merger.finalize();
        await this.db.close();
        this.finalizeAt.removeBlocker(this.finalizeBound);
      })();
    }
    return this.finalizePromise;
  }
}

this.SyncedBookmarksMirror = SyncedBookmarksMirror;

/** Key names for the key-value `meta` table. */
SyncedBookmarksMirror.META_KEY = {
  LAST_MODIFIED: "collection/lastModified",
  SYNC_ID: "collection/syncId",
};

/**
 * An error thrown when the merge can't proceed because the local or remote
 * tree is inconsistent.
 */
class ConsistencyError extends Error {
  constructor(message) {
    super(message);
    this.name = "ConsistencyError";
  }
}
SyncedBookmarksMirror.ConsistencyError = ConsistencyError;

class MergeError extends Error {
  constructor(message) {
    super(message);
    this.name = "MergeError";
  }
}
SyncedBookmarksMirror.MergeError = MergeError;

/**
 * An error thrown when the merge can't proceed because the local tree
 * changed during the merge.
 */
class MergeConflictError extends Error {
  constructor(message) {
    super(message);
    this.name = "MergeConflictError";
  }
}
SyncedBookmarksMirror.MergeConflictError = MergeConflictError;

/**
 * An error thrown when the mirror database is corrupt, or can't be migrated to
 * the latest schema version, and must be replaced.
 */
class DatabaseCorruptError extends Error {
  constructor(message) {
    super(message);
    this.name = "DatabaseCorruptError";
  }
}

// Indicates if the mirror should be replaced because the database file is
// corrupt.
function isDatabaseCorrupt(error) {
  if (error instanceof DatabaseCorruptError) {
    return true;
  }
  if (error.errors) {
    return error.errors.some(error =>
      error instanceof Ci.mozIStorageError &&
      (error.result == Ci.mozIStorageError.CORRUPT ||
       error.result == Ci.mozIStorageError.NOTADB));
  }
  return false;
}

/**
 * Attaches a cloned Places database connection to the mirror database,
 * migrates the mirror schema to the latest version, and creates temporary
 * tables, views, and triggers.
 *
 * @param {Sqlite.OpenedConnection} db
 *        The Places database connection.
 * @param {String} path
 *        The full path to the mirror database file.
 */
async function attachAndInitMirrorDatabase(db, path) {
  await db.execute(`ATTACH :path AS mirror`, { path });
  try {
    await db.executeTransaction(async function() {
      let currentSchemaVersion = await db.getSchemaVersion("mirror");
      if (currentSchemaVersion > 0) {
        if (currentSchemaVersion < MIRROR_SCHEMA_VERSION) {
          await migrateMirrorSchema(db, currentSchemaVersion);
        }
      } else {
        await initializeMirrorDatabase(db);
      }
      // Downgrading from a newer profile to an older profile rolls back the
      // schema version, but leaves all new columns in place. We'll run the
      // migration logic again on the next upgrade.
      await db.setSchemaVersion(MIRROR_SCHEMA_VERSION, "mirror");
      await initializeTempMirrorEntities(db);
    });
  } catch (ex) {
    await db.execute(`DETACH mirror`);
    throw ex;
  }
}

/**
 * Migrates the mirror database schema to the latest version.
 *
 * @param {Sqlite.OpenedConnection} db
 *        The mirror database connection.
 * @param {Number} currentSchemaVersion
 *        The current mirror database schema version.
 */
async function migrateMirrorSchema(db, currentSchemaVersion) {
  if (currentSchemaVersion < 3) {
    // The mirror was pref'd off by default for schema versions 1 and 2.
    throw new DatabaseCorruptError(`Can't migrate from schema version ${
      currentSchemaVersion}; too old`);
  }
}

/**
 * Initializes a new mirror database, creating persistent tables, indexes, and
 * roots.
 *
 * @param {Sqlite.OpenedConnection} db
 *        The mirror database connection.
 */
async function initializeMirrorDatabase(db) {
  // Key-value metadata table. Stores the server collection last modified time
  // and sync ID.
  await db.execute(`CREATE TABLE mirror.meta(
    key TEXT NOT NULL PRIMARY KEY,
    value NOT NULL
  )`);

  // Note: description and loadInSidebar are not used as of Firefox 63, but
  // remain to avoid rebuilding the database if the user happens to downgrade.
  await db.execute(`CREATE TABLE mirror.items(
    id INTEGER PRIMARY KEY,
    guid TEXT UNIQUE NOT NULL,
    /* The "parentid" from the record. */
    parentGuid TEXT,
    /* The server modified time, in milliseconds. */
    serverModified INTEGER NOT NULL DEFAULT 0,
    needsMerge BOOLEAN NOT NULL DEFAULT 0,
    validity INTEGER NOT NULL DEFAULT ${
      Ci.mozISyncedBookmarksMerger.VALIDITY_VALID},
    isDeleted BOOLEAN NOT NULL DEFAULT 0,
    kind INTEGER NOT NULL DEFAULT -1,
    /* The creation date, in milliseconds. */
    dateAdded INTEGER NOT NULL DEFAULT 0,
    title TEXT,
    urlId INTEGER REFERENCES urls(id)
                  ON DELETE SET NULL,
    keyword TEXT,
    description TEXT,
    loadInSidebar BOOLEAN,
    smartBookmarkName TEXT,
    feedURL TEXT,
    siteURL TEXT
  )`);

  await db.execute(`CREATE TABLE mirror.structure(
    guid TEXT NOT NULL PRIMARY KEY,
    parentGuid TEXT NOT NULL REFERENCES items(guid)
                             ON DELETE CASCADE,
    position INTEGER NOT NULL
  ) WITHOUT ROWID`);

  await db.execute(`CREATE TABLE mirror.urls(
    id INTEGER PRIMARY KEY,
    guid TEXT NOT NULL,
    url TEXT NOT NULL,
    hash INTEGER NOT NULL,
    revHost TEXT NOT NULL
  )`);

  await db.execute(`CREATE TABLE mirror.tags(
    itemId INTEGER NOT NULL REFERENCES items(id)
                            ON DELETE CASCADE,
    tag TEXT NOT NULL
  )`);

  await db.execute(`CREATE INDEX mirror.urlHashes ON urls(hash)`);

  await createMirrorRoots(db);
}

/**
 * Sets up the syncable roots. All items in the mirror we apply will descend
 * from these roots - however, malformed records from the server which create
 * a different root *will* be created in the mirror - just not applied.
 *
 *
 * @param {Sqlite.OpenedConnection} db
 *        The mirror database connection.
 */
async function createMirrorRoots(db) {
  const syncableRoots = [{
    guid: PlacesUtils.bookmarks.rootGuid,
    // The Places root is its own parent, to satisfy the foreign key and
    // `NOT NULL` constraints on `structure`.
    parentGuid: PlacesUtils.bookmarks.rootGuid,
    position: -1,
    needsMerge: false,
  }, ...PlacesUtils.bookmarks.userContentRoots.map((guid, position) => {
    return {
      guid,
      parentGuid: PlacesUtils.bookmarks.rootGuid,
      position,
      needsMerge: true,
    };
  })];

  for (let { guid, parentGuid, position, needsMerge } of syncableRoots) {
    await db.executeCached(`
      INSERT INTO items(guid, parentGuid, kind, needsMerge)
      VALUES(:guid, :parentGuid, :kind, :needsMerge)`,
      { guid, parentGuid, kind: Ci.mozISyncedBookmarksMerger.KIND_FOLDER,
        needsMerge });

    await db.executeCached(`
      INSERT INTO structure(guid, parentGuid, position)
      VALUES(:guid, :parentGuid, :position)`,
      { guid, parentGuid, position });
  }
}

/**
 * Creates temporary tables, views, and triggers to apply the mirror to Places.
 *
 * The bulk of the logic to apply all remotely changed items is defined in
 * `INSTEAD OF DELETE` triggers on the `itemsToMerge` and `structureToMerge`
 * views. When we execute `DELETE FROM newRemote{Items, Structure}`, SQLite
 * fires the triggers for each row in the view. This is equivalent to, but more
 * efficient than, issuing `SELECT * FROM newRemote{Items, Structure}`,
 * followed by separate `INSERT` and `UPDATE` statements.
 *
 * Using triggers to execute all these statements avoids the overhead of passing
 * results between the storage and main threads, and wrapping each result row in
 * a `mozStorageRow` object.
 *
 * @param {Sqlite.OpenedConnection} db
 *        The mirror database connection.
 */
async function initializeTempMirrorEntities(db) {
  // Stores the value and structure states of all nodes in the merged tree.
  await db.execute(`CREATE TEMP TABLE mergeStates(
    mergedGuid TEXT PRIMARY KEY,
    localGuid TEXT,
    remoteGuid TEXT,
    mergedParentGuid TEXT NOT NULL,
    level INTEGER NOT NULL,
    position INTEGER NOT NULL,
    useRemote BOOLEAN NOT NULL, /* Take the remote state when merging? */
    shouldUpload BOOLEAN NOT NULL, /* Flag the item for upload? */
    /* The node should exist on at least one side. */
    CHECK(localGuid NOT NULL OR remoteGuid NOT NULL)
  ) WITHOUT ROWID`);

  // Stages all items to delete locally and remotely. Items to delete locally
  // don't need tombstones: since we took the remote deletion, the tombstone
  // already exists on the server. Items to delete remotely, or non-syncable
  // items to delete on both sides, need tombstones.
  await db.execute(`CREATE TEMP TABLE itemsToRemove(
    guid TEXT PRIMARY KEY,
    localLevel INTEGER NOT NULL,
    shouldUploadTombstone BOOLEAN NOT NULL
  ) WITHOUT ROWID`);

  await db.execute(`
    CREATE TEMP TRIGGER noteItemRemoved
    AFTER INSERT ON itemsToRemove
    BEGIN
      /* Note that we can't record item removed notifications in the
         "removeLocalItems" trigger, because SQLite can delete rows in any
         order, and might fire the trigger for a removed parent before its
         children. */
      INSERT INTO itemsRemoved(itemId, parentId, position, type, placeId,
                               guid, parentGuid, level)
      SELECT b.id, b.parent, b.position, b.type, b.fk, b.guid, p.guid,
             NEW.localLevel
      FROM moz_bookmarks b
      JOIN moz_bookmarks p ON p.id = b.parent
      WHERE b.guid = NEW.guid;
    END`);

  // Removes items that are deleted on one or both sides from Places, and
  // inserts new tombstones for non-syncable items to delete remotely.
  await db.execute(`
    CREATE TEMP TRIGGER removeLocalItems
    AFTER DELETE ON itemsToRemove
    BEGIN
      /* Flag URL frecency for recalculation. */
      UPDATE moz_places SET
        frecency = -frecency
      WHERE id = (SELECT fk FROM moz_bookmarks
                  WHERE guid = OLD.guid) AND
            frecency > 0;

      /* Trigger frecency updates for all affected origins. */
      DELETE FROM moz_updateoriginsupdate_temp;

      /* Remove annos for the deleted items. This can be removed in bug
         1460577. */
      DELETE FROM moz_items_annos
      WHERE item_id = (SELECT id FROM moz_bookmarks
                       WHERE guid = OLD.guid);

      /* Don't reupload tombstones for items that are already deleted on the
         server. */
      DELETE FROM moz_bookmarks_deleted
      WHERE NOT OLD.shouldUploadTombstone AND
            guid = OLD.guid;

      /* Upload tombstones for non-syncable items. We can remove the
         "shouldUploadTombstone" check and persist tombstones unconditionally
         in bug 1343103. */
      INSERT OR IGNORE INTO moz_bookmarks_deleted(guid, dateRemoved)
      SELECT OLD.guid, STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000
      WHERE OLD.shouldUploadTombstone;

      /* Remove the item from Places. */
      DELETE FROM moz_bookmarks
      WHERE guid = OLD.guid;

      /* Flag applied deletions as merged. */
      UPDATE items SET
        needsMerge = 0
      WHERE needsMerge AND
            guid = OLD.guid AND
            /* Don't flag tombstones for items that don't exist in the local
               tree. This can be removed once we persist tombstones in bug
               1343103. */
            (NOT isDeleted OR OLD.localLevel > -1);
    END`);

  // A view of the value states for all bookmarks in the mirror. We use triggers
  // on this view to update Places. Note that we can't just `REPLACE INTO
  // moz_bookmarks`, because `REPLACE` doesn't fire the `AFTER DELETE` triggers
  // that Places uses to maintain schema coherency.
  await db.execute(`
    CREATE TEMP VIEW itemsToMerge(localId, localGuid, remoteId, remoteGuid,
                                  mergedGuid, useRemote, shouldUpload, newLevel,
                                  newType,
                                  newDateAddedMicroseconds,
                                  newTitle, oldPlaceId, newPlaceId,
                                  newKeyword) AS
    SELECT b.id, b.guid, v.id, v.guid,
           r.mergedGuid, r.useRemote, r.shouldUpload, r.level,
           (CASE WHEN v.kind IN (${[
                        Ci.mozISyncedBookmarksMerger.KIND_BOOKMARK,
                        Ci.mozISyncedBookmarksMerger.KIND_QUERY,
                      ].join(",")}) THEN ${PlacesUtils.bookmarks.TYPE_BOOKMARK}
                 WHEN v.kind IN (${[
                        Ci.mozISyncedBookmarksMerger.KIND_FOLDER,
                        Ci.mozISyncedBookmarksMerger.KIND_LIVEMARK,
                      ].join(",")}) THEN ${PlacesUtils.bookmarks.TYPE_FOLDER}
                 ELSE ${PlacesUtils.bookmarks.TYPE_SEPARATOR} END),
           /* Take the older creation date. "b.dateAdded" is in microseconds;
              "v.dateAdded" is in milliseconds. */
           (CASE WHEN b.dateAdded / 1000 < v.dateAdded THEN b.dateAdded
                 ELSE v.dateAdded * 1000 END),
           v.title, h.id, (SELECT n.id FROM moz_places n
                           WHERE n.url_hash = u.hash AND
                                 n.url = u.url),
           v.keyword
    FROM mergeStates r
    LEFT JOIN items v ON v.guid = r.remoteGuid
    LEFT JOIN moz_bookmarks b ON b.guid = r.localGuid
    LEFT JOIN moz_places h ON h.id = b.fk
    LEFT JOIN urls u ON u.id = v.urlId
    WHERE r.mergedGuid <> '${PlacesUtils.bookmarks.rootGuid}'`);

  // Changes local GUIDs to remote GUIDs, drops local tombstones for revived
  // remote items, and flags remote items as merged. In the trigger body, `OLD`
  // refers to the row for the unmerged item in `itemsToMerge`.
  await db.execute(`
    CREATE TEMP TRIGGER updateGuidsAndSyncFlags
    INSTEAD OF DELETE ON itemsToMerge
    BEGIN
      UPDATE moz_bookmarks SET
        /* We update GUIDs here, instead of in the "updateExistingLocalItems"
           trigger, because deduped items where we're keeping the local value
           state won't have "useRemote" set. */
        guid = OLD.mergedGuid,
        syncStatus = CASE WHEN OLD.useRemote
                     THEN ${PlacesUtils.bookmarks.SYNC_STATUS.NORMAL}
                     ELSE syncStatus
                     END,
        /* Flag updated local items and new structure for upload. */
        syncChangeCounter = OLD.shouldUpload,
        lastModified = STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000
      WHERE id = OLD.localId;

      /* Record item changed notifications for the updated GUIDs. */
      INSERT INTO guidsChanged(itemId, oldGuid, level)
      SELECT OLD.localId, OLD.localGuid, OLD.newLevel
      WHERE OLD.localGuid <> OLD.mergedGuid;

      /* Drop local tombstones for revived remote items. */
      DELETE FROM moz_bookmarks_deleted
      WHERE guid IN (OLD.localGuid, OLD.remoteGuid);

      /* Flag the remote item as merged. */
      UPDATE items SET
        needsMerge = 0
      WHERE needsMerge AND
            guid IN (OLD.remoteGuid, OLD.localGuid);
    END`);

  await db.execute(`
    CREATE TEMP TRIGGER updateLocalItems
    INSTEAD OF DELETE ON itemsToMerge WHEN OLD.useRemote
    BEGIN
      /* Record an item added notification for the new item. */
      INSERT INTO itemsAdded(guid, keywordChanged, level)
      SELECT OLD.mergedGuid, OLD.newKeyword NOT NULL OR
                             EXISTS(SELECT 1 FROM moz_keywords
                                    WHERE place_id = OLD.newPlaceId OR
                                          keyword = OLD.newKeyword),
             OLD.newLevel
      WHERE OLD.localId IS NULL;

      /* Record an item changed notification for the existing item. */
      INSERT INTO itemsChanged(itemId, oldTitle, oldPlaceId, keywordChanged,
                               level)
      SELECT id, title, OLD.oldPlaceId, OLD.newKeyword NOT NULL OR
               EXISTS(SELECT 1 FROM moz_keywords
                      WHERE place_id IN (OLD.oldPlaceId, OLD.newPlaceId) OR
                            keyword = OLD.newKeyword),
             OLD.newLevel
      FROM moz_bookmarks
      WHERE OLD.localId NOT NULL AND
            id = OLD.localId;

      /* Sync associates keywords with bookmarks, and doesn't sync POST data;
         Places associates keywords with (URL, POST data) pairs, and multiple
         bookmarks may have the same URL. For consistency (bug 1328737), we
         reupload all items with the old URL, new URL, and new keyword. Note
         that we intentionally use "k.place_id IN (...)" instead of
         "b.fk = OLD.newPlaceId OR fk IN (...)" in the WHERE clause because we
         only want to reupload items with keywords. */
      INSERT OR IGNORE INTO relatedIdsToReupload(id)
      SELECT b.id FROM moz_bookmarks b
      JOIN moz_keywords k ON k.place_id = b.fk
      WHERE (b.id <> OLD.localId OR OLD.localId IS NULL) AND (
              k.place_id IN (OLD.oldPlaceId, OLD.newPlaceId) OR
              k.keyword = OLD.newKeyword
            );

      /* Remove all keywords from the old and new URLs, and remove the new
         keyword from all existing URLs. */
      DELETE FROM moz_keywords WHERE place_id IN (OLD.oldPlaceId,
                                                  OLD.newPlaceId) OR
                                     keyword = OLD.newKeyword;

      /* Remove existing tags. */
      DELETE FROM localTags WHERE placeId IN (OLD.oldPlaceId, OLD.newPlaceId);

      /* Insert the new item, using "-1" as the placeholder parent and
         position. We'll update these later, in the "updateLocalStructure"
         trigger. */
      INSERT INTO moz_bookmarks(id, guid, parent, position, type, fk, title,
                                dateAdded, lastModified, syncStatus,
                                syncChangeCounter)
      VALUES(OLD.localId, OLD.mergedGuid, -1, -1, OLD.newType, OLD.newPlaceId,
             OLD.newTitle, OLD.newDateAddedMicroseconds,
             STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000,
             ${PlacesUtils.bookmarks.SYNC_STATUS.NORMAL}, OLD.shouldUpload)
      ON CONFLICT(id) DO UPDATE SET
        title = excluded.title,
        dateAdded = excluded.dateAdded,
        lastModified = excluded.lastModified,
        /* It's important that we update the URL *after* removing old keywords
           and *before* inserting new ones, so that the above DELETEs select
           the correct affected items. */
        fk = excluded.fk;

      /* Recalculate frecency. */
      UPDATE moz_places SET
        frecency = -frecency
      WHERE OLD.oldPlaceId <> OLD.newPlaceId AND
            id IN (OLD.oldPlaceId, OLD.newPlaceId) AND
            frecency > 0;

      /* Trigger frecency updates for all affected origins. */
      DELETE FROM moz_updateoriginsupdate_temp;

      /* Insert a new keyword for the new URL, if one is set. */
      INSERT OR IGNORE INTO moz_keywords(keyword, place_id, post_data)
      SELECT OLD.newKeyword, OLD.newPlaceId, ''
      WHERE OLD.newKeyword NOT NULL;

      /* Insert new tags for the new URL. */
      INSERT INTO localTags(tag, placeId)
      SELECT t.tag, OLD.newPlaceId FROM tags t
      WHERE t.itemId = OLD.remoteId;
    END`);

  // A view of the structure states for all items in the merged tree. The
  // mirror stores structure info in a separate table, like iOS, while Places
  // stores structure info on children. Unlike iOS, we can't simply check the
  // parent's merge state to know if its children changed. This is because our
  // merged tree might diverge from the mirror if we're missing children, or if
  // we temporarily reparented children without parents to "unfiled". In that
  // case, we want to keep syncing, but *don't* want to reupload the new local
  // structure to the server.
  await db.execute(`
    CREATE TEMP VIEW structureToMerge(localId, oldParentId, newParentId,
                                      oldPosition, newPosition, newLevel) AS
    SELECT b.id, b.parent, p.id, b.position, r.position, r.level
    FROM moz_bookmarks b
    JOIN mergeStates r ON r.mergedGuid = b.guid
    JOIN moz_bookmarks p ON p.guid = r.mergedParentGuid
    /* Don't reposition roots, since we never upload the Places root, and our
       merged tree doesn't have a tags root. */
    WHERE '${PlacesUtils.bookmarks.rootGuid}' NOT IN (r.mergedGuid,
                                                      r.mergedParentGuid)`);

  // Updates all parents and positions to reflect the merged tree.
  await db.execute(`
    CREATE TEMP TRIGGER updateLocalStructure
    INSTEAD OF DELETE ON structureToMerge
    BEGIN
      UPDATE moz_bookmarks SET
        parent = OLD.newParentId
      WHERE id = OLD.localId AND
            parent <> OLD.newParentId;

      UPDATE moz_bookmarks SET
        position = OLD.newPosition
      WHERE id = OLD.localId AND
            position <> OLD.newPosition;

      /* Record observer notifications for moved items. We ignore items that
         didn't move, and items with placeholder parents and positions of "-1",
         since they're new. */
      INSERT INTO itemsMoved(itemId, oldParentId, oldParentGuid, oldPosition,
                             level)
      SELECT OLD.localId, OLD.oldParentId, p.guid, OLD.oldPosition,
             OLD.newLevel
      FROM moz_bookmarks p
      WHERE p.id = OLD.oldParentId AND
            -1 NOT IN (OLD.oldParentId, OLD.oldPosition) AND
            (OLD.oldParentId <> OLD.newParentId OR
             OLD.oldPosition <> OLD.newPosition);
    END`);

  // A view of local bookmark tags. Tags, like keywords, are associated with
  // URLs, so two bookmarks with the same URL should have the same tags. Unlike
  // keywords, one tag may be associated with many different URLs. Tags are also
  // different because they're implemented as bookmarks under the hood. Each tag
  // is stored as a folder under the tags root, and tagged URLs are stored as
  // untitled bookmarks under these folders. This complexity can be removed once
  // bug 424160 lands.
  await db.execute(`
    CREATE TEMP VIEW localTags(tagEntryId, tagEntryGuid, tagFolderId,
                               tagFolderGuid, tagEntryPosition, tagEntryType,
                               tag, placeId) AS
    SELECT b.id, b.guid, p.id, p.guid, b.position, b.type, p.title, b.fk
    FROM moz_bookmarks b
    JOIN moz_bookmarks p ON p.id = b.parent
    JOIN moz_bookmarks r ON r.id = p.parent
    WHERE b.type = ${PlacesUtils.bookmarks.TYPE_BOOKMARK} AND
          r.guid = '${PlacesUtils.bookmarks.tagsGuid}'`);

  // Untags a URL by removing its tag entry.
  await db.execute(`
    CREATE TEMP TRIGGER untagLocalPlace
    INSTEAD OF DELETE ON localTags
    BEGIN
      /* Record an item removed notification for the tag entry. */
      INSERT INTO itemsRemoved(itemId, parentId, position, type, placeId, guid,
                               parentGuid, isUntagging)
      VALUES(OLD.tagEntryId, OLD.tagFolderId, OLD.tagEntryPosition,
             OLD.tagEntryType, OLD.placeId, OLD.tagEntryGuid,
             OLD.tagFolderGuid, 1);

      DELETE FROM moz_bookmarks WHERE id = OLD.tagEntryId;

      /* Fix the positions of the sibling tag entries. */
      UPDATE moz_bookmarks SET
        position = position - 1
      WHERE parent = OLD.tagFolderId AND
            position > OLD.tagEntryPosition;
    END`);

  // Tags a URL by creating a tag folder if it doesn't exist, then inserting a
  // tag entry for the URL into the tag folder. `NEW.placeId` can be NULL, in
  // which case we'll just create the tag folder.
  await db.execute(`
    CREATE TEMP TRIGGER tagLocalPlace
    INSTEAD OF INSERT ON localTags
    BEGIN
      /* Ensure the tag folder exists. */
      INSERT OR IGNORE INTO moz_bookmarks(guid, parent, position, type, title,
                                          dateAdded, lastModified)
      VALUES(IFNULL((SELECT b.guid FROM moz_bookmarks b
                     JOIN moz_bookmarks p ON p.id = b.parent
                     WHERE b.title = NEW.tag AND
                           p.guid = '${PlacesUtils.bookmarks.tagsGuid}'),
                    GENERATE_GUID()),
             (SELECT id FROM moz_bookmarks
              WHERE guid = '${PlacesUtils.bookmarks.tagsGuid}'),
             (SELECT COUNT(*) FROM moz_bookmarks b
              JOIN moz_bookmarks p ON p.id = b.parent
              WHERE p.guid = '${PlacesUtils.bookmarks.tagsGuid}'),
             ${PlacesUtils.bookmarks.TYPE_FOLDER}, NEW.tag,
             STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000,
             STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000);

      /* Record an item added notification if we created a tag folder.
         "CHANGES()" returns the number of rows affected by the INSERT above:
         1 if we created the folder, or 0 if the folder already existed. */
      INSERT INTO itemsAdded(guid, isTagging)
      SELECT b.guid, 1 FROM moz_bookmarks b
      JOIN moz_bookmarks p ON p.id = b.parent
      WHERE CHANGES() > 0 AND
            b.title = NEW.tag AND
            p.guid = '${PlacesUtils.bookmarks.tagsGuid}';

      /* Add a tag entry for the URL under the tag folder. Omitting the place
         ID creates a tag folder without tagging the URL. */
      INSERT OR IGNORE INTO moz_bookmarks(guid, parent, position, type, fk,
                                          dateAdded, lastModified)
      SELECT GENERATE_GUID(),
             (SELECT b.id FROM moz_bookmarks b
              JOIN moz_bookmarks p ON p.id = b.parent
              WHERE p.guid = '${PlacesUtils.bookmarks.tagsGuid}' AND
                    b.title = NEW.tag),
             (SELECT COUNT(*) FROM moz_bookmarks b
              JOIN moz_bookmarks p ON p.id = b.parent
              JOIN moz_bookmarks r ON r.id = p.parent
              WHERE p.title = NEW.tag AND
                    r.guid = '${PlacesUtils.bookmarks.tagsGuid}'),
             ${PlacesUtils.bookmarks.TYPE_BOOKMARK}, NEW.placeId,
             STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000,
             STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000
      WHERE NEW.placeId NOT NULL;

      /* Record an item added notification for the tag entry. */
      INSERT INTO itemsAdded(guid, isTagging)
      SELECT b.guid, 1 FROM moz_bookmarks b
      JOIN moz_bookmarks p ON p.id = b.parent
      JOIN moz_bookmarks r ON r.id = p.parent
      WHERE b.fk = NEW.placeId AND
            p.title = NEW.tag AND
            r.guid = '${PlacesUtils.bookmarks.tagsGuid}';
    END`);

  // Stores properties to pass to `onItem{Added, Changed, Moved, Removed}`
  // bookmark observers for new, updated, moved, and deleted items.
  await db.execute(`CREATE TEMP TABLE itemsAdded(
    guid TEXT PRIMARY KEY,
    isTagging BOOLEAN NOT NULL DEFAULT 0,
    keywordChanged BOOLEAN NOT NULL DEFAULT 0,
    level INTEGER NOT NULL DEFAULT -1
  ) WITHOUT ROWID`);

  await db.execute(`CREATE TEMP TABLE guidsChanged(
    itemId INTEGER NOT NULL,
    oldGuid TEXT NOT NULL,
    level INTEGER NOT NULL DEFAULT -1,
    PRIMARY KEY(itemId, oldGuid)
  ) WITHOUT ROWID`);

  await db.execute(`CREATE TEMP TABLE itemsChanged(
    itemId INTEGER PRIMARY KEY,
    oldTitle TEXT,
    oldPlaceId INTEGER,
    keywordChanged BOOLEAN NOT NULL DEFAULT 0,
    level INTEGER NOT NULL DEFAULT -1
  )`);

  await db.execute(`CREATE TEMP TABLE itemsMoved(
    itemId INTEGER PRIMARY KEY,
    oldParentId INTEGER NOT NULL,
    oldParentGuid TEXT NOT NULL,
    oldPosition INTEGER NOT NULL,
    level INTEGER NOT NULL DEFAULT -1
  )`);

  await db.execute(`CREATE TEMP TABLE itemsRemoved(
    guid TEXT PRIMARY KEY,
    itemId INTEGER NOT NULL,
    parentId INTEGER NOT NULL,
    position INTEGER NOT NULL,
    type INTEGER NOT NULL,
    placeId INTEGER,
    parentGuid TEXT NOT NULL,
    /* We record the original level of the removed item in the tree so that we
       can notify children before parents. */
    level INTEGER NOT NULL DEFAULT -1,
    isUntagging BOOLEAN NOT NULL DEFAULT 0
  ) WITHOUT ROWID`);

  // Stores local IDs for items to upload even if they're not flagged as changed
  // in Places. These are "weak" because we won't try to reupload the item on
  // the next sync if the upload is interrupted or fails.
  await db.execute(`CREATE TEMP TABLE idsToWeaklyUpload(
    id INTEGER PRIMARY KEY
  )`);

  // Stores local IDs for items to reupload. Removing an
  // ID from this table bumps its local change counter, so, unlike weak uploads,
  // we *will* reupload the item on the next sync if the current sync fails.
  // This is used to ensure that all bookmarks with the same URL have the same keyword (bug 1328737).
  await db.execute(`CREATE TEMP TABLE relatedIdsToReupload(
    id INTEGER PRIMARY KEY
  )`);

  await db.execute(`
    CREATE TEMP TRIGGER reuploadIds
    AFTER DELETE ON relatedIdsToReupload
    BEGIN
      UPDATE moz_bookmarks SET
        syncChangeCounter = syncChangeCounter + 1
      WHERE id = OLD.id;
    END`);

  // Stores locally changed items staged for upload.
  await db.execute(`CREATE TEMP TABLE itemsToUpload(
    id INTEGER PRIMARY KEY,
    guid TEXT UNIQUE NOT NULL,
    syncChangeCounter INTEGER NOT NULL,
    isDeleted BOOLEAN NOT NULL DEFAULT 0,
    parentGuid TEXT,
    parentTitle TEXT,
    dateAdded INTEGER, /* In milliseconds. */
    type INTEGER,
    title TEXT,
    placeId INTEGER,
    isQuery BOOLEAN NOT NULL DEFAULT 0,
    url TEXT,
    tagFolderName TEXT,
    keyword TEXT,
    position INTEGER
  )`);

  await db.execute(`CREATE TEMP TABLE structureToUpload(
    guid TEXT PRIMARY KEY,
    parentId INTEGER NOT NULL REFERENCES itemsToUpload(id)
                              ON DELETE CASCADE,
    position INTEGER NOT NULL
  ) WITHOUT ROWID`);

  await db.execute(`CREATE TEMP TABLE tagsToUpload(
    id INTEGER REFERENCES itemsToUpload(id)
               ON DELETE CASCADE,
    tag TEXT,
    PRIMARY KEY(id, tag)
  ) WITHOUT ROWID`);
}

async function resetMirror(db) {
  await db.execute(`DELETE FROM meta`);
  await db.execute(`DELETE FROM structure`);
  await db.execute(`DELETE FROM items`);
  await db.execute(`DELETE FROM urls`);

  // Since we need to reset the modified times and merge flags for the syncable
  // roots, we simply delete and recreate them.
  await createMirrorRoots(db);
}

// Converts a Sync record's last modified time to milliseconds.
function determineServerModified(record) {
  return Math.max(record.modified * 1000, 0) || 0;
}

// Determines a Sync record's creation date.
function determineDateAdded(record) {
  let serverModified = determineServerModified(record);
  return PlacesSyncUtils.bookmarks.ratchetTimestampBackwards(
    record.dateAdded, serverModified);
}

function validateTitle(rawTitle) {
  if (typeof rawTitle != "string" || !rawTitle) {
    return null;
  }
  return rawTitle.slice(0, DB_TITLE_LENGTH_MAX);
}

function validateURL(rawURL) {
  if (typeof rawURL != "string" || rawURL.length > DB_URL_LENGTH_MAX) {
    return null;
  }
  let url = null;
  try {
    url = new URL(rawURL);
  } catch (ex) {}
  return url;
}

function validateKeyword(rawKeyword) {
  if (typeof rawKeyword != "string") {
    return null;
  }
  let keyword = rawKeyword.trim();
  // Drop empty keywords.
  return keyword ? keyword.toLowerCase() : null;
}

function validateTag(rawTag) {
  if (typeof rawTag != "string") {
    return null;
  }
  let tag = rawTag.trim();
  if (!tag || tag.length > PlacesUtils.bookmarks.MAX_TAG_LENGTH) {
    // Drop empty and oversized tags.
    return null;
  }
  return tag;
}

/**
 * Measures and logs the time taken to execute a function, using a monotonic
 * clock.
 *
 * @param  {String} name
 *         The name of the operation, used for logging.
 * @param  {Function} func
 *         The function to time.
 * @param  {Function} recordTiming
 *         A function with the signature `(time: Number, result: Object?)`,
 *         where `time` is the measured time, and `result` is the return
 *         value of the timed function.
 * @return The return value of the timed function.
 */
async function withTiming(name, func, recordTiming) {
  MirrorLog.debug(name);

  let startTime = Cu.now();
  let result = await func();
  let elapsedTime = Cu.now() - startTime;

  MirrorLog.trace(`${name} took ${elapsedTime.toFixed(3)}ms`);
  recordTiming(elapsedTime, result);

  return result;
}

/**
 * Content info for an item in the local or remote tree. This is used to dedupe
 * NEW local items to remote items that don't exist locally. See `makeDupeKey`
 * for how we determine if two items are dupes.
 */
class BookmarkContent {
  constructor(title, urlHref, position) {
    this.title = title;
    this.urlHref = urlHref;
    this.position = position;
  }

  static fromRow(row) {
    let title = row.getResultByName("title");
    let urlHref = row.getResultByName("url");
    let position = row.getResultByName("position");
    return new BookmarkContent(title, urlHref, position);
  }
}

/**
 * Builds a lookup key for a node and its content. This is used to match nodes
 * with different GUIDs and similar content.
 *
 * - Bookmarks must have the same title and URL.
 * - Queries must have the same title and query URL.
 * - Folders and livemarks must have the same title.
 * - Separators must have the same position within their parents.
 *
 * @param  {BookmarkNode} node
 *         A local or remote node.
 * @param  {BookmarkContent} content
 *         Content info for the node.
 * @return {String}
 *         A map key that represents the node and its content.
 */
function makeDupeKey(node, content) {
  switch (node.kind) {
    case SyncedBookmarksMirror.KIND.QUERY:
      // Fallthrough, treat the same as a bookmark.
    case SyncedBookmarksMirror.KIND.BOOKMARK:
      // We use `JSON.stringify([...])` instead of `[...].join(",")` to avoid
      // escaping the `,` in titles and URLs.
      return JSON.stringify([node.kind, content.title, content.urlHref]);

    case SyncedBookmarksMirror.KIND.FOLDER:
    case SyncedBookmarksMirror.KIND.LIVEMARK:
      return JSON.stringify([node.kind, content.title]);

    case SyncedBookmarksMirror.KIND.SEPARATOR:
      return JSON.stringify([node.kind, content.position]);
  }
  return JSON.stringify([node.guid]);
}

/**
 * The merge state indicates which node we should prefer when reconciling
 * with Places. Recall that a merged node may point to a local node, remote
 * node, or both.
 */
class BookmarkMergeState {
  constructor(value, structure = value) {
    this.value = value;
    this.structure = structure;
  }

  /**
   * Takes an existing value state, and a new structure state. We use the new
   * merge state to resolve conflicts caused by moving local items out of a
   * remotely deleted folder, or remote items out of a locally deleted folder.
   *
   * Applying a new merged node bumps its local change counter, so that the
   * merged structure is reuploaded to the server.
   *
   * @param  {BookmarkMergeState} oldState
   *         The existing merge state.
   * @return {BookmarkMergeState}
   *         The new merge state.
   */
  static new(oldState) {
    if (oldState.structure == BookmarkMergeState.TYPE.NEW) {
      return oldState;
    }
    return new BookmarkMergeState(oldState.value, BookmarkMergeState.TYPE.NEW);
  }

  /**
   * Returns a representation of the value ("V") and structure ("S") state
   * for logging. "L" is "local", "R" is "remote", and "+" is "new". We use
   * compact notation here to reduce noise in trace logs, which log the
   * merge state of every node in the tree.
   *
   * @return {String}
   */
  toString() {
    return `(${this.valueToString()}; ${this.structureToString()})`;
  }

  valueToString() {
    switch (this.value) {
      case BookmarkMergeState.TYPE.LOCAL:
        return "Value: Local";
      case BookmarkMergeState.TYPE.REMOTE:
        return "Value: Remote";
    }
    return "Value: ?";
  }

  structureToString() {
    switch (this.structure) {
      case BookmarkMergeState.TYPE.LOCAL:
        return "Structure: Local";
      case BookmarkMergeState.TYPE.REMOTE:
        return "Structure: Remote";
      case BookmarkMergeState.TYPE.NEW:
        return "Structure: New";
    }
    return "Structure: ?";
  }

  toJSON() {
    return this.toString();
  }
}

BookmarkMergeState.TYPE = {
  LOCAL: 1,
  REMOTE: 2,
  NEW: 3,
};

/**
 * A local merge state means no changes: we keep the local value and structure
 * state. This could mean that the item doesn't exist on the server yet, or that
 * it has newer local changes that we should upload.
 *
 * It's an error for a merged node to have a local merge state without a local
 * node. Deciding the value state for the merged node asserts this.
 */
BookmarkMergeState.local = new BookmarkMergeState(
  BookmarkMergeState.TYPE.LOCAL);

/**
 * A remote merge state means we should update Places with new value and
 * structure state from the mirror. The item might not exist locally yet, or
 * might have newer remote changes that we should apply.
 *
 * As with local, a merged node can't have a remote merge state without a
 * remote node.
 */
BookmarkMergeState.remote = new BookmarkMergeState(
  BookmarkMergeState.TYPE.REMOTE);

/**
 * A node in a local or remote bookmark tree. Nodes are lightweight: they carry
 * enough information for the merger to resolve trivial conflicts without
 * querying the mirror or Places for the complete value state.
 */
class BookmarkNode {
  constructor(guid, kind, { age = 0, needsMerge = false, level = 0,
                            isSyncable = true } = {}) {
    this.guid = guid;
    this.kind = kind;
    this.age = age;
    this.needsMerge = needsMerge;
    this.level = level;
    this.isSyncable = isSyncable;
    this.children = [];
  }

  // Creates a virtual folder node for the Places root.
  static root() {
    let guid = PlacesUtils.bookmarks.rootGuid;
    return new BookmarkNode(guid, SyncedBookmarksMirror.KIND.FOLDER);
  }

  /**
   * Creates a bookmark node from a Places row.
   *
   * @param  {mozIStorageRow} row
   *         The Places row containing the node info.
   * @param  {Number} localTimeSeconds
   *         The current local time, in seconds, used to calculate the
   *         item's age.
   * @return {BookmarkNode}
   *         A bookmark node for the local item.
   */
  static fromLocalRow(row, localTimeSeconds) {
    let guid = row.getResultByName("guid");

    // Note that this doesn't account for local clock skew. `localModified`
    // is in milliseconds.
    let localModified = row.getResultByName("localModified");
    let age = Math.max(localTimeSeconds - localModified / 1000, 0) || 0;

    let kind = row.getResultByName("kind");
    let level = row.getResultByName("level");
    let isSyncable = !!row.getResultByName("isSyncable");

    let syncChangeCounter = row.getResultByName("syncChangeCounter");
    let needsMerge = syncChangeCounter > 0;

    return new BookmarkNode(guid, kind, { age, needsMerge, level, isSyncable });
  }

  /**
   * Creates a bookmark node from a mirror row.
   *
   * @param  {mozIStorageRow} row
   *         The mirror row containing the node info.
   * @param  {Number} remoteTimeSeconds
   *         The current server time, in seconds, used to calculate the
   *         item's age.
   * @return {BookmarkNode}
   *         A bookmark node for the remote item.
   */
  static fromRemoteRow(row, remoteTimeSeconds) {
    let guid = row.getResultByName("guid");

    // `serverModified` is in milliseconds.
    let serverModified = row.getResultByName("serverModified");
    let age = Math.max(remoteTimeSeconds - serverModified / 1000, 0) || 0;

    let kind = row.getResultByName("kind");
    let needsMerge = !!row.getResultByName("needsMerge");

    return new BookmarkNode(guid, kind, { age, needsMerge });
  }

  isFolder() {
    return this.kind == SyncedBookmarksMirror.KIND.FOLDER;
  }

  newerThan(otherNode) {
    return this.age < otherNode.age;
  }

  /**
   * Checks if remoteNode has a kind that's compatible with this *local* node.
   * - Nodes with the same kind are always compatible.
   * - Local folders are compatible with remote livemarks, but not vice-versa
   *   (ie, remote folders are *not* compatible with local livemarks)
   * - Bookmarks and queries are always compatible.
   *
   * @return {Boolean}
   */
  hasCompatibleKind(remoteNode) {
    if (this.kind == remoteNode.kind) {
      return true;
    }
    // bookmarks and queries are interchangable as simply changing the URL
    // can cause it to flip kinds - and webextensions are able to change the
    // URL of any bookmark.
    if ((this.kind == SyncedBookmarksMirror.KIND.BOOKMARK &&
         remoteNode.kind == SyncedBookmarksMirror.KIND.QUERY) ||
        (this.kind == SyncedBookmarksMirror.KIND.QUERY &&
         remoteNode.kind == SyncedBookmarksMirror.KIND.BOOKMARK)) {
      return true;
    }
    return false;
  }

  /**
   * Generates a human-readable, ASCII art representation of the node and its
   * descendants. This is useful for visualizing the tree structure in trace
   * logs.
   *
   * @return {String}
   */
  toASCIITreeString(prefix = "") {
    if (!this.isFolder()) {
      return prefix + "- " + this.toString();
    }
    return prefix + "+ " + this.toString() + "\n" + this.children.map(childNode =>
      childNode.toASCIITreeString(`${prefix}| `)
    ).join("\n");
  }

  /**
   * Returns a representation of the node for logging. This should be compact,
   * because the merger logs every local and remote node when trace logging is
   * enabled.
   *
   * @return {String} A string in the form of
   *         "bookmarkAAAA (Bookmark; Age = 1.234s; Unmerged)", where "Bookmark"
   *         is the kind, "Age = 1.234s" indicates the age in seconds, and
   *         "Unmerged" (which may not be present) indicates that the node
   *         needs to be merged.
   */
  toString() {
    let info = `${this.kindToString()}; Age = ${this.age.toFixed(3)}s`;
    if (this.needsMerge) {
      info += "; Unmerged";
    }
    return `${this.guid} (${info})`;
  }

  kindToString() {
    switch (this.kind) {
      case SyncedBookmarksMirror.KIND.BOOKMARK:
        return "Bookmark";
      case SyncedBookmarksMirror.KIND.QUERY:
        return "Query";
      case SyncedBookmarksMirror.KIND.FOLDER:
        return "Folder";
      case SyncedBookmarksMirror.KIND.LIVEMARK:
        return "Livemark";
      case SyncedBookmarksMirror.KIND.SEPARATOR:
        return "Separator";
    }
    return "Unknown";
  }

  // Used by `Log.jsm`.
  toJSON() {
    return this.toString();
  }
}

/**
 * A complete, rooted tree with tombstones.
 */
class BookmarkTree {
  constructor(root) {
    this.root = root;
    this.byGuid = new Map([[this.root.guid, this.root]]);
    this.parentNodeByChildNode = new Map([[this.root, null]]);
    this.deletedGuids = new Set();
  }

  isDeleted(guid) {
    return this.deletedGuids.has(guid);
  }

  nodeForGuid(guid) {
    return this.byGuid.get(guid);
  }

  parentNodeFor(childNode) {
    return this.parentNodeByChildNode.get(childNode);
  }

  /**
   * Inserts a node into the tree. The node must not already exist in the tree,
   * and the node's parent must be a folder.
   */
  insert(parentGuid, node) {
    if (this.byGuid.has(node.guid)) {
      let existingNode = this.byGuid.get(node.guid);
      MirrorLog.error("Can't replace existing node ${existingNode} with node " +
                      "${node}", { existingNode, node });
      throw new TypeError("Node already exists in tree");
    }
    let parentNode = this.byGuid.get(parentGuid);
    if (!parentNode) {
      MirrorLog.error("Missing parent ${parentGuid} for node ${node}",
                      { parentGuid, node });
      throw new TypeError("Can't insert node into nonexistent parent");
    }
    if (!parentNode.isFolder()) {
      MirrorLog.error("Non-folder parent ${parentNode} for node ${node}",
                      { parentNode, node });
      throw new TypeError("Can't insert node into non-folder");
    }

    parentNode.children.push(node);
    this.byGuid.set(node.guid, node);
    this.parentNodeByChildNode.set(node, parentNode);
  }

  noteDeleted(guid) {
    this.deletedGuids.add(guid);
  }

  * guids() {
    for (let [guid] of this.byGuid) {
      yield guid;
    }
    for (let guid of this.deletedGuids) {
      yield guid;
    }
  }

  /**
   * Generates an ASCII art representation of the complete tree.
   *
   * @return {String}
   */
  toASCIITreeString() {
    return `${this.root.toASCIITreeString()}\nDeleted: [${
            Array.from(this.deletedGuids).join(", ")}]`;
  }
}

/**
 * A node in a merged bookmark tree. Holds the local node, remote node,
 * merged children, and a merge state indicating which side to prefer.
 */
class MergedBookmarkNode {
  constructor(guid, localNode, remoteNode, mergeState) {
    this.guid = guid;
    this.localNode = localNode;
    this.remoteNode = remoteNode;
    this.mergeState = mergeState;
    this.mergedChildren = [];
  }

  /**
   * Indicates whether to prefer the remote side when applying the merged tree.
   *
   * @return {Boolean}
   */
  useRemote() {
    switch (this.mergeState.value) {
      case BookmarkMergeState.TYPE.LOCAL:
        if (!this.localNode) {
          // Should never happen. See the comment for
          // `BookmarkMergeState.local`.
          throw new TypeError(
            "Can't have local value state without local node");
        }
        return false;

      case BookmarkMergeState.TYPE.REMOTE:
        if (!this.remoteNode) {
          // Should never happen. See the comment for
          // `BookmarkMergeState.remote`.
          throw new TypeError(
            "Can't have remote value state without remote node");
        }
        if (this.localNode) {
          // If the item exists locally and remotely, check if the remote node
          // changed.
          return this.remoteNode.needsMerge;
        }
        // Otherwise, the item only exists remotely, so take the remote state
        // unconditionally.
        return true;
    }
    throw new TypeError("Unexpected value state");
  }

  /**
   * Indicates whether the merged item should be uploaded to the server.
   *
   * @return {Boolean}
   */
  shouldUpload() {
    switch (this.mergeState.structure) {
      case BookmarkMergeState.TYPE.LOCAL:
        if (!this.localNode) {
          // Should never happen. See the comment for
          // `BookmarkMergeState.local`.
          throw new TypeError(
            "Can't have local structure state without local node");
        }
        if (this.remoteNode) {
          // If the item exists locally and remotely, check if the local node
          // changed.
          return this.localNode.needsMerge;
        }
        // Otherwise, the item only exists locally, so upload the local state
        // unconditionally.
        return true;

      case BookmarkMergeState.TYPE.REMOTE:
        if (!this.remoteNode) {
          // Should never happen. See the comment for
          // `BookmarkMergeState.remote`.
          throw new TypeError(
            "Can't have remote structure state without remote node");
        }
        return false;

      case BookmarkMergeState.TYPE.NEW:
        return true;
    }
    throw new TypeError("Unexpected structure state");
  }

  /**
   * Yields the decided value and structure states of the merged node's
   * descendants. We use these as binding parameters to populate the temporary
   * `mergeStates` table when applying the merged tree to Places.
   */
  * mergeStatesParams(level = 0) {
    for (let position = 0; position < this.mergedChildren.length; ++position) {
      let mergedChild = this.mergedChildren[position];
      let mergeStateParam = {
        localGuid: mergedChild.localNode ? mergedChild.localNode.guid :
                   mergedChild.guid,
        // The merged GUID is different than the local GUID if we deduped a
        // NEW local item to a remote item.
        mergedGuid: mergedChild.guid,
        parentGuid: this.guid,
        level,
        position,
        // SQLite represents Booleans as 0/1.
        useRemote: mergedChild.useRemote() ? 1 : 0,
        shouldUpload: mergedChild.shouldUpload() ? 1 : 0,
      };
      yield mergeStateParam;
      yield* mergedChild.mergeStatesParams(level + 1);
    }
  }

  /**
   * Generates an ASCII art representation of the merged node and its
   * descendants. This is similar to the format generated by
   * `BookmarkNode#toASCIITreeString`, but logs value and structure states for
   * merged children.
   *
   * @return {String}
   */
  toASCIITreeString(prefix = "") {
    if (!this.mergedChildren.length) {
      return prefix + "- " + this.toString();
    }
    return prefix + "+ " + this.toString() + "\n" + this.mergedChildren.map(
      mergedChildNode => mergedChildNode.toASCIITreeString(`${prefix}| `)
    ).join("\n");
  }

  /**
   * Returns a representation of the merged node for logging.
   *
   * @return {String}
   *         A string in the form of "bookmarkAAAA (V: R, S: R)", where
   *         "V" is the value state and "R" is the structure state.
   */
  toString() {
    return `${this.guid} ${this.mergeState.toString()}`;
  }

  toJSON() {
    return this.toString();
  }
}

/**
 * A two-way merger that produces a complete merged tree from a complete local
 * tree and a complete remote tree with changes since the last sync.
 *
 * This is ported almost directly from iOS. On iOS, the `ThreeWayMerger` takes a
 * complete "mirror" tree with the server state after the last sync, and two
 * incomplete trees with local and remote changes to the mirror: "local" and
 * "mirror", respectively. Overlaying buffer onto mirror yields the current
 * server tree; overlaying local onto mirror yields the complete local tree.
 *
 * On Desktop, our `localTree` is the union of iOS's mirror and local, and our
 * `remoteTree` is the union of iOS's mirror and buffer. Mapping the iOS
 * concepts to Desktop:
 *
 * - "Mirror" is approximately all `moz_bookmarks` where `syncChangeCounter = 0`
 *   and `items` where `needsMerge = 0`. This is approximate because Desktop
 *   doesn't store the shared parent for changed items.
 * - "Local" is all `moz_bookmarks` where `syncChangeCounter > 0`.
 * - "Buffer" is all `items` where `needsMerge = 1`.
 *
 * Since we don't store the shared parent, we can only do two-way merges. Also,
 * our merger doesn't distinguish between structure and value changes, since we
 * don't record that state in Places. The change counter notes *that* a bookmark
 * changed, but not *how*. This means we might choose the wrong side when
 * resolving merge conflicts, while iOS will do the right thing.
 *
 * Fortunately, most of our users don't organize their bookmarks into deeply
 * nested hierarchies, or make conflicting changes on multiple devices
 * simultaneously. Changing Places to record structure and value changes would
 * require significant changes to the storage schema. A simpler two-way tree
 * merge strikes a good balance between correctness and complexity.
 */
class BookmarkMerger {
  constructor(localTree, newLocalContents, remoteTree, newRemoteContents) {
    this.localTree = localTree;
    this.newLocalContents = newLocalContents;
    this.remoteTree = remoteTree;
    this.newRemoteContents = newRemoteContents;
    this.matchingDupesByLocalParentNode = new Map();
    this.mergedGuids = new Set();
    this.deleteLocally = new Set();
    this.deleteRemotely = new Set();
    this.structureCounts = {
      remoteRevives: 0, // Remote non-folder change wins over local deletion.
      localDeletes: 0, // Local folder deletion wins over remote change.
      localRevives: 0, // Local non-folder change wins over remote deletion.
      remoteDeletes: 0, // Remote folder deletion wins over local change.
    };
    this.dupeCount = 0;
  }

  async merge() {
    let localRoot = this.localTree.nodeForGuid(PlacesUtils.bookmarks.rootGuid);
    let remoteRoot = this.remoteTree.nodeForGuid(PlacesUtils.bookmarks.rootGuid);
    let mergedRoot = await this.mergeNode(PlacesUtils.bookmarks.rootGuid, localRoot,
                                          remoteRoot);

    // Any remaining deletions on one side should be deleted on the other side.
    // This happens when the remote tree has tombstones for items that don't
    // exist in Places, or Places has tombstones for items that aren't on the
    // server.
    for await (let guid of yieldingIterator(this.localTree.deletedGuids)) {
      if (!this.mentions(guid)) {
        this.deleteRemotely.add(guid);
      }
    }
    for await (let guid of yieldingIterator(this.remoteTree.deletedGuids)) {
      if (!this.mentions(guid)) {
        this.deleteLocally.add(guid);
      }
    }
    return mergedRoot;
  }

  async subsumes(tree) {
    for await (let guid of yieldingIterator(tree.guids())) {
      if (!this.mentions(guid)) {
        return false;
      }
    }
    return true;
  }

  mentions(guid) {
    return this.mergedGuids.has(guid) || this.deleteLocally.has(guid) ||
           this.deleteRemotely.has(guid);
  }

  * deletions() {
    // Items that should be deleted locally already have tombstones on the
    // server, so we don't need to upload tombstones for these deletions.
    for (let guid of this.deleteLocally) {
      if (this.deleteRemotely.has(guid)) {
        continue;
      }
      let localNode = this.localTree.nodeForGuid(guid);
      yield { guid, localLevel: localNode ? localNode.level : -1,
              shouldUploadTombstone: false };
    }

    // Items that should be deleted remotely, or on both sides, need tombstones.
    for (let guid of this.deleteRemotely) {
      let localNode = this.localTree.nodeForGuid(guid);
      yield { guid, localLevel: localNode ? localNode.level : -1,
              shouldUploadTombstone: true };
    }
  }

  /**
   * Merges two nodes, recursively walking folders.
   *
   * @param  {String} guid
   *         The GUID to use for the merged node.
   * @param  {BookmarkNode?} localNode
   *         The local node. May be `null` if the node only exists remotely.
   * @param  {BookmarkNode?} remoteNode
   *         The remote node. May be `null` if the node only exists locally.
   * @return {MergedBookmarkNode}
   *         The merged node, with merged folder children.
   */
  async mergeNode(mergedGuid, localNode, remoteNode) {
    await maybeYield();
    this.mergedGuids.add(mergedGuid);

    if (localNode) {
      if (localNode.guid != mergedGuid) {
        // We deduped a NEW local item to a remote item.
        this.mergedGuids.add(localNode.guid);
      }

      if (remoteNode) {
        MirrorLog.trace("Item ${mergedGuid} exists locally as ${localNode} " +
                        "and remotely as ${remoteNode}; merging",
                        { mergedGuid, localNode, remoteNode });
        let mergedNode = await this.twoWayMerge(mergedGuid, localNode, remoteNode);
        return mergedNode;
      }

      MirrorLog.trace("Item ${mergedGuid} only exists locally as " +
                      "${localNode}; taking local state", { mergedGuid,
                                                            localNode });
      let mergedNode = new MergedBookmarkNode(mergedGuid, localNode, null,
                                              BookmarkMergeState.local);
      if (localNode.isFolder()) {
        // The local folder doesn't exist remotely, but its children might, so
        // we still need to recursively walk and merge them. This method will
        // change the merge state from local to new if any children were moved
        // or deleted.
        await this.mergeChildListsIntoMergedNode(mergedNode, localNode,
                                                 /* remoteNode */ null);
      }
      return mergedNode;
    }

    if (remoteNode) {
      MirrorLog.trace("Item ${mergedGuid} only exists remotely as " +
                      "${remoteNode}; taking remote state", { mergedGuid,
                                                              remoteNode });
      let mergedNode = new MergedBookmarkNode(mergedGuid, null, remoteNode,
                                              BookmarkMergeState.remote);
      if (remoteNode.isFolder()) {
        // As above, a remote folder's children might still exist locally, so we
        // need to merge them and update the merge state from remote to new if
        // any children were moved or deleted.
        await this.mergeChildListsIntoMergedNode(mergedNode, /* localNode */ null,
                                                 remoteNode);
      }
      return mergedNode;
    }

    // Should never happen. We need to have at least one node for a two-way
    // merge.
    throw new TypeError("Can't merge two nonexistent nodes");
  }

  /**
   * Merges two nodes that exist locally and remotely.
   *
   * @param  {String} mergedGuid
   *         The GUID to use for the merged node.
   * @param  {BookmarkNode} localNode
   *         The existing local node.
   * @param  {BookmarkNode} remoteNode
   *         The existing remote node.
   * @return {MergedBookmarkNode}
   *         The merged node, with merged folder children.
   */
  async twoWayMerge(mergedGuid, localNode, remoteNode) {
    let mergeState = this.resolveTwoWayValueConflict(mergedGuid, localNode,
                                                     remoteNode);
    MirrorLog.trace("Merge state for ${mergedGuid} is ${mergeState}",
                    { mergedGuid, mergeState });

    let mergedNode = new MergedBookmarkNode(mergedGuid, localNode, remoteNode,
                                            mergeState);

    if (!localNode.hasCompatibleKind(remoteNode)) {
      MirrorLog.error("Merging local ${localNode} and remote ${remoteNode} " +
                      "with different kinds", { localNode, remoteNode });
      throw new SyncedBookmarksMirror.ConsistencyError(
        "Can't merge different item kinds");
    }

    if (localNode.isFolder()) {
      if (remoteNode.isFolder()) {
        // Merging two folders, so we need to walk their children to handle
        // structure changes.
        MirrorLog.trace("Merging folders ${localNode} and ${remoteNode}",
                        { localNode, remoteNode });
        await this.mergeChildListsIntoMergedNode(mergedNode, localNode, remoteNode);
        return mergedNode;
      }
      // Otherwise it must be a livemark, so fall through.
    }
    // Otherwise are compatible kinds of non-folder, so there's no need to
    // walk children - just return the merged node.
    MirrorLog.trace("Merging non-folders ${localNode} and ${remoteNode}",
                    { localNode, remoteNode });
    return mergedNode;
  }

  /**
   * Determines the merge state for a node that exists locally and remotely.
   *
   * @param  {String} mergedGuid
   *         The GUID of the merged node. This is the same as the remote GUID,
   *         and usually the same as the local GUID. The local GUID may be
   *         different if we're deduping a local item to a remote item.
   * @param  {String} localNode
   *         The local bookmark node.
   * @param  {BookmarkNode} remoteNode
   *         The remote bookmark node.
   * @return {BookmarkMergeState}
   *         The two-way merge state.
   */
  resolveTwoWayValueConflict(mergedGuid, localNode, remoteNode) {
    if (PlacesUtils.bookmarks.userContentRoots.includes(mergedGuid)) {
      // Don't update root titles or other properties.
      return BookmarkMergeState.local;
    }
    if (localNode.needsMerge && remoteNode.needsMerge) {
      // The item changed locally and remotely. Use the timestamp to decide
      // which is newer.
      let valueState = localNode.newerThan(remoteNode) ?
                       BookmarkMergeState.local :
                       BookmarkMergeState.remote;
      return valueState;
    }
    if (remoteNode.needsMerge) {
      // The item changed remotely since the last sync, but not locally. Take
      // the remote state.
      return BookmarkMergeState.remote;
    }
    // The item changed locally, or is unchanged on both sides. Keep the local
    // state.
    return BookmarkMergeState.local;
  }

  /**
   * Merges a remote child node into a merged folder node. This handles the
   * following cases:
   *
   * - The remote child is locally deleted. We recursively move all of its
   *   descendants that don't exist locally to the merged folder.
   * - The remote child doesn't exist locally, but has a content match in the
   *   corresponding local folder. We dedupe the local child to the remote
   *   child.
   * - The remote child exists locally, but in a different folder. We compare
   *   merge flags and timestamps to decide where to keep the child.
   * - The remote child exists locally, and in the same folder. We merge the
   *   local and remote children.
   *
   * This is the inverse of `mergeLocalChildIntoMergedNode`.
   *
   * @param  {MergedBookmarkNode} mergedNode
   *         The merged folder node.
   * @param  {BookmarkNode} remoteParentNode
   *         The remote folder node.
   * @param  {BookmarkNode} remoteChildNode
   *         The remote child node.
   */
  async mergeRemoteChildIntoMergedNode(mergedNode, remoteParentNode,
                                       remoteChildNode) {
    if (this.mergedGuids.has(remoteChildNode.guid)) {
      MirrorLog.trace("Remote child ${remoteChildNode} already seen in " +
                      "another folder and merged", { remoteChildNode });
      return;
    }

    MirrorLog.trace("Merging remote child ${remoteChildNode} of " +
                    "${remoteParentNode} into ${mergedNode}",
                    { remoteChildNode, remoteParentNode, mergedNode });

    if (PlacesUtils.bookmarks.userContentRoots.includes(remoteChildNode.guid)) {
      // Remote child is a root. We always prefer local roots, since remote
      // roots might be misparented, and we checked that the local roots were
      // correct before merging. We can just bail here: if the root is parented
      // correctly, we won't reupload anything, since we never upload the Places
      // root; if not, we'll flag the wrong parent for reupload.
      MirrorLog.trace("Ignoring remote root ${remoteChildNode} in " +
                      "${remoteParentNode}", { remoteChildNode,
                                               remoteParentNode });
      mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
      return;
    }

    // Make sure the remote child isn't locally deleted.
    let structureChange = await this.checkForLocalStructureChangeOfRemoteNode(
      mergedNode, remoteParentNode, remoteChildNode);
    if (structureChange == BookmarkMerger.STRUCTURE.DELETED) {
      // If the remote child is locally deleted, we need to move all descendants
      // that aren't also remotely deleted to the merged node. This handles the
      // case where a user deletes a folder on this device, and adds a bookmark
      // to the same folder on another device. We want to keep the folder
      // deleted, but we also don't want to lose the new bookmark, so we move
      // the bookmark to the deleted folder's parent.
      mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
      return;
    }

    // The remote child isn't locally deleted. Does it exist in the local tree?
    let localChildNode = this.localTree.nodeForGuid(remoteChildNode.guid);
    if (!localChildNode) {
      // Remote child is not a root, and doesn't exist locally. Try to find a
      // content match in the containing folder, and dedupe the local item if
      // we can.
      MirrorLog.trace("Remote child ${remoteChildNode} doesn't exist " +
                      "locally; looking for local content match",
                      { remoteChildNode });

      let localChildNodeByContent = await this.findLocalNodeMatchingRemoteNode(
        mergedNode, remoteChildNode);

      let mergedChildNode = await this.mergeNode(remoteChildNode.guid,
                                                 localChildNodeByContent,
                                                 remoteChildNode);
      mergedNode.mergedChildren.push(mergedChildNode);
      return;
    }

    // Otherwise, the remote child exists in the local tree. Did it move?
    let localParentNode = this.localTree.parentNodeFor(localChildNode);
    if (!localParentNode) {
      // Should never happen. If a node in the local tree doesn't have a parent,
      // we built the tree incorrectly.
      MirrorLog.error("Remote child ${remoteChildNode} exists locally as " +
                      "${localChildNode} without local parent",
                      { remoteChildNode, localChildNode });
      throw new TypeError(
        "Can't merge existing remote child without local parent");
    }

    MirrorLog.trace("Remote child ${remoteChildNode} exists locally in " +
                    "${localParentNode} and remotely in ${remoteParentNode}",
                    { remoteChildNode, localParentNode, remoteParentNode });

    if (this.remoteTree.isDeleted(localParentNode.guid)) {
      MirrorLog.trace("Unconditionally taking remote move for " +
                      "${remoteChildNode} to ${remoteParentNode} because " +
                      "local parent ${localParentNode} is deleted remotely",
                      { remoteChildNode, remoteParentNode, localParentNode });

      let mergedChildNode = await this.mergeNode(localChildNode.guid,
                                                 localChildNode, remoteChildNode);
      mergedNode.mergedChildren.push(mergedChildNode);
      return;
    }

    if (localParentNode.needsMerge) {
      if (remoteParentNode.needsMerge) {
        MirrorLog.trace("Local ${localParentNode} and remote " +
                        "${remoteParentNode} parents changed; comparing " +
                        "modified times to decide parent for remote child " +
                        "${remoteChildNode}",
                        { localParentNode, remoteParentNode, remoteChildNode });

        let latestLocalAge = Math.min(localChildNode.age,
                                      localParentNode.age);
        let latestRemoteAge = Math.min(remoteChildNode.age,
                                       remoteParentNode.age);

        if (latestRemoteAge > latestLocalAge) {
          // Local move is younger, so we ignore the remote move. We'll
          // merge the child later, when we walk its new local parent.
          MirrorLog.trace("Ignoring older remote move for ${remoteChildNode} " +
                          "to ${remoteParentNode} at ${latestRemoteAge}; " +
                          "local move to ${localParentNode} at " +
                          "${latestLocalAge} is newer",
                          { remoteChildNode, remoteParentNode, latestRemoteAge,
                            localParentNode, latestLocalAge });

          // Flag the old parent for reupload, since we're moving the
          // remote child. Note that, since we only flag the remote parent here,
          // we don't need to handle reparenting and repositioning separately.
          mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
          return;
        }

        // Otherwise, the remote move is younger, so we ignore the local move
        // and merge the child now.
        MirrorLog.trace("Taking newer remote move for ${remoteChildNode} to " +
                        "${remoteParentNode} at ${latestRemoteAge}; local " +
                        "move to ${localParentNode} at ${latestLocalAge} is " +
                        "older", { remoteChildNode, remoteParentNode,
                                   latestRemoteAge, localParentNode,
                                   latestLocalAge });

        let mergedChildNode = await this.mergeNode(remoteChildNode.guid,
                                                   localChildNode, remoteChildNode);
        mergedNode.mergedChildren.push(mergedChildNode);
        return;
      }

      MirrorLog.trace("Remote parent unchanged; keeping remote child " +
                      "${remoteChildNode} in ${localParentNode}",
                      { remoteChildNode, localParentNode });

      // Only flag the parent of the remote child for reupload.
      mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
      return;
    }

    MirrorLog.trace("Local parent unchanged; keeping remote child " +
                    "${remoteChildNode} in ${remoteParentNode}",
                    { remoteChildNode, remoteParentNode });

    let mergedChildNode = await this.mergeNode(remoteChildNode.guid, localChildNode,
                                               remoteChildNode);
    mergedNode.mergedChildren.push(mergedChildNode);
  }

  /**
   * Merges a local child node into a merged folder node.
   *
   * This is the inverse of `mergeRemoteChildIntoMergedNode`.
   *
   * @param  {MergedBookmarkNode} mergedNode
   *         The merged folder node.
   * @param  {BookmarkNode} localParentNode
   *         The local folder node.
   * @param  {BookmarkNode} localChildNode
   *         The local child node.
   */
  async mergeLocalChildIntoMergedNode(mergedNode, localParentNode, localChildNode) {
    if (this.mergedGuids.has(localChildNode.guid)) {
      // We already merged the child when we walked another folder.
      MirrorLog.trace("Local child ${localChildNode} already seen in " +
                      "another folder and merged", { localChildNode });
      return;
    }

    MirrorLog.trace("Merging local child ${localChildNode} of " +
                    "${localParentNode} into ${mergedNode}",
                    { localChildNode, localParentNode, mergedNode });

    if (PlacesUtils.bookmarks.userContentRoots.includes(localChildNode.guid)) {
      // Local child is a root, which may or may not exist remotely. We know
      // local roots are parented correctly, so we merge them unconditionally.
      // Places maintenance also bumps the change counter when fixing incorrect
      // parents, so we'll flag the merged root node for reupload.
      let remoteChildNode = this.remoteTree.nodeForGuid(localChildNode.guid);
      if (remoteChildNode) {
        let remoteParentNode = this.remoteTree.parentNodeFor(remoteChildNode);
        if (!remoteParentNode) {
          // Should never happen. If a node in the remote tree doesn't have a
          // parent, we built the tree incorrectly.
          MirrorLog.error("Local child ${localChildNode} exists remotely as " +
                          "${remoteChildNode} without remote parent",
                          { localChildNode, remoteChildNode });
          throw new TypeError(
            "Can't merge existing local syncable root without remote Places root");
        }
        if (localParentNode.guid != remoteParentNode.guid) {
          let mergedRootNode = await this.mergeNode(localChildNode.guid,
                                                    localChildNode, remoteChildNode);
          mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
          mergedRootNode.mergeState = BookmarkMergeState.new(mergedRootNode.mergeState);
          mergedNode.mergedChildren.push(mergedRootNode);
          return;
        }
        let mergedRootNode = await this.mergeNode(localChildNode.guid,
                                                  localChildNode, remoteChildNode);
        mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
        mergedNode.mergedChildren.push(mergedRootNode);
        return;
      }

      let mergedRootNode = await this.mergeNode(localChildNode.guid,
                                                localChildNode, /* remoteNode */ null);
      mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
      mergedRootNode.mergeState = BookmarkMergeState.new(mergedRootNode.mergeState);
      mergedNode.mergedChildren.push(mergedRootNode);
      return;
    }

    // Now, we know we haven't seen the local child before, and it's not in
    // this folder on the server. Check if the child is remotely deleted.
    let structureChange = await this.checkForRemoteStructureChangeOfLocalNode(
      mergedNode, localParentNode, localChildNode);
    if (structureChange == BookmarkMerger.STRUCTURE.DELETED) {
      // If the child is remotely deleted, we need to move any new local
      // descendants to the merged node, just as we did for new remote
      // descendants of locally deleted children.
      mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
      return;
    }

    // At this point, we know the local child isn't deleted. See if it
    // exists in the remote tree.
    let remoteChildNode = this.remoteTree.nodeForGuid(localChildNode.guid);
    if (!remoteChildNode) {
      // Local child is not a root, and doesn't exist remotely. Try to find a
      // content match in the containing folder, and dedupe the local item if
      // we can.
      MirrorLog.trace("Local child ${localChildNode} doesn't exist " +
                      "remotely; looking for remote content match",
                      { localChildNode });

      let remoteChildNodeByContent = await this.findRemoteNodeMatchingLocalNode(
        mergedNode, localChildNode);

      if (remoteChildNodeByContent) {
        // The local child has a remote content match, so take the remote GUID
        // and merge.
        let mergedChildNode = await this.mergeNode(
          remoteChildNodeByContent.guid, localChildNode,
          remoteChildNodeByContent);
        mergedNode.mergedChildren.push(mergedChildNode);
        return;
      }

      // The local child doesn't exist remotely, so flag the merged parent and
      // new child for upload, and walk its descendants.
      let mergedChildNode = await this.mergeNode(localChildNode.guid, localChildNode,
                                                 /* remoteChildNode */ null);
      mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
      mergedChildNode.mergeState = BookmarkMergeState.new(mergedChildNode.mergeState);
      mergedNode.mergedChildren.push(mergedChildNode);
      return;
    }

    // The local child exists remotely. It must have moved; otherwise, we
    // would have seen it when we walked the remote children.
    let remoteParentNode = this.remoteTree.parentNodeFor(remoteChildNode);
    if (!remoteParentNode) {
      // Should never happen. If a node in the remote tree doesn't have a
      // parent, we built the tree incorrectly.
      MirrorLog.error("Local child ${localChildNode} exists remotely as " +
                      "${remoteChildNode} without remote parent",
                      { localChildNode, remoteChildNode });
      throw new TypeError(
        "Can't merge existing local child without remote parent");
    }

    MirrorLog.trace("Local child ${localChildNode} exists locally in " +
                    "${localParentNode} and remotely in ${remoteParentNode}",
                    { localChildNode, localParentNode, remoteParentNode });

    if (this.localTree.isDeleted(remoteParentNode.guid)) {
      MirrorLog.trace("Unconditionally taking local move for " +
                      "${localChildNode} to ${localParentNode} because " +
                      "remote parent ${remoteParentNode} is deleted locally",
                      { localChildNode, localParentNode, remoteParentNode });

      let mergedChildNode = await this.mergeNode(localChildNode.guid,
                                                 localChildNode, remoteChildNode);
      mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
      mergedChildNode.mergeState = BookmarkMergeState.new(mergedChildNode.mergeState);
      mergedNode.mergedChildren.push(mergedChildNode);
      return;
    }

    if (localParentNode.needsMerge) {
      if (remoteParentNode.needsMerge) {
        // If both parents changed, compare timestamps to decide where
        // to keep the local child.
        let latestLocalAge = Math.min(localChildNode.age,
                                      localParentNode.age);
        let latestRemoteAge = Math.min(remoteChildNode.age,
                                       remoteParentNode.age);

        // Did the child move to a different folder?
        if (localParentNode.guid != remoteParentNode.guid) {
          if (latestRemoteAge <= latestLocalAge) {
            MirrorLog.trace("Local child ${localChildNode} reparented " +
                            "locally to ${localParentNode} at " +
                            "${latestLocalAge} and remotely to " +
                            "${remoteParentNode} at ${latestRemoteAge}; " +
                            "keeping child in newer remote parent",
                            { localChildNode, localParentNode, latestLocalAge,
                              remoteParentNode, latestRemoteAge });
            return;
          }

          MirrorLog.trace("Local child ${localChildNode} reparented " +
                          "locally to ${localParentNode} at " +
                          "${latestLocalAge} and remotely to " +
                          "${remoteParentNode} at ${latestRemoteAge}; " +
                          "keeping child in newer local parent",
                          { localChildNode, localParentNode, latestLocalAge,
                            remoteParentNode, latestRemoteAge });

          let mergedChildNode = await this.mergeNode(localChildNode.guid,
                                                     localChildNode, remoteChildNode);
          mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
          mergedChildNode.mergeState = BookmarkMergeState.new(mergedChildNode.mergeState);
          mergedNode.mergedChildren.push(mergedChildNode);
          return;
        }

        // Otherwise, the child was repositioned in the same folder.
        // Compare timestamps to decide the child's new position.
        if (latestRemoteAge <= latestLocalAge) {
          MirrorLog.trace("Local child ${localChildNode} repositioned " +
                          "locally in ${localParentNode} at " +
                          "${latestLocalAge} and remotely in " +
                          "${remoteParentNode} at ${latestRemoteAge}; " +
                          "keeping child in newer remote position",
                          { localChildNode, localParentNode, latestLocalAge,
                            remoteParentNode, latestRemoteAge });
          return;
        }

        MirrorLog.trace("Local child ${localChildNode} repositioned " +
                        "locally in ${localParentNode} at " +
                        "${latestLocalAge} and remotely in " +
                        "${remoteParentNode} at ${latestRemoteAge}; " +
                        "keeping child in newer local position",
                        { localChildNode, localParentNode, latestLocalAge,
                          remoteParentNode, latestRemoteAge });

        let mergedChildNode = await this.mergeNode(localChildNode.guid,
                                                   localChildNode, remoteChildNode);
        mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
        mergedNode.mergedChildren.push(mergedChildNode);
        return;
      }

      // If only the local parent changed, keep the local child in its
      // new parent.
      if (localParentNode.guid != remoteParentNode.guid) {
        MirrorLog.trace("Local child ${localChildNode} reparented locally to " +
                        "${localParentNode}", { localChildNode,
                                                localParentNode });

        // Merge and flag both the new parent and child for
        // reupload.
        let mergedChildNode = await this.mergeNode(localChildNode.guid, localChildNode,
                                                   remoteChildNode);
        mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
        mergedChildNode.mergeState = BookmarkMergeState.new(mergedChildNode.mergeState);
        mergedNode.mergedChildren.push(mergedChildNode);
        return;
      }

      // Otherwise, the child was repositioned locally.
      MirrorLog.trace("Local child ${localChildNode} repositioned locally in " +
                      "${localParentNode}", { localChildNode,
                                              localParentNode });

      // Only flag the parent of the repositioned local child for
      // reupload.
      let mergedChildNode = await this.mergeNode(localChildNode.guid,
                                                 localChildNode, remoteChildNode);
      mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
      mergedNode.mergedChildren.push(mergedChildNode);
      return;
    }

    MirrorLog.trace("Local child ${localChildNode} unchanged locally; " +
                    "keeping child in remote parent ${remoteParentNode}",
                    { localChildNode, remoteParentNode });
  }

  /**
   * Recursively merges the children of a local folder node and a matching
   * remote folder node.
   *
   * @param {MergedBookmarkNode} mergedNode
   *        The merged folder node. This method mutates the merged node to
   *        append local and remote children, and sets a new merge state
   *        state if needed.
   * @param {BookmarkNode?} localNode
   *        The local folder node. May be `null` if the folder only exists
   *        remotely.
   * @param {BookmarkNode?} remoteNode
   *        The remote folder node. May be `null` if the folder only exists
   *        locally.
   */
  async mergeChildListsIntoMergedNode(mergedNode, localNode, remoteNode) {
    if (localNode && remoteNode) {
      if (localNode.needsMerge && remoteNode.needsMerge) {
        // The folder exists locally and remotely, and changed on both sides.
        // Compare timestamps to determine which children to merge first,
        // followed by remaining unmerged children on the other side.
        if (localNode.newerThan(remoteNode)) {
          await this.mergeLocalChildrenIntoMergedNode(mergedNode, localNode);
          await this.mergeRemoteChildrenIntoMergedNode(mergedNode, remoteNode);
        } else {
          await this.mergeRemoteChildrenIntoMergedNode(mergedNode, remoteNode);
          await this.mergeLocalChildrenIntoMergedNode(mergedNode, localNode);
        }
        return;
      }

      if (remoteNode.needsMerge) {
        // The folder exists locally and remotely, and only changed remotely.
        // Merge remote children first, followed by remaining local children.
        await this.mergeRemoteChildrenIntoMergedNode(mergedNode, remoteNode);
        await this.mergeLocalChildrenIntoMergedNode(mergedNode, localNode);
        return;
      }

      // The folder exists locally and remotely, and only changed locally, or
      // is unchanged on both sides. Merge local first, then remote.
      await this.mergeLocalChildrenIntoMergedNode(mergedNode, localNode);
      await this.mergeRemoteChildrenIntoMergedNode(mergedNode, remoteNode);
      return;
    }

    if (localNode) {
      // The folder only exists locally, so no remote children to merge.
      await this.mergeLocalChildrenIntoMergedNode(mergedNode, localNode);
      return;
    }

    if (remoteNode) {
      // The folder only exists remotely, so no local children to merge.
      await this.mergeRemoteChildrenIntoMergedNode(mergedNode, remoteNode);
      return;
    }

    // Should never happen.
    throw new TypeError("Can't merge children for two nonexistent nodes");
  }

  /**
   * Recursively merges the children of a remote folder node.
   *
   * @param  {MergedBookmarkNode} mergedNode
   *         The merged folder node. This method mutates the merged node to
   *         append remote children.
   * @param  {BookmarkNode} remoteNode
   *         The remote folder node.
   */
  async mergeRemoteChildrenIntoMergedNode(mergedNode, remoteNode) {
    MirrorLog.trace("Merging remote children of ${remoteNode} into " +
                    "${mergedNode}", { remoteNode, mergedNode });

    for await (let remoteChildNode of yieldingIterator(remoteNode.children)) {
      await this.mergeRemoteChildIntoMergedNode(
        mergedNode, remoteNode, remoteChildNode);
    }
  }

  /**
   * Recursively merges the children of a local folder node.
   *
   * @param  {MergedBookmarkNode} mergedNode
   *         The merged folder node. This method mutates the merged node to
   *         append local children.
   * @param  {BookmarkNode} localNode
   *         The local folder node.
   */
  async mergeLocalChildrenIntoMergedNode(mergedNode, localNode) {
    MirrorLog.trace("Merging local children of ${localNode} into " +
                    "${mergedNode}", { localNode, mergedNode });

    for await (let localChildNode of yieldingIterator(localNode.children)) {
      await this.mergeLocalChildIntoMergedNode(
        mergedNode, localNode, localChildNode);
    }
  }

  /**
   * Checks if a remote node is locally moved or deleted, and reparents any
   * descendants that aren't also remotely deleted to the merged node.
   *
   * This is the inverse of `checkForRemoteStructureChangeOfLocalNode`.
   *
   * @param  {MergedBookmarkNode} mergedNode
   *         The merged folder node to hold relocated remote orphans.
   * @param  {BookmarkNode} remoteParentNode
   *         The remote parent of the potentially deleted child node.
   * @param  {BookmarkNode} remoteNode
   *         The remote potentially deleted child node.
   * @return {BookmarkMerger.STRUCTURE}
   *         A structure change type: `UNCHANGED` if the remote node is not
   *         deleted or doesn't exist locally, `MOVED` if the node is moved
   *         locally, or `DELETED` if the node is deleted locally.
   */
  async checkForLocalStructureChangeOfRemoteNode(mergedNode, remoteParentNode,
                                                 remoteNode) {
    if (PlacesUtils.bookmarks.userContentRoots.includes(remoteNode.guid)) {
      // Should never happen. We should have seen and ignored remote roots.
      throw new TypeError(
        "Shouldn't check remote syncable root for structure changes");
    }

    if (!remoteNode.isSyncable) {
      // If the remote node is known to be non-syncable, we unconditionally
      // delete it from the server, even if it's syncable locally.
      this.deleteRemotely.add(remoteNode.guid);
      if (remoteNode.isFolder()) {
        // If the remote node is a folder, we also need to walk its descendants
        // and reparent any syncable descendants, and descendants that only
        // exist remotely, to the merged node.
        await this.relocateRemoteOrphansToMergedNode(mergedNode, remoteNode);
      }
      return BookmarkMerger.STRUCTURE.DELETED;
    }

    if (!this.localTree.isDeleted(remoteNode.guid)) {
      let localNode = this.localTree.nodeForGuid(remoteNode.guid);
      if (!localNode) {
        return BookmarkMerger.STRUCTURE.UNCHANGED;
      }
      if (!localNode.isSyncable) {
        // The remote node is syncable, but the local node is non-syncable.
        // This is unlikely now that Places no longer supports custom roots,
        // but, for consistency, we unconditionally delete the node from the
        // server.
        this.deleteRemotely.add(remoteNode.guid);
        if (remoteNode.isFolder()) {
          await this.relocateRemoteOrphansToMergedNode(mergedNode, remoteNode);
        }
        return BookmarkMerger.STRUCTURE.DELETED;
      }
      let localParentNode = this.localTree.parentNodeFor(localNode);
      if (!localParentNode) {
        // Should never happen. If a node in the local tree doesn't have a
        // parent, we built the tree incorrectly.
        throw new TypeError(
          "Can't check for structure changes without local parent");
      }
      if (localParentNode.guid != remoteParentNode.guid) {
        return BookmarkMerger.STRUCTURE.MOVED;
      }
      return BookmarkMerger.STRUCTURE.UNCHANGED;
    }

    if (remoteNode.needsMerge) {
      if (!remoteNode.isFolder()) {
        // If a non-folder child is deleted locally and changed remotely, we
        // ignore the local deletion and take the remote child.
        MirrorLog.trace("Remote non-folder ${remoteNode} deleted locally " +
                        "and changed remotely; taking remote change",
                        { remoteNode });
        this.structureCounts.remoteRevives++;
        return BookmarkMerger.STRUCTURE.UNCHANGED;
      }
      // For folders, we always take the local deletion and relocate remotely
      // changed grandchildren to the merged node. We could use the mirror to
      // revive the child folder, but it's easier to relocate orphaned
      // grandchildren than to partially revive the child folder.
      MirrorLog.trace("Remote folder ${remoteNode} deleted locally " +
                      "and changed remotely; taking local deletion",
                      { remoteNode });
      this.structureCounts.localDeletes++;
    } else {
      MirrorLog.trace("Remote node ${remoteNode} deleted locally and not " +
                      "changed remotely; taking local deletion",
                      { remoteNode });
    }

    // Take the local deletion and relocate any new remote descendants to the
    // merged node.
    this.deleteRemotely.add(remoteNode.guid);
    if (remoteNode.isFolder()) {
      await this.relocateRemoteOrphansToMergedNode(mergedNode, remoteNode);
    }
    return BookmarkMerger.STRUCTURE.DELETED;
  }

  /**
   * Checks if a local node is remotely moved or deleted, and reparents any
   * descendants that aren't also locally deleted to the merged node.
   *
   * This is the inverse of `checkForLocalStructureChangeOfRemoteNode`.
   *
   * @param  {MergedBookmarkNode} mergedNode
   *         The merged folder node to hold relocated local orphans.
   * @param  {BookmarkNode} localParentNode
   *         The local parent of the potentially deleted child node.
   * @param  {BookmarkNode} localNode
   *         The local potentially deleted child node.
   * @return {BookmarkMerger.STRUCTURE}
   *         A structure change type: `UNCHANGED` if the local node is not
   *         deleted or doesn't exist remotely, `MOVED` if the node is moved
   *         remotely, or `DELETED` if the node is deleted remotely.
   */
  async checkForRemoteStructureChangeOfLocalNode(mergedNode, localParentNode,
                                                 localNode) {
    if (PlacesUtils.bookmarks.userContentRoots.includes(localNode.guid)) {
      // Should never happen. We should have merged local roots unconditionally.
      throw new TypeError(
        "Shouldn't check local syncable root for structure changes");
    }

    if (!localNode.isSyncable) {
      // If the local node is known to be non-syncable, we unconditionally
      // delete it from Places, even if it's syncable remotely. This is
      // unlikely now that Places no longer supports custom roots.
      this.deleteLocally.add(localNode.guid);
      if (localNode.isFolder()) {
        await this.relocateLocalOrphansToMergedNode(mergedNode, localNode);
      }
      return BookmarkMerger.STRUCTURE.DELETED;
    }

    if (!this.remoteTree.isDeleted(localNode.guid)) {
      let remoteNode = this.remoteTree.nodeForGuid(localNode.guid);
      if (!remoteNode) {
        return BookmarkMerger.STRUCTURE.UNCHANGED;
      }
      if (!remoteNode.isSyncable) {
        // The local node is syncable, but the remote node is non-syncable.
        // This can happen if we applied an orphaned left pane query in a
        // previous sync, and later saw the left pane root on the server.
        // Since we now have the complete subtree, we can remove the item from
        // Places.
        this.deleteLocally.add(localNode.guid);
        if (remoteNode.isFolder()) {
          await this.relocateLocalOrphansToMergedNode(mergedNode, localNode);
        }
        return BookmarkMerger.STRUCTURE.DELETED;
      }
      let remoteParentNode = this.remoteTree.parentNodeFor(remoteNode);
      if (!remoteParentNode) {
        // Should never happen. If a node in the remote tree doesn't have a
        // parent, we built the tree incorrectly.
        throw new TypeError(
          "Can't check for structure changes without remote parent");
      }
      if (remoteParentNode.guid != localParentNode.guid) {
        return BookmarkMerger.STRUCTURE.MOVED;
      }
      return BookmarkMerger.STRUCTURE.UNCHANGED;
    }

    if (localNode.needsMerge) {
      if (!localNode.isFolder()) {
        MirrorLog.trace("Local non-folder ${localNode} deleted remotely and " +
                        "changed locally; taking local change", { localNode });
        this.structureCounts.localRevives++;
        return BookmarkMerger.STRUCTURE.UNCHANGED;
      }
      MirrorLog.trace("Local folder ${localNode} deleted remotely and " +
                      "changed locally; taking remote deletion", { localNode });
      this.structureCounts.remoteDeletes++;
    } else {
      MirrorLog.trace("Local node ${localNode} deleted remotely and not " +
                      "changed locally; taking remote deletion", { localNode });
    }

    // Take the remote deletion and relocate any new local descendants to the
    // merged node.
    this.deleteLocally.add(localNode.guid);
    if (localNode.isFolder()) {
      await this.relocateLocalOrphansToMergedNode(mergedNode, localNode);
    }
    return BookmarkMerger.STRUCTURE.DELETED;
  }

  /**
   * Takes a local deletion for a remote node by marking the node as deleted,
   * and relocating all remote descendants that aren't also locally deleted to
   * the closest surviving ancestor. We do this to avoid data loss if the
   * user adds a bookmark to a folder on another device, and deletes that
   * folder locally.
   *
   * This is the inverse of `relocateLocalOrphansToMergedNode`.
   *
   * @param {MergedBookmarkNode} mergedNode
   *        The closest surviving ancestor, to hold relocated remote orphans.
   * @param {BookmarkNode} remoteNode
   *        The locally deleted remote node.
   */
  async relocateRemoteOrphansToMergedNode(mergedNode, remoteNode) {
    let remoteOrphanNodes = [];
    for await (let remoteChildNode of yieldingIterator(remoteNode.children)) {
      if (this.mergedGuids.has(remoteChildNode.guid)) {
        MirrorLog.trace("Remote child ${remoteChildNode} can't be an orphan; " +
                        "already merged", { remoteChildNode });
        continue;
      }
      let structureChange = await this.checkForLocalStructureChangeOfRemoteNode(
        mergedNode, remoteNode, remoteChildNode);
      if (structureChange == BookmarkMerger.STRUCTURE.MOVED ||
          structureChange == BookmarkMerger.STRUCTURE.DELETED) {
        // The remote child is already moved or deleted locally, so we should
        // ignore it instead of treating it as a remote orphan.
        continue;
      }
      remoteOrphanNodes.push(remoteChildNode);
    }

    let mergedOrphanNodes = [];
    for await (let remoteOrphanNode of yieldingIterator(remoteOrphanNodes)) {
      let localOrphanNode = this.localTree.nodeForGuid(remoteOrphanNode.guid);
      let mergedOrphanNode = await this.mergeNode(remoteOrphanNode.guid,
                                                  localOrphanNode, remoteOrphanNode);
      mergedOrphanNodes.push(mergedOrphanNode);
    }

    MirrorLog.trace("Relocating remote orphans ${mergedOrphanNodes} to " +
                    "${mergedNode}", { mergedOrphanNodes, mergedNode });
    for await (let mergedOrphanNode of yieldingIterator(mergedOrphanNodes)) {
      // Flag the new parent and moved orphans for reupload.
      mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
      mergedOrphanNode.mergeState = BookmarkMergeState.new(mergedOrphanNode.mergeState);
      mergedNode.mergedChildren.push(mergedOrphanNode);
    }
  }

  /**
   * Takes a remote deletion for a local node by marking the node as deleted,
   * and relocating all local descendants that aren't also remotely deleted to
   * the closest surviving ancestor.
   *
   * This is the inverse of `relocateRemoteOrphansToMergedNode`.
   *
   * @param {MergedBookmarkNode} mergedNode
   *        The closest surviving ancestor, to hold relocated local orphans.
   * @param {BookmarkNode} localNode
   *        The remotely deleted local node.
   */
  async relocateLocalOrphansToMergedNode(mergedNode, localNode) {
    let localOrphanNodes = [];
    for await (let localChildNode of yieldingIterator(localNode.children)) {
      if (this.mergedGuids.has(localChildNode.guid)) {
        MirrorLog.trace("Local child ${localChildNode} can't be an orphan; " +
                        "already merged", { localChildNode });
        continue;
      }
      let structureChange = await this.checkForRemoteStructureChangeOfLocalNode(
        mergedNode, localNode, localChildNode);
      if (structureChange == BookmarkMerger.STRUCTURE.MOVED ||
          structureChange == BookmarkMerger.STRUCTURE.DELETED) {
        // The local child is already moved or deleted remotely, so we should
        // ignore it instead of treating it as a local orphan.
        continue;
      }
      localOrphanNodes.push(localChildNode);
    }

    let mergedOrphanNodes = [];
    for await (let localOrphanNode of yieldingIterator(localOrphanNodes)) {
      let remoteOrphanNode = this.remoteTree.nodeForGuid(localOrphanNode.guid);
      let mergedNode = await this.mergeNode(localOrphanNode.guid,
                                            localOrphanNode, remoteOrphanNode);
      mergedOrphanNodes.push(mergedNode);
    }

    MirrorLog.trace("Relocating local orphans ${mergedOrphanNodes} to " +
                    "${mergedNode}", { mergedOrphanNodes, mergedNode });

    for await (let mergedOrphanNode of yieldingIterator(mergedOrphanNodes)) {
      mergedNode.mergeState = BookmarkMergeState.new(mergedNode.mergeState);
      mergedOrphanNode.mergeState = BookmarkMergeState.new(mergedOrphanNode.mergeState);
      mergedNode.mergedChildren.push(mergedOrphanNode);
    }
  }

  /**
   * Finds all children of a local folder with similar content as children of
   * the corresponding remote folder. This is used to dedupe local items that
   * haven't been uploaded yet, to remote items that don't exist locally.
   *
   * Recall that we match items by GUID as we walk down the tree. If a GUID on
   * one side doesn't exist on the other, we fall back to a content match in
   * the same folder.
   *
   * This method is called the first time that `findRemoteNodeMatchingLocalNode`
   * merges a local child that doesn't exist remotely, and the first time that
   * `findLocalNodeMatchingRemoteNode` merges a remote child that doesn't exist
   * locally.
   *
   * Finding all possible dupes is O(m + n) in the worst case, where `m` is the
   * number of local children, and `n` is the number of remote children. We
   * cache matches in `matchingDupesByLocalParentNode`, so deduping all
   * remaining children of the same folder, on both sides, only needs two O(1)
   * map lookups per child.
   *
   * @param   {BookmarkNode} localParentNode
   *          The local folder containing children to dedupe.
   * @param   {BookmarkNode} remoteParentNode
   *          The corresponding remote folder.
   * @returns {Map.<BookmarkNode, BookmarkNode>}
   *          A bidirectional map of local children to remote children, and
   *          remote children to local children.
   *          `findRemoteNodeMatchingLocalNode` looks up matching remote
   *          children by local node. `findLocalNodeMatchingRemoteNode` looks up
   *          local children by remote node.
   */
  async findAllMatchingDupesInFolders(localParentNode, remoteParentNode) {
    let matches = new Map();
    let dupeKeyToLocalNodes = new Map();

    for await (let localChildNode of yieldingIterator(localParentNode.children)) {
      let localChildContent = this.newLocalContents.get(localChildNode.guid);
      if (!localChildContent) {
        MirrorLog.trace("Not deduping local child ${localChildNode}; already " +
                        "uploaded", { localChildNode });
        continue;
      }
      let remoteChildNodeByGuid = this.remoteTree.nodeForGuid(
        localChildNode.guid);
      if (remoteChildNodeByGuid) {
        MirrorLog.trace("Not deduping local child ${localChildNode}; already " +
                        "exists remotely as ${remoteChildNodeByGuid}",
                        { localChildNode, remoteChildNodeByGuid });
        continue;
      }
      if (this.remoteTree.isDeleted(localChildNode.guid)) {
        MirrorLog.trace("Not deduping local child ${localChildNode}; deleted " +
                        "remotely", { localChildNode });
        continue;
      }
      let dupeKey = makeDupeKey(localChildNode, localChildContent);
      let localNodesForKey = dupeKeyToLocalNodes.get(dupeKey);
      if (localNodesForKey) {
        // Store matching local children in an array, in case multiple children
        // have the same dupe key (for example, a toolbar containing multiple
        // empty folders, as in bug 1213369).
        localNodesForKey.push(localChildNode);
      } else {
        dupeKeyToLocalNodes.set(dupeKey, [localChildNode]);
      }
    }

    for await (let remoteChildNode of yieldingIterator(remoteParentNode.children)) {
      if (matches.has(remoteChildNode)) {
        MirrorLog.trace("Not deduping remote child ${remoteChildNode}; " +
                        "already deduped", { remoteChildNode });
        continue;
      }
      let remoteChildContent = this.newRemoteContents.get(
        remoteChildNode.guid);
      if (!remoteChildContent) {
        MirrorLog.trace("Not deduping remote child ${remoteChildNode}; " +
                        "already merged", { remoteChildNode });
        continue;
      }
      let dupeKey = makeDupeKey(remoteChildNode, remoteChildContent);
      let localNodesForKey = dupeKeyToLocalNodes.get(dupeKey);
      if (!localNodesForKey) {
        MirrorLog.trace("Not deduping remote child ${remoteChildNode}; no " +
                        "local content matches", { remoteChildNode });
        continue;
      }
      let localChildNode = localNodesForKey.shift();
      if (!localChildNode) {
        MirrorLog.trace("Not deduping remote child ${remoteChildNode}; no " +
                        "remaining local content matches", { remoteChildNode });
        continue;
      }
      MirrorLog.trace("Deduping local child ${localChildNode} to remote " +
                      "child ${remoteChildNode}", { localChildNode,
                                                    remoteChildNode });
      matches.set(localChildNode, remoteChildNode);
      matches.set(remoteChildNode, localChildNode);
    }
    return matches;
  }

  /**
   * Finds a remote node with a different GUID that matches the content of a
   * local node. This is the inverse of `findLocalNodeMatchingRemoteNode`.
   *
   * @param  {MergedBookmarkNode} mergedNode
   *         The merged folder node.
   * @param  {BookmarkNode} localChildNode
   *         The NEW local child node.
   * @return {BookmarkNode?}
   *         A matching unmerged remote child node, or `null` if there are no
   *         matching remote items.
   */
  async findRemoteNodeMatchingLocalNode(mergedNode, localChildNode) {
    let remoteParentNode = mergedNode.remoteNode;
    if (!remoteParentNode) {
      MirrorLog.trace("Merged node ${mergedNode} doesn't exist remotely; no " +
                      "potential dupes for local child ${localChildNode}",
                      { mergedNode, localChildNode });
      return null;
    }
    let localParentNode = mergedNode.localNode;
    if (!localParentNode) {
      // Should never happen. Trying to find a remote content match for a
      // child of a folder that doesn't exist locally is a coding error.
      throw new TypeError(
        "Can't find remote content match without local parent");
    }
    let matches = this.matchingDupesByLocalParentNode.get(localParentNode);
    if (!matches) {
      MirrorLog.trace("First local child ${localChildNode} doesn't exist " +
                      "remotely; finding all matching dupes in local " +
                      "${localParentNode} and remote ${remoteParentNode}",
                      { localChildNode, localParentNode, remoteParentNode });
      matches = await this.findAllMatchingDupesInFolders(localParentNode,
                                                         remoteParentNode);
      this.matchingDupesByLocalParentNode.set(localParentNode, matches);
    }
    let newRemoteNode = matches.get(localChildNode);
    if (!newRemoteNode) {
      return null;
    }
    this.dupeCount++;
    return newRemoteNode;
  }

  /**
   * Finds a local node with a different GUID that matches the content of a
   * remote node. This is the inverse of `findRemoteNodeMatchingLocalNode`.
   *
   * @param  {MergedBookmarkNode} mergedNode
   *         The merged folder node.
   * @param  {BookmarkNode} remoteChildNode
   *         The unmerged remote child node.
   * @return {BookmarkNode?}
   *         A matching NEW local child node, or `null` if there are no matching
   *         local items.
   */
  async findLocalNodeMatchingRemoteNode(mergedNode, remoteChildNode) {
    let localParentNode = mergedNode.localNode;
    if (!localParentNode) {
      MirrorLog.trace("Merged node ${mergedNode} doesn't exist locally; no " +
                      "potential dupes for remote child ${remoteChildNode}",
                      { mergedNode, remoteChildNode });
      return null;
    }
    let remoteParentNode = mergedNode.remoteNode;
    if (!remoteParentNode) {
      // Should never happen. Trying to find a local content match for a
      // child of a folder that doesn't exist remotely is a coding error.
      throw new TypeError(
        "Can't find local content match without remote parent");
    }
    let matches = this.matchingDupesByLocalParentNode.get(localParentNode);
    if (!matches) {
      MirrorLog.trace("First remote child ${remoteChildNode} doesn't exist " +
                      "locally; finding all matching dupes in local " +
                      "${localParentNode} and remote ${remoteParentNode}",
                      { remoteChildNode, localParentNode, remoteParentNode });
      matches = await this.findAllMatchingDupesInFolders(localParentNode,
                                                         remoteParentNode);
      this.matchingDupesByLocalParentNode.set(localParentNode, matches);
    }
    let newLocalNode = matches.get(remoteChildNode);
    if (!newLocalNode) {
      return null;
    }
    this.dupeCount++;
    return newLocalNode;
  }

  /**
   * Returns an array of local and remote deletions for logging.
   *
   * @return {String[]}
   */
  deletionsToStrings() {
    let infos = [];
    if (this.deleteLocally.size) {
      infos.push("Delete Locally: " + Array.from(this.deleteLocally).join(
        ", "));
    }
    if (this.deleteRemotely.size) {
      infos.push("Delete Remotely: " + Array.from(this.deleteRemotely).join(
        ", "));
    }
    return infos;
  }
}

/**
 * Structure change types, used to indicate if a node on one side is moved
 * or deleted on the other.
 */
BookmarkMerger.STRUCTURE = {
  UNCHANGED: 1,
  MOVED: 2,
  DELETED: 3,
};

/**
 * Fires bookmark and keyword observer notifications for all changes made during
 * the merge.
 */
class BookmarkObserverRecorder {
  constructor(db, { maxFrecenciesToRecalculate }) {
    this.db = db;
    this.maxFrecenciesToRecalculate = maxFrecenciesToRecalculate;
    this.bookmarkObserverNotifications = [];
    this.shouldInvalidateKeywords = false;
  }

  /**
   * Fires observer notifications for all changed items, invalidates the
   * livemark cache if necessary, and recalculates frecencies for changed
   * URLs. This is called outside the merge transaction.
   */
  async notifyAll() {
    await this.noteAllChanges();
    if (this.shouldInvalidateKeywords) {
      await PlacesUtils.keywords.invalidateCachedKeywords();
    }
    await this.notifyBookmarkObservers();
    await this.updateFrecencies();
  }

  async updateFrecencies() {
    MirrorLog.trace("Recalculating frecencies for new URLs");
    await this.db.execute(`
      UPDATE moz_places SET
        frecency = CALCULATE_FRECENCY(id)
      WHERE id IN (
        SELECT id FROM moz_places
        WHERE frecency < 0
        ORDER BY frecency ASC
        LIMIT :limit
      )`,
      { limit: this.maxFrecenciesToRecalculate });

    // Trigger frecency updates for all affected origins.
    await this.db.execute(`DELETE FROM moz_updateoriginsupdate_temp`);
  }

  /**
   * Records Places observer notifications for removed, added, moved, and
   * changed items.
   */
  async noteAllChanges() {
    MirrorLog.trace("Recording observer notifications for removed items");
    // `ORDER BY v.level DESC` sorts deleted children before parents, to ensure
    // that we update caches in the correct order (bug 1297941). We also order
    // by parent and position so that the notifications are well-ordered for
    // tests.
    let removedItemRows = await this.db.execute(`
      SELECT v.itemId AS id, v.parentId, v.parentGuid, v.position, v.type,
             h.url, v.guid, v.isUntagging
      FROM itemsRemoved v
      LEFT JOIN moz_places h ON h.id = v.placeId
      ORDER BY v.level DESC, v.parentId, v.position`);
    for await (let row of yieldingIterator(removedItemRows)) {
      let info = {
        id: row.getResultByName("id"),
        parentId: row.getResultByName("parentId"),
        position: row.getResultByName("position"),
        type: row.getResultByName("type"),
        urlHref: row.getResultByName("url"),
        guid: row.getResultByName("guid"),
        parentGuid: row.getResultByName("parentGuid"),
        isUntagging: row.getResultByName("isUntagging"),
      };
      this.noteItemRemoved(info);
    }

    MirrorLog.trace("Recording observer notifications for changed GUIDs");
    let changedGuidRows = await this.db.execute(`
      SELECT b.id, b.lastModified, b.type, b.guid AS newGuid,
             c.oldGuid, p.id AS parentId, p.guid AS parentGuid
      FROM guidsChanged c
      JOIN moz_bookmarks b ON b.id = c.itemId
      JOIN moz_bookmarks p ON p.id = b.parent
      ORDER BY c.level, p.id, b.position`);
    for await (let row of yieldingIterator(changedGuidRows)) {
      let info = {
        id: row.getResultByName("id"),
        lastModified: row.getResultByName("lastModified"),
        type: row.getResultByName("type"),
        newGuid: row.getResultByName("newGuid"),
        oldGuid: row.getResultByName("oldGuid"),
        parentId: row.getResultByName("parentId"),
        parentGuid: row.getResultByName("parentGuid"),
      };
      this.noteGuidChanged(info);
    }

    MirrorLog.trace("Recording observer notifications for new items");
    let newItemRows = await this.db.execute(`
      SELECT b.id, p.id AS parentId, b.position, b.type, h.url,
             IFNULL(b.title, "") AS title, b.dateAdded, b.guid,
             p.guid AS parentGuid, n.isTagging
      FROM itemsAdded n
      JOIN moz_bookmarks b ON b.guid = n.guid
      JOIN moz_bookmarks p ON p.id = b.parent
      LEFT JOIN moz_places h ON h.id = b.fk
      ORDER BY n.level, p.id, b.position`);
    for await (let row of yieldingIterator(newItemRows)) {
      let info = {
        id: row.getResultByName("id"),
        parentId: row.getResultByName("parentId"),
        position: row.getResultByName("position"),
        type: row.getResultByName("type"),
        urlHref: row.getResultByName("url"),
        title: row.getResultByName("title"),
        dateAdded: row.getResultByName("dateAdded"),
        guid: row.getResultByName("guid"),
        parentGuid: row.getResultByName("parentGuid"),
        isTagging: row.getResultByName("isTagging"),
      };
      this.noteItemAdded(info);
    }

    MirrorLog.trace("Recording observer notifications for moved items");
    let movedItemRows = await this.db.execute(`
      SELECT b.id, b.guid, b.type, p.id AS newParentId, c.oldParentId,
             p.guid AS newParentGuid, c.oldParentGuid,
             b.position AS newPosition, c.oldPosition, h.url
      FROM itemsMoved c
      JOIN moz_bookmarks b ON b.id = c.itemId
      JOIN moz_bookmarks p ON p.id = b.parent
      LEFT JOIN moz_places h ON h.id = b.fk
      ORDER BY c.level, newParentId, newPosition`);
    for await (let row of yieldingIterator(movedItemRows)) {
      let info = {
        id: row.getResultByName("id"),
        guid: row.getResultByName("guid"),
        type: row.getResultByName("type"),
        newParentId: row.getResultByName("newParentId"),
        oldParentId: row.getResultByName("oldParentId"),
        newParentGuid: row.getResultByName("newParentGuid"),
        oldParentGuid: row.getResultByName("oldParentGuid"),
        newPosition: row.getResultByName("newPosition"),
        oldPosition: row.getResultByName("oldPosition"),
        urlHref: row.getResultByName("url"),
      };
      this.noteItemMoved(info);
    }

    MirrorLog.trace("Recording observer notifications for changed items");
    let changedItemRows = await this.db.execute(`
      SELECT b.id, b.guid, b.lastModified, b.type,
             IFNULL(b.title, "") AS newTitle,
             IFNULL(c.oldTitle, "") AS oldTitle,
             h.url AS newURL, i.url AS oldURL,
             p.id AS parentId, p.guid AS parentGuid
      FROM itemsChanged c
      JOIN moz_bookmarks b ON b.id = c.itemId
      JOIN moz_bookmarks p ON p.id = b.parent
      LEFT JOIN moz_places h ON h.id = b.fk
      LEFT JOIN moz_places i ON i.id = c.oldPlaceId
      ORDER BY c.level, p.id, b.position`);
    for await (let row of yieldingIterator(changedItemRows)) {
      let info = {
        id: row.getResultByName("id"),
        guid: row.getResultByName("guid"),
        lastModified: row.getResultByName("lastModified"),
        type: row.getResultByName("type"),
        newTitle: row.getResultByName("newTitle"),
        oldTitle: row.getResultByName("oldTitle"),
        newURLHref: row.getResultByName("newURL"),
        oldURLHref: row.getResultByName("oldURL"),
        parentId: row.getResultByName("parentId"),
        parentGuid: row.getResultByName("parentGuid"),
      };
      this.noteItemChanged(info);
    }

    MirrorLog.trace("Recording notifications for changed keywords");
    let keywordsChangedRows = await this.db.execute(`
      SELECT EXISTS(SELECT 1 FROM itemsAdded WHERE keywordChanged) OR
             EXISTS(SELECT 1 FROM itemsChanged WHERE keywordChanged)
             AS keywordsChanged`);
    this.shouldInvalidateKeywords =
      !!keywordsChangedRows[0].getResultByName("keywordsChanged");
  }

  noteItemAdded(info) {
    this.bookmarkObserverNotifications.push(new PlacesBookmarkAddition({
      id: info.id,
      parentId: info.parentId,
      index: info.position,
      url: info.urlHref || "",
      title: info.title,
      dateAdded: info.dateAdded,
      guid: info.guid,
      parentGuid: info.parentGuid,
      source: PlacesUtils.bookmarks.SOURCES.SYNC,
      itemType: info.type,
      isTagging: info.isTagging,
    }));
  }

  noteGuidChanged(info) {
    PlacesUtils.invalidateCachedGuidFor(info.id);
    this.bookmarkObserverNotifications.push({
      name: "onItemChanged",
      isTagging: false,
      args: [info.id, "guid", /* isAnnotationProperty */ false, info.newGuid,
        info.lastModified, info.type, info.parentId, info.newGuid,
        info.parentGuid, info.oldGuid, PlacesUtils.bookmarks.SOURCES.SYNC],
    });
  }

  noteItemMoved(info) {
    this.bookmarkObserverNotifications.push({
      name: "onItemMoved",
      isTagging: false,
      args: [info.id, info.oldParentId, info.oldPosition, info.newParentId,
        info.newPosition, info.type, info.guid, info.oldParentGuid,
        info.newParentGuid, PlacesUtils.bookmarks.SOURCES.SYNC, info.urlHref],
    });
  }

  noteItemChanged(info) {
    if (info.oldTitle != info.newTitle) {
      this.bookmarkObserverNotifications.push({
        name: "onItemChanged",
        isTagging: false,
        args: [info.id, "title", /* isAnnotationProperty */ false,
          info.newTitle, info.lastModified, info.type, info.parentId,
          info.guid, info.parentGuid, info.oldTitle,
          PlacesUtils.bookmarks.SOURCES.SYNC],
      });
    }
    if (info.oldURLHref != info.newURLHref) {
      this.bookmarkObserverNotifications.push({
        name: "onItemChanged",
        isTagging: false,
        args: [info.id, "uri", /* isAnnotationProperty */ false,
          info.newURLHref, info.lastModified, info.type, info.parentId,
          info.guid, info.parentGuid, info.oldURLHref,
          PlacesUtils.bookmarks.SOURCES.SYNC],
      });
    }
  }

  noteItemRemoved(info) {
    let uri = info.urlHref ? Services.io.newURI(info.urlHref) : null;
    this.bookmarkObserverNotifications.push({
      name: "onItemRemoved",
      isTagging: info.isUntagging,
      args: [info.id, info.parentId, info.position, info.type, uri, info.guid,
        info.parentGuid, PlacesUtils.bookmarks.SOURCES.SYNC],
    });
  }

  async notifyBookmarkObservers() {
    MirrorLog.trace("Notifying bookmark observers");
    let observers = PlacesUtils.bookmarks.getObservers();
    for (let observer of observers) {
      this.notifyObserver(observer, "onBeginUpdateBatch");
    }
    for await (let info of yieldingIterator(this.bookmarkObserverNotifications)) {
      if (info instanceof PlacesEvent) {
        PlacesObservers.notifyListeners([info]);
      } else {
        for (let observer of observers) {
          if (info.isTagging && observer.skipTags) {
            continue;
          }
          this.notifyObserver(observer, info.name, info.args);
        }
      }
    }
    for (let observer of observers) {
      this.notifyObserver(observer, "onEndUpdateBatch");
    }
  }

  notifyObserver(observer, notification, args = []) {
    try {
      observer[notification](...args);
    } catch (ex) {
      MirrorLog.warn("Error notifying observer", ex);
    }
  }
}

/**
 * Holds Sync metadata and the cleartext for a locally changed record. The
 * bookmarks engine inflates a Sync record from the cleartext, and updates the
 * `synced` property for successfully uploaded items.
 *
 * At the end of the sync, the engine writes the uploaded cleartext back to the
 * mirror, and passes the updated change record as part of the changeset to
 * `PlacesSyncUtils.bookmarks.pushChanges`.
 */
class BookmarkChangeRecord {
  constructor(syncChangeCounter, cleartext) {
    this.tombstone = cleartext.deleted === true;
    this.counter = syncChangeCounter;
    this.cleartext = cleartext;
    this.synced = false;
  }
}

// In conclusion, this is why bookmark syncing is hard.
