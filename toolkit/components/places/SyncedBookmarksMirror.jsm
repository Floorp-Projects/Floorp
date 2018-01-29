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

this.EXPORTED_SYMBOLS = ["SyncedBookmarksMirror"];

const { utils: Cu, interfaces: Ci } = Components;

Cu.importGlobalProperties(["URL"]);

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
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

// These can be removed once they're exposed in a central location (bug
// 1375896).
const DB_URL_LENGTH_MAX = 65536;
const DB_TITLE_LENGTH_MAX = 4096;
const DB_DESCRIPTION_LENGTH_MAX = 256;

const SQLITE_MAX_VARIABLE_NUMBER = 999;

// The current mirror database schema version. Bump for migrations, then add
// migration code to `migrateMirrorSchema`.
const MIRROR_SCHEMA_VERSION = 1;

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
    try {
      try {
        await db.execute(`ATTACH :mirrorPath AS mirror`,
                         { mirrorPath: options.path });
      } catch (ex) {
        if (ex.errors && isDatabaseCorrupt(ex.errors[0])) {
          MirrorLog.warn("Error attaching mirror to Places; removing and " +
                         "recreating mirror", ex);
          options.recordTelemetryEvent("mirror", "open", "error",
                                       { why: "corrupt" });
          await OS.File.remove(options.path);
          await db.execute(`ATTACH :mirrorPath AS mirror`,
                           { mirrorPath: options.path });
        } else {
          MirrorLog.warn("Unrecoverable error attaching mirror to Places", ex);
          throw ex;
        }
      }
      await db.execute(`PRAGMA foreign_keys = ON`);
      await migrateMirrorSchema(db);
      await initializeTempMirrorEntities(db);
    } catch (ex) {
      options.recordTelemetryEvent("mirror", "open", "error",
                                   { why: "initialize" });
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
    let rows = await this.db.execute(`
      SELECT MAX(
        IFNULL((SELECT MAX(serverModified) - 1000 FROM items), 0),
        IFNULL((SELECT CAST(value AS INTEGER) FROM meta
                WHERE key = :modifiedKey), 0)
      ) AS highWaterMark`,
      { modifiedKey: SyncedBookmarksMirror.META.MODIFIED });
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
    let lastModified = lastModifiedSeconds * 1000;
    if (!Number.isFinite(lastModified)) {
      throw new TypeError("Invalid collection last modified time");
    }
    await this.db.executeBeforeShutdown(
      "SyncedBookmarksMirror: setCollectionLastModified",
      db => db.execute(`
        REPLACE INTO meta(key, value)
        VALUES(:modifiedKey, :lastModified)`,
        { modifiedKey: SyncedBookmarksMirror.META.MODIFIED, lastModified })
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
    await this.db.executeBeforeShutdown(
      "SyncedBookmarksMirror: store",
      db => db.executeTransaction(async () => {
        for (let record of records) {
          switch (record.type) {
            case "bookmark":
              MirrorLog.trace("Storing bookmark in mirror", record.cleartext);
              await this.storeRemoteBookmark(record, options);
              continue;

            case "query":
              MirrorLog.trace("Storing query in mirror", record.cleartext);
              await this.storeRemoteQuery(record, options);
              continue;

            case "folder":
              MirrorLog.trace("Storing folder in mirror", record.cleartext);
              await this.storeRemoteFolder(record, options);
              continue;

            case "livemark":
              MirrorLog.trace("Storing livemark in mirror", record.cleartext);
              await this.storeRemoteLivemark(record, options);
              continue;

            case "separator":
              MirrorLog.trace("Storing separator in mirror", record.cleartext);
              await this.storeRemoteSeparator(record, options);
              continue;

            default:
              if (record.deleted) {
                MirrorLog.trace("Storing tombstone in mirror",
                                record.cleartext);
                await this.storeRemoteTombstone(record, options);
                continue;
              }
          }
          MirrorLog.warn("Ignoring record with unknown type", record.type);
          this.recordTelemetryEvent("mirror", "ignore", "unknown",
                                    { why: "kind" });
        }
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
   * @return {Object.<String, BookmarkChangeRecord>}
   *         A changeset containing locally changed and reconciled records to
   *         upload to the server, and to store in the mirror once upload
   *         succeeds.
   */
  async apply({ localTimeSeconds = Date.now() / 1000,
                remoteTimeSeconds = 0 } = {}) {
    // We intentionally don't use `executeBeforeShutdown` in this function,
    // since merging can take a while for large trees, and we don't want to
    // block shutdown. Since all new items are in the mirror, we'll just try
    // to merge again on the next sync.
    let { missingParents, missingChildren } = await this.fetchRemoteOrphans();
    if (missingParents.length) {
      MirrorLog.debug("Temporarily reparenting remote items with missing " +
                      "parents to unfiled", missingParents);
      this.recordTelemetryEvent("mirror", "orphans", "parents",
                                { count: String(missingParents.length) });
    }
    if (missingChildren.length) {
      MirrorLog.debug("Remote tree missing items", missingChildren);
      this.recordTelemetryEvent("mirror", "orphans", "children",
                                { count: String(missingChildren.length) });
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
      let mergedRoot = merger.merge();
      for (let { value, extra } of merger.telemetryEvents) {
        this.recordTelemetryEvent("mirror", "merge", value, extra);
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
      if (!merger.subsumes(localTree)) {
        throw new SyncedBookmarksMirror.ConsistencyError(
          "Merged tree doesn't mention all items from local tree");
      }
      if (!merger.subsumes(remoteTree)) {
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
      await this.stageItemsToUpload();

      MirrorLog.debug("Fetching records for local items to upload");
      let changeRecords = await this.fetchLocalChangeRecords();

      await this.db.execute(`DELETE FROM mergeStates`);
      await this.db.execute(`DELETE FROM itemsAdded`);
      await this.db.execute(`DELETE FROM guidsChanged`);
      await this.db.execute(`DELETE FROM itemsChanged`);
      await this.db.execute(`DELETE FROM itemsRemoved`);
      await this.db.execute(`DELETE FROM itemsMoved`);
      await this.db.execute(`DELETE FROM annosChanged`);
      await this.db.execute(`DELETE FROM keywordsChanged`);
      await this.db.execute(`DELETE FROM itemsToUpload`);

      return changeRecords;
    }, this.db.TRANSACTION_IMMEDIATE);

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
      async function(db) {
        await db.executeTransaction(async function() {
          await db.execute(`DELETE FROM meta`);
          await db.execute(`DELETE FROM items`);
          await db.execute(`DELETE FROM urls`);

          // Since we need to reset the modified times for the syncable roots,
          // we simply delete and recreate them.
          await createMirrorRoots(db);
        });
      }
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
    let guid = validateGuid(record.id);
    if (!guid) {
      MirrorLog.warn("Ignoring bookmark with invalid ID", record.id);
      this.recordTelemetryEvent("mirror", "ignore", "bookmark",
                                { why: "id" });
      return;
    }

    let url = validateURL(record.bmkUri);
    if (!url) {
      MirrorLog.trace("Ignoring bookmark ${guid} with invalid URL ${url}",
                      { guid, url: record.bmkUri });
      this.recordTelemetryEvent("mirror", "ignore", "bookmark",
                                { why: "url" });
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

  async storeRemoteQuery(record, { needsMerge }) {
    let guid = validateGuid(record.id);
    if (!guid) {
      MirrorLog.warn("Ignoring query with invalid ID", record.id);
      this.recordTelemetryEvent("mirror", "ignore", "query",
                                { why: "id" });
      return;
    }

    let url = validateURL(record.bmkUri);
    if (!url) {
      MirrorLog.trace("Ignoring query ${guid} with invalid URL ${url}",
                      { guid, url: record.bmkUri });
      this.recordTelemetryEvent("mirror", "ignore", "query",
                                { why: "url" });
      return;
    }

    await this.maybeStoreRemoteURL(url);

    let serverModified = determineServerModified(record);
    let dateAdded = determineDateAdded(record);
    let title = validateTitle(record.title);
    let tagFolderName = validateTag(record.folderName);
    let description = validateDescription(record.description);
    let smartBookmarkName = typeof record.queryId == "string" ?
                            record.queryId : null;

    await this.db.executeCached(`
      REPLACE INTO items(guid, serverModified, needsMerge, kind,
                         dateAdded, title, tagFolderName,
                         urlId, description, smartBookmarkName)
      VALUES(:guid, :serverModified, :needsMerge, :kind,
             :dateAdded, NULLIF(:title, ""), :tagFolderName,
             (SELECT id FROM urls
              WHERE hash = hash(:url) AND
                    url = :url),
             :description, :smartBookmarkName)`,
      { guid, serverModified, needsMerge,
        kind: SyncedBookmarksMirror.KIND.QUERY, dateAdded, title, tagFolderName,
        url: url.href, description, smartBookmarkName });
  }

  async storeRemoteFolder(record, { needsMerge }) {
    let guid = validateGuid(record.id);
    if (!guid) {
      MirrorLog.warn("Ignoring folder with invalid ID", record.id);
      this.recordTelemetryEvent("mirror", "ignore", "folder",
                                { why: "id" });
      return;
    }
    if (guid == PlacesUtils.bookmarks.rootGuid) {
      // The Places root shouldn't be synced at all.
      MirrorLog.warn("Ignoring Places root record", record.cleartext);
      this.recordTelemetryEvent("mirror", "ignore", "folder",
                                { why: "root" });
      return;
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
        let childRecordId = children[position];
        let childGuid = validateGuid(childRecordId);
        if (!childGuid) {
          MirrorLog.warn("Ignoring child of folder ${parentGuid} with " +
                         "invalid ID ${childRecordId}", { parentGuid: guid,
                                                          childRecordId });
          this.recordTelemetryEvent("mirror", "ignore", "child",
                                    { why: "id" });
          continue;
        }
        await this.db.executeCached(`
          REPLACE INTO structure(guid, parentGuid, position)
          VALUES(:childGuid, :parentGuid, :position)`,
          { childGuid, parentGuid: guid, position });
      }
    }
  }

  async storeRemoteLivemark(record, { needsMerge }) {
    let guid = validateGuid(record.id);
    if (!guid) {
      MirrorLog.warn("Ignoring livemark with invalid ID", record.id);
      this.recordTelemetryEvent("mirror", "ignore", "livemark",
                                { why: "id" });
      return;
    }

    let feedURL = validateURL(record.feedUri);
    if (!feedURL) {
      MirrorLog.trace("Ignoring livemark ${guid} with invalid feed URL ${url}",
                      { guid, url: record.feedUri });
      this.recordTelemetryEvent("mirror", "ignore", "livemark",
                                { why: "feed" });
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

  async storeRemoteSeparator(record, { needsMerge }) {
    let guid = validateGuid(record.id);
    if (!guid) {
      MirrorLog.warn("Ignoring separator with invalid ID", record.id);
      this.recordTelemetryEvent("mirror", "ignore", "separator",
                                { why: "id" });
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

  async storeRemoteTombstone(record, { needsMerge }) {
    let guid = validateGuid(record.id);
    if (!guid) {
      MirrorLog.warn("Ignoring tombstone with invalid ID", record.id);
      this.recordTelemetryEvent("mirror", "ignore", "tombstone",
                                { why: "id" });
      return;
    }

    if (PlacesUtils.bookmarks.userContentRoots.includes(guid)) {
      MirrorLog.warn("Ignoring tombstone for syncable root", guid);
      this.recordTelemetryEvent("mirror", "ignore", "tombstone",
                                { why: "root" });
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

    for (let row of orphanRows) {
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
    let itemRows = await this.db.execute(`
      SELECT v.guid, IFNULL(s.parentGuid, :unfiledGuid) AS parentGuid,
             IFNULL(s.position, -1) AS position, v.serverModified, v.kind,
             v.needsMerge
      FROM items v
      LEFT JOIN structure s ON s.guid = v.guid
      WHERE NOT v.isDeleted AND
            v.guid <> :rootGuid
      ORDER BY parentGuid, position = -1, position, v.guid`,
      { rootGuid: PlacesUtils.bookmarks.rootGuid,
        unfiledGuid: PlacesUtils.bookmarks.unfiledGuid });

    let pseudoTree = new Map();
    for (let row of itemRows) {
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
    inflateTree(remoteTree, pseudoTree, PlacesUtils.bookmarks.rootGuid);

    // Note tombstones for remotely deleted items.
    let tombstoneRows = await this.db.execute(`
      SELECT guid FROM items
      WHERE isDeleted AND
            needsMerge`);

    for (let row of tombstoneRows) {
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

    for (let row of rows) {
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
        WHERE b.guid IN (:menuGuid, :toolbarGuid, :unfiledGuid, :mobileGuid)
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
             b.lastModified, b.syncChangeCounter
      FROM moz_bookmarks b
      JOIN moz_bookmarks p ON p.id = b.parent
      JOIN syncedItems s ON s.id = b.id
      ORDER BY s.level, b.parent, b.position`,
      { menuGuid: PlacesUtils.bookmarks.menuGuid,
        toolbarGuid: PlacesUtils.bookmarks.toolbarGuid,
        unfiledGuid: PlacesUtils.bookmarks.unfiledGuid,
        mobileGuid: PlacesUtils.bookmarks.mobileGuid,
        bookmarkType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        queryKind: SyncedBookmarksMirror.KIND.QUERY,
        bookmarkKind: SyncedBookmarksMirror.KIND.BOOKMARK,
        folderType: PlacesUtils.bookmarks.TYPE_FOLDER,
        feedURLAnno: PlacesUtils.LMANNO_FEEDURI,
        livemarkKind: SyncedBookmarksMirror.KIND.LIVEMARK,
        folderKind: SyncedBookmarksMirror.KIND.FOLDER,
        separatorKind: SyncedBookmarksMirror.KIND.SEPARATOR });

    for (let row of itemRows) {
      let parentGuid = row.getResultByName("parentGuid");
      let node = BookmarkNode.fromLocalRow(row, localTimeSeconds);
      localTree.insert(parentGuid, node);
    }

    // Note tombstones for locally deleted items.
    let tombstoneRows = await this.db.execute(`
      SELECT guid FROM moz_bookmarks_deleted`);

    for (let row of tombstoneRows) {
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
   * Fetches content info for all NEW local items that don't exist in the
   * mirror. We'll try to dedupe them to changed items with similar contents and
   * different GUIDs in the mirror.
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

    for (let row of rows) {
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
   * tree, rewrites tag queries, and updates Places to match the merged tree.
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

    MirrorLog.debug("Rewriting tag queries in mirror");
    await this.rewriteRemoteTagQueries();

    MirrorLog.debug("Inserting new URLs into Places");
    await this.db.execute(`
      INSERT OR IGNORE INTO moz_places(url, url_hash, rev_host, hidden,
                                       frecency, guid)
      SELECT u.url, u.hash, u.revHost, 0,
             (CASE SUBSTR(u.url, 1, 6) WHEN 'place:' THEN 0 ELSE -1 END),
             IFNULL(h.guid, u.guid)
      FROM items v
      JOIN urls u ON u.id = v.urlId
      LEFT JOIN moz_places h ON h.url_hash = u.hash AND
                                h.url = u.url
      JOIN mergeStates r ON r.mergedGuid = v.guid
      WHERE r.valueState = :valueState`,
      { valueState: BookmarkMergeState.TYPE.REMOTE });
    await this.db.execute(`DELETE FROM moz_updatehostsinsert_temp`);

    // Deleting from `newRemoteItems` fires the `insertNewLocalItems` and
    // `updateExistingLocalItems` triggers.
    MirrorLog.debug("Updating value states for local bookmarks");
    await this.db.execute(`DELETE FROM newRemoteItems`);

    // Update the structure. The mirror stores structure info in a separate
    // table, like iOS, while Places stores structure info on children. We don't
    // check the parent's merge state here because our merged tree might
    // diverge from the server if we're missing children, or moved children
    // without parents to "unfiled". In that case, we *don't* want to reupload
    // the new local structure to the server.
    MirrorLog.debug("Updating structure states for local bookmarks");
    await this.db.execute(`DELETE FROM newRemoteStructure`);

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
   * Creates local tag folders mentioned in remotely changed tag queries, then
   * rewrites the query URLs in the mirror to point to the new local folders.
   *
   * This can be removed once bug 1293445 lands.
   */
  async rewriteRemoteTagQueries() {
    // Create local tag folders that don't already exist. This fires the
    // `tagLocalPlace` trigger.
    await this.db.execute(`
      INSERT INTO localTags(tag)
      SELECT v.tagFolderName FROM items v
      JOIN mergeStates r ON r.mergedGuid = v.guid
      WHERE r.valueState = :valueState AND
            v.tagFolderName NOT NULL`,
      { valueState: BookmarkMergeState.TYPE.REMOTE });

    let queryRows = await this.db.execute(`
      SELECT u.id AS urlId, u.url, b.id AS newTagFolderId FROM urls u
      JOIN items v ON v.urlId = u.id
      JOIN mergeStates r ON r.mergedGuid = v.guid
      JOIN moz_bookmarks b ON b.title = v.tagFolderName
      JOIN moz_bookmarks p ON p.id = b.parent
      WHERE p.guid = :tagsGuid AND
            r.valueState = :valueState AND
            v.kind = :queryKind AND
            v.tagFolderName NOT NULL`,
      { tagsGuid: PlacesUtils.bookmarks.tagsGuid,
        valueState: BookmarkMergeState.TYPE.REMOTE,
        queryKind: SyncedBookmarksMirror.KIND.QUERY });

    let urlsParams = [];
    for (let row of queryRows) {
      let url = new URL(row.getResultByName("url"));
      let tagQueryParams = new URLSearchParams(url.pathname);
      let type = Number(tagQueryParams.get("type"));
      if (type != Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_CONTENTS) {
        continue;
      }

      // Rewrite the query URL to point to the new folder.
      let newTagFolderId = row.getResultByName("newTagFolderId");
      tagQueryParams.set("folder", newTagFolderId);

      let newURLHref = url.protocol + tagQueryParams;
      urlsParams.push({
        urlId: row.getResultByName("urlId"),
        url: newURLHref,
      });
    }

    if (urlsParams.length) {
      await this.db.execute(`
        UPDATE urls SET
          url = :url,
          hash = hash(:url)
        WHERE id = :urlId`,
        urlsParams);
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
    for (let row of removedItemRows) {
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
      JOIN mergeStates r ON r.mergedGuid = b.guid
      ORDER BY r.level, p.id, b.position`);
    for (let row of changedGuidRows) {
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
    // We `LEFT JOIN` to `mergeStates` because `itemsAdded` may include tag
    // folders and entries, which are not part of the merged tree structure, and
    // so don't exist in `mergeStates`.
    let newItemRows = await this.db.execute(`
      SELECT b.id, p.id AS parentId, b.position, b.type, h.url,
             IFNULL(b.title, "") AS title, b.dateAdded, b.guid,
             p.guid AS parentGuid, n.isTagging
      FROM itemsAdded n
      JOIN moz_bookmarks b ON b.guid = n.guid
      JOIN moz_bookmarks p ON p.id = b.parent
      LEFT JOIN moz_places h ON h.id = b.fk
      LEFT JOIN mergeStates r ON r.mergedGuid = b.guid
      ORDER BY r.level, p.id, b.position`);
    for (let row of newItemRows) {
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
      JOIN mergeStates r ON r.mergedGuid = b.guid
      ORDER BY r.level, newParentId, newPosition`);
    for (let row of movedItemRows) {
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
      JOIN mergeStates r ON r.mergedGuid = b.guid
      LEFT JOIN moz_places h ON h.id = b.fk
      LEFT JOIN moz_places i ON i.id = c.oldPlaceId
      ORDER BY r.level, p.id, b.position`);
    for (let row of changedItemRows) {
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
    for (let row of annoRows) {
      let id = row.getResultByName("itemId");
      let name = row.getResultByName("annoName");
      if (row.getResultByName("wasRemoved")) {
        observersToNotify.noteAnnoRemoved(id, name);
      } else {
        observersToNotify.noteAnnoSet(id, name);
      }
    }

    MirrorLog.debug("Recording notifications for changed keywords");
    // `ORDER BY k.ROWID` replays additions and deletions for the same keyword
    // or URL in order.
    let changedKeywordRows = await this.db.execute(`
      SELECT b.id, IFNULL(k.keyword, "") AS keyword, b.lastModified, b.type,
             p.id AS parentId, b.guid, p.guid AS parentGuid, h.url
      FROM keywordsChanged k
      JOIN moz_bookmarks b ON b.id = k.itemId
      JOIN moz_bookmarks p ON p.id = b.parent
      JOIN moz_places h ON h.id = k.placeId
      ORDER BY k.ROWID`);
    for (let row of changedKeywordRows) {
      let info = {
        id: row.getResultByName("id"),
        keyword: row.getResultByName("keyword"),
        lastModified: row.getResultByName("lastModified"),
        type: row.getResultByName("type"),
        parentId: row.getResultByName("parentId"),
        guid: row.getResultByName("guid"),
        parentGuid: row.getResultByName("parentGuid"),
        urlHref: row.getResultByName("url"),
      };
      observersToNotify.noteKeywordChanged(info);
    }
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
   */
  async stageItemsToUpload() {
    // Stage all locally changed items for upload, along with any remotely
    // changed records with older local creation dates. These are tracked
    // "weakly", in the in-memory table only. If the upload is interrupted
    // or fails, we won't reupload the record on the next sync.
    await this.db.execute(`
      WITH RECURSIVE
      syncedItems(id, level) AS (
        SELECT b.id, 0 AS level FROM moz_bookmarks b
        WHERE b.guid IN (:menuGuid, :toolbarGuid, :unfiledGuid, :mobileGuid)
        UNION ALL
        SELECT b.id, s.level + 1 AS level FROM moz_bookmarks b
        JOIN syncedItems s ON s.id = b.parent
      ),
      annos(itemId, name, content) AS (
        SELECT a.item_id, n.name, a.content FROM moz_items_annos a
        JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
      )
      INSERT INTO itemsToUpload(guid, syncChangeCounter, parentGuid,
                                parentTitle, dateAdded, type, title, isQuery,
                                url, tags, description, loadInSidebar,
                                smartBookmarkName, keyword, feedURL, siteURL,
                                position)
      SELECT b.guid, b.syncChangeCounter, p.guid, p.title, b.dateAdded, b.type,
             b.title, IFNULL(SUBSTR(h.url, 1, 6) = 'place:', 0), h.url,
             (SELECT GROUP_CONCAT(t.title, ',') FROM moz_bookmarks e
              JOIN moz_bookmarks t ON t.id = e.parent
              JOIN moz_bookmarks r ON r.id = t.parent
              WHERE r.guid = :tagsGuid AND
                    e.fk = h.id),
             (SELECT content FROM annos WHERE itemId = b.id AND
                                              name = :descriptionAnno),
             IFNULL((SELECT content FROM annos WHERE itemId = b.id AND
                                                     name = :sidebarAnno), 0),
             (SELECT content FROM annos WHERE itemId = b.id AND
                                              name = :smartBookmarkAnno),
             (SELECT keyword FROM moz_keywords WHERE place_id = h.id),
             (SELECT content FROM annos WHERE itemId = b.id AND
                                              name = :feedURLAnno),
             (SELECT content FROM annos WHERE itemId = b.id AND
                                              name = :siteURLAnno),
             b.position
      FROM moz_bookmarks b
      JOIN moz_bookmarks p ON p.id = b.parent
      JOIN syncedItems s ON s.id = b.id
      LEFT JOIN moz_places h ON h.id = b.fk
      JOIN mergeStates r ON r.mergedGuid = b.guid
      LEFT JOIN items v ON v.guid = r.mergedGuid
      WHERE b.syncChangeCounter >= 1 OR
            (r.valueState = :valueState AND
              b.dateAdded < v.dateAdded)`,
      { menuGuid: PlacesUtils.bookmarks.menuGuid,
        toolbarGuid: PlacesUtils.bookmarks.toolbarGuid,
        unfiledGuid: PlacesUtils.bookmarks.unfiledGuid,
        mobileGuid: PlacesUtils.bookmarks.mobileGuid,
        tagsGuid: PlacesUtils.bookmarks.tagsGuid,
        descriptionAnno: PlacesSyncUtils.bookmarks.DESCRIPTION_ANNO,
        sidebarAnno: PlacesSyncUtils.bookmarks.SIDEBAR_ANNO,
        smartBookmarkAnno: PlacesSyncUtils.bookmarks.SMART_BOOKMARKS_ANNO,
        feedURLAnno: PlacesUtils.LMANNO_FEEDURI,
        siteURLAnno: PlacesUtils.LMANNO_SITEURI,
        valueState: BookmarkMergeState.TYPE.REMOTE });

    // Record tag folder names for tag queries. Parsing query URLs one by one
    // is inefficient, but queries aren't common today, and we can remove this
    // logic entirely once bug 1293445 lands.
    let queryRows = await this.db.execute(`
      SELECT guid, url FROM itemsToUpload
      WHERE isQuery`);

    let tagFolderNameParams = [];
    for (let row of queryRows) {
      let url = new URL(row.getResultByName("url"));
      let tagQueryParams = new URLSearchParams(url.pathname);
      let type = Number(tagQueryParams.get("type"));
      if (type == Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_CONTENTS) {
        continue;
      }
      let tagFolderId = Number(tagQueryParams.get("folder"));
      tagFolderNameParams.push({
        guid: row.getResultByName("guid"),
        tagFolderId,
        folderType: PlacesUtils.bookmarks.TYPE_FOLDER,
      });
    }

    if (tagFolderNameParams.length) {
      await this.db.execute(`
        UPDATE itemsToUpload SET
          tagFolderName = (SELECT b.title FROM moz_bookmarks b
                           WHERE b.id = :tagFolderId AND
                                 b.type = :folderType)
        WHERE guid = :guid`);
    }

    // Record the child GUIDs of locally changed folders, which we use to
    // populate the `children` array in the record.
    await this.db.execute(`
      INSERT INTO structureToUpload(guid, parentGuid, position)
      SELECT b.guid, p.guid, b.position FROM moz_bookmarks b
      JOIN moz_bookmarks p ON p.id = b.parent
      JOIN itemsToUpload o ON o.guid = p.guid`);

    // Finally, stage tombstones for deleted items. Ignore conflicts if we have
    // tombstones for undeleted items; Places Maintenance should clean these up.
    await this.db.execute(`
      INSERT OR IGNORE INTO itemsToUpload(guid, syncChangeCounter, isDeleted,
                                          dateAdded)
      SELECT guid, 1, 1, dateRemoved FROM moz_bookmarks_deleted`);
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

    let itemRows = await this.db.execute(`
      SELECT syncChangeCounter, guid, isDeleted, type, isQuery,
             smartBookmarkName, IFNULL(tagFolderName, "") AS tagFolderName,
             loadInSidebar, keyword, tags, url, IFNULL(title, "") AS title,
             description, feedURL, siteURL, position, parentGuid,
             IFNULL(parentTitle, "") AS parentTitle, dateAdded
      FROM itemsToUpload`);

    for (let row of itemRows) {
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
      let dateAdded = PlacesUtils.toDate(
        row.getResultByName("dateAdded")).getTime();

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
              // Older Desktops use `hasDupe` and `parentName` for deduping.
              hasDupe: false,
              parentName: row.getResultByName("parentTitle"),
              dateAdded,
              bmkUri: row.getResultByName("url"),
              title: row.getResultByName("title"),
              queryId: row.getResultByName("smartBookmarkName"),
              folderName: row.getResultByName("tagFolderName"),
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
            hasDupe: false,
            parentName: row.getResultByName("parentTitle"),
            dateAdded,
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
              hasDupe: false,
              parentName: row.getResultByName("parentTitle"),
              dateAdded,
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
            hasDupe: false,
            parentName: row.getResultByName("parentTitle"),
            dateAdded,
            title: row.getResultByName("title"),
          };
          let description = row.getResultByName("description");
          if (description) {
            folderCleartext.description = description;
          }
          let childGuidRows = await this.db.executeCached(`
            SELECT guid FROM structureToUpload
            WHERE parentGuid = :guid
            ORDER BY position`,
            { guid });
          folderCleartext.children = childGuidRows.map(row => {
            let childGuid = row.getResultByName("guid");
            return PlacesSyncUtils.bookmarks.guidToRecordId(childGuid);
          });
          changeRecords[recordId] = new BookmarkChangeRecord(
            syncChangeCounter, folderCleartext);
          continue;
        }

        case PlacesUtils.bookmarks.TYPE_SEPARATOR: {
          let separatorCleartext = {
            id: recordId,
            type: "separator",
            parentid: parentRecordId,
            hasDupe: false,
            parentName: row.getResultByName("parentTitle"),
            dateAdded,
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

/** Valid key types for the key-value `meta` table. */
SyncedBookmarksMirror.META = {
  MODIFIED: 1,
};

/**
 * An error thrown when the merge can't proceed because the local or remote
 * tree is inconsistent.
 */
SyncedBookmarksMirror.ConsistencyError =
  class ConsistencyError extends Error {};

// Indicates if the mirror should be replaced because the database file is
// corrupt.
function isDatabaseCorrupt(error) {
  return error instanceof Ci.mozIStorageError &&
         (error.result == Ci.mozIStorageError.CORRUPT ||
          error.result == Ci.mozIStorageError.NOTADB);
}

/**
 * Migrates the mirror database schema to the latest version.
 *
 * @param {Sqlite.OpenedConnection} db
 *        The mirror database connection.
 */
function migrateMirrorSchema(db) {
  return db.executeTransaction(async function() {
    let currentSchemaVersion = await db.getSchemaVersion("mirror");
    if (currentSchemaVersion < 1) {
      await initializeMirrorDatabase(db);
    }
    // Downgrading from a newer profile to an older profile rolls back the
    // schema version, but leaves all new columns in place. We'll run the
    // migration logic again on the next upgrade.
    await db.setSchemaVersion(MIRROR_SCHEMA_VERSION, "mirror");
  });
}

/**
 * Initializes a new mirror database, creating persistent tables, indexes, and
 * roots.
 *
 * @param {Sqlite.OpenedConnection} db
 *        The mirror database connection.
 */
async function initializeMirrorDatabase(db) {
  // Key-value metadata table. Currently stores just the server collection
  // last modified time.
  await db.execute(`CREATE TABLE mirror.meta(
    key INTEGER PRIMARY KEY,
    value NOT NULL
    CHECK(key = ${SyncedBookmarksMirror.META.MODIFIED})
  )`);

  await db.execute(`CREATE TABLE mirror.items(
    id INTEGER PRIMARY KEY,
    guid TEXT UNIQUE NOT NULL,
    /* The server modified time, in milliseconds. */
    serverModified INTEGER NOT NULL DEFAULT 0,
    needsMerge BOOLEAN NOT NULL DEFAULT 0,
    isDeleted BOOLEAN NOT NULL DEFAULT 0,
    kind INTEGER NOT NULL DEFAULT -1,
    /* The creation date, in microseconds. */
    dateAdded INTEGER NOT NULL DEFAULT 0,
    title TEXT,
    urlId INTEGER REFERENCES urls(id)
                  ON DELETE SET NULL,
    keyword TEXT,
    tagFolderName TEXT,
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
 * Sets up the syncable roots. All items in the mirror should descend from these
 * roots. If we ever add new syncable roots to Places, this function should also
 * be updated to create them in the mirror.
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
  }, {
    guid: PlacesUtils.bookmarks.menuGuid,
    parentGuid: PlacesUtils.bookmarks.rootGuid,
    position: 0,
  }, {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    parentGuid: PlacesUtils.bookmarks.rootGuid,
    position: 1,
  }, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    parentGuid: PlacesUtils.bookmarks.rootGuid,
    position: 2,
  }, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    parentGuid: PlacesUtils.bookmarks.rootGuid,
    position: 3,
  }];
  for (let info of syncableRoots) {
    await db.executeCached(`
      INSERT INTO items(guid, kind)
      VALUES(:guid, :kind)`,
      { guid: info.guid, kind: SyncedBookmarksMirror.KIND.FOLDER });

    await db.executeCached(`
      INSERT INTO structure(guid, parentGuid, position)
      VALUES(:guid, :parentGuid, :position)`,
      info);
  }
}

/**
 * Creates temporary tables, views, and triggers to apply the mirror to Places.
 *
 * The bulk of the logic to apply all remotely changed items is defined in
 * `INSTEAD OF DELETE` triggers on the `newRemoteItems` and `newRemoteStructure`
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
    columnName: "description",
    type: PlacesUtils.annotations.TYPE_STRING,
  }, {
    annoName: PlacesSyncUtils.bookmarks.SIDEBAR_ANNO,
    columnName: "loadInSidebar",
    type: PlacesUtils.annotations.TYPE_INT32,
  }, {
    annoName: PlacesSyncUtils.bookmarks.SMART_BOOKMARKS_ANNO,
    columnName: "smartBookmarkName",
    type: PlacesUtils.annotations.TYPE_STRING,
  }, {
    annoName: PlacesUtils.LMANNO_FEEDURI,
    columnName: "feedURL",
    type: PlacesUtils.annotations.TYPE_STRING,
  }, {
    annoName: PlacesUtils.LMANNO_SITEURI,
    columnName: "siteURL",
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
    CREATE TEMP VIEW newRemoteItems(localId, remoteId, localGuid, mergedGuid,
                                    needsUpdate, type, dateAdded, title,
                                    oldPlaceId, newPlaceId, newKeyword,
                                    description, loadInSidebar,
                                    smartBookmarkName, feedURL, siteURL,
                                    syncChangeCounter) AS
    SELECT b.id, v.id, r.localGuid, r.mergedGuid,
           r.valueState = ${BookmarkMergeState.TYPE.REMOTE},
           (CASE WHEN v.kind IN (${[
                        SyncedBookmarksMirror.KIND.BOOKMARK,
                        SyncedBookmarksMirror.KIND.QUERY,
                      ].join(",")}) THEN ${PlacesUtils.bookmarks.TYPE_BOOKMARK}
                 WHEN v.kind IN (${[
                        SyncedBookmarksMirror.KIND.FOLDER,
                        SyncedBookmarksMirror.KIND.LIVEMARK,
                      ].join(",")}) THEN ${PlacesUtils.bookmarks.TYPE_FOLDER}
                 ELSE ${PlacesUtils.bookmarks.TYPE_SEPARATOR} END),
           (CASE WHEN b.dateAdded < v.dateAdded THEN b.dateAdded
                 ELSE v.dateAdded END),
           v.title, h.id, u.newPlaceId, v.keyword, v.description,
           v.loadInSidebar, v.smartBookmarkName, v.feedURL, v.siteURL,
           (CASE r.structureState WHEN ${BookmarkMergeState.TYPE.REMOTE} THEN 0
            ELSE 1 END)
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
  // refers to the row for the unmerged item in `newRemoteItems`.
  await db.execute(`
    CREATE TEMP TRIGGER mergeGuids
    INSTEAD OF DELETE ON newRemoteItems
    BEGIN
      /* We update GUIDs here, instead of in the "updateExistingLocalItems"
         trigger, because deduped items where we're keeping the local value
         state won't have "needsMerge" set. */
      UPDATE moz_bookmarks SET
        guid = OLD.mergedGuid
      WHERE OLD.localGuid <> OLD.mergedGuid AND
            guid = OLD.localGuid;

      /* Record item changed notifications for the updated GUIDs. */
      INSERT INTO guidsChanged(itemId, oldGuid)
      SELECT OLD.localId, OLD.localGuid
      WHERE OLD.localGuid <> OLD.mergedGuid;

      DELETE FROM moz_bookmarks_deleted WHERE guid = OLD.mergedGuid;

      /* Flag the remote item as merged. */
      UPDATE items SET
        needsMerge = 0
      WHERE needsMerge AND
            guid = OLD.mergedGuid;
    END`);

  // Inserts items from the mirror that don't exist locally.
  await db.execute(`
    CREATE TEMP TRIGGER insertNewLocalItems
    INSTEAD OF DELETE ON newRemoteItems WHEN OLD.localId IS NULL
    BEGIN
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

      /* Record item changed notifications for existing items with the new
         keyword and URL. */
      INSERT INTO keywordsChanged(itemId, placeId, keyword)
      SELECT b.id, b.fk, NULL FROM moz_bookmarks b
      JOIN moz_keywords k ON k.place_id = b.fk
      WHERE k.place_id = OLD.newPlaceId OR
            k.keyword = OLD.newKeyword;

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
      VALUES(OLD.mergedGuid, -1, -1, OLD.type, OLD.newPlaceId, OLD.title,
             OLD.dateAdded, STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000,
             ${PlacesUtils.bookmarks.SYNC_STATUS.NORMAL},
             OLD.syncChangeCounter);

      /* Record an item added notification for the new item. */
      INSERT INTO itemsAdded(guid)
      VALUES(OLD.mergedGuid);

      /* Insert new keywords after the item, so that "noteKeywordAdded" can find
         the new item by Place ID. */
      INSERT INTO moz_keywords(keyword, place_id)
      SELECT OLD.newKeyword, OLD.newPlaceId
      WHERE OLD.newKeyword NOT NULL;

      /* Record item changed notifications for the new keyword. */
      INSERT INTO keywordsChanged(itemId, placeId, keyword)
      SELECT b.id, OLD.newPlaceId, OLD.newKeyword FROM moz_bookmarks b
      WHERE b.guid = OLD.mergedGuid AND
            OLD.newKeyword NOT NULL;

      /* Insert new tags for the URL. */
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
                WHERE guid = OLD.mergedGuid),
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
        WHERE b.guid = OLD.mergedGuid AND
              OLD.${annoTrigger.columnName} NOT NULL;
      `).join("")}
    END`);

  // Updates existing items with new values from the mirror.
  await db.execute(`
    CREATE TEMP TRIGGER updateExistingLocalItems
    INSTEAD OF DELETE ON newRemoteItems WHEN OLD.needsUpdate AND
                                             OLD.localId NOT NULL
    BEGIN
      /* Record item changed notifications for the title and URL. */
      INSERT INTO itemsChanged(itemId, oldTitle, oldPlaceId)
      SELECT id, title, OLD.oldPlaceId FROM moz_bookmarks
      WHERE id = OLD.localId;

      UPDATE moz_bookmarks SET
        title = OLD.title,
        dateAdded = OLD.dateAdded,
        lastModified = STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000,
        syncStatus = ${PlacesUtils.bookmarks.SYNC_STATUS.NORMAL},
        syncChangeCounter = OLD.syncChangeCounter
      WHERE id = OLD.localId;

      /* Bump the change counter for items with the old URL, new URL, and new
         keyword. */
      UPDATE moz_bookmarks SET
        syncChangeCounter = syncChangeCounter + 1
      WHERE fk IN (SELECT place_id FROM moz_keywords
                   WHERE place_id IN (OLD.oldPlaceId, OLD.newPlaceId) OR
                         keyword = OLD.newKeyword);

      /* Record change observer notifications for items with the old URL, new
         URL, and new keyword. This is identical to the subquery above; we just
         query the item ID instead of updating by the Place ID. */
      INSERT INTO keywordsChanged(itemId, placeId, keyword)
      SELECT b.id, b.fk, NULL FROM moz_bookmarks b
      JOIN moz_keywords k ON k.place_id = b.fk
      WHERE k.place_id IN (OLD.oldPlaceId, OLD.newPlaceId) OR
            k.keyword = OLD.newKeyword;

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
      INSERT INTO moz_keywords(keyword, place_id)
      SELECT OLD.newKeyword, OLD.newPlaceId
      WHERE OLD.newKeyword NOT NULL;

      /* Record an item changed notification for the new keyword. */
      INSERT INTO keywordsChanged(itemId, placeId, keyword)
      SELECT OLD.localId, OLD.newPlaceId, OLD.newKeyword
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

  // A view of the new structure state for all items in the merged tree. The
  // mirror stores structure info in a separate table, like iOS, while Places
  // stores structure info on children. Unlike iOS, we can't simply check the
  // parent's merge state to know if its children changed. This is because our
  // merged tree might diverge from the mirror if we're missing children, or if
  // we temporarily reparented children without parents to "unfiled". In that
  // case, we want to keep syncing, but *don't* want to reupload the new local
  // structure to the server.
  await db.execute(`
    CREATE TEMP VIEW newRemoteStructure(localId, oldParentId, newParentId,
                                        oldPosition, newPosition) AS
    SELECT b.id, b.parent, p.id, b.position, r.position
    FROM moz_bookmarks b
    JOIN mergeStates r ON r.mergedGuid = b.guid
    JOIN moz_bookmarks p ON p.guid = r.parentGuid
    WHERE r.parentGuid <> '${PlacesUtils.bookmarks.rootGuid}'`);

  // Updates all parents and positions to reflect the merged tree.
  await db.execute(`
    CREATE TEMP TRIGGER updateLocalStructure
    INSTEAD OF DELETE ON newRemoteStructure
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
      INSERT INTO itemsMoved(itemId, oldParentId, oldParentGuid, oldPosition)
      SELECT OLD.localId, OLD.oldParentId, p.guid, OLD.oldPosition
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
  // untitled bookmarks under these folders. This complexity, along with tag
  // query rewriting, can be removed once bug 1293445 lands.
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
    isTagging BOOLEAN NOT NULL DEFAULT 0
  ) WITHOUT ROWID`);

  await db.execute(`CREATE TEMP TABLE guidsChanged(
    itemId INTEGER NOT NULL,
    oldGuid TEXT NOT NULL,
    PRIMARY KEY(itemId, oldGuid)
  ) WITHOUT ROWID`);

  await db.execute(`CREATE TEMP TABLE itemsChanged(
    itemId INTEGER PRIMARY KEY,
    oldTitle TEXT,
    oldPlaceId INTEGER
  )`);

  await db.execute(`CREATE TEMP TABLE itemsMoved(
    itemId INTEGER PRIMARY KEY,
    oldParentId INTEGER NOT NULL,
    oldParentGuid TEXT NOT NULL,
    oldPosition INTEGER NOT NULL
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

  // Stores properties to pass to `onItemChanged` observers for new and removed
  // keywords. A NULL keyword means we're removing all keywords from a Place.
  // Note that an item may appear multiple times in this table, so `itemId` is
  // not a primary key.
  await db.execute(`CREATE TEMP TABLE keywordsChanged(
    itemId INTEGER NOT NULL,
    placeId INTEGER NOT NULL,
    keyword TEXT
  )`);

  // Stores locally changed items staged for upload. See `stageItemsToUpload`
  // for an explanation of why these tables exists.
  await db.execute(`CREATE TEMP TABLE itemsToUpload(
    guid TEXT PRIMARY KEY,
    syncChangeCounter INTEGER NOT NULL,
    isDeleted BOOLEAN NOT NULL DEFAULT 0,
    parentGuid TEXT,
    parentTitle TEXT,
    dateAdded INTEGER,
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
  ) WITHOUT ROWID`);

  await db.execute(`CREATE TEMP TABLE structureToUpload(
    guid TEXT PRIMARY KEY,
    parentGuid TEXT NOT NULL REFERENCES itemsToUpload(guid)
                             ON DELETE CASCADE,
    position INTEGER NOT NULL
  ) WITHOUT ROWID`);
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
  let dateAdded = PlacesSyncUtils.bookmarks.ratchetTimestampBackwards(
    record.dateAdded, serverModified);
  return dateAdded ? PlacesUtils.toPRTime(new Date(dateAdded)) : 0;
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
function inflateTree(tree, pseudoTree, parentGuid) {
  let nodes = pseudoTree.get(parentGuid);
  if (nodes) {
    for (let node of nodes) {
      tree.insert(parentGuid, node);
      inflateTree(tree, pseudoTree, node.guid);
    }
  }
}

/**
 * Content info for an item in the local or remote tree. This is used to dedupe
 * NEW local items to remote items that don't exist locally. See `contentsMatch`
 * for how we determine if two items are dupes.
 */
class BookmarkContent {
  constructor(title, urlHref, smartBookmarkName, position) {
    this.title = title;
    this.url = urlHref ? new URL(urlHref) : null;
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

  hasSameURL(otherContent) {
    return !!this.url == !!otherContent.url &&
           this.url.href == otherContent.url.href;
  }
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
        return "V: L";
      case BookmarkMergeState.TYPE.REMOTE:
        return "V: R";
    }
    return "V: ?";
  }

  structureToString() {
    switch (this.structure()) {
      case BookmarkMergeState.TYPE.LOCAL:
        return "S: L";
      case BookmarkMergeState.TYPE.REMOTE:
        return "S: R";
      case BookmarkMergeState.TYPE.NEW:
        // We intentionally don't log the new structure node here, since
        // the merger already does that.
        return "S: +";
    }
    return "S: ?";
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
    // is in *microseconds*.
    let localModified = row.getResultByName("lastModified");
    let age = Math.max(localTimeSeconds - localModified / 1000000, 0) || 0;

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

    // `serverModified` is in *milliseconds*.
    let serverModified = row.getResultByName("serverModified");
    let age = Math.max(remoteTimeSeconds - serverModified / 1000, 0) || 0;

    let kind = row.getResultByName("kind");
    let needsMerge = !!row.getResultByName("needsMerge");

    return new BookmarkNode(guid, age, kind, needsMerge);
  }

  isRoot() {
    return this.guid == PlacesUtils.bookmarks.rootGuid ||
           PlacesUtils.bookmarks.userContentRoots.includes(this.guid);
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
   * @return {String}
   *         A string in the form of "bookmarkAAAA (B; 1.234s; !)", where
   *         "B" is the kind, "1.234s" is the age, and "!" indicates that the
   *         node needs to be merged.
   */
  toString() {
    let info = `${this.kindToString()}; ${this.age.toFixed(3)}s`;
    if (this.needsMerge) {
      info += "; !";
    }
    return `${this.guid} (${info})`;
  }

  kindToString() {
    switch (this.kind) {
      case SyncedBookmarksMirror.KIND.BOOKMARK:
        return "B";
      case SyncedBookmarksMirror.KIND.QUERY:
        return "Q";
      case SyncedBookmarksMirror.KIND.FOLDER:
        return "F";
      case SyncedBookmarksMirror.KIND.LIVEMARK:
        return "L";
      case SyncedBookmarksMirror.KIND.SEPARATOR:
        return "S";
    }
    return "?";
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

  * guids() {
    for (let [guid, node] of this.byGuid) {
      if (node == this.root) {
        continue;
      }
      yield guid;
    }
    for (let guid of this.deletedGuids) {
      yield guid;
    }
  }

  /**
   * Generates an ASCII art representation of the complete tree. Deleted GUIDs
   * are prefixed with "~".
   *
   * @return {String}
   */
  toASCIITreeString() {
    return this.root.toASCIITreeString() + "\n" + Array.from(this.deletedGuids,
      guid => `~${guid}`
    ).join(", ");
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
  toBookmarkNode() {
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
      for (let mergedChildNode of this.mergedChildren) {
        newNode.children.push(mergedChildNode.toBookmarkNode());
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
    this.mergedGuids = new Set();
    this.deleteLocally = new Set();
    this.deleteRemotely = new Set();
    this.telemetryEvents = [];
  }

  merge() {
    let localRoot = this.localTree.nodeForGuid(PlacesUtils.bookmarks.rootGuid);
    let remoteRoot = this.remoteTree.nodeForGuid(PlacesUtils.bookmarks.rootGuid);
    let mergedRoot = this.mergeNode(PlacesUtils.bookmarks.rootGuid, localRoot,
                                    remoteRoot);

    // Any remaining deletions on one side should be deleted on the other side.
    // This happens when the remote tree has tombstones for items that don't
    // exist in Places, or Places has tombstones for items that aren't on the
    // server.
    for (let guid of this.localTree.deletedGuids) {
      if (!this.mentions(guid)) {
        this.deleteRemotely.add(guid);
      }
    }
    for (let guid of this.remoteTree.deletedGuids) {
      if (!this.mentions(guid)) {
        this.deleteLocally.add(guid);
      }
    }
    return mergedRoot;
  }

  subsumes(tree) {
    for (let guid of tree.guids()) {
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
  mergeNode(mergedGuid, localNode, remoteNode) {
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
        let mergedNode = this.twoWayMerge(mergedGuid, localNode, remoteNode);
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
        this.mergeChildListsIntoMergedNode(mergedNode, localNode,
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
        this.mergeChildListsIntoMergedNode(mergedNode, /* localNode */ null,
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
  twoWayMerge(mergedGuid, localNode, remoteNode) {
    let mergeState = this.resolveTwoWayValueConflict(mergedGuid, localNode,
                                                     remoteNode);
    MirrorLog.trace("Merge state for ${mergedGuid} is ${mergeState}",
                    { mergedGuid, mergeState });

    let mergedNode = new MergedBookmarkNode(mergedGuid, localNode, remoteNode,
                                            mergeState);

    if (localNode.isFolder()) {
      if (remoteNode.isFolder()) {
        // Merging two folders, so we need to walk their children to handle
        // structure changes.
        MirrorLog.trace("Merging folders ${localNode} and ${remoteNode}",
                        { localNode, remoteNode });
        this.mergeChildListsIntoMergedNode(mergedNode, localNode, remoteNode);
        return mergedNode;
      }

      if (remoteNode.kind == SyncedBookmarksMirror.KIND.LIVEMARK) {
        // We allow merging local folders and remote livemarks because Places
        // stores livemarks as empty folders with feed and site URL annotations.
        // The livemarks service first inserts the folder, and *then* sets
        // annotations. Since this isn't wrapped in a transaction, we might sync
        // before the annotations are set, and upload a folder record instead
        // of a livemark record (bug 632287), then replace the folder with a
        // livemark on the next sync.
        MirrorLog.trace("Merging local folder ${localNode} and remote " +
                        "livemark ${remoteNode}", { localNode, remoteNode });
        this.telemetryEvents.push({
          value: "kind",
          extra: { local: "folder", remote: "folder" },
        });
        return mergedNode;
      }

      MirrorLog.error("Merging local folder ${localNode} and remote " +
                      "non-folder ${remoteNode}", { localNode, remoteNode });
      throw new SyncedBookmarksMirror.ConsistencyError(
        "Can't merge folder and non-folder");
    }

    if (localNode.kind == remoteNode.kind) {
      // Merging two non-folders, so no need to walk children.
      MirrorLog.trace("Merging non-folders ${localNode} and ${remoteNode}",
                      { localNode, remoteNode });
      return mergedNode;
    }

    MirrorLog.error("Merging local ${localNode} and remote ${remoteNode} " +
                    "with different kinds", { localNode, remoteNode });
    throw new SyncedBookmarksMirror.ConsistencyError(
      "Can't merge different item kinds");
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
   * Merges a remote child node into a merged folder node.
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
  mergeRemoteChildIntoMergedNode(mergedNode, remoteParentNode,
                                 remoteChildNode) {
    if (this.mergedGuids.has(remoteChildNode.guid)) {
      MirrorLog.trace("Remote child ${remoteChildNode} already seen in " +
                      "another folder and merged", { remoteChildNode });
      return false;
    }

    MirrorLog.trace("Merging remote child ${remoteChildNode} of " +
                    "${remoteParentNode} into ${mergedNode}",
                    { remoteChildNode, remoteParentNode, mergedNode });

    // Make sure the remote child isn't locally deleted. If it is, we need
    // to move all descendants that aren't also remotely deleted to the
    // merged node. This handles the case where a user deletes a folder
    // on this device, and adds a bookmark to the same folder on another
    // device. We want to keep the folder deleted, but we also don't want
    // to lose the new bookmark, so we move the bookmark to the deleted
    // folder's parent.
    let locallyDeleted = this.checkForLocalDeletionOfRemoteNode(mergedNode,
      remoteChildNode);
    if (locallyDeleted) {
      return true;
    }

    // The remote child isn't locally deleted. Does it exist in the local tree?
    let localChildNode = this.localTree.nodeForGuid(remoteChildNode.guid);
    if (!localChildNode) {
      // Remote child doesn't exist locally, either. Try to find a content
      // match in the containing folder, and dedupe the local item if we can.
      MirrorLog.trace("Remote child ${remoteChildNode} doesn't exist " +
                      "locally; looking for content match",
                      { remoteChildNode });

      let localChildNodeByContent = this.findLocalNodeMatchingRemoteNode(
        mergedNode, remoteChildNode);

      let mergedChildNode = this.mergeNode(remoteChildNode.guid,
                                           localChildNodeByContent,
                                           remoteChildNode);
      mergedNode.mergedChildren.push(mergedChildNode);
      return false;
    }

    // Otherwise, the remote child exists in the local tree. Did it move?
    let localParentNode = this.localTree.parentNodeFor(localChildNode);
    if (!localParentNode) {
      // Should never happen. The local tree must be complete.
      MirrorLog.error("Remote child ${remoteChildNode} exists locally as " +
                      "${localChildNode} without local parent",
                      { remoteChildNode, localChildNode });
      throw new SyncedBookmarksMirror.ConsistencyError(
        "Local child node is orphan");
    }

    MirrorLog.trace("Remote child ${remoteChildNode} exists locally in " +
                    "${localParentNode} and remotely in ${remoteParentNode}",
                    { remoteChildNode, localParentNode, remoteParentNode });

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

        let mergedChildNode = this.mergeNode(remoteChildNode.guid,
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

    let mergedChildNode = this.mergeNode(remoteChildNode.guid, localChildNode,
                                         remoteChildNode);
    mergedNode.mergedChildren.push(mergedChildNode);
    return false;
  }

  /**
   * Merges a local child node into a merged folder node.
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
  mergeLocalChildIntoMergedNode(mergedNode, localParentNode, localChildNode) {
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
    // If so, we need to move any new local descendants to the merged node,
    // just as we did for new remote descendants of locally deleted parents.
    let remotelyDeleted = this.checkForRemoteDeletionOfLocalNode(mergedNode,
      localChildNode);
    if (remotelyDeleted) {
      return true;
    }

    // At this point, we know the local child isn't deleted. See if it
    // exists in the remote tree.
    let remoteChildNode = this.remoteTree.nodeForGuid(localChildNode.guid);
    if (!remoteChildNode) {
      // The local child doesn't exist remotely, but we still need to walk
      // its children.
      let mergedChildNode = this.mergeNode(localChildNode.guid, localChildNode,
                                           /* remoteChildNode */ null);
      mergedNode.mergedChildren.push(mergedChildNode);
      return true;
    }

    // The local child exists remotely. It must have moved; otherwise, we
    // would have seen it when we walked the remote children.
    let remoteParentNode = this.remoteTree.parentNodeFor(remoteChildNode);
    if (!remoteParentNode) {
      // Should never happen. The remote tree must be complete.
      MirrorLog.error("Local child ${localChildNode} exists remotely as " +
                      "${remoteChildNode} without remote parent",
                      { localChildNode, remoteChildNode });
      throw new SyncedBookmarksMirror.ConsistencyError(
        "Remote child node is orphan");
    }

    MirrorLog.trace("Local child ${localChildNode} exists locally in " +
                    "${localParentNode} and remotely in ${remoteParentNode}",
                    { localChildNode, localParentNode, remoteParentNode });

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

        let mergedChildNode = this.mergeNode(localChildNode.guid,
                                             localChildNode, remoteChildNode);
        mergedNode.mergedChildren.push(mergedChildNode);
        return true;
      }

      MirrorLog.trace("Remote parent unchanged; keeping local child " +
                      "${localChildNode} in local parent ${localParentNode}",
                      { localChildNode, localParentNode });

      let mergedChildNode = this.mergeNode(localChildNode.guid, localChildNode,
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
   *        The merged folder state. This method mutates the merged node to
   *        append merged children, and change the node's merge state to new
   *        if needed.
   * @param {BookmarkNode?} localNode
   *        The local folder node. May be `null` if the folder only exists
   *        remotely.
   * @param {BookmarkNode?} remoteNode
   *        The remote folder node. May be `null` if the folder only exists
   *        locally.
   */
  mergeChildListsIntoMergedNode(mergedNode, localNode, remoteNode) {
    let mergeStateChanged = false;

    // Walk and merge remote children first.
    MirrorLog.trace("Merging remote children of ${remoteNode} into " +
                    "${mergedNode}", { remoteNode, mergedNode });
    if (remoteNode) {
      for (let remoteChildNode of remoteNode.children) {
        let remoteChildrenChanged = this.mergeRemoteChildIntoMergedNode(
          mergedNode, remoteNode, remoteChildNode);
        if (remoteChildrenChanged) {
          mergeStateChanged = true;
        }
      }
    }

    // Now walk and merge any local children that we haven't already merged.
    MirrorLog.trace("Merging local children of ${localNode} into " +
                    "${mergedNode}", { localNode, mergedNode });
    if (localNode) {
      for (let localChildNode of localNode.children) {
        let remoteChildrenChanged = this.mergeLocalChildIntoMergedNode(
          mergedNode, localNode, localChildNode);
        if (remoteChildrenChanged) {
          mergeStateChanged = true;
        }
      }
    }

    // Update the merge state if we moved children orphaned on one side by a
    // deletion on the other side, if we kept newer locally moved children,
    // or if the child order changed. We already updated the merge state of the
    // orphans, but we also need to flag the containing folder so that it's
    // reuploaded to the server along with the new children.
    if (mergeStateChanged) {
      let newStructureNode = mergedNode.toBookmarkNode();
      let newMergeState = BookmarkMergeState.new(mergedNode.mergeState,
                                                 newStructureNode);
      MirrorLog.trace("Merge state for ${mergedNode} has new structure " +
                      "${newMergeState}", { mergedNode, newMergeState });
      this.telemetryEvents.push({
        value: "structure",
        extra: { type: "new" },
      });
      mergedNode.mergeState = newMergeState;
    }
  }

  /**
   * Walks a locally deleted remote node's children, reparenting any children
   * that aren't also deleted remotely to the merged node. Returns `true` if
   * `remoteNode` is deleted locally; `false` if `remoteNode` is not deleted or
   * doesn't exist locally.
   *
   * This is the inverse of `checkForRemoteDeletionOfLocalNode`.
   */
  checkForLocalDeletionOfRemoteNode(mergedNode, remoteNode) {
    if (!this.localTree.isDeleted(remoteNode.guid)) {
      return false;
    }

    if (remoteNode.needsMerge) {
      if (!remoteNode.isFolder()) {
        // If a non-folder child is deleted locally and changed remotely, we
        // ignore the local deletion and take the remote child.
        MirrorLog.trace("Remote non-folder ${remoteNode} deleted locally " +
                        "and changed remotely; taking remote change",
                        { remoteNode });
        this.telemetryEvents.push({
          value: "structure",
          extra: { type: "delete", kind: "item", prefer: "remote" },
        });
        return false;
      }
      // For folders, we always take the local deletion and relocate remotely
      // changed grandchildren to the merged node. We could use the mirror to
      // revive the child folder, but it's easier to relocate orphaned
      // grandchildren than to partially revive the child folder.
      MirrorLog.trace("Remote folder ${remoteNode} deleted locally " +
                      "and changed remotely; taking local deletion",
                      { remoteNode });
      this.telemetryEvents.push({
        value: "structure",
        extra: { type: "delete", kind: "folder", prefer: "local" },
      });
    } else {
      MirrorLog.trace("Remote node ${remoteNode} deleted locally and not " +
                       "changed remotely; taking local deletion",
                       { remoteNode });
    }

    this.deleteRemotely.add(remoteNode.guid);

    let mergedOrphanNodes = this.processRemoteOrphansForNode(mergedNode,
                                                             remoteNode);
    this.relocateOrphansTo(mergedNode, mergedOrphanNodes);
    MirrorLog.trace("Relocating remote orphans ${mergedOrphanNodes} to " +
                    "${mergedNode}", { mergedOrphanNodes, mergedNode });

    return true;
  }

  /**
   * Walks a remotely deleted local node's children, reparenting any children
   * that aren't also deleted locally to the merged node. Returns `true` if
   * `localNode` is deleted remotely; `false` if `localNode` is not deleted or
   * doesn't exist locally.
   *
   * This is the inverse of `checkForLocalDeletionOfRemoteNode`.
   */
  checkForRemoteDeletionOfLocalNode(mergedNode, localNode) {
    if (!this.remoteTree.isDeleted(localNode.guid)) {
      return false;
    }

    if (localNode.needsMerge) {
      if (!localNode.isFolder()) {
        MirrorLog.trace("Local non-folder ${localNode} deleted remotely and " +
                        "changed locally; taking local change", { localNode });
        this.telemetryEvents.push({
          value: "structure",
          extra: { type: "delete", kind: "item", prefer: "local" },
        });
        return false;
      }
      MirrorLog.trace("Local folder ${localNode} deleted remotely and " +
                      "changed locally; taking remote deletion", { localNode });
      this.telemetryEvents.push({
        value: "structure",
        extra: { type: "delete", kind: "folder", prefer: "remote" },
      });
    } else {
      MirrorLog.trace("Local node ${localNode} deleted remotely and not " +
                      "changed locally; taking remote deletion", { localNode });
    }

    MirrorLog.trace("Local node ${localNode} deleted remotely; taking remote " +
                    "deletion", { localNode });

    this.deleteLocally.add(localNode.guid);

    let mergedOrphanNodes = this.processLocalOrphansForNode(mergedNode,
                                                            localNode);
    this.relocateOrphansTo(mergedNode, mergedOrphanNodes);
    MirrorLog.trace("Relocating local orphans ${mergedOrphanNodes} to " +
                    "${mergedNode}", { mergedOrphanNodes, mergedNode });

    return true;
  }

  /**
   * Recursively merges all remote children of a locally deleted folder that
   * haven't also been deleted remotely. This can happen if the user adds a
   * bookmark to a folder on another device, and deletes that folder locally.
   * This is the inverse of `processLocalOrphansForNode`.
   */
  processRemoteOrphansForNode(mergedNode, remoteNode) {
    let remoteOrphanNodes = [];

    for (let remoteChildNode of remoteNode.children) {
      let locallyDeleted = this.checkForLocalDeletionOfRemoteNode(mergedNode,
        remoteChildNode);
      if (locallyDeleted) {
        // The remote child doesn't exist locally, or is also deleted locally,
        // so we can safely delete its parent.
        continue;
      }
      remoteOrphanNodes.push(remoteChildNode);
    }

    let mergedOrphanNodes = [];
    for (let remoteOrphanNode of remoteOrphanNodes) {
      let localOrphanNode = this.localTree.nodeForGuid(remoteOrphanNode.guid);
      let mergedOrphanNode = this.mergeNode(remoteOrphanNode.guid,
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
  processLocalOrphansForNode(mergedNode, localNode) {
    if (!localNode.isFolder()) {
      // The local node isn't a folder, so it won't have orphans.
      return [];
    }

    let localOrphanNodes = [];
    for (let localChildNode of localNode.children) {
      let remotelyDeleted = this.checkForRemoteDeletionOfLocalNode(mergedNode,
        localChildNode);
      if (remotelyDeleted) {
        // The local child doesn't exist or is also deleted on the server, so we
        // can safely delete its parent without orphaning any local children.
        continue;
      }
      localOrphanNodes.push(localChildNode);
    }

    let mergedOrphanNodes = [];
    for (let localOrphanNode of localOrphanNodes) {
      let remoteOrphanNode = this.remoteTree.nodeForGuid(localOrphanNode.guid);
      let mergedNode = this.mergeNode(localOrphanNode.guid,
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
   * @param {MergedBookmarkNode[]} mergedOrphanNodes
   */
  relocateOrphansTo(mergedNode, mergedOrphanNodes) {
    for (let mergedOrphanNode of mergedOrphanNodes) {
      let newStructureNode = mergedOrphanNode.toBookmarkNode();
      let newMergeState = BookmarkMergeState.new(mergedOrphanNode.mergeState,
                                                 newStructureNode);
      mergedOrphanNode.mergeState = newMergeState;
      mergedNode.mergedChildren.push(mergedOrphanNode);
    }
  }

  /**
   * Finds a local node with a different GUID that matches the content of a
   * remote node. This is used to dedupe local items that haven't been uploaded
   * to remote items that don't exist locally.
   *
   * @param  {MergedBookmarkNode} mergedNode
   *         The merged folder node.
   * @param  {BookmarkNode} remoteChildNode
   *         The remote child node.
   * @return {BookmarkNode?}
   *         A matching local child node, or `null` if there are no matching
   *         local items.
   */
  findLocalNodeMatchingRemoteNode(mergedNode, remoteChildNode) {
    let localParentNode = mergedNode.localNode;
    if (!localParentNode) {
      MirrorLog.trace("Merged node ${mergedNode} doesn't exist locally; no " +
                      "potential dupes for ${remoteChildNode}",
                      { mergedNode, remoteChildNode });
      return null;
    }
    let remoteChildContent = this.newRemoteContents.get(remoteChildNode.guid);
    if (!remoteChildContent) {
      // The node doesn't exist locally, but it's also flagged as merged in the
      // mirror.
      return null;
    }
    let newLocalNode = null;
    for (let localChildNode of localParentNode.children) {
      if (this.mergedGuids.has(localChildNode.guid)) {
        MirrorLog.trace("Not deduping ${localChildNode}; already seen in " +
                        "another folder", { localChildNode });
        continue;
      }
      if (!this.newLocalContents.has(localChildNode.guid)) {
        MirrorLog.trace("Not deduping ${localChildNode}; already uploaded",
                        { localChildNode });
        continue;
      }
      let remoteCandidate = this.remoteTree.nodeForGuid(localChildNode.guid);
      if (remoteCandidate) {
        MirrorLog.trace("Not deduping ${localChildNode}; already exists " +
                        "remotely", { localChildNode });
        continue;
      }
      if (this.remoteTree.isDeleted(localChildNode.guid)) {
        MirrorLog.trace("Not deduping ${localChildNode}; deleted on server",
                        { localChildNode });
        continue;
      }
      let localChildContent = this.newLocalContents.get(localChildNode.guid);
      if (!contentsMatch(localChildNode, localChildContent, remoteChildNode,
                         remoteChildContent)) {
        MirrorLog.trace("${localChildNode} is not a dupe of ${remoteChildNode}",
                        { localChildNode, remoteChildNode });
        continue;
      }
      this.telemetryEvents.push({ value: "dupe" });
      newLocalNode = localChildNode;
      break;
    }
    return newLocalNode;
  }

  /**
   * Returns an array of local ("L: ~bookmarkAAAA, ~bookmarkBBBB") and remote
   * ("R: ~bookmarkCCCC, ~bookmarkDDDD") deletions for logging.
   *
   * @return {String[]}
   */
  deletionsToStrings() {
    let infos = [];
    if (this.deleteLocally.size) {
      infos.push("L: " + Array.from(this.deleteLocally,
        guid => `~${guid}`).join(", "));
    }
    if (this.deleteRemotely.size) {
      infos.push("R: " + Array.from(this.deleteRemotely,
        guid => `~${guid}`).join(", "));
    }
    return infos;
  }
}

/**
 * Determines if two new local and remote nodes are of the same kind, and have
 * similar contents.
 *
 * - Bookmarks must have the same title and URL.
 * - Smart bookmarks must have the same smart bookmark name. Other queries
 *   must have the same title and query URL.
 * - Folders and livemarks must have the same title.
 * - Separators must have the same position within their parents.
 *
 * @param  {BookmarkNode} localNode
 * @param  {BookmarkContent} localContent
 * @param  {BookmarkNode} remoteNode
 * @param  {BookmarkContent} remoteContent
 * @return {Boolean}
 */
function contentsMatch(localNode, localContent, remoteNode, remoteContent) {
  if (localNode.kind != remoteNode.kind) {
    return false;
  }
  switch (localNode.kind) {
    case SyncedBookmarksMirror.KIND.BOOKMARK:
      return localContent.title == remoteContent.title &&
             localContent.hasSameURL(remoteContent);

    case SyncedBookmarksMirror.KIND.QUERY:
      if (localContent.smartBookmarkName || remoteContent.smartBookmarkName) {
        return localContent.smartBookmarkName ==
               remoteContent.smartBookmarkName;
      }
      return localContent.title == remoteContent.title &&
             localContent.hasSameURL(remoteContent);

    case SyncedBookmarksMirror.KIND.FOLDER:
    case SyncedBookmarksMirror.KIND.LIVEMARK:
      return localContent.title == remoteContent.title;

    case SyncedBookmarksMirror.KIND.SEPARATOR:
      return localContent.position == remoteContent.position;
  }
  return false;
}

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
    this.shouldInvalidateLivemarks = false;
  }

  /**
   * Fires all recorded observer notifications, invalidates the livemark cache
   * if necessary, and recalculates frecencies for changed URLs. This is called
   * outside the merge transaction.
   */
  async notifyAll() {
    this.notifyBookmarkObservers();
    this.notifyAnnoObservers();
    if (this.shouldInvalidateLivemarks) {
      await PlacesUtils.livemarks.invalidateCachedLivemarks();
    }
    await this.updateFrecencies();
  }

  async updateFrecencies() {
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

  noteKeywordChanged(info) {
    this.bookmarkObserverNotifications.push({
      name: "onItemChanged",
      isTagging: false,
      args: [info.id, "keyword", /* isAnnotationProperty */ false, info.keyword,
        info.lastModified, info.type, info.parentId, info.guid, info.parentGuid,
      /* oldValue */ info.urlHref, PlacesUtils.bookmarks.SOURCES.SYNC],
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

  notifyBookmarkObservers() {
    MirrorLog.debug("Notifying bookmark observers");
    let observers = PlacesUtils.bookmarks.getObservers();
    for (let observer of observers) {
      this.notifyObserver(observer, "onBeginUpdateBatch");
      for (let info of this.bookmarkObserverNotifications) {
        if (info.isTagging && observer.skipTags) {
          continue;
        }
        this.notifyObserver(observer, info.name, info.args);
      }
      this.notifyObserver(observer, "onEndUpdateBatch");
    }
  }

  notifyAnnoObservers() {
    MirrorLog.debug("Notifying anno observers");
    let observers = PlacesUtils.annotations.getObservers();
    for (let observer of observers) {
      for (let { name, args } of this.annoObserverNotifications) {
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
