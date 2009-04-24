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

let EXPORTED_SYMBOLS = [ "PlacesDBUtils" ];

////////////////////////////////////////////////////////////////////////////////
//// Constants

const IS_CONTRACTID = "@mozilla.org/widget/idleservice;1";
const OS_CONTRACTID = "@mozilla.org/observer-service;1";
const HS_CONTRACTID = "@mozilla.org/browser/nav-history-service;1";
const BS_CONTRACTID = "@mozilla.org/browser/nav-bookmarks-service;1";
const TS_CONTRACTID = "@mozilla.org/timer;1";
const SB_CONTRACTID = "@mozilla.org/intl/stringbundle;1";
const TIM_CONTRACTID = "@mozilla.org/updates/timer-manager;1";

const PLACES_STRING_BUNDLE_URI = "chrome://places/locale/places.properties";

const FINISHED_MAINTENANCE_NOTIFICATION_TOPIC = "places-maintenance-finished";

// Do maintenance after 10 minutes of idle.
// We choose a small idle time because we must not prevent laptops from going
// to standby, also we don't want to hit cpu/disk while the user is doing other
// activities on the computer, like watching a movie.
// So, we suppose that after 10 idle minutes the user is moving to another task
// and we can hit without big troubles.
const IDLE_TIMEOUT = 10 * 60 * 1000;

// Check for idle every 10 minutes and do maintenance if the user has been idle
// for more than IDLE_TIMEOUT.
const IDLE_LOOKUP_REPEAT = 10 * 60 * 1000;

// These are the seconds between each maintenance (24h).
const MAINTENANCE_REPEAT =  24 * 60 * 60;

////////////////////////////////////////////////////////////////////////////////
//// nsPlacesDBUtils class

function nsPlacesDBUtils() {
  //////////////////////////////////////////////////////////////////////////////
  //// Smart getters

  this.__defineGetter__("_bms", function() {
    delete this._bms;
    return this._bms = Cc[BS_CONTRACTID].getService(Ci.nsINavBookmarksService);
  });

  this.__defineGetter__("_hs", function() {
    delete this._hs;
    return this._hs = Cc[HS_CONTRACTID].getService(Ci.nsINavHistoryService);
  });

  this.__defineGetter__("_os", function() {
    delete this._os;
    return this._os = Cc[OS_CONTRACTID].getService(Ci.nsIObserverService);
  });

  this.__defineGetter__("_idlesvc", function() {
    delete this._idlesvc;
    return this._idlesvc = Cc[IS_CONTRACTID].getService(Ci.nsIIdleService);
  });

  this.__defineGetter__("_dbConn", function() {
    delete this._dbConn;
    return this._dbConn = Cc[HS_CONTRACTID].
                          getService(Ci.nsPIPlacesDatabase).DBConnection;
  });

  this.__defineGetter__("_bundle", function() {
    delete this._bundle;
    return this._bundle = Cc[SB_CONTRACTID].
                          getService(Ci.nsIStringBundleService).
                          createBundle(PLACES_STRING_BUNDLE_URI);
  });

  // register the maintenance timer
  try {
    let tim = Cc[TIM_CONTRACTID].getService(Ci.nsIUpdateTimerManager);
    tim.registerTimer("places-maintenance-timer", this, MAINTENANCE_REPEAT);
  } catch (ex) {
    // The timer manager is not available in xpc shell tests
  }
}

nsPlacesDBUtils.prototype = {
  _idleLookupTimer: null,
  _statementsRunningCount: 0,

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsITimerCallback,
    Ci.nsIObserver,
  ]),

  //////////////////////////////////////////////////////////////////////////////
  //// nsITimerCallback

  notify: function PDBU_notify(aTimer) {
    switch (aTimer) {
      case this._idleLookUpTimer:
        let idleTime = 0;
        try {
          idleTime = this._idlesvc.idleTime;
        } catch (ex) {}

        // do maintenance on idle
        if (idleTime > IDLE_TIMEOUT) {
          // Stop the timer, we do maintenance once per day
          this._idleLookUpTimer.cancel();
          this._idleLookUpTimer = null;

          // start the cleanup
          this.maintenanceOnIdle();
        }
        break;
      default:
        // Start the idle lookup timer
        this._idleLookUpTimer = Cc[TS_CONTRACTID].createInstance(Ci.nsITimer);
        this._idleLookUpTimer.initWithCallback(this, IDLE_LOOKUP_REPEAT,
                                            Ci.nsITimer.TYPE_REPEATING_SLACK);
        break;
    }
  },

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
    this._hs.runInBatchMode({runBatched: function(aUserData){}}, null);
    this._bms.runInBatchMode({runBatched: function(aUserData){}}, null);
    // Notify observers that maintenance tasks are complete
    this._os.notifyObservers(null, FINISHED_MAINTENANCE_NOTIFICATION_TOPIC, null);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Tasks

  maintenanceOnIdle: function PDBU_maintenanceOnIdle() {
    // bug 431558: preventive maintenance for Places database
    let cleanupStatements = [];

    // MOZ_ANNO_ATTRIBUTES
    // A.1 remove unused attributes
    let deleteUnusedAnnoAttributes = this._dbConn.createStatement(
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
    let deleteInvalidAttributeAnnos = this._dbConn.createStatement(
      "DELETE FROM moz_annos WHERE id IN ( " +
        "SELECT id FROM moz_annos a " +
        "WHERE NOT EXISTS " +
          "(SELECT id FROM moz_anno_attributes " +
            "WHERE id = a.anno_attribute_id LIMIT 1) " +
      ")");
    cleanupStatements.push(deleteInvalidAttributeAnnos);

    // B.2 remove orphan annos
    let deleteOrphanAnnos = this._dbConn.createStatement(
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
    let selectPlacesRoot = this._dbConn.createStatement(
      "SELECT id FROM moz_bookmarks WHERE id = :places_root");
    selectPlacesRoot.params["places_root"] = this._bms.placesRoot;
    if (!selectPlacesRoot.executeStep()) {
      // We are missing the root, try to recreate it.
      let createPlacesRoot = this._dbConn.createStatement(
        "INSERT INTO moz_bookmarks (id, type, fk, parent, position, title) " +
        "VALUES (:places_root, 2, NULL, 0, 0, :title)");
      createPlacesRoot.params["places_root"] = this._bms.placesRoot;
      createPlacesRoot.params["title"] = "";
      cleanupStatements.push(createPlacesRoot);

      // Now ensure that other roots are children of Places root.
      let fixPlacesRootChildren = this._dbConn.createStatement(
        "UPDATE moz_bookmarks SET parent = :places_root WHERE id IN " +
          "(SELECT folder_id FROM moz_bookmarks_roots " +
            "WHERE folder_id <> :places_root)");
      fixPlacesRootChildren.params["places_root"] = this._bms.placesRoot;
      cleanupStatements.push(fixPlacesRootChildren);
    }
    selectPlacesRoot.finalize();

    // C.2 fix roots titles
    //     some alpha version has wrong roots title, and this also fixes them if
    //     locale has changed.
    let updateRootTitleSql = "UPDATE moz_bookmarks SET title = :title " +
                             "WHERE id = :root_id AND title <> :title";
    // root
    let fixPlacesRootTitle = this._dbConn.createStatement(updateRootTitleSql);
    fixPlacesRootTitle.params["root_id"] = this._bms.placesRoot;
    fixPlacesRootTitle.params["title"] = "";
    cleanupStatements.push(fixPlacesRootTitle);
    // bookmarks menu
    let fixBookmarksMenuTitle = this._dbConn.createStatement(updateRootTitleSql);
    fixBookmarksMenuTitle.params["root_id"] = this._bms.bookmarksMenuFolder;
    fixBookmarksMenuTitle.params["title"] =
      this._bundle.GetStringFromName("BookmarksMenuFolderTitle");
    cleanupStatements.push(fixBookmarksMenuTitle);
    // bookmarks toolbar
    let fixBookmarksToolbarTitle = this._dbConn.createStatement(updateRootTitleSql);
    fixBookmarksToolbarTitle.params["root_id"] = this._bms.toolbarFolder;
    fixBookmarksToolbarTitle.params["title"] =
      this._bundle.GetStringFromName("BookmarksToolbarFolderTitle");
    cleanupStatements.push(fixBookmarksToolbarTitle);
    // unsorted bookmarks
    let fixUnsortedBookmarksTitle = this._dbConn.createStatement(updateRootTitleSql);
    fixUnsortedBookmarksTitle.params["root_id"] = this._bms.unfiledBookmarksFolder;
    fixUnsortedBookmarksTitle.params["title"] =
      this._bundle.GetStringFromName("UnsortedBookmarksFolderTitle");
    cleanupStatements.push(fixUnsortedBookmarksTitle);
    // tags
    let fixTagsRootTitle = this._dbConn.createStatement(updateRootTitleSql);
    fixTagsRootTitle.params["root_id"] = this._bms.tagsFolder;
    fixTagsRootTitle.params["title"] =
      this._bundle.GetStringFromName("TagsFolderTitle");
    cleanupStatements.push(fixTagsRootTitle);

    // MOZ_BOOKMARKS
    // D.1 remove items without a valid place
    // if fk IS NULL we fix them in D.7
    let deleteNoPlaceItems = this._dbConn.createStatement(
      "DELETE FROM moz_bookmarks WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " + // skip roots
      ") AND id IN (" +
        "SELECT b.id FROM moz_bookmarks b " +
        "WHERE fk NOT NULL AND b.type = :bookmark_type " +
          "AND NOT EXISTS (SELECT url FROM moz_places_temp WHERE id = b.fk LIMIT 1) " +
          "AND NOT EXISTS (SELECT url FROM moz_places WHERE id = b.fk LIMIT 1) " +
      ")");
    deleteNoPlaceItems.params["bookmark_type"] = this._bms.TYPE_BOOKMARK;
    cleanupStatements.push(deleteNoPlaceItems);

    // D.2 remove items that are not uri bookmarks from tag containers
    let deleteBogusTagChildren = this._dbConn.createStatement(
      "DELETE FROM moz_bookmarks WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " + // skip roots
      ") AND id IN (" +
        "SELECT b.id FROM moz_bookmarks b " +
        "WHERE b.parent IN " +
          "(SELECT id FROM moz_bookmarks WHERE parent = :tags_folder) " +
          "AND b.type <> :bookmark_type " +
      ")");
    deleteBogusTagChildren.params["tags_folder"] = this._bms.tagsFolder;
    deleteBogusTagChildren.params["bookmark_type"] = this._bms.TYPE_BOOKMARK;
    cleanupStatements.push(deleteBogusTagChildren);

    // D.3 remove empty tags
    let deleteEmptyTags = this._dbConn.createStatement(
      "DELETE FROM moz_bookmarks WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " + // skip roots
      ") AND id IN (" +
        "SELECT b.id FROM moz_bookmarks b " +
        "WHERE b.id IN " +
          "(SELECT id FROM moz_bookmarks WHERE parent = :tags_folder) " +
          "AND NOT EXISTS " +
            "(SELECT id from moz_bookmarks WHERE parent = b.id LIMIT 1) " +
      ")");
    deleteEmptyTags.params["tags_folder"] = this._bms.tagsFolder;
    cleanupStatements.push(deleteEmptyTags);

    // D.4 move orphan items to unsorted folder
    let fixOrphanItems = this._dbConn.createStatement(
      "UPDATE moz_bookmarks SET parent = :unsorted_folder WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " +  // skip roots
      ") AND id IN (" +
        "SELECT b.id FROM moz_bookmarks b " +
        "WHERE b.parent <> 0 " + // exclude Places root
        "AND NOT EXISTS " +
          "(SELECT id FROM moz_bookmarks WHERE id = b.parent LIMIT 1) " +
      ")");
    fixOrphanItems.params["unsorted_folder"] = this._bms.unfiledBookmarksFolder;
    cleanupStatements.push(fixOrphanItems);

    // D.5 fix wrong keywords
    let fixInvalidKeywords = this._dbConn.createStatement(
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
    let fixBookmarksAsFolders = this._dbConn.createStatement(
      "UPDATE moz_bookmarks SET type = :bookmark_type WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " + // skip roots
      ") AND id IN ( " +
        "SELECT id FROM moz_bookmarks b " +
        "WHERE type IN (:folder_type, :separator_type, :dynamic_type) " +
          "AND fk NOTNULL " +
      ")");
    fixBookmarksAsFolders.params["bookmark_type"] = this._bms.TYPE_BOOKMARK;
    fixBookmarksAsFolders.params["folder_type"] = this._bms.TYPE_FOLDER;
    fixBookmarksAsFolders.params["separator_type"] = this._bms.TYPE_SEPARATOR;
    fixBookmarksAsFolders.params["dynamic_type"] = this._bms.TYPE_DYNAMIC_CONTAINER;
    cleanupStatements.push(fixBookmarksAsFolders);

    // D.7 fix wrong item types
    //     Bookmarks should have an fk, if they don't have any, convert them to
    //     folders.
    let fixFoldersAsBookmarks = this._dbConn.createStatement(
      "UPDATE moz_bookmarks SET type = :folder_type WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " + // skip roots
      ") AND id IN ( " +
        "SELECT id FROM moz_bookmarks b " +
        "WHERE type = :bookmark_type " +
          "AND fk IS NULL " +
      ")");
    fixFoldersAsBookmarks.params["bookmark_type"] = this._bms.TYPE_BOOKMARK;
    fixFoldersAsBookmarks.params["folder_type"] = this._bms.TYPE_FOLDER;
    cleanupStatements.push(fixFoldersAsBookmarks);

    // D.8 fix wrong item types
    //     Dynamic containers should have a folder_type, if they don't have any
    //     convert them to folders.
    let fixFoldersAsDynamic = this._dbConn.createStatement(
      "UPDATE moz_bookmarks SET type = :folder_type WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " + // skip roots
      ") AND id IN ( " +
        "SELECT id FROM moz_bookmarks b " +
        "WHERE type = :dynamic_type " +
          "AND folder_type IS NULL " +
      ")");
    fixFoldersAsDynamic.params["dynamic_type"] = this._bms.TYPE_DYNAMIC_CONTAINER;
    fixFoldersAsDynamic.params["folder_type"] = this._bms.TYPE_FOLDER;
    cleanupStatements.push(fixFoldersAsDynamic);

    // D.9 fix wrong parents
    //     Items cannot have dynamic containers, separators or other bookmarks
    //     as parent, if they have bad parent move them to unsorted bookmarks.
    let fixInvalidParents = this._dbConn.createStatement(
      "UPDATE moz_bookmarks SET parent = :unsorted_folder WHERE id NOT IN ( " +
        "SELECT folder_id FROM moz_bookmarks_roots " +  // skip roots
      ") AND id IN ( " +
        "SELECT id FROM moz_bookmarks b " +
        "WHERE EXISTS " +
          "(SELECT id FROM moz_bookmarks WHERE id = b.parent " +
            "AND type IN (:bookmark_type, :separator_type, :dynamic_type) " +
            "LIMIT 1) " +
      ")");
    fixInvalidParents.params["unsorted_folder"] = this._bms.unfiledBookmarksFolder;
    fixInvalidParents.params["bookmark_type"] = this._bms.TYPE_BOOKMARK;
    fixInvalidParents.params["separator_type"] = this._bms.TYPE_SEPARATOR;
    fixInvalidParents.params["dynamic_type"] = this._bms.TYPE_DYNAMIC_CONTAINER;
    cleanupStatements.push(fixInvalidParents);

/* XXX needs test
    // D.10 recalculate positions
    //      This requires multiple related statements.
    //      We can detect a folder with bad position values comparing the sum of
    //      all position values with the triangular numbers obtained by the number
    //      of children: (n * (n + 1) / 2). Starting from 0 is (n * (n - 1) / 2).
    let detectWrongPositionsParents = this._dbConn.createStatement(
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
      let fixPositionsForParent = this._dbConn.createStatement(
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
    let removeLivemarkStaticItems = this._dbConn.createStatement(
      "DELETE FROM moz_bookmarks WHERE type = :bookmark_type AND fk IN ( " +
        "SELECT id FROM moz_places WHERE url = :lmloading OR url = :lmfailed " +
      ")");
    removeLivemarkStaticItems.params["bookmark_type"] = this._bms.TYPE_BOOKMARK;
    removeLivemarkStaticItems.params["lmloading"] = "about:livemark-loading";
    removeLivemarkStaticItems.params["lmfailed"] = "about:livemark-failed";
    cleanupStatements.push(removeLivemarkStaticItems);

    // MOZ_FAVICONS
    // E.1 remove orphan icons
    let deleteOrphanIcons = this._dbConn.createStatement(
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
    let deleteOrphanVisits = this._dbConn.createStatement(
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
    let deleteOrphanInputHistory = this._dbConn.createStatement(
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
    let deleteInvalidAttributeItemsAnnos = this._dbConn.createStatement(
      "DELETE FROM moz_items_annos WHERE id IN ( " +
        "SELECT id FROM moz_items_annos t " +
        "WHERE NOT EXISTS " +
          "(SELECT id FROM moz_anno_attributes " +
            "WHERE id = t.anno_attribute_id LIMIT 1) " +
      ")");
    cleanupStatements.push(deleteInvalidAttributeItemsAnnos);

    // H.2 remove orphan item annos
    let deleteOrphanItemsAnnos = this._dbConn.createStatement(
      "DELETE FROM moz_items_annos WHERE id IN ( " +
        "SELECT id FROM moz_items_annos t " +
        "WHERE NOT EXISTS " +
          "(SELECT id FROM moz_bookmarks WHERE id = t.item_id LIMIT 1) " +
      ")");
    cleanupStatements.push(deleteOrphanItemsAnnos);

    // MOZ_KEYWORDS
    // I.1 remove unused keywords
    let deleteUnusedKeywords = this._dbConn.createStatement(
      "DELETE FROM moz_keywords WHERE id IN ( " +
        "SELECT id FROM moz_keywords k " +
        "WHERE NOT EXISTS " +
          "(SELECT id FROM moz_bookmarks WHERE keyword_id = k.id LIMIT 1) " +
      ")");
    cleanupStatements.push(deleteUnusedKeywords);

    // MOZ_PLACES
    // L.1 fix wrong favicon ids
    let fixInvalidFaviconIds = this._dbConn.createStatement(
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
    let detectWrongCountPlaces = this._dbConn.createStatement(
      "SELECT id FROM moz_places h " +
      "WHERE id NOT IN (SELECT id FROM moz_places_temp) " +
        "AND h.visit_count <> " +
          "(SELECT count(*) FROM moz_historyvisits " +
            "WHERE place_id = h.id AND visit_type NOT IN (0,4,7))");
    while (detectWrongCountPlaces.executeStep()) {
      let placeId = detectWrongCountPlaces.getInt64(0);

      let fixCountForPlace = this._dbConn.createStatement(
        "UPDATE moz_places_view SET visit_count = ( " +
          "(SELECT count(*) FROM moz_historyvisits " +
            "WHERE place_id = :place_id AND visit_type NOT IN (0,4,7)) + " +
          "(SELECT count(*) FROM moz_historyvisits_temp " +
            "WHERE place_id = :place_id AND visit_type NOT IN (0,4,7)) + " +
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
};

__defineGetter__("PlacesDBUtils", function() {
  delete this.PlacesDBUtils;
  return this.PlacesDBUtils = new nsPlacesDBUtils;
});
