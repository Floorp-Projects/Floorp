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
Cu.import("resource://gre/modules/PlacesUtils.jsm");

this.EXPORTED_SYMBOLS = [ "PlacesDBUtils" ];

////////////////////////////////////////////////////////////////////////////////
//// Constants

const FINISHED_MAINTENANCE_TOPIC = "places-maintenance-finished";

const BYTES_PER_MEBIBYTE = 1048576;

////////////////////////////////////////////////////////////////////////////////
//// Smart getters

XPCOMUtils.defineLazyGetter(this, "DBConn", function() {
  return PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
});

////////////////////////////////////////////////////////////////////////////////
//// PlacesDBUtils

this.PlacesDBUtils = {
  /**
   * Executes a list of maintenance tasks.
   * Once finished it will pass a array log to the callback attached to tasks.
   * FINISHED_MAINTENANCE_TOPIC is notified through observer service on finish.
   *
   * @param aTasks
   *        Tasks object to execute.
   */
  _executeTasks: function PDBU__executeTasks(aTasks)
  {
    if (PlacesDBUtils._isShuttingDown) {
      aTasks.log("- We are shutting down. Will not schedule the tasks.");
      aTasks.clear();
    }

    let task = aTasks.pop();
    if (task) {
      task.call(PlacesDBUtils, aTasks);
    }
    else {
      // All tasks have been completed.
      // Telemetry the time it took for maintenance, if a start time exists.
      if (aTasks._telemetryStart) {
        Services.telemetry.getHistogramById("PLACES_IDLE_MAINTENANCE_TIME_MS")
                          .add(Date.now() - aTasks._telemetryStart);
        aTasks._telemetryStart = 0;
      }

      if (aTasks.callback) {
        let scope = aTasks.scope || Cu.getGlobalForObject(aTasks.callback);
        aTasks.callback.call(scope, aTasks.messages);
      }

      // Notify observers that maintenance finished.
      Services.obs.notifyObservers(null, FINISHED_MAINTENANCE_TOPIC, null);
    }
  },

  _isShuttingDown : false,
  shutdown: function PDBU_shutdown() {
    PlacesDBUtils._isShuttingDown = true;
  },

  /**
   * Executes integrity check and common maintenance tasks.
   *
   * @param [optional] aCallback
   *        Callback to be invoked when done.  The callback will get a array
   *        of log messages.
   * @param [optional] aScope
   *        Scope for the callback.
   */
  maintenanceOnIdle: function PDBU_maintenanceOnIdle(aCallback, aScope)
  {
    let tasks = new Tasks([
      this.checkIntegrity
    , this.checkCoherence
    , this._refreshUI
    ]);
    tasks._telemetryStart = Date.now();
    tasks.callback = function() {
      Services.prefs.setIntPref("places.database.lastMaintenance",
                                parseInt(Date.now() / 1000));
      if (aCallback)
        aCallback();
    }
    tasks.scope = aScope;
    this._executeTasks(tasks);
  },

  /**
   * Executes integrity check, common and advanced maintenance tasks (like
   * expiration and vacuum).  Will also collect statistics on the database.
   *
   * @param [optional] aCallback
   *        Callback to be invoked when done.  The callback will get a array
   *        of log messages.
   * @param [optional] aScope
   *        Scope for the callback.
   */
  checkAndFixDatabase: function PDBU_checkAndFixDatabase(aCallback, aScope)
  {
    let tasks = new Tasks([
      this.checkIntegrity
    , this.checkCoherence
    , this.expire
    , this.vacuum
    , this.stats
    , this._refreshUI
    ]);
    tasks.callback = aCallback;
    tasks.scope = aScope;
    this._executeTasks(tasks);
  },

  /**
   * Forces a full refresh of Places views.
   *
   * @param [optional] aTasks
   *        Tasks object to execute.
   */
  _refreshUI: function PDBU__refreshUI(aTasks)
  {
    let tasks = new Tasks(aTasks);

    // Send batch update notifications to update the UI.
    PlacesUtils.history.runInBatchMode({
      runBatched: function (aUserData) {}
    }, null);
    PlacesDBUtils._executeTasks(tasks);
  },

  _handleError: function PDBU__handleError(aError)
  {
    Cu.reportError("Async statement execution returned with '" +
                   aError.result + "', '" + aError.message + "'");
  },

  /**
   * Tries to execute a REINDEX on the database.
   *
   * @param [optional] aTasks
   *        Tasks object to execute.
   */
  reindex: function PDBU_reindex(aTasks)
  {
    let tasks = new Tasks(aTasks);
    tasks.log("> Reindex");

    let stmt = DBConn.createAsyncStatement("REINDEX");
    stmt.executeAsync({
      handleError: PlacesDBUtils._handleError,
      handleResult: function () {},

      handleCompletion: function (aReason)
      {
        if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
          tasks.log("+ The database has been reindexed");
        }
        else {
          tasks.log("- Unable to reindex database");
        }

        PlacesDBUtils._executeTasks(tasks);
      }
    });
    stmt.finalize();
  },

  /**
   * Checks integrity but does not try to fix the database through a reindex.
   *
   * @param [optional] aTasks
   *        Tasks object to execute.
   */
  _checkIntegritySkipReindex: function PDBU__checkIntegritySkipReindex(aTasks) {
    return this.checkIntegrity(aTasks, true);
  },

  /**
   * Checks integrity and tries to fix the database through a reindex.
   *
   * @param [optional] aTasks
   *        Tasks object to execute.
   * @param [optional] aSkipdReindex
   *        Whether to try to reindex database or not.
   */
  checkIntegrity: function PDBU_checkIntegrity(aTasks, aSkipReindex)
  {
    let tasks = new Tasks(aTasks);
    tasks.log("> Integrity check");

    // Run a integrity check, but stop at the first error.
    let stmt = DBConn.createAsyncStatement("PRAGMA integrity_check(1)");
    stmt.executeAsync({
      handleError: PlacesDBUtils._handleError,

      _corrupt: false,
      handleResult: function (aResultSet)
      {
        let row = aResultSet.getNextRow();
        this._corrupt = row.getResultByIndex(0) != "ok";
      },

      handleCompletion: function (aReason)
      {
        if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
          if (this._corrupt) {
            tasks.log("- The database is corrupt");
            if (aSkipReindex) {
              tasks.log("- Unable to fix corruption, database will be replaced on next startup");
              Services.prefs.setBoolPref("places.database.replaceOnStartup", true);
              tasks.clear();
            }
            else {
              // Try to reindex, this often fixed simple indices corruption.
              // We insert from the top of the queue, they will run inverse.
              tasks.push(PlacesDBUtils._checkIntegritySkipReindex);
              tasks.push(PlacesDBUtils.reindex);
            }
          }
          else {
            tasks.log("+ The database is sane");
          }
        }
        else {
          tasks.log("- Unable to check database status");
          tasks.clear();
        }

        PlacesDBUtils._executeTasks(tasks);
      }
    });
    stmt.finalize();
  },

  /**
   * Checks data coherence and tries to fix most common errors.
   *
   * @param [optional] aTasks
   *        Tasks object to execute.
   */
  checkCoherence: function PDBU_checkCoherence(aTasks)
  {
    let tasks = new Tasks(aTasks);
    tasks.log("> Coherence check");

    let stmts = PlacesDBUtils._getBoundCoherenceStatements();
    DBConn.executeAsync(stmts, stmts.length, {
      handleError: PlacesDBUtils._handleError,
      handleResult: function () {},

      handleCompletion: function (aReason)
      {
        if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
          tasks.log("+ The database is coherent");
        }
        else {
          tasks.log("- Unable to check database coherence");
          tasks.clear();
        }

        PlacesDBUtils._executeTasks(tasks);
      }
    });
    stmts.forEach(aStmt => aStmt.finalize());
  },

  _getBoundCoherenceStatements: function PDBU__getBoundCoherenceStatements()
  {
    let cleanupStatements = [];

    // MOZ_ANNO_ATTRIBUTES
    // A.1 remove obsolete annotations from moz_annos.
    // The 'weave0' idiom exploits character ordering (0 follows /) to
    // efficiently select all annos with a 'weave/' prefix.
    let deleteObsoleteAnnos = DBConn.createAsyncStatement(
      `DELETE FROM moz_annos
       WHERE type = 4
          OR anno_attribute_id IN (
         SELECT id FROM moz_anno_attributes
         WHERE name BETWEEN 'weave/' AND 'weave0'
       )`);
    cleanupStatements.push(deleteObsoleteAnnos);

    // A.2 remove obsolete annotations from moz_items_annos.
    let deleteObsoleteItemsAnnos = DBConn.createAsyncStatement(
      `DELETE FROM moz_items_annos
       WHERE type = 4
          OR anno_attribute_id IN (
         SELECT id FROM moz_anno_attributes
         WHERE name = 'sync/children'
            OR name = 'placesInternal/GUID'
            OR name BETWEEN 'weave/' AND 'weave0'
       )`);
    cleanupStatements.push(deleteObsoleteItemsAnnos);

    // A.3 remove unused attributes.
    let deleteUnusedAnnoAttributes = DBConn.createAsyncStatement(
      `DELETE FROM moz_anno_attributes WHERE id IN (
         SELECT id FROM moz_anno_attributes n
         WHERE NOT EXISTS
             (SELECT id FROM moz_annos WHERE anno_attribute_id = n.id LIMIT 1)
           AND NOT EXISTS
             (SELECT id FROM moz_items_annos WHERE anno_attribute_id = n.id LIMIT 1)
       )`);
    cleanupStatements.push(deleteUnusedAnnoAttributes);

    // MOZ_ANNOS
    // B.1 remove annos with an invalid attribute
    let deleteInvalidAttributeAnnos = DBConn.createAsyncStatement(
      `DELETE FROM moz_annos WHERE id IN (
         SELECT id FROM moz_annos a
         WHERE NOT EXISTS
           (SELECT id FROM moz_anno_attributes
             WHERE id = a.anno_attribute_id LIMIT 1)
       )`);
    cleanupStatements.push(deleteInvalidAttributeAnnos);

    // B.2 remove orphan annos
    let deleteOrphanAnnos = DBConn.createAsyncStatement(
      `DELETE FROM moz_annos WHERE id IN (
         SELECT id FROM moz_annos a
         WHERE NOT EXISTS
           (SELECT id FROM moz_places WHERE id = a.place_id LIMIT 1)
       )`);
    cleanupStatements.push(deleteOrphanAnnos);

    // Bookmarks roots
    // C.1 fix missing Places root
    //     Bug 477739 shows a case where the root could be wrongly removed
    //     due to an endianness issue.  We try to fix broken roots here.
    let selectPlacesRoot = DBConn.createStatement(
      "SELECT id FROM moz_bookmarks WHERE id = :places_root");
    selectPlacesRoot.params["places_root"] = PlacesUtils.placesRootId;
    if (!selectPlacesRoot.executeStep()) {
      // We are missing the root, try to recreate it.
      let createPlacesRoot = DBConn.createAsyncStatement(
        `INSERT INTO moz_bookmarks (id, type, fk, parent, position, title,
                                    guid)
         VALUES (:places_root, 2, NULL, 0, 0, :title, :guid)`);
      createPlacesRoot.params["places_root"] = PlacesUtils.placesRootId;
      createPlacesRoot.params["title"] = "";
      createPlacesRoot.params["guid"] = PlacesUtils.bookmarks.rootGuid;
      cleanupStatements.push(createPlacesRoot);

      // Now ensure that other roots are children of Places root.
      let fixPlacesRootChildren = DBConn.createAsyncStatement(
        `UPDATE moz_bookmarks SET parent = :places_root WHERE guid IN
           ( :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid )`);
      fixPlacesRootChildren.params["places_root"] = PlacesUtils.placesRootId;
      fixPlacesRootChildren.params["menuGuid"] = PlacesUtils.bookmarks.menuGuid;
      fixPlacesRootChildren.params["toolbarGuid"] = PlacesUtils.bookmarks.toolbarGuid;
      fixPlacesRootChildren.params["unfiledGuid"] = PlacesUtils.bookmarks.unfiledGuid;
      fixPlacesRootChildren.params["tagsGuid"] = PlacesUtils.bookmarks.tagsGuid;
      cleanupStatements.push(fixPlacesRootChildren);
    }
    selectPlacesRoot.finalize();

    // C.2 fix roots titles
    //     some alpha version has wrong roots title, and this also fixes them if
    //     locale has changed.
    let updateRootTitleSql = `UPDATE moz_bookmarks SET title = :title
                              WHERE id = :root_id AND title <> :title`;
    // root
    let fixPlacesRootTitle = DBConn.createAsyncStatement(updateRootTitleSql);
    fixPlacesRootTitle.params["root_id"] = PlacesUtils.placesRootId;
    fixPlacesRootTitle.params["title"] = "";
    cleanupStatements.push(fixPlacesRootTitle);
    // bookmarks menu
    let fixBookmarksMenuTitle = DBConn.createAsyncStatement(updateRootTitleSql);
    fixBookmarksMenuTitle.params["root_id"] = PlacesUtils.bookmarksMenuFolderId;
    fixBookmarksMenuTitle.params["title"] =
      PlacesUtils.getString("BookmarksMenuFolderTitle");
    cleanupStatements.push(fixBookmarksMenuTitle);
    // bookmarks toolbar
    let fixBookmarksToolbarTitle = DBConn.createAsyncStatement(updateRootTitleSql);
    fixBookmarksToolbarTitle.params["root_id"] = PlacesUtils.toolbarFolderId;
    fixBookmarksToolbarTitle.params["title"] =
      PlacesUtils.getString("BookmarksToolbarFolderTitle");
    cleanupStatements.push(fixBookmarksToolbarTitle);
    // unsorted bookmarks
    let fixUnsortedBookmarksTitle = DBConn.createAsyncStatement(updateRootTitleSql);
    fixUnsortedBookmarksTitle.params["root_id"] = PlacesUtils.unfiledBookmarksFolderId;
    fixUnsortedBookmarksTitle.params["title"] =
      PlacesUtils.getString("OtherBookmarksFolderTitle");
    cleanupStatements.push(fixUnsortedBookmarksTitle);
    // tags
    let fixTagsRootTitle = DBConn.createAsyncStatement(updateRootTitleSql);
    fixTagsRootTitle.params["root_id"] = PlacesUtils.tagsFolderId;
    fixTagsRootTitle.params["title"] =
      PlacesUtils.getString("TagsFolderTitle");
    cleanupStatements.push(fixTagsRootTitle);

    // MOZ_BOOKMARKS
    // D.1 remove items without a valid place
    // if fk IS NULL we fix them in D.7
    let deleteNoPlaceItems = DBConn.createAsyncStatement(
      `DELETE FROM moz_bookmarks WHERE guid NOT IN (
         :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
       ) AND id IN (
         SELECT b.id FROM moz_bookmarks b
         WHERE fk NOT NULL AND b.type = :bookmark_type
           AND NOT EXISTS (SELECT url FROM moz_places WHERE id = b.fk LIMIT 1)
       )`);
    deleteNoPlaceItems.params["bookmark_type"] = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    deleteNoPlaceItems.params["rootGuid"] = PlacesUtils.bookmarks.rootGuid;
    deleteNoPlaceItems.params["menuGuid"] = PlacesUtils.bookmarks.menuGuid;
    deleteNoPlaceItems.params["toolbarGuid"] = PlacesUtils.bookmarks.toolbarGuid;
    deleteNoPlaceItems.params["unfiledGuid"] = PlacesUtils.bookmarks.unfiledGuid;
    deleteNoPlaceItems.params["tagsGuid"] = PlacesUtils.bookmarks.tagsGuid;
    cleanupStatements.push(deleteNoPlaceItems);

    // D.2 remove items that are not uri bookmarks from tag containers
    let deleteBogusTagChildren = DBConn.createAsyncStatement(
      `DELETE FROM moz_bookmarks WHERE guid NOT IN (
         :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
       ) AND id IN (
         SELECT b.id FROM moz_bookmarks b
         WHERE b.parent IN
           (SELECT id FROM moz_bookmarks WHERE parent = :tags_folder)
           AND b.type <> :bookmark_type
       )`);
    deleteBogusTagChildren.params["tags_folder"] = PlacesUtils.tagsFolderId;
    deleteBogusTagChildren.params["bookmark_type"] = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    deleteBogusTagChildren.params["rootGuid"] = PlacesUtils.bookmarks.rootGuid;
    deleteBogusTagChildren.params["menuGuid"] = PlacesUtils.bookmarks.menuGuid;
    deleteBogusTagChildren.params["toolbarGuid"] = PlacesUtils.bookmarks.toolbarGuid;
    deleteBogusTagChildren.params["unfiledGuid"] = PlacesUtils.bookmarks.unfiledGuid;
    deleteBogusTagChildren.params["tagsGuid"] = PlacesUtils.bookmarks.tagsGuid;
    cleanupStatements.push(deleteBogusTagChildren);

    // D.3 remove empty tags
    let deleteEmptyTags = DBConn.createAsyncStatement(
      `DELETE FROM moz_bookmarks WHERE guid NOT IN (
         :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
       ) AND id IN (
         SELECT b.id FROM moz_bookmarks b
         WHERE b.id IN
           (SELECT id FROM moz_bookmarks WHERE parent = :tags_folder)
           AND NOT EXISTS
             (SELECT id from moz_bookmarks WHERE parent = b.id LIMIT 1)
       )`);
    deleteEmptyTags.params["tags_folder"] = PlacesUtils.tagsFolderId;
    deleteEmptyTags.params["rootGuid"] = PlacesUtils.bookmarks.rootGuid;
    deleteEmptyTags.params["menuGuid"] = PlacesUtils.bookmarks.menuGuid;
    deleteEmptyTags.params["toolbarGuid"] = PlacesUtils.bookmarks.toolbarGuid;
    deleteEmptyTags.params["unfiledGuid"] = PlacesUtils.bookmarks.unfiledGuid;
    deleteEmptyTags.params["tagsGuid"] = PlacesUtils.bookmarks.tagsGuid;
    cleanupStatements.push(deleteEmptyTags);

    // D.4 move orphan items to unsorted folder
    let fixOrphanItems = DBConn.createAsyncStatement(
      `UPDATE moz_bookmarks SET parent = :unsorted_folder WHERE guid NOT IN (
         :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
       ) AND id IN (
         SELECT b.id FROM moz_bookmarks b
         WHERE NOT EXISTS
           (SELECT id FROM moz_bookmarks WHERE id = b.parent LIMIT 1)
       )`);
    fixOrphanItems.params["unsorted_folder"] = PlacesUtils.unfiledBookmarksFolderId;
    fixOrphanItems.params["rootGuid"] = PlacesUtils.bookmarks.rootGuid;
    fixOrphanItems.params["menuGuid"] = PlacesUtils.bookmarks.menuGuid;
    fixOrphanItems.params["toolbarGuid"] = PlacesUtils.bookmarks.toolbarGuid;
    fixOrphanItems.params["unfiledGuid"] = PlacesUtils.bookmarks.unfiledGuid;
    fixOrphanItems.params["tagsGuid"] = PlacesUtils.bookmarks.tagsGuid;
    cleanupStatements.push(fixOrphanItems);

    // D.6 fix wrong item types
    //     Folders and separators should not have an fk.
    //     If they have a valid fk convert them to bookmarks. Later in D.9 we
    //     will move eventual children to unsorted bookmarks.
    let fixBookmarksAsFolders = DBConn.createAsyncStatement(
      `UPDATE moz_bookmarks SET type = :bookmark_type WHERE guid NOT IN (
         :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
       ) AND id IN (
         SELECT id FROM moz_bookmarks b
         WHERE type IN (:folder_type, :separator_type)
           AND fk NOTNULL
       )`);
    fixBookmarksAsFolders.params["bookmark_type"] = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    fixBookmarksAsFolders.params["folder_type"] = PlacesUtils.bookmarks.TYPE_FOLDER;
    fixBookmarksAsFolders.params["separator_type"] = PlacesUtils.bookmarks.TYPE_SEPARATOR;
    fixBookmarksAsFolders.params["rootGuid"] = PlacesUtils.bookmarks.rootGuid;
    fixBookmarksAsFolders.params["menuGuid"] = PlacesUtils.bookmarks.menuGuid;
    fixBookmarksAsFolders.params["toolbarGuid"] = PlacesUtils.bookmarks.toolbarGuid;
    fixBookmarksAsFolders.params["unfiledGuid"] = PlacesUtils.bookmarks.unfiledGuid;
    fixBookmarksAsFolders.params["tagsGuid"] = PlacesUtils.bookmarks.tagsGuid;
    cleanupStatements.push(fixBookmarksAsFolders);

    // D.7 fix wrong item types
    //     Bookmarks should have an fk, if they don't have any, convert them to
    //     folders.
    let fixFoldersAsBookmarks = DBConn.createAsyncStatement(
      `UPDATE moz_bookmarks SET type = :folder_type WHERE guid NOT IN (
         :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
       ) AND id IN (
         SELECT id FROM moz_bookmarks b
         WHERE type = :bookmark_type
           AND fk IS NULL
       )`);
    fixFoldersAsBookmarks.params["bookmark_type"] = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    fixFoldersAsBookmarks.params["folder_type"] = PlacesUtils.bookmarks.TYPE_FOLDER;
    fixFoldersAsBookmarks.params["rootGuid"] = PlacesUtils.bookmarks.rootGuid;
    fixFoldersAsBookmarks.params["menuGuid"] = PlacesUtils.bookmarks.menuGuid;
    fixFoldersAsBookmarks.params["toolbarGuid"] = PlacesUtils.bookmarks.toolbarGuid;
    fixFoldersAsBookmarks.params["unfiledGuid"] = PlacesUtils.bookmarks.unfiledGuid;
    fixFoldersAsBookmarks.params["tagsGuid"] = PlacesUtils.bookmarks.tagsGuid;
    cleanupStatements.push(fixFoldersAsBookmarks);

    // D.9 fix wrong parents
    //     Items cannot have separators or other bookmarks
    //     as parent, if they have bad parent move them to unsorted bookmarks.
    let fixInvalidParents = DBConn.createAsyncStatement(
      `UPDATE moz_bookmarks SET parent = :unsorted_folder WHERE guid NOT IN (
         :rootGuid, :menuGuid, :toolbarGuid, :unfiledGuid, :tagsGuid  /* skip roots */
       ) AND id IN (
         SELECT id FROM moz_bookmarks b
         WHERE EXISTS
           (SELECT id FROM moz_bookmarks WHERE id = b.parent
             AND type IN (:bookmark_type, :separator_type)
             LIMIT 1)
       )`);
    fixInvalidParents.params["unsorted_folder"] = PlacesUtils.unfiledBookmarksFolderId;
    fixInvalidParents.params["bookmark_type"] = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    fixInvalidParents.params["separator_type"] = PlacesUtils.bookmarks.TYPE_SEPARATOR;
    fixInvalidParents.params["rootGuid"] = PlacesUtils.bookmarks.rootGuid;
    fixInvalidParents.params["menuGuid"] = PlacesUtils.bookmarks.menuGuid;
    fixInvalidParents.params["toolbarGuid"] = PlacesUtils.bookmarks.toolbarGuid;
    fixInvalidParents.params["unfiledGuid"] = PlacesUtils.bookmarks.unfiledGuid;
    fixInvalidParents.params["tagsGuid"] = PlacesUtils.bookmarks.tagsGuid;
    cleanupStatements.push(fixInvalidParents);

    // D.10 recalculate positions
    //      This requires multiple related statements.
    //      We can detect a folder with bad position values comparing the sum of
    //      all distinct position values (+1 since position is 0-based) with the
    //      triangular numbers obtained by the number of children (n).
    //      SUM(DISTINCT position + 1) == (n * (n + 1) / 2).
    cleanupStatements.push(DBConn.createAsyncStatement(
      `CREATE TEMP TABLE IF NOT EXISTS moz_bm_reindex_temp (
         id INTEGER PRIMARY_KEY
       , parent INTEGER
       , position INTEGER
       )`
    ));
    cleanupStatements.push(DBConn.createAsyncStatement(
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
    ));
    cleanupStatements.push(DBConn.createAsyncStatement(
      `CREATE INDEX IF NOT EXISTS moz_bm_reindex_temp_index
       ON moz_bm_reindex_temp(parent)`
    ));
    cleanupStatements.push(DBConn.createAsyncStatement(
      `UPDATE moz_bm_reindex_temp SET position = (
         ROWID - (SELECT MIN(t.ROWID) FROM moz_bm_reindex_temp t
                  WHERE t.parent = moz_bm_reindex_temp.parent)
       )`
    ));
    cleanupStatements.push(DBConn.createAsyncStatement(
      `CREATE TEMP TRIGGER IF NOT EXISTS moz_bm_reindex_temp_trigger
       BEFORE DELETE ON moz_bm_reindex_temp
       FOR EACH ROW
       BEGIN
         UPDATE moz_bookmarks SET position = OLD.position WHERE id = OLD.id;
       END`
    ));
    cleanupStatements.push(DBConn.createAsyncStatement(
      "DELETE FROM moz_bm_reindex_temp "
    ));
    cleanupStatements.push(DBConn.createAsyncStatement(
      "DROP INDEX moz_bm_reindex_temp_index "
    ));
    cleanupStatements.push(DBConn.createAsyncStatement(
      "DROP TRIGGER moz_bm_reindex_temp_trigger "
    ));
    cleanupStatements.push(DBConn.createAsyncStatement(
      "DROP TABLE moz_bm_reindex_temp "
    ));

    // D.12 Fix empty-named tags.
    //      Tags were allowed to have empty names due to a UI bug.  Fix them
    //      replacing their title with "(notitle)".
    let fixEmptyNamedTags = DBConn.createAsyncStatement(
      `UPDATE moz_bookmarks SET title = :empty_title
       WHERE length(title) = 0 AND type = :folder_type
         AND parent = :tags_folder`
    );
    fixEmptyNamedTags.params["empty_title"] = "(notitle)";
    fixEmptyNamedTags.params["folder_type"] = PlacesUtils.bookmarks.TYPE_FOLDER;
    fixEmptyNamedTags.params["tags_folder"] = PlacesUtils.tagsFolderId;
    cleanupStatements.push(fixEmptyNamedTags);

    // MOZ_FAVICONS
    // E.1 remove orphan icons
    let deleteOrphanIcons = DBConn.createAsyncStatement(
      `DELETE FROM moz_favicons WHERE id IN (
         SELECT id FROM moz_favicons f
         WHERE NOT EXISTS
           (SELECT id FROM moz_places WHERE favicon_id = f.id LIMIT 1)
       )`);
    cleanupStatements.push(deleteOrphanIcons);

    // MOZ_HISTORYVISITS
    // F.1 remove orphan visits
    let deleteOrphanVisits = DBConn.createAsyncStatement(
      `DELETE FROM moz_historyvisits WHERE id IN (
         SELECT id FROM moz_historyvisits v
         WHERE NOT EXISTS
           (SELECT id FROM moz_places WHERE id = v.place_id LIMIT 1)
       )`);
    cleanupStatements.push(deleteOrphanVisits);

    // MOZ_INPUTHISTORY
    // G.1 remove orphan input history
    let deleteOrphanInputHistory = DBConn.createAsyncStatement(
      `DELETE FROM moz_inputhistory WHERE place_id IN (
         SELECT place_id FROM moz_inputhistory i
         WHERE NOT EXISTS
           (SELECT id FROM moz_places WHERE id = i.place_id LIMIT 1)
       )`);
    cleanupStatements.push(deleteOrphanInputHistory);

    // MOZ_ITEMS_ANNOS
    // H.1 remove item annos with an invalid attribute
    let deleteInvalidAttributeItemsAnnos = DBConn.createAsyncStatement(
      `DELETE FROM moz_items_annos WHERE id IN (
         SELECT id FROM moz_items_annos t
         WHERE NOT EXISTS
           (SELECT id FROM moz_anno_attributes
             WHERE id = t.anno_attribute_id LIMIT 1)
       )`);
    cleanupStatements.push(deleteInvalidAttributeItemsAnnos);

    // H.2 remove orphan item annos
    let deleteOrphanItemsAnnos = DBConn.createAsyncStatement(
      `DELETE FROM moz_items_annos WHERE id IN (
         SELECT id FROM moz_items_annos t
         WHERE NOT EXISTS
           (SELECT id FROM moz_bookmarks WHERE id = t.item_id LIMIT 1)
       )`);
    cleanupStatements.push(deleteOrphanItemsAnnos);

    // MOZ_KEYWORDS
    // I.1 remove unused keywords
    let deleteUnusedKeywords = DBConn.createAsyncStatement(
      `DELETE FROM moz_keywords WHERE id IN (
         SELECT id FROM moz_keywords k
         WHERE NOT EXISTS
           (SELECT 1 FROM moz_places h WHERE k.place_id = h.id)
       )`);
    cleanupStatements.push(deleteUnusedKeywords);

    // MOZ_PLACES
    // L.1 fix wrong favicon ids
    let fixInvalidFaviconIds = DBConn.createAsyncStatement(
      `UPDATE moz_places SET favicon_id = NULL WHERE id IN (
         SELECT id FROM moz_places h
         WHERE favicon_id NOT NULL
           AND NOT EXISTS
             (SELECT id FROM moz_favicons WHERE id = h.favicon_id LIMIT 1)
       )`);
    cleanupStatements.push(fixInvalidFaviconIds);

    // L.2 recalculate visit_count and last_visit_date
    let fixVisitStats = DBConn.createAsyncStatement(
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
       )`);
    cleanupStatements.push(fixVisitStats);

    // L.3 recalculate hidden for redirects.
    let fixRedirectsHidden = DBConn.createAsyncStatement(
      `UPDATE moz_places
       SET hidden = 1
       WHERE id IN (
         SELECT h.id FROM moz_places h
         JOIN moz_historyvisits src ON src.place_id = h.id
         JOIN moz_historyvisits dst ON dst.from_visit = src.id AND dst.visit_type IN (5,6)
         LEFT JOIN moz_bookmarks on fk = h.id AND fk ISNULL
         GROUP BY src.place_id HAVING count(*) = visit_count
       )`);
    cleanupStatements.push(fixRedirectsHidden);

    // L.4 recalculate foreign_count.
    let fixForeignCount = DBConn.createAsyncStatement(
      `UPDATE moz_places SET foreign_count =
         (SELECT count(*) FROM moz_bookmarks WHERE fk = moz_places.id ) +
         (SELECT count(*) FROM moz_keywords WHERE place_id = moz_places.id )`);
    cleanupStatements.push(fixForeignCount);

    // L.5 recalculate missing hashes.
    let fixMissingHashes = DBConn.createAsyncStatement(
      `UPDATE moz_places SET url_hash = hash(url) WHERE url_hash = 0`);
    cleanupStatements.push(fixMissingHashes);

    // MAINTENANCE STATEMENTS SHOULD GO ABOVE THIS POINT!

    return cleanupStatements;
  },

  /**
   * Tries to vacuum the database.
   *
   * @param [optional] aTasks
   *        Tasks object to execute.
   */
  vacuum: function PDBU_vacuum(aTasks)
  {
    let tasks = new Tasks(aTasks);
    tasks.log("> Vacuum");

    let DBFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
    DBFile.append("places.sqlite");
    tasks.log("Initial database size is " +
              parseInt(DBFile.fileSize / 1024) + " KiB");

    let stmt = DBConn.createAsyncStatement("VACUUM");
    stmt.executeAsync({
      handleError: PlacesDBUtils._handleError,
      handleResult: function () {},

      handleCompletion: function (aReason)
      {
        if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
          tasks.log("+ The database has been vacuumed");
          let vacuumedDBFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
          vacuumedDBFile.append("places.sqlite");
          tasks.log("Final database size is " +
                    parseInt(vacuumedDBFile.fileSize / 1024) + " KiB");
        }
        else {
          tasks.log("- Unable to vacuum database");
          tasks.clear();
        }

        PlacesDBUtils._executeTasks(tasks);
      }
    });
    stmt.finalize();
  },

  /**
   * Forces a full expiration on the database.
   *
   * @param [optional] aTasks
   *        Tasks object to execute.
   */
  expire: function PDBU_expire(aTasks)
  {
    let tasks = new Tasks(aTasks);
    tasks.log("> Orphans expiration");

    let expiration = Cc["@mozilla.org/places/expiration;1"].
                     getService(Ci.nsIObserver);

    Services.obs.addObserver(function (aSubject, aTopic, aData) {
      Services.obs.removeObserver(arguments.callee, aTopic);
      tasks.log("+ Database cleaned up");
      PlacesDBUtils._executeTasks(tasks);
    }, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);

    // Force an orphans expiration step.
    expiration.observe(null, "places-debug-start-expiration", 0);
  },

  /**
   * Collects statistical data on the database.
   *
   * @param [optional] aTasks
   *        Tasks object to execute.
   */
  stats: function PDBU_stats(aTasks)
  {
    let tasks = new Tasks(aTasks);
    tasks.log("> Statistics");

    let DBFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
    DBFile.append("places.sqlite");
    tasks.log("Database size is " + parseInt(DBFile.fileSize / 1024) + " KiB");

    [ "user_version"
    , "page_size"
    , "cache_size"
    , "journal_mode"
    , "synchronous"
    ].forEach(function (aPragma) {
      let stmt = DBConn.createStatement("PRAGMA " + aPragma);
      stmt.executeStep();
      tasks.log(aPragma + " is " + stmt.getString(0));
      stmt.finalize();
    });

    // Get maximum number of unique URIs.
    try {
      let limitURIs = Services.prefs.getIntPref(
        "places.history.expiration.transient_current_max_pages");
      tasks.log("History can store a maximum of " + limitURIs + " unique pages");
    } catch (ex) {}

    let stmt = DBConn.createStatement(
      "SELECT name FROM sqlite_master WHERE type = :type");
    stmt.params.type = "table";
    while (stmt.executeStep()) {
      let tableName = stmt.getString(0);
      let countStmt = DBConn.createStatement(
        `SELECT count(*) FROM ${tableName}`);
      countStmt.executeStep();
      tasks.log("Table " + tableName + " has " + countStmt.getInt32(0) + " records");
      countStmt.finalize();
    }
    stmt.reset();

    stmt.params.type = "index";
    while (stmt.executeStep()) {
      tasks.log("Index " + stmt.getString(0));
    }
    stmt.reset();

    stmt.params.type = "trigger";
    while (stmt.executeStep()) {
      tasks.log("Trigger " + stmt.getString(0));
    }
    stmt.finalize();

    PlacesDBUtils._executeTasks(tasks);
  },

  /**
   * Collects telemetry data and reports it to Telemetry.
   *
   * @param [optional] aTasks
   *        Tasks object to execute.
   */
  telemetry: function PDBU_telemetry(aTasks)
  {
    let tasks = new Tasks(aTasks);

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
        callback: function () {
          let DBFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
          DBFile.append("places.sqlite");
          return parseInt(DBFile.fileSize / BYTES_PER_MEBIBYTE);
        }
      },

      { histogram: "PLACES_DATABASE_PAGESIZE_B",
        query:     "PRAGMA page_size /* PlacesDBUtils.jsm PAGESIZE_B */" },

      { histogram: "PLACES_DATABASE_SIZE_PER_PAGE_B",
        query:     "PRAGMA page_count",
        callback: function (aDbPageCount) {
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
        callback: function () {
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

        let stmt = DBConn.createAsyncStatement(probe.query);
        for (let param in params) {
          if (probe.query.indexOf(":" + param) > 0) {
            stmt.params[param] = params[param];
          }
        }

        try {
          stmt.executeAsync({
            handleError: reject,
            handleResult: function (aResultSet) {
              let row = aResultSet.getNextRow();
              resolve([probe, row.getResultByIndex(0)]);
            },
            handleCompletion: function () {}
          });
        } finally {
          stmt.finalize();
        }
      });

      // Report the result of the probe through Telemetry.
      // The resulting promise cannot reject.
      promiseDone.then(
        // On success
        ([aProbe, aValue]) => {
          let value = aValue;
          try {
            if ("callback" in aProbe) {
              value = aProbe.callback(value);
            }
            probeValues[aProbe.histogram] = value;
            Services.telemetry.getHistogramById(aProbe.histogram).add(value);
          } catch (ex) {
            Components.utils.reportError("Error adding value " + value +
                                         " to histogram " + aProbe.histogram +
                                         ": " + ex);
          }
        },
        // On failure
        this._handleError);
    }

    PlacesDBUtils._executeTasks(tasks);
  },

  /**
   * Runs a list of tasks, notifying log messages to the callback.
   *
   * @param aTasks
   *        Array of tasks to be executed, in form of pointers to methods in
   *        this module.
   * @param [optional] aCallback
   *        Callback to be invoked when done.  It will receive an array of
   *        log messages.
   */
  runTasks: function PDBU_runTasks(aTasks, aCallback) {
    let tasks = new Tasks(aTasks);
    tasks.callback = aCallback;
    PlacesDBUtils._executeTasks(tasks);
  }
};

/**
 * LIFO tasks stack.
 *
 * @param [optional] aTasks
 *        Array of tasks or another Tasks object to clone.
 */
function Tasks(aTasks)
{
  if (aTasks) {
    if (Array.isArray(aTasks)) {
      this._list = aTasks.slice(0, aTasks.length);
    }
    // This supports passing in a Tasks-like object, with a "list" property,
    // for compatibility reasons.
    else if (typeof(aTasks) == "object" &&
             (Tasks instanceof Tasks || "list" in aTasks)) {
      this._list = aTasks.list;
      this._log = aTasks.messages;
      this.callback = aTasks.callback;
      this.scope = aTasks.scope;
      this._telemetryStart = aTasks._telemetryStart;
    }
  }
}

Tasks.prototype = {
  _list: [],
  _log: [],
  callback: null,
  scope: null,
  _telemetryStart: 0,

  /**
   * Adds a task to the top of the list.
   *
   * @param aNewElt
   *        Task to be added.
   */
  push: function T_push(aNewElt)
  {
    this._list.unshift(aNewElt);
  },

  /**
   * Returns and consumes next task.
   *
   * @return next task or undefined if no task is left.
   */
  pop: function T_pop()
  {
    return this._list.shift();
  },

  /**
   * Removes all tasks.
   */
  clear: function T_clear()
  {
    this._list.length = 0;
  },

  /**
   * Returns array of tasks ordered from the next to be run to the latest.
   */
  get list()
  {
    return this._list.slice(0, this._list.length);
  },

  /**
   * Adds a message to the log.
   *
   * @param aMsg
   *        String message to be added.
   */
  log: function T_log(aMsg)
  {
    this._log.push(aMsg);
  },

  /**
   * Returns array of log messages ordered from oldest to newest.
   */
  get messages()
  {
    return this._log.slice(0, this._log.length);
  },
}
