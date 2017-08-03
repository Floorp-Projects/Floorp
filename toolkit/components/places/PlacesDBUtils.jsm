/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

this.EXPORTED_SYMBOLS = [ "PlacesDBUtils" ];

// Constants

const BYTES_PER_MEBIBYTE = 1048576;

this.PlacesDBUtils = {

  /**
   * Converts the `Map` returned by `runTasks` to an array of messages (legacy).
   * @param taskStatusMap
   *        The Map[String -> Object] returned by `runTasks`.
   * @return an array of log messages.
   */
  getLegacyLog(taskStatusMap) {
    let logs = [];
    for (let [key, value] of taskStatusMap) {
      logs.push(`> Task: ${key}`);
      let prefix = value.succeeded ? "+ " : "- ";
      logs = logs.concat(value.logs.map(m => `${prefix}${m}`));
    }
    return logs;
  },

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
      this.checkCoherence,
      this._refreshUI
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
  async checkAndFixDatabase() { // eslint-disable-line require-await
    let tasks = [
      this.checkIntegrity,
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
  async _refreshUI() { // eslint-disable-line require-await
    // Send batch update notifications to update the UI.
    let observers = PlacesUtils.history.getObservers();
    for (let observer of observers) {
      observer.onBeginUpdateBatch();
      observer.onEndUpdateBatch();
    }
    return [];
  },

  /**
   * Tries to execute a REINDEX on the database.
   *
   * @return {Promise} resolved when reindex is complete.
   * @resolves to an array of logs for this task.
   * @rejects if we're unable to reindex the database.
   */
  async reindex() {
    try {
      let logs = [];
      await PlacesUtils.withConnectionWrapper(
        "PlacesDBUtils: Reindex the database",
        async (db) => {
          let query = "REINDEX";
          await db.execute(query);
          logs.push("The database has been re indexed");
        });
      return logs;
    } catch (ex) {
      throw new Error("Unable to reindex the database.");
    }
  },

  /**
   * Checks integrity but does not try to fix the database through a reindex.
   *
   * Note: although this function isn't actually async, we keep it async to
   * allow us to maintain a simple, consistent API for the tasks within this object.
   *
   * @return {Promise} resolves if database is sane.
   * @resolves to an array of logs for this task.
   * @rejects if we're unable to fix corruption or unable to check status.
   */
  async _checkIntegritySkipReindex() { // eslint-disable-line require-await
    return this.checkIntegrity(true);
  },

  /**
   * Checks integrity and tries to fix the database through a reindex.
   *
   * @param [optional] skipReindex
   *        Whether to try to reindex database or not.
   *
   * @return {Promise} resolves if database is sane or is made sane.
   * @resolves to an array of logs for this task.
   * @rejects if we're unable to fix corruption or unable to check status.
   */
  async checkIntegrity(skipReindex) {
    let logs = [];

    try {
      // Run a integrity check, but stop at the first error.
      await PlacesUtils.withConnectionWrapper("PlacesDBUtils: check the integrity", async (db) => {
        let row;
        await db.execute(
          "PRAGMA integrity_check",
          null,
          r => {
            row = r;
            throw StopIteration;
          });
        if (row.getResultByIndex(0) === "ok") {
          logs.push("The database is sane");
        } else {
          // We stopped due to an integrity corruption, try to fix if possible.
          logs.push("The database is corrupt");
          if (skipReindex) {
            Services.prefs.setBoolPref("places.database.replaceOnStartup", true);
            PlacesDBUtils.clearPendingTasks();
            throw new Error("Unable to fix corruption, database will be replaced on next startup");
          } else {
            // Try to reindex, this often fixes simple indices corruption.
            let reindexLogs = await PlacesDBUtils.reindex();
            let checkLogs = await PlacesDBUtils._checkIntegritySkipReindex();
            logs = logs.concat(reindexLogs).concat(checkLogs);
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

  /**
   * Checks data coherence and tries to fix most common errors.
   *
   * @return {Promise} resolves when coherence is checked.
   * @resolves to an array of logs for this task.
   * @rejects if database is not coherent.
   */
  async checkCoherence() {
    let logs = [];

    let stmts = await PlacesDBUtils._getBoundCoherenceStatements();
    let coherenceCheck = true;
    await PlacesUtils.withConnectionWrapper(
      "PlacesDBUtils: coherence check:",
      db => db.executeTransaction(async () => {
        for (let {query, params} of stmts) {
          params = params ? params : null;
          try {
            await db.execute(query, params);
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
      throw new Error("Unable to check database coherence");
    }
    return logs;
  },

  async _getBoundCoherenceStatements() {
    let cleanupStatements = [];

    // MOZ_ANNO_ATTRIBUTES
    // A.1 remove obsolete annotations from moz_annos.
    // The 'weave0' idiom exploits character ordering (0 follows /) to
    // efficiently select all annos with a 'weave/' prefix.
    let deleteObsoleteAnnos = {
      query:
      `DELETE FROM moz_annos
       WHERE type = 4
          OR anno_attribute_id IN (
         SELECT id FROM moz_anno_attributes
         WHERE name = 'downloads/destinationFileName' OR
               name BETWEEN 'weave/' AND 'weave0'
       )`
    };
    cleanupStatements.push(deleteObsoleteAnnos);

    // A.2 remove obsolete annotations from moz_items_annos.
    let deleteObsoleteItemsAnnos = {
      query:
      `DELETE FROM moz_items_annos
       WHERE type = 4
          OR anno_attribute_id IN (
         SELECT id FROM moz_anno_attributes
         WHERE name = 'sync/children'
            OR name = 'placesInternal/GUID'
            OR name BETWEEN 'weave/' AND 'weave0'
       )`
    };
    cleanupStatements.push(deleteObsoleteItemsAnnos);

    // A.3 remove unused attributes.
    let deleteUnusedAnnoAttributes = {
      query:
      `DELETE FROM moz_anno_attributes WHERE id IN (
         SELECT id FROM moz_anno_attributes n
         WHERE NOT EXISTS
             (SELECT id FROM moz_annos WHERE anno_attribute_id = n.id LIMIT 1)
           AND NOT EXISTS
             (SELECT id FROM moz_items_annos WHERE anno_attribute_id = n.id LIMIT 1)
       )`
    };
    cleanupStatements.push(deleteUnusedAnnoAttributes);

    // MOZ_ANNOS
    // B.1 remove annos with an invalid attribute
    let deleteInvalidAttributeAnnos = {
      query:
      `DELETE FROM moz_annos WHERE id IN (
         SELECT id FROM moz_annos a
         WHERE NOT EXISTS
           (SELECT id FROM moz_anno_attributes
             WHERE id = a.anno_attribute_id LIMIT 1)
       )`
    };
    cleanupStatements.push(deleteInvalidAttributeAnnos);

    // B.2 remove orphan annos
    let deleteOrphanAnnos = {
      query:
      `DELETE FROM moz_annos WHERE id IN (
         SELECT id FROM moz_annos a
         WHERE NOT EXISTS
           (SELECT id FROM moz_places WHERE id = a.place_id LIMIT 1)
       )`
    };
    cleanupStatements.push(deleteOrphanAnnos);

    // Bookmarks roots
    // C.1 fix missing Places root
    //     Bug 477739 shows a case where the root could be wrongly removed
    //     due to an endianness issue.  We try to fix broken roots here.
    let selectPlacesRoot = {
      query: "SELECT id FROM moz_bookmarks WHERE id = :places_root",
      params: {}
    };
    selectPlacesRoot.params.places_root = PlacesUtils.placesRootId;
    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.execute(selectPlacesRoot.query, selectPlacesRoot.params);
    if (rows.length === 0) {
      // We are missing the root, try to recreate it.
      let createPlacesRoot = {
        query:
        `INSERT INTO moz_bookmarks (id, type, fk, parent, position, title, guid)
           VALUES (:places_root, 2, NULL, 0, 0, :title, :guid)`,
        params: {}
      };
      createPlacesRoot.params.places_root = PlacesUtils.placesRootId;
      createPlacesRoot.params.title = "";
      createPlacesRoot.params.guid = PlacesUtils.bookmarks.rootGuid;
      cleanupStatements.push(createPlacesRoot);

      // Now ensure that other roots are children of Places root.
      let fixPlacesRootChildren = {
        query:
        `UPDATE moz_bookmarks SET parent = :places_root WHERE guid IN
             ( :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid )`,
        params: {}
      };
      fixPlacesRootChildren.params.places_root = PlacesUtils.placesRootId;
      fixPlacesRootChildren.params.menuGuid = PlacesUtils.bookmarks.menuGuid;
      fixPlacesRootChildren.params.toolbarGuid = PlacesUtils.bookmarks.toolbarGuid;
      fixPlacesRootChildren.params.unfiledGuid = PlacesUtils.bookmarks.unfiledGuid;
      fixPlacesRootChildren.params.tagsGuid = PlacesUtils.bookmarks.tagsGuid;
      cleanupStatements.push(fixPlacesRootChildren);
    }
    // C.2 fix roots titles
    //     some alpha version has wrong roots title, and this also fixes them if
    //     locale has changed.
    let updateRootTitleSql = `UPDATE moz_bookmarks SET title = :title
                              WHERE id = :root_id AND title <> :title`;
    // root
    let fixPlacesRootTitle = {
      query: updateRootTitleSql,
      params: {}
    };
    fixPlacesRootTitle.params.root_id = PlacesUtils.placesRootId;
    fixPlacesRootTitle.params.title = "";
    cleanupStatements.push(fixPlacesRootTitle);
    // bookmarks menu
    let fixBookmarksMenuTitle = {
      query: updateRootTitleSql,
      params: {}
    };
    fixBookmarksMenuTitle.params.root_id = PlacesUtils.bookmarksMenuFolderId;
    fixBookmarksMenuTitle.params.title =
      PlacesUtils.getString("BookmarksMenuFolderTitle");
    cleanupStatements.push(fixBookmarksMenuTitle);
    // bookmarks toolbar
    let fixBookmarksToolbarTitle = {
      query: updateRootTitleSql,
      params: {}
    };
    fixBookmarksToolbarTitle.params.root_id = PlacesUtils.toolbarFolderId;
    fixBookmarksToolbarTitle.params.title =
      PlacesUtils.getString("BookmarksToolbarFolderTitle");
    cleanupStatements.push(fixBookmarksToolbarTitle);
    // unsorted bookmarks
    let fixUnsortedBookmarksTitle = {
      query: updateRootTitleSql,
      params: {}
    };
    fixUnsortedBookmarksTitle.params.root_id = PlacesUtils.unfiledBookmarksFolderId;
    fixUnsortedBookmarksTitle.params.title =
      PlacesUtils.getString("OtherBookmarksFolderTitle");
    cleanupStatements.push(fixUnsortedBookmarksTitle);
    // tags
    let fixTagsRootTitle = {
      query: updateRootTitleSql,
      params: {}
    };
    fixTagsRootTitle.params.root_id = PlacesUtils.tagsFolderId;
    fixTagsRootTitle.params.title =
      PlacesUtils.getString("TagsFolderTitle");
    cleanupStatements.push(fixTagsRootTitle);

    // MOZ_BOOKMARKS
    // D.1 remove items without a valid place
    // if fk IS NULL we fix them in D.7
    let deleteNoPlaceItems = {
      query:
      `DELETE FROM moz_bookmarks WHERE guid NOT IN (
         :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
       ) AND id IN (
         SELECT b.id FROM moz_bookmarks b
         WHERE fk NOT NULL AND b.type = :bookmark_type
           AND NOT EXISTS (SELECT url FROM moz_places WHERE id = b.fk LIMIT 1)
       )`,
      params: {}
    };
    deleteNoPlaceItems.params.bookmark_type = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    deleteNoPlaceItems.params.rootGuid = PlacesUtils.bookmarks.rootGuid;
    deleteNoPlaceItems.params.menuGuid = PlacesUtils.bookmarks.menuGuid;
    deleteNoPlaceItems.params.toolbarGuid = PlacesUtils.bookmarks.toolbarGuid;
    deleteNoPlaceItems.params.unfiledGuid = PlacesUtils.bookmarks.unfiledGuid;
    deleteNoPlaceItems.params.tagsGuid = PlacesUtils.bookmarks.tagsGuid;
    cleanupStatements.push(deleteNoPlaceItems);

    // D.2 remove items that are not uri bookmarks from tag containers
    let deleteBogusTagChildren = {
      query:
      `DELETE FROM moz_bookmarks WHERE guid NOT IN (
         :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
       ) AND id IN (
         SELECT b.id FROM moz_bookmarks b
         WHERE b.parent IN
           (SELECT id FROM moz_bookmarks WHERE parent = :tags_folder)
           AND b.type <> :bookmark_type
       )`,
      params: {}
    };
    deleteBogusTagChildren.params.tags_folder = PlacesUtils.tagsFolderId;
    deleteBogusTagChildren.params.bookmark_type = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    deleteBogusTagChildren.params.rootGuid = PlacesUtils.bookmarks.rootGuid;
    deleteBogusTagChildren.params.menuGuid = PlacesUtils.bookmarks.menuGuid;
    deleteBogusTagChildren.params.toolbarGuid = PlacesUtils.bookmarks.toolbarGuid;
    deleteBogusTagChildren.params.unfiledGuid = PlacesUtils.bookmarks.unfiledGuid;
    deleteBogusTagChildren.params.tagsGuid = PlacesUtils.bookmarks.tagsGuid;
    cleanupStatements.push(deleteBogusTagChildren);

    // D.3 remove empty tags
    let deleteEmptyTags = {
      query:
      `DELETE FROM moz_bookmarks WHERE guid NOT IN (
         :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
       ) AND id IN (
         SELECT b.id FROM moz_bookmarks b
         WHERE b.id IN
           (SELECT id FROM moz_bookmarks WHERE parent = :tags_folder)
           AND NOT EXISTS
             (SELECT id from moz_bookmarks WHERE parent = b.id LIMIT 1)
       )`,
      params: {}
    };
    deleteEmptyTags.params.tags_folder = PlacesUtils.tagsFolderId;
    deleteEmptyTags.params.rootGuid = PlacesUtils.bookmarks.rootGuid;
    deleteEmptyTags.params.menuGuid = PlacesUtils.bookmarks.menuGuid;
    deleteEmptyTags.params.toolbarGuid = PlacesUtils.bookmarks.toolbarGuid;
    deleteEmptyTags.params.unfiledGuid = PlacesUtils.bookmarks.unfiledGuid;
    deleteEmptyTags.params.tagsGuid = PlacesUtils.bookmarks.tagsGuid;
    cleanupStatements.push(deleteEmptyTags);

    // D.4 move orphan items to unsorted folder
    let fixOrphanItems = {
      query:
      `UPDATE moz_bookmarks SET parent = :unsorted_folder WHERE guid NOT IN (
         :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
       ) AND id IN (
         SELECT b.id FROM moz_bookmarks b
         WHERE NOT EXISTS
           (SELECT id FROM moz_bookmarks WHERE id = b.parent LIMIT 1)
       )`,
      params: {}
    };
    fixOrphanItems.params.unsorted_folder = PlacesUtils.unfiledBookmarksFolderId;
    fixOrphanItems.params.rootGuid = PlacesUtils.bookmarks.rootGuid;
    fixOrphanItems.params.menuGuid = PlacesUtils.bookmarks.menuGuid;
    fixOrphanItems.params.toolbarGuid = PlacesUtils.bookmarks.toolbarGuid;
    fixOrphanItems.params.unfiledGuid = PlacesUtils.bookmarks.unfiledGuid;
    fixOrphanItems.params.tagsGuid = PlacesUtils.bookmarks.tagsGuid;
    cleanupStatements.push(fixOrphanItems);

    // D.6 fix wrong item types
    //     Folders and separators should not have an fk.
    //     If they have a valid fk convert them to bookmarks. Later in D.9 we
    //     will move eventual children to unsorted bookmarks.
    let fixBookmarksAsFolders = {
      query:
      `UPDATE moz_bookmarks SET type = :bookmark_type WHERE guid NOT IN (
         :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
       ) AND id IN (
         SELECT id FROM moz_bookmarks b
         WHERE type IN (:folder_type, :separator_type)
           AND fk NOTNULL
       )`,
      params: {}
    };
    fixBookmarksAsFolders.params.bookmark_type = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    fixBookmarksAsFolders.params.folder_type = PlacesUtils.bookmarks.TYPE_FOLDER;
    fixBookmarksAsFolders.params.separator_type = PlacesUtils.bookmarks.TYPE_SEPARATOR;
    fixBookmarksAsFolders.params.rootGuid = PlacesUtils.bookmarks.rootGuid;
    fixBookmarksAsFolders.params.menuGuid = PlacesUtils.bookmarks.menuGuid;
    fixBookmarksAsFolders.params.toolbarGuid = PlacesUtils.bookmarks.toolbarGuid;
    fixBookmarksAsFolders.params.unfiledGuid = PlacesUtils.bookmarks.unfiledGuid;
    fixBookmarksAsFolders.params.tagsGuid = PlacesUtils.bookmarks.tagsGuid;
    cleanupStatements.push(fixBookmarksAsFolders);

    // D.7 fix wrong item types
    //     Bookmarks should have an fk, if they don't have any, convert them to
    //     folders.
    let fixFoldersAsBookmarks = {
      query:
      `UPDATE moz_bookmarks SET type = :folder_type WHERE guid NOT IN (
         :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
       ) AND id IN (
         SELECT id FROM moz_bookmarks b
         WHERE type = :bookmark_type
           AND fk IS NULL
       )`,
      params: {}
    };
    fixFoldersAsBookmarks.params.bookmark_type = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    fixFoldersAsBookmarks.params.folder_type = PlacesUtils.bookmarks.TYPE_FOLDER;
    fixFoldersAsBookmarks.params.rootGuid = PlacesUtils.bookmarks.rootGuid;
    fixFoldersAsBookmarks.params.menuGuid = PlacesUtils.bookmarks.menuGuid;
    fixFoldersAsBookmarks.params.toolbarGuid = PlacesUtils.bookmarks.toolbarGuid;
    fixFoldersAsBookmarks.params.unfiledGuid = PlacesUtils.bookmarks.unfiledGuid;
    fixFoldersAsBookmarks.params.tagsGuid = PlacesUtils.bookmarks.tagsGuid;
    cleanupStatements.push(fixFoldersAsBookmarks);

    // D.9 fix wrong parents
    //     Items cannot have separators or other bookmarks
    //     as parent, if they have bad parent move them to unsorted bookmarks.
    let fixInvalidParents = {
      query:
      `UPDATE moz_bookmarks SET parent = :unsorted_folder WHERE guid NOT IN (
         :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
       ) AND id IN (
         SELECT id FROM moz_bookmarks b
         WHERE EXISTS
           (SELECT id FROM moz_bookmarks WHERE id = b.parent
             AND type IN (:bookmark_type, :separator_type)
             LIMIT 1)
       )`,
      params: {}
    };
    fixInvalidParents.params.unsorted_folder = PlacesUtils.unfiledBookmarksFolderId;
    fixInvalidParents.params.bookmark_type = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    fixInvalidParents.params.separator_type = PlacesUtils.bookmarks.TYPE_SEPARATOR;
    fixInvalidParents.params.rootGuid = PlacesUtils.bookmarks.rootGuid;
    fixInvalidParents.params.menuGuid = PlacesUtils.bookmarks.menuGuid;
    fixInvalidParents.params.toolbarGuid = PlacesUtils.bookmarks.toolbarGuid;
    fixInvalidParents.params.unfiledGuid = PlacesUtils.bookmarks.unfiledGuid;
    fixInvalidParents.params.tagsGuid = PlacesUtils.bookmarks.tagsGuid;
    cleanupStatements.push(fixInvalidParents);

    // D.10 recalculate positions
    //      This requires multiple related statements.
    //      We can detect a folder with bad position values comparing the sum of
    //      all distinct position values (+1 since position is 0-based) with the
    //      triangular numbers obtained by the number of children (n).
    //      SUM(DISTINCT position + 1) == (n * (n + 1) / 2).
    cleanupStatements.push({
      query:
      `CREATE TEMP TABLE IF NOT EXISTS moz_bm_reindex_temp (
         id INTEGER PRIMARY_KEY
       , parent INTEGER
       , position INTEGER
       )`
    });
    cleanupStatements.push({
      query:
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
    });
    cleanupStatements.push({
      query:
      `CREATE INDEX IF NOT EXISTS moz_bm_reindex_temp_index
       ON moz_bm_reindex_temp(parent)`
    });
    cleanupStatements.push({
      query:
      `UPDATE moz_bm_reindex_temp SET position = (
         ROWID - (SELECT MIN(t.ROWID) FROM moz_bm_reindex_temp t
                  WHERE t.parent = moz_bm_reindex_temp.parent)
       )`
    });
    cleanupStatements.push({
      query:
      `CREATE TEMP TRIGGER IF NOT EXISTS moz_bm_reindex_temp_trigger
       BEFORE DELETE ON moz_bm_reindex_temp
       FOR EACH ROW
       BEGIN
         UPDATE moz_bookmarks SET position = OLD.position WHERE id = OLD.id;
       END`
    });
    cleanupStatements.push({
      query: "DELETE FROM moz_bm_reindex_temp "
    });
    cleanupStatements.push({
      query: "DROP INDEX moz_bm_reindex_temp_index "
    });
    cleanupStatements.push({
      query: "DROP TRIGGER moz_bm_reindex_temp_trigger "
    });
    cleanupStatements.push({
      query: "DROP TABLE moz_bm_reindex_temp "
    });

    // D.12 Fix empty-named tags.
    //      Tags were allowed to have empty names due to a UI bug.  Fix them
    //      replacing their title with "(notitle)".
    let fixEmptyNamedTags = {
      query:
      `UPDATE moz_bookmarks SET title = :empty_title
       WHERE length(title) = 0 AND type = :folder_type
         AND parent = :tags_folder`,
      params: {}
    };
    fixEmptyNamedTags.params.empty_title = "(notitle)";
    fixEmptyNamedTags.params.folder_type = PlacesUtils.bookmarks.TYPE_FOLDER;
    fixEmptyNamedTags.params.tags_folder = PlacesUtils.tagsFolderId;
    cleanupStatements.push(fixEmptyNamedTags);

    // MOZ_ICONS
    // E.1 remove orphan icon entries.
    let deleteOrphanIconPages = {
      query:
      `DELETE FROM moz_pages_w_icons WHERE page_url_hash NOT IN (
         SELECT url_hash FROM moz_places
       )`
    };
    cleanupStatements.push(deleteOrphanIconPages);

    let deleteOrphanIcons = {
      query:
      `DELETE FROM moz_icons WHERE root = 0 AND id NOT IN (
         SELECT icon_id FROM moz_icons_to_pages
       )`
    };
    cleanupStatements.push(deleteOrphanIcons);

    // MOZ_HISTORYVISITS
    // F.1 remove orphan visits
    let deleteOrphanVisits = {
      query:
      `DELETE FROM moz_historyvisits WHERE id IN (
         SELECT id FROM moz_historyvisits v
         WHERE NOT EXISTS
           (SELECT id FROM moz_places WHERE id = v.place_id LIMIT 1)
       )`
    };
    cleanupStatements.push(deleteOrphanVisits);

    // MOZ_INPUTHISTORY
    // G.1 remove orphan input history
    let deleteOrphanInputHistory = {
      query:
      `DELETE FROM moz_inputhistory WHERE place_id IN (
         SELECT place_id FROM moz_inputhistory i
         WHERE NOT EXISTS
           (SELECT id FROM moz_places WHERE id = i.place_id LIMIT 1)
       )`
    };
    cleanupStatements.push(deleteOrphanInputHistory);

    // MOZ_ITEMS_ANNOS
    // H.1 remove item annos with an invalid attribute
    let deleteInvalidAttributeItemsAnnos = {
      query:
      `DELETE FROM moz_items_annos WHERE id IN (
         SELECT id FROM moz_items_annos t
         WHERE NOT EXISTS
           (SELECT id FROM moz_anno_attributes
             WHERE id = t.anno_attribute_id LIMIT 1)
       )`
    };
    cleanupStatements.push(deleteInvalidAttributeItemsAnnos);

    // H.2 remove orphan item annos
    let deleteOrphanItemsAnnos = {
      query:
      `DELETE FROM moz_items_annos WHERE id IN (
         SELECT id FROM moz_items_annos t
         WHERE NOT EXISTS
           (SELECT id FROM moz_bookmarks WHERE id = t.item_id LIMIT 1)
       )`
    };
    cleanupStatements.push(deleteOrphanItemsAnnos);

    // MOZ_KEYWORDS
    // I.1 remove unused keywords
    let deleteUnusedKeywords = {
      query:
      `DELETE FROM moz_keywords WHERE id IN (
         SELECT id FROM moz_keywords k
         WHERE NOT EXISTS
           (SELECT 1 FROM moz_places h WHERE k.place_id = h.id)
       )`
    };
    cleanupStatements.push(deleteUnusedKeywords);

    // MOZ_PLACES
    // L.2 recalculate visit_count and last_visit_date
    let fixVisitStats = {
      query:
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
    };
    cleanupStatements.push(fixVisitStats);

    // L.3 recalculate hidden for redirects.
    let fixRedirectsHidden = {
      query:
      `UPDATE moz_places
       SET hidden = 1
       WHERE id IN (
         SELECT h.id FROM moz_places h
         JOIN moz_historyvisits src ON src.place_id = h.id
         JOIN moz_historyvisits dst ON dst.from_visit = src.id AND dst.visit_type IN (5,6)
         LEFT JOIN moz_bookmarks on fk = h.id AND fk ISNULL
         GROUP BY src.place_id HAVING count(*) = visit_count
       )`
    };
    cleanupStatements.push(fixRedirectsHidden);

    // L.4 recalculate foreign_count.
    let fixForeignCount = {
      query:
      `UPDATE moz_places SET foreign_count =
         (SELECT count(*) FROM moz_bookmarks WHERE fk = moz_places.id ) +
         (SELECT count(*) FROM moz_keywords WHERE place_id = moz_places.id )`
    };
    cleanupStatements.push(fixForeignCount);

    // L.5 recalculate missing hashes.
    let fixMissingHashes = {
      query: `UPDATE moz_places SET url_hash = hash(url) WHERE url_hash = 0`
    };
    cleanupStatements.push(fixMissingHashes);

    // MAINTENANCE STATEMENTS SHOULD GO ABOVE THIS POINT!

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
  async vacuum() { // eslint-disable-line require-await
    let logs = [];
    let DBFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
    DBFile.append("places.sqlite");
    logs.push("Initial database size is " +
                parseInt(DBFile.fileSize / 1024) + " KiB");
    return PlacesUtils.withConnectionWrapper(
      "PlacesDBUtils: vacuum",
      async (db) => {
        await db.execute("VACUUM");
      }).then(() => {
        logs.push("The database has been vacuumed");
        let vacuumedDBFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
        vacuumedDBFile.append("places.sqlite");
        logs.push("Final database size is " +
                   parseInt(vacuumedDBFile.fileSize / 1024) + " KiB");
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
  async expire() { // eslint-disable-line require-await
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
    let DBFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
    DBFile.append("places.sqlite");
    logs.push("Database size is " + parseInt(DBFile.fileSize / 1024) + " KiB");

    // Execute each step async.
    let pragmas = [ "user_version",
                    "page_size",
                    "cache_size",
                    "journal_mode",
                    "synchronous"
                  ].map(p => `pragma_${p}`);
    let pragmaQuery = `SELECT * FROM ${ pragmas.join(", ") }`;
    await PlacesUtils.withConnectionWrapper(
      "PlacesDBUtils: pragma for stats",
      async (db) => {
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
  async telemetry() { // eslint-disable-line require-await
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
                    WHERE b.type = :type_bookmark` },

      { histogram: "PLACES_TAGS_COUNT",
        query:     `SELECT count(*) FROM moz_bookmarks
                    WHERE parent = :tags_folder` },

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
                    )), 0)` },

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
                    )), 0)` },

      { histogram: "PLACES_DATABASE_FILESIZE_MB",
        callback() {
          let DBFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
          DBFile.append("places.sqlite");
          return parseInt(DBFile.fileSize / BYTES_PER_MEBIBYTE);
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

    let params = {
      tags_folder: PlacesUtils.tagsFolderId,
      type_folder: PlacesUtils.bookmarks.TYPE_FOLDER,
      type_bookmark: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      places_root: PlacesUtils.placesRootId
    };

    for (let i = 0; i < probes.length; i++) {
      let probe = probes[i];

      let promiseDone = new Promise((resolve, reject) => {
        if (!("query" in probe)) {
          resolve([probe]);
          return;
        }

        let filteredParams = {};
        for (let p in params) {
          if (probe.query.includes(`:${p}`)) {
            filteredParams[p] = params[p];
          }
        }
        PlacesUtils.promiseDBConnection()
          .then(db => db.execute(probe.query, filteredParams))
          .then(rows => resolve([probe, rows[0].getResultByIndex(0)]))
          .catch(ex => reject(new Error("Unable to get telemetry from database.")));
      });
      // Report the result of the probe through Telemetry.
      // The resulting promise cannot reject.
      promiseDone.then(([aProbe, aValue]) => {
        let value = aValue;
        if ("callback" in aProbe) {
          value = aProbe.callback(value);
        }
        probeValues[aProbe.histogram] = value;
        Services.telemetry.getHistogramById(aProbe.histogram).add(value);
      }).catch(Cu.reportError);
    }
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
          { succeeded: false, logs: ["Shutting down, will now schedule the task."] });
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
