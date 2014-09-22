/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module provides an asynchronous API for managing bookmarks.
 *
 * Bookmarks are organized in a tree structure, and can be bookmarked URIs,
 * folders or separators.  Multiple bookmarks for the same URI are allowed.
 *
 * Note that if you are handling bookmarks operations in the UI, you should
 * not use this API directly, but rather use PlacesTransactions.jsm, so that
 * any operation is undo/redo-able.
 *
 * Each bookmarked item is represented by an object having the following
 * properties:
 *
 *  - guid (string)
 *      The globally unique identifier of the item.
 *  - parentGuid (string)
 *      The globally unique identifier of the folder containing the item.
 *      This will be an empty string for the Places root folder.
 *  - index (number)
 *      The 0-based position of the item in the parent folder.
 *  - dateAdded (number, microseconds from the epoch)
 *      The time at which the item was added.  This is a PRTime (microseconds).
 *  - lastModified (number, microseconds from the epoch)
 *      The time at which the item was last modified. This is a PRTime (microseconds).
 *  - type (number)
 *      The item's type, either TYPE_BOOKMARK, TYPE_FOLDER or TYPE_SEPARATOR.
 *
 *  The following properties are only valid for bookmarks or folders.
 *
 *  - title (string)
 *      The item's title, if any.  Empty titles and null titles are considered
 *      the same and the property is unset on retrieval in such a case.
 *
 *  The following properties are only valid for bookmarks:
 *
 *  - uri (nsIURI)
 *      The item's URI.
 *  - keyword (string)
 *      The associated keyword, if any.
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
 *
 * @note livemarks are implemented as empty folders.
 *       @see mozIAsyncLivemarks.idl
 */

this.EXPORTED_SYMBOLS = [ "Bookmarks" ];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

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

const URI_LENGTH_MAX = 65536;
const TITLE_LENGTH_MAX = 4096;

let Bookmarks = Object.freeze({
  /**
   * Item's type constants.
   * These should stay consistent with nsINavBookmarksService.idl
   */
  TYPE_BOOKMARK: 1,
  TYPE_FOLDER: 2,
  TYPE_SEPARATOR: 3,

  /**
   * Creates or updates a bookmarked item.
   *
   * If the given guid is found the corresponding item is updated, otherwise,
   * if no guid is provided, a bookmark is created and a new guid is assigned
   * to it.
   *
   * In the creation case, a minimum set of properties must be provided:
   *  - type
   *  - parentGuid
   *  - URI, only for bookmarks
   * If an index is not specified, it defaults to appending.
   * It's also possible to pass a non-existent guid to force creation of an
   * item with the given guid, but unless you have a very sound reason, such as
   * an undo manager implementation or synchronization, you should not do that.
   *
   * In the update case, you should only set the properties which should be
   * changed, undefined properties won't be taken into account for the update.
   * Moreover, the item's type and the guid are ignored, since they are
   * immutable after creation.  Note that if the passed in values are not
   * coherent with the known values, this rejects.
   * Passing null or an empty string as keyword clears any keyword
   * associated with this bookmark.
   *
   * Note that any known property that doesn't apply to the specific item type
   * causes rejection.
   *
   * @param info
   *        object representing a bookmarked item, as defined above.
   *
   * @return {Promise} resolved when the update is complete.
   * @resolves to the input object, updated with relevant information.
   * @rejects JavaScript exception.
   *
   * @note title is truncated to TITLE_LENGTH_MAX and URI is rejected if
   *       greater than URI_LENGTH_MAX.
   */
  // XXX WIP XXX Will replace functionality from these methods:
  // long long insertBookmark(in long long aParentId, in nsIURI aURI, in long aIndex, in AUTF8String aTitle, [optional] in ACString aGUID);
  // long long createFolder(in long long aParentFolder, in AUTF8String name, in long index, [optional] in ACString aGUID);
  // void moveItem(in long long aItemId, in long long aNewParentId, in long aIndex);
  // long long insertSeparator(in long long aParentId, in long aIndex, [optional] in ACString aGUID);
  // void setItemTitle(in long long aItemId, in AUTF8String aTitle);
  // void setItemDateAdded(in long long aItemId, in PRTime aDateAdded);
  // void setItemLastModified(in long long aItemId, in PRTime aLastModified);
  // void changeBookmarkURI(in long long aItemId, in nsIURI aNewURI);
  // void setKeywordForBookmark(in long long aItemId, in AString aKeyword);
  update: Task.async(function* (info) {
    throw new Error("Not yet implemented");
  }),

  /**
   * Removes a bookmarked item.
   *
   * Input can either be a guid or an object with one of the following
   * properties set:
   *  - guid: if set, only the corresponding item is removed.
   *  - parentGuid: if it's set and is a folder, any children of that folder is
   *                removed, but not the folder itself.
   *  - URI: if set, any bookmark for that URI is removed.
   * If multiple of these properties are set, the method rejects.
   *
   * Any other property is ignored, known properties may be overwritten.
   *
   * @param guidOrInfo
   *        The globally unique identifier of the item to remove, or an
   *        object representing it, as defined above.
   *
   * @return {Promise} resolved when the removal is complete.
   * @resolves to the removed object or an array of them.
   * @rejects JavaScript exception.
   */
  // XXX WIP XXX Will replace functionality from these methods:
  // removeItem(in long long aItemId);
  // removeFolderChildren(in long long aItemId);
  remove: Task.async(function* (guidOrInfo) {
    throw new Error("Not yet implemented");
  }),

  /**
   * Fetches information about a bookmarked item.
   *
   * Input can be either a guid or an object with one, and only one, of these
   * filtering properties set:
   *  - guid
   *      retrieves the item with the specified guid
   *  - parentGuid and index
   *      retrieves the item by its position
   *  - URI
   *      retrieves all items having the given URI.
   *  - keyword
   *      retrieves all items having the given keyword.
   *
   * Any other property is ignored.  Known properties may be overwritten.
   *
   * @param guidOrInfo
   *        The globally unique identifier of the item to fetch, or an
   *        object representing it, as defined above.
   *
   * @return {Promise} resolved when the fetch is complete.
   * @resolves to an object representing the found item, as described above, or
   *           an array of such objects.  if no item is found, the returned
   *           promise is resolved to null.
   * @rejects JavaScript exception.
   */
  // XXX WIP XXX Will replace functionality from these methods:
  // long long getIdForItemAt(in long long aParentId, in long aIndex);
  // AUTF8String getItemTitle(in long long aItemId);
  // PRTime getItemDateAdded(in long long aItemId);
  // PRTime getItemLastModified(in long long aItemId);
  // nsIURI getBookmarkURI(in long long aItemId);
  // long getItemIndex(in long long aItemId);
  // unsigned short getItemType(in long long aItemId);
  // boolean isBookmarked(in nsIURI aURI);
  // long long getFolderIdForItem(in long long aItemId);
  // void getBookmarkIdsForURI(in nsIURI aURI, [optional] out unsigned long count, [array, retval, size_is(count)] out long long bookmarks);
  // AString getKeywordForURI(in nsIURI aURI);
  // AString getKeywordForBookmark(in long long aItemId);
  // nsIURI getURIForKeyword(in AString keyword);
  fetch: Task.async(function* (guidOrInfo) {
    throw new Error("Not yet implemented");
  }),

  /**
   * Retrieves an object representation of a bookmarked item, along with all of
   * its descendants, if any.
   *
   * Each node in the tree is an object that extends
   * the item representation described above with some additional properties:
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
   * Bookmarked URIs may also have the following properties:
   *  - tags (string)
   *      csv string of the bookmark's tags, if any.
   *  - charset (string)
   *      the last known charset of the bookmark, if any.
   *  - iconuri (string)
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
   * @rejects JavaScript exception.
   */
  // XXX WIP XXX: will replace functionality for these methods:
  // PlacesUtils.promiseBookmarksTree()
  fetchTree: Task.async(function* (guid = "", options = {}) {
    throw new Error("Not yet implemented");
  }),

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
   * @rejects JavaScript exception.
   */
  // XXX WIP XXX Will replace functionality from these methods:
  // void setItemIndex(in long long aItemId, in long aNewIndex);
  reorder: Task.async(function* (parentGuid, orderedChildrenGuids) {
    throw new Error("Not yet implemented");
  })
});
