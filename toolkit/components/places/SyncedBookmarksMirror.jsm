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

Cu.importGlobalProperties(["URL"]);

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Async: "resource://services-common/async.js",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  Log: "resource://gre/modules/Log.jsm",
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PlacesSyncUtils: "resource://gre/modules/PlacesSyncUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Sqlite: "resource://gre/modules/Sqlite.jsm",
});

XPCOMUtils.defineLazyGetter(this, "MirrorLog", () =>
  Log.repository.getLogger("Sync.Engine.Bookmarks.Mirror")
);

XPCOMUtils.defineLazyGetter(this, "UserContentRootsAsSqlList", () =>
  PlacesUtils.bookmarks.userContentRoots.map(v => `'${v}'`).join(",")
);

// These can be removed once they're exposed in a central location (bug
// 1375896).
const DB_URL_LENGTH_MAX = 65536;
const DB_TITLE_LENGTH_MAX = 4096;
const DB_DESCRIPTION_LENGTH_MAX = 256;

const SQLITE_MAX_VARIABLE_NUMBER = 999;

// The current mirror database schema version. Bump for migrations, then add
// migration code to `migrateMirrorSchema`.
const MIRROR_SCHEMA_VERSION = 2;

// Use a shared jankYielder in these functions
XPCOMUtils.defineLazyGetter(this, "maybeYield", () => Async.jankYielder());
function yieldingIterator(collection) {
  return Async.yieldingIterator(collection, maybeYield);
}

/**
 * A mirror maintains a copy of the complete tree as stored on the Sync server.
 * It is persistent.
 *
 * The mirror schema is a hybrid of how Sync and Places represent bookmarks.
 * The `items` table contains item attributes (title, kind, description,
 * URL, etc.), while the `structure` table stores parent-child relationships and
 * position. This is similar to how iOS encodes "value" and "structure" state,
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
   *         The full path to the mirror database file.
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
    let whyFailed = "initialize";
    try {
      await db.execute(`PRAGMA foreign_keys = ON`);
      try {
        await attachAndInitMirrorDatabase(db, options.path);
      } catch (ex) {
        if (isDatabaseCorrupt(ex)) {
          MirrorLog.warn("Error attaching mirror to Places; removing and " +
                         "recreating mirror", ex);
          options.recordTelemetryEvent("mirror", "open", "retry",
                                       { why: "corrupt" });

          whyFailed = "remove";
          await OS.File.remove(options.path);

          whyFailed = "replace";
          await attachAndInitMirrorDatabase(db, options.path);
        } else {
          MirrorLog.warn("Unrecoverable error attaching mirror to Places", ex);
          throw ex;
        }
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
   *       `true` for incoming records, and `false` for successfully uploaded
   *        records. Tests can also pass `false` to set up an existing mirror.
   */
  async store(records, { needsMerge = true } = {}) {
    let options = { needsMerge };
    let ignoreCounts = {
      bookmark: { id: 0, url: 0 },
      query: { id: 0, url: 0 },
      folder: { id: 0, root: 0 },
      child: { id: 0, },
      livemark: { id: 0, feed: 0 },
      separator: { id: 0 },
      tombstone: { id: 0, root: 0 },
    };
    let extraTelemetryEvents = [];
    try {
      await this.db.executeBeforeShutdown(
        "SyncedBookmarksMirror: store",
        db => db.executeTransaction(async () => {
          for await (let record of yieldingIterator(records)) {
            MirrorLog.trace(`Storing in mirror: ${record.cleartextToString()}`);
            switch (record.type) {
              case "bookmark":
                await this.storeRemoteBookmark(record, ignoreCounts, options);
                continue;

              case "query":
                await this.storeRemoteQuery(record, ignoreCounts, options);
                continue;

              case "folder":
                await this.storeRemoteFolder(record, ignoreCounts, options);
                continue;

              case "livemark":
                await this.storeRemoteLivemark(record, ignoreCounts, options);
                continue;

              case "separator":
                await this.storeRemoteSeparator(record, ignoreCounts, options);
                continue;

              default:
                if (record.deleted) {
                  await this.storeRemoteTombstone(record, ignoreCounts,
                                                  options);
                  continue;
                }
            }
            MirrorLog.warn("Ignoring record with unknown type", record.type);
            extraTelemetryEvents.push({
              method: "ignore",
              value: "unknown-kind",
              extra: { kind: record.type },
            });
          }
        }
      ));
    } finally {
      for (let { method, value, extra } of extraTelemetryEvents) {
        this.recordTelemetryEvent("mirror", method, value, extra);
      }
      for (let kind in ignoreCounts) {
        let reasons = ignoreCounts[kind];
        let extra = {};
        for (let reason in reasons) {
          let count = reasons[reason];
          if (count > 0) {
            extra[reason] = String(count);
          }
        }
        if (!ObjectUtils.isEmpty(extra)) {
          this.recordTelemetryEvent("mirror", "ignore", kind, extra);
        }
      }
    }
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
   * @return {Object.<String, BookmarkChangeRecord>}
   *         A changeset containing locally changed and reconciled records to
   *         upload to the server, and to store in the mirror once upload
   *         succeeds.
   */
  async apply({ localTimeSeconds = Date.now() / 1000,
                remoteTimeSeconds = 0,
                weakUpload = [] } = {}) {
    let hasChanges = weakUpload.length > 0 || (await this.hasChanges());
    if (!hasChanges) {
      MirrorLog.debug("No changes detected in both mirror and Places");
      return {};
    }
    // We intentionally don't use `executeBeforeShutdown` in this function,
    // since merging can take a while for large trees, and we don't want to
    // block shutdown. Since all new items are in the mirror, we'll just try
    // to merge again on the next sync.
    let { missingParents, missingChildren } = await this.fetchRemoteOrphans();
    if (missingParents.length) {
      MirrorLog.warn("Temporarily reparenting remote items with missing " +
                     "parents to unfiled", missingParents);
      this.recordTelemetryEvent("mirror", "orphans", "parents",
                                { count: String(missingParents.length) });
    }
    if (missingChildren.length) {
      MirrorLog.warn("Remote tree missing items", missingChildren);
      this.recordTelemetryEvent("mirror", "orphans", "children",
                                { count: String(missingChildren.length) });
    }

    let { missingLocal, missingRemote, wrongSyncStatus } =
      await this.fetchInconsistencies();
    if (missingLocal.length) {
      MirrorLog.warn("Remote tree has merged items that don't exist locally",
                     missingLocal);
      this.recordTelemetryEvent("mirror", "inconsistencies", "local",
                                { count: String(missingLocal.length) });
    }
    if (missingRemote.length) {
      MirrorLog.warn("Local tree has synced items that don't exist remotely",
                     missingRemote);
      this.recordTelemetryEvent("mirror", "inconsistencies", "remote",
                                { count: String(missingRemote.length) });
    }
    if (wrongSyncStatus.length) {
      MirrorLog.warn("Local tree has wrong sync statuses for items that " +
                     "exist remotely", wrongSyncStatus);
      this.recordTelemetryEvent("mirror", "inconsistencies", "syncStatus",
                                { count: String(wrongSyncStatus.length) });
    }

    // It's safe to build the remote tree outside the transaction because
    // `fetchRemoteTree` doesn't join to Places, only Sync writes to the
    // mirror, and we're holding the Sync lock at this point.
    MirrorLog.debug("Building remote tree from mirror");
    let remoteTree = await this.fetchRemoteTree(remoteTimeSeconds);
    if (MirrorLog.level <= Log.Level.Trace) {
      MirrorLog.trace("Built remote tree from mirror\n" +
                      remoteTree.toASCIITreeString());
    }

    let observersToNotify = new BookmarkObserverRecorder(this.db);

    let changeRecords = await this.db.executeTransaction(async () => {
      MirrorLog.debug("Building local tree from Places");
      let localTree = await this.fetchLocalTree(localTimeSeconds);
      if (MirrorLog.level <= Log.Level.Trace) {
        MirrorLog.trace("Built local tree from Places\n" +
                        localTree.toASCIITreeString());
      }

      MirrorLog.debug("Fetching content info for new mirror items");
      let newRemoteContents = await this.fetchNewRemoteContents();

      MirrorLog.debug("Fetching content info for new Places items");
      let newLocalContents = await this.fetchNewLocalContents();

      MirrorLog.debug("Building complete merged tree");
      let merger = new BookmarkMerger(localTree, newLocalContents,
                                      remoteTree, newRemoteContents);
      let mergedRoot;
      try {
        mergedRoot = await merger.merge();
      } finally {
        for (let { value, extra } of merger.summarizeTelemetryEvents()) {
          this.recordTelemetryEvent("mirror", "merge", value, extra);
        }
      }

      if (MirrorLog.level <= Log.Level.Trace) {
        MirrorLog.trace([
          "Built new merged tree",
          mergedRoot.toASCIITreeString(),
          ...merger.deletionsToStrings(),
        ].join("\n"));
      }

      // The merged tree should know about all items mentioned in the local
      // and remote trees. Otherwise, it's incomplete, and we'll corrupt
      // Places or lose data on the server if we try to apply it.
      if (!await merger.subsumes(localTree)) {
        throw new SyncedBookmarksMirror.ConsistencyError(
          "Merged tree doesn't mention all items from local tree");
      }
      if (!await merger.subsumes(remoteTree)) {
        throw new SyncedBookmarksMirror.ConsistencyError(
          "Merged tree doesn't mention all items from remote tree");
      }

      MirrorLog.debug("Applying merged tree");
      let localDeletions = Array.from(merger.deleteLocally).map(guid =>
        ({ guid, level: localTree.levelForGuid(guid) })
      );
      let remoteDeletions = Array.from(merger.deleteRemotely);
      await this.updateLocalItemsInPlaces(mergedRoot, localDeletions,
                                          remoteDeletions);

      // At this point, the database is consistent, and we can fetch info to
      // pass to observers. Note that we can't fetch observer info in the
      // triggers above, because the structure might not be complete yet. An
      // incomplete structure might cause us to miss or record wrong parents and
      // positions.

      MirrorLog.debug("Recording observer notifications");
      await this.noteObserverChanges(observersToNotify);

      MirrorLog.debug("Staging locally changed items for upload");
      await this.stageItemsToUpload(weakUpload);

      MirrorLog.debug("Fetching records for local items to upload");
      let changeRecords = await this.fetchLocalChangeRecords();

      await this.db.execute(`DELETE FROM mergeStates`);
      await this.db.execute(`DELETE FROM itemsAdded`);
      await this.db.execute(`DELETE FROM guidsChanged`);
      await this.db.execute(`DELETE FROM itemsChanged`);
      await this.db.execute(`DELETE FROM itemsRemoved`);
      await this.db.execute(`DELETE FROM itemsMoved`);
      await this.db.execute(`DELETE FROM annosChanged`);
      await this.db.execute(`DELETE FROM itemsToWeaklyReupload`);
      await this.db.execute(`DELETE FROM itemsToUpload`);

      return changeRecords;
    });

    MirrorLog.debug("Replaying recorded observer notifications");
    try {
      await observersToNotify.notifyAll();
    } catch (ex) {
      MirrorLog.error("Error notifying Places observers", ex);
    }

    return changeRecords;
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

  async storeRemoteBookmark(record, ignoreCounts, { needsMerge }) {
    let guid = validateGuid(record.id);
    if (!guid) {
      MirrorLog.warn("Ignoring bookmark with invalid ID", record.id);
      ignoreCounts.bookmark.id++;
      return;
    }

    let url = validateURL(record.bmkUri);
    if (!url) {
      MirrorLog.warn("Ignoring bookmark ${guid} with invalid URL ${url}",
                     { guid, url: record.bmkUri });
      ignoreCounts.bookmark.url++;
      return;
    }

    await this.maybeStoreRemoteURL(url);

    let serverModified = determineServerModified(record);
    let dateAdded = determineDateAdded(record);
    let title = validateTitle(record.title);
    let keyword = validateKeyword(record.keyword);
    let description = validateDescription(record.description);
    let loadInSidebar = record.loadInSidebar === true ? "1" : null;

    await this.db.executeCached(`
      REPLACE INTO items(guid, serverModified, needsMerge, kind,
                         dateAdded, title, keyword,
                         urlId, description, loadInSidebar)
      VALUES(:guid, :serverModified, :needsMerge, :kind,
             :dateAdded, NULLIF(:title, ""), :keyword,
             (SELECT id FROM urls
              WHERE hash = hash(:url) AND
                    url = :url),
             :description, :loadInSidebar)`,
      { guid, serverModified, needsMerge,
        kind: SyncedBookmarksMirror.KIND.BOOKMARK, dateAdded, title, keyword,
        url: url.href, description, loadInSidebar });

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

  async storeRemoteQuery(record, ignoreCounts, { needsMerge }) {
    let guid = validateGuid(record.id);
    if (!guid) {
      MirrorLog.warn("Ignoring query with invalid ID", record.id);
      ignoreCounts.query.id++;
      return;
    }

    let url = validateURL(record.bmkUri);
    if (!url) {
      MirrorLog.warn("Ignoring query ${guid} with invalid URL ${url}",
                     { guid, url: record.bmkUri });
      ignoreCounts.query.url++;
      return;
    }

    // Legacy tag queries may use a `place:` URL with a `folder` param that
    // points to the tag folder ID. We need to rewrite these queries to
    // directly reference the tag.
    let params = new URLSearchParams(url.pathname);
    let type = +params.get("type");
    if (type == Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_CONTENTS) {
      let tagFolderName = validateTag(record.folderName);
      if (!tagFolderName) {
        MirrorLog.warn("Ignoring tag query ${guid} with invalid tag name " +
                       "${tagFolderName}", { guid, tagFolderName });
        ignoreCounts.query.url++;
        return;
      }
      url = new URL(`place:tag=${tagFolderName}`);
    }

    await this.maybeStoreRemoteURL(url);

    let serverModified = determineServerModified(record);
    let dateAdded = determineDateAdded(record);
    let title = validateTitle(record.title);
    let description = validateDescription(record.description);
    let smartBookmarkName = typeof record.queryId == "string" ?
                            record.queryId : null;

    await this.db.executeCached(`
      REPLACE INTO items(guid, serverModified, needsMerge, kind,
                         dateAdded, title, urlId, description,
                         smartBookmarkName)
      VALUES(:guid, :serverModified, :needsMerge, :kind,
             :dateAdded, NULLIF(:title, ""),
             (SELECT id FROM urls
              WHERE hash = hash(:url) AND
                    url = :url),
             :description, :smartBookmarkName)`,
      { guid, serverModified, needsMerge,
        kind: SyncedBookmarksMirror.KIND.QUERY, dateAdded, title,
        url: url.href, description, smartBookmarkName });
  }

  async storeRemoteFolder(record, ignoreCounts, { needsMerge }) {
    let guid = validateGuid(record.id);
    if (!guid) {
      MirrorLog.warn("Ignoring folder with invalid ID", record.id);
      ignoreCounts.folder.id++;
      return;
    }
    if (guid == PlacesUtils.bookmarks.rootGuid) {
      // The Places root shouldn't be synced at all.
      MirrorLog.warn("Ignoring Places root record", record);
      ignoreCounts.folder.root++;
    }

    let serverModified = determineServerModified(record);
    let dateAdded = determineDateAdded(record);
    let title = validateTitle(record.title);
    let description = validateDescription(record.description);

    await this.db.executeCached(`
      REPLACE INTO items(guid, serverModified, needsMerge, kind,
                         dateAdded, title, description)
      VALUES(:guid, :serverModified, :needsMerge, :kind,
             :dateAdded, NULLIF(:title, ""),
             :description)`,
      { guid, serverModified, needsMerge, kind: SyncedBookmarksMirror.KIND.FOLDER,
        dateAdded, title, description });

    let children = record.children;
    if (children && Array.isArray(children)) {
      for (let position = 0; position < children.length; ++position) {
        await maybeYield();
        let childRecordId = children[position];
        let childGuid = validateGuid(childRecordId);
        if (!childGuid) {
          MirrorLog.warn("Ignoring child of folder ${parentGuid} with " +
                         "invalid ID ${childRecordId}", { parentGuid: guid,
                                                          childRecordId });
          ignoreCounts.child.id++;
          continue;
        }
        await this.db.executeCached(`
          REPLACE INTO structure(guid, parentGuid, position)
          VALUES(:childGuid, :parentGuid, :position)`,
          { childGuid, parentGuid: guid, position });
      }
    }
    // parentChild relationships are usually determined via the children list
    // in the parent. However, this doesn't work for an item that is directly
    // under the root - such items are invalid, but we still want to store
    // them so we can ignore that entire sub-tree - so such items need special
    // treatment.
    let parentGuid = validateGuid(record.parentid);
    if (parentGuid == PlacesUtils.bookmarks.rootGuid) {
        await this.db.executeCached(`
          INSERT OR IGNORE INTO structure(guid, parentGuid, position)
          VALUES(:guid, :parentGuid, -1)`,
          { guid, parentGuid });
      }
  }

  async storeRemoteLivemark(record, ignoreCounts, { needsMerge }) {
    let guid = validateGuid(record.id);
    if (!guid) {
      MirrorLog.warn("Ignoring livemark with invalid ID", record.id);
      ignoreCounts.livemark.id++;
      return;
    }

    let feedURL = validateURL(record.feedUri);
    if (!feedURL) {
      MirrorLog.warn("Ignoring livemark ${guid} with invalid feed URL ${url}",
                     { guid, url: record.feedUri });
      ignoreCounts.livemark.feed++;
      return;
    }

    let serverModified = determineServerModified(record);
    let dateAdded = determineDateAdded(record);
    let title = validateTitle(record.title);
    let description = validateDescription(record.description);
    let siteURL = validateURL(record.siteUri);

    await this.db.executeCached(`
      REPLACE INTO items(guid, serverModified, needsMerge, kind, dateAdded,
                         title, description, feedURL, siteURL)
      VALUES(:guid, :serverModified, :needsMerge, :kind, :dateAdded,
             NULLIF(:title, ""), :description, :feedURL, :siteURL)`,
      { guid, serverModified, needsMerge,
        kind: SyncedBookmarksMirror.KIND.LIVEMARK,
        dateAdded, title, description, feedURL: feedURL.href,
        siteURL: siteURL ? siteURL.href : null });
  }

  async storeRemoteSeparator(record, ignoreCounts, { needsMerge }) {
    let guid = validateGuid(record.id);
    if (!guid) {
      MirrorLog.warn("Ignoring separator with invalid ID", record.id);
      ignoreCounts.separator.id++;
      return;
    }

    let serverModified = determineServerModified(record);
    let dateAdded = determineDateAdded(record);

    await this.db.executeCached(`
      REPLACE INTO items(guid, serverModified, needsMerge, kind,
                         dateAdded)
      VALUES(:guid, :serverModified, :needsMerge, :kind,
             :dateAdded)`,
      { guid, serverModified, needsMerge, kind: SyncedBookmarksMirror.KIND.SEPARATOR,
        dateAdded });
  }

  async storeRemoteTombstone(record, ignoreCounts, { needsMerge }) {
    let guid = validateGuid(record.id);
    if (!guid) {
      MirrorLog.warn("Ignoring tombstone with invalid ID", record.id);
      ignoreCounts.tombstone.id++;
      return;
    }

    if (PlacesUtils.bookmarks.userContentRoots.includes(guid)) {
      MirrorLog.warn("Ignoring tombstone for syncable root", guid);
      ignoreCounts.tombstone.root++;
      return;
    }

    await this.db.executeCached(`
      REPLACE INTO items(guid, serverModified, needsMerge, isDeleted)
      VALUES(:guid, :serverModified, :needsMerge, 1)`,
      { guid, serverModified: determineServerModified(record), needsMerge });
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
    };

    let orphanRows = await this.db.execute(`
      SELECT v.guid AS guid, 1 AS missingParent, 0 AS missingChild
      FROM items v
      LEFT JOIN structure s ON s.guid = v.guid
      WHERE NOT v.isDeleted AND
            s.guid IS NULL
      UNION ALL
      SELECT s.guid AS guid, 0 AS missingParent, 1 AS missingChild
      FROM structure s
      LEFT JOIN items v ON v.guid = s.guid
      WHERE v.guid IS NULL`);

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
  async fetchInconsistencies() {
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
       syncedItems(id, syncChangeCounter) AS (
         SELECT b.id, b.syncChangeCounter FROM moz_bookmarks b
         WHERE b.guid IN (${UserContentRootsAsSqlList})
         UNION ALL
         SELECT b.id, b.syncChangeCounter FROM moz_bookmarks b
         JOIN syncedItems s ON b.parent = s.id
       )
       SELECT 1
       FROM syncedItems
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
   * Builds a fully rooted, consistent tree from the items and tombstones in the
   * mirror.
   *
   * @param  {Number} remoteTimeSeconds
   *         The current server time, in seconds.
   * @return {BookmarkTree}
   *         The remote bookmark tree.
   */
  async fetchRemoteTree(remoteTimeSeconds) {
    let remoteTree = new BookmarkTree(BookmarkNode.root());
    let startTime = Cu.now();

    // First, build a flat mapping of parents to children. The `LEFT JOIN`
    // includes items orphaned by an interrupted upload on another device.
    // We keep the orphans in "unfiled" until the other device returns and
    // uploads the missing parent.
    // Note that we avoid returning orphaned queries here - there's a really
    // good chance that orphaned queries are actually left-pane queries with
    // the left-pane root missing.
    // In some bizarre edge-cases this might mean some legitimate queries are
    // lost for the user, but this should happen far less often than us finding
    // illegitimate left-pane queries - and if the actual parents ever do
    // appear, they will spring back into life.
    let itemRows = await this.db.execute(`
      SELECT v.guid, IFNULL(s.parentGuid, :unfiledGuid) AS parentGuid,
             IFNULL(s.position, -1) AS position, v.serverModified, v.kind,
             v.needsMerge
      FROM items v
      LEFT JOIN structure s ON s.guid = v.guid
      WHERE NOT v.isDeleted AND
            v.guid <> :rootGuid AND
            (s.parentGuid IS NOT NULL OR v.kind <> :queryKind)
      ORDER BY parentGuid, position = -1, position, v.guid`,
      { rootGuid: PlacesUtils.bookmarks.rootGuid,
        unfiledGuid: PlacesUtils.bookmarks.unfiledGuid,
        queryKind: SyncedBookmarksMirror.KIND.QUERY });

    let pseudoTree = new Map();
    for await (let row of yieldingIterator(itemRows)) {
      let parentGuid = row.getResultByName("parentGuid");
      let node = BookmarkNode.fromRemoteRow(row, remoteTimeSeconds);
      if (pseudoTree.has(parentGuid)) {
        let nodes = pseudoTree.get(parentGuid);
        nodes.push(node);
      } else {
        pseudoTree.set(parentGuid, [node]);
      }
    }

    // Second, build a complete tree from the pseudo-tree. We could do these
    // two steps in SQL, but it's extremely inefficient. An equivalent
    // recursive query, with joins in the base and recursive clauses, takes
    // 10 seconds for a mirror with 5k items. Building the pseudo-tree and
    // the pseudo-tree and recursing in JS takes 30ms for 5k items.
    // (Note: Timing was done before adding maybeYield calls)
    await inflateTree(remoteTree, pseudoTree, PlacesUtils.bookmarks.rootGuid);

    // Note tombstones for remotely deleted items.
    let tombstoneRows = await this.db.execute(`
      SELECT guid FROM items
      WHERE isDeleted AND
            needsMerge`);

    for await (let row of yieldingIterator(tombstoneRows)) {
      let guid = row.getResultByName("guid");
      remoteTree.noteDeleted(guid);
    }

    let elapsedTime = Cu.now() - startTime;
    let totalRows = itemRows.length + tombstoneRows.length;
    this.recordTelemetryEvent("mirror", "fetch", "remoteTree",
                              { time: String(elapsedTime),
                                count: String(totalRows) });

    return remoteTree;
  }

  /**
   * Fetches content info for all items in the mirror that changed since the
   * last sync and don't exist locally.
   *
   * @return {Map.<String, BookmarkContent>}
   *         Changed items in the mirror that don't exist in Places, keyed by
   *         their GUIDs.
   */
  async fetchNewRemoteContents() {
    let newRemoteContents = new Map();
    let startTime = Cu.now();

    let rows = await this.db.execute(`
      SELECT v.guid, IFNULL(v.title, "") AS title, u.url, v.smartBookmarkName,
             IFNULL(s.position, -1) AS position
      FROM items v
      LEFT JOIN urls u ON u.id = v.urlId
      LEFT JOIN structure s ON s.guid = v.guid
      LEFT JOIN moz_bookmarks b ON b.guid = v.guid
      WHERE NOT v.isDeleted AND
            v.needsMerge AND
            b.guid IS NULL AND
            IFNULL(s.parentGuid, :unfiledGuid) <> :rootGuid`,
      { unfiledGuid: PlacesUtils.bookmarks.unfiledGuid,
        rootGuid: PlacesUtils.bookmarks.rootGuid });

    for await (let row of yieldingIterator(rows)) {
      let guid = row.getResultByName("guid");
      let content = BookmarkContent.fromRow(row);
      newRemoteContents.set(guid, content);
    }

    let elapsedTime = Cu.now() - startTime;
    this.recordTelemetryEvent("mirror", "fetch", "newRemoteContents",
                              { time: String(elapsedTime),
                                count: String(rows.length) });

    return newRemoteContents;
  }

  /**
   * Builds a fully rooted, consistent tree from the items and tombstones in
   * Places.
   *
   * @param  {Number} localTimeSeconds
   *         The current local time, in seconds.
   * @return {BookmarkTree}
   *         The local bookmark tree.
   */
  async fetchLocalTree(localTimeSeconds) {
    let localTree = new BookmarkTree(BookmarkNode.root());
    let startTime = Cu.now();

    // This unsightly query collects all descendants and maps their Places types
    // to the Sync record kinds. We start with the roots, and work our way down.
    // The list of roots in `syncedItems` should be updated if we add new
    // syncable roots to Places.
    let itemRows = await this.db.execute(`
      WITH RECURSIVE
      syncedItems(id, level) AS (
        SELECT b.id, 0 AS level FROM moz_bookmarks b
        WHERE b.guid IN (${UserContentRootsAsSqlList})
        UNION ALL
        SELECT b.id, s.level + 1 AS level FROM moz_bookmarks b
        JOIN syncedItems s ON s.id = b.parent
      )
      SELECT b.id, b.guid, p.guid AS parentGuid,
             /* Map Places item types to Sync record kinds. */
             (CASE b.type
                WHEN :bookmarkType THEN (
                  CASE SUBSTR((SELECT h.url FROM moz_places h
                               WHERE h.id = b.fk), 1, 6)
                  /* Queries are bookmarks with a "place:" URL scheme. */
                  WHEN 'place:' THEN :queryKind
                  ELSE :bookmarkKind END)
                WHEN :folderType THEN (
                  CASE WHEN EXISTS(
                    /* Livemarks are folders with a feed URL annotation. */
                    SELECT 1 FROM moz_items_annos a
                    JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
                    WHERE a.item_id = b.id AND
                          n.name = :feedURLAnno
                  ) THEN :livemarkKind
                  ELSE :folderKind END)
                ELSE :separatorKind END) AS kind,
             b.lastModified / 1000 AS localModified, b.syncChangeCounter
      FROM moz_bookmarks b
      JOIN moz_bookmarks p ON p.id = b.parent
      JOIN syncedItems s ON s.id = b.id
      ORDER BY s.level, b.parent, b.position`,
      { bookmarkType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        queryKind: SyncedBookmarksMirror.KIND.QUERY,
        bookmarkKind: SyncedBookmarksMirror.KIND.BOOKMARK,
        folderType: PlacesUtils.bookmarks.TYPE_FOLDER,
        feedURLAnno: PlacesUtils.LMANNO_FEEDURI,
        livemarkKind: SyncedBookmarksMirror.KIND.LIVEMARK,
        folderKind: SyncedBookmarksMirror.KIND.FOLDER,
        separatorKind: SyncedBookmarksMirror.KIND.SEPARATOR });

    for await (let row of yieldingIterator(itemRows)) {
      let parentGuid = row.getResultByName("parentGuid");
      let node = BookmarkNode.fromLocalRow(row, localTimeSeconds);
      localTree.insert(parentGuid, node);
    }

    // Note tombstones for locally deleted items.
    let tombstoneRows = await this.db.execute(`
      SELECT guid FROM moz_bookmarks_deleted`);

    for await (let row of yieldingIterator(tombstoneRows)) {
      let guid = row.getResultByName("guid");
      localTree.noteDeleted(guid);
    }

    let elapsedTime = Cu.now() - startTime;
    let totalRows = itemRows.length + tombstoneRows.length;
    this.recordTelemetryEvent("mirror", "fetch", "localTree",
                              { time: String(elapsedTime),
                                count: String(totalRows) });

    return localTree;
  }

  /**
   * Fetches content info for all NEW and UNKNOWN local items that don't exist
   * in the mirror. We'll try to dedupe them to changed items with similar
   * contents and different GUIDs in the mirror.
   *
   * @return {Map.<String, BookmarkContent>}
   *         New items in Places that don't exist in the mirror, keyed by their
   *         GUIDs.
   */
  async fetchNewLocalContents() {
    let newLocalContents = new Map();
    let startTime = Cu.now();

    let rows = await this.db.execute(`
      SELECT b.guid, IFNULL(b.title, "") AS title, h.url,
             (SELECT a.content FROM moz_items_annos a
              JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
              WHERE a.item_id = b.id AND
                    n.name = :smartBookmarkAnno) AS smartBookmarkName,
             b.position
      FROM moz_bookmarks b
      JOIN moz_bookmarks p ON p.id = b.parent
      LEFT JOIN moz_places h ON h.id = b.fk
      LEFT JOIN items v ON v.guid = b.guid
      WHERE v.guid IS NULL AND
            p.guid <> :rootGuid AND
            b.syncStatus <> :syncStatus`,
      { smartBookmarkAnno: PlacesSyncUtils.bookmarks.SMART_BOOKMARKS_ANNO,
        rootGuid: PlacesUtils.bookmarks.rootGuid,
        syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL });

    for await (let row of yieldingIterator(rows)) {
      let guid = row.getResultByName("guid");
      let content = BookmarkContent.fromRow(row);
      newLocalContents.set(guid, content);
    }

    let elapsedTime = Cu.now() - startTime;
    this.recordTelemetryEvent("mirror", "fetch", "newLocalContents",
                              { time: String(elapsedTime),
                                count: String(rows.length) });

    return newLocalContents;
  }

  /**
   * Builds a temporary table with the merge states of all nodes in the merged
   * tree and updates Places to match the merged tree.
   *
   * Conceptually, we examine the merge state of each item, and either keep the
   * complete local state, take the complete remote state, or apply a new
   * structure state and flag the item for reupload.
   *
   * Note that we update Places and flag items *before* upload, while iOS
   * updates the mirror *after* a successful upload. This simplifies our
   * implementation, though we lose idempotent merges. If upload is interrupted,
   * the next sync won't distinguish between new merge states from the previous
   * sync, and local changes. Since this is how Desktop behaved before
   * structured application, that's OK. In the future, we can make this more
   * like iOS.
   *
   * @param {MergedBookmarkNode} mergedRoot
   *        The root of the merged bookmark tree.
   * @param {Object[]} localDeletions
   *        `{ guid, level }` tuples for items to remove from Places and flag as
   *        merged.
   * @param {String[]} remoteDeletions
   *        Remotely deleted GUIDs that should be flagged as merged.
   */
  async updateLocalItemsInPlaces(mergedRoot, localDeletions, remoteDeletions) {
    MirrorLog.debug("Setting up merge states table");
    let mergeStatesParams = Array.from(mergedRoot.mergeStatesParams());
    if (mergeStatesParams.length) {
      await this.db.execute(`
        INSERT INTO mergeStates(localGuid, mergedGuid, parentGuid, level,
                                position, valueState, structureState)
        VALUES(IFNULL(:localGuid, :mergedGuid), :mergedGuid, :parentGuid,
               :level, :position, :valueState, :structureState)`,
        mergeStatesParams);
    }

    MirrorLog.debug("Inserting new URLs into Places");
    await this.db.execute(`
      INSERT OR IGNORE INTO moz_places(url, url_hash, rev_host, hidden,
                                       frecency, guid)
      SELECT u.url, u.hash, u.revHost, 0,
             (CASE v.kind WHEN :queryKind THEN 0 ELSE -1 END),
             IFNULL((SELECT h.guid FROM moz_places h
                     WHERE h.url_hash = u.hash AND
                           h.url = u.url), u.guid)
      FROM items v
      JOIN urls u ON u.id = v.urlId
      JOIN mergeStates r ON r.mergedGuid = v.guid
      WHERE r.valueState = :valueState`,
      { queryKind: SyncedBookmarksMirror.KIND.QUERY,
        valueState: BookmarkMergeState.TYPE.REMOTE });
    await this.db.execute(`DELETE FROM moz_updatehostsinsert_temp`);

    // Deleting from `itemsToMerge` fires the `insertNewLocalItems` and
    // `updateExistingLocalItems` triggers.
    MirrorLog.debug("Updating value states for local bookmarks");
    await this.db.execute(`DELETE FROM itemsToMerge`);

    // Update the structure. The mirror stores structure info in a separate
    // table, like iOS, while Places stores structure info on children. We don't
    // check the parent's merge state here because our merged tree might
    // diverge from the server if we're missing children, or moved children
    // without parents to "unfiled". In that case, we *don't* want to reupload
    // the new local structure to the server.
    MirrorLog.debug("Updating structure states for local bookmarks");
    await this.db.execute(`DELETE FROM structureToMerge`);

    MirrorLog.debug("Removing remotely deleted items from Places");
    for (let chunk of PlacesSyncUtils.chunkArray(localDeletions,
      SQLITE_MAX_VARIABLE_NUMBER)) {

      let guids = chunk.map(({ guid }) => guid);

      // Record item removed notifications.
      await this.db.execute(`
        WITH
        guidsWithLevelsToDelete(guid, level) AS (
          VALUES ${chunk.map(({ level }) => `(?, ${level})`).join(",")}
        )
        INSERT INTO itemsRemoved(itemId, parentId, position, type, placeId,
                                 guid, parentGuid, level)
        SELECT b.id, b.parent, b.position, b.type, b.fk, b.guid, p.guid,
               o.level
        FROM moz_bookmarks b
        JOIN moz_bookmarks p ON p.id = b.parent
        JOIN guidsWithLevelsToDelete o ON o.guid = b.guid`,
        guids);

      // Recalculate frecencies. The `isUntagging` check is a formality, since
      // tags shouldn't have annos or tombstones, should not appear in local
      // deletions, and should not affect frecency. This can go away with
      // bug 1293445.
      await this.db.execute(`
        UPDATE moz_places SET
          frecency = -1
        WHERE id IN (SELECT placeId FROM itemsRemoved
                     WHERE NOT isUntagging)`);

      // Remove annos for the deleted items.
      await this.db.execute(`
        DELETE FROM moz_items_annos
        WHERE item_id IN (SELECT itemId FROM itemsRemoved
                          WHERE NOT isUntagging)`);

      // Remove any local tombstones for deleted items.
      await this.db.execute(`
        DELETE FROM moz_bookmarks_deleted
        WHERE guid IN (SELECT guid FROM itemsRemoved)`);

      await this.db.execute(`
        DELETE FROM moz_bookmarks
        WHERE id IN (SELECT itemId FROM itemsRemoved
                     WHERE NOT isUntagging)`);

      // Flag locally deleted items as merged.
      await this.db.execute(`
        UPDATE items SET
          needsMerge = 0
        WHERE needsMerge AND
              guid IN (SELECT guid FROM itemsRemoved
                       WHERE NOT isUntagging)`);
    }

    MirrorLog.debug("Flagging remotely deleted items as merged");
    for (let chunk of PlacesSyncUtils.chunkArray(remoteDeletions,
      SQLITE_MAX_VARIABLE_NUMBER)) {

      await this.db.execute(`
        UPDATE items SET
          needsMerge = 0
        WHERE needsMerge AND
              guid IN (${new Array(chunk.length).fill("?").join(",")})`,
        chunk);
    }
  }

  /**
   * Records Places observer notifications for removed, added, moved, and
   * changed items.
   *
   * @param {BookmarkObserverRecorder} observersToNotify
   */
  async noteObserverChanges(observersToNotify) {
    MirrorLog.debug("Recording observer notifications for removed items");
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
      observersToNotify.noteItemRemoved(info);
    }

    MirrorLog.debug("Recording observer notifications for changed GUIDs");
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
      observersToNotify.noteGuidChanged(info);
    }

    MirrorLog.debug("Recording observer notifications for new items");
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
      observersToNotify.noteItemAdded(info);
    }

    MirrorLog.debug("Recording observer notifications for moved items");
    let movedItemRows = await this.db.execute(`
      SELECT b.id, b.guid, b.type, p.id AS newParentId, c.oldParentId,
             p.guid AS newParentGuid, c.oldParentGuid,
             b.position AS newPosition, c.oldPosition
      FROM itemsMoved c
      JOIN moz_bookmarks b ON b.id = c.itemId
      JOIN moz_bookmarks p ON p.id = b.parent
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
      };
      observersToNotify.noteItemMoved(info);
    }

    MirrorLog.debug("Recording observer notifications for changed items");
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
      observersToNotify.noteItemChanged(info);
    }

    MirrorLog.debug("Recording observer notifications for changed annos");
    let annoRows = await this.db.execute(`
      SELECT itemId, annoName, wasRemoved FROM annosChanged
      ORDER BY itemId`);
    for await (let row of yieldingIterator(annoRows)) {
      let id = row.getResultByName("itemId");
      let name = row.getResultByName("annoName");
      if (row.getResultByName("wasRemoved")) {
        observersToNotify.noteAnnoRemoved(id, name);
      } else {
        observersToNotify.noteAnnoSet(id, name);
      }
    }

    MirrorLog.debug("Recording notifications for changed keywords");
    let keywordsChangedRows = await this.db.execute(`
      SELECT EXISTS(SELECT 1 FROM itemsAdded WHERE keywordChanged) OR
             EXISTS(SELECT 1 FROM itemsChanged WHERE keywordChanged)
             AS keywordsChanged`);
    observersToNotify.shouldInvalidateKeywords =
      !!keywordsChangedRows[0].getResultByName("keywordsChanged");
  }

  /**
   * Stores a snapshot of all locally changed items in a temporary table for
   * upload. This is called from within the merge transaction, to ensure that
   * structure changes made during the sync don't cause us to upload an
   * inconsistent tree.
   *
   * For an example of why we use a temporary table instead of reading directly
   * from Places, consider a user adding a bookmark, then changing its parent
   * folder. We first add the bookmark to the default folder, bump the change
   * counter of the new bookmark and the default folder, then trigger a sync.
   * Depending on how quickly the user picks the new parent, we might upload
   * a record for the default folder, commit the move, then upload the bookmark.
   * We'll still upload the new parent on the next sync, but, in the meantime,
   * we've introduced a parent-child disagreement. This can also happen if the
   * user moves many items between folders.
   *
   * Conceptually, `itemsToUpload` is a transient "view" of locally changed
   * items. The change counter in Places is the persistent record of items that
   * we need to upload, so, if upload is interrupted or fails, we'll stage the
   * items again on the next sync.
   *
   * @param  {String[]} weakUpload
   *         GUIDs of bookmarks to weakly upload.
   */
  async stageItemsToUpload(weakUpload) {
    // Stage explicit weak uploads such as repair responses.
    for (let chunk of PlacesSyncUtils.chunkArray(weakUpload,
      SQLITE_MAX_VARIABLE_NUMBER)) {
      await this.db.execute(`
        INSERT INTO itemsToWeaklyReupload(id)
        SELECT b.id FROM moz_bookmarks b
        WHERE b.guid IN (${new Array(chunk.length).fill("?").join(",")})`,
        chunk);
    }

    // Stage remotely changed items with older local creation dates. These are
    // tracked "weakly": if the upload is interrupted or fails, we won't
    // reupload the record on the next sync.
    await this.db.execute(`
      INSERT OR IGNORE INTO itemsToWeaklyReupload(id)
      SELECT b.id FROM moz_bookmarks b
      JOIN mergeStates r ON r.mergedGuid = b.guid
      JOIN items v ON v.guid = r.mergedGuid
      WHERE r.valueState = :valueState AND
            /* "b.dateAdded" is in microseconds; "v.dateAdded" is in
               milliseconds. */
            b.dateAdded / 1000 < v.dateAdded`,
      { valueState: BookmarkMergeState.TYPE.REMOTE });

    // Stage remaining locally changed items for upload.
    await this.db.execute(`
      WITH RECURSIVE
      syncedItems(id, level) AS (
        SELECT b.id, 0 AS level FROM moz_bookmarks b
        WHERE b.guid IN (${UserContentRootsAsSqlList})
        UNION ALL
        SELECT b.id, s.level + 1 AS level FROM moz_bookmarks b
        JOIN syncedItems s ON s.id = b.parent
      )
      INSERT INTO itemsToUpload(id, guid, syncChangeCounter, parentGuid,
                                parentTitle, dateAdded, type, title, isQuery,
                                url, tags, description, loadInSidebar,
                                smartBookmarkName, keyword, feedURL, siteURL,
                                position, tagFolderName)
      SELECT b.id, b.guid, b.syncChangeCounter, p.guid, p.title,
             b.dateAdded / 1000, b.type, b.title,
             IFNULL(SUBSTR(h.url, 1, 6) = 'place:', 0) AS isQuery,
             h.url,
             (SELECT GROUP_CONCAT(t.title, ',') FROM moz_bookmarks e
              JOIN moz_bookmarks t ON t.id = e.parent
              JOIN moz_bookmarks r ON r.id = t.parent
              WHERE b.type = :bookmarkType AND
                    r.guid = :tagsGuid AND
                    e.fk = h.id),
             (SELECT a.content FROM moz_items_annos a
              JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
              WHERE b.type IN (:bookmarkType, :folderType) AND
                    a.item_id = b.id AND
                    n.name = :descriptionAnno),
             IFNULL((SELECT a.content FROM moz_items_annos a
                     JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
                     WHERE a.item_id = b.id AND
                           n.name = :sidebarAnno), 0),
             (SELECT a.content FROM moz_items_annos a
              JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
              WHERE a.item_id = b.id AND
                    n.name = :smartBookmarkAnno),
             (SELECT keyword FROM moz_keywords WHERE place_id = h.id),
             (SELECT a.content FROM moz_items_annos a
              JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
              WHERE b.type = :folderType AND
                    a.item_id = b.id AND
                    n.name = :feedURLAnno),
             (SELECT a.content FROM moz_items_annos a
              JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
              WHERE b.type = :folderType AND
                    a.item_id = b.id AND
                    n.name = :siteURLAnno),
             b.position,
             (SELECT get_query_param(substr(url, 7), 'tag')
              WHERE substr(h.url, 1, 6) = 'place:')
      FROM moz_bookmarks b
      JOIN moz_bookmarks p ON p.id = b.parent
      JOIN syncedItems s ON s.id = b.id
      LEFT JOIN moz_places h ON h.id = b.fk
      LEFT JOIN itemsToWeaklyReupload w ON w.id = b.id
      WHERE b.syncChangeCounter >= 1 OR
            w.id NOT NULL`,
      { bookmarkType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        tagsGuid: PlacesUtils.bookmarks.tagsGuid,
        descriptionAnno: PlacesSyncUtils.bookmarks.DESCRIPTION_ANNO,
        sidebarAnno: PlacesSyncUtils.bookmarks.SIDEBAR_ANNO,
        smartBookmarkAnno: PlacesSyncUtils.bookmarks.SMART_BOOKMARKS_ANNO,
        folderType: PlacesUtils.bookmarks.TYPE_FOLDER,
        feedURLAnno: PlacesUtils.LMANNO_FEEDURI,
        siteURLAnno: PlacesUtils.LMANNO_SITEURI });

    // Record the child GUIDs of locally changed folders, which we use to
    // populate the `children` array in the record.
    await this.db.execute(`
      INSERT INTO structureToUpload(guid, parentId, position)
      SELECT b.guid, b.parent, b.position FROM moz_bookmarks b
      JOIN itemsToUpload o ON o.id = b.parent`);

    // Finally, stage tombstones for deleted items. Ignore conflicts if we have
    // tombstones for undeleted items; Places Maintenance should clean these up.
    await this.db.execute(`
      INSERT OR IGNORE INTO itemsToUpload(guid, syncChangeCounter, isDeleted,
                                          dateAdded)
      SELECT guid, 1, 1, dateRemoved / 1000 FROM moz_bookmarks_deleted`);
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

    let childGuidRows = await this.db.execute(`
      SELECT parentId, guid FROM structureToUpload
      ORDER BY parentId, position`);

    for (let row of childGuidRows) {
      let localParentId = row.getResultByName("parentId");
      let childRecordId = PlacesSyncUtils.bookmarks.guidToRecordId(
        row.getResultByName("guid"));
      if (childRecordIdsByLocalParentId.has(localParentId)) {
        let childRecordIds = childRecordIdsByLocalParentId.get(localParentId);
        childRecordIds.push(childRecordId);
      } else {
        childRecordIdsByLocalParentId.set(localParentId, [childRecordId]);
      }
    }

    let itemRows = await this.db.execute(`
      SELECT id, syncChangeCounter, guid, isDeleted, type, isQuery,
             smartBookmarkName, tagFolderName,
             loadInSidebar, keyword, tags, url, IFNULL(title, "") AS title,
             description, feedURL, siteURL, position, parentGuid,
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
              queryId: row.getResultByName("smartBookmarkName"),
              // folderName should never be an empty string or null
              folderName: row.getResultByName("tagFolderName") || undefined,
            };
            let description = row.getResultByName("description");
            if (description) {
              queryCleartext.description = description;
            }
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
          let description = row.getResultByName("description");
          if (description) {
            bookmarkCleartext.description = description;
          }
          let loadInSidebar = row.getResultByName("loadInSidebar");
          if (loadInSidebar) {
            bookmarkCleartext.loadInSidebar = true;
          }
          let keyword = row.getResultByName("keyword");
          if (keyword) {
            bookmarkCleartext.keyword = keyword;
          }
          let tags = row.getResultByName("tags");
          if (tags) {
            bookmarkCleartext.tags = tags.split(",");
          }
          changeRecords[recordId] = new BookmarkChangeRecord(
            syncChangeCounter, bookmarkCleartext);
          continue;
        }

        case PlacesUtils.bookmarks.TYPE_FOLDER: {
          let feedURLHref = row.getResultByName("feedURL");
          if (feedURLHref) {
            // Places stores livemarks as folders with feed and site URL annos.
            // See bug 1072833 for discussion about changing them to queries.
            let livemarkCleartext = {
              id: recordId,
              type: "livemark",
              parentid: parentRecordId,
              hasDupe: true,
              parentName: row.getResultByName("parentTitle"),
              dateAdded: row.getResultByName("dateAdded") || undefined,
              title: row.getResultByName("title"),
              feedUri: feedURLHref,
            };
            let description = row.getResultByName("description");
            if (description) {
              livemarkCleartext.description = description;
            }
            let siteURLHref = row.getResultByName("siteURL");
            if (siteURLHref) {
              livemarkCleartext.siteUri = siteURLHref;
            }
            changeRecords[recordId] = new BookmarkChangeRecord(
              syncChangeCounter, livemarkCleartext);
            continue;
          }

          let folderCleartext = {
            id: recordId,
            type: "folder",
            parentid: parentRecordId,
            hasDupe: true,
            parentName: row.getResultByName("parentTitle"),
            dateAdded: row.getResultByName("dateAdded") || undefined,
            title: row.getResultByName("title"),
          };
          let description = row.getResultByName("description");
          if (description) {
            folderCleartext.description = description;
          }
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
        await this.db.close();
        this.finalizeAt.removeBlocker(this.finalizeBound);
      })();
    }
    return this.finalizePromise;
  }
}

this.SyncedBookmarksMirror = SyncedBookmarksMirror;

/** Synced item kinds. Each corresponds to a Sync record type. */
SyncedBookmarksMirror.KIND = {
  BOOKMARK: 1,
  QUERY: 2,
  FOLDER: 3,
  LIVEMARK: 4,
  SEPARATOR: 5,
};

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
  if (currentSchemaVersion < 2) {
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
    key TEXT PRIMARY KEY,
    value NOT NULL
  )`);

  await db.execute(`CREATE TABLE mirror.items(
    id INTEGER PRIMARY KEY,
    guid TEXT UNIQUE NOT NULL,
    /* The server modified time, in milliseconds. */
    serverModified INTEGER NOT NULL DEFAULT 0,
    needsMerge BOOLEAN NOT NULL DEFAULT 0,
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
    siteURL TEXT,
    /* Only bookmarks and queries must have URLs. */
    CHECK(CASE WHEN kind IN (${[
                      SyncedBookmarksMirror.KIND.BOOKMARK,
                      SyncedBookmarksMirror.KIND.QUERY,
                    ].join(",")}) THEN urlId NOT NULL
               ELSE urlId IS NULL END)
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
      INSERT INTO items(guid, kind, needsMerge)
      VALUES(:guid, :kind, :needsMerge)`,
      { guid, kind: SyncedBookmarksMirror.KIND.FOLDER, needsMerge });

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
  // Columns in `items` that correspond to annos stored in `moz_items_annos`.
  // We use this table to build SQL fragments for the `insertNewLocalItems` and
  // `updateExistingLocalItems` triggers below.
  const syncedAnnoTriggers = [{
    annoName: PlacesSyncUtils.bookmarks.DESCRIPTION_ANNO,
    columnName: "newDescription",
    type: PlacesUtils.annotations.TYPE_STRING,
  }, {
    annoName: PlacesSyncUtils.bookmarks.SIDEBAR_ANNO,
    columnName: "newLoadInSidebar",
    type: PlacesUtils.annotations.TYPE_INT32,
  }, {
    annoName: PlacesSyncUtils.bookmarks.SMART_BOOKMARKS_ANNO,
    columnName: "newSmartBookmarkName",
    type: PlacesUtils.annotations.TYPE_STRING,
  }, {
    annoName: PlacesUtils.LMANNO_FEEDURI,
    columnName: "newFeedURL",
    type: PlacesUtils.annotations.TYPE_STRING,
  }, {
    annoName: PlacesUtils.LMANNO_SITEURI,
    columnName: "newSiteURL",
    type: PlacesUtils.annotations.TYPE_STRING,
  }];

  // Stores the value and structure states of all nodes in the merged tree.
  await db.execute(`CREATE TEMP TABLE mergeStates(
    localGuid TEXT NOT NULL,
    mergedGuid TEXT NOT NULL,
    parentGuid TEXT NOT NULL,
    level INTEGER NOT NULL,
    position INTEGER NOT NULL,
    valueState INTEGER NOT NULL,
    structureState INTEGER NOT NULL,
    PRIMARY KEY(localGuid, mergedGuid)
  ) WITHOUT ROWID`);

  // A view of the value states for all bookmarks in the mirror. We use triggers
  // on this view to update Places. Note that we can't just `REPLACE INTO
  // moz_bookmarks`, because `REPLACE` doesn't fire the `AFTER DELETE` triggers
  // that Places uses to maintain schema coherency.
  await db.execute(`
    CREATE TEMP VIEW itemsToMerge(localId, remoteId, hasRemoteValue, newLevel,
                                  oldGuid, newGuid, newType,
                                  newDateAddedMicroseconds, newTitle,
                                  oldPlaceId, newPlaceId, newKeyword,
                                  newDescription, newLoadInSidebar,
                                  newSmartBookmarkName, newFeedURL,
                                  newSiteURL) AS
    SELECT b.id, v.id, r.valueState = ${BookmarkMergeState.TYPE.REMOTE},
           r.level, r.localGuid, r.mergedGuid,
           (CASE WHEN v.kind IN (${[
                        SyncedBookmarksMirror.KIND.BOOKMARK,
                        SyncedBookmarksMirror.KIND.QUERY,
                      ].join(",")}) THEN ${PlacesUtils.bookmarks.TYPE_BOOKMARK}
                 WHEN v.kind IN (${[
                        SyncedBookmarksMirror.KIND.FOLDER,
                        SyncedBookmarksMirror.KIND.LIVEMARK,
                      ].join(",")}) THEN ${PlacesUtils.bookmarks.TYPE_FOLDER}
                 ELSE ${PlacesUtils.bookmarks.TYPE_SEPARATOR} END),
           /* Take the older creation date. "b.dateAdded" is in microseconds;
              "v.dateAdded" is in milliseconds. */
           (CASE WHEN b.dateAdded / 1000 < v.dateAdded THEN b.dateAdded
                 ELSE v.dateAdded * 1000 END),
           v.title, h.id, u.newPlaceId, v.keyword, v.description,
           v.loadInSidebar, v.smartBookmarkName, v.feedURL, v.siteURL
    FROM items v
    JOIN mergeStates r ON r.mergedGuid = v.guid
    LEFT JOIN moz_bookmarks b ON b.guid = r.localGuid
    LEFT JOIN moz_places h ON h.id = b.fk
    LEFT JOIN (
      SELECT h.id AS newPlaceId, u.id AS urlId
      FROM urls u
      JOIN moz_places h ON h.url_hash = u.hash AND
                           h.url = u.url
    ) u ON u.urlId = v.urlId
    WHERE r.mergedGuid <> '${PlacesUtils.bookmarks.rootGuid}'`);

  // Changes local GUIDs to remote GUIDs, drops local tombstones for revived
  // remote items, and flags remote items as merged. In the trigger body, `OLD`
  // refers to the row for the unmerged item in `itemsToMerge`.
  await db.execute(`
    CREATE TEMP TRIGGER mergeGuids
    INSTEAD OF DELETE ON itemsToMerge
    BEGIN
      /* We update GUIDs here, instead of in the "updateExistingLocalItems"
         trigger, because deduped items where we're keeping the local value
         state won't have "hasRemoteValue" set. */
      UPDATE moz_bookmarks SET
        guid = OLD.newGuid,
        syncStatus = ${PlacesUtils.bookmarks.SYNC_STATUS.NORMAL}
      WHERE OLD.oldGuid <> OLD.newGuid AND
            id = OLD.localId;

      /* Record item changed notifications for the updated GUIDs. */
      INSERT INTO guidsChanged(itemId, oldGuid, level)
      SELECT OLD.localId, OLD.oldGuid, OLD.newLevel
      WHERE OLD.oldGuid <> OLD.newGuid;

      /* Drop local tombstones for revived remote items. */
      DELETE FROM moz_bookmarks_deleted WHERE guid = OLD.newGuid;

      /* Flag the remote item as merged. */
      UPDATE items SET
        needsMerge = 0
      WHERE needsMerge AND
            id = OLD.remoteId;
    END`);

  // Inserts items from the mirror that don't exist locally.
  await db.execute(`
    CREATE TEMP TRIGGER insertNewLocalItems
    INSTEAD OF DELETE ON itemsToMerge WHEN OLD.localId IS NULL
    BEGIN
      /* Record an item added notification for the new item. */
      INSERT INTO itemsAdded(guid, keywordChanged, level)
      VALUES(OLD.newGuid, OLD.newKeyword NOT NULL OR
                          EXISTS(SELECT 1 FROM moz_keywords
                                 WHERE place_id = OLD.newPlaceId OR
                                       keyword = OLD.newKeyword),
             OLD.newLevel);

      /* Sync associates keywords with bookmarks, and doesn't sync POST data;
         Places associates keywords with (URL, POST data) pairs, and multiple
         bookmarks may have the same URL. For simplicity, we bump the change
         counter for all local bookmarks with the remote URL (bug 1328737),
         then remove all local keywords from remote URLs, and the remote keyword
         from local URLs. */
      UPDATE moz_bookmarks SET
        syncChangeCounter = syncChangeCounter + 1
      WHERE fk IN (
        /* We intentionally use "place_id = OLD.newPlaceId" in the subquery,
           instead of "fk = OLD.newPlaceId OR fk IN (...)" in the WHERE clause
           above, because we only want to bump the counter if the URL has
           keywords. */
        SELECT place_id FROM moz_keywords
        WHERE place_id = OLD.newPlaceId OR
              keyword = OLD.newKeyword);

      /* Remove the new keyword from existing items, and all keywords from the
         new URL. */
      DELETE FROM moz_keywords WHERE place_id = OLD.newPlaceId OR
                                     keyword = OLD.newKeyword;

      /* Remove existing tags for the new URL. */
      DELETE FROM localTags WHERE placeId = OLD.newPlaceId;

      /* Insert the new item, using "-1" as the placeholder parent and
         position. We'll update these later, in the "updateLocalStructure"
         trigger. */
      INSERT INTO moz_bookmarks(guid, parent, position, type, fk, title,
                                dateAdded, lastModified, syncStatus,
                                syncChangeCounter)
      VALUES(OLD.newGuid, -1, -1, OLD.newType, OLD.newPlaceId, OLD.newTitle,
             OLD.newDateAddedMicroseconds,
             STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000,
             ${PlacesUtils.bookmarks.SYNC_STATUS.NORMAL}, 0);

      /* Insert a new keyword for the new URL, if one is set. */
      INSERT OR IGNORE INTO moz_keywords(keyword, place_id, post_data)
      SELECT OLD.newKeyword, OLD.newPlaceId, ''
      WHERE OLD.newKeyword NOT NULL;

      /* Insert new tags for the new URL. */
      INSERT INTO localTags(tag, placeId)
      SELECT t.tag, OLD.newPlaceId FROM tags t
      WHERE t.itemId = OLD.remoteId;

      /* Insert new synced annos. These are almost identical to the statements
         for updates, except we need an additional subquery to fetch the new
         item's ID. We can also skip removing existing annos. */
      INSERT OR IGNORE INTO moz_anno_attributes(name)
      VALUES ${syncedAnnoTriggers.map(annoTrigger =>
        `('${annoTrigger.annoName}')`
      ).join(",")};

      ${syncedAnnoTriggers.map(annoTrigger => `
        INSERT INTO moz_items_annos(item_id, anno_attribute_id, content, flags,
                                    expiration, type, lastModified, dateAdded)
        SELECT (SELECT id FROM moz_bookmarks
                WHERE guid = OLD.newGuid),
               (SELECT id FROM moz_anno_attributes
                WHERE name = '${annoTrigger.annoName}'),
               OLD.${annoTrigger.columnName}, 0,
               ${PlacesUtils.annotations.EXPIRE_NEVER}, ${annoTrigger.type},
               STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000,
               STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000
        WHERE OLD.${annoTrigger.columnName} NOT NULL;

        /* Record an anno set notification for the new synced anno. */
        REPLACE INTO annosChanged(itemId, annoName, wasRemoved)
        SELECT b.id, '${annoTrigger.annoName}', 0 FROM moz_bookmarks b
        WHERE b.guid = OLD.newGuid AND
              OLD.${annoTrigger.columnName} NOT NULL;
      `).join("")}
    END`);

  // Updates existing items with new values from the mirror.
  await db.execute(`
    CREATE TEMP TRIGGER updateExistingLocalItems
    INSTEAD OF DELETE ON itemsToMerge WHEN OLD.hasRemoteValue AND
                                           OLD.localId NOT NULL
    BEGIN
      /* Record an item changed notification for the existing item. */
      INSERT INTO itemsChanged(itemId, oldTitle, oldPlaceId, keywordChanged,
                               level)
      SELECT id, title, OLD.oldPlaceId, OLD.newKeyword NOT NULL OR
               EXISTS(SELECT 1 FROM moz_keywords
                      WHERE place_id IN (OLD.oldPlaceId, OLD.newPlaceId) OR
                            keyword = OLD.newKeyword),
             OLD.newLevel
      FROM moz_bookmarks
      WHERE id = OLD.localId;

      UPDATE moz_bookmarks SET
        title = OLD.newTitle,
        dateAdded = OLD.newDateAddedMicroseconds,
        lastModified = STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000,
        syncStatus = ${PlacesUtils.bookmarks.SYNC_STATUS.NORMAL},
        syncChangeCounter = 0
      WHERE id = OLD.localId;

      /* Bump the change counter for items with the old URL, new URL, and new
         keyword. */
      UPDATE moz_bookmarks SET
        syncChangeCounter = syncChangeCounter + 1
      WHERE fk IN (SELECT place_id FROM moz_keywords
                   WHERE place_id IN (OLD.oldPlaceId, OLD.newPlaceId) OR
                         keyword = OLD.newKeyword);

      /* Remove the new keyword from existing items, and all keywords from the
         old and new URLs. */
      DELETE FROM moz_keywords WHERE place_id IN (OLD.oldPlaceId,
                                                  OLD.newPlaceId) OR
                                     keyword = OLD.newKeyword;

      /* Remove existing tags. */
      DELETE FROM localTags WHERE placeId IN (OLD.oldPlaceId, OLD.newPlaceId);

      /* Update the URL and recalculate frecency. It's important we do this
         *after* removing old keywords and *before* inserting new ones, so that
         the above statements select the correct affected items. */
      UPDATE moz_bookmarks SET
        fk = OLD.newPlaceId
      WHERE OLD.oldPlaceId <> OLD.newPlaceId AND
            id = OLD.localId;

      UPDATE moz_places SET
        frecency = -1
      WHERE OLD.oldPlaceId <> OLD.newPlaceId AND
            id IN (OLD.oldPlaceId, OLD.newPlaceId);

      /* Insert a new keyword for the new URL, if one is set. */
      INSERT OR IGNORE INTO moz_keywords(keyword, place_id, post_data)
      SELECT OLD.newKeyword, OLD.newPlaceId, ''
      WHERE OLD.newKeyword NOT NULL;

      /* Insert new tags for the new URL. */
      INSERT INTO localTags(tag, placeId)
      SELECT t.tag, OLD.newPlaceId FROM tags t
      WHERE t.itemId = OLD.remoteId;

      /* Record anno removed notifications for the synced annos. */
      REPLACE INTO annosChanged(itemId, annoName, wasRemoved)
      SELECT a.item_id, n.name, 1 FROM moz_items_annos a
      JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
      WHERE item_id = OLD.localId AND
            anno_attribute_id IN (SELECT id FROM moz_anno_attributes
                                  WHERE name IN (${syncedAnnoTriggers.map(
                                    annoTrigger => `'${annoTrigger.annoName}'`
                                  ).join(",")}));

      /* Remove existing synced annos. */
      DELETE FROM moz_items_annos
      WHERE item_id = OLD.localId AND
            anno_attribute_id IN (SELECT id FROM moz_anno_attributes
                                  WHERE name IN (${syncedAnnoTriggers.map(
                                    annoTrigger => `'${annoTrigger.annoName}'`
                                  ).join(",")}));

      /* Insert new synced annos. */
      INSERT OR IGNORE INTO moz_anno_attributes(name)
      VALUES ${syncedAnnoTriggers.map(annoTrigger =>
        `('${annoTrigger.annoName}')`
      ).join(",")};

      ${syncedAnnoTriggers.map(annoTrigger => `
        INSERT INTO moz_items_annos(item_id, anno_attribute_id, content, flags,
                                    expiration, type, lastModified, dateAdded)
        SELECT OLD.localId, (SELECT id FROM moz_anno_attributes
                             WHERE name = '${annoTrigger.annoName}'),
               OLD.${annoTrigger.columnName}, 0,
               ${PlacesUtils.annotations.EXPIRE_NEVER}, ${annoTrigger.type},
               STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000,
               STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000
         WHERE OLD.${annoTrigger.columnName} NOT NULL;

         /* Record an anno set notification for the new synced anno. */
         REPLACE INTO annosChanged(itemId, annoName, wasRemoved)
         SELECT OLD.localId, '${annoTrigger.annoName}', 0
         WHERE OLD.${annoTrigger.columnName} NOT NULL;
      `).join("")}
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
    CREATE TEMP VIEW structureToMerge(localId, hasNewStructure, isRoot,
                                      oldParentId, newParentId, oldPosition,
                                      newPosition, newLevel) AS
    SELECT b.id, r.structureState = ${BookmarkMergeState.TYPE.NEW},
           '${PlacesUtils.bookmarks.rootGuid}' IN (r.mergedGuid, r.parentGuid),
           b.parent, p.id, b.position, r.position, r.level
    FROM moz_bookmarks b
    JOIN mergeStates r ON r.mergedGuid = b.guid
    JOIN moz_bookmarks p ON p.guid = r.parentGuid`);

  // Updates all parents and positions to reflect the merged tree.
  await db.execute(`
    CREATE TEMP TRIGGER updateLocalStructure
    INSTEAD OF DELETE ON structureToMerge WHEN NOT OLD.isRoot
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

  // Bump the change counter for folders with new structure state, so that
  // they're reuploaded to the server.
  await db.execute(`
    CREATE TEMP TRIGGER flagNewStructure
    INSTEAD OF DELETE ON structureToMerge WHEN OLD.hasNewStructure
    BEGIN
      UPDATE moz_bookmarks SET
        syncChangeCounter = syncChangeCounter + 1
      WHERE id = OLD.localId;
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

  // Stores properties to pass to `onItemAnnotation{Set, Removed}` anno
  // observers.
  await db.execute(`CREATE TEMP TABLE annosChanged(
    itemId INTEGER NOT NULL,
    annoName TEXT NOT NULL,
    wasRemoved BOOLEAN NOT NULL,
    PRIMARY KEY(itemId, annoName, wasRemoved)
  ) WITHOUT ROWID`);

  await db.execute(`CREATE TEMP TABLE itemsToWeaklyReupload(
    id INTEGER PRIMARY KEY
  )`);

  // Stores locally changed items staged for upload. See `stageItemsToUpload`
  // for an explanation of why these tables exists.
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
    isQuery BOOLEAN NOT NULL DEFAULT 0,
    url TEXT,
    tags TEXT,
    description TEXT,
    loadInSidebar BOOLEAN,
    smartBookmarkName TEXT,
    tagFolderName TEXT,
    keyword TEXT,
    feedURL TEXT,
    siteURL TEXT,
    position INTEGER
  )`);

  await db.execute(`CREATE TEMP TABLE structureToUpload(
    guid TEXT PRIMARY KEY,
    parentId INTEGER NOT NULL REFERENCES itemsToUpload(id)
                              ON DELETE CASCADE,
    position INTEGER NOT NULL
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

// Converts a Sync record ID to a Places GUID. Returns `null` if the ID is
// invalid.
function validateGuid(recordId) {
  let guid = PlacesSyncUtils.bookmarks.recordIdToGuid(recordId);
  return PlacesUtils.isValidGuid(guid) ? guid : null;
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

function validateDescription(rawDescription) {
  if (typeof rawDescription != "string" || !rawDescription) {
    return null;
  }
  return rawDescription.slice(0, DB_DESCRIPTION_LENGTH_MAX);
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
  if (!tag || tag.length > Ci.nsITaggingService.MAX_TAG_LENGTH) {
    // Drop empty and oversized tags.
    return null;
  }
  return tag;
}

// Recursively inflates a bookmark tree from a pseudo-tree that maps
// parents to children.
async function inflateTree(tree, pseudoTree, parentGuid) {
  let nodes = pseudoTree.get(parentGuid);
  if (nodes) {
    for (let node of nodes) {
      await maybeYield();
      tree.insert(parentGuid, node);
      await inflateTree(tree, pseudoTree, node.guid);
    }
  }
}

/**
 * Content info for an item in the local or remote tree. This is used to dedupe
 * NEW local items to remote items that don't exist locally. See `makeDupeKey`
 * for how we determine if two items are dupes.
 */
class BookmarkContent {
  constructor(title, urlHref, smartBookmarkName, position) {
    this.title = title;
    this.urlHref = urlHref;
    this.smartBookmarkName = smartBookmarkName;
    this.position = position;
  }

  static fromRow(row) {
    let title = row.getResultByName("title");
    let urlHref = row.getResultByName("url");
    let smartBookmarkName = row.getResultByName("smartBookmarkName");
    let position = row.getResultByName("position");
    return new BookmarkContent(title, urlHref, smartBookmarkName, position);
  }
}

/**
 * Builds a lookup key for a node and its content. This is used to match nodes
 * with different GUIDs and similar content.
 *
 * - Bookmarks must have the same title and URL.
 * - Smart bookmarks must have the same smart bookmark name. Other queries
 *   must have the same title and query URL.
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
    case SyncedBookmarksMirror.KIND.BOOKMARK:
      // We use `JSON.stringify([...])` instead of `[...].join(",")` to avoid
      // escaping the `,` in titles and URLs.
      return JSON.stringify([node.kind, content.title, content.urlHref]);

    case SyncedBookmarksMirror.KIND.QUERY:
      if (content.smartBookmarkName) {
        return JSON.stringify([node.kind, content.smartBookmarkName]);
      }
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
  constructor(type, newStructureNode = null) {
    this.type = type;
    this.newStructureNode = newStructureNode;
  }

  /**
   * Takes an existing value state, and a new node for the structure state. We
   * use the new merge state to resolve conflicts caused by moving local items
   * out of a remotely deleted folder, or remote items out of a locally deleted
   * folder.
   *
   * Applying a new merged node bumps its local change counter, so that the
   * merged structure is reuploaded to the server.
   *
   * @param  {BookmarkMergeState} oldState
   *         The existing value state.
   * @param  {BookmarkNode} newStructureNode
   *         A node to use for the new structure state.
   * @return {BookmarkMergeState}
   *         The new merge state.
   */
  static new(oldState, newStructureNode) {
    return new BookmarkMergeState(oldState.type, newStructureNode);
  }

  // Returns the structure state type: `LOCAL`, `REMOTE`, or `NEW`.
  structure() {
    return this.newStructureNode ? BookmarkMergeState.TYPE.NEW : this.type;
  }

  // Returns the value state type: `LOCAL` or `REMOTE`.
  value() {
    return this.type;
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
    switch (this.value()) {
      case BookmarkMergeState.TYPE.LOCAL:
        return "Value: Local";
      case BookmarkMergeState.TYPE.REMOTE:
        return "Value: Remote";
    }
    return "Value: ?";
  }

  structureToString() {
    switch (this.structure()) {
      case BookmarkMergeState.TYPE.LOCAL:
        return "Structure: Local";
      case BookmarkMergeState.TYPE.REMOTE:
        return "Structure: Remote";
      case BookmarkMergeState.TYPE.NEW:
        // We intentionally don't log the new structure node here, since
        // the merger already does that.
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
  constructor(guid, age, kind, needsMerge = false) {
    this.guid = guid;
    this.kind = kind;
    this.age = age;
    this.needsMerge = needsMerge;
    this.children = [];
  }

  // Creates a virtual folder node for the Places root.
  static root() {
    let guid = PlacesUtils.bookmarks.rootGuid;
    return new BookmarkNode(guid, 0, SyncedBookmarksMirror.KIND.FOLDER);
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

    let syncChangeCounter = row.getResultByName("syncChangeCounter");
    let needsMerge = syncChangeCounter > 0;

    return new BookmarkNode(guid, age, kind, needsMerge);
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

    return new BookmarkNode(guid, age, kind, needsMerge);
  }

  isFolder() {
    return this.kind == SyncedBookmarksMirror.KIND.FOLDER;
  }

  newerThan(otherNode) {
    return this.age < otherNode.age;
  }

  * descendants() {
    for (let node of this.children) {
      yield node;
      if (node.isFolder()) {
        yield* node.descendants();
      }
    }
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
    // A local folder can become a livemark as the remote may have synced
    // as a folder before the annotation was added. However, we don't allow
    // a local livemark to "downgrade" to a folder.
    // We allow merging local folders and remote livemarks because Places
    // stores livemarks as empty folders with feed and site URL annotations.
    // The livemarks service first inserts the folder, and *then* sets
    // annotations. Since this isn't wrapped in a transaction, we might sync
    // before the annotations are set, and upload a folder record instead
    // of a livemark record (bug 632287), then replace the folder with a
    // livemark on the next sync.
    if (this.kind == SyncedBookmarksMirror.KIND.FOLDER &&
        remoteNode.kind == SyncedBookmarksMirror.KIND.LIVEMARK) {
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
    this.byGuid = new Map();
    this.infosByNode = new WeakMap();
    this.deletedGuids = new Set();

    this.root = root;
    this.byGuid.set(this.root.guid, this.root);
  }

  isDeleted(guid) {
    return this.deletedGuids.has(guid);
  }

  nodeForGuid(guid) {
    return this.byGuid.get(guid);
  }

  parentNodeFor(childNode) {
    let info = this.infosByNode.get(childNode);
    return info ? info.parentNode : null;
  }

  levelForGuid(guid) {
    let node = this.byGuid.get(guid);
    if (!node) {
      return -1;
    }
    let info = this.infosByNode.get(node);
    return info ? info.level : -1;
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

    let parentInfo = this.infosByNode.get(parentNode);
    let level = parentInfo ? parentInfo.level + 1 : 0;
    this.infosByNode.set(node, { parentNode, level });
  }

  noteDeleted(guid) {
    this.deletedGuids.add(guid);
  }

  * syncableGuids() {
    let nodesToWalk = PlacesUtils.bookmarks.userContentRoots.map(guid => {
      let node = this.byGuid.get(guid);
      return node ? node.children : [];
    });
    while (nodesToWalk.length) {
      let childNodes = nodesToWalk.pop();
      for (let node of childNodes) {
        yield node.guid;
        nodesToWalk.push(node.children);
      }
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
   * Yields the decided value and structure states of the merged node's
   * descendants. We use these as binding parameters to populate the temporary
   * `mergeStates` table when applying the merged tree to Places.
   */
  * mergeStatesParams(level = 0) {
    for (let position = 0; position < this.mergedChildren.length; ++position) {
      let mergedChild = this.mergedChildren[position];
      let mergeStateParam = {
        localGuid: mergedChild.localNode ? mergedChild.localNode.guid : null,
        // The merged GUID is different than the local GUID if we deduped a
        // NEW local item to a remote item.
        mergedGuid: mergedChild.guid,
        parentGuid: this.guid,
        level,
        position,
        valueState: mergedChild.mergeState.value(),
        structureState: mergedChild.mergeState.structure(),
      };
      yield mergeStateParam;
      yield* mergedChild.mergeStatesParams(level + 1);
    }
  }

  /**
   * Creates a bookmark node from this merged node.
   *
   * @return {BookmarkNode}
   *         A node containing the decided value and structure state.
   */
  async toBookmarkNode() {
    if (MergedBookmarkNode.cachedBookmarkNodes.has(this)) {
      return MergedBookmarkNode.cachedBookmarkNodes.get(this);
    }

    let decidedValueNode = this.decidedValue();
    let decidedStructureState = this.mergeState.structure();
    let needsMerge = decidedStructureState == BookmarkMergeState.TYPE.NEW ||
                     (decidedStructureState == BookmarkMergeState.TYPE.LOCAL &&
                      decidedValueNode.needsMerge);

    let newNode = new BookmarkNode(this.guid, decidedValueNode.age,
                                   decidedValueNode.kind, needsMerge);
    MergedBookmarkNode.cachedBookmarkNodes.set(this, newNode);

    if (newNode.isFolder()) {
      for await (let mergedChildNode of yieldingIterator(this.mergedChildren)) {
        newNode.children.push(await mergedChildNode.toBookmarkNode());
      }
    }

    return newNode;
  }

  /**
   * Decides the value state for the merged node. Note that you can't walk the
   * decided node's children: since the value node doesn't include structure
   * changes from the other side, you'll depart from the merged tree. You'll
   * want to use `toBookmarkNode` instead, which returns a node with the
   * decided value *and* structure.
   *
   * @return {BookmarkNode}
   *         The local or remote node containing the decided value state.
   */
  decidedValue() {
    let valueState = this.mergeState.value();
    switch (valueState) {
      case BookmarkMergeState.TYPE.LOCAL:
        if (!this.localNode) {
          MirrorLog.error("Merged node ${guid} has local value state, but " +
                          "no local node", this);
          throw new TypeError(
            "Can't take local value state without local node");
        }
        return this.localNode;

      case BookmarkMergeState.TYPE.REMOTE:
        if (!this.remoteNode) {
          MirrorLog.error("Merged node ${guid} has remote value state, but " +
                          "no remote node", this);
          throw new TypeError(
            "Can't take remote value state without remote node");
        }
        return this.remoteNode;
    }
    MirrorLog.error("Merged node ${guid} has unknown value state ${valueState}",
                    { guid: this.guid, valueState });
    throw new TypeError("Can't take unknown value state");
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

// Caches bookmark nodes containing the decided value and structure.
MergedBookmarkNode.cachedBookmarkNodes = new WeakMap();

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
      new: 0,
      remoteRevives: 0, // Remote non-folder change wins over local deletion.
      localDeletes: 0, // Local folder deletion wins over remote change.
      localRevives: 0, // Local non-folder change wins over remote deletion.
      remoteDeletes: 0, // Remote folder deletion wins over local change.
    };
    this.dupeCount = 0;
    this.extraTelemetryEvents = [];
  }

  summarizeTelemetryEvents() {
    let events = [...this.extraTelemetryEvents];
    if (this.dupeCount > 0) {
      events.push({ value: "dupes",
                    extra: { count: String(this.dupeCount) } });
    }
    let structureExtra = {};
    for (let key in this.structureCounts) {
      let count = this.structureCounts[key];
      if (count > 0) {
        structureExtra[key] = String(count);
      }
    }
    if (!ObjectUtils.isEmpty(structureExtra)) {
      events.push({ value: "structure", extra: structureExtra });
    }
    return events;
  }

  async merge() {
    let mergedRoot = new MergedBookmarkNode(PlacesUtils.bookmarks.rootGuid,
      BookmarkNode.root(), null, BookmarkMergeState.local);
    for (let guid of PlacesUtils.bookmarks.userContentRoots) {
      let localSyncableRoot = this.localTree.nodeForGuid(guid);
      let remoteSyncableRoot = this.remoteTree.nodeForGuid(guid);
      let mergedSyncableRoot = await this.mergeNode(guid, localSyncableRoot,
                                                    remoteSyncableRoot);
      mergedRoot.mergedChildren.push(mergedSyncableRoot);
    }

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
    for await (let guid of Async.yieldingIterator(tree.syncableGuids())) {
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
      this.extraTelemetryEvents.push({
        value: "kind-mismatch",
        extra: { local: localNode.kindToString().toLowerCase(),
                 remote: remoteNode.kindToString().toLowerCase() },
      });
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
      return BookmarkMergeState.local;
    }
    if (!remoteNode.needsMerge) {
      // The node wasn't changed remotely since the last sync. Keep the local
      // state.
      return BookmarkMergeState.local;
    }
    if (!localNode.needsMerge) {
      // The node was changed remotely, but not locally. Take the remote state.
      return BookmarkMergeState.remote;
    }
    // At this point, we know the item changed locally and remotely. We could
    // query storage to determine if the value state is the same, as iOS does.
    // However, that's an expensive check that requires joining `moz_bookmarks`,
    // `moz_items_annos`, and `moz_places` to the mirror. It's unlikely that
    // the value state is identical, so we skip the value check and use the
    // timestamp to decide which node is newer.
    let valueState = localNode.newerThan(remoteNode) ?
                     BookmarkMergeState.local :
                     BookmarkMergeState.remote;
    return valueState;
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
   * @return {Boolean}
   *         `true` if the merged structure state changed because the remote
   *         child was locally moved or deleted; `false` otherwise.
   */
  async mergeRemoteChildIntoMergedNode(mergedNode, remoteParentNode,
                                       remoteChildNode) {
    if (this.mergedGuids.has(remoteChildNode.guid)) {
      MirrorLog.trace("Remote child ${remoteChildNode} already seen in " +
                      "another folder and merged", { remoteChildNode });
      return false;
    }

    MirrorLog.trace("Merging remote child ${remoteChildNode} of " +
                    "${remoteParentNode} into ${mergedNode}",
                    { remoteChildNode, remoteParentNode, mergedNode });

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
      return true;
    }

    // The remote child isn't locally deleted. Does it exist in the local tree?
    let localChildNode = this.localTree.nodeForGuid(remoteChildNode.guid);
    if (!localChildNode) {
      // Remote child doesn't exist locally, either. Try to find a content
      // match in the containing folder, and dedupe the local item if we can.
      MirrorLog.trace("Remote child ${remoteChildNode} doesn't exist " +
                      "locally; looking for local content match",
                      { remoteChildNode });

      let localChildNodeByContent = await this.findLocalNodeMatchingRemoteNode(
        mergedNode, remoteChildNode);

      let mergedChildNode = await this.mergeNode(remoteChildNode.guid,
                                                 localChildNodeByContent,
                                                 remoteChildNode);
      mergedNode.mergedChildren.push(mergedChildNode);
      return false;
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
      return false;
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

        if (latestLocalAge < latestRemoteAge) {
          // Local move is younger, so we ignore the remote move. We'll
          // merge the child later, when we walk its new local parent.
          MirrorLog.trace("Ignoring older remote move for ${remoteChildNode} " +
                          "to ${remoteParentNode} at ${latestRemoteAge}; " +
                          "local move to ${localParentNode} at " +
                          "${latestLocalAge} is newer",
                          { remoteChildNode, remoteParentNode, latestRemoteAge,
                            localParentNode, latestLocalAge });
          return true;
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
        return false;
      }

      MirrorLog.trace("Remote parent unchanged; keeping remote child " +
                      "${remoteChildNode} in ${localParentNode}",
                      { remoteChildNode, localParentNode });
      return true;
    }

    MirrorLog.trace("Local parent unchanged; keeping remote child " +
                    "${remoteChildNode} in ${remoteParentNode}",
                    { remoteChildNode, remoteParentNode });

    let mergedChildNode = await this.mergeNode(remoteChildNode.guid, localChildNode,
                                               remoteChildNode);
    mergedNode.mergedChildren.push(mergedChildNode);
    return false;
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
   * @return {Boolean}
   *         `true` if the merged structure state changed because the local
   *         child doesn't exist remotely or was locally moved; `false`
   *         otherwise.
   */
  async mergeLocalChildIntoMergedNode(mergedNode, localParentNode, localChildNode) {
    if (this.mergedGuids.has(localChildNode.guid)) {
      // We already merged the child when we walked another folder.
      MirrorLog.trace("Local child ${localChildNode} already seen in " +
                      "another folder and merged", { localChildNode });
      return false;
    }

    MirrorLog.trace("Merging local child ${localChildNode} of " +
                    "${localParentNode} into ${mergedNode}",
                    { localChildNode, localParentNode, mergedNode });

    // Now, we know we haven't seen the local child before, and it's not in
    // this folder on the server. Check if the child is remotely deleted.
    let structureChange = await this.checkForRemoteStructureChangeOfLocalNode(
      mergedNode, localParentNode, localChildNode);
    if (structureChange == BookmarkMerger.STRUCTURE.DELETED) {
      // If the child is remotely deleted, we need to move any new local
      // descendants to the merged node, just as we did for new remote
      // descendants of locally deleted children.
      return true;
    }

    // At this point, we know the local child isn't deleted. See if it
    // exists in the remote tree.
    let remoteChildNode = this.remoteTree.nodeForGuid(localChildNode.guid);
    if (!remoteChildNode) {
      // Local child doesn't exist remotely, either. Try to find a content
      // match in the containing folder, and dedupe the local item if we can.
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
        return false;
      }

      // The local child doesn't exist remotely, but we still need to walk
      // its children.
      let mergedChildNode = await this.mergeNode(localChildNode.guid, localChildNode,
                                                 /* remoteChildNode */ null);
      mergedNode.mergedChildren.push(mergedChildNode);
      return true;
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
      mergedNode.mergedChildren.push(mergedChildNode);
      return true;
    }

    if (localParentNode.needsMerge) {
      if (remoteParentNode.needsMerge) {
        MirrorLog.trace("Local ${localParentNode} and remote " +
                        "${remoteParentNode} parents changed; comparing " +
                        "modified times to decide parent for local child " +
                        "${localChildNode}", { localParentNode,
                                               remoteParentNode,
                                               localChildNode });

        let latestLocalAge = Math.min(localChildNode.age,
                                      localParentNode.age);
        let latestRemoteAge = Math.min(remoteChildNode.age,
                                       remoteParentNode.age);

        if (latestRemoteAge <= latestLocalAge) {
          MirrorLog.trace("Ignoring older local move for ${localChildNode} " +
                          "to ${localParentNode} at ${latestLocalAge}; " +
                          "remote move to ${remoteParentNode} at " +
                          "${latestRemoteAge} is newer",
                          { localChildNode, localParentNode, latestLocalAge,
                            remoteParentNode, latestRemoteAge });
          return false;
        }

        MirrorLog.trace("Taking newer local move for ${localChildNode} to " +
                        "${localParentNode} at ${latestLocalAge}; remote " +
                        "move to ${remoteParentNode} at ${latestRemoteAge} " +
                        "is older", { localChildNode, localParentNode,
                                      latestLocalAge, remoteParentNode,
                                      latestRemoteAge });

        let mergedChildNode = await this.mergeNode(localChildNode.guid,
                                                   localChildNode, remoteChildNode);
        mergedNode.mergedChildren.push(mergedChildNode);
        return true;
      }

      MirrorLog.trace("Remote parent unchanged; keeping local child " +
                      "${localChildNode} in local parent ${localParentNode}",
                      { localChildNode, localParentNode });

      let mergedChildNode = await this.mergeNode(localChildNode.guid, localChildNode,
                                                 remoteChildNode);
      mergedNode.mergedChildren.push(mergedChildNode);
      return true;
    }

    MirrorLog.trace("Local parent unchanged; keeping local child " +
                    "${localChildNode} in remote parent ${remoteParentNode}",
                    { localChildNode, remoteParentNode });
    return false;
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
    let mergeStateChanged = false;

    if (localNode && remoteNode) {
      if (localNode.newerThan(remoteNode)) {
        // The folder exists locally and remotely, and the local node is newer.
        // Walk and merge local children first, followed by remaining unmerged
        // remote children.
        if (await this.mergeLocalChildrenIntoMergedNode(mergedNode, localNode)) {
          mergeStateChanged = true;
        }
        if (await this.mergeRemoteChildrenIntoMergedNode(mergedNode, remoteNode)) {
          mergeStateChanged = true;
        }
      } else {
        // The folder exists locally and remotely, and the remote node is newer.
        // Merge remote children first, then remaining local children.
        if (await this.mergeRemoteChildrenIntoMergedNode(mergedNode, remoteNode)) {
          mergeStateChanged = true;
        }
        if (await this.mergeLocalChildrenIntoMergedNode(mergedNode, localNode)) {
          mergeStateChanged = true;
        }
      }
    } else if (localNode) {
      // The folder only exists locally, so no remote children to merge.
      if (await this.mergeLocalChildrenIntoMergedNode(mergedNode, localNode)) {
        mergeStateChanged = true;
      }
    } else if (remoteNode) {
      // The folder only exists remotely, so local children to merge.
      if (await this.mergeRemoteChildrenIntoMergedNode(mergedNode, remoteNode)) {
        mergeStateChanged = true;
      }
    } else {
      // Should never happen.
      throw new TypeError("Can't merge children for two nonexistent nodes");
    }

    // Update the merge state if we moved children orphaned on one side by a
    // deletion on the other side, if we kept newer locally moved children,
    // or if the child order changed. We already updated the merge state of the
    // orphans, but we also need to flag the containing folder so that it's
    // reuploaded to the server along with the new children.
    if (mergeStateChanged) {
      let newStructureNode = await mergedNode.toBookmarkNode();
      let newMergeState = BookmarkMergeState.new(mergedNode.mergeState,
                                                 newStructureNode);
      MirrorLog.trace("Merge state for ${mergedNode} has new structure " +
                      "${newMergeState}", { mergedNode, newMergeState });
      this.structureCounts.new++;
      mergedNode.mergeState = newMergeState;
    }
  }

  /**
   * Recursively merges the children of a remote folder node.
   *
   * @param  {MergedBookmarkNode} mergedNode
   *         The merged folder node. This method mutates the merged node to
   *         append remote children.
   * @param  {BookmarkNode} remoteNode
   *         The remote folder node.
   * @return {Boolean}
   *         `true` if the merge produced a new structure that should be
   *         reuploaded to the server; `false` otherwise.
   */
  async mergeRemoteChildrenIntoMergedNode(mergedNode, remoteNode) {
    MirrorLog.trace("Merging remote children of ${remoteNode} into " +
                    "${mergedNode}", { remoteNode, mergedNode });

    let mergeStateChanged = false;
    for await (let remoteChildNode of yieldingIterator(remoteNode.children)) {
      let remoteChildrenChanged = await this.mergeRemoteChildIntoMergedNode(
        mergedNode, remoteNode, remoteChildNode);
      if (remoteChildrenChanged) {
        mergeStateChanged = true;
      }
    }
    return mergeStateChanged;
  }

  /**
   * Recursively merges the children of a local folder node.
   *
   * @param  {MergedBookmarkNode} mergedNode
   *         The merged folder node. This method mutates the merged node to
   *         append local children.
   * @param  {BookmarkNode} localNode
   *         The local folder node.
   * @return {Boolean}
   *         `true` if the merge produced a new structure that should be
   *         reuploaded to the server; `false` otherwise.
   */
  async mergeLocalChildrenIntoMergedNode(mergedNode, localNode) {
    MirrorLog.trace("Merging local children of ${localNode} into " +
                    "${mergedNode}", { localNode, mergedNode });

    let mergeStateChanged = false;
    for await (let localChildNode of yieldingIterator(localNode.children)) {
      let remoteChildrenChanged = await this.mergeLocalChildIntoMergedNode(
        mergedNode, localNode, localChildNode);
      if (remoteChildrenChanged) {
        mergeStateChanged = true;
      }
    }
    return mergeStateChanged;
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
    if (!this.localTree.isDeleted(remoteNode.guid)) {
      let localNode = this.localTree.nodeForGuid(remoteNode.guid);
      if (!localNode) {
        return BookmarkMerger.STRUCTURE.UNCHANGED;
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

    this.deleteRemotely.add(remoteNode.guid);

    let mergedOrphanNodes = await this.processRemoteOrphansForNode(mergedNode,
                                                                   remoteNode);
    await this.relocateOrphansTo(mergedNode, mergedOrphanNodes);
    MirrorLog.trace("Relocating remote orphans ${mergedOrphanNodes} to " +
                    "${mergedNode}", { mergedOrphanNodes, mergedNode });

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
    if (!this.remoteTree.isDeleted(localNode.guid)) {
      let remoteNode = this.remoteTree.nodeForGuid(localNode.guid);
      if (!remoteNode) {
        return BookmarkMerger.STRUCTURE.UNCHANGED;
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

    MirrorLog.trace("Local node ${localNode} deleted remotely; taking remote " +
                    "deletion", { localNode });

    this.deleteLocally.add(localNode.guid);

    let mergedOrphanNodes = await this.processLocalOrphansForNode(mergedNode,
                                                                  localNode);
    await this.relocateOrphansTo(mergedNode, mergedOrphanNodes);
    MirrorLog.trace("Relocating local orphans ${mergedOrphanNodes} to " +
                    "${mergedNode}", { mergedOrphanNodes, mergedNode });

    return BookmarkMerger.STRUCTURE.DELETED;
  }

  /**
   * Recursively merges all remote children of a locally deleted folder that
   * haven't also been deleted remotely. This can happen if the user adds a
   * bookmark to a folder on another device, and deletes that folder locally.
   * This is the inverse of `processLocalOrphansForNode`.
   */
  async processRemoteOrphansForNode(mergedNode, remoteNode) {
    let remoteOrphanNodes = [];

    for await (let remoteChildNode of yieldingIterator(remoteNode.children)) {
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

    return mergedOrphanNodes;
  }

  /**
   * Recursively merges all local children of a remotely deleted folder that
   * haven't also been deleted locally. This is the inverse of
   * `processRemoteOrphansForNode`.
   */
  async processLocalOrphansForNode(mergedNode, localNode) {
    if (!localNode.isFolder()) {
      // The local node isn't a folder, so it won't have orphans.
      return [];
    }

    let localOrphanNodes = [];
    for await (let localChildNode of yieldingIterator(localNode.children)) {
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

    return mergedOrphanNodes;
  }

  /**
   * Moves a list of merged orphan nodes to the closest surviving ancestor.
   * Changes the merge state of the moved orphans to new, so that we reupload
   * them along with their new parent on the next sync.
   *
   * @param {MergedBookmarkNode} mergedNode
   *        The closest surviving ancestor.
   * @param {MergedBookmarkNode[]} mergedOrphanNodes
   *        Merged orphans to relocate to the surviving ancestor.
   */
  async relocateOrphansTo(mergedNode, mergedOrphanNodes) {
    for (let mergedOrphanNode of mergedOrphanNodes) {
      let newStructureNode = await mergedOrphanNode.toBookmarkNode();
      let newMergeState = BookmarkMergeState.new(mergedOrphanNode.mergeState,
                                                 newStructureNode);
      mergedOrphanNode.mergeState = newMergeState;
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
 * Records bookmark, annotation, and keyword observer notifications for all
 * changes made during the merge, then fires the notifications after the merge
 * is done.
 *
 * Recording bookmark changes and deletions is somewhat expensive, because we
 * need to fetch all observer infos before writing. Making this more efficient
 * is tracked in bug 1340498.
 *
 * Annotation observers don't require the extra context, so they're cheap to
 * record and fire.
 */
class BookmarkObserverRecorder {
  constructor(db) {
    this.db = db;
    this.bookmarkObserverNotifications = [];
    this.annoObserverNotifications = [];
    this.shouldInvalidateKeywords = false;
    this.shouldInvalidateLivemarks = false;
  }

  /**
   * Fires all recorded observer notifications, invalidates the livemark cache
   * if necessary, and recalculates frecencies for changed URLs. This is called
   * outside the merge transaction.
   */
  async notifyAll() {
    if (this.shouldInvalidateKeywords) {
      await PlacesUtils.keywords.invalidateCachedKeywords();
    }
    await this.notifyBookmarkObservers();
    await this.notifyAnnoObservers();
    if (this.shouldInvalidateLivemarks) {
      await PlacesUtils.livemarks.invalidateCachedLivemarks();
    }
    await this.updateFrecencies();
  }

  async updateFrecencies() {
    MirrorLog.debug("Recalculating frecencies for new URLs");
    await this.db.execute(`
      UPDATE moz_places SET
        frecency = CALCULATE_FRECENCY(id)
      WHERE frecency = -1`);
  }

  noteItemAdded(info) {
    let uri = info.urlHref ? Services.io.newURI(info.urlHref) : null;
    this.bookmarkObserverNotifications.push({
      name: "onItemAdded",
      isTagging: info.isTagging,
      args: [info.id, info.parentId, info.position, info.type, uri, info.title,
        info.dateAdded, info.guid, info.parentGuid,
        PlacesUtils.bookmarks.SOURCES.SYNC],
    });
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
        info.newParentGuid, PlacesUtils.bookmarks.SOURCES.SYNC],
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

  noteAnnoSet(id, name) {
    if (isLivemarkAnno(name)) {
      this.shouldInvalidateLivemarks = true;
    }
    this.annoObserverNotifications.push({
      name: "onItemAnnotationSet",
      args: [id, name, PlacesUtils.bookmarks.SOURCES.SYNC],
    });
  }

  noteAnnoRemoved(id, name) {
    if (isLivemarkAnno(name)) {
      this.shouldInvalidateLivemarks = true;
    }
    this.annoObserverNotifications.push({
      name: "onItemAnnotationRemoved",
      args: [id, name, PlacesUtils.bookmarks.SOURCES.SYNC],
    });
  }

  async notifyBookmarkObservers() {
    MirrorLog.debug("Notifying bookmark observers");
    let observers = PlacesUtils.bookmarks.getObservers();
    for (let observer of observers) {
      this.notifyObserver(observer, "onBeginUpdateBatch");
      for await (let info of yieldingIterator(this.bookmarkObserverNotifications)) {
        if (info.isTagging && observer.skipTags) {
          continue;
        }
        this.notifyObserver(observer, info.name, info.args);
      }
      this.notifyObserver(observer, "onEndUpdateBatch");
    }
  }

  async notifyAnnoObservers() {
    MirrorLog.debug("Notifying anno observers");
    let observers = PlacesUtils.annotations.getObservers();
    for (let observer of observers) {
      let wrapped = yieldingIterator(this.annoObserverNotifications);
      for await (let { name, args } of wrapped) {
        this.notifyObserver(observer, name, args);
      }
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

function isLivemarkAnno(name) {
  return name == PlacesUtils.LMANNO_FEEDURI ||
         name == PlacesUtils.LMANNO_SITEURI;
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
