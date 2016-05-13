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
 *      the same, and the property is unset on retrieval in such a case.
 *      Titles longer than DB_TITLE_LENGTH_MAX will be truncated.
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
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
                                  "resource://gre/modules/Sqlite.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

// Imposed to limit database size.
const DB_URL_LENGTH_MAX = 65536;
const DB_TITLE_LENGTH_MAX = 4096;

const MATCH_ANYWHERE_UNMODIFIED = Ci.mozIPlacesAutoComplete.MATCH_ANYWHERE_UNMODIFIED;
const BEHAVIOR_BOOKMARK = Ci.mozIPlacesAutoComplete.BEHAVIOR_BOOKMARK;

var Bookmarks = Object.freeze({
  /**
   * Item's type constants.
   * These should stay consistent with nsINavBookmarksService.idl
   */
  TYPE_BOOKMARK: 1,
  TYPE_FOLDER: 2,
  TYPE_SEPARATOR: 3,

  /**
   * Default index used to append a bookmark-item at the end of a folder.
   * This should stay consistent with nsINavBookmarksService.idl
   */
  DEFAULT_INDEX: -1,

  /**
   * Special GUIDs associated with bookmark roots.
   * It's guaranteed that the roots will always have these guids.
   */

   rootGuid:    "root________",
   menuGuid:    "menu________",
   toolbarGuid: "toolbar_____",
   unfiledGuid: "unfiled_____",

   // With bug 424160, tags will stop being bookmarks, thus this root will
   // be removed.  Do not rely on this, rather use the tagging service API.
   tagsGuid:    "tags________",

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
    // Ensure to use the same date for dateAdded and lastModified, even if
    // dateAdded may be imposed by the caller.
    let time = (info && info.dateAdded) || new Date();
    let insertInfo = validateBookmarkObject(info,
      { type: { defaultValue: this.TYPE_BOOKMARK }
      , index: { defaultValue: this.DEFAULT_INDEX }
      , url: { requiredIf: b => b.type == this.TYPE_BOOKMARK
             , validIf: b => b.type == this.TYPE_BOOKMARK }
      , parentGuid: { required: true }
      , title: { validIf: b => [ this.TYPE_BOOKMARK
                               , this.TYPE_FOLDER ].includes(b.type) }
      , dateAdded: { defaultValue: time
                   , validIf: b => !b.lastModified ||
                                    b.dateAdded <= b.lastModified }
      , lastModified: { defaultValue: time,
                        validIf: b => (!b.dateAdded && b.lastModified >= time) ||
                                      (b.dateAdded && b.lastModified >= b.dateAdded) }
      });

    return Task.spawn(function* () {
      // Ensure the parent exists.
      let parent = yield fetchBookmark({ guid: insertInfo.parentGuid });
      if (!parent)
        throw new Error("parentGuid must be valid");

      // Set index in the appending case.
      if (insertInfo.index == this.DEFAULT_INDEX ||
          insertInfo.index > parent._childCount) {
        insertInfo.index = parent._childCount;
      }

      let item = yield insertBookmark(insertInfo, parent);

      // Notify onItemAdded to listeners.
      let observers = PlacesUtils.bookmarks.getObservers();
      // We need the itemId to notify, though once the switch to guids is
      // complete we may stop using it.
      let uri = item.hasOwnProperty("url") ? PlacesUtils.toURI(item.url) : null;
      let itemId = yield PlacesUtils.promiseItemId(item.guid);
      notify(observers, "onItemAdded", [ itemId, parent._id, item.index,
                                         item.type, uri, item.title || null,
                                         PlacesUtils.toPRTime(item.dateAdded), item.guid,
                                         item.parentGuid ]);

      // If it's a tag, notify OnItemChanged to all bookmarks for this URL.
      let isTagging = parent._parentId == PlacesUtils.tagsFolderId;
      if (isTagging) {
        for (let entry of (yield fetchBookmarksByURL(item))) {
          notify(observers, "onItemChanged", [ entry._id, "tags", false, "",
                                               PlacesUtils.toPRTime(entry.lastModified),
                                               entry.type, entry._parentId,
                                               entry.guid, entry.parentGuid,
                                               "" ]);
        }
      }

      // Remove non-enumerable properties.
      return Object.assign({}, item);
    }.bind(this));
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
    let updateInfo = validateBookmarkObject(info,
      { guid: { required: true }
      , index: { requiredIf: b => b.hasOwnProperty("parentGuid")
               , validIf: b => b.index >= 0 || b.index == this.DEFAULT_INDEX }
      });

    // There should be at last one more property in addition to guid.
    if (Object.keys(updateInfo).length < 2)
      throw new Error("Not enough properties to update");

    return Task.spawn(function* () {
      // Ensure the item exists.
      let item = yield fetchBookmark(updateInfo);
      if (!item)
        throw new Error("No bookmarks found for the provided GUID");
      if (updateInfo.hasOwnProperty("type") && updateInfo.type != item.type)
        throw new Error("The bookmark type cannot be changed");
      if (updateInfo.hasOwnProperty("dateAdded") &&
          updateInfo.dateAdded.getTime() != item.dateAdded.getTime())
        throw new Error("The bookmark dateAdded cannot be changed");

      // Remove any property that will stay the same.
      removeSameValueProperties(updateInfo, item);
      // Check if anything should still be updated.
      if (Object.keys(updateInfo).length < 2) {
        // Remove non-enumerable properties.
        return Object.assign({}, item);
      }

      let time = (updateInfo && updateInfo.dateAdded) || new Date();
      updateInfo = validateBookmarkObject(updateInfo,
        { url: { validIf: () => item.type == this.TYPE_BOOKMARK }
        , title: { validIf: () => [ this.TYPE_BOOKMARK
                                  , this.TYPE_FOLDER ].includes(item.type) }
        , lastModified: { defaultValue: new Date()
                        , validIf: b => b.lastModified >= item.dateAdded }
        });

      return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: update",
        Task.async(function*(db) {
        let parent;
        if (updateInfo.hasOwnProperty("parentGuid")) {
          if (item.type == this.TYPE_FOLDER) {
            // Make sure we are not moving a folder into itself or one of its
            // descendants.
            let rows = yield db.executeCached(
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

          parent = yield fetchBookmark({ guid: updateInfo.parentGuid });
          if (!parent)
            throw new Error("No bookmarks found for the provided parentGuid");
        }

        if (updateInfo.hasOwnProperty("index")) {
          // If at this point we don't have a parent yet, we are moving into
          // the same container.  Thus we know it exists.
          if (!parent)
            parent = yield fetchBookmark({ guid: item.parentGuid });

          if (updateInfo.index >= parent._childCount ||
              updateInfo.index == this.DEFAULT_INDEX) {
             updateInfo.index = parent._childCount;

            // Fix the index when moving within the same container.
            if (parent.guid == item.parentGuid)
               updateInfo.index--;
          }
        }

        let updatedItem = yield updateBookmark(updateInfo, item, parent);

        if (item.type == this.TYPE_BOOKMARK &&
            item.url.href != updatedItem.url.href) {
          // ...though we don't wait for the calculation.
          updateFrecency(db, [item.url]).then(null, Cu.reportError);
          updateFrecency(db, [updatedItem.url]).then(null, Cu.reportError);
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
                                               updatedItem.parentGuid, "" ]);
        }
        if (updateInfo.hasOwnProperty("title")) {
          notify(observers, "onItemChanged", [ updatedItem._id, "title",
                                               false, updatedItem.title,
                                               PlacesUtils.toPRTime(updatedItem.lastModified),
                                               updatedItem.type,
                                               updatedItem._parentId,
                                               updatedItem.guid,
                                               updatedItem.parentGuid, "" ]);
        }
        if (updateInfo.hasOwnProperty("url")) {
          notify(observers, "onItemChanged", [ updatedItem._id, "uri",
                                               false, updatedItem.url.href,
                                               PlacesUtils.toPRTime(updatedItem.lastModified),
                                               updatedItem.type,
                                               updatedItem._parentId,
                                               updatedItem.guid,
                                               updatedItem.parentGuid,
                                               item.url.href ]);
        }
        // If the item was moved, notify onItemMoved.
        if (item.parentGuid != updatedItem.parentGuid ||
            item.index != updatedItem.index) {
          notify(observers, "onItemMoved", [ updatedItem._id, item._parentId,
                                             item.index, updatedItem._parentId,
                                             updatedItem.index, updatedItem.type,
                                             updatedItem.guid, item.parentGuid,
                                             updatedItem.parentGuid ]);
        }

        // Remove non-enumerable properties.
        return Object.assign({}, updatedItem);
      }.bind(this)));
    }.bind(this));
  },

  /**
   * Removes a bookmark-item.
   *
   * @param guidOrInfo
   *        The globally unique identifier of the item to remove, or an
   *        object representing it, as defined above.
   * @param {Object} [options={}]
   *        Additional options that can be passed to the function.
   *        Currently supports preventRemovalOfNonEmptyFolders which
   *        will cause an exception to be thrown if attempting to remove
   *        a folder that is not empty.
   *
   * @return {Promise} resolved when the removal is complete.
   * @resolves to an object representing the removed bookmark.
   * @rejects if the provided guid doesn't match any existing bookmark.
   * @throws if the arguments are invalid.
   */
  remove(guidOrInfo, options={}) {
    let info = guidOrInfo;
    if (!info)
      throw new Error("Input should be a valid object");
    if (typeof(guidOrInfo) != "object")
      info = { guid: guidOrInfo };

    // Disallow removing the root folders.
    if ([this.rootGuid, this.menuGuid, this.toolbarGuid, this.unfiledGuid,
         this.tagsGuid].includes(info.guid)) {
      throw new Error("It's not possible to remove Places root folders.");
    }

    // Even if we ignore any other unneeded property, we still validate any
    // known property to reduce likelihood of hidden bugs.
    let removeInfo = validateBookmarkObject(info);

    return Task.spawn(function* () {
      let item = yield fetchBookmark(removeInfo);
      if (!item)
        throw new Error("No bookmarks found for the provided GUID.");

      item = yield removeBookmark(item, options);

      // Notify onItemRemoved to listeners.
      let observers = PlacesUtils.bookmarks.getObservers();
      let uri = item.hasOwnProperty("url") ? PlacesUtils.toURI(item.url) : null;
      notify(observers, "onItemRemoved", [ item._id, item._parentId, item.index,
                                           item.type, uri, item.guid,
                                           item.parentGuid ]);

      let isUntagging = item._grandParentId == PlacesUtils.tagsFolderId;
      if (isUntagging) {
        for (let entry of (yield fetchBookmarksByURL(item))) {
          notify(observers, "onItemChanged", [ entry._id, "tags", false, "",
                                               PlacesUtils.toPRTime(entry.lastModified),
                                               entry.type, entry._parentId,
                                               entry.guid, entry.parentGuid,
                                               "" ]);
        }
      }

      // Remove non-enumerable properties.
      return Object.assign({}, item);
    });
  },

  /**
   * Removes ALL bookmarks, resetting the bookmarks storage to an empty tree.
   *
   * Note that roots are preserved, only their children will be removed.
   *
   * @return {Promise} resolved when the removal is complete.
   * @resolves once the removal is complete.
   */
  eraseEverything: function() {
    return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: eraseEverything",
      db => db.executeTransaction(function* () {
        const folderGuids = [this.toolbarGuid, this.menuGuid, this.unfiledGuid];
        yield removeFoldersContents(db, folderGuids);
        const time = PlacesUtils.toPRTime(new Date());
        for (let folderGuid of folderGuids) {
          yield db.executeCached(
            `UPDATE moz_bookmarks SET lastModified = :time
             WHERE id IN (SELECT id FROM moz_bookmarks WHERE guid = :folderGuid )
            `, { folderGuid, time });
        }
      }.bind(this))
    );
  },

  /**
   * Searches a list of bookmark-items by a search term, url or title.
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
      query = { query: query };
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

    return Task.spawn(function* () {
      let results = yield queryBookmarks(query);

      return results;
    });
  },

  /**
   * Returns a list of recently bookmarked items.
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
    if (!typeof numberOfItems === 'number' || (numberOfItems % 1) !== 0) {
      throw new Error("numberOfItems argument must be an integer");
    }
    if (numberOfItems <= 0) {
      throw new Error("numberOfItems argument must be greater than zero");
    }

    return Task.spawn(function* () {
      return yield fetchRecentBookmarks(numberOfItems);
    });
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
  fetch(guidOrInfo, onResult=null) {
    if (onResult && typeof onResult != "function")
      throw new Error("onResult callback must be a valid function");
    let info = guidOrInfo;
    if (!info)
      throw new Error("Input should be a valid object");
    if (typeof(info) != "object")
      info = { guid: guidOrInfo };

    // Only one condition at a time can be provided.
    let conditionsCount = [
      v => v.hasOwnProperty("guid"),
      v => v.hasOwnProperty("parentGuid") && v.hasOwnProperty("index"),
      v => v.hasOwnProperty("url")
    ].reduce((old, fn) => old + fn(info)|0, 0);
    if (conditionsCount != 1)
      throw new Error(`Unexpected number of conditions provided: ${conditionsCount}`);

    // Even if we ignore any other unneeded property, we still validate any
    // known property to reduce likelihood of hidden bugs.
    let fetchInfo = validateBookmarkObject(info,
      { parentGuid: { requiredIf: b => b.hasOwnProperty("index") }
      , index: { requiredIf: b => b.hasOwnProperty("parentGuid")
               , validIf: b => typeof(b.index) == "number" &&
                               b.index >= 0 || b.index == this.DEFAULT_INDEX }
      });

    return Task.spawn(function* () {
      let results;
      if (fetchInfo.hasOwnProperty("guid"))
        results = yield fetchBookmark(fetchInfo);
      else if (fetchInfo.hasOwnProperty("parentGuid") && fetchInfo.hasOwnProperty("index"))
        results = yield fetchBookmarkByPosition(fetchInfo);
      else if (fetchInfo.hasOwnProperty("url"))
        results = yield fetchBookmarksByURL(fetchInfo);

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
    });
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
   *        incomplete, missing entries will be appended.
   *
   * @return {Promise} resolved when reordering is complete.
   * @rejects if an error happens while reordering.
   * @throws if the arguments are invalid.
   */
  reorder(parentGuid, orderedChildrenGuids) {
    let info = { guid: parentGuid };
    info = validateBookmarkObject(info, { guid: { required: true } });

    if (!Array.isArray(orderedChildrenGuids) || !orderedChildrenGuids.length)
      throw new Error("Must provide a sorted array of children GUIDs.");
    try {
      orderedChildrenGuids.forEach(VALIDATORS.guid);
    } catch (ex) {
      throw new Error("Invalid GUID found in the sorted children array.");
    }

    return Task.spawn(function* () {
      let parent = yield fetchBookmark(info);
      if (!parent || parent.type != this.TYPE_FOLDER)
        throw new Error("No folder found for the provided GUID.");

      let sortedChildren = yield reorderChildren(parent, orderedChildrenGuids);

      let observers = PlacesUtils.bookmarks.getObservers();
      // Note that child.index is the old index.
      for (let i = 0; i < sortedChildren.length; ++i) {
        let child = sortedChildren[i];
        notify(observers, "onItemMoved", [ child._id, child._parentId,
                                           child.index, child._parentId,
                                           i, child.type,
                                           child.guid, child.parentGuid,
                                           child.parentGuid ]);
      }
    }.bind(this));
  }
});

////////////////////////////////////////////////////////////////////////////////
// Globals.

/**
 * Sends a bookmarks notification through the given observers.
 *
 * @param observers
 *        array of nsINavBookmarkObserver objects.
 * @param notification
 *        the notification name.
 * @param args
 *        array of arguments to pass to the notification.
 */
function notify(observers, notification, args) {
  for (let observer of observers) {
    try {
      observer[notification](...args);
    } catch (ex) {}
  }
}

////////////////////////////////////////////////////////////////////////////////
// Update implementation.

function updateBookmark(info, item, newParent) {
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: updateBookmark",
    Task.async(function*(db) {

    let tuples = new Map();
    if (info.hasOwnProperty("lastModified"))
      tuples.set("lastModified", { value: PlacesUtils.toPRTime(info.lastModified) });
    if (info.hasOwnProperty("title"))
      tuples.set("title", { value: info.title });

    yield db.executeTransaction(function* () {
      if (info.hasOwnProperty("url")) {
        // Ensure a page exists in moz_places for this URL.
        yield db.executeCached(
          `INSERT OR IGNORE INTO moz_places (url, rev_host, hidden, frecency, guid)
           VALUES (:url, :rev_host, 0, :frecency, GENERATE_GUID())
          `, { url: info.url ? info.url.href : null,
               rev_host: PlacesUtils.getReversedHost(info.url),
               frecency: info.url.protocol == "place:" ? 0 : -1 });
        tuples.set("url", { value: info.url.href
                          , fragment: "fk = (SELECT id FROM moz_places WHERE url = :url)" });
      }

      if (newParent) {
        // For simplicity, update the index regardless.
        let newIndex = info.hasOwnProperty("index") ? info.index : item.index;
        tuples.set("position", { value: newIndex });

        if (newParent.guid == item.parentGuid) {
          // Moving inside the original container.
          // When moving "up", add 1 to each index in the interval.
          // Otherwise when moving down, we subtract 1.
          let sign = newIndex < item.index ? +1 : -1;
          yield db.executeCached(
            `UPDATE moz_bookmarks SET position = position + :sign
             WHERE parent = :newParentId
               AND position BETWEEN :lowIndex AND :highIndex
            `, { sign: sign, newParentId: newParent._id,
                 lowIndex: Math.min(item.index, newIndex),
                 highIndex: Math.max(item.index, newIndex) });
        } else {
          // Moving across different containers.
          tuples.set("parent", { value: newParent._id} );
          yield db.executeCached(
            `UPDATE moz_bookmarks SET position = position + :sign
             WHERE parent = :oldParentId
               AND position >= :oldIndex
            `, { sign: -1, oldParentId: item._parentId, oldIndex: item.index });
          yield db.executeCached(
            `UPDATE moz_bookmarks SET position = position + :sign
             WHERE parent = :newParentId
               AND position >= :newIndex
            `, { sign: +1, newParentId: newParent._id, newIndex: newIndex });

          yield setAncestorsLastModified(db, item.parentGuid, info.lastModified);
        }
        yield setAncestorsLastModified(db, newParent.guid, info.lastModified);
      }

      yield db.executeCached(
        `UPDATE moz_bookmarks
         SET ${Array.from(tuples.keys()).map(v => tuples.get(v).fragment || `${v} = :${v}`).join(", ")}
         WHERE guid = :guid
        `, Object.assign({ guid: info.guid },
                         [...tuples.entries()].reduce((p, c) => { p[c[0]] = c[1].value; return p; }, {})));
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

    // Don't return an empty title to the caller.
    if (updatedItem.hasOwnProperty("title") && updatedItem.title === null)
      delete updatedItem.title;

    return updatedItem;
  }));
}

////////////////////////////////////////////////////////////////////////////////
// Insert implementation.

function insertBookmark(item, parent) {
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: insertBookmark",
    Task.async(function*(db) {

    // If a guid was not provided, generate one, so we won't need to fetch the
    // bookmark just after having created it.
    if (!item.hasOwnProperty("guid"))
      item.guid = (yield db.executeCached("SELECT GENERATE_GUID() AS guid"))[0].getResultByName("guid");

    yield db.executeTransaction(function* transaction() {
      if (item.type == Bookmarks.TYPE_BOOKMARK) {
        // Ensure a page exists in moz_places for this URL.
        yield db.executeCached(
          `INSERT OR IGNORE INTO moz_places (url, rev_host, hidden, frecency, guid)
           VALUES (:url, :rev_host, 0, :frecency, GENERATE_GUID())
          `, { url: item.url.href, rev_host: PlacesUtils.getReversedHost(item.url),
               frecency: item.url.protocol == "place:" ? 0 : -1 });
      }

      // Adjust indices.
      yield db.executeCached(
        `UPDATE moz_bookmarks SET position = position + 1
         WHERE parent = :parent
         AND position >= :index
        `, { parent: parent._id, index: item.index });

      // Insert the bookmark into the database.
      yield db.executeCached(
        `INSERT INTO moz_bookmarks (fk, type, parent, position, title,
                                    dateAdded, lastModified, guid)
         VALUES ((SELECT id FROM moz_places WHERE url = :url), :type, :parent,
                 :index, :title, :date_added, :last_modified, :guid)
        `, { url: item.hasOwnProperty("url") ? item.url.href : "nonexistent",
             type: item.type, parent: parent._id, index: item.index,
             title: item.title, date_added: PlacesUtils.toPRTime(item.dateAdded),
             last_modified: PlacesUtils.toPRTime(item.lastModified), guid: item.guid });

      yield setAncestorsLastModified(db, item.parentGuid, item.dateAdded);
    });

    // If not a tag recalculate frecency...
    let isTagging = parent._parentId == PlacesUtils.tagsFolderId;
    if (item.type == Bookmarks.TYPE_BOOKMARK && !isTagging) {
      // ...though we don't wait for the calculation.
      updateFrecency(db, [item.url]).then(null, Cu.reportError);
    }

    // Don't return an empty title to the caller.
    if (item.hasOwnProperty("title") && item.title === null)
      delete item.title;

    return item;
  }));
}

////////////////////////////////////////////////////////////////////////////////
// Query implementation.

function queryBookmarks(info) {
  let queryParams = {tags_folder: PlacesUtils.tagsFolderId};
  // we're searching for bookmarks, so exclude tags
  let queryString = "WHERE p.parent <> :tags_folder";

  if (info.title) {
    queryString += " AND b.title = :title";
    queryParams.title = info.title;
  }

  if (info.url) {
    queryString += " AND h.url = :url";
    queryParams.url = info.url;
  }

  if (info.query) {
    queryString += " AND AUTOCOMPLETE_MATCH(:query, h.url, b.title, NULL, NULL, 1, 1, NULL, :matchBehavior, :searchBehavior) ";
    queryParams.query = info.query;
    queryParams.matchBehavior = MATCH_ANYWHERE_UNMODIFIED;
    queryParams.searchBehavior = BEHAVIOR_BOOKMARK;
  }

  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: queryBookmarks",
    Task.async(function*(db) {

    // _id, _childCount, _grandParentId and _parentId fields
    // are required to be in the result by the converting function
    // hence setting them to NULL
    let rows = yield db.executeCached(
      `SELECT b.guid, IFNULL(p.guid, "") AS parentGuid, b.position AS 'index',
              b.dateAdded, b.lastModified, b.type, b.title,
              h.url AS url, b.parent, p.parent,
              NULL AS _id,
              NULL AS _childCount,
              NULL AS _grandParentId,
              NULL AS _parentId
       FROM moz_bookmarks b
       LEFT JOIN moz_bookmarks p ON p.id = b.parent
       LEFT JOIN moz_places h ON h.id = b.fk
       ${queryString}
      `, queryParams);

    return rowsToItemsArray(rows);
  }));
}


////////////////////////////////////////////////////////////////////////////////
// Fetch implementation.

function fetchBookmark(info) {
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: fetchBookmark",
    Task.async(function*(db) {

    let rows = yield db.executeCached(
      `SELECT b.guid, IFNULL(p.guid, "") AS parentGuid, b.position AS 'index',
              b.dateAdded, b.lastModified, b.type, b.title, h.url AS url,
              b.id AS _id, b.parent AS _parentId,
              (SELECT count(*) FROM moz_bookmarks WHERE parent = b.id) AS _childCount,
              p.parent AS _grandParentId
       FROM moz_bookmarks b
       LEFT JOIN moz_bookmarks p ON p.id = b.parent
       LEFT JOIN moz_places h ON h.id = b.fk
       WHERE b.guid = :guid
      `, { guid: info.guid });

    return rows.length ? rowsToItemsArray(rows)[0] : null;
  }));
}

function fetchBookmarkByPosition(info) {
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: fetchBookmarkByPosition",
    Task.async(function*(db) {
    let index = info.index == Bookmarks.DEFAULT_INDEX ? null : info.index;

    let rows = yield db.executeCached(
      `SELECT b.guid, IFNULL(p.guid, "") AS parentGuid, b.position AS 'index',
              b.dateAdded, b.lastModified, b.type, b.title, h.url AS url,
              b.id AS _id, b.parent AS _parentId,
              (SELECT count(*) FROM moz_bookmarks WHERE parent = b.id) AS _childCount,
              p.parent AS _grandParentId
       FROM moz_bookmarks b
       LEFT JOIN moz_bookmarks p ON p.id = b.parent
       LEFT JOIN moz_places h ON h.id = b.fk
       WHERE p.guid = :parentGuid
       AND b.position = IFNULL(:index, (SELECT count(*) - 1
                                        FROM moz_bookmarks
                                        WHERE parent = p.id))
      `, { parentGuid: info.parentGuid, index });

    return rows.length ? rowsToItemsArray(rows)[0] : null;
  }));
}

function fetchBookmarksByURL(info) {
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: fetchBookmarksByURL",
    Task.async(function*(db) {

    let rows = yield db.executeCached(
      `/* do not warn (bug no): not worth to add an index */
       SELECT b.guid, IFNULL(p.guid, "") AS parentGuid, b.position AS 'index',
              b.dateAdded, b.lastModified, b.type, b.title, h.url AS url,
              b.id AS _id, b.parent AS _parentId,
              (SELECT count(*) FROM moz_bookmarks WHERE parent = b.id) AS _childCount,
              p.parent AS _grandParentId
       FROM moz_bookmarks b
       LEFT JOIN moz_bookmarks p ON p.id = b.parent
       LEFT JOIN moz_places h ON h.id = b.fk
       WHERE h.url = :url
       AND _grandParentId <> :tags_folder
       ORDER BY b.lastModified DESC
      `, { url: info.url.href,
           tags_folder: PlacesUtils.tagsFolderId });

    return rows.length ? rowsToItemsArray(rows) : null;
  }));
}

function fetchRecentBookmarks(numberOfItems) {
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: fetchRecentBookmarks",
    Task.async(function*(db) {

    let rows = yield db.executeCached(
      `SELECT b.guid, IFNULL(p.guid, "") AS parentGuid, b.position AS 'index',
              b.dateAdded, b.lastModified, b.type, b.title, h.url AS url,
              NULL AS _id, NULL AS _parentId, NULL AS _childCount, NULL AS _grandParentId
       FROM moz_bookmarks b
       LEFT JOIN moz_bookmarks p ON p.id = b.parent
       LEFT JOIN moz_places h ON h.id = b.fk
       WHERE p.parent <> :tags_folder
       ORDER BY b.dateAdded DESC, b.ROWID DESC
       LIMIT :numberOfItems
      `, { tags_folder: PlacesUtils.tagsFolderId, numberOfItems });

    return rows.length ? rowsToItemsArray(rows) : [];
  }));
}

function fetchBookmarksByParent(info) {
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: fetchBookmarksByParent",
    Task.async(function*(db) {

    let rows = yield db.executeCached(
      `SELECT b.guid, IFNULL(p.guid, "") AS parentGuid, b.position AS 'index',
              b.dateAdded, b.lastModified, b.type, b.title, h.url AS url,
              b.id AS _id, b.parent AS _parentId,
              (SELECT count(*) FROM moz_bookmarks WHERE parent = b.id) AS _childCount,
              p.parent AS _grandParentId
       FROM moz_bookmarks b
       LEFT JOIN moz_bookmarks p ON p.id = b.parent
       LEFT JOIN moz_places h ON h.id = b.fk
       WHERE p.guid = :parentGuid
       ORDER BY b.position ASC
      `, { parentGuid: info.parentGuid });

    return rowsToItemsArray(rows);
  }));
}

////////////////////////////////////////////////////////////////////////////////
// Remove implementation.

function removeBookmark(item, options) {
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: updateBookmark",
    Task.async(function*(db) {

    let isUntagging = item._grandParentId == PlacesUtils.tagsFolderId;

    yield db.executeTransaction(function* transaction() {
      // If it's a folder, remove its contents first.
      if (item.type == Bookmarks.TYPE_FOLDER) {
        if (options.preventRemovalOfNonEmptyFolders && item._childCount > 0) {
          throw new Error("Cannot remove a non-empty folder.");
        }
        yield removeFoldersContents(db, [item.guid]);
      }

      // Remove annotations first.  If it's a tag, we can avoid paying that cost.
      if (!isUntagging) {
        // We don't go through the annotations service for this cause otherwise
        // we'd get a pointless onItemChanged notification and it would also
        // set lastModified to an unexpected value.
        yield removeAnnotationsForItem(db, item._id);
      }

      // Remove the bookmark from the database.
      yield db.executeCached(
        `DELETE FROM moz_bookmarks WHERE guid = :guid`, { guid: item.guid });

      // Fix indices in the parent.
      yield db.executeCached(
        `UPDATE moz_bookmarks SET position = position - 1 WHERE
         parent = :parentId AND position > :index
        `, { parentId: item._parentId, index: item.index });

      yield setAncestorsLastModified(db, item.parentGuid, new Date());
    });

    // If not a tag recalculate frecency...
    if (item.type == Bookmarks.TYPE_BOOKMARK && !isUntagging) {
      // ...though we don't wait for the calculation.
      updateFrecency(db, [item.url]).then(null, Cu.reportError);
    }

    return item;
  }));
}

////////////////////////////////////////////////////////////////////////////////
// Reorder implementation.

function reorderChildren(parent, orderedChildrenGuids) {
  return PlacesUtils.withConnectionWrapper("Bookmarks.jsm: updateBookmark",
    db => db.executeTransaction(function* () {
      // Select all of the direct children for the given parent.
      let children = yield fetchBookmarksByParent({ parentGuid: parent.guid });
      if (!children.length)
        return undefined;

      // Reorder the children array according to the specified order, provided
      // GUIDs come first, others are appended in somehow random order.
      children.sort((a, b) => {
        let i = orderedChildrenGuids.indexOf(a.guid);
        let j = orderedChildrenGuids.indexOf(b.guid);
        // This works provided fetchBookmarksByParent returns sorted children.
        if (i == -1 && j == -1)
          return 0;
        return (i != -1 && j != -1 && i < j) || (i != -1 && j == -1) ? -1 : 1;
       });

      // Update the bookmarks position now.  If any unknown guid have been
      // inserted meanwhile, its position will be set to -position, and we'll
      // handle it later.
      // To do the update in a single step, we build a VALUES (guid, position)
      // table.  We then use count() in the sorting table to avoid skipping values
      // when no more existing GUIDs have been provided.
      let valuesTable = children.map((child, i) => `("${child.guid}", ${i})`)
                                .join();
      yield db.execute(
        `WITH sorting(g, p) AS (
           VALUES ${valuesTable}
         )
         UPDATE moz_bookmarks SET position = (
           SELECT CASE count(a.g) WHEN 0 THEN -position
                                  ELSE count(a.g) - 1
                  END
           FROM sorting a
           JOIN sorting b ON b.p <= a.p
           WHERE a.g = guid
             AND parent = :parentId
        )`, { parentId: parent._id});

      // Update position of items that could have been inserted in the meanwhile.
      // Since this can happen rarely and it's only done for schema coherence
      // resonds, we won't notify about these changes.
      yield db.executeCached(
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

      yield db.executeCached(
        `UPDATE moz_bookmarks SET position = -1 WHERE position < 0`);

      yield db.executeCached(`DROP TRIGGER moz_bookmarks_reorder_trigger`);

      return children;
    }.bind(this))
  );
}

////////////////////////////////////////////////////////////////////////////////
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
    for (let prop of ["guid", "index", "type"]) {
      item[prop] = row.getResultByName(prop);
    }
    for (let prop of ["dateAdded", "lastModified"]) {
      item[prop] = PlacesUtils.toDate(row.getResultByName(prop));
    }
    for (let prop of ["title", "parentGuid", "url" ]) {
      let val = row.getResultByName(prop);
      if (val)
        item[prop] = prop === "url" ? new URL(val) : val;
    }
    for (let prop of ["_id", "_parentId", "_childCount", "_grandParentId"]) {
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

/**
 * Executes a boolean validate function, throwing if it returns false.
 *
 * @param boolValidateFn
 *        A boolean validate function.
 * @return the input value.
 * @throws if input doesn't pass the validate function.
 */
function simpleValidateFunc(boolValidateFn) {
  return (v, input) => {
    if (!boolValidateFn(v, input))
      throw new Error("Invalid value");
    return v;
  };
}

/**
 * List of validators, one per each known property.
 * Validators must throw if the property value is invalid and return a fixed up
 * version of the value, if needed.
 */
const VALIDATORS = Object.freeze({
  guid: simpleValidateFunc(v => typeof(v) == "string" &&
                                PlacesUtils.isValidGuid(v)),
  parentGuid: simpleValidateFunc(v => typeof(v) == "string" &&
                                      /^[a-zA-Z0-9\-_]{12}$/.test(v)),
  index: simpleValidateFunc(v => Number.isInteger(v) &&
                                 v >= Bookmarks.DEFAULT_INDEX),
  dateAdded: simpleValidateFunc(v => v.constructor.name == "Date"),
  lastModified: simpleValidateFunc(v => v.constructor.name == "Date"),
  type: simpleValidateFunc(v => Number.isInteger(v) &&
                                [ Bookmarks.TYPE_BOOKMARK
                                , Bookmarks.TYPE_FOLDER
                                , Bookmarks.TYPE_SEPARATOR ].includes(v)),
  title: v => {
    simpleValidateFunc(val => val === null || typeof(val) == "string").call(this, v);
    if (!v)
      return null;
    return v.slice(0, DB_TITLE_LENGTH_MAX);
  },
  url: v => {
    simpleValidateFunc(val => (typeof(val) == "string" && val.length <= DB_URL_LENGTH_MAX) ||
                              (val instanceof Ci.nsIURI && val.spec.length <= DB_URL_LENGTH_MAX) ||
                              (val instanceof URL && val.href.length <= DB_URL_LENGTH_MAX)
                      ).call(this, v);
    if (typeof(v) === "string")
      return new URL(v);
    if (v instanceof Ci.nsIURI)
      return new URL(v.spec);
    return v;
  }
});

/**
 * Checks validity of a bookmark object, filling up default values for optional
 * properties.
 *
 * @param input (object)
 *        The bookmark object to validate.
 * @param behavior (object) [optional]
 *        Object defining special behavior for some of the properties.
 *        The following behaviors may be optionally set:
 *         - requiredIf: if the provided condition is satisfied, then this
 *                       property is required.
 *         - validIf: if the provided condition is not satisfied, then this
 *                    property is invalid.
 *         - defaultValue: an undefined property should default to this value.
 *
 * @return a validated and normalized bookmark-item.
 * @throws if the object contains invalid data.
 * @note any unknown properties are pass-through.
 */
function validateBookmarkObject(input, behavior={}) {
  if (!input)
    throw new Error("Input should be a valid object");
  let normalizedInput = {};
  let required = new Set();
  for (let prop in behavior) {
    if (behavior[prop].hasOwnProperty("required") && behavior[prop].required) {
      required.add(prop);
    }
    if (behavior[prop].hasOwnProperty("requiredIf") && behavior[prop].requiredIf(input)) {
      required.add(prop);
    }
    if (behavior[prop].hasOwnProperty("validIf") && input[prop] !== undefined &&
        !behavior[prop].validIf(input)) {
      throw new Error(`Invalid value for property '${prop}': ${input[prop]}`);
    }
    if (behavior[prop].hasOwnProperty("defaultValue") && input[prop] === undefined) {
      input[prop] = behavior[prop].defaultValue;
    }
  }

  for (let prop in input) {
    if (required.has(prop)) {
      required.delete(prop);
    } else if (input[prop] === undefined) {
      // Skip undefined properties that are not required.
      continue;
    }
    if (VALIDATORS.hasOwnProperty(prop)) {
      try {
        normalizedInput[prop] = VALIDATORS[prop](input[prop], input);
      } catch(ex) {
        throw new Error(`Invalid value for property '${prop}': ${input[prop]}`);
      }
    }
  }
  if (required.size > 0)
    throw new Error(`The following properties were expected: ${[...required].join(", ")}`);
  return normalizedInput;
}

/**
 * Updates frecency for a list of URLs.
 *
 * @param db
 *        the Sqlite.jsm connection handle.
 * @param urls
 *        the array of URLs to update.
 */
var updateFrecency = Task.async(function* (db, urls) {
  yield db.execute(
    `UPDATE moz_places
     SET frecency = NOTIFY_FRECENCY(
       CALCULATE_FRECENCY(id), url, guid, hidden, last_visit_date
     ) WHERE url IN ( ${urls.map(url => JSON.stringify(url.href)).join(", ")} )
    `);

  yield db.execute(
    `UPDATE moz_places
     SET hidden = 0
     WHERE url IN ( ${urls.map(url => JSON.stringify(url.href)).join(", ")} )
       AND frecency <> 0
    `);
});

/**
 * Removes any orphan annotation entries.
 *
 * @param db
 *        the Sqlite.jsm connection handle.
 */
var removeOrphanAnnotations = Task.async(function* (db) {
  yield db.executeCached(
    `DELETE FROM moz_items_annos
     WHERE id IN (SELECT a.id from moz_items_annos a
                  LEFT JOIN moz_bookmarks b ON a.item_id = b.id
                  WHERE b.id ISNULL)
    `);
  yield db.executeCached(
    `DELETE FROM moz_anno_attributes
     WHERE id IN (SELECT n.id from moz_anno_attributes n
                  LEFT JOIN moz_annos a1 ON a1.anno_attribute_id = n.id
                  LEFT JOIN moz_items_annos a2 ON a2.anno_attribute_id = n.id
                  WHERE a1.id ISNULL AND a2.id ISNULL)
    `);
});

/**
 * Removes annotations for a given item.
 *
 * @param db
 *        the Sqlite.jsm connection handle.
 * @param itemId
 *        internal id of the item for which to remove annotations.
 */
var removeAnnotationsForItem = Task.async(function* (db, itemId) {
  yield db.executeCached(
    `DELETE FROM moz_items_annos
     WHERE item_id = :id
    `, { id: itemId });
  yield db.executeCached(
    `DELETE FROM moz_anno_attributes
     WHERE id IN (SELECT n.id from moz_anno_attributes n
                  LEFT JOIN moz_annos a1 ON a1.anno_attribute_id = n.id
                  LEFT JOIN moz_items_annos a2 ON a2.anno_attribute_id = n.id
                  WHERE a1.id ISNULL AND a2.id ISNULL)
    `);
});

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
var setAncestorsLastModified = Task.async(function* (db, folderGuid, time) {
  yield db.executeCached(
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
});

/**
 * Remove all descendants of one or more bookmark folders.
 *
 * @param db
 *        the Sqlite.jsm connection handle.
 * @param folderGuids
 *        array of folder guids.
 */
var removeFoldersContents =
Task.async(function* (db, folderGuids) {
  let itemsRemoved = [];
  for (let folderGuid of folderGuids) {
    let rows = yield db.executeCached(
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
              b.lastModified, b.title, p.parent AS _grandParentId,
              NULL AS _childCount
       FROM moz_bookmarks b
       JOIN moz_bookmarks p ON p.id = b.parent
       LEFT JOIN moz_places h ON b.fk = h.id
       WHERE b.id IN descendants`, { folderGuid });

    itemsRemoved = itemsRemoved.concat(rowsToItemsArray(rows));

    yield db.executeCached(
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

  // Cleanup orphans.
  yield removeOrphanAnnotations(db);

  // TODO (Bug 1087576): this may leave orphan tags behind.

  let urls = itemsRemoved.filter(item => "url" in item).map(item => item.url);
  updateFrecency(db, urls).then(null, Cu.reportError);

  // Send onItemRemoved notifications to listeners.
  // TODO (Bug 1087580): for the case of eraseEverything, this should send a
  // single clear bookmarks notification rather than notifying for each
  // bookmark.

  // Notify listeners in reverse order to serve children before parents.
  let observers = PlacesUtils.bookmarks.getObservers();
  for (let item of itemsRemoved.reverse()) {
    let uri = item.hasOwnProperty("url") ? PlacesUtils.toURI(item.url) : null;
    notify(observers, "onItemRemoved", [ item._id, item._parentId,
                                         item.index, item.type, uri,
                                         item.guid, item.parentGuid ]);

    let isUntagging = item._grandParentId == PlacesUtils.tagsFolderId;
    if (isUntagging) {
      for (let entry of (yield fetchBookmarksByURL(item))) {
        notify(observers, "onItemChanged", [ entry._id, "tags", false, "",
                                             PlacesUtils.toPRTime(entry.lastModified),
                                             entry.type, entry._parentId,
                                             entry.guid, entry.parentGuid,
                                             "" ]);
      }
    }
  }
});
