/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module provides an asynchronous API for managing bookmarks.
 *
 * Bookmarks are organized in a tree structure, and include URLs, folders and
 * separators.  Multiple bookmarks for the same URL are allowed.
 *
 * Note that if you are handling bookmarks operations in the UI, you should
 * not use this API directly, but rather use PlacesTransactions.jsm, so that
 * any operation is undo/redo-able.
 *
 * Each bookmark-item is represented by an object having the following
 * properties:
 *
 *  - guid (string)
 *      The globally unique identifier of the item.
 *  - parentGuid (string)
 *      The globally unique identifier of the folder containing the item.
 *      This will be an empty string for the Places root folder.
 *  - index (number)
 *      The 0-based position of the item in the parent folder.
 *  - dateAdded (Date)
 *      The time at which the item was added.
 *  - lastModified (Date)
 *      The time at which the item was last modified.
 *  - type (number)
 *      The item's type, either TYPE_BOOKMARK, TYPE_FOLDER or TYPE_SEPARATOR.
 *
 *  The following properties are only valid for URLs or folders.
 *
 *  - title (string)
 *      The item's title, if any.  Empty titles and null titles are considered
 *      the same. Titles longer than DB_TITLE_LENGTH_MAX will be truncated.
 *
 *  The following properties are only valid for URLs:
 *
 *  - url (URL, href or nsIURI)
 *      The item's URL.  Note that while input objects can contains either
 *      an URL object, an href string, or an nsIURI, output objects will always
 *      contain an URL object.
 *      An URL cannot be longer than DB_URL_LENGTH_MAX, methods will throw if a
 *      longer value is provided.
 *
 * Each successful operation notifies through the nsINavBookmarksObserver
 * interface.  To listen to such notifications you must register using
 * nsINavBookmarksService addObserver and removeObserver methods.
 * Note that bookmark addition or order changes won't notify onItemMoved for
 * items that have their indexes changed.
 * Similarly, lastModified changes not done explicitly (like changing another
 * property) won't fire an onItemChanged notification for the lastModified
 * property.
 * @see nsINavBookmarkObserver
 */

this.EXPORTED_SYMBOLS = [ "Bookmarks" ];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.importGlobalProperties(["URL"]);

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
                                  "resource://gre/modules/Sqlite.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesSyncUtils",
                                  "resource://gre/modules/PlacesSyncUtils.jsm");

// This is an helper to temporarily cover the need to know the tags folder
// itemId until bug 424160 is fixed.  This exists so that startup paths won't
// pay the price to initialize the bookmarks service just to fetch this value.
// If the method is already initing the bookmarks service for other reasons
// (most of the writing methods will invoke getObservers() already) it can
// directly use the PlacesUtils.tagsFolderId property.
var gTagsFolderId;
async function promiseTagsFolderId() {
  if (gTagsFolderId)
    return gTagsFolderId;
  let db =  await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(
    "SELECT id FROM moz_bookmarks WHERE guid = :guid",
    { guid: Bookmarks.tagsGuid }
  );
  return gTagsFolderId = rows[0].getResultByName("id");
}

const MATCH_ANYWHERE_UNMODIFIED = Ci.mozIPlacesAutoComplete.MATCH_ANYWHERE_UNMODIFIED;
const BEHAVIOR_BOOKMARK = Ci.mozIPlacesAutoComplete.BEHAVIOR_BOOKMARK;
const SQLITE_MAX_VARIABLE_NUMBER = 999;

var Bookmarks = Object.freeze({
  /**
   * Item's type constants.
   * These should stay consistent with nsINavBookmarksService.idl
   */
  TYPE_BOOKMARK: 1,
  TYPE_FOLDER: 2,
  TYPE_SEPARATOR: 3,

  /**
   * Sync status constants, stored for each item.
   */
  SYNC_STATUS: {
    UNKNOWN: Ci.nsINavBookmarksService.SYNC_STATUS_UNKNOWN,
    NEW: Ci.nsINavBookmarksService.SYNC_STATUS_NEW,
    NORMAL: Ci.nsINavBookmarksService.SYNC_STATUS_NORMAL,
  },

  /**
   * Default index used to append a bookmark-item at the end of a folder.
   * This should stay consistent with nsINavBookmarksService.idl
   */
  DEFAULT_INDEX: -1,

  /**
   * Bookmark change source constants, passed as optional properties and
   * forwarded to observers. See nsINavBookmarksService.idl for an explanation.
   */
  SOURCES: {
    DEFAULT: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
    SYNC: Ci.nsINavBookmarksService.SOURCE_SYNC,
    IMPORT: Ci.nsINavBookmarksService.SOURCE_IMPORT,
    IMPORT_REPLACE: Ci.nsINavBookmarksService.SOURCE_IMPORT_REPLACE,
    SYNC_REPARENT_REMOVED_FOLDER_CHILDREN: Ci.nsINavBookmarksService.SOURCE_SYNC_REPARENT_REMOVED_FOLDER_CHILDREN,
  },

  /**
   * Special GUIDs associated with bookmark roots.
   * It's guaranteed that the roots will always have these guids.
   */

   rootGuid:    "root________",
   menuGuid:    "menu________",
   toolbarGuid: "toolbar_____",
   unfiledGuid: "unfiled_____",
   mobileGuid:  "mobile______",

   // With bug 424160, tags will stop being bookmarks, thus this root will
   // be removed.  Do not rely on this, rather use the tagging service API.
   tagsGuid:    "tags________",

   /**
    * The GUIDs of the user content root folders that we support, for easy access
    * as a set.
    */
   userContentRoots: ["toolbar_____", "menu________", "unfiled_____", "mobile______"],

  /**
   * Inserts a bookmark-item into the bookmarks tree.
   *
   * For creating a bookmark, the following set of properties is required:
   *  - type
   *  - parentGuid
   *  - url, only for bookmarked URLs
   *
   * If an index is not specified, it defaults to appending.
   * It's also possible to pass a non-existent GUID to force creation of an
   * item with the given GUID, but unless you have a very sound reason, such as
   * an undo manager implementation or synchronization, don't do that.
   *
   * Note that any known properties that don't apply to the specific item type
   * cause an exception.
   *
   * @param info
   *        object representing a bookmark-item.
   *
   * @return {Promise} resolved when the creation is complete.
   * @resolves to an object representing the created bookmark.
   * @rejects if it's not possible to create the requested bookmark.
   * @throws if the arguments are invalid.
   */
  insert(info) {
    let now = new Date();
    let addedTime = (info && info.dateAdded) || now;
    let modTime = addedTime;
    if (addedTime > now) {
      modTime = now;
    }
    let insertInfo = validateBookmarkObject("Bookmarks.jsm: insert", info,
      { type: { defaultValue: this.TYPE_BOOKMARK },
        index: { defaultValue: this.DEFAULT_INDEX },
        url: { requiredIf: b => b.type == this.TYPE_BOOKMARK,
               validIf: b => b.type == this.TYPE_BOOKMARK },
        parentGuid: { required: true },
        title: { defaultValue: "",
                 validIf: b => b.type == this.TYPE_BOOKMARK ||
                               b.type == this.TYPE_FOLDER ||
                               b.title === "" },
        dateAdded: { defaultValue: addedTime },
        lastModified: { defaultValue: modTime,
                        validIf: b => b.lastModified >= now || (b.dateAdded && b.lastModified >= b.dateAdded) },
        source: { defaultValue: this.SOURCES.DEFAULT }
      });

    return (async () => {
      // Ensure the parent exists.
      let parent = await fetchBookmark({ guid: insertInfo.parentGuid });
      if (!parent)
        throw new Error("parentGuid must be valid");

      // Set index in the appending case.
      if (insertInfo.index == this.DEFAULT_INDEX ||
          insertInfo.index > parent._childCount) {
        insertInfo.index = parent._childCount;
      }

      let item = await insertBookmark(insertInfo, parent);

      // Notify onItemAdded to listeners.
      let observers = PlacesUtils.bookmarks.getObservers();
      // We need the itemId to notify, though once the switch to guids is
      // complete we may stop using it.
      let uri = item.hasOwnProperty("url") ? PlacesUtils.toURI(item.url) : null;
      let itemId = await PlacesUtils.promiseItemId(item.guid);

      // Pass tagging information for the observers to skip over these notifications when needed.
      let isTagging = parent._parentId == PlacesUtils.tagsFolderId;
      let isTagsFolder = parent._id == PlacesUtils.tagsFolderId;
      notify(observers, "onItemAdded", [ itemId, parent._id, item.index,
                                         item.type, uri, item.title,
                                         PlacesUtils.toPRTime(item.dateAdded), item.guid,
                                         item.parentGuid, item.source ],
                                       { isTagging: isTagging || isTagsFolder });

      // If it's a tag, notify OnItemChanged to all bookmarks for this URL.
      if (isTagging) {
        for (let entry of (await fetchBookmarksByURL(item, true))) {
          notify(observers, "onItemChanged", [ entry._id, "tags", false, "",
                                               PlacesUtils.toPRTime(entry.lastModified),
                                               entry.type, entry._parentId,
                                               entry.guid, entry.parentGuid,
                                               "", item.source ]);
        }
      }

      // Remove non-enumerable properties.
      delete item.source;
      return Object.assign({}, item);
    })();
  },


  /**
   * Inserts a bookmark-tree into the existing bookmarks tree.
   *
   * All the specified folders and bookmarks will be inserted as new, even
   * if duplicates. There's no merge support at this time.
   *
   * The input should be of the form:
   * {
   *   guid: "<some-existing-guid-to-use-as-parent>",
   *   source: "<some valid source>", (optional)
   *   children: [
   *     ... valid bookmark objects.
   *   ]
   * }
   *
   * Children will be appended to any existing children of the parent
   * that is specified. The source specified on the root of the tree
   * will be used for all the items inserted. Any indices or custom parentGuids
   * set on children will be ignored and overwritten.
   *
   * @param {Object} tree
   *        object representing a tree of bookmark items to insert.
   *
   * @return {Promise} resolved when the creation is complete.
   * @resolves to an object representing the created bookmark.
   * @rejects if it's not possible to create the requested bookmark.
   * @throws if the arguments are invalid.
   */
  insertTree(tree) {
    if (!tree || typeof tree != "object") {
      throw new Error("Should be provided a valid tree object.");
    }

    if (!Array.isArray(tree.children) || !tree.children.length) {
      throw new Error("Should have a non-zero number of children to insert.");
    }

    if (!PlacesUtils.isValidGuid(tree.guid)) {
      throw new Error(`The parent guid is not valid (${tree.guid} ${tree.title}).`);
    }

    if (tree.guid == this.rootGuid) {
      throw new Error("Can't insert into the root.");
    }

    if (tree.guid == this.tagsGuid) {
      throw new Error("Can't use insertTree to insert tags.");
    }

    if (tree.hasOwnProperty("source") &&
        !Object.values(this.SOURCES).includes(tree.source)) {
      throw new Error("Can't use source value " + tree.source);
    }

    // Serialize the tree into an array of items to insert into the db.
    let insertInfos = [];
    let insertLivemarkInfos = [];
    let urlsThatMightNeedPlaces = [];

    // We want to use the same 'last added' time for all the entries
    // we import (so they won't differ by a few ms based on where
    // they are in the tree, and so we don't needlessly construct
    // multiple dates).
    let fallbackLastAdded = new Date();

    const {TYPE_BOOKMARK, TYPE_FOLDER, SOURCES} = this;

    // Reuse the 'source' property for all the entries.
    let source = tree.source || SOURCES.DEFAULT;

    function appendInsertionInfoForInfoArray(infos, indexToUse, parentGuid) {
      // We want to keep the index of items that will be inserted into the root
      // NULL, and then use a subquery to select the right index, to avoid
      // races where other consumers might add items between when we determine
      // the index and when we insert. However, the validator does not allow
      // NULL values in in the index, so we fake it while validating and then
      // correct later. Keep track of whether we're doing this:
      let shouldUseNullIndices = false;
      if (indexToUse === null) {
        shouldUseNullIndices = true;
        indexToUse = 0;
      }

      // When a folder gets an item added, its last modified date is updated
      // to be equal to the date we added the item (if that date is newer).
      // Because we're inserting a tree, we keep track of this date for the
      // loop, updating it for inserted items as well as from any subfolders
      // we insert.
      let lastAddedForParent = new Date(0);
      for (let info of infos) {
        // Add a guid if none is present.
        if (!info.hasOwnProperty("guid")) {
          info.guid = PlacesUtils.history.makeGuid();
        }
        // Set the correct parent guid.
        info.parentGuid = parentGuid;
        // Ensure to use the same date for dateAdded and lastModified, even if
        // dateAdded may be imposed by the caller.
        let time = (info && info.dateAdded) || fallbackLastAdded;
        let insertInfo = validateBookmarkObject("Bookmarks.jsm: insertTree",
                                                info, {
          type: { defaultValue: TYPE_BOOKMARK },
          url: { requiredIf: b => b.type == TYPE_BOOKMARK,
                 validIf: b => b.type == TYPE_BOOKMARK },
          parentGuid: { required: true },
          title: { defaultValue: "",
                   validIf: b => b.type == TYPE_BOOKMARK ||
                                 b.type == TYPE_FOLDER ||
                                 b.title === "" },
          dateAdded: { defaultValue: time,
                       validIf: b => !b.lastModified ||
                                      b.dateAdded <= b.lastModified },
          lastModified: { defaultValue: time,
                          validIf: b => (!b.dateAdded && b.lastModified >= time) ||
                                        (b.dateAdded && b.lastModified >= b.dateAdded) },
          index: { replaceWith: indexToUse++ },
          source: { replaceWith: source },
          annos: {},
          keyword: { validIf: b => b.type == TYPE_BOOKMARK },
          charset: { validIf: b => b.type == TYPE_BOOKMARK },
          postData: { validIf: b => b.type == TYPE_BOOKMARK },
          tags: { validIf: b => b.type == TYPE_BOOKMARK },
          children: { validIf: b => b.type == TYPE_FOLDER && Array.isArray(b.children) }
        });

        if (shouldUseNullIndices) {
          insertInfo.index = null;
        }
        // Store the URL if this is a bookmark, so we can ensure we create an
        // entry in moz_places for it.
        if (insertInfo.type == Bookmarks.TYPE_BOOKMARK) {
          urlsThatMightNeedPlaces.push(insertInfo.url);
        }

        // As we don't track indexes for children of root folders, and we
        // insert livemarks separately, we create a temporary placeholder in
        // the bookmarks, and later we'll replace it by the real livemark.
        if (isLivemark(insertInfo)) {
          // Make the current insertInfo item a placeholder.
          let livemarkInfo = Object.assign({}, insertInfo);

          // Delete the annotations that make it a livemark.
          delete insertInfo.annos;

          // Now save the livemark info for later.
          insertLivemarkInfos.push(livemarkInfo);
        }

        insertInfos.push(insertInfo);
        // Process any children. We have to use info.children here rather than
        // insertInfo.children because validateBookmarkObject doesn't copy over
        // the children ref, as the default bookmark validators object doesn't
        // know about children.
        if (info.children) {
          // start children of this item off at index 0.
          let childrenLastAdded = appendInsertionInfoForInfoArray(info.children, 0, insertInfo.guid);
          if (childrenLastAdded > insertInfo.lastModified) {
            insertInfo.lastModified = childrenLastAdded;
          }
          if (childrenLastAdded > lastAddedForParent) {
            lastAddedForParent = childrenLastAdded;
          }
        }

        // Ensure we track what time to update the parent to.
        if (insertInfo.dateAdded > lastAddedForParent) {
          lastAddedForParent = insertInfo.dateAdded;
        }
      }
      return lastAddedForParent;
    }

    // We want to validate synchronously, but we can't know the index at which
    // we're inserting into the parent. We just use NULL instead,
    // and the SQL query with which we insert will update it as necessary.
    let lastAddedForParent = appendInsertionInfoForInfoArray(tree.children, null, tree.guid);

    return (async function() {
      let treeParent = await fetchBookmark({ guid: tree.guid });
      if (!treeParent) {
        throw new Error("The parent you specified doesn't exist.");
      }

      if (treeParent._parentId == PlacesUtils.tagsFolderId) {
        throw new Error("Can't use insertTree to insert tags.");
      }

      await insertBookmarkTree(insertInfos, source, treeParent,
                               urlsThatMightNeedPlaces, lastAddedForParent);

      await insertLivemarkData(insertLivemarkInfos);

      // Now update the indices of root items in the objects we return.
      // These may be wrong if someone else modified the table between
      // when we fetched the parent and inserted our items, but the actual
      // inserts will have been correct, and we don't want to query the DB
      // again if we don't have to. bug 1347230 covers improving this.
      let rootIndex = treeParent._childCount;
      for (let insertInfo of insertInfos) {
        if (insertInfo.parentGuid == tree.guid) {
          insertInfo.index += rootIndex++;
        }
      }
      // We need the itemIds to notify, though once the switch to guids is
      // complete we may stop using them.
      let itemIdMap = await PlacesUtils.promiseManyItemIds(insertInfos.map(info => info.guid));
      // Notify onItemAdded to listeners.
      let observers = PlacesUtils.bookmarks.getObservers();
      for (let i = 0; i < insertInfos.length; i++) {
        let item = insertInfos[i];
        let itemId = itemIdMap.get(item.guid);
        let uri = item.hasOwnProperty("url") ? PlacesUtils.toURI(item.url) : null;
        // For sub-folders, we need to make sure their children have the correct parent ids.
        let parentId;
        if (item.parentGuid === treeParent.guid) {
          // This is a direct child of the tree parent, so we can use the
          // existing parent's id.
          parentId = treeParent._id;
        } else {
          // This is a parent folder that's been updated, so we need to
          // use the new item id.
          parentId = itemIdMap.get(item.parentGuid);
        }

        notify(observers, "onItemAdded", [ itemId, parentId, item.index,
                                           item.type, uri, item.title,
                                           PlacesUtils.toPRTime(item.dateAdded), item.guid,
                                           item.parentGuid, item.source ],
                                         { isTagging: false });
        // Note, annotations for livemark data are deleted from insertInfo
        // within appendInsertionInfoForInfoArray, so we won't be duplicating
        // the insertions here.
        await handleBookmarkItemSpecialData(itemId, item);

        // Remove non-enumerable properties.
        delete item.source;

        insertInfos[i] = Object.assign({}, item);
      }
      return insertInfos;
    })();
  },

  /**
   * Updates a bookmark-item.
   *
   * Only set the properties which should be changed (undefined properties
   * won't be taken into account).
   * Moreover, the item's type or dateAdded cannot be changed, since they are
   * immutable after creation.  Trying to change them will reject.
   *
   * Note that any known properties that don't apply to the specific item type
   * cause an exception.
   *
   * @param info
   *        object representing a bookmark-item, as defined above.
   *
   * @return {Promise} resolved when the update is complete.
   * @resolves to an object representing the updated bookmark.
   * @rejects if it's not possible to update the given bookmark.
   * @throws if the arguments are invalid.
   */
  update(info) {
    // The info object is first validated here to ensure it's consistent, then
    // it's compared to the existing item to remove any properties that don't
    // need to be updated.
    let updateInfo = validateBookmarkObject("Bookmarks.jsm: update", info,
      { guid: { required: true },
        index: { requiredIf: b => b.hasOwnProperty("parentGuid"),
                 validIf: b => b.index >= 0 || b.index == this.DEFAULT_INDEX },
        source: { defaultValue: this.SOURCES.DEFAULT }
      });

    // There should be at last one more property in addition to guid and source.
    if (Object.keys(updateInfo).length < 3)
      throw new Error("Not enough properties to update");

    return (async () => {
      // Ensure the item exists.
      let item = await fetchBookmark(updateInfo);
      if (!item)
        throw new Error("No bookmarks found for the provided GUID");
      if (updateInfo.hasOwnProperty("type") && updateInfo.type != item.type)
        throw new Error("The bookmark type cannot be changed");

      // Remove any property that will stay the same.
      removeSameValueProperties(updateInfo, item);
      // Check if anything should still be updated.
      if (Object.keys(updateInfo).length < 3) {
        // Remove non-enumerable properties.
        return Object.assign({}, item);
      }
      const now = new Date();
      let lastModifiedDefault = now;
      // In the case where `dateAdded` is specified, but `lastModified` is not,
      // we only update `lastModified` if it is older than the new `dateAdded`.
      if (!("lastModified" in updateInfo) &&
          "dateAdded" in updateInfo) {
        lastModifiedDefault = new Date(Math.max(item.lastModified, updateInfo.dateAdded));
      }
      updateInfo = validateBookmarkObject("Bookmarks.jsm: update", updateInfo,
        { url: { validIf: () => item.type == this.TYPE_BOOKMARK },
          title: { validIf: () => [ this.TYPE_BOOKMARK,
                                    this.TYPE_FOLDER ].includes(item.type) },
          lastModified: { defaultValue: lastModifiedDefault,
                          validIf: b => b.lastModified >= now ||
                                        b.lastModified >= (b.dateAdded || item.dateAdded) },
          dateAdded: { defaultValue: item.dateAdded }
        });

      return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: update",
        async db => {
        let parent;
        if (updateInfo.hasOwnProperty("parentGuid")) {
          if (item.type == this.TYPE_FOLDER) {
            // Make sure we are not moving a folder into itself or one of its
            // descendants.
            let rows = await db.executeCached(
              `WITH RECURSIVE
               descendants(did) AS (
                 VALUES(:id)
                 UNION ALL
                 SELECT id FROM moz_bookmarks
                 JOIN descendants ON parent = did
                 WHERE type = :type
               )
               SELECT guid FROM moz_bookmarks
               WHERE id IN descendants
              `, { id: item._id, type: this.TYPE_FOLDER });
            if (rows.map(r => r.getResultByName("guid")).includes(updateInfo.parentGuid))
              throw new Error("Cannot insert a folder into itself or one of its descendants");
          }

          parent = await fetchBookmark({ guid: updateInfo.parentGuid });
          if (!parent)
            throw new Error("No bookmarks found for the provided parentGuid");
        }

        if (updateInfo.hasOwnProperty("index")) {
          // If at this point we don't have a parent yet, we are moving into
          // the same container.  Thus we know it exists.
          if (!parent)
            parent = await fetchBookmark({ guid: item.parentGuid });

          if (updateInfo.index >= parent._childCount ||
              updateInfo.index == this.DEFAULT_INDEX) {
             updateInfo.index = parent._childCount;

            // Fix the index when moving within the same container.
            if (parent.guid == item.parentGuid)
               updateInfo.index--;
          }
        }

        let updatedItem = await updateBookmark(updateInfo, item, parent);

        if (item.type == this.TYPE_BOOKMARK &&
            item.url.href != updatedItem.url.href) {
          // ...though we don't wait for the calculation.
          updateFrecency(db, [item.url]).catch(Cu.reportError);
          updateFrecency(db, [updatedItem.url]).catch(Cu.reportError);
        }

        // Notify onItemChanged to listeners.
        let observers = PlacesUtils.bookmarks.getObservers();
        // For lastModified, we only care about the original input, since we
        // should not notify implciit lastModified changes.
        if (info.hasOwnProperty("lastModified") &&
            updateInfo.hasOwnProperty("lastModified") &&
            item.lastModified != updatedItem.lastModified) {
          notify(observers, "onItemChanged", [ updatedItem._id, "lastModified",
                                               false,
                                               `${PlacesUtils.toPRTime(updatedItem.lastModified)}`,
                                               PlacesUtils.toPRTime(updatedItem.lastModified),
                                               updatedItem.type,
                                               updatedItem._parentId,
                                               updatedItem.guid,
                                               updatedItem.parentGuid, "",
                                               updatedItem.source ]);
        }
        if (info.hasOwnProperty("dateAdded") &&
            updateInfo.hasOwnProperty("dateAdded") &&
            item.dateAdded != updatedItem.dateAdded) {
          notify(observers, "onItemChanged", [ updatedItem._id, "dateAdded",
                                               false, `${PlacesUtils.toPRTime(updatedItem.dateAdded)}`,
                                               PlacesUtils.toPRTime(updatedItem.lastModified),
                                               updatedItem.type,
                                               updatedItem._parentId,
                                               updatedItem.guid,
                                               updatedItem.parentGuid,
                                               "",
                                               updatedItem.source ]);
        }
        if (updateInfo.hasOwnProperty("title")) {
          let isTagging = updatedItem.parentGuid == Bookmarks.tagsGuid;
          notify(observers, "onItemChanged", [ updatedItem._id, "title",
                                               false, updatedItem.title,
                                               PlacesUtils.toPRTime(updatedItem.lastModified),
                                               updatedItem.type,
                                               updatedItem._parentId,
                                               updatedItem.guid,
                                               updatedItem.parentGuid, "",
                                               updatedItem.source ],
                                               { isTagging });
          // If we're updating a tag, we must notify all the tagged bookmarks
          // about the change.
          if (isTagging) {
            let URIs = PlacesUtils.tagging.getURIsForTag(updatedItem.title);
            for (let uri of URIs) {
              for (let entry of (await fetchBookmarksByURL({ url: new URL(uri.spec) }, true))) {
                notify(observers, "onItemChanged", [ entry._id, "tags", false, "",
                                                     PlacesUtils.toPRTime(entry.lastModified),
                                                     entry.type, entry._parentId,
                                                     entry.guid, entry.parentGuid,
                                                     "", updatedItem.source ]);
              }
            }
          }
        }
        if (updateInfo.hasOwnProperty("url")) {
          notify(observers, "onItemChanged", [ updatedItem._id, "uri",
                                               false, updatedItem.url.href,
                                               PlacesUtils.toPRTime(updatedItem.lastModified),
                                               updatedItem.type,
                                               updatedItem._parentId,
                                               updatedItem.guid,
                                               updatedItem.parentGuid,
                                               item.url.href,
                                               updatedItem.source ]);
        }
        // If the item was moved, notify onItemMoved.
        if (item.parentGuid != updatedItem.parentGuid ||
            item.index != updatedItem.index) {
          notify(observers, "onItemMoved", [ updatedItem._id, item._parentId,
                                             item.index, updatedItem._parentId,
                                             updatedItem.index, updatedItem.type,
                                             updatedItem.guid, item.parentGuid,
                                             updatedItem.parentGuid,
                                             updatedItem.source ]);
        }

        // Remove non-enumerable properties.
        delete updatedItem.source;
        return Object.assign({}, updatedItem);
      });
    })();
  },

  /**
   * Removes a bookmark-item.
   *
   * @param guidOrInfo
   *        The globally unique identifier of the item to remove, or an
   *        object representing it, as defined above.
   * @param {Object} [options={}]
   *        Additional options that can be passed to the function.
   *        Currently supports the following properties:
   *         - preventRemovalOfNonEmptyFolders: Causes an exception to be
   *           thrown when attempting to remove a folder that is not empty.
   *         - source: The change source, forwarded to all bookmark observers.
   *           Defaults to nsINavBookmarksService::SOURCE_DEFAULT.
   *
   * @return {Promise} resolved when the removal is complete.
   * @resolves to an object representing the removed bookmark.
   * @rejects if the provided guid doesn't match any existing bookmark.
   * @throws if the arguments are invalid.
   */
  remove(guidOrInfo, options = {}) {
    let info = guidOrInfo;
    if (!info)
      throw new Error("Input should be a valid object");
    if (typeof(guidOrInfo) != "object")
      info = { guid: guidOrInfo };

    // Disallow removing the root folders.
    if ([this.rootGuid, this.menuGuid, this.toolbarGuid, this.unfiledGuid,
         this.tagsGuid, this.mobileGuid].includes(info.guid)) {
      throw new Error("It's not possible to remove Places root folders.");
    }

    if (!("source" in options)) {
      options.source = Bookmarks.SOURCES.DEFAULT;
    }

    // Even if we ignore any other unneeded property, we still validate any
    // known property to reduce likelihood of hidden bugs.
    let removeInfo = validateBookmarkObject("Bookmarks.jsm: remove", info);

    return (async function() {
      let item = await fetchBookmark(removeInfo);
      if (!item)
        throw new Error("No bookmarks found for the provided GUID.");

      item = await removeBookmark(item, options);

      // Notify onItemRemoved to listeners.
      let observers = PlacesUtils.bookmarks.getObservers();
      let uri = item.hasOwnProperty("url") ? PlacesUtils.toURI(item.url) : null;
      let isUntagging = item._grandParentId == PlacesUtils.tagsFolderId;
      notify(observers, "onItemRemoved", [ item._id, item._parentId, item.index,
                                           item.type, uri, item.guid,
                                           item.parentGuid,
                                           options.source ],
                                         { isTagging: isUntagging });

      if (isUntagging) {
        for (let entry of (await fetchBookmarksByURL(item, true))) {
          notify(observers, "onItemChanged", [ entry._id, "tags", false, "",
                                               PlacesUtils.toPRTime(entry.lastModified),
                                               entry.type, entry._parentId,
                                               entry.guid, entry.parentGuid,
                                               "", options.source ]);
        }
      }

      // Remove non-enumerable properties.
      return Object.assign({}, item);
    })();
  },

  /**
   * Removes ALL bookmarks, resetting the bookmarks storage to an empty tree.
   *
   * Note that roots are preserved, only their children will be removed.
   *
   * @param {Object} [options={}]
   *        Additional options. Currently supports the following properties:
   *         - source: The change source, forwarded to all bookmark observers.
   *           Defaults to nsINavBookmarksService::SOURCE_DEFAULT.
   *
   * @return {Promise} resolved when the removal is complete.
   * @resolves once the removal is complete.
   */
  eraseEverything(options = {}) {
    if (!options.source) {
      options.source = Bookmarks.SOURCES.DEFAULT;
    }

    return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: eraseEverything",
      async function(db) {
        let urls;

        await db.executeTransaction(async function() {
          urls = await removeFoldersContents(db, Bookmarks.userContentRoots, options);
          const time = PlacesUtils.toPRTime(new Date());
          const syncChangeDelta =
            PlacesSyncUtils.bookmarks.determineSyncChangeDelta(options.source);
          for (let folderGuid of Bookmarks.userContentRoots) {
            await db.executeCached(
              `UPDATE moz_bookmarks SET lastModified = :time,
                                        syncChangeCounter = syncChangeCounter + :syncChangeDelta
               WHERE id IN (SELECT id FROM moz_bookmarks WHERE guid = :folderGuid )
              `, { folderGuid, time, syncChangeDelta });
          }
        });

        // We don't wait for the frecency calculation.
        if (urls && urls.length) {
          updateFrecency(db, urls, true).catch(Cu.reportError);
        }
      }
    );
  },

  /**
   * Returns a list of recently bookmarked items.
   * Only includes actual bookmarks. Excludes folders, separators and queries.
   *
   * @param {integer} numberOfItems
   *        The maximum number of bookmark items to return.
   *
   * @return {Promise} resolved when the listing is complete.
   * @resolves to an array of recent bookmark-items.
   * @rejects if an error happens while querying.
   */
  getRecent(numberOfItems) {
    if (numberOfItems === undefined) {
      throw new Error("numberOfItems argument is required");
    }
    if (!typeof numberOfItems === "number" || (numberOfItems % 1) !== 0) {
      throw new Error("numberOfItems argument must be an integer");
    }
    if (numberOfItems <= 0) {
      throw new Error("numberOfItems argument must be greater than zero");
    }

    return fetchRecentBookmarks(numberOfItems);
  },

  /**
   * Fetches information about a bookmark-item.
   *
   * REMARK: any successful call to this method resolves to a single
   *         bookmark-item (or null), even when multiple bookmarks may exist
   *         (e.g. fetching by url).  If you wish to retrieve all of the
   *         bookmarks for a given match, use the callback instead.
   *
   * Input can be either a guid or an object with one, and only one, of these
   * filtering properties set:
   *  - guid
   *      retrieves the item with the specified guid.
   *  - parentGuid and index
   *      retrieves the item by its position.
   *  - url
   *      retrieves the most recent bookmark having the given URL.
   *      To retrieve ALL of the bookmarks for that URL, you must pass in an
   *      onResult callback, that will be invoked once for each found bookmark.
   *
   * @param guidOrInfo
   *        The globally unique identifier of the item to fetch, or an
   *        object representing it, as defined above.
   * @param onResult [optional]
   *        Callback invoked for each found bookmark.
   * @param options [optional]
   *        an optional object whose properties describe options for the fetch:
   *         - concurrent: fetches concurrently to any writes, returning results
   *                       faster. On the negative side, it may return stale
   *                       information missing the currently ongoing write.
   *
   * @return {Promise} resolved when the fetch is complete.
   * @resolves to an object representing the found item, as described above, or
   *           an array of such objects.  if no item is found, the returned
   *           promise is resolved to null.
   * @rejects if an error happens while fetching.
   * @throws if the arguments are invalid.
   *
   * @note Any unknown property in the info object is ignored.  Known properties
   *       may be overwritten.
   */
  fetch(guidOrInfo, onResult = null, options = {}) {
    if (!("concurrent" in options)) {
      options.concurrent = false;
    }
    if (onResult && typeof onResult != "function")
      throw new Error("onResult callback must be a valid function");
    let info = guidOrInfo;
    if (!info)
      throw new Error("Input should be a valid object");
    if (typeof(info) != "object") {
      info = { guid: guidOrInfo };
    } else if (Object.keys(info).length == 1) {
      // Just a faster code path.
      if (!["url", "guid", "parentGuid", "index"].includes(Object.keys(info)[0]))
        throw new Error(`Unexpected number of conditions provided: 0`);
    } else {
      // Only one condition at a time can be provided.
      let conditionsCount = [
        v => v.hasOwnProperty("guid"),
        v => v.hasOwnProperty("parentGuid") && v.hasOwnProperty("index"),
        v => v.hasOwnProperty("url")
      ].reduce((old, fn) => old + fn(info) | 0, 0);
      if (conditionsCount != 1)
        throw new Error(`Unexpected number of conditions provided: ${conditionsCount}`);
    }

    let behavior = {};
    if (info.hasOwnProperty("parentGuid") || info.hasOwnProperty("index")) {
      behavior = {
        parentGuid: { requiredIf: b => b.hasOwnProperty("index") },
        index: { requiredIf: b => b.hasOwnProperty("parentGuid"),
                 validIf: b => typeof(b.index) == "number" &&
                               b.index >= 0 || b.index == this.DEFAULT_INDEX }
      };
    }

    // Even if we ignore any other unneeded property, we still validate any
    // known property to reduce likelihood of hidden bugs.
    let fetchInfo = validateBookmarkObject("Bookmarks.jsm: fetch", info,
                                           behavior);

    return (async function() {
      let results;
      if (fetchInfo.hasOwnProperty("url"))
        results = await fetchBookmarksByURL(fetchInfo, options && options.concurrent);
      else if (fetchInfo.hasOwnProperty("guid"))
        results = await fetchBookmark(fetchInfo, options && options.concurrent);
      else if (fetchInfo.hasOwnProperty("parentGuid") && fetchInfo.hasOwnProperty("index"))
        results = await fetchBookmarkByPosition(fetchInfo, options && options.concurrent);

      if (!results)
        return null;

      if (!Array.isArray(results))
        results = [results];
      // Remove non-enumerable properties.
      results = results.map(r => Object.assign({}, r));

      // Ideally this should handle an incremental behavior and thus be invoked
      // while we fetch.  Though, the likelihood of 2 or more bookmarks for the
      // same match is very low, so it's not worth the added code complication.
      if (onResult) {
        for (let result of results) {
          try {
            onResult(result);
          } catch (ex) {
            Cu.reportError(ex);
          }
        }
      }

      return results[0];
    })();
  },

  /**
   * Retrieves an object representation of a bookmark-item, along with all of
   * its descendants, if any.
   *
   * Each node in the tree is an object that extends the item representation
   * described above with some additional properties:
   *
   *  - [deprecated] id (number)
   *      the item's id.  Defined only if aOptions.includeItemIds is set.
   *  - annos (array)
   *      the item's annotations.  This is not set if there are no annotations
   *      set for the item.
   *
   * The root object of the tree also has the following properties set:
   *  - itemsCount (number, not enumerable)
   *      the number of items, including the root item itself, which are
   *      represented in the resolved object.
   *
   * Bookmarked URLs may also have the following properties:
   *  - tags (string)
   *      csv string of the bookmark's tags, if any.
   *  - charset (string)
   *      the last known charset of the bookmark, if any.
   *  - iconurl (URL)
   *      the bookmark's favicon URL, if any.
   *
   * Folders may also have the following properties:
   *  - children (array)
   *      the folder's children information, each of them having the same set of
   *      properties as above.
   *
   * @param [optional] guid
   *        the topmost item to be queried.  If it's not passed, the Places
   *        root folder is queried: that is, you get a representation of the
   *        entire bookmarks hierarchy.
   * @param [optional] options
   *        Options for customizing the query behavior, in the form of an
   *        object with any of the following properties:
   *         - excludeItemsCallback: a function for excluding items, along with
   *           their descendants.  Given an item object (that has everything set
   *           apart its potential children data), it should return true if the
   *           item should be excluded.  Once an item is excluded, the function
   *           isn't called for any of its descendants.  This isn't called for
   *           the root item.
   *           WARNING: since the function may be called for each item, using
   *           this option can slow down the process significantly if the
   *           callback does anything that's not relatively trivial.  It is
   *           highly recommended to avoid any synchronous I/O or DB queries.
   *         - includeItemIds: opt-in to include the deprecated id property.
   *           Use it if you must. It'll be removed once the switch to guids is
   *           complete.
   *
   * @return {Promise} resolved when the fetch is complete.
   * @resolves to an object that represents either a single item or a
   *           bookmarks tree.  if guid points to a non-existent item, the
   *           returned promise is resolved to null.
   * @rejects if an error happens while fetching.
   * @throws if the arguments are invalid.
   */
  // TODO must implement these methods yet:
  // PlacesUtils.promiseBookmarksTree()
  fetchTree(guid = "", options = {}) {
    throw new Error("Not yet implemented");
  },

  /**
   * Reorders contents of a folder based on a provided array of GUIDs.
   *
   * @param parentGuid
   *        The globally unique identifier of the folder whose contents should
   *        be reordered.
   * @param orderedChildrenGuids
   *        Ordered array of the children's GUIDs.  If this list contains
   *        non-existing entries they will be ignored.  If the list is
   *        incomplete, and the current child list is already in order with
   *        respect to orderedChildrenGuids, no change is made. Otherwise, the
   *        new items are appended but maintain their current order relative to
   *        eachother.
   * @param {Object} [options={}]
   *        Additional options. Currently supports the following properties:
   *         - source: The change source, forwarded to all bookmark observers.
   *           Defaults to nsINavBookmarksService::SOURCE_DEFAULT.
   *
   * @return {Promise} resolved when reordering is complete.
   * @rejects if an error happens while reordering.
   * @throws if the arguments are invalid.
   */
  reorder(parentGuid, orderedChildrenGuids, options = {}) {
    let info = { guid: parentGuid };
    info = validateBookmarkObject("Bookmarks.jsm: reorder", info,
                                  { guid: { required: true } });

    if (!Array.isArray(orderedChildrenGuids) || !orderedChildrenGuids.length)
      throw new Error("Must provide a sorted array of children GUIDs.");
    try {
      orderedChildrenGuids.forEach(PlacesUtils.BOOKMARK_VALIDATORS.guid);
    } catch (ex) {
      throw new Error("Invalid GUID found in the sorted children array.");
    }

    if (!("source" in options)) {
      options.source = Bookmarks.SOURCES.DEFAULT;
    }

    return (async () => {
      let parent = await fetchBookmark(info);
      if (!parent || parent.type != this.TYPE_FOLDER)
        throw new Error("No folder found for the provided GUID.");

      let sortedChildren = await reorderChildren(parent, orderedChildrenGuids,
                                                 options);

      let { source = Bookmarks.SOURCES.DEFAULT } = options;
      let observers = PlacesUtils.bookmarks.getObservers();
      // Note that child.index is the old index.
      for (let i = 0; i < sortedChildren.length; ++i) {
        let child = sortedChildren[i];
        notify(observers, "onItemMoved", [ child._id, child._parentId,
                                           child.index, child._parentId,
                                           i, child.type,
                                           child.guid, child.parentGuid,
                                           child.parentGuid,
                                           source ]);
      }
    })();
  },

  /**
   * Searches a list of bookmark-items by a search term, url or title.
   *
   * IMPORTANT:
   * This is intended as an interim API for the web-extensions implementation.
   * It will be removed as soon as we have a new querying API.
   *
   * Note also that this used to exclude separators but no longer does so.
   *
   * If you just want to search bookmarks by URL, use .fetch() instead.
   *
   * @param query
   *        Either a string to use as search term, or an object
   *        containing any of these keys: query, title or url with the
   *        corresponding string to match as value.
   *        The url property can be either a string or an nsIURI.
   *
   * @return {Promise} resolved when the search is complete.
   * @resolves to an array of found bookmark-items.
   * @rejects if an error happens while searching.
   * @throws if the arguments are invalid.
   *
   * @note Any unknown property in the query object is ignored.
   *       Known properties may be overwritten.
   */
  search(query) {
    if (!query) {
      throw new Error("Query object is required");
    }
    if (typeof query === "string") {
      query = { query };
    }
    if (typeof query !== "object") {
      throw new Error("Query must be an object or a string");
    }
    if (query.query && typeof query.query !== "string") {
      throw new Error("Query option must be a string");
    }
    if (query.title && typeof query.title !== "string") {
      throw new Error("Title option must be a string");
    }

    if (query.url) {
      if (typeof query.url === "string" || (query.url instanceof URL)) {
        query.url = new URL(query.url).href;
      } else if (query.url instanceof Ci.nsIURI) {
        query.url = query.url.spec;
      } else {
        throw new Error("Url option must be a string or a URL object");
      }
    }

    return queryBookmarks(query);
  },
});

// Globals.

/**
 * Sends a bookmarks notification through the given observers.
 *
 * @param {Array} observers
 *        array of nsINavBookmarkObserver objects.
 * @param {String} notification
 *        the notification name.
 * @param {Array} [args]
 *        array of arguments to pass to the notification.
 * @param {Object} [information]
 *        Information about the notification, so we can filter based
 *        based on the observer's preferences.
 */
function notify(observers, notification, args = [], information = {}) {
  for (let observer of observers) {
    if (information.isTagging && observer.skipTags) {
      continue;
    }

    if (information.isDescendantRemoval && observer.skipDescendantsOnItemRemoval &&
        !(PlacesUtils.bookmarks.userContentRoots.includes(information.parentGuid))) {
      continue;
    }

    try {
      observer[notification](...args);
    } catch (ex) {}
  }
}

// Update implementation.

function updateBookmark(info, item, newParent) {
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: updateBookmark",
    async function(db) {

    let tuples = new Map();
    tuples.set("lastModified", { value: PlacesUtils.toPRTime(info.lastModified) });
    if (info.hasOwnProperty("title"))
      tuples.set("title", { value: info.title,
                            fragment: `title = NULLIF(:title, "")` });
    if (info.hasOwnProperty("dateAdded"))
      tuples.set("dateAdded", { value: PlacesUtils.toPRTime(info.dateAdded) });

    await db.executeTransaction(async function() {
      let isTagging = item._grandParentId == PlacesUtils.tagsFolderId;
      let syncChangeDelta =
        PlacesSyncUtils.bookmarks.determineSyncChangeDelta(info.source);

      if (info.hasOwnProperty("url")) {
        // Ensure a page exists in moz_places for this URL.
        await maybeInsertPlace(db, info.url);
        // Update tuples for the update query.
        tuples.set("url", { value: info.url.href,
                            fragment: "fk = (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url)" });
      }

      let newIndex = info.hasOwnProperty("index") ? info.index : item.index;
      if (newParent) {
        // For simplicity, update the index regardless.
        tuples.set("position", { value: newIndex });

        if (newParent.guid == item.parentGuid) {
          // Moving inside the original container.
          // When moving "up", add 1 to each index in the interval.
          // Otherwise when moving down, we subtract 1.
          // Only the parent needs a sync change, which is handled in
          // `setAncestorsLastModified`.
          let sign = newIndex < item.index ? +1 : -1;
          await db.executeCached(
            `UPDATE moz_bookmarks SET position = position + :sign
             WHERE parent = :newParentId
               AND position BETWEEN :lowIndex AND :highIndex
            `, { sign, newParentId: newParent._id,
                 lowIndex: Math.min(item.index, newIndex),
                 highIndex: Math.max(item.index, newIndex) });
        } else {
          // Moving across different containers. In this case, both parents and
          // the child need sync changes. `setAncestorsLastModified` handles the
          // parents; the `needsSyncChange` check below handles the child.
          tuples.set("parent", { value: newParent._id} );
          await db.executeCached(
            `UPDATE moz_bookmarks SET position = position + :sign
             WHERE parent = :oldParentId
               AND position >= :oldIndex
            `, { sign: -1, oldParentId: item._parentId, oldIndex: item.index });
          await db.executeCached(
            `UPDATE moz_bookmarks SET position = position + :sign
             WHERE parent = :newParentId
               AND position >= :newIndex
            `, { sign: +1, newParentId: newParent._id, newIndex });

          await setAncestorsLastModified(db, item.parentGuid, info.lastModified,
                                         syncChangeDelta);
        }
        await setAncestorsLastModified(db, newParent.guid, info.lastModified,
                                       syncChangeDelta);
      }

      if (syncChangeDelta) {
        // Sync stores child indices in the parent's record, so we only bump the
        // item's counter if we're updating at least one more property in
        // addition to the index, last modified time, and dateAdded.
        let sizeThreshold = 1;
        if (info.hasOwnProperty("index") && info.index != item.index) {
          ++sizeThreshold;
        }
        if (tuples.has("dateAdded")) {
          ++sizeThreshold;
        }
        let needsSyncChange = tuples.size > sizeThreshold;
        if (needsSyncChange) {
          tuples.set("syncChangeDelta", { value: syncChangeDelta,
                                          fragment: "syncChangeCounter = syncChangeCounter + :syncChangeDelta" });
        }
      }

      if (isTagging) {
        // If we're updating a tag entry, bump the sync change counter for
        // bookmarks with the tagged URL.
        await PlacesSyncUtils.bookmarks.addSyncChangesForBookmarksWithURL(
          db, item.url, syncChangeDelta);
        if (info.hasOwnProperty("url")) {
          // Changing the URL of a tag entry is equivalent to untagging the
          // old URL and tagging the new one, so we bump the change counter
          // for the new URL here.
          await PlacesSyncUtils.bookmarks.addSyncChangesForBookmarksWithURL(
            db, info.url, syncChangeDelta);
        }
      }

      let isChangingTagFolder = item._parentId == PlacesUtils.tagsFolderId;
      if (isChangingTagFolder) {
        // If we're updating a tag folder (for example, changing a tag's title),
        // bump the change counter for all tagged bookmarks.
        await addSyncChangesForBookmarksInFolder(db, item, syncChangeDelta);
      }

      await db.executeCached(
        `UPDATE moz_bookmarks
         SET ${Array.from(tuples.keys()).map(v => tuples.get(v).fragment || `${v} = :${v}`).join(", ")}
         WHERE guid = :guid
        `, Object.assign({ guid: info.guid },
                         [...tuples.entries()].reduce((p, c) => { p[c[0]] = c[1].value; return p; }, {})));

      if (newParent) {
        if (newParent.guid == item.parentGuid) {
          // Mark all affected separators as changed
          // Also bumps the change counter if the item itself is a separator
          const startIndex = Math.min(newIndex, item.index);
          await adjustSeparatorsSyncCounter(db, newParent._id, startIndex, syncChangeDelta);
        } else {
          // Mark all affected separators as changed
          await adjustSeparatorsSyncCounter(db, item._parentId, item.index, syncChangeDelta);
          await adjustSeparatorsSyncCounter(db, newParent._id, newIndex, syncChangeDelta);
        }
        // Remove the Sync orphan annotation from reparented items. We don't
        // notify annotation observers about this because this is a temporary,
        // internal anno that's only used by Sync.
        await db.executeCached(
          `DELETE FROM moz_items_annos
           WHERE anno_attribute_id = (SELECT id FROM moz_anno_attributes
                                      WHERE name = :orphanAnno) AND
                 item_id = :id`,
          { orphanAnno: PlacesSyncUtils.bookmarks.SYNC_PARENT_ANNO,
            id: item._id });
      }
    });

    // If the parent changed, update related non-enumerable properties.
    let additionalParentInfo = {};
    if (newParent) {
      Object.defineProperty(additionalParentInfo, "_parentId",
                            { value: newParent._id, enumerable: false });
      Object.defineProperty(additionalParentInfo, "_grandParentId",
                            { value: newParent._parentId, enumerable: false });
    }

    let updatedItem = mergeIntoNewObject(item, info, additionalParentInfo);

    return updatedItem;
  });
}

// Insert implementation.

function insertBookmark(item, parent) {
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: insertBookmark",
    async function(db) {

    // If a guid was not provided, generate one, so we won't need to fetch the
    // bookmark just after having created it.
    let hasExistingGuid = item.hasOwnProperty("guid");
    if (!hasExistingGuid)
      item.guid = PlacesUtils.history.makeGuid();

    let isTagging = parent._parentId == PlacesUtils.tagsFolderId;

    await db.executeTransaction(async function transaction() {
      if (item.type == Bookmarks.TYPE_BOOKMARK) {
        // Ensure a page exists in moz_places for this URL.
        // The IGNORE conflict can trigger on `guid`.
        await maybeInsertPlace(db, item.url);
      }

      // Adjust indices.
      await db.executeCached(
        `UPDATE moz_bookmarks SET position = position + 1
         WHERE parent = :parent
         AND position >= :index
        `, { parent: parent._id, index: item.index });

      let syncChangeDelta =
        PlacesSyncUtils.bookmarks.determineSyncChangeDelta(item.source);
      let syncStatus =
        PlacesSyncUtils.bookmarks.determineInitialSyncStatus(item.source);

      // Insert the bookmark into the database.
      await db.executeCached(
        `INSERT INTO moz_bookmarks (fk, type, parent, position, title,
                                    dateAdded, lastModified, guid,
                                    syncChangeCounter, syncStatus)
         VALUES (CASE WHEN :url ISNULL THEN NULL ELSE (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url) END,
                 :type, :parent, :index, NULLIF(:title, ""), :date_added,
                 :last_modified, :guid, :syncChangeCounter, :syncStatus)
        `, { url: item.hasOwnProperty("url") ? item.url.href : null,
             type: item.type, parent: parent._id, index: item.index,
             title: item.title, date_added: PlacesUtils.toPRTime(item.dateAdded),
             last_modified: PlacesUtils.toPRTime(item.lastModified), guid: item.guid,
             syncChangeCounter: syncChangeDelta, syncStatus });

      // Mark all affected separators as changed
      await adjustSeparatorsSyncCounter(db, parent._id, item.index + 1, syncChangeDelta);

      if (hasExistingGuid) {
        // Remove stale tombstones if we're reinserting an item.
        await db.executeCached(
          `DELETE FROM moz_bookmarks_deleted WHERE guid = :guid`,
          { guid: item.guid });
      }

      if (isTagging) {
        // New tag entry; bump the change counter for bookmarks with the
        // tagged URL.
        await PlacesSyncUtils.bookmarks.addSyncChangesForBookmarksWithURL(
          db, item.url, syncChangeDelta);
      }

      await setAncestorsLastModified(db, item.parentGuid, item.dateAdded,
                                     syncChangeDelta);
    });

    // If not a tag recalculate frecency...
    if (item.type == Bookmarks.TYPE_BOOKMARK && !isTagging) {
      // ...though we don't wait for the calculation.
      updateFrecency(db, [item.url]).catch(Cu.reportError);
    }

    return item;
  });
}

/**
 * Determines if a bookmark is a Livemark depending on how it is annotated.
 *
 * @param {Object} node The bookmark node to check.
 * @returns {Boolean} True if the node is a Livemark, false otherwise.
 */
function isLivemark(node) {
  return node.type == Bookmarks.TYPE_FOLDER && node.annos &&
         node.annos.some(anno => anno.name == PlacesUtils.LMANNO_FEEDURI);
}

function insertBookmarkTree(items, source, parent, urls, lastAddedForParent) {
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: insertBookmarkTree", async function(db) {
    await db.executeTransaction(async function transaction() {
      await maybeInsertManyPlaces(db, urls);

      let syncChangeDelta =
        PlacesSyncUtils.bookmarks.determineSyncChangeDelta(source);
      let syncStatus =
        PlacesSyncUtils.bookmarks.determineInitialSyncStatus(source);

      let rootId = parent._id;

      items = items.map(item => ({
        url: item.url && item.url.href,
        type: item.type, parentGuid: item.parentGuid, index: item.index,
        title: item.title, date_added: PlacesUtils.toPRTime(item.dateAdded),
        last_modified: PlacesUtils.toPRTime(item.lastModified), guid: item.guid,
        syncChangeCounter: syncChangeDelta, syncStatus, rootId
      }));
      await db.executeCached(
        `INSERT INTO moz_bookmarks (fk, type, parent, position, title,
                                    dateAdded, lastModified, guid,
                                    syncChangeCounter, syncStatus)
         VALUES (CASE WHEN :url ISNULL THEN NULL ELSE (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url) END, :type,
         (SELECT id FROM moz_bookmarks WHERE guid = :parentGuid),
         IFNULL(:index, (SELECT count(*) FROM moz_bookmarks WHERE parent = :rootId)),
         NULLIF(:title, ""), :date_added, :last_modified, :guid,
         :syncChangeCounter, :syncStatus)`, items);

      // Remove stale tombstones for new items.
      for (let chunk of chunkArray(items, SQLITE_MAX_VARIABLE_NUMBER)) {
        await db.executeCached(
          `DELETE FROM moz_bookmarks_deleted WHERE guid IN (${
            new Array(chunk.length).fill("?").join(",")})`,
          chunk.map(item => item.guid)
        );
      }

      await setAncestorsLastModified(db, parent.guid, lastAddedForParent,
                                     syncChangeDelta);
    });

    // We don't wait for the frecency calculation.
    updateFrecency(db, urls, true).catch(Cu.reportError);

    return items;
  });
}

/**
 * Handles any Livemarks within the passed items.
 *
 * @param {Array} items Livemark items that need to be added.
 */
async function insertLivemarkData(items) {
  for (let item of items) {
    let feedURI = null;
    let siteURI = null;
    item.annos = item.annos.filter(function(aAnno) {
      switch (aAnno.name) {
        case PlacesUtils.LMANNO_FEEDURI:
          feedURI = NetUtil.newURI(aAnno.value);
          return false;
        case PlacesUtils.LMANNO_SITEURI:
          siteURI = NetUtil.newURI(aAnno.value);
          return false;
        default:
          return true;
      }
    });

    let index = null;

    // Delete the placeholder but note the index of it, so that we
    // can insert the livemark item at the right place.
    let placeholder = await Bookmarks.fetch(item.guid);
    index = placeholder.index;

    await Bookmarks.remove(item.guid, {source: item.source});

    if (feedURI) {
      item.feedURI = feedURI;
      item.siteURI = siteURI;
      item.index = index;

      if (item.dateAdded) {
        item.dateAdded = PlacesUtils.toPRTime(item.dateAdded);
      }
      if (item.lastModified) {
        item.lastModified = PlacesUtils.toPRTime(item.lastModified);
      }

      let livemark = await PlacesUtils.livemarks.addLivemark(item);

      let id = livemark.id;
      if (item.annos && item.annos.length) {
        // Note: for annotations, we intentionally skip updating the last modified
        // value for the bookmark, to avoid a second update of the added bookmark.
        PlacesUtils.setAnnotationsForItem(id, item.annos,
                                          item.source, true);
      }
    }
  }
}

/**
 * Handles special data on a bookmark, e.g. annotations, keywords, tags, charsets,
 * inserting the data into the appropriate place.
 *
 * @param {Integer} itemId The ID of the item within the bookmarks database.
 * @param {Object} item The bookmark item with possible special data to be inserted.
 */
async function handleBookmarkItemSpecialData(itemId, item) {
  if (item.annos && item.annos.length) {
    // Note: for annotations, we intentionally skip updating the last modified
    // value for the bookmark, to avoid a second update of the added bookmark.
    PlacesUtils.setAnnotationsForItem(itemId, item.annos, item.source, true);
  }
  if ("keyword" in item && item.keyword) {
    // POST data could be set in 2 ways:
    // 1. new backups have a postData property
    // 2. old backups have an item annotation
    let postDataAnno = item.annos &&
                       item.annos.find(anno => anno.name == PlacesUtils.POST_DATA_ANNO);
    let postData = item.postData || (postDataAnno && postDataAnno.value);
    try {
      await PlacesUtils.keywords.insert({
        keyword: item.keyword,
        url: item.url,
        postData,
        source: item.source
      });
    } catch (ex) {
      Cu.reportError(`Failed to insert keywords: ${ex}`);
    }
  }
  if ("tags" in item) {
    try {
      PlacesUtils.tagging.tagURI(NetUtil.newURI(item.url), item.tags, item.source);
    } catch (ex) {
      // Invalid tag child, skip it.
      Cu.reportError(`Unable to set tags "${item.tags.join(", ")}" for ${item.url}: ${ex}`);
    }
  }
  if ("charset" in item && item.charset) {
    await PlacesUtils.setCharsetForURI(NetUtil.newURI(item.url), item.charset);
  }
}

// Query implementation.

async function queryBookmarks(info) {
  let queryParams = {
    tags_folder: await promiseTagsFolderId(),
  };
  // We're searching for bookmarks, so exclude tags.
  let queryString = "WHERE b.parent <> :tags_folder";
  queryString += " AND p.parent <> :tags_folder";

  if (info.title) {
    queryString += " AND b.title = :title";
    queryParams.title = info.title;
  }

  if (info.url) {
    queryString += " AND h.url_hash = hash(:url) AND h.url = :url";
    queryParams.url = info.url;
  }

  if (info.query) {
    queryString += " AND AUTOCOMPLETE_MATCH(:query, h.url, b.title, NULL, NULL, 1, 1, NULL, :matchBehavior, :searchBehavior) ";
    queryParams.query = info.query;
    queryParams.matchBehavior = MATCH_ANYWHERE_UNMODIFIED;
    queryParams.searchBehavior = BEHAVIOR_BOOKMARK;
  }

  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: queryBookmarks",
    async function(db) {
      // _id, _childCount, _grandParentId and _parentId fields
      // are required to be in the result by the converting function
      // hence setting them to NULL
      let rows = await db.executeCached(
        `SELECT b.guid, IFNULL(p.guid, "") AS parentGuid, b.position AS 'index',
                b.dateAdded, b.lastModified, b.type,
                IFNULL(b.title, "") AS title, h.url AS url, b.parent, p.parent,
                NULL AS _id,
                NULL AS _childCount,
                NULL AS _grandParentId,
                NULL AS _parentId,
                NULL AS _syncStatus
         FROM moz_bookmarks b
         LEFT JOIN moz_bookmarks p ON p.id = b.parent
         LEFT JOIN moz_places h ON h.id = b.fk
         ${queryString}
        `, queryParams);

      return rowsToItemsArray(rows);
    }
  );
}


// Fetch implementation.

async function fetchBookmark(info, concurrent) {
  let query = async function(db) {
    let rows = await db.executeCached(
      `SELECT b.guid, IFNULL(p.guid, "") AS parentGuid, b.position AS 'index',
              b.dateAdded, b.lastModified, b.type, IFNULL(b.title, "") AS title,
              h.url AS url, b.id AS _id, b.parent AS _parentId,
              (SELECT count(*) FROM moz_bookmarks WHERE parent = b.id) AS _childCount,
              p.parent AS _grandParentId, b.syncStatus AS _syncStatus
       FROM moz_bookmarks b
       LEFT JOIN moz_bookmarks p ON p.id = b.parent
       LEFT JOIN moz_places h ON h.id = b.fk
       WHERE b.guid = :guid
      `, { guid: info.guid });

    return rows.length ? rowsToItemsArray(rows)[0] : null;
  };
  if (concurrent) {
    let db = await PlacesUtils.promiseDBConnection();
    return query(db);
  }
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: fetchBookmark",
                                           query);
}

async function fetchBookmarkByPosition(info, concurrent) {
  let query = async function(db) {
    let index = info.index == Bookmarks.DEFAULT_INDEX ? null : info.index;
    let rows = await db.executeCached(
      `SELECT b.guid, IFNULL(p.guid, "") AS parentGuid, b.position AS 'index',
              b.dateAdded, b.lastModified, b.type, IFNULL(b.title, "") AS title,
              h.url AS url, b.id AS _id, b.parent AS _parentId,
              (SELECT count(*) FROM moz_bookmarks WHERE parent = b.id) AS _childCount,
              p.parent AS _grandParentId, b.syncStatus AS _syncStatus
       FROM moz_bookmarks b
       LEFT JOIN moz_bookmarks p ON p.id = b.parent
       LEFT JOIN moz_places h ON h.id = b.fk
       WHERE p.guid = :parentGuid
       AND b.position = IFNULL(:index, (SELECT count(*) - 1
                                        FROM moz_bookmarks
                                        WHERE parent = p.id))
      `, { parentGuid: info.parentGuid, index });

    return rows.length ? rowsToItemsArray(rows)[0] : null;
  };
  if (concurrent) {
    let db = await PlacesUtils.promiseDBConnection();
    return query(db);
  }
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: fetchBookmarkByPosition",
                                           query);
}

async function fetchBookmarksByURL(info, concurrent) {
  let query = async function(db) {
    let tagsFolderId = await promiseTagsFolderId();
    let rows = await db.executeCached(
      `/* do not warn (bug no): not worth to add an index */
      SELECT b.guid, IFNULL(p.guid, "") AS parentGuid, b.position AS 'index',
              b.dateAdded, b.lastModified, b.type, IFNULL(b.title, "") AS title,
              h.url AS url, b.id AS _id, b.parent AS _parentId,
              NULL AS _childCount, /* Unused for now */
              p.parent AS _grandParentId, b.syncStatus AS _syncStatus
      FROM moz_bookmarks b
      JOIN moz_bookmarks p ON p.id = b.parent
      JOIN moz_places h ON h.id = b.fk
      WHERE h.url_hash = hash(:url) AND h.url = :url
      AND _grandParentId <> :tagsFolderId
      ORDER BY b.lastModified DESC
      `, { url: info.url.href, tagsFolderId });

    return rows.length ? rowsToItemsArray(rows) : null;
  };

  if (concurrent) {
    let db = await PlacesUtils.promiseDBConnection();
    return query(db);
  }
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: fetchBookmarksByURL",
                                           query);
}

function fetchRecentBookmarks(numberOfItems) {
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: fetchRecentBookmarks",
    async function(db) {
      let tagsFolderId = await promiseTagsFolderId();
      let rows = await db.executeCached(
        `SELECT b.guid, IFNULL(p.guid, "") AS parentGuid, b.position AS 'index',
                b.dateAdded, b.lastModified, b.type,
                IFNULL(b.title, "") AS title, h.url AS url, NULL AS _id,
                NULL AS _parentId, NULL AS _childCount, NULL AS _grandParentId,
                NULL AS _syncStatus
        FROM moz_bookmarks b
        JOIN moz_bookmarks p ON p.id = b.parent
        JOIN moz_places h ON h.id = b.fk
        WHERE p.parent <> :tagsFolderId
        AND b.type = :type
        AND url_hash NOT BETWEEN hash("place", "prefix_lo")
                              AND hash("place", "prefix_hi")
        ORDER BY b.dateAdded DESC, b.ROWID DESC
        LIMIT :numberOfItems
        `, {
          tagsFolderId,
          type: Bookmarks.TYPE_BOOKMARK,
          numberOfItems,
        });

      return rows.length ? rowsToItemsArray(rows) : [];
    }
  );
}

async function fetchBookmarksByParent(db, info) {
  let rows = await db.executeCached(
    `SELECT b.guid, IFNULL(p.guid, "") AS parentGuid, b.position AS 'index',
            b.dateAdded, b.lastModified, b.type, IFNULL(b.title, "") AS title,
            h.url AS url, b.id AS _id, b.parent AS _parentId,
            (SELECT count(*) FROM moz_bookmarks WHERE parent = b.id) AS _childCount,
            p.parent AS _grandParentId, b.syncStatus AS _syncStatus
     FROM moz_bookmarks b
     LEFT JOIN moz_bookmarks p ON p.id = b.parent
     LEFT JOIN moz_places h ON h.id = b.fk
     WHERE p.guid = :parentGuid
     ORDER BY b.position ASC
    `, { parentGuid: info.parentGuid });

  return rowsToItemsArray(rows);
}

// Remove implementation.

function removeBookmark(item, options) {
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: removeBookmark",
    async function(db) {
    let urls;
    let isUntagging = item._grandParentId == PlacesUtils.tagsFolderId;

    await db.executeTransaction(async function transaction() {
      // If it's a folder, remove its contents first.
      if (item.type == Bookmarks.TYPE_FOLDER) {
        if (options.preventRemovalOfNonEmptyFolders && item._childCount > 0) {
          throw new Error("Cannot remove a non-empty folder.");
        }
        urls = await removeFoldersContents(db, [item.guid], options);
      }

      // Remove annotations first.  If it's a tag, we can avoid paying that cost.
      if (!isUntagging) {
        // We don't go through the annotations service for this cause otherwise
        // we'd get a pointless onItemChanged notification and it would also
        // set lastModified to an unexpected value.
        await removeAnnotationsForItem(db, item._id);
      }

      // Remove the bookmark from the database.
      await db.executeCached(
        `DELETE FROM moz_bookmarks WHERE guid = :guid`, { guid: item.guid });

      // Fix indices in the parent.
      await db.executeCached(
        `UPDATE moz_bookmarks SET position = position - 1 WHERE
         parent = :parentId AND position > :index
        `, { parentId: item._parentId, index: item.index });

      let syncChangeDelta =
        PlacesSyncUtils.bookmarks.determineSyncChangeDelta(options.source);

      // Mark all affected separators as changed
      await adjustSeparatorsSyncCounter(db, item._parentId, item.index, syncChangeDelta);

      if (isUntagging) {
        // If we're removing a tag entry, increment the change counter for all
        // bookmarks with the tagged URL.
        await PlacesSyncUtils.bookmarks.addSyncChangesForBookmarksWithURL(
          db, item.url, syncChangeDelta);
      }

      // Write a tombstone for the removed item.
      await insertTombstone(db, item, syncChangeDelta);

      await setAncestorsLastModified(db, item.parentGuid, new Date(),
                                     syncChangeDelta);
    });

    // If not a tag recalculate frecency...
    if (item.type == Bookmarks.TYPE_BOOKMARK && !isUntagging) {
      // ...though we don't wait for the calculation.
      updateFrecency(db, [item.url]).catch(Cu.reportError);
    }

    if (urls && urls.length && item.type == Bookmarks.TYPE_FOLDER) {
      updateFrecency(db, urls, true).catch(Cu.reportError);
    }

    return item;
  });
}

// Reorder implementation.

function reorderChildren(parent, orderedChildrenGuids, options) {
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: reorderChildren",
    db => db.executeTransaction(async function() {
      // Select all of the direct children for the given parent.
      let children = await fetchBookmarksByParent(db, { parentGuid: parent.guid });
      if (!children.length) {
        return [];
      }

      // Maps of GUIDs to indices for fast lookups in the comparator function.
      let guidIndices = new Map();
      let currentIndices = new Map();
      for (let i = 0; i < orderedChildrenGuids.length; ++i) {
        let guid = orderedChildrenGuids[i];
        guidIndices.set(guid, i);
      }

      // If we got an incomplete list but everything we have is in the right
      // order, we do nothing.
      let needReorder = true;
      let requestedChildIndices = [];
      for (let i = 0; i < children.length; ++i) {
        // Take the opportunity to build currentIndices here, since we already
        // are iterating over the children array.
        currentIndices.set(children[i].guid, i);

        if (guidIndices.has(children[i].guid)) {
          let index = guidIndices.get(children[i].guid);
          requestedChildIndices.push(index);
        }
      }

      if (requestedChildIndices.length) {
        needReorder = false;
        for (let i = 1; i < requestedChildIndices.length; ++i) {
          if (requestedChildIndices[i - 1] > requestedChildIndices[i]) {
            needReorder = true;
            break;
          }
        }
      }

      if (needReorder) {


        // Reorder the children array according to the specified order, provided
        // GUIDs come first, others are appended in somehow random order.
        children.sort((a, b) => {
          // This works provided fetchBookmarksByParent returns sorted children.
          if (!guidIndices.has(a.guid) && !guidIndices.has(b.guid)) {
            return currentIndices.get(a.guid) < currentIndices.get(b.guid) ? -1 : 1;
          }
          if (!guidIndices.has(a.guid)) {
            return 1;
          }
          if (!guidIndices.has(b.guid)) {
            return -1;
          }
          return guidIndices.get(a.guid) < guidIndices.get(b.guid) ? -1 : 1;
        });

        // Update the bookmarks position now.  If any unknown guid have been
        // inserted meanwhile, its position will be set to -position, and we'll
        // handle it later.
        // To do the update in a single step, we build a VALUES (guid, position)
        // table.  We then use count() in the sorting table to avoid skipping values
        // when no more existing GUIDs have been provided.
        let valuesTable = children.map((child, i) => `("${child.guid}", ${i})`)
                                  .join();
        await db.execute(
          `WITH sorting(g, p) AS (
             VALUES ${valuesTable}
           )
           UPDATE moz_bookmarks SET position = (
             SELECT CASE count(*) WHEN 0 THEN -position
                                         ELSE count(*) - 1
                    END
             FROM sorting a
             JOIN sorting b ON b.p <= a.p
             WHERE a.g = guid
           )
           WHERE parent = :parentId
          `, { parentId: parent._id});

        let syncChangeDelta =
          PlacesSyncUtils.bookmarks.determineSyncChangeDelta(options.source);
        if (syncChangeDelta) {
          // Flag the parent as having a change.
          await db.executeCached(`
            UPDATE moz_bookmarks SET
              syncChangeCounter = syncChangeCounter + :syncChangeDelta
            WHERE id = :parentId`,
            { parentId: parent._id, syncChangeDelta });
        }

        // Update position of items that could have been inserted in the meanwhile.
        // Since this can happen rarely and it's only done for schema coherence
        // resonds, we won't notify about these changes.
        await db.executeCached(
          `CREATE TEMP TRIGGER moz_bookmarks_reorder_trigger
             AFTER UPDATE OF position ON moz_bookmarks
             WHEN NEW.position = -1
           BEGIN
             UPDATE moz_bookmarks
             SET position = (SELECT MAX(position) FROM moz_bookmarks
                             WHERE parent = NEW.parent) +
                            (SELECT count(*) FROM moz_bookmarks
                             WHERE parent = NEW.parent
                               AND position BETWEEN OLD.position AND -1)
             WHERE guid = NEW.guid;
           END
          `);

        await db.executeCached(
          `UPDATE moz_bookmarks SET position = -1 WHERE position < 0`);

        await db.executeCached(`DROP TRIGGER moz_bookmarks_reorder_trigger`);
      }

      // Remove the Sync orphan annotation from the reordered children, so that
      // Sync doesn't try to reparent them once it sees the original parents. We
      // only do this for explicitly ordered children, to avoid removing orphan
      // annos set by Sync.
      let possibleOrphanIds = [];
      for (let child of children) {
        if (guidIndices.has(child.guid)) {
          possibleOrphanIds.push(child._id);
        }
      }
      await db.executeCached(
        `DELETE FROM moz_items_annos
         WHERE anno_attribute_id = (SELECT id FROM moz_anno_attributes
                                    WHERE name = :orphanAnno) AND
               item_id IN (${possibleOrphanIds.join(", ")})`,
        { orphanAnno: PlacesSyncUtils.bookmarks.SYNC_PARENT_ANNO });

      return children;
    })
  );
}

// Helpers.

/**
 * Merges objects into a new object, included non-enumerable properties.
 *
 * @param sources
 *        source objects to merge.
 * @return a new object including all properties from the source objects.
 */
function mergeIntoNewObject(...sources) {
  let dest = {};
  for (let src of sources) {
    for (let prop of Object.getOwnPropertyNames(src)) {
      Object.defineProperty(dest, prop, Object.getOwnPropertyDescriptor(src, prop));
    }
  }
  return dest;
}

/**
 * Remove properties that have the same value across two bookmark objects.
 *
 * @param dest
 *        destination bookmark object.
 * @param src
 *        source bookmark object.
 * @return a cleaned up bookmark object.
 * @note "guid" is never removed.
 */
function removeSameValueProperties(dest, src) {
  for (let prop in dest) {
    let remove = false;
    switch (prop) {
      case "lastModified":
      case "dateAdded":
        remove = src.hasOwnProperty(prop) && dest[prop].getTime() == src[prop].getTime();
        break;
      case "url":
        remove = src.hasOwnProperty(prop) && dest[prop].href == src[prop].href;
        break;
      default:
        remove = dest[prop] == src[prop];
    }
    if (remove && prop != "guid")
      delete dest[prop];
  }
}

/**
 * Convert an array of mozIStorageRow objects to an array of bookmark objects.
 *
 * @param rows
 *        the array of mozIStorageRow objects.
 * @return an array of bookmark objects.
 */
function rowsToItemsArray(rows) {
  return rows.map(row => {
    let item = {};
    for (let prop of ["guid", "index", "type", "title"]) {
      item[prop] = row.getResultByName(prop);
    }
    for (let prop of ["dateAdded", "lastModified"]) {
      let value = row.getResultByName(prop);
      if (value)
        item[prop] = PlacesUtils.toDate(value);
    }
    let parentGuid = row.getResultByName("parentGuid");
    if (parentGuid) {
      item.parentGuid = parentGuid;
    }
    let url = row.getResultByName("url");
    if (url) {
      item.url = new URL(url);
    }

    for (let prop of ["_id", "_parentId", "_childCount", "_grandParentId",
                      "_syncStatus"]) {
      let val = row.getResultByName(prop);
      if (val !== null) {
        // These properties should not be returned to the API consumer, thus
        // they are non-enumerable and removed through Object.assign just before
        // the object is returned.
        // Configurable is set to support mergeIntoNewObject overwrites.
        Object.defineProperty(item, prop, { value: val, enumerable: false,
                                                        configurable: true });
      }
    }

    return item;
  });
}

function validateBookmarkObject(name, input, behavior) {
  return PlacesUtils.validateItemProperties(name,
    PlacesUtils.BOOKMARK_VALIDATORS, input, behavior);
}

/**
 * Updates frecency for a list of URLs.
 *
 * @param db
 *        the Sqlite.jsm connection handle.
 * @param urls
 *        the array of URLs to update.
 * @param [optional] collapseNotifications
 *        whether we can send just one onManyFrecenciesChanged
 *        notification instead of sending one notification for every URL.
 */
var updateFrecency = async function(db, urls, collapseNotifications = false) {
  let urlQuery = 'hash("' + urls.map(url => url.href).join('"), hash("') + '")';

  let frecencyClause = "CALCULATE_FRECENCY(id)";
  if (!collapseNotifications) {
    frecencyClause = "NOTIFY_FRECENCY(" + frecencyClause +
                     ", url, guid, hidden, last_visit_date)";
  }
  // We just use the hashes, since updating a few additional urls won't hurt.
  await db.execute(
    `UPDATE moz_places
     SET hidden = (url_hash BETWEEN hash("place", "prefix_lo") AND hash("place", "prefix_hi")),
         frecency = ${frecencyClause}
     WHERE url_hash IN ( ${urlQuery} )
    `);
  if (collapseNotifications) {
    let observers = PlacesUtils.history.getObservers();
    notify(observers, "onManyFrecenciesChanged");
  }
};

/**
 * Removes any orphan annotation entries.
 *
 * @param db
 *        the Sqlite.jsm connection handle.
 */
var removeOrphanAnnotations = async function(db) {
  await db.executeCached(
    `DELETE FROM moz_items_annos
     WHERE id IN (SELECT a.id from moz_items_annos a
                  LEFT JOIN moz_bookmarks b ON a.item_id = b.id
                  WHERE b.id ISNULL)
    `);
  await db.executeCached(
    `DELETE FROM moz_anno_attributes
     WHERE id IN (SELECT n.id from moz_anno_attributes n
                  LEFT JOIN moz_annos a1 ON a1.anno_attribute_id = n.id
                  LEFT JOIN moz_items_annos a2 ON a2.anno_attribute_id = n.id
                  WHERE a1.id ISNULL AND a2.id ISNULL)
    `);
};

/**
 * Removes annotations for a given item.
 *
 * @param db
 *        the Sqlite.jsm connection handle.
 * @param itemId
 *        internal id of the item for which to remove annotations.
 */
var removeAnnotationsForItem = async function(db, itemId) {
  await db.executeCached(
    `DELETE FROM moz_items_annos
     WHERE item_id = :id
    `, { id: itemId });
  await db.executeCached(
    `DELETE FROM moz_anno_attributes
     WHERE id IN (SELECT n.id from moz_anno_attributes n
                  LEFT JOIN moz_annos a1 ON a1.anno_attribute_id = n.id
                  LEFT JOIN moz_items_annos a2 ON a2.anno_attribute_id = n.id
                  WHERE a1.id ISNULL AND a2.id ISNULL)
    `);
};

/**
 * Updates lastModified for all the ancestors of a given folder GUID.
 *
 * @param db
 *        the Sqlite.jsm connection handle.
 * @param folderGuid
 *        the GUID of the folder whose ancestors should be updated.
 * @param time
 *        a Date object to use for the update.
 *
 * @note the folder itself is also updated.
 */
var setAncestorsLastModified = async function(db, folderGuid, time, syncChangeDelta) {
  await db.executeCached(
    `WITH RECURSIVE
     ancestors(aid) AS (
       SELECT id FROM moz_bookmarks WHERE guid = :guid
       UNION ALL
       SELECT parent FROM moz_bookmarks
       JOIN ancestors ON id = aid
       WHERE type = :type
     )
     UPDATE moz_bookmarks SET lastModified = :time
     WHERE id IN ancestors
    `, { guid: folderGuid, type: Bookmarks.TYPE_FOLDER,
         time: PlacesUtils.toPRTime(time) });

  if (syncChangeDelta) {
    // Flag the folder as having a change.
    await db.executeCached(`
      UPDATE moz_bookmarks SET
        syncChangeCounter = syncChangeCounter + :syncChangeDelta
      WHERE guid = :guid`,
      { guid: folderGuid, syncChangeDelta });
  }
};

/**
 * Remove all descendants of one or more bookmark folders.
 *
 * @param {Object} db
 *        the Sqlite.jsm connection handle.
 * @param {Array} folderGuids
 *        array of folder guids.
 * @return {Array}
 *         An array of urls that will need to be updated for frecency. These
 *         are returned rather than updated immediately so that the caller
 *         can decide when they need to be updated - they do not need to
 *         stop this function from completing.
 */
var removeFoldersContents =
async function(db, folderGuids, options) {
  let syncChangeDelta =
    PlacesSyncUtils.bookmarks.determineSyncChangeDelta(options.source);

  let itemsRemoved = [];
  for (let folderGuid of folderGuids) {
    let rows = await db.executeCached(
      `WITH RECURSIVE
       descendants(did) AS (
         SELECT b.id FROM moz_bookmarks b
         JOIN moz_bookmarks p ON b.parent = p.id
         WHERE p.guid = :folderGuid
         UNION ALL
         SELECT id FROM moz_bookmarks
         JOIN descendants ON parent = did
       )
       SELECT b.id AS _id, b.parent AS _parentId, b.position AS 'index',
              b.type, url, b.guid, p.guid AS parentGuid, b.dateAdded,
              b.lastModified, IFNULL(b.title, "") AS title,
              p.parent AS _grandParentId, NULL AS _childCount,
              b.syncStatus AS _syncStatus
       FROM descendants
       /* The usage of CROSS JOIN is not random, it tells the optimizer
          to retain the original rows order, so the hierarchy is respected */
       CROSS JOIN moz_bookmarks b ON did = b.id
       JOIN moz_bookmarks p ON p.id = b.parent
       LEFT JOIN moz_places h ON b.fk = h.id`, { folderGuid });

    itemsRemoved = itemsRemoved.concat(rowsToItemsArray(rows));

    await db.executeCached(
      `WITH RECURSIVE
       descendants(did) AS (
         SELECT b.id FROM moz_bookmarks b
         JOIN moz_bookmarks p ON b.parent = p.id
         WHERE p.guid = :folderGuid
         UNION ALL
         SELECT id FROM moz_bookmarks
         JOIN descendants ON parent = did
       )
       DELETE FROM moz_bookmarks WHERE id IN descendants`, { folderGuid });
  }

  // Write tombstones for removed items.
  await insertTombstones(db, itemsRemoved, syncChangeDelta);

  // Bump the change counter for all tagged bookmarks when removing tag
  // folders.
  await addSyncChangesForRemovedTagFolders(db, itemsRemoved, syncChangeDelta);

  // Cleanup orphans.
  await removeOrphanAnnotations(db);

  // TODO (Bug 1087576): this may leave orphan tags behind.

  // Send onItemRemoved notifications to listeners.
  // TODO (Bug 1087580): for the case of eraseEverything, this should send a
  // single clear bookmarks notification rather than notifying for each
  // bookmark.

  // Notify listeners in reverse order to serve children before parents.
  let { source = Bookmarks.SOURCES.DEFAULT } = options;
  let observers = PlacesUtils.bookmarks.getObservers();
  for (let item of itemsRemoved.reverse()) {
    let uri = item.hasOwnProperty("url") ? PlacesUtils.toURI(item.url) : null;
    notify(observers, "onItemRemoved", [ item._id, item._parentId,
                                         item.index, item.type, uri,
                                         item.guid, item.parentGuid,
                                         source ],
                                       // Notify observers that this item is being
                                       // removed as a descendent.
                                       {
                                         isDescendantRemoval: true,
                                         parentGuid: item.parentGuid
                                       });

    let isUntagging = item._grandParentId == PlacesUtils.tagsFolderId;
    if (isUntagging) {
      for (let entry of (await fetchBookmarksByURL(item, true))) {
        notify(observers, "onItemChanged", [ entry._id, "tags", false, "",
                                             PlacesUtils.toPRTime(entry.lastModified),
                                             entry.type, entry._parentId,
                                             entry.guid, entry.parentGuid,
                                             "", source ]);
      }
    }
  }
  return itemsRemoved.filter(item => "url" in item).map(item => item.url);
};

/**
 * Tries to insert a new place if it doesn't exist yet.
 * @param url
 *        A valid URL object.
 * @return {Promise} resolved when the operation is complete.
 */
function maybeInsertPlace(db, url) {
  // The IGNORE conflict can trigger on `guid`.
  return db.executeCached(
    `INSERT OR IGNORE INTO moz_places (url, url_hash, rev_host, hidden, frecency, guid)
     VALUES (:url, hash(:url), :rev_host, 0, :frecency,
             IFNULL((SELECT guid FROM moz_places WHERE url_hash = hash(:url) AND url = :url),
                    GENERATE_GUID()))
    `, { url: url.href,
         rev_host: PlacesUtils.getReversedHost(url),
         frecency: url.protocol == "place:" ? 0 : -1 });
}

/**
 * Tries to insert a new place if it doesn't exist yet.
 * @param db
 *        The database to use
 * @param urls
 *        An array with all the url objects to insert.
 * @return {Promise} resolved when the operation is complete.
 */
function maybeInsertManyPlaces(db, urls) {
  return db.executeCached(
    `INSERT OR IGNORE INTO moz_places (url, url_hash, rev_host, hidden, frecency, guid) VALUES
     (:url, hash(:url), :rev_host, 0, :frecency,
     IFNULL((SELECT guid FROM moz_places WHERE url_hash = hash(:url) AND url = :url), :maybeguid))`,
     urls.map(url => ({
       url: url.href,
       rev_host: PlacesUtils.getReversedHost(url),
       frecency: url.protocol == "place:" ? 0 : -1,
       maybeguid: PlacesUtils.history.makeGuid(),
     })));
}

// Indicates whether we should write a tombstone for an item that has been
// uploaded to the server. We ignore "NEW" and "UNKNOWN" items: "NEW" items
// haven't been uploaded yet, and "UNKNOWN" items need a full reconciliation
// with the server.
function needsTombstone(item) {
  return item._syncStatus == Bookmarks.SYNC_STATUS.NORMAL;
}

// Inserts a tombstone for a removed synced item. Tombstones are stored as rows
// in the `moz_bookmarks_deleted` table, and only written for "NORMAL" items.
// After each sync, `PlacesSyncUtils.bookmarks.pushChanges` drops successfully
// uploaded tombstones.
function insertTombstone(db, item, syncChangeDelta) {
  if (!syncChangeDelta || !needsTombstone(item)) {
    return Promise.resolve();
  }
  return db.executeCached(`
    INSERT INTO moz_bookmarks_deleted (guid, dateRemoved)
    VALUES (:guid, :dateRemoved)`,
    { guid: item.guid,
      dateRemoved: PlacesUtils.toPRTime(Date.now()) });
}

// Inserts tombstones for removed synced items.
function insertTombstones(db, itemsRemoved, syncChangeDelta) {
  if (!syncChangeDelta) {
    return Promise.resolve();
  }
  let syncedItems = itemsRemoved.filter(needsTombstone);
  if (!syncedItems.length) {
    return Promise.resolve();
  }
  let dateRemoved = PlacesUtils.toPRTime(Date.now());
  let valuesTable = syncedItems.map(item => `(
    ${JSON.stringify(item.guid)},
    ${dateRemoved}
  )`).join(",");
  return db.execute(`
    INSERT INTO moz_bookmarks_deleted (guid, dateRemoved)
    VALUES ${valuesTable}`
  );
}

// Bumps the change counter for all bookmarks with URLs referenced in removed
// tag folders.
var addSyncChangesForRemovedTagFolders = async function(db, itemsRemoved, syncChangeDelta) {
  if (!syncChangeDelta) {
    return;
  }
  for (let item of itemsRemoved) {
    let isUntagging = item._grandParentId == PlacesUtils.tagsFolderId;
    if (isUntagging) {
      await PlacesSyncUtils.bookmarks.addSyncChangesForBookmarksWithURL(
        db, item.url, syncChangeDelta);
    }
  }
};

// Bumps the change counter for all bookmarked URLs within `folders`.
// This is used to update tagged bookmarks when changing a tag folder.
function addSyncChangesForBookmarksInFolder(db, folder, syncChangeDelta) {
  if (!syncChangeDelta) {
    return Promise.resolve();
  }
  return db.execute(`
    UPDATE moz_bookmarks SET
      syncChangeCounter = syncChangeCounter + :syncChangeDelta
    WHERE type = :type AND
          fk = (SELECT fk FROM moz_bookmarks WHERE parent = :parent)
    `,
    { syncChangeDelta, type: Bookmarks.TYPE_BOOKMARK, parent: folder._id });
}

function adjustSeparatorsSyncCounter(db, parentId, startIndex, syncChangeDelta) {
  if (!syncChangeDelta) {
    return Promise.resolve();
  }

  return db.executeCached(`
    UPDATE moz_bookmarks
    SET syncChangeCounter = syncChangeCounter + :delta
    WHERE parent = :parent AND position >= :start_index
      AND type = :item_type
    `,
    {
      delta: syncChangeDelta,
      parent: parentId,
      start_index: startIndex,
      item_type: Bookmarks.TYPE_SEPARATOR
    });
}

function* chunkArray(array, chunkLength) {
  if (array.length <= chunkLength) {
    yield array;
    return;
  }
  let startIndex = 0;
  while (startIndex < array.length) {
    yield array.slice(startIndex, startIndex += chunkLength);
  }
}
