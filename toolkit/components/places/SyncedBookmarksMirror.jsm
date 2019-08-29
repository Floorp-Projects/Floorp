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

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

XPCOMUtils.defineLazyModuleGetters(this, {
  Async: "resource://services-common/async.js",
  Log: "resource://gre/modules/Log.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PlacesSyncUtils: "resource://gre/modules/PlacesSyncUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "MirrorLog", () =>
  Log.repository.getLogger("Sync.Engine.Bookmarks.Mirror")
);

const SyncedBookmarksMerger = Components.Constructor(
  "@mozilla.org/browser/synced-bookmarks-merger;1",
  "mozISyncedBookmarksMerger"
);

// These can be removed once they're exposed in a central location (bug
// 1375896).
const DB_URL_LENGTH_MAX = 65536;
const DB_TITLE_LENGTH_MAX = 4096;

const SQLITE_MAX_VARIABLE_NUMBER = 999;

// The current mirror database schema version. Bump for migrations, then add
// migration code to `migrateMirrorSchema`.
const MIRROR_SCHEMA_VERSION = 7;

const DEFAULT_MAX_FRECENCIES_TO_RECALCULATE = 400;

// Use a shared jankYielder in these functions
XPCOMUtils.defineLazyGetter(this, "yieldState", () => Async.yieldState());

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
 * A helper to track the progress of a merge for telemetry and shutdown hang
 * reporting.
 */
class ProgressTracker {
  constructor(recordStepTelemetry) {
    this.recordStepTelemetry = recordStepTelemetry;
    this.steps = [];
  }

  /**
   * Records a merge step, updating the shutdown blocker state.
   *
   * @param {String} name A step name from `ProgressTracker.STEPS`. This is
   *        included in shutdown hang crash reports, along with the timestamp
   *        the step was recorded.
   * @param {Number} [took] The time taken, in milliseconds.
   * @param {Array} [counts] An array of additional counts to report in the
   *        shutdown blocker state.
   */
  step(name, took = -1, counts = null) {
    let info = { step: name, at: Date.now() };
    if (took > -1) {
      info.took = took;
    }
    if (counts) {
      info.counts = counts;
    }
    this.steps.push(info);
  }

  /**
   * Records a merge step with timings and counts for telemetry.
   *
   * @param {String} name The step name.
   * @param {Number} took The time taken, in milliseconds.
   * @param {Array} [counts] An array of additional `{ name, count }` tuples to
   *        record in telemetry for this step.
   */
  stepWithTelemetry(name, took, counts = null) {
    this.step(name, took, counts);
    this.recordStepTelemetry(name, took, counts);
  }

  /**
   * Records a merge step with the time taken and item count.
   *
   * @param {String} name The step name.
   * @param {Number} took The time taken, in milliseconds.
   * @param {Number} count The number of items handled in this step.
   */
  stepWithItemCount(name, took, count) {
    this.stepWithTelemetry(name, took, [{ name: "items", count }]);
  }

  /**
   * Clears all recorded merge steps.
   */
  reset() {
    this.steps = [];
  }

  /**
   * Returns the shutdown blocker state. This is included in shutdown hang
   * crash reports, in the `AsyncShutdownTimeout` annotation.
   *
   * @see    `fetchState` in `AsyncShutdown` for more details.
   * @return {Object} A stringifiable object with the recorded steps.
   */
  fetchState() {
    return { steps: this.steps };
  }
}

/** Merge steps for which we record progress. */
ProgressTracker.STEPS = {
  FETCH_LOCAL_TREE: "fetchLocalTree",
  FETCH_REMOTE_TREE: "fetchRemoteTree",
  MERGE: "merge",
  APPLY: "apply",
  NOTIFY_OBSERVERS: "notifyObservers",
  FETCH_LOCAL_CHANGE_RECORDS: "fetchLocalChangeRecords",
  FINALIZE: "finalize",
};

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
  constructor(
    db,
    {
      recordTelemetryEvent,
      recordStepTelemetry,
      recordValidationTelemetry,
      finalizeAt = PlacesUtils.history.shutdownClient.jsclient,
    } = {}
  ) {
    this.db = db;
    this.recordTelemetryEvent = recordTelemetryEvent;
    this.recordValidationTelemetry = recordValidationTelemetry;

    this.merger = new SyncedBookmarksMerger();
    this.merger.db = db.unsafeRawConnection.QueryInterface(
      Ci.mozIStorageConnection
    );
    this.merger.logger = new MirrorLoggerAdapter(MirrorLog);

    // Automatically close the database connection on shutdown. `progress`
    // tracks state for shutdown hang reporting.
    this.progress = new ProgressTracker(recordStepTelemetry);
    this.finalizeController = new AbortController();
    this.finalizeAt = finalizeAt;
    this.finalizeBound = () => this.finalize({ alsoCleanup: false });
    this.finalizeAt.addBlocker(
      "SyncedBookmarksMirror: finalize",
      this.finalizeBound,
      { fetchState: () => this.progress }
    );
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
   * @param  {Function} options.recordStepTelemetry
   *         A function with the signature `(name: String, took: Number,
   *         counts: Array?)`, where `name` is the name of the merge step,
   *         `took` is the time taken in milliseconds, and `counts` is an
   *         array of named counts (`{ name, count }` tuples) with additional
   *         counts for the step to record in the telemetry ping.
   * @param  {Function} options.recordValidationTelemetry
   *         A function with the signature `(took: Number, checked: Number,
   *         problems: Array)`, where `took` is the time taken to run
   *         validation in milliseconds, `checked` is the number of items
   *         checked, and `problems` is an array of named problem counts.
   * @param  {AsyncShutdown.Barrier} [options.finalizeAt]
   *         A shutdown phase, barrier, or barrier client that should
   *         automatically finalize the mirror when triggered. Exposed for
   *         testing.
   * @return {SyncedBookmarksMirror}
   *         A mirror ready for use.
   */
  static async open(options) {
    let db = await PlacesUtils.promiseUnsafeWritableDBConnection();
    let path = OS.Path.join(OS.Constants.Path.profileDir, options.path);
    let whyFailed = "initialize";
    try {
      try {
        await attachAndInitMirrorDatabase(db, path);
      } catch (ex) {
        if (isDatabaseCorrupt(ex)) {
          MirrorLog.warn(
            "Error attaching mirror to Places; removing and " +
              "recreating mirror",
            ex
          );
          options.recordTelemetryEvent("mirror", "open", "retry", {
            why: "corrupt",
          });

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
      options.recordTelemetryEvent("mirror", "open", "error", {
        why: whyFailed,
      });
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
    let rows = await this.db.executeCached(
      `
      SELECT MAX(
        IFNULL((SELECT MAX(serverModified) - 1000 FROM items), 0),
        IFNULL((SELECT CAST(value AS INTEGER) FROM meta
                WHERE key = :modifiedKey), 0)
      ) AS highWaterMark`,
      { modifiedKey: SyncedBookmarksMirror.META_KEY.LAST_MODIFIED }
    );
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
      db =>
        db.executeCached(
          `
        REPLACE INTO meta(key, value)
        VALUES(:modifiedKey, :lastModified)`,
          {
            modifiedKey: SyncedBookmarksMirror.META_KEY.LAST_MODIFIED,
            lastModified,
          }
        )
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
    let rows = await this.db.executeCached(
      `
      SELECT value FROM meta WHERE key = :syncIdKey`,
      { syncIdKey: SyncedBookmarksMirror.META_KEY.SYNC_ID }
    );
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
    MirrorLog.info(
      "Sync ID changed from ${existingSyncId} to " +
        "${newSyncId}; resetting mirror",
      { existingSyncId, newSyncId }
    );
    await this.db.executeBeforeShutdown(
      "SyncedBookmarksMirror: ensureCurrentSyncId",
      db =>
        db.executeTransaction(async function() {
          await resetMirror(db);
          await db.execute(
            `
          REPLACE INTO meta(key, value)
          VALUES(:syncIdKey, :newSyncId)`,
            { syncIdKey: SyncedBookmarksMirror.META_KEY.SYNC_ID, newSyncId }
          );
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
    await this.db.executeBeforeShutdown("SyncedBookmarksMirror: store", db =>
      db.executeTransaction(async () => {
        await Async.yieldingForEach(
          records,
          async record => {
            let guid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.id);
            if (guid == PlacesUtils.bookmarks.rootGuid) {
              // The engine should hard DELETE Places roots from the server.
              throw new TypeError("Can't store Places root");
            }
            MirrorLog.trace(`Storing in mirror: ${record.cleartextToString()}`);
            switch (record.type) {
              case "bookmark":
                await this.storeRemoteBookmark(record, options);
                return;

              case "query":
                await this.storeRemoteQuery(record, options);
                return;

              case "folder":
                await this.storeRemoteFolder(record, options);
                return;

              case "livemark":
                await this.storeRemoteLivemark(record, options);
                return;

              case "separator":
                await this.storeRemoteSeparator(record, options);
                return;

              default:
                if (record.deleted) {
                  await this.storeRemoteTombstone(record, options);
                  return;
                }
            }
            MirrorLog.warn("Ignoring record with unknown type", record.type);
          },
          yieldState
        );
      })
    );
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
   * @param  {Boolean} [options.notifyInStableOrder]
   *         If `true`, fire observer notifications for items in the same folder
   *         in a stable order. This is disabled by default, to avoid the cost
   *         of sorting the notifications, but enabled in some tests to simplify
   *         their checks.
   * @param  {AbortSignal} [options.signal]
   *         An abort signal that can be used to interrupt a merge when its
   *         associated `AbortController` is aborted. If omitted, the merge can
   *         still be interrupted when the mirror is finalized.
   * @return {Object.<String, BookmarkChangeRecord>}
   *         A changeset containing locally changed and reconciled records to
   *         upload to the server, and to store in the mirror once upload
   *         succeeds.
   */
  async apply({
    localTimeSeconds,
    remoteTimeSeconds,
    weakUpload,
    maxFrecenciesToRecalculate,
    notifyInStableOrder,
    signal = null,
  } = {}) {
    // We intentionally don't use `executeBeforeShutdown` in this function,
    // since merging can take a while for large trees, and we don't want to
    // block shutdown. Since all new items are in the mirror, we'll just try
    // to merge again on the next sync.

    let finalizeOrInterruptSignal = anyAborted(
      this.finalizeController.signal,
      signal
    );

    let changeRecords;
    try {
      changeRecords = await this.tryApply(
        finalizeOrInterruptSignal,
        localTimeSeconds,
        remoteTimeSeconds,
        weakUpload,
        maxFrecenciesToRecalculate,
        notifyInStableOrder
      );
    } finally {
      this.progress.reset();
    }

    return changeRecords;
  }

  async tryApply(
    signal,
    localTimeSeconds,
    remoteTimeSeconds,
    weakUpload,
    maxFrecenciesToRecalculate = DEFAULT_MAX_FRECENCIES_TO_RECALCULATE,
    notifyInStableOrder = false
  ) {
    let wasMerged = await withTiming("Merging bookmarks in Rust", () =>
      this.merge(signal, localTimeSeconds, remoteTimeSeconds, weakUpload)
    );

    if (!wasMerged) {
      MirrorLog.debug("No changes detected in both mirror and Places");
      await updateFrecencies(this.db, maxFrecenciesToRecalculate);
      return {};
    }

    // At this point, the database is consistent, so we can notify observers and
    // inflate records for outgoing items.

    let observersToNotify = new BookmarkObserverRecorder(this.db, {
      maxFrecenciesToRecalculate,
      signal,
      notifyInStableOrder,
    });

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
          // Places relies on observer notifications to update internal caches.
          // If notifying observers failed, these caches may be inconsistent,
          // so we invalidate them just in case.
          PlacesUtils.invalidateCachedGuids();
          await PlacesUtils.keywords.invalidateCachedKeywords();
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
      time =>
        this.progress.stepWithTelemetry(
          ProgressTracker.STEPS.NOTIFY_OBSERVERS,
          time
        )
    );

    let { changeRecords } = await withTiming(
      "Fetching records for local items to upload",
      async () => {
        try {
          let result = await this.fetchLocalChangeRecords(signal);
          return result;
        } finally {
          await this.db.execute(`DELETE FROM itemsToUpload`);
        }
      },
      (time, result) =>
        this.progress.stepWithItemCount(
          ProgressTracker.STEPS.FETCH_LOCAL_CHANGE_RECORDS,
          time,
          result.count
        )
    );

    return changeRecords;
  }

  merge(
    signal,
    localTimeSeconds = Date.now() / 1000,
    remoteTimeSeconds = 0,
    weakUpload = []
  ) {
    return new Promise((resolve, reject) => {
      let op = null;
      function onAbort() {
        signal.removeEventListener("abort", onAbort);
        op.cancel();
      }
      let callback = {
        QueryInterface: ChromeUtils.generateQI([
          Ci.mozISyncedBookmarksMirrorProgressListener,
          Ci.mozISyncedBookmarksMirrorCallback,
        ]),
        // `mozISyncedBookmarksMirrorProgressListener` methods.
        onFetchLocalTree: (took, count, problems) => {
          this.progress.stepWithItemCount(
            ProgressTracker.STEPS.FETCH_LOCAL_TREE,
            took,
            count
          );
          // We don't record local tree problems in validation telemetry.
        },
        onFetchRemoteTree: (took, count, problemsBag) => {
          this.progress.stepWithItemCount(
            ProgressTracker.STEPS.FETCH_REMOTE_TREE,
            took,
            count
          );
          // Record validation telemetry for problems in the remote tree.
          let problems = bagToNamedCounts(problemsBag, [
            "orphans",
            "misparentedRoots",
            "multipleParents",
            "nonFolderParents",
            "parentChildDisagreements",
            "missingChildren",
          ]);
          this.recordValidationTelemetry(took, count, problems);
        },
        onMerge: (took, countsBag) => {
          let counts = bagToNamedCounts(countsBag, [
            "items",
            "deletes",
            "dupes",
            "remoteRevives",
            "localDeletes",
            "localRevives",
            "remoteDeletes",
          ]);
          this.progress.stepWithTelemetry(
            ProgressTracker.STEPS.MERGE,
            took,
            counts
          );
        },
        onApply: took => {
          this.progress.stepWithTelemetry(ProgressTracker.STEPS.APPLY, took);
        },
        // `mozISyncedBookmarksMirrorCallback` methods.
        handleSuccess(result) {
          signal.removeEventListener("abort", onAbort);
          resolve(result);
        },
        handleError(code, message) {
          signal.removeEventListener("abort", onAbort);
          switch (code) {
            case Cr.NS_ERROR_STORAGE_BUSY:
              reject(
                new SyncedBookmarksMirror.MergeConflictError(
                  "Local tree changed during merge"
                )
              );
              break;

            case Cr.NS_ERROR_ABORT:
              reject(new SyncedBookmarksMirror.InterruptedError(message));
              break;

            default:
              reject(new SyncedBookmarksMirror.MergeError(message));
          }
        },
      };
      op = this.merger.merge(
        localTimeSeconds,
        remoteTimeSeconds,
        weakUpload,
        callback
      );
      if (signal.aborted) {
        op.cancel();
      } else {
        signal.addEventListener("abort", onAbort);
      }
    });
  }

  /**
   * Discards the mirror contents. This is called when the user is node
   * reassigned, disables the bookmarks engine, or signs out.
   */
  async reset() {
    await this.db.executeBeforeShutdown("SyncedBookmarksMirror: reset", db =>
      db.executeTransaction(() => resetMirror(db))
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
    let rows = await this.db.execute(`
      SELECT guid FROM items
      WHERE needsMerge
      ORDER BY guid`);
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
    let validity = url
      ? Ci.mozISyncedBookmarksMerger.VALIDITY_VALID
      : Ci.mozISyncedBookmarksMerger.VALIDITY_REPLACE;

    await this.db.executeCached(
      `
      REPLACE INTO items(guid, parentGuid, serverModified, needsMerge, kind,
                         dateAdded, title, keyword, validity,
                         urlId)
      VALUES(:guid, :parentGuid, :serverModified, :needsMerge, :kind,
             :dateAdded, NULLIF(:title, ''), :keyword, :validity,
             (SELECT id FROM urls
              WHERE hash = hash(:url) AND
                    url = :url))`,
      {
        guid,
        parentGuid,
        serverModified,
        needsMerge,
        kind: Ci.mozISyncedBookmarksMerger.KIND_BOOKMARK,
        dateAdded,
        title,
        keyword,
        url: url ? url.href : null,
        validity,
      }
    );

    let tags = record.tags;
    if (tags && Array.isArray(tags)) {
      for (let rawTag of tags) {
        let tag = validateTag(rawTag);
        if (!tag) {
          continue;
        }
        await this.db.executeCached(
          `
          INSERT INTO tags(itemId, tag)
          SELECT id, :tag FROM items
          WHERE guid = :guid`,
          { tag, guid }
        );
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

    await this.db.executeCached(
      `
      REPLACE INTO items(guid, parentGuid, serverModified, needsMerge, kind,
                         dateAdded, title,
                         urlId,
                         validity)
      VALUES(:guid, :parentGuid, :serverModified, :needsMerge, :kind,
             :dateAdded, NULLIF(:title, ''),
             (SELECT id FROM urls
              WHERE hash = hash(:url) AND
                    url = :url),
             :validity)`,
      {
        guid,
        parentGuid,
        serverModified,
        needsMerge,
        kind: Ci.mozISyncedBookmarksMerger.KIND_QUERY,
        dateAdded,
        title,
        url: url ? url.href : null,
        validity,
      }
    );
  }

  async storeRemoteFolder(record, { needsMerge }) {
    let guid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.id);
    let parentGuid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.parentid);
    let serverModified = determineServerModified(record);
    let dateAdded = determineDateAdded(record);
    let title = validateTitle(record.title);

    await this.db.executeCached(
      `
      REPLACE INTO items(guid, parentGuid, serverModified, needsMerge, kind,
                         dateAdded, title)
      VALUES(:guid, :parentGuid, :serverModified, :needsMerge, :kind,
             :dateAdded, NULLIF(:title, ''))`,
      {
        guid,
        parentGuid,
        serverModified,
        needsMerge,
        kind: Ci.mozISyncedBookmarksMerger.KIND_FOLDER,
        dateAdded,
        title,
      }
    );

    let children = record.children;
    if (children && Array.isArray(children)) {
      for (let [offset, chunk] of PlacesSyncUtils.chunkArray(
        children,
        SQLITE_MAX_VARIABLE_NUMBER - 1
      )) {
        // Builds a fragment like `(?2, ?1, 0), (?3, ?1, 1), ...`, where ?1 is
        // the folder's GUID, [?2, ?3] are the first and second child GUIDs
        // (SQLite binding parameters index from 1), and [0, 1] are the
        // positions. This lets us store the folder's children using as few
        // statements as possible.
        let valuesFragment = Array.from(
          { length: chunk.length },
          (_, index) => `(?${index + 2}, ?1, ${offset + index})`
        ).join(",");
        await this.db.execute(
          `
          INSERT INTO structure(guid, parentGuid, position)
          VALUES ${valuesFragment}`,
          [guid, ...chunk.map(PlacesSyncUtils.bookmarks.recordIdToGuid)]
        );
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

    let validity = feedURL
      ? Ci.mozISyncedBookmarksMerger.VALIDITY_VALID
      : Ci.mozISyncedBookmarksMerger.VALIDITY_REPLACE;

    await this.db.executeCached(
      `
      REPLACE INTO items(guid, parentGuid, serverModified, needsMerge, kind,
                         dateAdded, title, feedURL, siteURL, validity)
      VALUES(:guid, :parentGuid, :serverModified, :needsMerge, :kind,
             :dateAdded, NULLIF(:title, ''), :feedURL, :siteURL, :validity)`,
      {
        guid,
        parentGuid,
        serverModified,
        needsMerge,
        kind: Ci.mozISyncedBookmarksMerger.KIND_LIVEMARK,
        dateAdded,
        title,
        feedURL: feedURL ? feedURL.href : null,
        siteURL: siteURL ? siteURL.href : null,
        validity,
      }
    );
  }

  async storeRemoteSeparator(record, { needsMerge }) {
    let guid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.id);
    let parentGuid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.parentid);
    let serverModified = determineServerModified(record);
    let dateAdded = determineDateAdded(record);

    await this.db.executeCached(
      `
      REPLACE INTO items(guid, parentGuid, serverModified, needsMerge, kind,
                         dateAdded)
      VALUES(:guid, :parentGuid, :serverModified, :needsMerge, :kind,
             :dateAdded)`,
      {
        guid,
        parentGuid,
        serverModified,
        needsMerge,
        kind: Ci.mozISyncedBookmarksMerger.KIND_SEPARATOR,
        dateAdded,
      }
    );
  }

  async storeRemoteTombstone(record, { needsMerge }) {
    let guid = PlacesSyncUtils.bookmarks.recordIdToGuid(record.id);
    let serverModified = determineServerModified(record);

    await this.db.executeCached(
      `
      REPLACE INTO items(guid, serverModified, needsMerge, isDeleted)
      VALUES(:guid, :serverModified, :needsMerge, 1)`,
      { guid, serverModified, needsMerge }
    );
  }

  async maybeStoreRemoteURL(url) {
    await this.db.executeCached(
      `
      INSERT OR IGNORE INTO urls(guid, url, hash, revHost)
      VALUES(IFNULL((SELECT guid FROM urls
                     WHERE hash = hash(:url) AND
                                  url = :url),
                    GENERATE_GUID()), :url, hash(:url), :revHost)`,
      { url: url.href, revHost: PlacesUtils.getReversedHost(url) }
    );
  }

  /**
   * Inflates Sync records for all staged outgoing items.
   *
   * @param  {AbortSignal} signal
   *         Stops fetching records when the associated `AbortController`
   *         is aborted.
   * @return {Object}
   *         A `{ changeRecords, count }` tuple, where `changeRecords` is a
   *         changeset containing Sync record cleartexts for outgoing items and
   *         tombstones, keyed by their Sync record IDs, and `count` is the
   *         number of records.
   */
  async fetchLocalChangeRecords(signal) {
    let changeRecords = {};
    let childRecordIdsByLocalParentId = new Map();
    let tagsByLocalId = new Map();

    let childGuidRows = [];
    await this.db.execute(
      `SELECT parentId, guid FROM structureToUpload
       ORDER BY parentId, position`,
      null,
      (row, cancel) => {
        if (signal.aborted) {
          cancel();
        } else {
          // `Sqlite.jsm` callbacks swallow exceptions (bug 1387775), so we
          // accumulate all rows in an array, and process them after.
          childGuidRows.push(row);
        }
      }
    );

    await Async.yieldingForEach(
      childGuidRows,
      row => {
        if (signal.aborted) {
          throw new SyncedBookmarksMirror.InterruptedError(
            "Interrupted while fetching structure to upload"
          );
        }
        let localParentId = row.getResultByName("parentId");
        let childRecordId = PlacesSyncUtils.bookmarks.guidToRecordId(
          row.getResultByName("guid")
        );
        let childRecordIds = childRecordIdsByLocalParentId.get(localParentId);
        if (childRecordIds) {
          childRecordIds.push(childRecordId);
        } else {
          childRecordIdsByLocalParentId.set(localParentId, [childRecordId]);
        }
      },
      yieldState
    );

    let tagRows = [];
    await this.db.execute(
      `SELECT id, tag FROM tagsToUpload`,
      null,
      (row, cancel) => {
        if (signal.aborted) {
          cancel();
        } else {
          tagRows.push(row);
        }
      }
    );

    await Async.yieldingForEach(
      tagRows,
      row => {
        if (signal.aborted) {
          throw new SyncedBookmarksMirror.InterruptedError(
            "Interrupted while fetching tags to upload"
          );
        }
        let localId = row.getResultByName("id");
        let tag = row.getResultByName("tag");
        let tags = tagsByLocalId.get(localId);
        if (tags) {
          tags.push(tag);
        } else {
          tagsByLocalId.set(localId, [tag]);
        }
      },
      yieldState
    );

    let itemRows = [];
    await this.db.execute(
      `SELECT id, syncChangeCounter, guid, isDeleted, type, isQuery,
              tagFolderName, keyword, url, IFNULL(title, '') AS title,
              position, parentGuid,
              IFNULL(parentTitle, '') AS parentTitle, dateAdded
       FROM itemsToUpload`,
      null,
      (row, cancel) => {
        if (signal.interrupted) {
          cancel();
        } else {
          itemRows.push(row);
        }
      }
    );

    await Async.yieldingForEach(
      itemRows,
      row => {
        if (signal.aborted) {
          throw new SyncedBookmarksMirror.InterruptedError(
            "Interrupted while fetching items to upload"
          );
        }
        let syncChangeCounter = row.getResultByName("syncChangeCounter");

        let guid = row.getResultByName("guid");
        let recordId = PlacesSyncUtils.bookmarks.guidToRecordId(guid);

        // Tombstones don't carry additional properties.
        let isDeleted = row.getResultByName("isDeleted");
        if (isDeleted) {
          changeRecords[recordId] = new BookmarkChangeRecord(
            syncChangeCounter,
            {
              id: recordId,
              deleted: true,
            }
          );
          return;
        }

        let parentGuid = row.getResultByName("parentGuid");
        let parentRecordId = PlacesSyncUtils.bookmarks.guidToRecordId(
          parentGuid
        );

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
                syncChangeCounter,
                queryCleartext
              );
              return;
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
              syncChangeCounter,
              bookmarkCleartext
            );
            return;
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
              syncChangeCounter,
              folderCleartext
            );
            return;
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
              syncChangeCounter,
              separatorCleartext
            );
            return;
          }

          default:
            throw new TypeError("Can't create record for unknown Places item");
        }
      },
      yieldState
    );

    return { changeRecords, count: itemRows.length };
  }

  /**
   * Closes the mirror database connection. This is called automatically on
   * shutdown, but may also be called explicitly when the mirror is no longer
   * needed.
   *
   * @param {Boolean} [options.alsoCleanup]
   *                  If specified, drop all temp tables, views, and triggers,
   *                  and detach from the mirror database before closing the
   *                  connection. Defaults to `true`.
   */
  finalize({ alsoCleanup = true } = {}) {
    if (!this.finalizePromise) {
      this.finalizePromise = (async () => {
        this.progress.step(ProgressTracker.STEPS.FINALIZE);
        this.finalizeController.abort();
        this.merger.reset();
        if (alsoCleanup) {
          // If the mirror is finalized explicitly, clean up temp entities and
          // detach from the mirror database. We can skip this for automatic
          // finalization, since the Places connection is already shutting
          // down.
          await cleanupMirrorDatabase(this.db);
        }
        await this.db.execute(`PRAGMA mirror.optimize(0x02)`);
        await this.db.execute(`DETACH mirror`);
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
 * An error thrown when the merge was interrupted.
 */
class InterruptedError extends Error {
  constructor(message) {
    super(message);
    this.name = "InterruptedError";
  }
}
SyncedBookmarksMirror.InterruptedError = InterruptedError;

/**
 * An error thrown when the merge failed for an unexpected reason.
 */
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
    return error.errors.some(
      error =>
        error instanceof Ci.mozIStorageError &&
        (error.result == Ci.mozIStorageError.CORRUPT ||
          error.result == Ci.mozIStorageError.NOTADB)
    );
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
  if (currentSchemaVersion < 5) {
    // The mirror was pref'd off by default for schema versions 1-4.
    throw new DatabaseCorruptError(
      `Can't migrate from schema version ${currentSchemaVersion}; too old`
    );
  }
  if (currentSchemaVersion < 6) {
    await db.execute(`CREATE INDEX mirror.itemURLs ON items(urlId)`);
    await db.execute(`CREATE INDEX mirror.itemKeywords ON items(keyword)
                      WHERE keyword NOT NULL`);
  }
  if (currentSchemaVersion < 7) {
    await db.execute(`CREATE INDEX mirror.structurePositions ON
                      structure(parentGuid, position)`);
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
    key TEXT PRIMARY KEY,
    value NOT NULL
  ) WITHOUT ROWID`);

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
      Ci.mozISyncedBookmarksMerger.VALIDITY_VALID
    },
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
    guid TEXT,
    parentGuid TEXT REFERENCES items(guid)
                    ON DELETE CASCADE,
    position INTEGER NOT NULL,
    PRIMARY KEY(parentGuid, guid)
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

  await db.execute(
    `CREATE INDEX mirror.structurePositions ON structure(parentGuid, position)`
  );

  await db.execute(`CREATE INDEX mirror.urlHashes ON urls(hash)`);

  await db.execute(`CREATE INDEX mirror.itemURLs ON items(urlId)`);

  await db.execute(`CREATE INDEX mirror.itemKeywords ON items(keyword)
                    WHERE keyword NOT NULL`);

  await createMirrorRoots(db);
}

/**
 * Drops all temp tables, views, and triggers used for merging, and detaches
 * from the mirror database.
 *
 * @param {Sqlite.OpenedConnection} db
 *        The mirror database connection.
 */
async function cleanupMirrorDatabase(db) {
  await db.executeTransaction(async function() {
    await db.execute(`DROP TABLE changeGuidOps`);
    await db.execute(`DROP TABLE itemsToApply`);
    await db.execute(`DROP TABLE applyNewLocalStructureOps`);
    await db.execute(`DROP TABLE itemsToRemove`);
    await db.execute(`DROP VIEW localTags`);
    await db.execute(`DROP TABLE itemsAdded`);
    await db.execute(`DROP TABLE guidsChanged`);
    await db.execute(`DROP TABLE itemsChanged`);
    await db.execute(`DROP TABLE itemsMoved`);
    await db.execute(`DROP TABLE itemsRemoved`);
    await db.execute(`DROP TABLE itemsToUpload`);
    await db.execute(`DROP TABLE structureToUpload`);
    await db.execute(`DROP TABLE tagsToUpload`);
  });
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
  const syncableRoots = [
    {
      guid: PlacesUtils.bookmarks.rootGuid,
      // The Places root is its own parent, to satisfy the foreign key and
      // `NOT NULL` constraints on `structure`.
      parentGuid: PlacesUtils.bookmarks.rootGuid,
      position: -1,
      needsMerge: false,
    },
    ...PlacesUtils.bookmarks.userContentRoots.map((guid, position) => {
      return {
        guid,
        parentGuid: PlacesUtils.bookmarks.rootGuid,
        position,
        needsMerge: true,
      };
    }),
  ];

  for (let { guid, parentGuid, position, needsMerge } of syncableRoots) {
    await db.executeCached(
      `
      INSERT INTO items(guid, parentGuid, kind, needsMerge)
      VALUES(:guid, :parentGuid, :kind, :needsMerge)`,
      {
        guid,
        parentGuid,
        kind: Ci.mozISyncedBookmarksMerger.KIND_FOLDER,
        needsMerge,
      }
    );

    await db.executeCached(
      `
      INSERT INTO structure(guid, parentGuid, position)
      VALUES(:guid, :parentGuid, :position)`,
      { guid, parentGuid, position }
    );
  }
}

/**
 * Creates temporary tables, views, and triggers to apply the mirror to Places.
 *
 * @param {Sqlite.OpenedConnection} db
 *        The mirror database connection.
 */
async function initializeTempMirrorEntities(db) {
  await db.execute(`CREATE TEMP TABLE changeGuidOps(
    localGuid TEXT PRIMARY KEY,
    mergedGuid TEXT UNIQUE NOT NULL,
    syncStatus INTEGER,
    level INTEGER NOT NULL,
    lastModifiedMicroseconds INTEGER NOT NULL
  ) WITHOUT ROWID`);

  await db.execute(`
    CREATE TEMP TRIGGER changeGuids
    AFTER DELETE ON changeGuidOps
    BEGIN
      /* Record item changed notifications for the updated GUIDs. */
      INSERT INTO guidsChanged(itemId, oldGuid, level)
      SELECT b.id, OLD.localGuid, OLD.level
      FROM moz_bookmarks b
      WHERE b.guid = OLD.localGuid;

      UPDATE moz_bookmarks SET
        guid = OLD.mergedGuid,
        lastModified = OLD.lastModifiedMicroseconds,
        syncStatus = IFNULL(OLD.syncStatus, syncStatus)
      WHERE guid = OLD.localGuid;
    END`);

  await db.execute(`CREATE TEMP TABLE itemsToApply(
    mergedGuid TEXT PRIMARY KEY,
    localId INTEGER UNIQUE,
    remoteId INTEGER UNIQUE NOT NULL,
    remoteGuid TEXT UNIQUE NOT NULL,
    newLevel INTEGER NOT NULL,
    newType INTEGER NOT NULL,
    localDateAddedMicroseconds INTEGER,
    remoteDateAddedMicroseconds INTEGER NOT NULL,
    lastModifiedMicroseconds INTEGER NOT NULL,
    oldTitle TEXT,
    newTitle TEXT,
    oldPlaceId INTEGER,
    newPlaceId INTEGER,
    newKeyword TEXT
  )`);

  await db.execute(`CREATE INDEX existingItems ON itemsToApply(localId)
                    WHERE localId NOT NULL`);

  await db.execute(`CREATE INDEX oldPlaceIds ON itemsToApply(oldPlaceId)
                    WHERE oldPlaceId NOT NULL`);

  await db.execute(`CREATE INDEX newPlaceIds ON itemsToApply(newPlaceId)
                    WHERE newPlaceId NOT NULL`);

  await db.execute(`CREATE INDEX newKeywords ON itemsToApply(newKeyword)
                    WHERE newKeyword NOT NULL`);

  await db.execute(`CREATE TEMP TABLE applyNewLocalStructureOps(
    mergedGuid TEXT PRIMARY KEY,
    mergedParentGuid TEXT NOT NULL,
    position INTEGER NOT NULL,
    level INTEGER NOT NULL,
    lastModifiedMicroseconds INTEGER NOT NULL
  ) WITHOUT ROWID`);

  await db.execute(`
    CREATE TEMP TRIGGER applyNewLocalStructure
    AFTER DELETE ON applyNewLocalStructureOps
    BEGIN
      INSERT INTO itemsMoved(itemId, oldParentId, oldParentGuid, oldPosition,
                             level)
      SELECT b.id, p.id, p.guid, b.position, OLD.level
      FROM moz_bookmarks b
      JOIN moz_bookmarks p ON p.id = b.parent
      WHERE b.guid = OLD.mergedGuid;

      UPDATE moz_bookmarks SET
        parent = (SELECT id FROM moz_bookmarks
                  WHERE guid = OLD.mergedParentGuid),
        position = OLD.position,
        lastModified = OLD.lastModifiedMicroseconds
      WHERE guid = OLD.mergedGuid;
    END`);

  // Stages all items to delete locally and remotely. Items to delete locally
  // don't need tombstones: since we took the remote deletion, the tombstone
  // already exists on the server. Items to delete remotely, or non-syncable
  // items to delete on both sides, need tombstones.
  await db.execute(`CREATE TEMP TABLE itemsToRemove(
    guid TEXT PRIMARY KEY,
    localLevel INTEGER NOT NULL,
    shouldUploadTombstone BOOLEAN NOT NULL,
    dateRemovedMicroseconds INTEGER NOT NULL
  ) WITHOUT ROWID`);

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
                               tag, placeId, lastModifiedMicroseconds) AS
    SELECT b.id, b.guid, p.id, p.guid, b.position, b.type,
           p.title, b.fk, b.lastModified
    FROM moz_bookmarks b
    JOIN moz_bookmarks p ON p.id = b.parent
    WHERE b.type = ${PlacesUtils.bookmarks.TYPE_BOOKMARK} AND
          p.parent = (SELECT id FROM moz_bookmarks
                      WHERE guid = '${PlacesUtils.bookmarks.tagsGuid}')`);

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
             NEW.lastModifiedMicroseconds,
             NEW.lastModifiedMicroseconds);

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
              WHERE p.title = NEW.tag AND
                    p.parent = (SELECT id FROM moz_bookmarks
                                WHERE guid = '${
                                  PlacesUtils.bookmarks.tagsGuid
                                }')),
             ${PlacesUtils.bookmarks.TYPE_BOOKMARK}, NEW.placeId,
             NEW.lastModifiedMicroseconds,
             NEW.lastModifiedMicroseconds
      WHERE NEW.placeId NOT NULL;

      /* Record an item added notification for the tag entry. */
      INSERT INTO itemsAdded(guid, isTagging)
      SELECT b.guid, 1 FROM moz_bookmarks b
      JOIN moz_bookmarks p ON p.id = b.parent
      WHERE b.fk = NEW.placeId AND
            p.title = NEW.tag AND
            p.parent = (SELECT id FROM moz_bookmarks
                        WHERE guid = '${PlacesUtils.bookmarks.tagsGuid}');
    END`);

  // Stores properties to pass to `onItem{Added, Changed, Moved, Removed}`
  // bookmark observers for new, updated, moved, and deleted items.
  await db.execute(`CREATE TEMP TABLE itemsAdded(
    guid TEXT PRIMARY KEY,
    isTagging BOOLEAN NOT NULL DEFAULT 0,
    keywordChanged BOOLEAN NOT NULL DEFAULT 0,
    level INTEGER NOT NULL DEFAULT -1
  ) WITHOUT ROWID`);

  await db.execute(`CREATE INDEX addedItemLevels ON itemsAdded(level)`);

  await db.execute(`CREATE TEMP TABLE guidsChanged(
    itemId INTEGER PRIMARY KEY,
    oldGuid TEXT NOT NULL,
    level INTEGER NOT NULL DEFAULT -1
  )`);

  await db.execute(`CREATE INDEX changedGuidLevels ON guidsChanged(level)`);

  await db.execute(`CREATE TEMP TABLE itemsChanged(
    itemId INTEGER PRIMARY KEY,
    oldTitle TEXT,
    oldPlaceId INTEGER,
    keywordChanged BOOLEAN NOT NULL DEFAULT 0,
    level INTEGER NOT NULL DEFAULT -1
  )`);

  await db.execute(`CREATE INDEX changedItemLevels ON itemsChanged(level)`);

  await db.execute(`CREATE TEMP TABLE itemsMoved(
    itemId INTEGER PRIMARY KEY,
    oldParentId INTEGER NOT NULL,
    oldParentGuid TEXT NOT NULL,
    oldPosition INTEGER NOT NULL,
    level INTEGER NOT NULL DEFAULT -1
  )`);

  await db.execute(`CREATE INDEX movedItemLevels ON itemsMoved(level)`);

  await db.execute(`CREATE TEMP TABLE itemsRemoved(
    itemId INTEGER PRIMARY KEY,
    guid TEXT NOT NULL,
    parentId INTEGER NOT NULL,
    position INTEGER NOT NULL,
    type INTEGER NOT NULL,
    placeId INTEGER,
    parentGuid TEXT NOT NULL,
    /* We record the original level of the removed item in the tree so that we
       can notify children before parents. */
    level INTEGER NOT NULL DEFAULT -1,
    isUntagging BOOLEAN NOT NULL DEFAULT 0
  )`);

  await db.execute(
    `CREATE INDEX removedItemLevels ON itemsRemoved(level DESC)`
  );

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

  await db.execute(
    `CREATE INDEX parentsToUpload ON structureToUpload(parentId, position)`
  );

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
    record.dateAdded,
    serverModified
  );
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
 * @param  {Function} [recordTiming]
 *         An optional function with the signature `(time: Number)`, where
 *         `time` is the measured time.
 * @return The return value of the timed function.
 */
async function withTiming(name, func, recordTiming) {
  MirrorLog.debug(name);

  let startTime = Cu.now();
  let result = await func();
  let elapsedTime = Cu.now() - startTime;

  MirrorLog.debug(`${name} took ${elapsedTime.toFixed(3)}ms`);
  if (typeof recordTiming == "function") {
    recordTiming(elapsedTime, result);
  }

  return result;
}

/**
 * Fires bookmark and keyword observer notifications for all changes made during
 * the merge.
 */
class BookmarkObserverRecorder {
  constructor(db, { maxFrecenciesToRecalculate, notifyInStableOrder, signal }) {
    this.db = db;
    this.maxFrecenciesToRecalculate = maxFrecenciesToRecalculate;
    this.notifyInStableOrder = notifyInStableOrder;
    this.signal = signal;
    this.placesEvents = [];
    this.itemRemovedNotifications = [];
    this.guidChangedArgs = [];
    this.itemMovedArgs = [];
    this.itemChangedArgs = [];
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
    if (this.signal.aborted) {
      throw new SyncedBookmarksMirror.InterruptedError(
        "Interrupted before recalculating frecencies for new URLs"
      );
    }
    await updateFrecencies(this.db, this.maxFrecenciesToRecalculate);
  }

  orderBy(level, parent, position) {
    return `ORDER BY ${
      this.notifyInStableOrder ? `${level}, ${parent}, ${position}` : level
    }`;
  }

  /**
   * Records Places observer notifications for removed, added, moved, and
   * changed items.
   */
  async noteAllChanges() {
    MirrorLog.trace("Recording observer notifications for removed items");
    // `ORDER BY v.level DESC` sorts deleted children before parents, to ensure
    // that we update caches in the correct order (bug 1297941).
    await this.db.execute(
      `SELECT v.itemId AS id, v.parentId, v.parentGuid, v.position, v.type,
              (SELECT h.url FROM moz_places h WHERE h.id = v.placeId) AS url,
              v.guid, v.isUntagging
       FROM itemsRemoved v
       ${this.orderBy("v.level", "v.parentId", "v.position")}`,
      null,
      (row, cancel) => {
        if (this.signal.aborted) {
          cancel();
          return;
        }
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
    );
    if (this.signal.aborted) {
      throw new SyncedBookmarksMirror.InterruptedError(
        "Interrupted while recording observer notifications for removed items"
      );
    }

    MirrorLog.trace("Recording observer notifications for changed GUIDs");
    await this.db.execute(
      `SELECT b.id, b.lastModified, b.type, b.guid AS newGuid,
              c.oldGuid, p.id AS parentId, p.guid AS parentGuid
       FROM guidsChanged c
       JOIN moz_bookmarks b ON b.id = c.itemId
       JOIN moz_bookmarks p ON p.id = b.parent
       ${this.orderBy("c.level", "b.parent", "b.position")}`,
      null,
      (row, cancel) => {
        if (this.signal.aborted) {
          cancel();
          return;
        }
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
    );
    if (this.signal.aborted) {
      throw new SyncedBookmarksMirror.InterruptedError(
        "Interrupted while recording observer notifications for changed GUIDs"
      );
    }

    MirrorLog.trace("Recording observer notifications for new items");
    await this.db.execute(
      `SELECT b.id, p.id AS parentId, b.position, b.type,
              (SELECT h.url FROM moz_places h WHERE h.id = b.fk) AS url,
              IFNULL(b.title, '') AS title, b.dateAdded, b.guid,
              p.guid AS parentGuid, n.isTagging, n.keywordChanged
       FROM itemsAdded n
       JOIN moz_bookmarks b ON b.guid = n.guid
       JOIN moz_bookmarks p ON p.id = b.parent
       ${this.orderBy("n.level", "b.parent", "b.position")}`,
      null,
      (row, cancel) => {
        if (this.signal.aborted) {
          cancel();
          return;
        }
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
        if (row.getResultByName("keywordChanged")) {
          this.shouldInvalidateKeywords = true;
        }
      }
    );
    if (this.signal.aborted) {
      throw new SyncedBookmarksMirror.InterruptedError(
        "Interrupted while recording observer notifications for new items"
      );
    }

    MirrorLog.trace("Recording observer notifications for moved items");
    await this.db.execute(
      `SELECT b.id, b.guid, b.type, p.id AS newParentId, c.oldParentId,
              p.guid AS newParentGuid, c.oldParentGuid,
              b.position AS newPosition, c.oldPosition,
              (SELECT h.url FROM moz_places h WHERE h.id = b.fk) AS url
       FROM itemsMoved c
       JOIN moz_bookmarks b ON b.id = c.itemId
       JOIN moz_bookmarks p ON p.id = b.parent
       ${this.orderBy("c.level", "b.parent", "b.position")}`,
      null,
      (row, cancel) => {
        if (this.signal.aborted) {
          cancel();
          return;
        }
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
    );
    if (this.signal.aborted) {
      throw new SyncedBookmarksMirror.InterruptedError(
        "Interrupted while recording observer notifications for moved items"
      );
    }

    MirrorLog.trace("Recording observer notifications for changed items");
    await this.db.execute(
      `SELECT b.id, b.guid, b.lastModified, b.type,
              IFNULL(b.title, '') AS newTitle,
              IFNULL(c.oldTitle, '') AS oldTitle,
              (SELECT h.url FROM moz_places h
               WHERE h.id = b.fk) AS newURL,
              (SELECT h.url FROM moz_places h
               WHERE h.id = c.oldPlaceId) AS oldURL,
              p.id AS parentId, p.guid AS parentGuid,
              c.keywordChanged
       FROM itemsChanged c
       JOIN moz_bookmarks b ON b.id = c.itemId
       JOIN moz_bookmarks p ON p.id = b.parent
       ${this.orderBy("c.level", "b.parent", "b.position")}`,
      null,
      (row, cancel) => {
        if (this.signal.aborted) {
          cancel();
          return;
        }
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
        if (row.getResultByName("keywordChanged")) {
          this.shouldInvalidateKeywords = true;
        }
      }
    );
    if (this.signal.aborted) {
      throw new SyncedBookmarksMirror.InterruptedError(
        "Interrupted while recording observer notifications for changed items"
      );
    }
  }

  noteItemAdded(info) {
    this.placesEvents.push(
      new PlacesBookmarkAddition({
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
      })
    );
  }

  noteGuidChanged(info) {
    PlacesUtils.invalidateCachedGuidFor(info.id);
    this.guidChangedArgs.push([
      info.id,
      "guid",
      /* isAnnotationProperty */ false,
      info.newGuid,
      info.lastModified,
      info.type,
      info.parentId,
      info.newGuid,
      info.parentGuid,
      info.oldGuid,
      PlacesUtils.bookmarks.SOURCES.SYNC,
    ]);
  }

  noteItemMoved(info) {
    this.itemMovedArgs.push([
      info.id,
      info.oldParentId,
      info.oldPosition,
      info.newParentId,
      info.newPosition,
      info.type,
      info.guid,
      info.oldParentGuid,
      info.newParentGuid,
      PlacesUtils.bookmarks.SOURCES.SYNC,
      info.urlHref,
    ]);
  }

  noteItemChanged(info) {
    if (info.oldTitle != info.newTitle) {
      this.itemChangedArgs.push([
        info.id,
        "title",
        /* isAnnotationProperty */ false,
        info.newTitle,
        info.lastModified,
        info.type,
        info.parentId,
        info.guid,
        info.parentGuid,
        info.oldTitle,
        PlacesUtils.bookmarks.SOURCES.SYNC,
      ]);
    }
    if (info.oldURLHref != info.newURLHref) {
      this.itemChangedArgs.push([
        info.id,
        "uri",
        /* isAnnotationProperty */ false,
        info.newURLHref,
        info.lastModified,
        info.type,
        info.parentId,
        info.guid,
        info.parentGuid,
        info.oldURLHref,
        PlacesUtils.bookmarks.SOURCES.SYNC,
      ]);
    }
  }

  noteItemRemoved(info) {
    let uri = info.urlHref ? Services.io.newURI(info.urlHref) : null;
    this.itemRemovedNotifications.push({
      isTagging: info.isUntagging,
      args: [
        info.id,
        info.parentId,
        info.position,
        info.type,
        uri,
        info.guid,
        info.parentGuid,
        PlacesUtils.bookmarks.SOURCES.SYNC,
      ],
    });
  }

  async notifyBookmarkObservers() {
    MirrorLog.trace("Notifying bookmark observers");
    let observers = PlacesUtils.bookmarks.getObservers();
    for (let observer of observers) {
      this.notifyObserver(observer, "onBeginUpdateBatch");
    }
    await Async.yieldingForEach(
      this.itemRemovedNotifications,
      info => {
        if (this.signal.aborted) {
          throw new SyncedBookmarksMirror.InterruptedError(
            "Interrupted while notifying observers for removed items"
          );
        }
        this.notifyObserversWithInfo(observers, "onItemRemoved", info);
      },
      yieldState
    );
    await Async.yieldingForEach(
      this.guidChangedArgs,
      args => {
        if (this.signal.aborted) {
          throw new SyncedBookmarksMirror.InterruptedError(
            "Interrupted while notifying observers for changed GUIDs"
          );
        }
        this.notifyObserversWithInfo(observers, "onItemChanged", {
          isTagging: false,
          args,
        });
      },
      yieldState
    );
    if (this.signal.aborted) {
      throw new SyncedBookmarksMirror.InterruptedError(
        "Interrupted before notifying observers for new items"
      );
    }
    PlacesObservers.notifyListeners(this.placesEvents);
    await Async.yieldingForEach(
      this.itemMovedArgs,
      args => {
        if (this.signal.aborted) {
          throw new SyncedBookmarksMirror.InterruptedError(
            "Interrupted before notifying observers for moved items"
          );
        }
        this.notifyObserversWithInfo(observers, "onItemMoved", {
          isTagging: false,
          args,
        });
      },
      yieldState
    );
    await Async.yieldingForEach(
      this.itemChangedArgs,
      args => {
        if (this.signal.aborted) {
          throw new SyncedBookmarksMirror.InterruptedError(
            "Interrupted before notifying observers for changed items"
          );
        }
        this.notifyObserversWithInfo(observers, "onItemChanged", {
          isTagging: false,
          args,
        });
      },
      yieldState
    );
    for (let observer of observers) {
      this.notifyObserver(observer, "onEndUpdateBatch");
    }
  }

  notifyObserversWithInfo(observers, name, info) {
    for (let observer of observers) {
      if (info.isTagging && observer.skipTags) {
        return;
      }
      this.notifyObserver(observer, name, info.args);
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

async function updateFrecencies(db, limit) {
  MirrorLog.trace("Recalculating frecencies for new URLs");
  await db.execute(
    `
    UPDATE moz_places SET
      frecency = CALCULATE_FRECENCY(id)
    WHERE id IN (
      SELECT id FROM moz_places
      WHERE frecency < 0
      ORDER BY frecency ASC
      LIMIT :limit
    )`,
    { limit }
  );
}

function bagToNamedCounts(bag, names) {
  let counts = [];
  for (let name of names) {
    let count = bag.getProperty(name);
    if (count > 0) {
      counts.push({ name, count });
    }
  }
  return counts;
}

/**
 * Returns an `AbortSignal` that aborts if either `finalizeSignal` or
 * `interruptSignal` aborts. This is like `Promise.race`, but for
 * cancellations.
 *
 * @param  {AbortSignal} finalizeSignal
 * @param  {AbortSignal?} signal
 * @return {AbortSignal}
 */
function anyAborted(finalizeSignal, interruptSignal = null) {
  if (finalizeSignal.aborted || !interruptSignal) {
    // If the mirror was already finalized, or we don't have an interrupt
    // signal for this merge, just use the finalize signal.
    return finalizeSignal;
  }
  if (interruptSignal.aborted) {
    // If the merge was interrupted, return its already-aborted signal.
    return interruptSignal;
  }
  // Otherwise, we return a new signal that aborts if either the mirror is
  // finalized, or the merge is interrupted, whichever happens first.
  let controller = new AbortController();
  function onAbort() {
    finalizeSignal.removeEventListener("abort", onAbort);
    interruptSignal.removeEventListener("abort", onAbort);
    controller.abort();
  }
  finalizeSignal.addEventListener("abort", onAbort);
  interruptSignal.addEventListener("abort", onAbort);
  return controller.signal;
}

// In conclusion, this is why bookmark syncing is hard.
