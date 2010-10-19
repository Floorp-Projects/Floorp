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
 * Mozilla Corporation.
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

const FINISHED_MAINTENANCE_NOTIFICATION_TOPIC = "places-maintenance-finished";

////////////////////////////////////////////////////////////////////////////////
//// Smart getters

XPCOMUtils.defineLazyGetter(this, "DBConn", function() {
  return PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
});

////////////////////////////////////////////////////////////////////////////////
//// nsPlacesDBUtils class

function nsPlacesDBUtils() {
}

nsPlacesDBUtils.prototype = {
  _statementsRunningCount: 0,

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageStatementCallback

  handleError: function PDBU_handleError(aError) {
    Cu.reportError("Async statement execution returned with '" +
                   aError.result + "', '" + aError.message + "'");
  },

  handleCompletion: function PDBU_handleCompletion(aReason) {
    // We serve only the last statement completion
    if (--this._statementsRunningCount > 0)
      return;

    // We finished executing all statements.
    // Sending Begin/EndUpdateBatch notification will ensure that the UI
    // is correctly refreshed.
    PlacesUtils.history.runInBatchMode({runBatched: function(aUserData){}}, null);
    PlacesUtils.bookmarks.runInBatchMode({runBatched: function(aUserData){}}, null);
    // Notify observers that maintenance tasks are complete
    Services.obs.notifyObservers(null, FINISHED_MAINTENANCE_NOTIFICATION_TOPIC, null);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Tasks

  maintenanceOnIdle: function PDBU_maintenanceOnIdle() {
    // bug 431558: preventive maintenance for Places database
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
          "(SELECT id FROM moz_places_temp WHERE id = a.place_id LIMIT 1) " +
        "AND NOT EXISTS " +
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
        "INSERT INTO moz_bookmarks (id, type, fk, parent, position, title) " +
        "VALUES (:places_root, 2, NULL, 0, 0, :title)");
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
          "AND NOT EXISTS (SELECT url FROM moz_places_temp WHERE id = b.fk LIMIT 1) " +
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
    //      Note: This does not need to query the temp table.
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
          "(SELECT id FROM moz_places_temp WHERE favicon_id = f.id LIMIT 1) " +
          "AND NOT EXISTS" +
          "(SELECT id FROM moz_places WHERE favicon_id = f.id LIMIT 1) " +
      ")");
    cleanupStatements.push(deleteOrphanIcons);

    // MOZ_HISTORYVISITS
    // F.1 remove orphan visits
    let deleteOrphanVisits = DBConn.createStatement(
      "DELETE FROM moz_historyvisits WHERE id IN (" +
        "SELECT id FROM moz_historyvisits v " +
        "WHERE NOT EXISTS " +
          "(SELECT id FROM moz_places_temp WHERE id = v.place_id LIMIT 1) " +
          "AND NOT EXISTS " +
          "(SELECT id FROM moz_places WHERE id = v.place_id LIMIT 1) " +
      ")");
    cleanupStatements.push(deleteOrphanVisits);

    // MOZ_INPUTHISTORY
    // G.1 remove orphan input history
    let deleteOrphanInputHistory = DBConn.createStatement(
      "DELETE FROM moz_inputhistory WHERE place_id IN (" +
        "SELECT place_id FROM moz_inputhistory i " +
        "WHERE NOT EXISTS " +
          "(SELECT id FROM moz_places_temp WHERE id = i.place_id LIMIT 1) " +
          "AND NOT EXISTS " +
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
    // We're detecting errors only in disk table since temp tables could have
    // different values based on the number of visits not yet synced to disk.
    let detectWrongCountPlaces = DBConn.createStatement(
      "SELECT id FROM moz_places h " +
      "WHERE id NOT IN (SELECT id FROM moz_places_temp) " +
        "AND h.visit_count <> " +
          "(SELECT count(*) FROM moz_historyvisits " +
            "WHERE place_id = h.id AND visit_type NOT IN (0,4,7,8))");
    while (detectWrongCountPlaces.executeStep()) {
      let placeId = detectWrongCountPlaces.getInt64(0);
      let fixCountForPlace = DBConn.createStatement(
        "UPDATE moz_places_view SET visit_count = ( " +
          "(SELECT count(*) FROM moz_historyvisits " +
            "WHERE place_id = :place_id AND visit_type NOT IN (0,4,7,8)) + " +
          "(SELECT count(*) FROM moz_historyvisits_temp " +
            "WHERE place_id = :place_id AND visit_type NOT IN (0,4,7,8)) + " +
        ") WHERE id = :place_id");
      fixCountForPlace.params["place_id"] = placeId;
      cleanupStatements.push(fixCountForPlace);
    }
*/

    // MAINTENANCE STATEMENTS SHOULD GO ABOVE THIS POINT!

    // Used to keep track of last call to handleCompletion
    this._statementsRunningCount = cleanupStatements.length;
    // Statements are automatically queued-up by mozStorage
    cleanupStatements.forEach(function (aStatement) {
        aStatement.executeAsync(this);
        aStatement.finalize();
      }, this);
  },

  /**
   * This method is only for support purposes, it will run sync and will take
   * lot of time on big databases, but can be manually triggered to help
   * debugging common issues.
   */
  checkAndFixDatabase: function PDBU_checkAndFixDatabase(aLogCallback) {
    let log = [];
    let self = this;
    let sep = "- - -";

    function integrity() {
      let integrityCheckStmt =
        DBConn.createStatement("PRAGMA integrity_check");
      log.push("INTEGRITY");
      let logIndex = log.length;
      while (integrityCheckStmt.executeStep()) {
        log.push(integrityCheckStmt.getString(0));
      }
      integrityCheckStmt.finalize();
      log.push(sep);
      return log[logIndex] == "ok";
    }

    function vacuum(aVacuumCallback) {
      log.push("VACUUM");
      let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                   getService(Ci.nsIProperties);
      let placesDBFile = dirSvc.get("ProfD", Ci.nsILocalFile);
      placesDBFile.append("places.sqlite");
      log.push("places.sqlite: " + placesDBFile.fileSize + " byte");
      log.push(sep);
      let stmt = DBConn.createStatement("VACUUM");
      stmt.executeAsync({
        handleResult: function() {},
        handleError: function() {
          Cu.reportError("Maintenance VACUUM failed");
        },
        handleCompletion: function(aReason) {
          aVacuumCallback();
        }
      });
      stmt.finalize();
    }

    function backup() {
      log.push("BACKUP");
      let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                   getService(Ci.nsIProperties);
      let profD = dirSvc.get("ProfD", Ci.nsILocalFile);
      let placesDBFile = profD.clone();
      placesDBFile.append("places.sqlite");
      let backupDBFile = profD.clone();
      backupDBFile.append("places.sqlite.corrupt");
      backupDBFile.createUnique(backupDBFile.NORMAL_FILE_TYPE, 0666);
      let backupName = backupDBFile.leafName;
      backupDBFile.remove(false);
      placesDBFile.copyTo(profD, backupName);
      log.push(backupName);
      log.push(sep);
    }

    function reindex() {
      log.push("REINDEX");
      DBConn.executeSimpleSQL("REINDEX");
      log.push(sep);
    }

    function cleanup() {
      log.push("CLEANUP");
      self.maintenanceOnIdle()
      log.push(sep);
    }

    function expire() {
      log.push("EXPIRE");
      // Get expiration component.  This will also ensure it has already been
      // initialized.
      let expiration = Cc["@mozilla.org/places/expiration;1"].
                       getService(Ci.nsIObserver);
      // Get maximum number of unique URIs.
      let prefs = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefBranch);
      let limitURIs = prefs.getIntPref(
        "places.history.expiration.transient_current_max_pages");
      if (limitURIs >= 0)
        log.push("Current unique URIs limit: " + limitURIs);
      // Force a full expiration step.
      expiration.observe(null, "places-debug-start-expiration",
                         -1 /* expire all expirable entries */);
      log.push(sep);
    }

    function stats() {
      log.push("STATS");
      let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                   getService(Ci.nsIProperties);
      let placesDBFile = dirSvc.get("ProfD", Ci.nsILocalFile);
      placesDBFile.append("places.sqlite");
      log.push("places.sqlite: " + placesDBFile.fileSize + " byte");
      let stmt = DBConn.createStatement(
        "SELECT name FROM sqlite_master WHERE type = :DBType");
      stmt.params["DBType"] = "table";
      while (stmt.executeStep()) {
        let tableName = stmt.getString(0);
        let countStmt = DBConn.createStatement(
        "SELECT count(*) FROM " + tableName);
        countStmt.executeStep();
        log.push(tableName + ": " + countStmt.getInt32(0));
        countStmt.finalize();
      }
      stmt.finalize();
      log.push(sep);
    }

    // First of all execute an integrity check.
    let integrityIsGood = integrity();

    // If integrity check did fail, we can try to fix the database through
    // a reindex.
    if (!integrityIsGood) {
      // Backup current database.
      backup();
      // Execute a reindex.
      reindex();
      // Now check again the integrity.
      integrityIsGood = integrity();
    }

    // If integrity is fine, let's force a maintenance, execute a vacuum and
    // get some stats.
    if (integrityIsGood) {
      cleanup();
      expire();
      vacuum(function () {
        stats();
        if (aLogCallback) {
          aLogCallback(log);
        }
        else {
          try {
            let console = Cc["@mozilla.org/consoleservice;1"].
                          getService(Ci.nsIConsoleService);
            console.logStringMessage(log.join('\n'));
          }
          catch(ex) {}
        }
      });
    }
  }

};

__defineGetter__("PlacesDBUtils", function() {
  delete this.PlacesDBUtils;
  return this.PlacesDBUtils = new nsPlacesDBUtils;
});
