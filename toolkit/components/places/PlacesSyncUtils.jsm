/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PlacesSyncUtils"];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.importGlobalProperties(["URL", "URLSearchParams"]);

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Log",
                                  "resource://gre/modules/Log.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

/**
 * This module exports functions for Sync to use when applying remote
 * records. The calls are similar to those in `Bookmarks.jsm` and
 * `nsINavBookmarksService`, with special handling for smart bookmarks,
 * tags, keywords, synced annotations, and missing parents.
 */
var PlacesSyncUtils = {};

const { SOURCE_SYNC } = Ci.nsINavBookmarksService;

// These are defined as lazy getters to defer initializing the bookmarks
// service until it's needed.
XPCOMUtils.defineLazyGetter(this, "ROOT_SYNC_ID_TO_GUID", () => ({
  menu: PlacesUtils.bookmarks.menuGuid,
  places: PlacesUtils.bookmarks.rootGuid,
  tags: PlacesUtils.bookmarks.tagsGuid,
  toolbar: PlacesUtils.bookmarks.toolbarGuid,
  unfiled: PlacesUtils.bookmarks.unfiledGuid,
  mobile: PlacesUtils.bookmarks.mobileGuid,
}));

XPCOMUtils.defineLazyGetter(this, "ROOT_GUID_TO_SYNC_ID", () => ({
  [PlacesUtils.bookmarks.menuGuid]: "menu",
  [PlacesUtils.bookmarks.rootGuid]: "places",
  [PlacesUtils.bookmarks.tagsGuid]: "tags",
  [PlacesUtils.bookmarks.toolbarGuid]: "toolbar",
  [PlacesUtils.bookmarks.unfiledGuid]: "unfiled",
  [PlacesUtils.bookmarks.mobileGuid]: "mobile",
}));

XPCOMUtils.defineLazyGetter(this, "ROOTS", () =>
  Object.keys(ROOT_SYNC_ID_TO_GUID)
);

const BookmarkSyncUtils = PlacesSyncUtils.bookmarks = Object.freeze({
  SMART_BOOKMARKS_ANNO: "Places/SmartBookmark",
  DESCRIPTION_ANNO: "bookmarkProperties/description",
  SIDEBAR_ANNO: "bookmarkProperties/loadInSidebar",
  SYNC_PARENT_ANNO: "sync/parent",
  SYNC_MOBILE_ROOT_ANNO: "mobile/bookmarksRoot",

  KINDS: {
    BOOKMARK: "bookmark",
    // Microsummaries were removed from Places in bug 524091. For now, Sync
    // treats them identically to bookmarks. Bug 745410 tracks removing them
    // entirely.
    MICROSUMMARY: "microsummary",
    QUERY: "query",
    FOLDER: "folder",
    LIVEMARK: "livemark",
    SEPARATOR: "separator",
  },

  get ROOTS() {
    return ROOTS;
  },

  /**
   * Converts a Places GUID to a Sync ID. Sync IDs are identical to Places
   * GUIDs for all items except roots.
   */
  guidToSyncId(guid) {
    return ROOT_GUID_TO_SYNC_ID[guid] || guid;
  },

  /**
   * Converts a Sync record ID to a Places GUID.
   */
  syncIdToGuid(syncId) {
    return ROOT_SYNC_ID_TO_GUID[syncId] || syncId;
  },

  /**
   * Fetches the sync IDs for a folder's children, ordered by their position
   * within the folder.
   */
  fetchChildSyncIds: Task.async(function* (parentSyncId) {
    PlacesUtils.SYNC_BOOKMARK_VALIDATORS.syncId(parentSyncId);
    let parentGuid = BookmarkSyncUtils.syncIdToGuid(parentSyncId);

    let db = yield PlacesUtils.promiseDBConnection();
    let children = yield fetchAllChildren(db, parentGuid);
    return children.map(child =>
      BookmarkSyncUtils.guidToSyncId(child.guid)
    );
  }),

  /**
   * Reorders a folder's children, based on their order in the array of sync
   * IDs.
   *
   * Sync uses this method to reorder all synced children after applying all
   * incoming records.
   *
   */
  order: Task.async(function* (parentSyncId, childSyncIds) {
    PlacesUtils.SYNC_BOOKMARK_VALIDATORS.syncId(parentSyncId);
    if (!childSyncIds.length) {
      return undefined;
    }
    let parentGuid = BookmarkSyncUtils.syncIdToGuid(parentSyncId);
    if (parentGuid == PlacesUtils.bookmarks.rootGuid) {
      // Reordering roots doesn't make sense, but Sync will do this on the
      // first sync.
      return undefined;
    }
    let orderedChildrenGuids = childSyncIds.map(BookmarkSyncUtils.syncIdToGuid);
    return PlacesUtils.bookmarks.reorder(parentGuid, orderedChildrenGuids,
                                         { source: SOURCE_SYNC });
  }),

  /**
   * Removes an item from the database. Options are passed through to
   * PlacesUtils.bookmarks.remove.
   */
  remove: Task.async(function* (syncId, options = {}) {
    let guid = BookmarkSyncUtils.syncIdToGuid(syncId);
    if (guid in ROOT_GUID_TO_SYNC_ID) {
      BookmarkSyncLog.warn(`remove: Refusing to remove root ${syncId}`);
      return null;
    }
    return PlacesUtils.bookmarks.remove(guid, Object.assign({}, options, {
      source: SOURCE_SYNC,
    }));
  }),

  /**
   * Returns true for sync IDs that are considered roots.
   */
  isRootSyncID(syncID) {
    return ROOT_SYNC_ID_TO_GUID.hasOwnProperty(syncID);
  },

  /**
   * Changes the GUID of an existing item. This method only allows Places GUIDs
   * because root sync IDs cannot be changed.
   *
   * @return {Promise} resolved once the GUID has been changed.
   * @resolves to the new GUID.
   * @rejects if the old GUID does not exist.
   */
  changeGuid: Task.async(function* (oldGuid, newGuid) {
    PlacesUtils.BOOKMARK_VALIDATORS.guid(oldGuid);
    PlacesUtils.BOOKMARK_VALIDATORS.guid(newGuid);

    let itemId = yield PlacesUtils.promiseItemId(oldGuid);
    if (PlacesUtils.isRootItem(itemId)) {
      throw new Error(`Cannot change GUID of Places root ${oldGuid}`);
    }
    return PlacesUtils.withConnectionWrapper("BookmarkSyncUtils: changeGuid",
      Task.async(function* (db) {
        yield db.executeCached(`UPDATE moz_bookmarks SET guid = :newGuid
          WHERE id = :itemId`, { newGuid, itemId });
        PlacesUtils.invalidateCachedGuidFor(itemId);
        return newGuid;
      })
    );
  }),

  /**
   * Updates a bookmark with synced properties. Only Sync should call this
   * method; other callers should use `Bookmarks.update`.
   *
   * The following properties are supported:
   *  - kind: Optional.
   *  - guid: Required.
   *  - parentGuid: Optional; reparents the bookmark if specified.
   *  - title: Optional.
   *  - url: Optional.
   *  - tags: Optional; replaces all existing tags.
   *  - keyword: Optional.
   *  - description: Optional.
   *  - loadInSidebar: Optional.
   *  - query: Optional.
   *
   * @param info
   *        object representing a bookmark-item, as defined above.
   *
   * @return {Promise} resolved when the update is complete.
   * @resolves to an object representing the updated bookmark.
   * @rejects if it's not possible to update the given bookmark.
   * @throws if the arguments are invalid.
   */
  update: Task.async(function* (info) {
    let updateInfo = validateSyncBookmarkObject(info,
      { syncId: { required: true }
      });

    return updateSyncBookmark(updateInfo);
  }),

  /**
   * Inserts a synced bookmark into the tree. Only Sync should call this
   * method; other callers should use `Bookmarks.insert`.
   *
   * The following properties are supported:
   *  - kind: Required.
   *  - guid: Required.
   *  - parentGuid: Required.
   *  - url: Required for bookmarks.
   *  - query: A smart bookmark query string, optional.
   *  - tags: An optional array of tag strings.
   *  - keyword: An optional keyword string.
   *  - description: An optional description string.
   *  - loadInSidebar: An optional boolean; defaults to false.
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
  insert: Task.async(function* (info) {
    let insertInfo = validateNewBookmark(info);
    return insertSyncBookmark(insertInfo);
  }),

  /**
   * Fetches a Sync bookmark object for an item in the tree. The object contains
   * the following properties, depending on the item's kind:
   *
   *  - kind (all): A string representing the item's kind.
   *  - syncId (all): The item's sync ID.
   *  - parentSyncId (all): The sync ID of the item's parent.
   *  - parentTitle (all): The title of the item's parent, used for de-duping.
   *    Omitted for the Places root and parents with empty titles.
   *  - title ("bookmark", "folder", "livemark", "query"): The item's title.
   *    Omitted if empty.
   *  - url ("bookmark", "query"): The item's URL.
   *  - tags ("bookmark", "query"): An array containing the item's tags.
   *  - keyword ("bookmark"): The bookmark's keyword, if one exists.
   *  - description ("bookmark", "folder", "livemark"): The item's description.
   *    Omitted if one isn't set.
   *  - loadInSidebar ("bookmark", "query"): Whether to load the bookmark in
   *    the sidebar. Always `false` for queries.
   *  - feed ("livemark"): A `URL` object pointing to the livemark's feed URL.
   *  - site ("livemark"): A `URL` object pointing to the livemark's site URL,
   *    or `null` if one isn't set.
   *  - childSyncIds ("folder"): An array containing the sync IDs of the item's
   *    children, used to determine child order.
   *  - folder ("query"): The tag folder name, if this is a tag query.
   *  - query ("query"): The smart bookmark query name, if this is a smart
   *    bookmark.
   *  - index ("separator"): The separator's position within its parent.
   */
  fetch: Task.async(function* (syncId) {
    let guid = BookmarkSyncUtils.syncIdToGuid(syncId);
    let bookmarkItem = yield PlacesUtils.bookmarks.fetch(guid);
    if (!bookmarkItem) {
      return null;
    }

    // Convert the Places bookmark object to a Sync bookmark and add
    // kind-specific properties.
    let kind = yield getKindForItem(bookmarkItem);
    let item;
    switch (kind) {
      case BookmarkSyncUtils.KINDS.BOOKMARK:
      case BookmarkSyncUtils.KINDS.MICROSUMMARY:
        item = yield fetchBookmarkItem(bookmarkItem);
        break;

      case BookmarkSyncUtils.KINDS.QUERY:
        item = yield fetchQueryItem(bookmarkItem);
        break;

      case BookmarkSyncUtils.KINDS.FOLDER:
        item = yield fetchFolderItem(bookmarkItem);
        break;

      case BookmarkSyncUtils.KINDS.LIVEMARK:
        item = yield fetchLivemarkItem(bookmarkItem);
        break;

      case BookmarkSyncUtils.KINDS.SEPARATOR:
        item = yield placesBookmarkToSyncBookmark(bookmarkItem);
        item.index = bookmarkItem.index;
        break;

      default:
        throw new Error(`Unknown bookmark kind: ${kind}`);
    }

    // Sync uses the parent title for de-duping.
    if (bookmarkItem.parentGuid) {
      let parent = yield PlacesUtils.bookmarks.fetch(bookmarkItem.parentGuid);
      if ("title" in parent) {
        item.parentTitle = parent.title;
      }
    }

    return item;
  }),

  /**
   * Get the sync record kind for the record with provided sync id.
   *
   * @param syncId
   *        Sync ID for the item in question
   *
   * @returns {Promise} A promise that resolves with the sync record kind (e.g.
   *                    something under `PlacesSyncUtils.bookmarks.KIND`), or
   *                    with `null` if no item with that guid exists.
   * @throws if `guid` is invalid.
   */
  getKindForSyncId(syncId) {
    PlacesUtils.SYNC_BOOKMARK_VALIDATORS.syncId(syncId);
    let guid = BookmarkSyncUtils.syncIdToGuid(syncId);
    return PlacesUtils.bookmarks.fetch(guid)
    .then(item => {
      if (!item) {
        return null;
      }
      return getKindForItem(item)
    });
  },
});

XPCOMUtils.defineLazyGetter(this, "BookmarkSyncLog", () => {
  return Log.repository.getLogger("BookmarkSyncUtils");
});

function validateSyncBookmarkObject(input, behavior) {
  return PlacesUtils.validateItemProperties(
    PlacesUtils.SYNC_BOOKMARK_VALIDATORS, input, behavior);
}

// Similar to the private `fetchBookmarksByParent` implementation in
// `Bookmarks.jsm`.
var fetchAllChildren = Task.async(function* (db, parentGuid) {
  let rows = yield db.executeCached(`
    SELECT id, parent, position, type, guid
    FROM moz_bookmarks
    WHERE parent = (
      SELECT id FROM moz_bookmarks WHERE guid = :parentGuid
    )
    ORDER BY position`,
    { parentGuid }
  );
  return rows.map(row => ({
    id: row.getResultByName("id"),
    parentId: row.getResultByName("parent"),
    index: row.getResultByName("position"),
    type: row.getResultByName("type"),
    guid: row.getResultByName("guid"),
  }));
});

// A helper for whenever we want to know if a GUID doesn't exist in the places
// database. Primarily used to detect orphans on incoming records.
var GUIDMissing = Task.async(function* (guid) {
  try {
    yield PlacesUtils.promiseItemId(guid);
    return false;
  } catch (ex) {
    if (ex.message == "no item found for the given GUID") {
      return true;
    }
    throw ex;
  }
});

// Tag queries use a `place:` URL that refers to the tag folder ID. When we
// apply a synced tag query from a remote client, we need to update the URL to
// point to the local tag folder.
var updateTagQueryFolder = Task.async(function* (info) {
  if (info.kind != BookmarkSyncUtils.KINDS.QUERY || !info.folder || !info.url ||
      info.url.protocol != "place:") {
    return info;
  }

  let params = new URLSearchParams(info.url.pathname);
  let type = +params.get("type");

  if (type != Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_CONTENTS) {
    return info;
  }

  let id = yield getOrCreateTagFolder(info.folder);
  BookmarkSyncLog.debug(`updateTagQueryFolder: Tag query folder: ${
    info.folder} = ${id}`);

  // Rewrite the query to reference the new ID.
  params.set("folder", id);
  info.url = new URL(info.url.protocol + params);

  return info;
});

var annotateOrphan = Task.async(function* (item, requestedParentSyncId) {
  let guid = BookmarkSyncUtils.syncIdToGuid(item.syncId);
  let itemId = yield PlacesUtils.promiseItemId(guid);
  PlacesUtils.annotations.setItemAnnotation(itemId,
    BookmarkSyncUtils.SYNC_PARENT_ANNO, requestedParentSyncId, 0,
    PlacesUtils.annotations.EXPIRE_NEVER,
    SOURCE_SYNC);
});

var reparentOrphans = Task.async(function* (item) {
  if (item.kind != BookmarkSyncUtils.KINDS.FOLDER) {
    return;
  }
  let orphanGuids = yield fetchGuidsWithAnno(BookmarkSyncUtils.SYNC_PARENT_ANNO,
                                             item.syncId);
  let folderGuid = BookmarkSyncUtils.syncIdToGuid(item.syncId);
  BookmarkSyncLog.debug(`reparentOrphans: Reparenting ${
    JSON.stringify(orphanGuids)} to ${item.syncId}`);
  for (let i = 0; i < orphanGuids.length; ++i) {
    let isReparented = false;
    try {
      // Reparenting can fail if we have a corrupted or incomplete tree
      // where an item's parent is one of its descendants.
      BookmarkSyncLog.trace(`reparentOrphans: Attempting to move item ${
        orphanGuids[i]} to new parent ${item.syncId}`);
      yield PlacesUtils.bookmarks.update({
        guid: orphanGuids[i],
        parentGuid: folderGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        source: SOURCE_SYNC,
      });
      isReparented = true;
    } catch (ex) {
      BookmarkSyncLog.error(`reparentOrphans: Failed to reparent item ${
        orphanGuids[i]} to ${item.syncId}`, ex);
    }
    if (isReparented) {
      // Remove the annotation once we've reparented the item.
      let orphanId = yield PlacesUtils.promiseItemId(orphanGuids[i]);
      PlacesUtils.annotations.removeItemAnnotation(orphanId,
        BookmarkSyncUtils.SYNC_PARENT_ANNO, SOURCE_SYNC);
    }
  }
});

// Inserts a synced bookmark into the database.
var insertSyncBookmark = Task.async(function* (insertInfo) {
  let requestedParentSyncId = insertInfo.parentSyncId;
  let requestedParentGuid =
    BookmarkSyncUtils.syncIdToGuid(insertInfo.parentSyncId);
  let isOrphan = yield GUIDMissing(requestedParentGuid);

  // Default to "unfiled" for new bookmarks if the parent doesn't exist.
  if (!isOrphan) {
    BookmarkSyncLog.debug(`insertSyncBookmark: Item ${
      insertInfo.syncId} is not an orphan`);
  } else {
    BookmarkSyncLog.debug(`insertSyncBookmark: Item ${
      insertInfo.syncId} is an orphan: parent ${
      insertInfo.parentSyncId} doesn't exist; reparenting to unfiled`);
    insertInfo.parentSyncId = "unfiled";
  }

  // If we're inserting a tag query, make sure the tag exists and fix the
  // folder ID to refer to the local tag folder.
  insertInfo = yield updateTagQueryFolder(insertInfo);

  let newItem;
  if (insertInfo.kind == BookmarkSyncUtils.KINDS.LIVEMARK) {
    newItem = yield insertSyncLivemark(insertInfo);
  } else {
    let bookmarkInfo = syncBookmarkToPlacesBookmark(insertInfo);
    let bookmarkItem = yield PlacesUtils.bookmarks.insert(bookmarkInfo);
    newItem = yield insertBookmarkMetadata(bookmarkItem, insertInfo);
  }

  if (!newItem) {
    return null;
  }

  // If the item is an orphan, annotate it with its real parent sync ID.
  if (isOrphan) {
    yield annotateOrphan(newItem, requestedParentSyncId);
  }

  // Reparent all orphans that expect this folder as the parent.
  yield reparentOrphans(newItem);

  return newItem;
});

// Inserts a synced livemark.
var insertSyncLivemark = Task.async(function* (insertInfo) {
  if (!insertInfo.feed) {
    BookmarkSyncLog.debug(`insertSyncLivemark: ${
      insertInfo.syncId} missing feed URL`);
    return null;
  }
  let livemarkInfo = syncBookmarkToPlacesBookmark(insertInfo);
  let parentIsLivemark = yield getAnno(livemarkInfo.parentGuid,
                                       PlacesUtils.LMANNO_FEEDURI);
  if (parentIsLivemark) {
    // A livemark can't be a descendant of another livemark.
    BookmarkSyncLog.debug(`insertSyncLivemark: Invalid parent ${
      insertInfo.parentSyncId}; skipping livemark record ${
      insertInfo.syncId}`);
    return null;
  }

  let livemarkItem = yield PlacesUtils.livemarks.addLivemark(livemarkInfo);

  return insertBookmarkMetadata(livemarkItem, insertInfo);
});

// Sets annotations, keywords, and tags on a new bookmark. Returns a Sync
// bookmark object.
var insertBookmarkMetadata = Task.async(function* (bookmarkItem, insertInfo) {
  let itemId = yield PlacesUtils.promiseItemId(bookmarkItem.guid);
  let newItem = yield placesBookmarkToSyncBookmark(bookmarkItem);

  if (insertInfo.query) {
    PlacesUtils.annotations.setItemAnnotation(itemId,
      BookmarkSyncUtils.SMART_BOOKMARKS_ANNO, insertInfo.query, 0,
      PlacesUtils.annotations.EXPIRE_NEVER,
      SOURCE_SYNC);
    newItem.query = insertInfo.query;
  }

  try {
    newItem.tags = yield tagItem(bookmarkItem, insertInfo.tags);
  } catch (ex) {
    BookmarkSyncLog.warn(`insertBookmarkMetadata: Error tagging item ${
      insertInfo.syncId}`, ex);
  }

  if (insertInfo.keyword) {
    yield PlacesUtils.keywords.insert({
      keyword: insertInfo.keyword,
      url: bookmarkItem.url.href,
      source: SOURCE_SYNC,
    });
    newItem.keyword = insertInfo.keyword;
  }

  if (insertInfo.description) {
    PlacesUtils.annotations.setItemAnnotation(itemId,
      BookmarkSyncUtils.DESCRIPTION_ANNO, insertInfo.description, 0,
      PlacesUtils.annotations.EXPIRE_NEVER,
      SOURCE_SYNC);
    newItem.description = insertInfo.description;
  }

  if (insertInfo.loadInSidebar) {
    PlacesUtils.annotations.setItemAnnotation(itemId,
      BookmarkSyncUtils.SIDEBAR_ANNO, insertInfo.loadInSidebar, 0,
      PlacesUtils.annotations.EXPIRE_NEVER,
      SOURCE_SYNC);
    newItem.loadInSidebar = insertInfo.loadInSidebar;
  }

  return newItem;
});

// Determines the Sync record kind for an existing bookmark.
var getKindForItem = Task.async(function* (item) {
  switch (item.type) {
    case PlacesUtils.bookmarks.TYPE_FOLDER: {
      let isLivemark = yield getAnno(item.guid,
                                     PlacesUtils.LMANNO_FEEDURI);
      return isLivemark ? BookmarkSyncUtils.KINDS.LIVEMARK :
                          BookmarkSyncUtils.KINDS.FOLDER;
    }
    case PlacesUtils.bookmarks.TYPE_BOOKMARK:
      return item.url.protocol == "place:" ?
             BookmarkSyncUtils.KINDS.QUERY :
             BookmarkSyncUtils.KINDS.BOOKMARK;

    case PlacesUtils.bookmarks.TYPE_SEPARATOR:
      return BookmarkSyncUtils.KINDS.SEPARATOR;
  }
  return null;
});

// Returns the `nsINavBookmarksService` bookmark type constant for a Sync
// record kind.
function getTypeForKind(kind) {
  switch (kind) {
    case BookmarkSyncUtils.KINDS.BOOKMARK:
    case BookmarkSyncUtils.KINDS.MICROSUMMARY:
    case BookmarkSyncUtils.KINDS.QUERY:
      return PlacesUtils.bookmarks.TYPE_BOOKMARK;

    case BookmarkSyncUtils.KINDS.FOLDER:
    case BookmarkSyncUtils.KINDS.LIVEMARK:
      return PlacesUtils.bookmarks.TYPE_FOLDER;

    case BookmarkSyncUtils.KINDS.SEPARATOR:
      return PlacesUtils.bookmarks.TYPE_SEPARATOR;
  }
  throw new Error(`Unknown bookmark kind: ${kind}`);
}

// Determines if a livemark should be reinserted. Returns true if `updateInfo`
// specifies different feed or site URLs; false otherwise.
var shouldReinsertLivemark = Task.async(function* (updateInfo) {
  let hasFeed = updateInfo.hasOwnProperty("feed");
  let hasSite = updateInfo.hasOwnProperty("site");
  if (!hasFeed && !hasSite) {
    return false;
  }
  let guid = BookmarkSyncUtils.syncIdToGuid(updateInfo.syncId);
  let livemark = yield PlacesUtils.livemarks.getLivemark({
    guid,
  });
  if (hasFeed) {
    let feedURI = PlacesUtils.toURI(updateInfo.feed);
    if (!livemark.feedURI.equals(feedURI)) {
      return true;
    }
  }
  if (hasSite) {
    if (!updateInfo.site) {
      return !!livemark.siteURI;
    }
    let siteURI = PlacesUtils.toURI(updateInfo.site);
    if (!livemark.siteURI || !siteURI.equals(livemark.siteURI)) {
      return true;
    }
  }
  return false;
});

var updateSyncBookmark = Task.async(function* (updateInfo) {
  let guid = BookmarkSyncUtils.syncIdToGuid(updateInfo.syncId);
  let oldBookmarkItem = yield PlacesUtils.bookmarks.fetch(guid);
  if (!oldBookmarkItem) {
    throw new Error(`Bookmark with sync ID ${
      updateInfo.syncId} does not exist`);
  }

  let shouldReinsert = false;
  let oldKind = yield getKindForItem(oldBookmarkItem);
  if (updateInfo.hasOwnProperty("kind") && updateInfo.kind != oldKind) {
    // If the item's aren't the same kind, we can't update the record;
    // we must remove and reinsert.
    shouldReinsert = true;
    if (BookmarkSyncLog.level <= Log.Level.Warn) {
      let oldSyncId = BookmarkSyncUtils.guidToSyncId(oldBookmarkItem.guid);
      BookmarkSyncLog.warn(`updateSyncBookmark: Local ${
        oldSyncId} kind = ${oldKind}; remote ${
        updateInfo.syncId} kind = ${
        updateInfo.kind}. Deleting and recreating`);
    }
  } else if (oldKind == BookmarkSyncUtils.KINDS.LIVEMARK) {
    // Similarly, if we're changing a livemark's site or feed URL, we need to
    // reinsert.
    shouldReinsert = yield shouldReinsertLivemark(updateInfo);
    if (BookmarkSyncLog.level <= Log.Level.Debug) {
      let oldSyncId = BookmarkSyncUtils.guidToSyncId(oldBookmarkItem.guid);
      BookmarkSyncLog.debug(`updateSyncBookmark: Local ${
        oldSyncId} and remote ${
        updateInfo.syncId} livemarks have different URLs`);
    }
  }

  if (shouldReinsert) {
    let newInfo = validateNewBookmark(updateInfo);
    yield PlacesUtils.bookmarks.remove({
      guid,
      source: SOURCE_SYNC,
    });
    // A reinsertion likely indicates a confused client, since there aren't
    // public APIs for changing livemark URLs or an item's kind (e.g., turning
    // a folder into a separator while preserving its annos and position).
    // This might be a good case to repair later; for now, we assume Sync has
    // passed a complete record for the new item, and don't try to merge
    // `oldBookmarkItem` with `updateInfo`.
    return insertSyncBookmark(newInfo);
  }

  let isOrphan = false, requestedParentSyncId;
  if (updateInfo.hasOwnProperty("parentSyncId")) {
    requestedParentSyncId = updateInfo.parentSyncId;
    let oldParentSyncId =
      BookmarkSyncUtils.guidToSyncId(oldBookmarkItem.parentGuid);
    if (requestedParentSyncId != oldParentSyncId) {
      let oldId = yield PlacesUtils.promiseItemId(oldBookmarkItem.guid);
      if (PlacesUtils.isRootItem(oldId)) {
        throw new Error(`Cannot move Places root ${oldId}`);
      }
      let requestedParentGuid =
        BookmarkSyncUtils.syncIdToGuid(requestedParentSyncId);
      isOrphan = yield GUIDMissing(requestedParentGuid);
      if (!isOrphan) {
        BookmarkSyncLog.debug(`updateSyncBookmark: Item ${
          updateInfo.syncId} is not an orphan`);
      } else {
        // Don't move the item if the new parent doesn't exist. Instead, mark
        // the item as an orphan. We'll annotate it with its real parent after
        // updating.
        BookmarkSyncLog.trace(`updateSyncBookmark: Item ${
          updateInfo.syncId} is an orphan: could not find parent ${
          requestedParentSyncId}`);
        delete updateInfo.parentSyncId;
      }
    } else {
      // If the parent is the same, just omit it so that `update` doesn't do
      // extra work.
      delete updateInfo.parentSyncId;
    }
  }

  updateInfo = yield updateTagQueryFolder(updateInfo);

  let bookmarkInfo = syncBookmarkToPlacesBookmark(updateInfo);
  let newBookmarkItem = shouldUpdateBookmark(bookmarkInfo) ?
                        yield PlacesUtils.bookmarks.update(bookmarkInfo) :
                        oldBookmarkItem;
  let newItem = yield updateBookmarkMetadata(oldBookmarkItem, newBookmarkItem,
                                             updateInfo);

  // If the item is an orphan, annotate it with its real parent sync ID.
  if (isOrphan) {
    yield annotateOrphan(newItem, requestedParentSyncId);
  }

  // Reparent all orphans that expect this folder as the parent.
  yield reparentOrphans(newItem);

  return newItem;
});

// Updates tags, keywords, and annotations for an existing bookmark. Returns a
// Sync bookmark object.
var updateBookmarkMetadata = Task.async(function* (oldBookmarkItem,
                                                   newBookmarkItem,
                                                   updateInfo) {
  let itemId = yield PlacesUtils.promiseItemId(newBookmarkItem.guid);
  let newItem = yield placesBookmarkToSyncBookmark(newBookmarkItem);

  try {
    newItem.tags = yield tagItem(newBookmarkItem, updateInfo.tags);
  } catch (ex) {
    BookmarkSyncLog.warn(`updateBookmarkMetadata: Error tagging item ${
      updateInfo.syncId}`, ex);
  }

  if (updateInfo.hasOwnProperty("keyword")) {
    // Unconditionally remove the old keyword.
    let entry = yield PlacesUtils.keywords.fetch({
      url: oldBookmarkItem.url.href,
    });
    if (entry) {
      yield PlacesUtils.keywords.remove({
        keyword: entry.keyword,
        source: SOURCE_SYNC,
      });
    }
    if (updateInfo.keyword) {
      yield PlacesUtils.keywords.insert({
        keyword: updateInfo.keyword,
        url: newItem.url.href,
        source: SOURCE_SYNC,
      });
    }
    newItem.keyword = updateInfo.keyword;
  }

  if (updateInfo.hasOwnProperty("description")) {
    if (updateInfo.description) {
      PlacesUtils.annotations.setItemAnnotation(itemId,
        BookmarkSyncUtils.DESCRIPTION_ANNO, updateInfo.description, 0,
        PlacesUtils.annotations.EXPIRE_NEVER,
        SOURCE_SYNC);
    } else {
      PlacesUtils.annotations.removeItemAnnotation(itemId,
        BookmarkSyncUtils.DESCRIPTION_ANNO, SOURCE_SYNC);
    }
    newItem.description = updateInfo.description;
  }

  if (updateInfo.hasOwnProperty("loadInSidebar")) {
    if (updateInfo.loadInSidebar) {
      PlacesUtils.annotations.setItemAnnotation(itemId,
        BookmarkSyncUtils.SIDEBAR_ANNO, updateInfo.loadInSidebar, 0,
        PlacesUtils.annotations.EXPIRE_NEVER,
        SOURCE_SYNC);
    } else {
      PlacesUtils.annotations.removeItemAnnotation(itemId,
        BookmarkSyncUtils.SIDEBAR_ANNO, SOURCE_SYNC);
    }
    newItem.loadInSidebar = updateInfo.loadInSidebar;
  }

  if (updateInfo.hasOwnProperty("query")) {
    PlacesUtils.annotations.setItemAnnotation(itemId,
      BookmarkSyncUtils.SMART_BOOKMARKS_ANNO, updateInfo.query, 0,
      PlacesUtils.annotations.EXPIRE_NEVER,
      SOURCE_SYNC);
    newItem.query = updateInfo.query;
  }

  return newItem;
});

function validateNewBookmark(info) {
  let insertInfo = validateSyncBookmarkObject(info,
    { kind: { required: true }
    , syncId: { required: true }
    , url: { requiredIf: b => [ BookmarkSyncUtils.KINDS.BOOKMARK
                              , BookmarkSyncUtils.KINDS.MICROSUMMARY
                              , BookmarkSyncUtils.KINDS.QUERY ].includes(b.kind)
           , validIf: b => [ BookmarkSyncUtils.KINDS.BOOKMARK
                           , BookmarkSyncUtils.KINDS.MICROSUMMARY
                           , BookmarkSyncUtils.KINDS.QUERY ].includes(b.kind) }
    , parentSyncId: { required: true }
    , title: { validIf: b => [ BookmarkSyncUtils.KINDS.BOOKMARK
                             , BookmarkSyncUtils.KINDS.MICROSUMMARY
                             , BookmarkSyncUtils.KINDS.QUERY
                             , BookmarkSyncUtils.KINDS.FOLDER
                             , BookmarkSyncUtils.KINDS.LIVEMARK ].includes(b.kind) }
    , query: { validIf: b => b.kind == BookmarkSyncUtils.KINDS.QUERY }
    , folder: { validIf: b => b.kind == BookmarkSyncUtils.KINDS.QUERY }
    , tags: { validIf: b => [ BookmarkSyncUtils.KINDS.BOOKMARK
                            , BookmarkSyncUtils.KINDS.MICROSUMMARY
                            , BookmarkSyncUtils.KINDS.QUERY ].includes(b.kind) }
    , keyword: { validIf: b => [ BookmarkSyncUtils.KINDS.BOOKMARK
                               , BookmarkSyncUtils.KINDS.MICROSUMMARY
                               , BookmarkSyncUtils.KINDS.QUERY ].includes(b.kind) }
    , description: { validIf: b => [ BookmarkSyncUtils.KINDS.BOOKMARK
                                   , BookmarkSyncUtils.KINDS.MICROSUMMARY
                                   , BookmarkSyncUtils.KINDS.QUERY
                                   , BookmarkSyncUtils.KINDS.FOLDER
                                   , BookmarkSyncUtils.KINDS.LIVEMARK ].includes(b.kind) }
    , loadInSidebar: { validIf: b => [ BookmarkSyncUtils.KINDS.BOOKMARK
                                     , BookmarkSyncUtils.KINDS.MICROSUMMARY
                                     , BookmarkSyncUtils.KINDS.QUERY ].includes(b.kind) }
    , feed: { validIf: b => b.kind == BookmarkSyncUtils.KINDS.LIVEMARK }
    , site: { validIf: b => b.kind == BookmarkSyncUtils.KINDS.LIVEMARK }
    });

  return insertInfo;
}

// Returns an array of GUIDs for items that have an `anno` with the given `val`.
var fetchGuidsWithAnno = Task.async(function* (anno, val) {
  let db = yield PlacesUtils.promiseDBConnection();
  let rows = yield db.executeCached(`
    SELECT b.guid FROM moz_items_annos a
    JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
    JOIN moz_bookmarks b ON b.id = a.item_id
    WHERE n.name = :anno AND
          a.content = :val`,
    { anno, val });
  return rows.map(row => row.getResultByName("guid"));
});

// Returns the value of an item's annotation, or `null` if it's not set.
var getAnno = Task.async(function* (guid, anno) {
  let db = yield PlacesUtils.promiseDBConnection();
  let rows = yield db.executeCached(`
    SELECT a.content FROM moz_items_annos a
    JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
    JOIN moz_bookmarks b ON b.id = a.item_id
    WHERE b.guid = :guid AND
          n.name = :anno`,
    { guid, anno });
  return rows.length ? rows[0].getResultByName("content") : null;
});

var tagItem = Task.async(function(item, tags) {
  if (!item.url) {
    return [];
  }

  // Remove leading and trailing whitespace, then filter out empty tags.
  let newTags = tags.map(tag => tag.trim()).filter(Boolean);

  // Removing the last tagged item will also remove the tag. To preserve
  // tag IDs, we temporarily tag a dummy URI, ensuring the tags exist.
  let dummyURI = PlacesUtils.toURI("about:weave#BStore_tagURI");
  let bookmarkURI = PlacesUtils.toURI(item.url.href);
  PlacesUtils.tagging.tagURI(dummyURI, newTags, SOURCE_SYNC);
  PlacesUtils.tagging.untagURI(bookmarkURI, null, SOURCE_SYNC);
  PlacesUtils.tagging.tagURI(bookmarkURI, newTags, SOURCE_SYNC);
  PlacesUtils.tagging.untagURI(dummyURI, null, SOURCE_SYNC);

  return newTags;
});

// `PlacesUtils.bookmarks.update` checks if we've supplied enough properties,
// but doesn't know about additional livemark properties. We check this to avoid
// having it throw in case we only pass properties like `{ guid, feedURI }`.
function shouldUpdateBookmark(bookmarkInfo) {
  return bookmarkInfo.hasOwnProperty("parentGuid") ||
         bookmarkInfo.hasOwnProperty("title") ||
         bookmarkInfo.hasOwnProperty("url");
}

var getTagFolder = Task.async(function* (tag) {
  let db = yield PlacesUtils.promiseDBConnection();
  let results = yield db.executeCached(`SELECT id FROM moz_bookmarks
    WHERE parent = :tagsFolder AND title = :tag LIMIT 1`,
    { tagsFolder: PlacesUtils.bookmarks.tagsFolder, tag });
  return results.length ? results[0].getResultByName("id") : null;
});

var getOrCreateTagFolder = Task.async(function* (tag) {
  let id = yield getTagFolder(tag);
  if (id) {
    return id;
  }
  // Create the tag if it doesn't exist.
  let item = yield PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.tagsGuid,
    title: tag,
    source: SOURCE_SYNC,
  });
  return PlacesUtils.promiseItemId(item.guid);
});

// Converts a Places bookmark or livemark to a Sync bookmark. This function
// maps Places GUIDs to sync IDs and filters out extra Places properties like
// date added, last modified, and index.
var placesBookmarkToSyncBookmark = Task.async(function* (bookmarkItem) {
  let item = {};

  for (let prop in bookmarkItem) {
    switch (prop) {
      // Sync IDs are identical to Places GUIDs for all items except roots.
      case "guid":
        item.syncId = BookmarkSyncUtils.guidToSyncId(bookmarkItem.guid);
        break;

      case "parentGuid":
        item.parentSyncId =
          BookmarkSyncUtils.guidToSyncId(bookmarkItem.parentGuid);
        break;

      // Sync uses kinds instead of types, which distinguish between folders,
      // livemarks, bookmarks, and queries.
      case "type":
        item.kind = yield getKindForItem(bookmarkItem);
        break;

      case "title":
      case "url":
        item[prop] = bookmarkItem[prop];
        break;

      // Livemark objects contain additional properties. The feed URL is
      // required; the site URL is optional.
      case "feedURI":
        item.feed = new URL(bookmarkItem.feedURI.spec);
        break;

      case "siteURI":
        if (bookmarkItem.siteURI) {
          item.site = new URL(bookmarkItem.siteURI.spec);
        }
        break;
    }
  }

  return item;
});

// Converts a Sync bookmark object to a Places bookmark or livemark object.
// This function maps sync IDs to Places GUIDs, and filters out extra Sync
// properties like keywords, tags, and descriptions. Returns an object that can
// be passed to `PlacesUtils.livemarks.addLivemark` or
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

      // Convert sync IDs to Places GUIDs for roots.
      case "syncId":
        bookmarkInfo.guid = BookmarkSyncUtils.syncIdToGuid(info.syncId);
        break;

      case "parentSyncId":
        bookmarkInfo.parentGuid =
          BookmarkSyncUtils.syncIdToGuid(info.parentSyncId);
        // Instead of providing an index, Sync reorders children at the end of
        // the sync using `BookmarkSyncUtils.order`. We explicitly specify the
        // default index here to prevent `PlacesUtils.bookmarks.update` and
        // `PlacesUtils.livemarks.addLivemark` from throwing.
        bookmarkInfo.index = PlacesUtils.bookmarks.DEFAULT_INDEX;
        break;

      case "title":
      case "url":
        bookmarkInfo[prop] = info[prop];
        break;

      // Livemark-specific properties.
      case "feed":
        bookmarkInfo.feedURI = PlacesUtils.toURI(info.feed);
        break;

      case "site":
        if (info.site) {
          bookmarkInfo.siteURI = PlacesUtils.toURI(info.site);
        }
        break;
    }
  }

  return bookmarkInfo;
}

// Creates and returns a Sync bookmark object containing the bookmark's
// tags, keyword, description, and whether it loads in the sidebar.
var fetchBookmarkItem = Task.async(function* (bookmarkItem) {
  let item = yield placesBookmarkToSyncBookmark(bookmarkItem);

  item.tags = PlacesUtils.tagging.getTagsForURI(
    PlacesUtils.toURI(bookmarkItem.url), {});

  let keywordEntry = yield PlacesUtils.keywords.fetch({
    url: bookmarkItem.url,
  });
  if (keywordEntry) {
    item.keyword = keywordEntry.keyword;
  }

  let description = yield getAnno(bookmarkItem.guid,
                                  BookmarkSyncUtils.DESCRIPTION_ANNO);
  if (description) {
    item.description = description;
  }

  item.loadInSidebar = !!(yield getAnno(bookmarkItem.guid,
                                        BookmarkSyncUtils.SIDEBAR_ANNO));

  return item;
});

// Creates and returns a Sync bookmark object containing the folder's
// description and children.
var fetchFolderItem = Task.async(function* (bookmarkItem) {
  let item = yield placesBookmarkToSyncBookmark(bookmarkItem);

  let description = yield getAnno(bookmarkItem.guid,
                                  BookmarkSyncUtils.DESCRIPTION_ANNO);
  if (description) {
    item.description = description;
  }

  let db = yield PlacesUtils.promiseDBConnection();
  let children = yield fetchAllChildren(db, bookmarkItem.guid);
  item.childSyncIds = children.map(child =>
    BookmarkSyncUtils.guidToSyncId(child.guid)
  );

  return item;
});

// Creates and returns a Sync bookmark object containing the livemark's
// description, children (none), feed URI, and site URI.
var fetchLivemarkItem = Task.async(function* (bookmarkItem) {
  let item = yield placesBookmarkToSyncBookmark(bookmarkItem);

  let description = yield getAnno(bookmarkItem.guid,
                                  BookmarkSyncUtils.DESCRIPTION_ANNO);
  if (description) {
    item.description = description;
  }

  let feedAnno = yield getAnno(bookmarkItem.guid, PlacesUtils.LMANNO_FEEDURI);
  item.feed = new URL(feedAnno);

  let siteAnno = yield getAnno(bookmarkItem.guid, PlacesUtils.LMANNO_SITEURI);
  if (siteAnno) {
    item.site = new URL(siteAnno);
  }

  return item;
});

// Creates and returns a Sync bookmark object containing the query's tag
// folder name and smart bookmark query ID.
var fetchQueryItem = Task.async(function* (bookmarkItem) {
  let item = yield placesBookmarkToSyncBookmark(bookmarkItem);

  let description = yield getAnno(bookmarkItem.guid,
                                  BookmarkSyncUtils.DESCRIPTION_ANNO);
  if (description) {
    item.description = description;
  }

  let folder = null;
  let params = new URLSearchParams(bookmarkItem.url.pathname);
  let tagFolderId = +params.get("folder");
  if (tagFolderId) {
    try {
      let tagFolderGuid = yield PlacesUtils.promiseItemGuid(tagFolderId);
      let tagFolder = yield PlacesUtils.bookmarks.fetch(tagFolderGuid);
      folder = tagFolder.title;
    } catch (ex) {
      BookmarkSyncLog.warn("fetchQueryItem: Query " + bookmarkItem.url.href +
                           " points to nonexistent folder " + tagFolderId, ex);
    }
  }
  if (folder != null) {
    item.folder = folder;
  }

  let query = yield getAnno(bookmarkItem.guid,
                            BookmarkSyncUtils.SMART_BOOKMARKS_ANNO);
  if (query) {
    item.query = query;
  }

  return item;
});
