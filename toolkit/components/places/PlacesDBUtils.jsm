/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const BYTES_PER_MEBIBYTE = 1048576;

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
});

var EXPORTED_SYMBOLS = [ "PlacesDBUtils" ];

var PlacesDBUtils = {
  _isShuttingDown: false,
  shutdown() {
    PlacesDBUtils._isShuttingDown = true;
  },

  _clearTaskQueue: false,
  clearPendingTasks() {
    PlacesDBUtils._clearTaskQueue = true;
  },

  /**
   * Executes integrity check and common maintenance tasks.
   *
   * @return a Map[taskName(String) -> Object]. The Object has the following properties:
   *         - succeeded: boolean
   *         - logs: an array of strings containing the messages logged by the task.
   */
  async maintenanceOnIdle() {
    let tasks = [
      this.checkIntegrity,
      this.invalidateCaches,
      this.checkCoherence,
      this._refreshUI,
      this.incrementalVacuum
    ];
    let telemetryStartTime = Date.now();
    let taskStatusMap = await PlacesDBUtils.runTasks(tasks);

    Services.prefs.setIntPref("places.database.lastMaintenance",
                               parseInt(Date.now() / 1000));
    Services.telemetry.getHistogramById("PLACES_IDLE_MAINTENANCE_TIME_MS")
                      .add(Date.now() - telemetryStartTime);
    return taskStatusMap;
  },

  /**
   * Executes integrity check, common and advanced maintenance tasks (like
   * expiration and vacuum).  Will also collect statistics on the database.
   *
   * Note: although this function isn't actually async, we keep it async to
   * allow us to maintain a simple, consistent API for the tasks within this object.
   *
   * @return {Promise}
   *        A promise that resolves with a Map[taskName(String) -> Object].
   *        The Object has the following properties:
   *         - succeeded: boolean
   *         - logs: an array of strings containing the messages logged by the task.
   */
  async checkAndFixDatabase() {
    let tasks = [
      this.checkIntegrity,
      this.invalidateCaches,
      this.checkCoherence,
      this.expire,
      this.vacuum,
      this.stats,
      this._refreshUI,
    ];
    return PlacesDBUtils.runTasks(tasks);
  },

  /**
   * Forces a full refresh of Places views.
   *
   * Note: although this function isn't actually async, we keep it async to
   * allow us to maintain a simple, consistent API for the tasks within this object.
   *
   * @returns {Array} An empty array.
   */
  async _refreshUI() {
    // Send batch update notifications to update the UI.
    let observers = PlacesUtils.history.getObservers();
    for (let observer of observers) {
      observer.onBeginUpdateBatch();
      observer.onEndUpdateBatch();
    }
    return [];
  },

  /**
   * Checks integrity and tries to fix the database through a reindex.
   *
   * @return {Promise} resolves if database is sane or is made sane.
   * @resolves to an array of logs for this task.
   * @rejects if we're unable to fix corruption or unable to check status.
   */
  async checkIntegrity() {
    let logs = [];

    async function integrity(db) {
      let row;
      await db.execute("PRAGMA integrity_check", null, (r, cancel) => {
        row = r;
        cancel();
      });
      return row.getResultByIndex(0) === "ok";
    }
    try {
      // Run a integrity check, but stop at the first error.
      await PlacesUtils.withConnectionWrapper("PlacesDBUtils: check the integrity", async db => {
        let isOk = await integrity(db);
        if (isOk) {
          logs.push("The database is sane");
        } else {
          // We stopped due to an integrity corruption, try to fix if possible.
          logs.push("The database is corrupt");
          // Try to reindex, this often fixes simple indices corruption.
          await db.execute("REINDEX");
          logs.push("The database has been REINDEXed");
          isOk = await integrity(db);
          if (isOk) {
            logs.push("The database is now sane");
          } else {
            logs.push("The database is still corrupt");
            Services.prefs.setBoolPref("places.database.replaceOnStartup", true);
            PlacesDBUtils.clearPendingTasks();
            throw new Error("Unable to fix corruption, database will be replaced on next startup");
          }
        }
      });
    } catch (ex) {
      if (ex.message.indexOf("Unable to fix corruption") !== 0) {
        // There was some other error, so throw.
        PlacesDBUtils.clearPendingTasks();
        throw new Error("Unable to check database integrity");
      }
    }
    return logs;
  },

  invalidateCaches() {
    let logs = [];
    return PlacesUtils.withConnectionWrapper("PlacesDBUtils: invalidate caches", async db => {
      let idsWithInvalidGuidsRows = await db.execute(`
        SELECT id FROM moz_bookmarks
        WHERE guid IS NULL OR
              NOT IS_VALID_GUID(guid)`);
      for (let row of idsWithInvalidGuidsRows) {
        let id = row.getResultByName("id");
        PlacesUtils.invalidateCachedGuidFor(id);
      }
      logs.push("The caches have been invalidated");
      return logs;
    }).catch(ex => {
      PlacesDBUtils.clearPendingTasks();
      throw new Error("Unable to invalidate caches");
    });
  },

  /**
   * Checks data coherence and tries to fix most common errors.
   *
   * @return {Promise} resolves when coherence is checked.
   * @resolves to an array of logs for this task.
   * @rejects if database is not coherent.
   */
  async checkCoherence() {
    let logs = [];
    let stmts = await PlacesDBUtils._getCoherenceStatements();
    let coherenceCheck = true;
    await PlacesUtils.withConnectionWrapper(
      "PlacesDBUtils: coherence check:",
      db => db.executeTransaction(async () => {
        for (let {query, params} of stmts) {
          try {
            await db.execute(query, params || null);
          } catch (ex) {
            Cu.reportError(ex);
            coherenceCheck = false;
          }
        }
      })
    );

    if (coherenceCheck) {
      logs.push("The database is coherent");
    } else {
      PlacesDBUtils.clearPendingTasks();
      throw new Error("Unable to complete the coherence check");
    }
    return logs;
  },

  /**
   * Runs incremental vacuum on databases supporting it.
   *
   * @return {Promise} resolves when done.
   * @resolves to an array of logs for this task.
   * @rejects if we were unable to vacuum.
   */
  async incrementalVacuum() {
    let logs = [];
    return PlacesUtils.withConnectionWrapper("PlacesDBUtils: incrementalVacuum",
      async db => {
        let count = (await db.execute("PRAGMA favicons.freelist_count"))[0].getResultByIndex(0);
        if (count < 10) {
          logs.push(`The favicons database has only ${count} free pages, not vacuuming.`);
        } else {
          logs.push(`The favicons database has ${count} free pages, vacuuming.`);
          await db.execute("PRAGMA favicons.incremental_vacuum");
          count = (await db.execute("PRAGMA favicons.freelist_count"))[0].getResultByIndex(0);
          logs.push(`The database has been vacuumed and has now ${count} free pages.`);
        }
        return logs;
      }).catch(ex => {
        PlacesDBUtils.clearPendingTasks();
        throw new Error("Unable to incrementally vacuum the favicons database " + ex);
      });
  },

  async _getCoherenceStatements() {
    let cleanupStatements = [
      // MOZ_ANNO_ATTRIBUTES
      // A.1 remove obsolete annotations from moz_annos.
      // The 'weave0' idiom exploits character ordering (0 follows /) to
      // efficiently select all annos with a 'weave/' prefix.
      { query:
        `DELETE FROM moz_annos
        WHERE type = 4 OR anno_attribute_id IN (
          SELECT id FROM moz_anno_attributes
          WHERE name = 'downloads/destinationFileName' OR
                name BETWEEN 'weave/' AND 'weave0'
        )`
      },

      // A.2 remove obsolete annotations from moz_items_annos.
      { query:
        `DELETE FROM moz_items_annos
        WHERE type = 4 OR anno_attribute_id IN (
          SELECT id FROM moz_anno_attributes
          WHERE name = 'sync/children'
             OR name = 'placesInternal/GUID'
             OR name BETWEEN 'weave/' AND 'weave0'
        )`
      },

      // A.3 remove unused attributes.
      { query:
        `DELETE FROM moz_anno_attributes WHERE id IN (
          SELECT id FROM moz_anno_attributes n
          WHERE NOT EXISTS
              (SELECT id FROM moz_annos WHERE anno_attribute_id = n.id LIMIT 1)
            AND NOT EXISTS
              (SELECT id FROM moz_items_annos WHERE anno_attribute_id = n.id LIMIT 1)
        )`
      },

      // MOZ_ANNOS
      // B.1 remove annos with an invalid attribute
      { query:
        `DELETE FROM moz_annos WHERE id IN (
          SELECT id FROM moz_annos a
          WHERE NOT EXISTS
            (SELECT id FROM moz_anno_attributes
              WHERE id = a.anno_attribute_id LIMIT 1)
        )`
      },

      // B.2 remove orphan annos
      { query:
        `DELETE FROM moz_annos WHERE id IN (
          SELECT id FROM moz_annos a
          WHERE NOT EXISTS
            (SELECT id FROM moz_places WHERE id = a.place_id LIMIT 1)
        )`
      },

      // C.1 Fix built-in folders with incorrect parents.
      { query:
        `UPDATE moz_bookmarks SET parent = :rootId
         WHERE guid IN (
           :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid, :mobileGuid
         ) AND parent <> :rootId`,
        params: {
          rootId: PlacesUtils.placesRootId,
          menuGuid: PlacesUtils.bookmarks.menuGuid,
          toolbarGuid: PlacesUtils.bookmarks.toolbarGuid,
          unfiledGuid: PlacesUtils.bookmarks.unfiledGuid,
          tagsGuid: PlacesUtils.bookmarks.tagsGuid,
          mobileGuid: PlacesUtils.bookmarks.mobileGuid,
        }
      },

      // D.1 remove items without a valid place
      // If fk IS NULL we fix them in D.7
      { query:
        `DELETE FROM moz_bookmarks WHERE guid NOT IN (
          :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
        ) AND id IN (
          SELECT b.id FROM moz_bookmarks b
          WHERE fk NOT NULL AND b.type = :bookmark_type
            AND NOT EXISTS (SELECT url FROM moz_places WHERE id = b.fk LIMIT 1)
        )`,
        params: {
          bookmark_type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          rootGuid: PlacesUtils.bookmarks.rootGuid,
          menuGuid: PlacesUtils.bookmarks.menuGuid,
          toolbarGuid: PlacesUtils.bookmarks.toolbarGuid,
          unfiledGuid: PlacesUtils.bookmarks.unfiledGuid,
          tagsGuid: PlacesUtils.bookmarks.tagsGuid,
        }
      },

      // D.2 remove items that are not uri bookmarks from tag containers
      { query:
        `DELETE FROM moz_bookmarks WHERE guid NOT IN (
          :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
        ) AND id IN (
          SELECT b.id FROM moz_bookmarks b
          WHERE b.parent IN
            (SELECT id FROM moz_bookmarks WHERE parent = :tags_folder)
            AND b.type <> :bookmark_type
        )`,
        params: {
          tags_folder: PlacesUtils.tagsFolderId,
          bookmark_type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          rootGuid: PlacesUtils.bookmarks.rootGuid,
          menuGuid: PlacesUtils.bookmarks.menuGuid,
          toolbarGuid: PlacesUtils.bookmarks.toolbarGuid,
          unfiledGuid: PlacesUtils.bookmarks.unfiledGuid,
          tagsGuid: PlacesUtils.bookmarks.tagsGuid,
        }
      },

      // D.3 remove empty tags
      { query:
        `DELETE FROM moz_bookmarks WHERE guid NOT IN (
          :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
        ) AND id IN (
          SELECT b.id FROM moz_bookmarks b
          WHERE b.id IN
            (SELECT id FROM moz_bookmarks WHERE parent = :tags_folder)
            AND NOT EXISTS
              (SELECT id from moz_bookmarks WHERE parent = b.id LIMIT 1)
        )`,
        params: {
          tags_folder: PlacesUtils.tagsFolderId,
          rootGuid: PlacesUtils.bookmarks.rootGuid,
          menuGuid: PlacesUtils.bookmarks.menuGuid,
          toolbarGuid: PlacesUtils.bookmarks.toolbarGuid,
          unfiledGuid: PlacesUtils.bookmarks.unfiledGuid,
          tagsGuid: PlacesUtils.bookmarks.tagsGuid,
        }
      },

      // D.4 move orphan items to unsorted folder
      { query:
        `UPDATE moz_bookmarks SET
          parent = :unsorted_folder,
          syncChangeCounter = syncChangeCounter + 1
        WHERE guid NOT IN (
          :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
        ) AND id IN (
          SELECT b.id FROM moz_bookmarks b
          WHERE NOT EXISTS
            (SELECT id FROM moz_bookmarks WHERE id = b.parent LIMIT 1)
        )`,
        params: {
          unsorted_folder: PlacesUtils.unfiledBookmarksFolderId,
          rootGuid: PlacesUtils.bookmarks.rootGuid,
          menuGuid: PlacesUtils.bookmarks.menuGuid,
          toolbarGuid: PlacesUtils.bookmarks.toolbarGuid,
          unfiledGuid: PlacesUtils.bookmarks.unfiledGuid,
          tagsGuid: PlacesUtils.bookmarks.tagsGuid,
        }
      },

      // D.6 fix wrong item types
      // Folders and separators should not have an fk.
      // If they have a valid fk convert them to bookmarks. Later in D.9 we
      // will move eventual children to unsorted bookmarks.
      { query:
        `UPDATE moz_bookmarks
        SET type = :bookmark_type,
            syncChangeCounter = syncChangeCounter + 1
        WHERE guid NOT IN (
          :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
        ) AND id IN (
          SELECT id FROM moz_bookmarks b
          WHERE type IN (:folder_type, :separator_type)
            AND fk NOTNULL
        )`,
        params: {
          bookmark_type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          folder_type: PlacesUtils.bookmarks.TYPE_FOLDER,
          separator_type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
          rootGuid: PlacesUtils.bookmarks.rootGuid,
          menuGuid: PlacesUtils.bookmarks.menuGuid,
          toolbarGuid: PlacesUtils.bookmarks.toolbarGuid,
          unfiledGuid: PlacesUtils.bookmarks.unfiledGuid,
          tagsGuid: PlacesUtils.bookmarks.tagsGuid,
        }
      },

      // D.7 fix wrong item types
      // Bookmarks should have an fk, if they don't have any, convert them to
      // folders.
      { query:
        `UPDATE moz_bookmarks
        SET type = :folder_type,
            syncChangeCounter = syncChangeCounter + 1
        WHERE guid NOT IN (
          :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
        ) AND id IN (
          SELECT id FROM moz_bookmarks b
          WHERE type = :bookmark_type
            AND fk IS NULL
        )`,
        params: {
          bookmark_type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          folder_type: PlacesUtils.bookmarks.TYPE_FOLDER,
          rootGuid: PlacesUtils.bookmarks.rootGuid,
          menuGuid: PlacesUtils.bookmarks.menuGuid,
          toolbarGuid: PlacesUtils.bookmarks.toolbarGuid,
          unfiledGuid: PlacesUtils.bookmarks.unfiledGuid,
          tagsGuid: PlacesUtils.bookmarks.tagsGuid,
        }
      },

      // D.9 fix wrong parents
      // Items cannot have separators or other bookmarks
      // as parent, if they have bad parent move them to unsorted bookmarks.
      { query:
        `UPDATE moz_bookmarks SET
          parent = :unsorted_folder,
          syncChangeCounter = syncChangeCounter + 1
        WHERE guid NOT IN (
          :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
        ) AND id IN (
          SELECT id FROM moz_bookmarks b
          WHERE EXISTS
            (SELECT id FROM moz_bookmarks WHERE id = b.parent
              AND type IN (:bookmark_type, :separator_type)
              LIMIT 1)
        )`,
        params: {
          unsorted_folder: PlacesUtils.unfiledBookmarksFolderId,
          bookmark_type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          separator_type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
          rootGuid: PlacesUtils.bookmarks.rootGuid,
          menuGuid: PlacesUtils.bookmarks.menuGuid,
          toolbarGuid: PlacesUtils.bookmarks.toolbarGuid,
          unfiledGuid: PlacesUtils.bookmarks.unfiledGuid,
          tagsGuid: PlacesUtils.bookmarks.tagsGuid,
        }
      },

      // D.10 recalculate positions
      // This requires multiple related statements.
      // We can detect a folder with bad position values comparing the sum of
      // all distinct position values (+1 since position is 0-based) with the
      // triangular numbers obtained by the number of children (n).
      // SUM(DISTINCT position + 1) == (n * (n + 1) / 2).
      // id is not a PRIMARY KEY on purpose, since we need a rowid that
      // increments monotonically.
      { query:
        `CREATE TEMP TABLE IF NOT EXISTS moz_bm_reindex_temp (
          id INTEGER
        , parent INTEGER
        , position INTEGER
        )`
      },
      { query:
        `INSERT INTO moz_bm_reindex_temp
        SELECT id, parent, 0
        FROM moz_bookmarks b
        WHERE parent IN (
          SELECT parent
          FROM moz_bookmarks
          GROUP BY parent
          HAVING (SUM(DISTINCT position + 1) - (count(*) * (count(*) + 1) / 2)) <> 0
        )
        ORDER BY parent ASC, position ASC, ROWID ASC`
      },
      { query:
        `CREATE INDEX IF NOT EXISTS moz_bm_reindex_temp_index
        ON moz_bm_reindex_temp(parent)`
      },
      { query:
        `UPDATE moz_bm_reindex_temp SET position = (
          ROWID - (SELECT MIN(t.ROWID) FROM moz_bm_reindex_temp t
                    WHERE t.parent = moz_bm_reindex_temp.parent)
        )`
      },
      { query:
        `CREATE TEMP TRIGGER IF NOT EXISTS moz_bm_reindex_temp_trigger
        BEFORE DELETE ON moz_bm_reindex_temp
        FOR EACH ROW
        BEGIN
          UPDATE moz_bookmarks SET position = OLD.position WHERE id = OLD.id;
        END`
      },
      { query: `DELETE FROM moz_bm_reindex_temp` },
      { query: `DROP INDEX moz_bm_reindex_temp_index` },
      { query: `DROP TRIGGER moz_bm_reindex_temp_trigger` },
      { query: `DROP TABLE moz_bm_reindex_temp` },

      // D.12 Fix empty-named tags.
      // Tags were allowed to have empty names due to a UI bug.  Fix them by
      // replacing their title with "(notitle)", and bumping the change counter
      // for all bookmarks with the fixed tags.
      { query:
        `UPDATE moz_bookmarks SET syncChangeCounter = syncChangeCounter + 1
         WHERE fk IN (SELECT b.fk FROM moz_bookmarks b
                      JOIN moz_bookmarks p ON p.id = b.parent
                      WHERE length(p.title) = 0 AND p.type = :folder_type AND
                            p.parent = :tags_folder)`,
        params: {
          folder_type: PlacesUtils.bookmarks.TYPE_FOLDER,
          tags_folder: PlacesUtils.tagsFolderId,
        },
      },
      { query:
        `UPDATE moz_bookmarks SET title = :empty_title
        WHERE length(title) = 0 AND type = :folder_type
          AND parent = :tags_folder`,
        params: {
          empty_title: "(notitle)",
          folder_type: PlacesUtils.bookmarks.TYPE_FOLDER,
          tags_folder: PlacesUtils.tagsFolderId,
        }
      },

      // MOZ_ICONS
      // E.1 remove orphan icon entries.
      { query:
        `DELETE FROM moz_pages_w_icons WHERE page_url_hash NOT IN (
          SELECT url_hash FROM moz_places
        )`
      },

      { query:
        `DELETE FROM moz_icons WHERE id IN (
          SELECT id FROM moz_icons WHERE root = 0
          EXCEPT
          SELECT icon_id FROM moz_icons_to_pages
        )`
      },

      // MOZ_HISTORYVISITS
      // F.1 remove orphan visits
      { query:
        `DELETE FROM moz_historyvisits WHERE id IN (
          SELECT id FROM moz_historyvisits v
          WHERE NOT EXISTS
            (SELECT id FROM moz_places WHERE id = v.place_id LIMIT 1)
        )`
      },

      // MOZ_INPUTHISTORY
      // G.1 remove orphan input history
      { query:
        `DELETE FROM moz_inputhistory WHERE place_id IN (
          SELECT place_id FROM moz_inputhistory i
          WHERE NOT EXISTS
            (SELECT id FROM moz_places WHERE id = i.place_id LIMIT 1)
        )`
      },

      // MOZ_ITEMS_ANNOS
      // H.1 remove item annos with an invalid attribute
      { query:
        `DELETE FROM moz_items_annos WHERE id IN (
          SELECT id FROM moz_items_annos t
          WHERE NOT EXISTS
            (SELECT id FROM moz_anno_attributes
              WHERE id = t.anno_attribute_id LIMIT 1)
        )`
      },

      // H.2 remove orphan item annos
      { query:
        `DELETE FROM moz_items_annos WHERE id IN (
          SELECT id FROM moz_items_annos t
          WHERE NOT EXISTS
            (SELECT id FROM moz_bookmarks WHERE id = t.item_id LIMIT 1)
        )`
      },

      // MOZ_KEYWORDS
      // I.1 remove unused keywords
      { query:
        `DELETE FROM moz_keywords WHERE id IN (
          SELECT id FROM moz_keywords k
          WHERE NOT EXISTS
            (SELECT 1 FROM moz_places h WHERE k.place_id = h.id)
        )`
      },

      // MOZ_PLACES
      // L.2 recalculate visit_count and last_visit_date
      { query:
        `UPDATE moz_places
        SET visit_count = (SELECT count(*) FROM moz_historyvisits
                            WHERE place_id = moz_places.id AND visit_type NOT IN (0,4,7,8,9)),
            last_visit_date = (SELECT MAX(visit_date) FROM moz_historyvisits
                                WHERE place_id = moz_places.id)
        WHERE id IN (
          SELECT h.id FROM moz_places h
          WHERE visit_count <> (SELECT count(*) FROM moz_historyvisits v
                                WHERE v.place_id = h.id AND visit_type NOT IN (0,4,7,8,9))
              OR last_visit_date <> (SELECT MAX(visit_date) FROM moz_historyvisits v
                                    WHERE v.place_id = h.id)
        )`
      },

      // L.3 recalculate hidden for redirects.
      { query:
        `UPDATE moz_places
        SET hidden = 1
        WHERE id IN (
          SELECT h.id FROM moz_places h
          JOIN moz_historyvisits src ON src.place_id = h.id
          JOIN moz_historyvisits dst ON dst.from_visit = src.id AND dst.visit_type IN (5,6)
          LEFT JOIN moz_bookmarks on fk = h.id AND fk ISNULL
          GROUP BY src.place_id HAVING count(*) = visit_count
        )`
      },

      // L.4 recalculate foreign_count.
      { query:
        `UPDATE moz_places SET foreign_count =
          (SELECT count(*) FROM moz_bookmarks WHERE fk = moz_places.id ) +
          (SELECT count(*) FROM moz_keywords WHERE place_id = moz_places.id )`
      },

      // L.5 recalculate missing hashes.
      { query: `UPDATE moz_places SET url_hash = hash(url) WHERE url_hash = 0` },

      // L.6 fix invalid Place GUIDs.
      { query:
        `UPDATE moz_places
        SET guid = GENERATE_GUID()
        WHERE guid IS NULL OR
              NOT IS_VALID_GUID(guid)`
      },

      // MOZ_BOOKMARKS
      // S.1 fix invalid GUIDs for synced bookmarks.
      // This requires multiple related statements.
      // First, we insert tombstones for all synced bookmarks with invalid
      // GUIDs, so that we can delete them on the server. Second, we add a
      // temporary trigger to bump the change counter for the parents of any
      // items we update, since Sync stores the list of child GUIDs on the
      // parent. Finally, we assign new GUIDs for all items with missing and
      // invalid GUIDs, bump their change counters, and reset their sync
      // statuses to NEW so that they're considered for deduping.
      { query:
        `INSERT OR IGNORE INTO moz_bookmarks_deleted(guid, dateRemoved)
        SELECT guid, :dateRemoved
        FROM moz_bookmarks
        WHERE syncStatus <> :syncStatus AND
              guid NOT NULL AND
              NOT IS_VALID_GUID(guid)`,
        params: {
          dateRemoved: PlacesUtils.toPRTime(new Date()),
          syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NEW,
        },
      },
      { query:
        `UPDATE moz_bookmarks
        SET guid = GENERATE_GUID(),
            syncChangeCounter = syncChangeCounter + 1,
            syncStatus = :syncStatus
        WHERE guid IS NULL OR
              NOT IS_VALID_GUID(guid)`,
        params: {
          syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NEW,
        },
      },

      // S.2 drop tombstones for bookmarks that aren't deleted.
      { query:
        `DELETE FROM moz_bookmarks_deleted
        WHERE guid IN (SELECT guid FROM moz_bookmarks)`,
      },

      // S.3 set missing added and last modified dates.
      { query:
        `UPDATE moz_bookmarks
        SET dateAdded = COALESCE(NULLIF(dateAdded, 0), NULLIF(lastModified, 0), NULLIF((
              SELECT MIN(visit_date) FROM moz_historyvisits
              WHERE place_id = fk
            ), 0), STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000),
            lastModified = COALESCE(NULLIF(lastModified, 0), NULLIF(dateAdded, 0), NULLIF((
              SELECT MAX(visit_date) FROM moz_historyvisits
              WHERE place_id = fk
            ), 0), STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000)
        WHERE NULLIF(dateAdded, 0) IS NULL OR
              NULLIF(lastModified, 0) IS NULL`,
      },

      // S.4 reset added dates that are ahead of last modified dates.
      { query:
        `UPDATE moz_bookmarks
         SET dateAdded = lastModified
         WHERE dateAdded > lastModified`,
      },
    ];

    // Create triggers for updating Sync metadata. The "sync change" trigger
    // bumps the parent's change counter when we update a GUID or move an item
    // to a different folder, since Sync stores the list of child GUIDs on the
    // parent. The "sync tombstone" trigger inserts tombstones for deleted
    // synced bookmarks.
    cleanupStatements.unshift({
      query:
      `CREATE TEMP TRIGGER IF NOT EXISTS moz_bm_sync_change_temp_trigger
      AFTER UPDATE of guid, parent, position ON moz_bookmarks
      FOR EACH ROW
      BEGIN
        UPDATE moz_bookmarks
        SET syncChangeCounter = syncChangeCounter + 1
        WHERE id IN (OLD.parent, NEW.parent);
      END`
    });
    cleanupStatements.unshift({
      query:
      `CREATE TEMP TRIGGER IF NOT EXISTS moz_bm_sync_tombstone_temp_trigger
      AFTER DELETE ON moz_bookmarks
      FOR EACH ROW WHEN OLD.guid NOT NULL AND
                        OLD.syncStatus <> 1
      BEGIN
        UPDATE moz_bookmarks
        SET syncChangeCounter = syncChangeCounter + 1
        WHERE id = OLD.parent;

        INSERT INTO moz_bookmarks_deleted(guid, dateRemoved)
        VALUES(OLD.guid, STRFTIME('%s', 'now', 'localtime', 'utc') * 1000000);
      END`
    });
    cleanupStatements.push({ query: `DROP TRIGGER moz_bm_sync_change_temp_trigger` });
    cleanupStatements.push({ query: `DROP TRIGGER moz_bm_sync_tombstone_temp_trigger` });

    return cleanupStatements;
  },

  /**
   * Tries to vacuum the database.
   *
   * Note: although this function isn't actually async, we keep it async to
   * allow us to maintain a simple, consistent API for the tasks within this object.
   *
   * @return {Promise} resolves when database is vacuumed.
   * @resolves to an array of logs for this task.
   * @rejects if we are unable to vacuum database.
   */
  async vacuum() {
    let logs = [];
    let placesDbPath = OS.Path.join(OS.Constants.Path.profileDir, "places.sqlite");
    let info = await OS.File.stat(placesDbPath);
    logs.push(`Initial database size is ${parseInt(info.size / 1024)}KiB`);
    return PlacesUtils.withConnectionWrapper("PlacesDBUtils: vacuum", async db => {
      await db.execute("VACUUM");
      logs.push("The database has been vacuumed");
      info = await OS.File.stat(placesDbPath);
      logs.push(`Final database size is ${parseInt(info.size / 1024)}KiB`);
      return logs;
    }).catch(() => {
      PlacesDBUtils.clearPendingTasks();
      throw new Error("Unable to vacuum database");
    });
  },

  /**
   * Forces a full expiration on the database.
   *
   * Note: although this function isn't actually async, we keep it async to
   * allow us to maintain a simple, consistent API for the tasks within this object.
   *
   * @return {Promise} resolves when the database in cleaned up.
   * @resolves to an array of logs for this task.
   */
  async expire() {
    let logs = [];

    let expiration = Cc["@mozilla.org/places/expiration;1"]
                       .getService(Ci.nsIObserver);

    let returnPromise = new Promise(res => {
      let observer = (subject, topic, data) => {
        Services.obs.removeObserver(observer, topic);
        logs.push("Database cleaned up");
        res(logs);
      };
      Services.obs.addObserver(observer, PlacesUtils.TOPIC_EXPIRATION_FINISHED);
    });

    // Force an orphans expiration step.
    expiration.observe(null, "places-debug-start-expiration", 0);
    return returnPromise;
  },

  /**
   * Collects statistical data on the database.
   *
   * @return {Promise} resolves when statistics are collected.
   * @resolves to an array of logs for this task.
   * @rejects if we are unable to collect stats for some reason.
   */
  async stats() {
    let logs = [];
    let placesDbPath = OS.Path.join(OS.Constants.Path.profileDir, "places.sqlite");
    let info = await OS.File.stat(placesDbPath);
    logs.push(`Places.sqlite size is ${parseInt(info.size / 1024)}KiB`);
    let faviconsDbPath = OS.Path.join(OS.Constants.Path.profileDir, "favicons.sqlite");
    info = await OS.File.stat(faviconsDbPath);
    logs.push(`Favicons.sqlite size is ${parseInt(info.size / 1024)}KiB`);

    // Execute each step async.
    let pragmas = [ "user_version",
                    "page_size",
                    "cache_size",
                    "journal_mode",
                    "synchronous"
                  ].map(p => `pragma_${p}`);
    let pragmaQuery = `SELECT * FROM ${ pragmas.join(", ") }`;
    await PlacesUtils.withConnectionWrapper("PlacesDBUtils: pragma for stats", async db => {
      let row = (await db.execute(pragmaQuery))[0];
      for (let i = 0; i != pragmas.length; i++) {
        logs.push(`${ pragmas[i] } is ${ row.getResultByIndex(i) }`);
      }
    }).catch(() => {
      logs.push("Could not set pragma for stat collection");
    });

    // Get maximum number of unique URIs.
    try {
      let limitURIs = Services.prefs.getIntPref(
        "places.history.expiration.transient_current_max_pages");
      logs.push("History can store a maximum of " + limitURIs + " unique pages");
    } catch (ex) {}

    let query = "SELECT name FROM sqlite_master WHERE type = :type";
    let params = {};
    let _getTableCount = async (tableName) => {
      let db = await PlacesUtils.promiseDBConnection();
      let rows = await db.execute(`SELECT count(*) FROM ${tableName}`);
      logs.push(`Table ${tableName} has ${rows[0].getResultByIndex(0)} records`);
    };

    try {
      params.type = "table";
      let db = await PlacesUtils.promiseDBConnection();
      await db.execute(query, params,
                       r => _getTableCount(r.getResultByIndex(0)));

      params.type = "index";
      await db.execute(query, params, r => {
        logs.push(`Index ${r.getResultByIndex(0)}`);
      });

      params.type = "trigger";
      await db.execute(query, params, r => {
        logs.push(`Trigger ${r.getResultByIndex(0)}`);
      });

    } catch (ex) {
      throw new Error("Unable to collect stats.");
    }

    return logs;
  },

  /**
   * Collects telemetry data and reports it to Telemetry.
   *
   * Note: although this function isn't actually async, we keep it async to
   * allow us to maintain a simple, consistent API for the tasks within this object.
   *
   */
  async telemetry() {
    // First deal with some scalars for feeds:
    await this._telemetryForFeeds();

    // This will be populated with one integer property for each probe result,
    // using the histogram name as key.
    let probeValues = {};

    // The following array contains an ordered list of entries that are
    // processed to collect telemetry data.  Each entry has these properties:
    //
    //  histogram: Name of the telemetry histogram to update.
    //  query:     This is optional.  If present, contains a database command
    //             that will be executed asynchronously, and whose result will
    //             be added to the telemetry histogram.
    //  callback:  This is optional.  If present, contains a function that must
    //             return the value that will be added to the telemetry
    //             histogram. If a query is also present, its result is passed
    //             as the first argument of the function.  If the function
    //             raises an exception, no data is added to the histogram.
    //
    // Since all queries are executed in order by the database backend, the
    // callbacks can also use the result of previous queries stored in the
    // probeValues object.
    let probes = [
      { histogram: "PLACES_PAGES_COUNT",
        query:     "SELECT count(*) FROM moz_places" },

      { histogram: "PLACES_BOOKMARKS_COUNT",
        query:     `SELECT count(*) FROM moz_bookmarks b
                    JOIN moz_bookmarks t ON t.id = b.parent
                    AND t.parent <> :tags_folder
                    WHERE b.type = :type_bookmark`,
        params: {
          tags_folder: PlacesUtils.tagsFolderId,
          type_bookmark: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        }
      },

      { histogram: "PLACES_TAGS_COUNT",
        query:     `SELECT count(*) FROM moz_bookmarks
                    WHERE parent = :tags_folder`,
        params: {
          tags_folder: PlacesUtils.tagsFolderId,
        }
      },

      { histogram: "PLACES_KEYWORDS_COUNT",
        query:     "SELECT count(*) FROM moz_keywords" },

      { histogram: "PLACES_SORTED_BOOKMARKS_PERC",
        query:     `SELECT IFNULL(ROUND((
                      SELECT count(*) FROM moz_bookmarks b
                      JOIN moz_bookmarks t ON t.id = b.parent
                      AND t.parent <> :tags_folder AND t.parent > :places_root
                      WHERE b.type  = :type_bookmark
                      ) * 100 / (
                      SELECT count(*) FROM moz_bookmarks b
                      JOIN moz_bookmarks t ON t.id = b.parent
                      AND t.parent <> :tags_folder
                      WHERE b.type = :type_bookmark
                    )), 0)`,
        params: {
          places_root: PlacesUtils.placesRootId,
          tags_folder: PlacesUtils.tagsFolderId,
          type_bookmark: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        }
      },

      { histogram: "PLACES_TAGGED_BOOKMARKS_PERC",
        query:     `SELECT IFNULL(ROUND((
                      SELECT count(*) FROM moz_bookmarks b
                      JOIN moz_bookmarks t ON t.id = b.parent
                      AND t.parent = :tags_folder
                      ) * 100 / (
                      SELECT count(*) FROM moz_bookmarks b
                      JOIN moz_bookmarks t ON t.id = b.parent
                      AND t.parent <> :tags_folder
                      WHERE b.type = :type_bookmark
                    )), 0)`,
        params: {
          tags_folder: PlacesUtils.tagsFolderId,
          type_bookmark: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        }
      },

      { histogram: "PLACES_DATABASE_FILESIZE_MB",
        async callback() {
          let placesDbPath = OS.Path.join(OS.Constants.Path.profileDir, "places.sqlite");
          let info = await OS.File.stat(placesDbPath);
          return parseInt(info.size / BYTES_PER_MEBIBYTE);
        }
      },

      { histogram: "PLACES_DATABASE_PAGESIZE_B",
        query:     "PRAGMA page_size /* PlacesDBUtils.jsm PAGESIZE_B */" },

      { histogram: "PLACES_DATABASE_SIZE_PER_PAGE_B",
        query:     "PRAGMA page_count",
        callback(aDbPageCount) {
          // Note that the database file size would not be meaningful for this
          // calculation, because the file grows in fixed-size chunks.
          let dbPageSize = probeValues.PLACES_DATABASE_PAGESIZE_B;
          let placesPageCount = probeValues.PLACES_PAGES_COUNT;
          return Math.round((dbPageSize * aDbPageCount) / placesPageCount);
        }
      },

      { histogram: "PLACES_DATABASE_FAVICONS_FILESIZE_MB",
        async callback() {
          let faviconsDbPath = OS.Path.join(OS.Constants.Path.profileDir, "favicons.sqlite");
          let info = await OS.File.stat(faviconsDbPath);
          return parseInt(info.size / BYTES_PER_MEBIBYTE);
        }
      },

      { histogram: "PLACES_ANNOS_BOOKMARKS_COUNT",
        query:     "SELECT count(*) FROM moz_items_annos" },

      { histogram: "PLACES_ANNOS_PAGES_COUNT",
        query:     "SELECT count(*) FROM moz_annos" },

      { histogram: "PLACES_MAINTENANCE_DAYSFROMLAST",
        callback() {
          try {
            let lastMaintenance = Services.prefs.getIntPref("places.database.lastMaintenance");
            let nowSeconds = parseInt(Date.now() / 1000);
            return parseInt((nowSeconds - lastMaintenance) / 86400);
          } catch (ex) {
            return 60;
          }
        }
      },
    ];

    for (let probe of probes) {
      let val;
      if (("query" in probe)) {
        let db = await PlacesUtils.promiseDBConnection();
        val = (await db.execute(probe.query, probe.params || {}))[0].getResultByIndex(0);
      }
      // Report the result of the probe through Telemetry.
      // The resulting promise cannot reject.
      if ("callback" in probe) {
        val = await probe.callback(val);
      }
      probeValues[probe.histogram] = val;
      Services.telemetry.getHistogramById(probe.histogram).add(val);
    }
  },

  async _telemetryForFeeds() {
    let db = await PlacesUtils.promiseDBConnection();
    let livebookmarkCount = await db.execute(
      `SELECT count(*) FROM moz_items_annos a
                       JOIN moz_anno_attributes aa ON a.anno_attribute_id = aa.id
                       WHERE aa.name = 'livemark/feedURI'`);
    livebookmarkCount = livebookmarkCount[0].getResultByIndex(0);
    Services.telemetry.scalarSet("browser.feeds.livebookmark_count", livebookmarkCount);
  },

  /**
   * Runs a list of tasks, returning a Map when done.
   *
   * @param tasks
   *        Array of tasks to be executed, in form of pointers to methods in
   *        this module.
   * @return {Promise}
   *        A promise that resolves with a Map[taskName(String) -> Object].
   *        The Object has the following properties:
   *         - succeeded: boolean
   *         - logs: an array of strings containing the messages logged by the task
   */
  async runTasks(tasks) {
    PlacesDBUtils._clearTaskQueue = false;
    let tasksMap = new Map();
    for (let task of tasks) {
      if (PlacesDBUtils._isShuttingDown) {
        tasksMap.set(
          task.name,
          { succeeded: false, logs: ["Shutting down, will not schedule the task."] });
        continue;
      }

      if (PlacesDBUtils._clearTaskQueue) {
        tasksMap.set(
          task.name,
          { succeeded: false, logs: ["The task queue was cleared by an error in another task."] });
        continue;
      }

      let result =
          await task().then(logs => { return { succeeded: true, logs }; })
                      .catch(err => { return { succeeded: false, logs: [err.message] }; });
      tasksMap.set(task.name, result);
    }
    return tasksMap;
  }
};
