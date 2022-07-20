/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineModuleGetter(lazy, "Log", "resource://gre/modules/Log.jsm");
ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

/**
 * This module exports functions for Sync to use when applying remote
 * records. The calls are similar to those in `Bookmarks.jsm` and
 * `nsINavBookmarksService`, with special handling for
 * tags, keywords, synced annotations, and missing parents.
 */
export var PlacesSyncUtils = {};

const { SOURCE_SYNC } = Ci.nsINavBookmarksService;

const MICROSECONDS_PER_SECOND = 1000000;

const MOBILE_BOOKMARKS_PREF = "browser.bookmarks.showMobileBookmarks";

// These are defined as lazy getters to defer initializing the bookmarks
// service until it's needed.
XPCOMUtils.defineLazyGetter(lazy, "ROOT_RECORD_ID_TO_GUID", () => ({
  menu: lazy.PlacesUtils.bookmarks.menuGuid,
  places: lazy.PlacesUtils.bookmarks.rootGuid,
  tags: lazy.PlacesUtils.bookmarks.tagsGuid,
  toolbar: lazy.PlacesUtils.bookmarks.toolbarGuid,
  unfiled: lazy.PlacesUtils.bookmarks.unfiledGuid,
  mobile: lazy.PlacesUtils.bookmarks.mobileGuid,
}));

XPCOMUtils.defineLazyGetter(lazy, "ROOT_GUID_TO_RECORD_ID", () => ({
  [lazy.PlacesUtils.bookmarks.menuGuid]: "menu",
  [lazy.PlacesUtils.bookmarks.rootGuid]: "places",
  [lazy.PlacesUtils.bookmarks.tagsGuid]: "tags",
  [lazy.PlacesUtils.bookmarks.toolbarGuid]: "toolbar",
  [lazy.PlacesUtils.bookmarks.unfiledGuid]: "unfiled",
  [lazy.PlacesUtils.bookmarks.mobileGuid]: "mobile",
}));

XPCOMUtils.defineLazyGetter(lazy, "ROOTS", () =>
  Object.keys(lazy.ROOT_RECORD_ID_TO_GUID)
);

// Gets the history transition values we ignore and do not sync, as a
// string, which is a comma-separated set of values - ie, something which can
// be used with sqlite's IN operator. Does *not* includes the parens.
XPCOMUtils.defineLazyGetter(lazy, "IGNORED_TRANSITIONS_AS_SQL_LIST", () =>
  // * We don't sync `TRANSITION_FRAMED_LINK` visits - these are excluded when
  //   rendering the history menu, so we use the same constraints for Sync.
  // * We don't sync `TRANSITION_DOWNLOAD` because it makes no sense to see
  //   these on other devices - the downloaded file can not exist.
  // * We don't want to sync TRANSITION_EMBED visits, but these aren't
  //   stored in the DB, so no need to specify them.
  // * 0 is invalid, and hopefully don't exist, but let's exclude it anyway.
  // Array.toString() semantics are well defined and exactly what we need, so..
  [
    0,
    lazy.PlacesUtils.history.TRANSITION_FRAMED_LINK,
    lazy.PlacesUtils.history.TRANSITION_DOWNLOAD,
  ].toString()
);

const HistorySyncUtils = (PlacesSyncUtils.history = Object.freeze({
  SYNC_ID_META_KEY: "sync/history/syncId",
  LAST_SYNC_META_KEY: "sync/history/lastSync",

  /**
   * Returns the current history sync ID, or `""` if one isn't set.
   */
  getSyncId() {
    return lazy.PlacesUtils.metadata.get(HistorySyncUtils.SYNC_ID_META_KEY, "");
  },

  /**
   * Assigns a new sync ID. This is called when we sync for the first time with
   * a new account, and when we're the first to sync after a node reassignment.
   *
   * @return {Promise} resolved once the ID has been updated.
   * @resolves to the new sync ID.
   */
  resetSyncId() {
    return lazy.PlacesUtils.withConnectionWrapper(
      "HistorySyncUtils: resetSyncId",
      function(db) {
        let newSyncId = lazy.PlacesUtils.history.makeGuid();
        return db.executeTransaction(async function() {
          await setHistorySyncId(db, newSyncId);
          return newSyncId;
        });
      }
    );
  },

  /**
   * Ensures that the existing local sync ID, if any, is up-to-date with the
   * server. This is called when we sync with an existing account.
   *
   * @param newSyncId
   *        The server's sync ID.
   * @return {Promise} resolved once the ID has been updated.
   */
  async ensureCurrentSyncId(newSyncId) {
    if (!newSyncId || typeof newSyncId != "string") {
      throw new TypeError("Invalid new history sync ID");
    }
    await lazy.PlacesUtils.withConnectionWrapper(
      "HistorySyncUtils: ensureCurrentSyncId",
      async function(db) {
        let existingSyncId = await lazy.PlacesUtils.metadata.getWithConnection(
          db,
          HistorySyncUtils.SYNC_ID_META_KEY,
          ""
        );

        if (existingSyncId == newSyncId) {
          lazy.HistorySyncLog.trace("History sync ID up-to-date", {
            existingSyncId,
          });
          return;
        }

        lazy.HistorySyncLog.info(
          "History sync ID changed; resetting metadata",
          {
            existingSyncId,
            newSyncId,
          }
        );
        await db.executeTransaction(function() {
          return setHistorySyncId(db, newSyncId);
        });
      }
    );
  },

  /**
   * Returns the last sync time, in seconds, for the history collection, or 0
   * if history has never synced before.
   */
  async getLastSync() {
    let lastSync = await lazy.PlacesUtils.metadata.get(
      HistorySyncUtils.LAST_SYNC_META_KEY,
      0
    );
    return lastSync / 1000;
  },

  /**
   * Updates the history collection last sync time.
   *
   * @param lastSyncSeconds
   *        The collection last sync time, in seconds, as a number or string.
   */
  async setLastSync(lastSyncSeconds) {
    let lastSync = Math.floor(lastSyncSeconds * 1000);
    if (!Number.isInteger(lastSync)) {
      throw new TypeError("Invalid history last sync timestamp");
    }
    await lazy.PlacesUtils.metadata.set(
      HistorySyncUtils.LAST_SYNC_META_KEY,
      lastSync
    );
  },

  /**
   * Removes all history visits and pages from the database. Sync calls this
   * method when it receives a command from a remote client to wipe all stored
   * data.
   *
   * @return {Promise} resolved once all pages and visits have been removed.
   */
  async wipe() {
    await lazy.PlacesUtils.history.clear();
    await HistorySyncUtils.reset();
  },

  /**
   * Removes the sync ID and last sync time for the history collection. Unlike
   * `wipe`, this keeps all existing history pages and visits.
   *
   * @return {Promise} resolved once the metadata have been removed.
   */
  reset() {
    return lazy.PlacesUtils.metadata.delete(
      HistorySyncUtils.SYNC_ID_META_KEY,
      HistorySyncUtils.LAST_SYNC_META_KEY
    );
  },

  /**
   * Clamps a history visit date between the current date and the earliest
   * sensible date.
   *
   * @param {Date} visitDate
   *        The visit date.
   * @return {Date} The clamped visit date.
   */
  clampVisitDate(visitDate) {
    let currentDate = new Date();
    if (visitDate > currentDate) {
      return currentDate;
    }
    if (visitDate < BookmarkSyncUtils.EARLIEST_BOOKMARK_TIMESTAMP) {
      return new Date(BookmarkSyncUtils.EARLIEST_BOOKMARK_TIMESTAMP);
    }
    return visitDate;
  },

  /**
   * Fetches the frecency for the URL provided
   *
   * @param url
   * @returns {Number} The frecency of the given url
   */
  async fetchURLFrecency(url) {
    let canonicalURL = lazy.PlacesUtils.SYNC_BOOKMARK_VALIDATORS.url(url);

    let db = await lazy.PlacesUtils.promiseDBConnection();
    let rows = await db.executeCached(
      `
      SELECT frecency
      FROM moz_places
      WHERE url_hash = hash(:url) AND url = :url
      LIMIT 1`,
      { url: canonicalURL.href }
    );

    return rows.length ? rows[0].getResultByName("frecency") : -1;
  },

  /**
   * Filters syncable places from a collection of places guids.
   *
   * @param guids
   *
   * @returns {Array} new Array with the guids that aren't syncable
   */
  async determineNonSyncableGuids(guids) {
    // Filter out hidden pages and transitions that we don't sync.
    let db = await lazy.PlacesUtils.promiseDBConnection();
    let nonSyncableGuids = [];
    for (let chunk of lazy.PlacesUtils.chunkArray(guids, db.variableLimit)) {
      let rows = await db.execute(
        `
        SELECT DISTINCT p.guid FROM moz_places p
        JOIN moz_historyvisits v ON p.id = v.place_id
        WHERE p.guid IN (${new Array(chunk.length).fill("?").join(",")}) AND
            (p.hidden = 1 OR v.visit_type IN (${
              lazy.IGNORED_TRANSITIONS_AS_SQL_LIST
            }))
      `,
        chunk
      );
      nonSyncableGuids = nonSyncableGuids.concat(
        rows.map(row => row.getResultByName("guid"))
      );
    }
    return nonSyncableGuids;
  },

  /**
   * Change the guid of the given uri
   *
   * @param uri
   * @param guid
   */
  changeGuid(uri, guid) {
    let canonicalURL = lazy.PlacesUtils.SYNC_BOOKMARK_VALIDATORS.url(uri);
    let validatedGuid = lazy.PlacesUtils.BOOKMARK_VALIDATORS.guid(guid);
    return lazy.PlacesUtils.withConnectionWrapper(
      "PlacesSyncUtils.history: changeGuid",
      async function(db) {
        await db.executeCached(
          `
            UPDATE moz_places
            SET guid = :guid
            WHERE url_hash = hash(:page_url) AND url = :page_url`,
          { guid: validatedGuid, page_url: canonicalURL.href }
        );
      }
    );
  },

  /**
   * Fetch the last 20 visits (date and type of it) corresponding to a given url
   *
   * @param url
   * @returns {Array} Each element of the Array is an object with members: date and type
   */
  async fetchVisitsForURL(url) {
    let canonicalURL = lazy.PlacesUtils.SYNC_BOOKMARK_VALIDATORS.url(url);
    let db = await lazy.PlacesUtils.promiseDBConnection();
    let rows = await db.executeCached(
      `
      SELECT visit_type type, visit_date date
      FROM moz_historyvisits
      JOIN moz_places h ON h.id = place_id
      WHERE url_hash = hash(:url) AND url = :url
      ORDER BY date DESC LIMIT 20`,
      { url: canonicalURL.href }
    );
    return rows.map(row => {
      let visitDate = row.getResultByName("date");
      let visitType = row.getResultByName("type");
      return { date: visitDate, type: visitType };
    });
  },

  /**
   * Fetches the guid of a uri
   *
   * @param uri
   * @returns {String} The guid of the given uri
   */
  async fetchGuidForURL(url) {
    let canonicalURL = lazy.PlacesUtils.SYNC_BOOKMARK_VALIDATORS.url(url);
    let db = await lazy.PlacesUtils.promiseDBConnection();
    let rows = await db.executeCached(
      `
        SELECT guid
        FROM moz_places
        WHERE url_hash = hash(:page_url) AND url = :page_url`,
      { page_url: canonicalURL.href }
    );
    if (!rows.length) {
      return null;
    }
    return rows[0].getResultByName("guid");
  },

  /**
   * Fetch information about a guid (url, title and frecency)
   *
   * @param guid
   * @returns {Object} Object with three members: url, title and frecency of the given guid
   */
  async fetchURLInfoForGuid(guid) {
    let db = await lazy.PlacesUtils.promiseDBConnection();
    let rows = await db.executeCached(
      `
      SELECT url, IFNULL(title, '') AS title, frecency
      FROM moz_places
      WHERE guid = :guid`,
      { guid }
    );
    if (rows.length === 0) {
      return null;
    }
    return {
      url: rows[0].getResultByName("url"),
      title: rows[0].getResultByName("title"),
      frecency: rows[0].getResultByName("frecency"),
    };
  },

  /**
   * Get all URLs filtered by the limit and since members of the options object.
   *
   * @param options
   *        Options object with two members, since and limit. Both of them must be provided
   * @returns {Array} - Up to limit number of URLs starting from the date provided by since
   *
   * Note that some visit types are explicitly excluded - downloads and framed
   * links.
   */
  async getAllURLs(options) {
    // Check that the limit property is finite number.
    if (!Number.isFinite(options.limit)) {
      throw new Error("The number provided in options.limit is not finite.");
    }
    // Check that the since property is of type Date.
    if (
      !options.since ||
      Object.prototype.toString.call(options.since) != "[object Date]"
    ) {
      throw new Error(
        "The property since of the options object must be of type Date."
      );
    }
    let db = await lazy.PlacesUtils.promiseDBConnection();
    let sinceInMicroseconds = lazy.PlacesUtils.toPRTime(options.since);
    let rows = await db.executeCached(
      `
      SELECT DISTINCT p.url
      FROM moz_places p
      JOIN moz_historyvisits v ON p.id = v.place_id
      WHERE p.last_visit_date > :cutoff_date AND
            p.hidden = 0 AND
            v.visit_type NOT IN (${lazy.IGNORED_TRANSITIONS_AS_SQL_LIST})
      ORDER BY frecency DESC
      LIMIT :max_results`,
      { cutoff_date: sinceInMicroseconds, max_results: options.limit }
    );
    return rows.map(row => row.getResultByName("url"));
  },
}));

const BookmarkSyncUtils = (PlacesSyncUtils.bookmarks = Object.freeze({
  SYNC_PARENT_ANNO: "sync/parent",

  SYNC_ID_META_KEY: "sync/bookmarks/syncId",
  LAST_SYNC_META_KEY: "sync/bookmarks/lastSync",
  WIPE_REMOTE_META_KEY: "sync/bookmarks/wipeRemote",

  // Jan 23, 1993 in milliseconds since 1970. Corresponds roughly to the release
  // of the original NCSA Mosiac. We can safely assume that any dates before
  // this time are invalid.
  EARLIEST_BOOKMARK_TIMESTAMP: Date.UTC(1993, 0, 23),

  KINDS: {
    BOOKMARK: "bookmark",
    QUERY: "query",
    FOLDER: "folder",
    LIVEMARK: "livemark",
    SEPARATOR: "separator",
  },

  get ROOTS() {
    return lazy.ROOTS;
  },

  /**
   * Returns the current bookmarks sync ID, or `""` if one isn't set.
   */
  getSyncId() {
    return lazy.PlacesUtils.metadata.get(
      BookmarkSyncUtils.SYNC_ID_META_KEY,
      ""
    );
  },

  /**
   * Indicates if the bookmarks engine should erase all bookmarks on the server
   * and all other clients, because the user manually restored their bookmarks
   * from a backup on this client.
   */
  async shouldWipeRemote() {
    let shouldWipeRemote = await lazy.PlacesUtils.metadata.get(
      BookmarkSyncUtils.WIPE_REMOTE_META_KEY,
      false
    );
    return !!shouldWipeRemote;
  },

  /**
   * Assigns a new sync ID, bumps the change counter, and flags all items as
   * "NEW" for upload. This is called when we sync for the first time with a
   * new account, when we're the first to sync after a node reassignment, and
   * on the first sync after a manual restore.
   *
   * @return {Promise} resolved once the ID and all items have been updated.
   * @resolves to the new sync ID.
   */
  resetSyncId() {
    return lazy.PlacesUtils.withConnectionWrapper(
      "BookmarkSyncUtils: resetSyncId",
      function(db) {
        let newSyncId = lazy.PlacesUtils.history.makeGuid();
        return db.executeTransaction(async function() {
          await setBookmarksSyncId(db, newSyncId);
          await resetAllSyncStatuses(
            db,
            lazy.PlacesUtils.bookmarks.SYNC_STATUS.NEW
          );
          return newSyncId;
        });
      }
    );
  },

  /**
   * Ensures that the existing local sync ID, if any, is up-to-date with the
   * server. This is called when we sync with an existing account.
   *
   * We always take the server's sync ID. If we don't have an existing ID,
   * we're either syncing for the first time with an existing account, or Places
   * has automatically restored from a backup. If the sync IDs don't match,
   * we're likely syncing after a node reassignment, where another client
   * uploaded their bookmarks first.
   *
   * @param newSyncId
   *        The server's sync ID.
   * @return {Promise} resolved once the ID and all items have been updated.
   */
  async ensureCurrentSyncId(newSyncId) {
    if (!newSyncId || typeof newSyncId != "string") {
      throw new TypeError("Invalid new bookmarks sync ID");
    }
    await lazy.PlacesUtils.withConnectionWrapper(
      "BookmarkSyncUtils: ensureCurrentSyncId",
      async function(db) {
        let existingSyncId = await lazy.PlacesUtils.metadata.getWithConnection(
          db,
          BookmarkSyncUtils.SYNC_ID_META_KEY,
          ""
        );

        // If we don't have a sync ID, take the server's without resetting
        // sync statuses.
        if (!existingSyncId) {
          lazy.BookmarkSyncLog.info("Taking new bookmarks sync ID", {
            newSyncId,
          });
          await db.executeTransaction(() => setBookmarksSyncId(db, newSyncId));
          return;
        }

        // If the existing sync ID matches the server, great!
        if (existingSyncId == newSyncId) {
          lazy.BookmarkSyncLog.trace("Bookmarks sync ID up-to-date", {
            existingSyncId,
          });
          return;
        }

        // Otherwise, we have a sync ID, but it doesn't match, so we were likely
        // node reassigned. Take the server's sync ID and reset all items to
        // "UNKNOWN" so that we can merge.
        lazy.BookmarkSyncLog.info(
          "Bookmarks sync ID changed; resetting sync statuses",
          { existingSyncId, newSyncId }
        );
        await db.executeTransaction(async function() {
          await setBookmarksSyncId(db, newSyncId);
          await resetAllSyncStatuses(
            db,
            lazy.PlacesUtils.bookmarks.SYNC_STATUS.UNKNOWN
          );
        });
      }
    );
  },

  /**
   * Returns the last sync time, in seconds, for the bookmarks collection, or 0
   * if bookmarks have never synced before.
   */
  async getLastSync() {
    let lastSync = await lazy.PlacesUtils.metadata.get(
      BookmarkSyncUtils.LAST_SYNC_META_KEY,
      0
    );
    return lastSync / 1000;
  },

  /**
   * Updates the bookmarks collection last sync time.
   *
   * @param lastSyncSeconds
   *        The collection last sync time, in seconds, as a number or string.
   */
  async setLastSync(lastSyncSeconds) {
    let lastSync = Math.floor(lastSyncSeconds * 1000);
    if (!Number.isInteger(lastSync)) {
      throw new TypeError("Invalid bookmarks last sync timestamp");
    }
    await lazy.PlacesUtils.metadata.set(
      BookmarkSyncUtils.LAST_SYNC_META_KEY,
      lastSync
    );
  },

  /**
   * Resets Sync metadata for bookmarks in Places. This function behaves
   * differently depending on the change source, and may be called from
   * `PlacesSyncUtils.bookmarks.reset` or
   * `PlacesUtils.bookmarks.eraseEverything`.
   *
   * - RESTORE: The user is restoring from a backup. Drop the sync ID, last
   *   sync time, and tombstones; reset sync statuses for remaining items to
   *   "NEW"; then set a flag to wipe the server and all other clients. On the
   *   next sync, we'll replace their bookmarks with ours.
   *
   * - RESTORE_ON_STARTUP: Places is automatically restoring from a backup to
   *   recover from a corrupt database. The sync ID, last sync time, and
   *   tombstones don't exist, since we don't back them up; reset sync statuses
   *   for the roots to "UNKNOWN"; but don't wipe the server. On the next sync,
   *   we'll merge the restored bookmarks with the ones on the server.
   *
   * - SYNC: Either another client told us to erase our bookmarks
   *   (`PlacesSyncUtils.bookmarks.wipe`), or the user disconnected Sync
   *   (`PlacesSyncUtils.bookmarks.reset`). In both cases, drop the existing
   *   sync ID, last sync time, and tombstones; reset sync statuses for
   *   remaining items to "NEW"; and don't wipe the server.
   *
   * @param db
   *        the Sqlite.jsm connection handle.
   * @param source
   *        the change source constant.
   */
  async resetSyncMetadata(db, source) {
    if (
      ![
        lazy.PlacesUtils.bookmarks.SOURCES.RESTORE,
        lazy.PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP,
        lazy.PlacesUtils.bookmarks.SOURCES.SYNC,
      ].includes(source)
    ) {
      return;
    }

    // Remove the sync ID and last sync time in all cases.
    await lazy.PlacesUtils.metadata.deleteWithConnection(
      db,
      BookmarkSyncUtils.SYNC_ID_META_KEY,
      BookmarkSyncUtils.LAST_SYNC_META_KEY
    );

    // If we're manually restoring from a backup, wipe the server and other
    // clients, so that we replace their bookmarks with the restored tree. If
    // we're automatically restoring to recover from a corrupt database, don't
    // wipe; we want to merge the restored tree with the one on the server.
    await lazy.PlacesUtils.metadata.setWithConnection(
      db,
      BookmarkSyncUtils.WIPE_REMOTE_META_KEY,
      source == lazy.PlacesUtils.bookmarks.SOURCES.RESTORE
    );

    // Reset change counters and sync statuses for roots and remaining
    // items, and drop tombstones.
    let syncStatus =
      source == lazy.PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP
        ? lazy.PlacesUtils.bookmarks.SYNC_STATUS.UNKNOWN
        : lazy.PlacesUtils.bookmarks.SYNC_STATUS.NEW;
    await resetAllSyncStatuses(db, syncStatus);
  },

  /**
   * Converts a Places GUID to a Sync record ID. Record IDs are identical to
   * Places GUIDs for all items except roots.
   */
  guidToRecordId(guid) {
    return lazy.ROOT_GUID_TO_RECORD_ID[guid] || guid;
  },

  /**
   * Converts a Sync record ID to a Places GUID.
   */
  recordIdToGuid(recordId) {
    return lazy.ROOT_RECORD_ID_TO_GUID[recordId] || recordId;
  },

  /**
   * Fetches the record IDs for a folder's children, ordered by their position
   * within the folder.
   * Used only be tests - but that includes tps, so it lives here.
   */
  fetchChildRecordIds(parentRecordId) {
    lazy.PlacesUtils.SYNC_BOOKMARK_VALIDATORS.recordId(parentRecordId);
    let parentGuid = BookmarkSyncUtils.recordIdToGuid(parentRecordId);

    return lazy.PlacesUtils.withConnectionWrapper(
      "BookmarkSyncUtils: fetchChildRecordIds",
      async function(db) {
        let childGuids = await fetchChildGuids(db, parentGuid);
        return childGuids.map(guid => BookmarkSyncUtils.guidToRecordId(guid));
      }
    );
  },

  /**
   * Migrates an array of `{ recordId, modified }` tuples from the old JSON-based
   * tracker to the new sync change counter. `modified` is when the change was
   * added to the old tracker, in milliseconds.
   *
   * Sync calls this method before the first bookmark sync after the Places
   * schema migration.
   */
  migrateOldTrackerEntries(entries) {
    return lazy.PlacesUtils.withConnectionWrapper(
      "BookmarkSyncUtils: migrateOldTrackerEntries",
      function(db) {
        return db.executeTransaction(async function() {
          // Mark all existing bookmarks as synced, and clear their change
          // counters to avoid a full upload on the next sync. Note that
          // this means we'll miss changes made between startup and the first
          // post-migration sync, as well as changes made on a new release
          // channel that weren't synced before the user downgraded. This is
          // unfortunate, but no worse than the behavior of the old tracker.
          //
          // We also likely have bookmarks that don't exist on the server,
          // because the old tracker missed them. We'll eventually fix the
          // server once we decide on a repair strategy.
          await db.executeCached(
            `
            WITH RECURSIVE
            syncedItems(id) AS (
              SELECT b.id FROM moz_bookmarks b
              WHERE b.guid IN ('menu________', 'toolbar_____', 'unfiled_____',
                               'mobile______')
              UNION ALL
              SELECT b.id FROM moz_bookmarks b
              JOIN syncedItems s ON b.parent = s.id
            )
            UPDATE moz_bookmarks SET
              syncStatus = :syncStatus,
              syncChangeCounter = 0
            WHERE id IN syncedItems`,
            { syncStatus: lazy.PlacesUtils.bookmarks.SYNC_STATUS.NORMAL }
          );

          await db.executeCached(`DELETE FROM moz_bookmarks_deleted`);

          await db.executeCached(`CREATE TEMP TABLE moz_bookmarks_tracked (
            guid TEXT PRIMARY KEY,
            time INTEGER
          )`);

          try {
            for (let { recordId, modified } of entries) {
              let guid = BookmarkSyncUtils.recordIdToGuid(recordId);
              if (!lazy.PlacesUtils.isValidGuid(guid)) {
                lazy.BookmarkSyncLog.warn(
                  `migrateOldTrackerEntries: Ignoring ` +
                    `change for invalid item ${guid}`
                );
                continue;
              }
              let time = lazy.PlacesUtils.toPRTime(
                Number.isFinite(modified) ? modified : Date.now()
              );
              await db.executeCached(
                `
                INSERT OR IGNORE INTO moz_bookmarks_tracked (guid, time)
                VALUES (:guid, :time)`,
                { guid, time }
              );
            }

            // Bump the change counter for existing tracked items.
            await db.executeCached(`
              INSERT OR REPLACE INTO moz_bookmarks (id, fk, type, parent,
                                                    position, title,
                                                    dateAdded, lastModified,
                                                    guid, syncChangeCounter,
                                                    syncStatus)
              SELECT b.id, b.fk, b.type, b.parent, b.position, b.title,
                     b.dateAdded, MAX(b.lastModified, t.time), b.guid,
                     b.syncChangeCounter + 1, b.syncStatus
              FROM moz_bookmarks b
              JOIN moz_bookmarks_tracked t ON b.guid = t.guid`);

            // Insert tombstones for nonexistent tracked items, using the most
            // recent deletion date for more accurate reconciliation. We assume
            // the tracked item belongs to a synced root.
            await db.executeCached(`
              INSERT OR REPLACE INTO moz_bookmarks_deleted (guid, dateRemoved)
              SELECT t.guid, MAX(IFNULL((SELECT dateRemoved FROM moz_bookmarks_deleted
                                         WHERE guid = t.guid), 0), t.time)
              FROM moz_bookmarks_tracked t
              LEFT JOIN moz_bookmarks b ON t.guid = b.guid
              WHERE b.guid IS NULL`);
          } finally {
            await db.executeCached(`DROP TABLE moz_bookmarks_tracked`);
          }
        });
      }
    );
  },

  /**
   * Reorders a folder's children, based on their order in the array of sync
   * IDs.
   *
   * Sync uses this method to reorder all synced children after applying all
   * incoming records.
   *
   * @return {Promise} resolved when reordering is complete.
   * @rejects if an error happens while reordering.
   * @throws if the arguments are invalid.
   */
  order(parentRecordId, childRecordIds) {
    lazy.PlacesUtils.SYNC_BOOKMARK_VALIDATORS.recordId(parentRecordId);
    if (!childRecordIds.length) {
      return undefined;
    }
    let parentGuid = BookmarkSyncUtils.recordIdToGuid(parentRecordId);
    if (parentGuid == lazy.PlacesUtils.bookmarks.rootGuid) {
      // Reordering roots doesn't make sense, but Sync will do this on the
      // first sync.
      return undefined;
    }
    let orderedChildrenGuids = childRecordIds.map(
      BookmarkSyncUtils.recordIdToGuid
    );
    return lazy.PlacesUtils.bookmarks.reorder(
      parentGuid,
      orderedChildrenGuids,
      {
        source: SOURCE_SYNC,
      }
    );
  },

  /**
   * Resolves to true if there are known sync changes.
   */
  havePendingChanges() {
    return lazy.PlacesUtils.withConnectionWrapper(
      "BookmarkSyncUtils: havePendingChanges",
      async function(db) {
        let rows = await db.executeCached(`
          WITH RECURSIVE
          syncedItems(id, guid, syncChangeCounter) AS (
            SELECT b.id, b.guid, b.syncChangeCounter
             FROM moz_bookmarks b
             WHERE b.guid IN ('menu________', 'toolbar_____', 'unfiled_____',
                              'mobile______')
            UNION ALL
            SELECT b.id, b.guid, b.syncChangeCounter
            FROM moz_bookmarks b
            JOIN syncedItems s ON b.parent = s.id
          ),
          changedItems(guid) AS (
            SELECT guid FROM syncedItems
            WHERE syncChangeCounter >= 1
            UNION ALL
            SELECT guid FROM moz_bookmarks_deleted
          )
          SELECT EXISTS(SELECT guid FROM changedItems) AS haveChanges`);
        return !!rows[0].getResultByName("haveChanges");
      }
    );
  },

  /**
   * Returns a changeset containing local bookmark changes since the last sync.
   *
   * @return {Promise} resolved once all items have been fetched.
   * @resolves to an object containing records for changed bookmarks, keyed by
   *           the record ID.
   * @see pullSyncChanges for the implementation, and markChangesAsSyncing for
   *      an explanation of why we update the sync status.
   */
  pullChanges() {
    return lazy.PlacesUtils.withConnectionWrapper(
      "BookmarkSyncUtils: pullChanges",
      pullSyncChanges
    );
  },

  /**
   * Updates the sync status of all "NEW" bookmarks to "NORMAL", so that Sync
   * can recover correctly after an interrupted sync.
   *
   * @param changeRecords
   *        A changeset containing sync change records, as returned by
   *        `pullChanges`.
   * @return {Promise} resolved once all records have been updated.
   */
  markChangesAsSyncing(changeRecords) {
    return lazy.PlacesUtils.withConnectionWrapper(
      "BookmarkSyncUtils: markChangesAsSyncing",
      db => markChangesAsSyncing(db, changeRecords)
    );
  },

  /**
   * Decrements the sync change counter, updates the sync status, and cleans up
   * tombstones for successfully synced items. Sync calls this method at the
   * end of each bookmark sync.
   *
   * @param changeRecords
   *        A changeset containing sync change records, as returned by
   *        `pullChanges`.
   * @return {Promise} resolved once all records have been updated.
   */
  pushChanges(changeRecords) {
    return lazy.PlacesUtils.withConnectionWrapper(
      "BookmarkSyncUtils: pushChanges",
      async function(db) {
        let skippedCount = 0;
        let weakCount = 0;
        let updateParams = [];
        let tombstoneGuidsToRemove = [];

        for (let recordId in changeRecords) {
          // Validate change records to catch coding errors.
          let changeRecord = validateChangeRecord(
            "BookmarkSyncUtils: pushChanges",
            changeRecords[recordId],
            {
              tombstone: { required: true },
              counter: { required: true },
              synced: { required: true },
            }
          );

          // Skip weakly uploaded records.
          if (!changeRecord.counter) {
            weakCount++;
            continue;
          }

          // Sync sets the `synced` flag for reconciled or successfully
          // uploaded items. If upload failed, ignore the change; we'll
          // try again on the next sync.
          if (!changeRecord.synced) {
            skippedCount++;
            continue;
          }

          let guid = BookmarkSyncUtils.recordIdToGuid(recordId);
          if (changeRecord.tombstone) {
            tombstoneGuidsToRemove.push(guid);
          } else {
            updateParams.push({
              guid,
              syncChangeDelta: changeRecord.counter,
              syncStatus: lazy.PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
            });
          }
        }

        // Reduce the change counter and update the sync status for
        // reconciled and uploaded items. If the bookmark was updated
        // during the sync, its change counter will still be > 0 for the
        // next sync.
        if (updateParams.length || tombstoneGuidsToRemove.length) {
          await db.executeTransaction(async function() {
            if (updateParams.length) {
              await db.executeCached(
                `
                UPDATE moz_bookmarks
                SET syncChangeCounter = MAX(syncChangeCounter - :syncChangeDelta, 0),
                    syncStatus = :syncStatus
                WHERE guid = :guid`,
                updateParams
              );
              // and if there are *both* bookmarks and tombstones for these
              // items, we nuke the tombstones.
              // This should be unlikely, but bad if it happens.
              let dupedGuids = updateParams.map(({ guid }) => guid);
              await removeUndeletedTombstones(db, dupedGuids);
            }
            await removeTombstones(db, tombstoneGuidsToRemove);
          });
        }

        lazy.BookmarkSyncLog.debug(`pushChanges: Processed change records`, {
          weak: weakCount,
          skipped: skippedCount,
          updated: updateParams.length,
        });
      }
    );
  },

  /**
   * Removes items from the database. Sync buffers incoming tombstones, and
   * calls this method to apply them at the end of each sync. Deletion
   * happens in three steps:
   *
   *  1. Remove all non-folder items. Deleting a folder on a remote client
   *     uploads tombstones for the folder and its children at the time of
   *     deletion. This preserves any new children we've added locally since
   *     the last sync.
   *  2. Reparent remaining children to the tombstoned folder's parent. This
   *     bumps the change counter for the children and their new parent.
   *  3. Remove the tombstoned folder. Because we don't do this in a
   *     transaction, the user might move new items into the folder before we
   *     can remove it. In that case, we keep the folder and upload the new
   *     subtree to the server.
   *
   * See the comment above `BookmarksStore::deletePending` for the details on
   * why delete works the way it does.
   */
  remove(recordIds) {
    if (!recordIds.length) {
      return null;
    }

    return lazy.PlacesUtils.withConnectionWrapper(
      "BookmarkSyncUtils: remove",
      async function(db) {
        let folderGuids = [];
        for (let recordId of recordIds) {
          if (recordId in lazy.ROOT_RECORD_ID_TO_GUID) {
            lazy.BookmarkSyncLog.warn(
              `remove: Refusing to remove root ${recordId}`
            );
            continue;
          }
          let guid = BookmarkSyncUtils.recordIdToGuid(recordId);
          let bookmarkItem = await lazy.PlacesUtils.bookmarks.fetch(guid);
          if (!bookmarkItem) {
            lazy.BookmarkSyncLog.trace(`remove: Item ${guid} already removed`);
            continue;
          }
          let kind = await getKindForItem(db, bookmarkItem);
          if (kind == BookmarkSyncUtils.KINDS.FOLDER) {
            folderGuids.push(bookmarkItem.guid);
            continue;
          }
          let wasRemoved = await deleteSyncedAtom(bookmarkItem);
          if (wasRemoved) {
            lazy.BookmarkSyncLog.trace(
              `remove: Removed item ${guid} with kind ${kind}`
            );
          }
        }

        for (let guid of folderGuids) {
          let bookmarkItem = await lazy.PlacesUtils.bookmarks.fetch(guid);
          if (!bookmarkItem) {
            lazy.BookmarkSyncLog.trace(
              `remove: Folder ${guid} already removed`
            );
            continue;
          }
          let wasRemoved = await deleteSyncedFolder(db, bookmarkItem);
          if (wasRemoved) {
            lazy.BookmarkSyncLog.trace(
              `remove: Removed folder ${bookmarkItem.guid}`
            );
          }
        }

        // TODO (Bug 1313890): Refactor the bookmarks engine to pull change records
        // before uploading, instead of returning records to merge into the engine's
        // initial changeset.
        return pullSyncChanges(db);
      }
    );
  },

  /**
   * Returns true for record IDs that are considered roots.
   */
  isRootRecordID(id) {
    return lazy.ROOT_RECORD_ID_TO_GUID.hasOwnProperty(id);
  },

  /**
   * Removes all bookmarks and tombstones from the database. Sync calls this
   * method when it receives a command from a remote client to wipe all stored
   * data.
   *
   * @return {Promise} resolved once all items have been removed.
   */
  wipe() {
    return lazy.PlacesUtils.bookmarks.eraseEverything({
      source: SOURCE_SYNC,
    });
  },

  /**
   * Marks all bookmarks as "NEW" and removes all tombstones. Unlike `wipe`,
   * this keeps all existing bookmarks, and only clears their sync change
   * tracking info.
   *
   * @return {Promise} resolved once all items have been updated.
   */
  reset() {
    return lazy.PlacesUtils.withConnectionWrapper(
      "BookmarkSyncUtils: reset",
      function(db) {
        return db.executeTransaction(async function() {
          await BookmarkSyncUtils.resetSyncMetadata(db, SOURCE_SYNC);
        });
      }
    );
  },

  /**
   * Fetches a Sync bookmark object for an item in the tree.
   *
   * Should only be used by SYNC TESTS.
   * We should remove this in bug XXXXXX, updating the tests to use
   * PlacesUtils.bookmarks.fetch.
   *
   * The object contains
   * the following properties, depending on the item's kind:
   *
   *  - kind (all): A string representing the item's kind.
   *  - recordId (all): The item's record ID.
   *  - parentRecordId (all): The record ID of the item's parent.
   *  - parentTitle (all): The title of the item's parent, used for de-duping.
   *    Omitted for the Places root and parents with empty titles.
   *  - dateAdded (all): Timestamp in milliseconds, when the bookmark was added
   *    or created on a remote device if known.
   *  - title ("bookmark", "folder", "query"): The item's title.
   *    Omitted if empty.
   *  - url ("bookmark", "query"): The item's URL.
   *  - tags ("bookmark", "query"): An array containing the item's tags.
   *  - keyword ("bookmark"): The bookmark's keyword, if one exists.
   *  - childRecordIds ("folder"): An array containing the record IDs of the item's
   *    children, used to determine child order.
   *  - folder ("query"): The tag folder name, if this is a tag query.
   *  - index ("separator"): The separator's position within its parent.
   */
  async fetch(recordId) {
    let guid = BookmarkSyncUtils.recordIdToGuid(recordId);
    let bookmarkItem = await lazy.PlacesUtils.bookmarks.fetch(guid);
    if (!bookmarkItem) {
      return null;
    }
    return lazy.PlacesUtils.withConnectionWrapper(
      "BookmarkSyncUtils: fetch",
      async function(db) {
        // Convert the Places bookmark object to a Sync bookmark and add
        // kind-specific properties. Titles are required for bookmarks,
        // and folders; optional for queries, and omitted for separators.
        let kind = await getKindForItem(db, bookmarkItem);
        let item;
        switch (kind) {
          case BookmarkSyncUtils.KINDS.BOOKMARK:
            item = await fetchBookmarkItem(db, bookmarkItem);
            break;

          case BookmarkSyncUtils.KINDS.QUERY:
            item = await fetchQueryItem(db, bookmarkItem);
            break;

          case BookmarkSyncUtils.KINDS.FOLDER:
            item = await fetchFolderItem(db, bookmarkItem);
            break;

          case BookmarkSyncUtils.KINDS.SEPARATOR:
            item = await placesBookmarkToSyncBookmark(db, bookmarkItem);
            item.index = bookmarkItem.index;
            break;

          default:
            throw new Error(`Unknown bookmark kind: ${kind}`);
        }

        // Sync uses the parent title for de-duping. All Sync bookmark objects
        // except the Places root should have this property.
        if (bookmarkItem.parentGuid) {
          let parent = await lazy.PlacesUtils.bookmarks.fetch(
            bookmarkItem.parentGuid
          );
          item.parentTitle = parent.title || "";
        }

        return item;
      }
    );
  },

  /**
   * Returns the sync change counter increment for a change source constant.
   */
  determineSyncChangeDelta(source) {
    // Don't bump the change counter when applying changes made by Sync, to
    // avoid sync loops.
    return source == lazy.PlacesUtils.bookmarks.SOURCES.SYNC ? 0 : 1;
  },

  /**
   * Returns the sync status for a new item inserted by a change source.
   */
  determineInitialSyncStatus(source) {
    if (source == lazy.PlacesUtils.bookmarks.SOURCES.SYNC) {
      // Incoming bookmarks are "NORMAL", since they already exist on the server.
      return lazy.PlacesUtils.bookmarks.SYNC_STATUS.NORMAL;
    }
    if (source == lazy.PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP) {
      // If the user restores from a backup, or Places automatically recovers
      // from a corrupt database, all prior sync tracking is lost. Setting the
      // status to "UNKNOWN" allows Sync to reconcile restored bookmarks with
      // those on the server.
      return lazy.PlacesUtils.bookmarks.SYNC_STATUS.UNKNOWN;
    }
    // For all other sources, mark items as "NEW". We'll update their statuses
    // to "NORMAL" after the first sync.
    return lazy.PlacesUtils.bookmarks.SYNC_STATUS.NEW;
  },

  /**
   * An internal helper that bumps the change counter for all bookmarks with
   * a given URL. This is used to update bookmarks when adding or changing a
   * tag or keyword entry.
   *
   * @param db
   *        the Sqlite.jsm connection handle.
   * @param url
   *        the bookmark URL object.
   * @param syncChangeDelta
   *        the sync change counter increment.
   * @return {Promise} resolved when the counters have been updated.
   */
  addSyncChangesForBookmarksWithURL(db, url, syncChangeDelta) {
    if (!url || !syncChangeDelta) {
      return Promise.resolve();
    }
    return db.executeCached(
      `
      UPDATE moz_bookmarks
        SET syncChangeCounter = syncChangeCounter + :syncChangeDelta
      WHERE type = :type AND
            fk = (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND
                  url = :url)`,
      {
        syncChangeDelta,
        type: lazy.PlacesUtils.bookmarks.TYPE_BOOKMARK,
        url: url.href,
      }
    );
  },

  async removeLivemark(livemarkInfo) {
    let info = validateSyncBookmarkObject(
      "BookmarkSyncUtils: removeLivemark",
      livemarkInfo,
      {
        kind: {
          required: true,
          validIf: b => b.kind == BookmarkSyncUtils.KINDS.LIVEMARK,
        },
        recordId: { required: true },
        parentRecordId: { required: true },
      }
    );

    let guid = BookmarkSyncUtils.recordIdToGuid(info.recordId);
    let parentGuid = BookmarkSyncUtils.recordIdToGuid(info.parentRecordId);

    return lazy.PlacesUtils.withConnectionWrapper(
      "BookmarkSyncUtils: removeLivemark",
      async function(db) {
        if (await GUIDMissing(guid)) {
          // If the livemark doesn't exist in the database, insert a tombstone
          // and bump its parent's change counter to ensure it's removed from
          // the server in the current sync.
          await db.executeTransaction(async function() {
            await db.executeCached(
              `
              UPDATE moz_bookmarks SET
                syncChangeCounter = syncChangeCounter + 1
              WHERE guid = :parentGuid`,
              { parentGuid }
            );

            await db.executeCached(
              `
              INSERT OR IGNORE INTO moz_bookmarks_deleted (guid, dateRemoved)
              VALUES (:guid, ${lazy.PlacesUtils.toPRTime(Date.now())})`,
              { guid }
            );
          });
        } else {
          await lazy.PlacesUtils.bookmarks.remove({
            guid,
            // `SYNC_REPARENT_REMOVED_FOLDER_CHILDREN` bumps the change counter for
            // the child and its new parent, without incrementing the bookmark
            // tracker's score.
            source:
              lazy.PlacesUtils.bookmarks.SOURCES
                .SYNC_REPARENT_REMOVED_FOLDER_CHILDREN,
          });
        }

        return pullSyncChanges(db, [guid, parentGuid]);
      }
    );
  },

  /**
   * Returns `0` if no sensible timestamp could be found.
   * Otherwise, returns the earliest sensible timestamp between `existingMillis`
   * and `serverMillis`.
   */
  ratchetTimestampBackwards(
    existingMillis,
    serverMillis,
    lowerBound = BookmarkSyncUtils.EARLIEST_BOOKMARK_TIMESTAMP
  ) {
    const possible = [+existingMillis, +serverMillis].filter(
      n => !isNaN(n) && n > lowerBound
    );
    if (!possible.length) {
      return 0;
    }
    return Math.min(...possible);
  },

  /**
   * Rebuilds the left pane query for the mobile root under "All Bookmarks" if
   * necessary. Sync calls this method at the end of each bookmark sync. This
   * code should eventually move to `PlacesUIUtils#maybeRebuildLeftPane`; see
   * bug 647605.
   *
   * - If there are no mobile bookmarks, the query will not be created, or
   *   will be removed if it already exists.
   * - If there are mobile bookmarks, the query will be created if it doesn't
   *   exist, or will be updated with the correct title and URL otherwise.
   */
  async ensureMobileQuery() {
    let db = await lazy.PlacesUtils.promiseDBConnection();

    let mobileChildGuids = await fetchChildGuids(
      db,
      lazy.PlacesUtils.bookmarks.mobileGuid
    );
    let hasMobileBookmarks = !!mobileChildGuids.length;

    Services.prefs.setBoolPref(MOBILE_BOOKMARKS_PREF, hasMobileBookmarks);
  },

  /**
   * Fetches an array of GUIDs for items that have an annotation set with the
   * given value.
   */
  async fetchGuidsWithAnno(anno, val) {
    let db = await lazy.PlacesUtils.promiseDBConnection();
    return fetchGuidsWithAnno(db, anno, val);
  },
}));

PlacesSyncUtils.test = {};
PlacesSyncUtils.test.bookmarks = Object.freeze({
  /**
   * Inserts a synced bookmark into the tree. Only SYNC TESTS should call this
   * method; other callers should use `PlacesUtils.bookmarks.insert`.
   *
   * It is in this file rather than a test-only file because it makes use of
   * other internal functions here, so moving is not trivial - see bug 1662602.
   *
   * The following properties are supported:
   *  - kind: Required.
   *  - guid: Required.
   *  - parentGuid: Required.
   *  - url: Required for bookmarks.
   *  - tags: An optional array of tag strings.
   *  - keyword: An optional keyword string.
   *
   * Sync doesn't set the index, since it appends and reorders children
   * after applying all incoming items.
   *
   * @param info
   *        object representing a synced bookmark.
   *
   * @return {Promise} resolved when the creation is complete.
   * @resolves to an object representing the created bookmark.
   * @rejects if it's not possible to create the requested bookmark.
   * @throws if the arguments are invalid.
   */
  insert(info) {
    let insertInfo = validateNewBookmark("BookmarkTestUtils: insert", info);

    return lazy.PlacesUtils.withConnectionWrapper(
      "BookmarkTestUtils: insert",
      async db => {
        // If we're inserting a tag query, make sure the tag exists and fix the
        // folder ID to refer to the local tag folder.
        insertInfo = await updateTagQueryFolder(db, insertInfo);

        let bookmarkInfo = syncBookmarkToPlacesBookmark(insertInfo);
        let bookmarkItem = await lazy.PlacesUtils.bookmarks.insert(
          bookmarkInfo
        );
        let newItem = await insertBookmarkMetadata(
          db,
          bookmarkItem,
          insertInfo
        );

        return newItem;
      }
    );
  },
});

XPCOMUtils.defineLazyGetter(lazy, "HistorySyncLog", () => {
  return lazy.Log.repository.getLogger("Sync.Engine.History.HistorySyncUtils");
});

XPCOMUtils.defineLazyGetter(lazy, "BookmarkSyncLog", () => {
  // Use a sub-log of the bookmarks engine, so setting the level for that
  // engine also adjust the level of this log.
  return lazy.Log.repository.getLogger(
    "Sync.Engine.Bookmarks.BookmarkSyncUtils"
  );
});

function validateSyncBookmarkObject(name, input, behavior) {
  return lazy.PlacesUtils.validateItemProperties(
    name,
    lazy.PlacesUtils.SYNC_BOOKMARK_VALIDATORS,
    input,
    behavior
  );
}

// Validates a sync change record as returned by `pullChanges` and passed to
// `pushChanges`.
function validateChangeRecord(name, changeRecord, behavior) {
  return lazy.PlacesUtils.validateItemProperties(
    name,
    lazy.PlacesUtils.SYNC_CHANGE_RECORD_VALIDATORS,
    changeRecord,
    behavior
  );
}

// Similar to the private `fetchBookmarksByParent` implementation in
// `Bookmarks.jsm`.
var fetchChildGuids = async function(db, parentGuid) {
  let rows = await db.executeCached(
    `
    SELECT guid
    FROM moz_bookmarks
    WHERE parent = (
      SELECT id FROM moz_bookmarks WHERE guid = :parentGuid
    )
    ORDER BY position`,
    { parentGuid }
  );
  return rows.map(row => row.getResultByName("guid"));
};

// A helper for whenever we want to know if a GUID doesn't exist in the places
// database. Primarily used to detect orphans on incoming records.
var GUIDMissing = async function(guid) {
  try {
    await lazy.PlacesUtils.promiseItemId(guid);
    return false;
  } catch (ex) {
    if (ex.message == "no item found for the given GUID") {
      return true;
    }
    throw ex;
  }
};

// Legacy tag queries may use a `place:` URL that refers to the tag folder ID.
// When we apply a synced tag query from a remote client, we need to update the
// URL to point to the local tag.
function updateTagQueryFolder(db, info) {
  if (
    info.kind != BookmarkSyncUtils.KINDS.QUERY ||
    !info.folder ||
    !info.url ||
    info.url.protocol != "place:"
  ) {
    return info;
  }

  let params = new URLSearchParams(info.url.pathname);
  let type = +params.get("type");
  if (type != Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_CONTENTS) {
    return info;
  }

  lazy.BookmarkSyncLog.debug(
    `updateTagQueryFolder: Tag query folder: ${info.folder}`
  );

  // Rewrite the query to directly reference the tag.
  params.delete("queryType");
  params.delete("type");
  params.delete("folder");
  params.set("tag", info.folder);
  info.url = new URL(info.url.protocol + params);
  return info;
}

// Keywords are a 1 to 1 mapping between strings and pairs of (URL, postData).
// (the postData is not synced, so we ignore it). Sync associates keywords with
// bookmarks, which is not really accurate. -- We might already have a keyword
// with that name, or we might already have another bookmark with that URL with
// a different keyword, etc.
//
// If we don't handle those cases by removing the conflicting keywords first,
// the insertion  will fail, and the keywords will either be wrong, or missing.
// This function handles those cases.
function removeConflictingKeywords(bookmarkURL, newKeyword) {
  return lazy.PlacesUtils.withConnectionWrapper(
    "BookmarkSyncUtils: removeConflictingKeywords",
    async function(db) {
      let entryForURL = await lazy.PlacesUtils.keywords.fetch({
        url: bookmarkURL.href,
      });
      if (entryForURL && entryForURL.keyword !== newKeyword) {
        await lazy.PlacesUtils.keywords.remove({
          keyword: entryForURL.keyword,
          source: SOURCE_SYNC,
        });
        // This will cause us to reupload this record for this sync, but
        // without it, we will risk data corruption.
        await BookmarkSyncUtils.addSyncChangesForBookmarksWithURL(
          db,
          entryForURL.url,
          1
        );
      }
      if (!newKeyword) {
        return;
      }
      let entryForNewKeyword = await lazy.PlacesUtils.keywords.fetch({
        keyword: newKeyword,
      });
      if (entryForNewKeyword) {
        await lazy.PlacesUtils.keywords.remove({
          keyword: entryForNewKeyword.keyword,
          source: SOURCE_SYNC,
        });
        await BookmarkSyncUtils.addSyncChangesForBookmarksWithURL(
          db,
          entryForNewKeyword.url,
          1
        );
      }
    }
  );
}

// Sets annotations, keywords, and tags on a new bookmark. Returns a Sync
// bookmark object.
async function insertBookmarkMetadata(db, bookmarkItem, insertInfo) {
  let newItem = await placesBookmarkToSyncBookmark(db, bookmarkItem);

  try {
    newItem.tags = tagItem(bookmarkItem, insertInfo.tags);
  } catch (ex) {
    lazy.BookmarkSyncLog.warn(
      `insertBookmarkMetadata: Error tagging item ${insertInfo.recordId}`,
      ex
    );
  }

  if (insertInfo.keyword) {
    await removeConflictingKeywords(bookmarkItem.url, insertInfo.keyword);
    await lazy.PlacesUtils.keywords.insert({
      keyword: insertInfo.keyword,
      url: bookmarkItem.url.href,
      source: SOURCE_SYNC,
    });
    newItem.keyword = insertInfo.keyword;
  }

  return newItem;
}

// Determines the Sync record kind for an existing bookmark.
async function getKindForItem(db, item) {
  switch (item.type) {
    case lazy.PlacesUtils.bookmarks.TYPE_FOLDER: {
      return BookmarkSyncUtils.KINDS.FOLDER;
    }
    case lazy.PlacesUtils.bookmarks.TYPE_BOOKMARK:
      return item.url.protocol == "place:"
        ? BookmarkSyncUtils.KINDS.QUERY
        : BookmarkSyncUtils.KINDS.BOOKMARK;

    case lazy.PlacesUtils.bookmarks.TYPE_SEPARATOR:
      return BookmarkSyncUtils.KINDS.SEPARATOR;
  }
  return null;
}

// Returns the `nsINavBookmarksService` bookmark type constant for a Sync
// record kind.
function getTypeForKind(kind) {
  switch (kind) {
    case BookmarkSyncUtils.KINDS.BOOKMARK:
    case BookmarkSyncUtils.KINDS.QUERY:
      return lazy.PlacesUtils.bookmarks.TYPE_BOOKMARK;

    case BookmarkSyncUtils.KINDS.FOLDER:
      return lazy.PlacesUtils.bookmarks.TYPE_FOLDER;

    case BookmarkSyncUtils.KINDS.SEPARATOR:
      return lazy.PlacesUtils.bookmarks.TYPE_SEPARATOR;
  }
  throw new Error(`Unknown bookmark kind: ${kind}`);
}

function validateNewBookmark(name, info) {
  let insertInfo = validateSyncBookmarkObject(name, info, {
    kind: { required: true },
    recordId: { required: true },
    url: {
      requiredIf: b =>
        [
          BookmarkSyncUtils.KINDS.BOOKMARK,
          BookmarkSyncUtils.KINDS.QUERY,
        ].includes(b.kind),
      validIf: b =>
        [
          BookmarkSyncUtils.KINDS.BOOKMARK,
          BookmarkSyncUtils.KINDS.QUERY,
        ].includes(b.kind),
    },
    parentRecordId: { required: true },
    title: {
      validIf: b =>
        [
          BookmarkSyncUtils.KINDS.BOOKMARK,
          BookmarkSyncUtils.KINDS.QUERY,
          BookmarkSyncUtils.KINDS.FOLDER,
        ].includes(b.kind) || b.title === "",
    },
    query: { validIf: b => b.kind == BookmarkSyncUtils.KINDS.QUERY },
    folder: { validIf: b => b.kind == BookmarkSyncUtils.KINDS.QUERY },
    tags: {
      validIf: b =>
        [
          BookmarkSyncUtils.KINDS.BOOKMARK,
          BookmarkSyncUtils.KINDS.QUERY,
        ].includes(b.kind),
    },
    keyword: {
      validIf: b =>
        [
          BookmarkSyncUtils.KINDS.BOOKMARK,
          BookmarkSyncUtils.KINDS.QUERY,
        ].includes(b.kind),
    },
    dateAdded: { required: false },
  });

  return insertInfo;
}

async function fetchGuidsWithAnno(db, anno, val) {
  let rows = await db.executeCached(
    `
    SELECT b.guid FROM moz_items_annos a
    JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
    JOIN moz_bookmarks b ON b.id = a.item_id
    WHERE n.name = :anno AND
          a.content = :val`,
    { anno, val }
  );
  return rows.map(row => row.getResultByName("guid"));
}

function tagItem(item, tags) {
  if (!item.url) {
    return [];
  }

  // Remove leading and trailing whitespace, then filter out empty tags.
  let newTags = tags ? tags.map(tag => tag.trim()).filter(Boolean) : [];

  // Removing the last tagged item will also remove the tag. To preserve
  // tag IDs, we temporarily tag a dummy URI, ensuring the tags exist.
  let dummyURI = lazy.PlacesUtils.toURI("about:weave#BStore_tagURI");
  let bookmarkURI = lazy.PlacesUtils.toURI(item.url.href);
  if (newTags && newTags.length) {
    lazy.PlacesUtils.tagging.tagURI(dummyURI, newTags, SOURCE_SYNC);
  }
  lazy.PlacesUtils.tagging.untagURI(bookmarkURI, null, SOURCE_SYNC);
  if (newTags && newTags.length) {
    lazy.PlacesUtils.tagging.tagURI(bookmarkURI, newTags, SOURCE_SYNC);
  }
  lazy.PlacesUtils.tagging.untagURI(dummyURI, null, SOURCE_SYNC);

  return newTags;
}

// Converts a Places bookmark to a Sync bookmark. This function maps Places
// GUIDs to record IDs and filters out extra Places properties like date added,
// last modified, and index.
async function placesBookmarkToSyncBookmark(db, bookmarkItem) {
  let item = {};

  for (let prop in bookmarkItem) {
    switch (prop) {
      // Record IDs are identical to Places GUIDs for all items except roots.
      case "guid":
        item.recordId = BookmarkSyncUtils.guidToRecordId(bookmarkItem.guid);
        break;

      case "parentGuid":
        item.parentRecordId = BookmarkSyncUtils.guidToRecordId(
          bookmarkItem.parentGuid
        );
        break;

      // Sync uses kinds instead of types, which distinguish between folders,
      // livemarks, bookmarks, and queries.
      case "type":
        item.kind = await getKindForItem(db, bookmarkItem);
        break;

      case "title":
      case "url":
        item[prop] = bookmarkItem[prop];
        break;

      case "dateAdded":
        item[prop] = new Date(bookmarkItem[prop]).getTime();
        break;
    }
  }

  return item;
}

// Converts a Sync bookmark object to a Places bookmark or livemark object.
// This function maps record IDs to Places GUIDs, and filters out extra Sync
// properties like keywords, tags. Returns an object that can be passed to
// `PlacesUtils.bookmarks.{insert, update}`.
function syncBookmarkToPlacesBookmark(info) {
  let bookmarkInfo = {
    source: SOURCE_SYNC,
  };

  for (let prop in info) {
    switch (prop) {
      case "kind":
        bookmarkInfo.type = getTypeForKind(info.kind);
        break;

      // Convert record IDs to Places GUIDs for roots.
      case "recordId":
        bookmarkInfo.guid = BookmarkSyncUtils.recordIdToGuid(info.recordId);
        break;

      case "dateAdded":
        bookmarkInfo.dateAdded = new Date(info.dateAdded);
        break;

      case "parentRecordId":
        bookmarkInfo.parentGuid = BookmarkSyncUtils.recordIdToGuid(
          info.parentRecordId
        );
        // Instead of providing an index, Sync reorders children at the end of
        // the sync using `BookmarkSyncUtils.order`. We explicitly specify the
        // default index here to prevent `PlacesUtils.bookmarks.update` from
        // throwing.
        bookmarkInfo.index = lazy.PlacesUtils.bookmarks.DEFAULT_INDEX;
        break;

      case "title":
      case "url":
        bookmarkInfo[prop] = info[prop];
        break;
    }
  }

  return bookmarkInfo;
}

// Creates and returns a Sync bookmark object containing the bookmark's
// tags, keyword.
var fetchBookmarkItem = async function(db, bookmarkItem) {
  let item = await placesBookmarkToSyncBookmark(db, bookmarkItem);

  if (!item.title) {
    item.title = "";
  }

  item.tags = lazy.PlacesUtils.tagging.getTagsForURI(
    lazy.PlacesUtils.toURI(bookmarkItem.url)
  );

  let keywordEntry = await lazy.PlacesUtils.keywords.fetch({
    url: bookmarkItem.url,
  });
  if (keywordEntry) {
    item.keyword = keywordEntry.keyword;
  }

  return item;
};

// Creates and returns a Sync bookmark object containing the folder's children.
async function fetchFolderItem(db, bookmarkItem) {
  let item = await placesBookmarkToSyncBookmark(db, bookmarkItem);

  if (!item.title) {
    item.title = "";
  }

  let childGuids = await fetchChildGuids(db, bookmarkItem.guid);
  item.childRecordIds = childGuids.map(guid =>
    BookmarkSyncUtils.guidToRecordId(guid)
  );

  return item;
}

// Creates and returns a Sync bookmark object containing the query's tag
// folder name.
async function fetchQueryItem(db, bookmarkItem) {
  let item = await placesBookmarkToSyncBookmark(db, bookmarkItem);

  let params = new URLSearchParams(bookmarkItem.url.pathname);
  let tags = params.getAll("tag");
  if (tags.length == 1) {
    item.folder = tags[0];
  }

  return item;
}

function addRowToChangeRecords(row, changeRecords) {
  let guid = row.getResultByName("guid");
  if (!guid) {
    throw new Error(`Changed item missing GUID`);
  }
  let isTombstone = !!row.getResultByName("tombstone");
  let recordId = BookmarkSyncUtils.guidToRecordId(guid);
  if (recordId in changeRecords) {
    let existingRecord = changeRecords[recordId];
    if (existingRecord.tombstone == isTombstone) {
      // Should never happen: `moz_bookmarks.guid` has a unique index, and
      // `moz_bookmarks_deleted.guid` is the primary key.
      throw new Error(`Duplicate item or tombstone ${recordId} in changeset`);
    }
    if (!existingRecord.tombstone && isTombstone) {
      // Don't replace undeleted items with tombstones...
      lazy.BookmarkSyncLog.warn(
        "addRowToChangeRecords: Ignoring tombstone for undeleted item",
        recordId
      );
      return;
    }
    // ...But replace undeleted tombstones with items.
    lazy.BookmarkSyncLog.warn(
      "addRowToChangeRecords: Replacing tombstone for undeleted item",
      recordId
    );
  }
  let modifiedAsPRTime = row.getResultByName("modified");
  let modified = modifiedAsPRTime / MICROSECONDS_PER_SECOND;
  if (Number.isNaN(modified) || modified <= 0) {
    lazy.BookmarkSyncLog.error(
      "addRowToChangeRecords: Invalid modified date for " + recordId,
      modifiedAsPRTime
    );
    modified = 0;
  }
  changeRecords[recordId] = {
    modified,
    counter: row.getResultByName("syncChangeCounter"),
    status: row.getResultByName("syncStatus"),
    tombstone: isTombstone,
    synced: false,
  };
}

/**
 * Queries the database for synced bookmarks and tombstones, and returns a
 * changeset for the Sync bookmarks engine.
 *
 * @param db
 *        The Sqlite.jsm connection handle.
 * @param forGuids
 *        Fetch Sync tracking information for only the requested GUIDs.
 * @return {Promise} resolved once all items have been fetched.
 * @resolves to an object containing records for changed bookmarks, keyed by
 *           the record ID.
 */
var pullSyncChanges = async function(db, forGuids = []) {
  let changeRecords = {};

  let itemConditions = ["syncChangeCounter >= 1"];
  let tombstoneConditions = ["1 = 1"];
  if (forGuids.length) {
    let restrictToGuids = `guid IN (${forGuids
      .map(guid => JSON.stringify(guid))
      .join(",")})`;
    itemConditions.push(restrictToGuids);
    tombstoneConditions.push(restrictToGuids);
  }

  let rows = await db.executeCached(
    `
    WITH RECURSIVE
    syncedItems(id, guid, modified, syncChangeCounter, syncStatus) AS (
      SELECT b.id, b.guid, b.lastModified, b.syncChangeCounter, b.syncStatus
       FROM moz_bookmarks b
       WHERE b.guid IN ('menu________', 'toolbar_____', 'unfiled_____',
                        'mobile______')
      UNION ALL
      SELECT b.id, b.guid, b.lastModified, b.syncChangeCounter, b.syncStatus
      FROM moz_bookmarks b
      JOIN syncedItems s ON b.parent = s.id
    )
    SELECT guid, modified, syncChangeCounter, syncStatus, 0 AS tombstone
    FROM syncedItems
    WHERE ${itemConditions.join(" AND ")}
    UNION ALL
    SELECT guid, dateRemoved AS modified, 1 AS syncChangeCounter,
           :deletedSyncStatus, 1 AS tombstone
    FROM moz_bookmarks_deleted
    WHERE ${tombstoneConditions.join(" AND ")}`,
    { deletedSyncStatus: lazy.PlacesUtils.bookmarks.SYNC_STATUS.NORMAL }
  );
  for (let row of rows) {
    addRowToChangeRecords(row, changeRecords);
  }

  return changeRecords;
};

// Moves a synced folder's remaining children to its parent, and deletes the
// folder if it's empty.
async function deleteSyncedFolder(db, bookmarkItem) {
  // At this point, any member in the folder that remains is either a folder
  // pending deletion (which we'll get to in this function), or an item that
  // should not be deleted. To avoid deleting these items, we first move them
  // to the parent of the folder we're about to delete.
  let childGuids = await fetchChildGuids(db, bookmarkItem.guid);
  if (!childGuids.length) {
    // No children -- just delete the folder.
    return deleteSyncedAtom(bookmarkItem);
  }

  if (lazy.BookmarkSyncLog.level <= lazy.Log.Level.Trace) {
    lazy.BookmarkSyncLog.trace(
      `deleteSyncedFolder: Moving ${JSON.stringify(childGuids)} children of ` +
        `"${bookmarkItem.guid}" to grandparent
      "${BookmarkSyncUtils.guidToRecordId(bookmarkItem.parentGuid)}" before ` +
        `deletion`
    );
  }

  // Move children out of the parent and into the grandparent
  for (let guid of childGuids) {
    await lazy.PlacesUtils.bookmarks.update({
      guid,
      parentGuid: bookmarkItem.parentGuid,
      index: lazy.PlacesUtils.bookmarks.DEFAULT_INDEX,
      // `SYNC_REPARENT_REMOVED_FOLDER_CHILDREN` bumps the change counter for
      // the child and its new parent, without incrementing the bookmark
      // tracker's score.
      //
      // We intentionally don't check if the child is one we'll remove later,
      // so it's possible we'll bump the change counter of the closest living
      // ancestor when it's not needed. This avoids inconsistency if removal
      // is interrupted, since we don't run this operation in a transaction.
      source:
        lazy.PlacesUtils.bookmarks.SOURCES
          .SYNC_REPARENT_REMOVED_FOLDER_CHILDREN,
    });
  }

  // Delete the (now empty) parent
  try {
    await lazy.PlacesUtils.bookmarks.remove(bookmarkItem.guid, {
      preventRemovalOfNonEmptyFolders: true,
      // We don't want to bump the change counter for this deletion, because
      // a tombstone for the folder is already on the server.
      source: SOURCE_SYNC,
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
    lazy.BookmarkSyncLog.trace(
      `deleteSyncedFolder: Error removing parent ` +
        `${bookmarkItem.guid} after reparenting children`,
      e
    );
    return false;
  }

  return true;
}

// Removes a synced bookmark or empty folder from the database.
var deleteSyncedAtom = async function(bookmarkItem) {
  try {
    await lazy.PlacesUtils.bookmarks.remove(bookmarkItem.guid, {
      preventRemovalOfNonEmptyFolders: true,
      source: SOURCE_SYNC,
    });
  } catch (ex) {
    // Likely already removed.
    lazy.BookmarkSyncLog.trace(
      `deleteSyncedAtom: Error removing ` + bookmarkItem.guid,
      ex
    );
    return false;
  }

  return true;
};

/**
 * Updates the sync status on all "NEW" and "UNKNOWN" bookmarks to "NORMAL".
 *
 * We do this when pulling changes instead of in `pushChanges` to make sure
 * we write tombstones if a new item is deleted after an interrupted sync. (For
 * example, if a "NEW" record is uploaded or reconciled, then the app is closed
 * before Sync calls `pushChanges`).
 */
function markChangesAsSyncing(db, changeRecords) {
  let unsyncedGuids = [];
  for (let recordId in changeRecords) {
    if (changeRecords[recordId].tombstone) {
      continue;
    }
    if (
      changeRecords[recordId].status ==
      lazy.PlacesUtils.bookmarks.SYNC_STATUS.NORMAL
    ) {
      continue;
    }
    let guid = BookmarkSyncUtils.recordIdToGuid(recordId);
    unsyncedGuids.push(JSON.stringify(guid));
  }
  if (!unsyncedGuids.length) {
    return Promise.resolve();
  }
  return db.execute(
    `
    UPDATE moz_bookmarks
    SET syncStatus = :syncStatus
    WHERE guid IN (${unsyncedGuids.join(",")})`,
    { syncStatus: lazy.PlacesUtils.bookmarks.SYNC_STATUS.NORMAL }
  );
}

/**
 * Removes tombstones for successfully synced items.
 *
 * @return {Promise}
 */
var removeTombstones = function(db, guids) {
  if (!guids.length) {
    return Promise.resolve();
  }
  return db.execute(`
    DELETE FROM moz_bookmarks_deleted
    WHERE guid IN (${guids.map(guid => JSON.stringify(guid)).join(",")})`);
};

/**
 * Removes tombstones for successfully synced items where the specified GUID
 * exists in *both* the bookmarks and tombstones tables.
 *
 * @return {Promise}
 */
var removeUndeletedTombstones = function(db, guids) {
  if (!guids.length) {
    return Promise.resolve();
  }
  // sqlite can't join in a DELETE, so we use a subquery.
  return db.execute(`
    DELETE FROM moz_bookmarks_deleted
    WHERE guid IN (${guids.map(guid => JSON.stringify(guid)).join(",")})
    AND guid IN (SELECT guid from moz_bookmarks)`);
};

// Sets the history sync ID and clears the last sync time.
async function setHistorySyncId(db, newSyncId) {
  await lazy.PlacesUtils.metadata.setWithConnection(
    db,
    HistorySyncUtils.SYNC_ID_META_KEY,
    newSyncId
  );

  await lazy.PlacesUtils.metadata.deleteWithConnection(
    db,
    HistorySyncUtils.LAST_SYNC_META_KEY
  );
}

// Sets the bookmarks sync ID and clears the last sync time.
async function setBookmarksSyncId(db, newSyncId) {
  await lazy.PlacesUtils.metadata.setWithConnection(
    db,
    BookmarkSyncUtils.SYNC_ID_META_KEY,
    newSyncId
  );

  await lazy.PlacesUtils.metadata.deleteWithConnection(
    db,
    BookmarkSyncUtils.LAST_SYNC_META_KEY,
    BookmarkSyncUtils.WIPE_REMOTE_META_KEY
  );
}

// Bumps the change counter and sets the given sync status for all bookmarks,
// removes all orphan annos, and drops stale tombstones.
async function resetAllSyncStatuses(db, syncStatus) {
  await db.execute(
    `
    UPDATE moz_bookmarks
    SET syncChangeCounter = 1,
        syncStatus = :syncStatus`,
    { syncStatus }
  );

  // The orphan anno isn't meaningful after a restore, disconnect, or node
  // reassignment.
  await db.execute(
    `
    DELETE FROM moz_items_annos
    WHERE anno_attribute_id = (SELECT id FROM moz_anno_attributes
                               WHERE name = :orphanAnno)`,
    { orphanAnno: BookmarkSyncUtils.SYNC_PARENT_ANNO }
  );

  // Drop stale tombstones.
  await db.execute("DELETE FROM moz_bookmarks_deleted");
}
