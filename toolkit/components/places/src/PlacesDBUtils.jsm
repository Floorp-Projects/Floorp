/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab filetype=javascript
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places Database Utils code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");

let EXPORTED_SYMBOLS = [ "PlacesDBUtils" ];

////////////////////////////////////////////////////////////////////////////////
//// Constants

const FINISHED_MAINTENANCE_TOPIC = "places-maintenance-finished";

////////////////////////////////////////////////////////////////////////////////
//// Smart getters

XPCOMUtils.defineLazyGetter(this, "DBConn", function() {
  return PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
});

////////////////////////////////////////////////////////////////////////////////
//// PlacesDBUtils

let PlacesDBUtils = {
  /**
   * Executes a list of maintenance tasks.
   * Once finished it will pass a array log to the callback attached to tasks,
   * or print out to the error console if no callback is defined.
   * FINISHED_MAINTENANCE_TOPIC is notified through observer service on finish.
   *
   * @param aTasks
   *        Tasks object to execute.
   */
  _executeTasks: function PDBU__executeTasks(aTasks)
  {
    let task = aTasks.pop();
    if (task) {
      task.call(PlacesDBUtils, aTasks);
    }
    else {
      if (aTasks.callback) {
        let scope = aTasks.scope || Cu.getGlobalForObject(aTasks.callback);
        aTasks.callback.call(scope, aTasks.messages);
      }
      else {
        // Output to the error console.
        let messages = aTasks.messages
                             .unshift("[ Places Maintenance ]");  
        try {
          Services.console.logStringMessage(messages.join("\n"));
        } catch(ex) {}
      }

      // Notify observers that maintenance finished.
      Services.prefs.setIntPref("places.database.lastMaintenance", parseInt(Date.now() / 1000));
      Services.obs.notifyObservers(null, FINISHED_MAINTENANCE_TOPIC, null);
    }
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
    tasks.callback = aCallback;
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
  _checkIntegritySkipReindex: function PDBU__checkIntegritySkipReindex(aTasks)
    this.checkIntegrity(aTasks, true),

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

    let stmts = this._getBoundCoherenceStatements();
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
    stmts.forEach(function (aStmt) aStmt.finalize());
  },

  _getBoundCoherenceStatements: function PDBU__getBoundCoherenceStatements()
  {
    let cleanupStatements = [];

    // MOZ_ANNO_ATTRIBUTES
    // A.1 remove unused attributes
    let deleteUnusedAnnoAttributes = DBConn.createStatement(
      "DELETE FROM moz_anno_attributes WHERE id IN ( " +
        "SELECT id FROM moz_anno_attributes n " +
        "WHERE NOT EXISTS " +
            "(SELECT id FROM moz_annos WHERE anno_attribute_id = n.id LIMIT 1) " +
          "AND NOT EXISTS " +
            "(SELECT id FROM moz_items_annos WHERE anno_attribute_id = n.id LIMIT 1) " +
      ")");
    cleanupStatements.push(deleteUnusedAnnoAttributes);

    // MOZ_ANNOS
    // B.1 remove annos with an invalid attribute
    let deleteInvalidAttributeAnnos = DBConn.createStatement(
      "DELETE FROM moz_annos WHERE id IN ( " +
        "SELECT id FROM moz_annos a " +
        "WHERE NOT EXISTS " +
          "(SELECT id FROM moz_anno_attributes " +
            "WHERE id = a.anno_attribute_id LIMIT 1) " +
      ")");
    cleanupStatements.push(deleteInvalidAttributeAnnos);

    // B.2 remove orphan annos
    let deleteOrphanAnnos = DBConn.createStatement(
      "DELETE FROM moz_annos WHERE id IN ( " +
        "SELECT id FROM moz_annos a " +
        "WHERE NOT EXISTS " +
          "(SELECT id FROM moz_places WHERE id = a.place_id LIMIT 1) " +
      ")");
    cleanupStatements.push(deleteOrphanAnnos);

    // MOZ_BOOKMARKS_ROOTS
    // C.1 fix missing Places root
    //     Bug 477739 shows a case where the root could be wrongly removed
    //     due to an endianness issue.  We try to fix broken roots here.
    let selectPlacesRoot = DBConn.createStatement(
      "SELECT id FROM moz_bookmarks WHERE id = :places_root");
    selectPlacesRoot.params["places_root"] = PlacesUtils.placesRootId;
    if (!selectPlacesRoot.executeStep()) {
      // We are missing the root, try to recreate it.
      let createPlacesRoot = DBConn.createStatement(
        "INSERT INTO moz_bookmarks (id, type, fk, parent, position, title, "
      +                            "guid) "
      + "VALUES (:places_root, 2, NULL, 0, 0, :title, GENERATE_GUID())");
      createPlacesRoot.params["places_root"] = PlacesUtils.placesRootId;
      createPlacesRoot.params["title"] = "";
      cleanupStatements.push(createPlacesRoot);

      // Now ensure that other roots are children of Places root.
      let fixPlacesRootChildren = DBConn.createStatement(
        "UPDATE moz_bookmarks SET parent = :places_root WHERE id IN " +
          "(SELECT folder_id FROM moz_bookmarks_roots " +
            "WHERE folder_id <> :places_root)");
      fixPlacesRootChildren.params["places_root"] = PlacesUtils.placesRootId;
      cleanupStatements.push(fixPlacesRootChildren);
    }
    selectPlacesRoot.finalize();

    // C.2 fix roots titles
    //     some alpha version has wrong roots title, and this also fixes them if
    //     locale has changed.
    let updateRootTitleSql = "UPDATE moz_bookmarks SET title = :title " +
                             "WHERE id = :root_id AND title <> :title";
    // root
    let fixPlacesRootTitle = DBConn.createStatement(updateRootTitleSql);
    fixPlacesRootTitle.params["root_id"] = PlacesUtils.placesRootId;
    fixPlacesRootTitle.params["title"] = "";
    cleanupStatements.push(fixPlacesRootTitle);
    // bookmarks menu
    let fixBookmarksMenuTitle = DBConn.createStatement(updateRootTitleSql);
    fixBookmarksMenuTitle.params["root_id"] = PlacesUtils.bookmarksMenuFolderId;
    fixBookmarksMenuTitle.params["title"] =
      PlacesUtils.getString("BookmarksMenuFolderTitle");
    cleanupStatements.push(fixBookmarksMenuTitle);
    // bookmarks toolbar
    let fixBookmarksToolbarTitle = DBConn.createStatement(updateRootTitleSql);
    fixBookmarksToolbarTitle.params["root_id"] = PlacesUtils.toolbarFolderId;
    fixBookmarksToolbarTitle.params["title"] =
      PlacesUtils.getString("BookmarksToolbarFolderTitle");
    cleanupStatements.push(fixBookmarksToolbarTitle);
    // unsorted bookmarks
    let fixUnsortedBookmarksTitle = DBConn.createStatement(updateRootTitleSql);
    fixUnsortedBookmarksTitle.params["root_id"] = PlacesUtils.unfiledBookmarksFolderId;
    fixUnsortedBookmarksTitle.params["title"] =
      PlacesUtils.getString("UnsortedBookmarksFolderTitle");
    cleanupStatements.push(fixUnsortedBookmarksTitle);
    // tags
    let fixTagsRootTitle = DBConn.createStatement(updateRootTitleSql);
    fixTagsRootTitle.params["root_id"] = PlacesUtils.tagsFolderId;
    fixTagsRootTitle.params["title"] =
      PlacesUtils.getString("TagsFolderTitle");
    cleanupStatements.push(fixTagsRootTitle);

    // MOZ_BOOKMARKS
    // D.1 remove items without a valid place
    // if fk IS NULL we fix them in D.7
    let deleteNoPlaceItems = DBConn.createStatement(
      "DELETE FROM moz_bookmarks WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " + // skip roots
      ") AND id IN (" +
        "SELECT b.id FROM moz_bookmarks b " +
        "WHERE fk NOT NULL AND b.type = :bookmark_type " +
          "AND NOT EXISTS (SELECT url FROM moz_places WHERE id = b.fk LIMIT 1) " +
      ")");
    deleteNoPlaceItems.params["bookmark_type"] = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    cleanupStatements.push(deleteNoPlaceItems);

    // D.2 remove items that are not uri bookmarks from tag containers
    let deleteBogusTagChildren = DBConn.createStatement(
      "DELETE FROM moz_bookmarks WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " + // skip roots
      ") AND id IN (" +
        "SELECT b.id FROM moz_bookmarks b " +
        "WHERE b.parent IN " +
          "(SELECT id FROM moz_bookmarks WHERE parent = :tags_folder) " +
          "AND b.type <> :bookmark_type " +
      ")");
    deleteBogusTagChildren.params["tags_folder"] = PlacesUtils.tagsFolderId;
    deleteBogusTagChildren.params["bookmark_type"] = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    cleanupStatements.push(deleteBogusTagChildren);

    // D.3 remove empty tags
    let deleteEmptyTags = DBConn.createStatement(
      "DELETE FROM moz_bookmarks WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " + // skip roots
      ") AND id IN (" +
        "SELECT b.id FROM moz_bookmarks b " +
        "WHERE b.id IN " +
          "(SELECT id FROM moz_bookmarks WHERE parent = :tags_folder) " +
          "AND NOT EXISTS " +
            "(SELECT id from moz_bookmarks WHERE parent = b.id LIMIT 1) " +
      ")");
    deleteEmptyTags.params["tags_folder"] = PlacesUtils.tagsFolderId;
    cleanupStatements.push(deleteEmptyTags);

    // D.4 move orphan items to unsorted folder
    let fixOrphanItems = DBConn.createStatement(
      "UPDATE moz_bookmarks SET parent = :unsorted_folder WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " +  // skip roots
      ") AND id IN (" +
        "SELECT b.id FROM moz_bookmarks b " +
        "WHERE b.parent <> 0 " + // exclude Places root
        "AND NOT EXISTS " +
          "(SELECT id FROM moz_bookmarks WHERE id = b.parent LIMIT 1) " +
      ")");
    fixOrphanItems.params["unsorted_folder"] = PlacesUtils.unfiledBookmarksFolderId;
    cleanupStatements.push(fixOrphanItems);

    // D.5 fix wrong keywords
    let fixInvalidKeywords = DBConn.createStatement(
      "UPDATE moz_bookmarks SET keyword_id = NULL WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " + // skip roots
      ") AND id IN ( " +
        "SELECT id FROM moz_bookmarks b " +
        "WHERE keyword_id NOT NULL " +
          "AND NOT EXISTS " +
            "(SELECT id FROM moz_keywords WHERE id = b.keyword_id LIMIT 1) " +
      ")");
    cleanupStatements.push(fixInvalidKeywords);

    // D.6 fix wrong item types
    //     Folders, separators and dynamic containers should not have an fk.
    //     If they have a valid fk convert them to bookmarks. Later in D.9 we
    //     will move eventual children to unsorted bookmarks.
    let fixBookmarksAsFolders = DBConn.createStatement(
      "UPDATE moz_bookmarks SET type = :bookmark_type WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " + // skip roots
      ") AND id IN ( " +
        "SELECT id FROM moz_bookmarks b " +
        "WHERE type IN (:folder_type, :separator_type, :dynamic_type) " +
          "AND fk NOTNULL " +
      ")");
    fixBookmarksAsFolders.params["bookmark_type"] = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    fixBookmarksAsFolders.params["folder_type"] = PlacesUtils.bookmarks.TYPE_FOLDER;
    fixBookmarksAsFolders.params["separator_type"] = PlacesUtils.bookmarks.TYPE_SEPARATOR;
    fixBookmarksAsFolders.params["dynamic_type"] = PlacesUtils.bookmarks.TYPE_DYNAMIC_CONTAINER;
    cleanupStatements.push(fixBookmarksAsFolders);

    // D.7 fix wrong item types
    //     Bookmarks should have an fk, if they don't have any, convert them to
    //     folders.
    let fixFoldersAsBookmarks = DBConn.createStatement(
      "UPDATE moz_bookmarks SET type = :folder_type WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " + // skip roots
      ") AND id IN ( " +
        "SELECT id FROM moz_bookmarks b " +
        "WHERE type = :bookmark_type " +
          "AND fk IS NULL " +
      ")");
    fixFoldersAsBookmarks.params["bookmark_type"] = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    fixFoldersAsBookmarks.params["folder_type"] = PlacesUtils.bookmarks.TYPE_FOLDER;
    cleanupStatements.push(fixFoldersAsBookmarks);

    // D.8 fix wrong item types
    //     Dynamic containers should have a folder_type, if they don't have any
    //     convert them to folders.
    let fixFoldersAsDynamic = DBConn.createStatement(
      "UPDATE moz_bookmarks SET type = :folder_type WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " + // skip roots
      ") AND id IN ( " +
        "SELECT id FROM moz_bookmarks b " +
        "WHERE type = :dynamic_type " +
          "AND folder_type IS NULL " +
      ")");
    fixFoldersAsDynamic.params["dynamic_type"] = PlacesUtils.bookmarks.TYPE_DYNAMIC_CONTAINER;
    fixFoldersAsDynamic.params["folder_type"] = PlacesUtils.bookmarks.TYPE_FOLDER;
    cleanupStatements.push(fixFoldersAsDynamic);

    // D.9 fix wrong parents
    //     Items cannot have dynamic containers, separators or other bookmarks
    //     as parent, if they have bad parent move them to unsorted bookmarks.
    let fixInvalidParents = DBConn.createStatement(
      "UPDATE moz_bookmarks SET parent = :unsorted_folder WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " +  // skip roots
      ") AND id IN ( " +
        "SELECT id FROM moz_bookmarks b " +
        "WHERE EXISTS " +
          "(SELECT id FROM moz_bookmarks WHERE id = b.parent " +
            "AND type IN (:bookmark_type, :separator_type, :dynamic_type) " +
            "LIMIT 1) " +
      ")");
    fixInvalidParents.params["unsorted_folder"] = PlacesUtils.unfiledBookmarksFolderId;
    fixInvalidParents.params["bookmark_type"] = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    fixInvalidParents.params["separator_type"] = PlacesUtils.bookmarks.TYPE_SEPARATOR;
    fixInvalidParents.params["dynamic_type"] = PlacesUtils.bookmarks.TYPE_DYNAMIC_CONTAINER;
    cleanupStatements.push(fixInvalidParents);

/* XXX needs test
    // D.10 recalculate positions
    //      This requires multiple related statements.
    //      We can detect a folder with bad position values comparing the sum of
    //      all position values with the triangular numbers obtained by the number
    //      of children: (n * (n + 1) / 2). Starting from 0 is (n * (n - 1) / 2).
    let detectWrongPositionsParents = DBConn.createStatement(
      "SELECT parent FROM " +
        "(SELECT parent, " +
                "(SUM(position) - (count(*) * (count(*) - 1) / 2)) AS diff " +
        "FROM moz_bookmarks " +
        "GROUP BY parent) " +
      "WHERE diff <> 0");
    while (detectWrongPositionsParents.executeStep()) {
      let parent = detectWrongPositionsParents.getInt64(0);
      // We will lose the previous position values and reposition items based
      // on the ROWID value. Not perfect, but we can't rely on position values.
      let fixPositionsForParent = DBConn.createStatement(
        "UPDATE moz_bookmarks SET position = ( " +
          "SELECT " +
          "((SELECT count(*) FROM moz_bookmarks WHERE parent = :parent) - " +
           "(SELECT count(*) FROM moz_bookmarks " +
            "WHERE parent = :parent AND ROWID >= b.ROWID)) " +
          "FROM moz_bookmarks b WHERE parent = :parent AND id = moz_bookmarks.id " +
        ") WHERE parent = :parent");
      fixPositionsForParent.params["parent"] = parent;
      cleanupStatements.push(fixPositionsForParent);
    }
*/

    // D.11 remove old livemarks status items
    //      Livemark status items are now static but some livemark has still old
    //      status items bookmarks inside it. We should remove them.
    let removeLivemarkStaticItems = DBConn.createStatement(
      "DELETE FROM moz_bookmarks WHERE type = :bookmark_type AND fk IN ( " +
        "SELECT id FROM moz_places WHERE url = :lmloading OR url = :lmfailed " +
      ")");
    removeLivemarkStaticItems.params["bookmark_type"] = PlacesUtils.bookmarks.TYPE_BOOKMARK;
    removeLivemarkStaticItems.params["lmloading"] = "about:livemark-loading";
    removeLivemarkStaticItems.params["lmfailed"] = "about:livemark-failed";
    cleanupStatements.push(removeLivemarkStaticItems);

    // D.12 Fix empty-named tags.
    //      Tags were allowed to have empty names due to a UI bug.  Fix them
    //      replacing their title with "(notitle)".
    let fixEmptyNamedTags = DBConn.createStatement(
      "UPDATE moz_bookmarks SET title = :empty_title " +
      "WHERE length(title) = 0 AND type = :folder_type " +
        "AND parent = :tags_folder"
    );
    fixEmptyNamedTags.params["empty_title"] = "(notitle)";
    fixEmptyNamedTags.params["folder_type"] = PlacesUtils.bookmarks.TYPE_FOLDER;
    fixEmptyNamedTags.params["tags_folder"] = PlacesUtils.tagsFolderId;
    cleanupStatements.push(fixEmptyNamedTags);

    // MOZ_FAVICONS
    // E.1 remove orphan icons
    let deleteOrphanIcons = DBConn.createStatement(
      "DELETE FROM moz_favicons WHERE id IN (" +
        "SELECT id FROM moz_favicons f " +
        "WHERE NOT EXISTS " +
          "(SELECT id FROM moz_places WHERE favicon_id = f.id LIMIT 1) " +
      ")");
    cleanupStatements.push(deleteOrphanIcons);

    // MOZ_HISTORYVISITS
    // F.1 remove orphan visits
    let deleteOrphanVisits = DBConn.createStatement(
      "DELETE FROM moz_historyvisits WHERE id IN (" +
        "SELECT id FROM moz_historyvisits v " +
        "WHERE NOT EXISTS " +
          "(SELECT id FROM moz_places WHERE id = v.place_id LIMIT 1) " +
      ")");
    cleanupStatements.push(deleteOrphanVisits);

    // MOZ_INPUTHISTORY
    // G.1 remove orphan input history
    let deleteOrphanInputHistory = DBConn.createStatement(
      "DELETE FROM moz_inputhistory WHERE place_id IN (" +
        "SELECT place_id FROM moz_inputhistory i " +
        "WHERE NOT EXISTS " +
          "(SELECT id FROM moz_places WHERE id = i.place_id LIMIT 1) " +
      ")");
    cleanupStatements.push(deleteOrphanInputHistory);

    // MOZ_ITEMS_ANNOS
    // H.1 remove item annos with an invalid attribute
    let deleteInvalidAttributeItemsAnnos = DBConn.createStatement(
      "DELETE FROM moz_items_annos WHERE id IN ( " +
        "SELECT id FROM moz_items_annos t " +
        "WHERE NOT EXISTS " +
          "(SELECT id FROM moz_anno_attributes " +
            "WHERE id = t.anno_attribute_id LIMIT 1) " +
      ")");
    cleanupStatements.push(deleteInvalidAttributeItemsAnnos);

    // H.2 remove orphan item annos
    let deleteOrphanItemsAnnos = DBConn.createStatement(
      "DELETE FROM moz_items_annos WHERE id IN ( " +
        "SELECT id FROM moz_items_annos t " +
        "WHERE NOT EXISTS " +
          "(SELECT id FROM moz_bookmarks WHERE id = t.item_id LIMIT 1) " +
      ")");
    cleanupStatements.push(deleteOrphanItemsAnnos);

    // MOZ_KEYWORDS
    // I.1 remove unused keywords
    let deleteUnusedKeywords = DBConn.createStatement(
      "DELETE FROM moz_keywords WHERE id IN ( " +
        "SELECT id FROM moz_keywords k " +
        "WHERE NOT EXISTS " +
          "(SELECT id FROM moz_bookmarks WHERE keyword_id = k.id LIMIT 1) " +
      ")");
    cleanupStatements.push(deleteUnusedKeywords);

    // MOZ_PLACES
    // L.1 fix wrong favicon ids
    let fixInvalidFaviconIds = DBConn.createStatement(
      "UPDATE moz_places SET favicon_id = NULL WHERE id IN ( " +
        "SELECT id FROM moz_places h " +
        "WHERE favicon_id NOT NULL " +
          "AND NOT EXISTS " +
            "(SELECT id FROM moz_favicons WHERE id = h.favicon_id LIMIT 1) " +
      ")");
    cleanupStatements.push(fixInvalidFaviconIds);

/* XXX needs test
    // L.2 recalculate visit_count
    let detectWrongCountPlaces = DBConn.createStatement(
      "SELECT id FROM moz_places h " +
      "WHERE h.visit_count <> " +
          "(SELECT count(*) FROM moz_historyvisits " +
            "WHERE place_id = h.id AND visit_type NOT IN (0,4,7,8))");
    while (detectWrongCountPlaces.executeStep()) {
      let placeId = detectWrongCountPlaces.getInt64(0);
      let fixCountForPlace = DBConn.createStatement(
        "UPDATE moz_places SET visit_count = ( " +
          "(SELECT count(*) FROM moz_historyvisits " +
            "WHERE place_id = :place_id AND visit_type NOT IN (0,4,7,8)) + "
        ") WHERE id = :place_id");
      fixCountForPlace.params["place_id"] = placeId;
      cleanupStatements.push(fixCountForPlace);
    }
*/

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

    // Force a full expiration step.
    expiration.observe(null, "places-debug-start-expiration", -1);
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
    } catch(ex) {}

    let stmt = DBConn.createStatement(
      "SELECT name FROM sqlite_master WHERE type = :type");
    stmt.params.type = "table";
    while (stmt.executeStep()) {
      let tableName = stmt.getString(0);
      let countStmt = DBConn.createStatement(
        "SELECT count(*) FROM " + tableName);
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
  if (Array.isArray(aTasks)) {
    this._list = aTasks.slice(0, aTasks.length);
  }
  else if ("list" in aTasks) {
    this._list = aTasks.list;
    this._log = aTasks.messages;
    this.callback = aTasks.callback;
    this.scope = aTasks.scope;
  }
}

Tasks.prototype = {
  _list: [],
  _log: [],
  callback: null,
  scope: null,

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
  pop: function T_pop() this._list.shift(),

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
  get list() this._list.slice(0, this._list.length),

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
  get messages() this._log.slice(0, this._log.length),
}
