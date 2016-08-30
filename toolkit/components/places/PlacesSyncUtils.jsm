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

const SMART_BOOKMARKS_ANNO = "Places/SmartBookmark";
const DESCRIPTION_ANNO = "bookmarkProperties/description";
const SIDEBAR_ANNO = "bookmarkProperties/loadInSidebar";
const PARENT_ANNO = "sync/parent";

const { SOURCE_SYNC } = Ci.nsINavBookmarksService;

const BookmarkSyncUtils = PlacesSyncUtils.bookmarks = Object.freeze({
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

  /**
   * Fetches a folder's children, ordered by their position within the folder.
   * Children without a GUID will be assigned one.
   */
  fetchChildGuids: Task.async(function* (parentGuid) {
    PlacesUtils.SYNC_BOOKMARK_VALIDATORS.guid(parentGuid);

    let db = yield PlacesUtils.promiseDBConnection();
    let children = yield fetchAllChildren(db, parentGuid);
    let childGuids = [];
    let guidsToSet = new Map();
    for (let child of children) {
      let guid = child.guid;
      if (!PlacesUtils.isValidGuid(guid)) {
        // Give the child a GUID if it doesn't have one. This shouldn't happen,
        // but the old bookmarks engine code does this, so we'll match its
        // behavior until we're sure this can be removed.
        guid = yield generateGuid(db);
        BookmarkSyncLog.warn(`fetchChildGuids: Assigning ${
          guid} to item without GUID ${child.id}`);
        guidsToSet.set(child.id, guid);
      }
      childGuids.push(guid);
    }
    if (guidsToSet.size > 0) {
      yield setGuids(guidsToSet);
    }
    return childGuids;
  }),

  /**
   * Reorders a folder's children, based on their order in the array of GUIDs.
   * This method is similar to `Bookmarks.reorder`, but leaves missing entries
   * in place instead of moving them to the end of the folder.
   *
   * Sync uses this method to reorder all synced children after applying all
   * incoming records.
   *
   */
  order: Task.async(function* (parentGuid, childGuids) {
    PlacesUtils.SYNC_BOOKMARK_VALIDATORS.guid(parentGuid);
    for (let guid of childGuids) {
      PlacesUtils.SYNC_BOOKMARK_VALIDATORS.guid(guid);
    }

    if (parentGuid == PlacesUtils.bookmarks.rootGuid) {
      // Reordering roots doesn't make sense, but Sync will do this on the
      // first sync.
      return Promise.resolve();
    }
    return PlacesUtils.withConnectionWrapper("BookmarkSyncUtils: order",
      Task.async(function* (db) {
        let children;

        yield db.executeTransaction(function* () {
          children = yield fetchAllChildren(db, parentGuid);
          if (!children.length) {
            return;
          }
          for (let child of children) {
            // Note the current index for notifying observers. This can
            // be removed once we switch to `reorder`.
            child.oldIndex = child.index;
          }

          // Reorder the list, ignoring missing children.
          let delta = 0;
          for (let i = 0; i < childGuids.length; ++i) {
            let guid = childGuids[i];
            let child = findChildByGuid(children, guid);
            if (!child) {
              delta++;
              BookmarkSyncLog.trace(`order: Ignoring missing child ${guid}`);
              continue;
            }
            let newIndex = i - delta;
            updateChildIndex(children, child, newIndex);
          }
          children.sort((a, b) => a.index - b.index);

          // Update positions. We use a custom query instead of
          // `PlacesUtils.bookmarks.reorder` because `reorder` introduces holes
          // (bug 1293365). Once it's fixed, we can uncomment this code and
          // remove the transaction, query, and observer notification code.

          /*
          let orderedChildrenGuids = children.map(({ guid }) => guid);
          yield PlacesUtils.bookmarks.reorder(parentGuid, orderedChildrenGuids);
          */

          yield db.executeCached(`WITH sorting(g, p) AS (
            VALUES ${children.map(
              (child, i) => `("${child.guid}", ${i})`
            ).join()}
          ) UPDATE moz_bookmarks SET position = (
            SELECT p FROM sorting WHERE g = guid
          ) WHERE parent = (
            SELECT id FROM moz_bookmarks WHERE guid = :parentGuid
          )`,
          { parentGuid });
        });

        // Notify observers.
        let observers = PlacesUtils.bookmarks.getObservers();
        for (let child of children) {
          notify(observers, "onItemMoved", [ child.id, child.parentId,
                                             child.oldIndex, child.parentId,
                                             child.index, child.type,
                                             child.guid, parentGuid,
                                             parentGuid, SOURCE_SYNC ]);
        }
      })
    );
  }),

  /**
   * Removes an item from the database.
   */
  remove: Task.async(function* (guid) {
    return PlacesUtils.bookmarks.remove(guid, {
      source: SOURCE_SYNC,
    });
  }),

  /**
   * Removes a folder's children. This is a temporary method that can be
   * replaced by `eraseEverything` once Places supports the Sync-specific
   * mobile root.
   */
  clear: Task.async(function* (folderGuid) {
    let folderId = yield PlacesUtils.promiseItemId(folderGuid);
    PlacesUtils.bookmarks.removeFolderChildren(folderId, SOURCE_SYNC);
  }),

  /**
   * Ensures an item with the |itemId| has a GUID, assigning one if necessary.
   * We should never have a bookmark without a GUID, but the old Sync bookmarks
   * engine code does this, so we'll match its behavior until we're sure it's
   * not needed.
   *
   * This method can be removed and replaced with `PlacesUtils.promiseItemGuid`
   * once bug 1294291 lands.
   *
   * @return {Promise} resolved once the GUID has been updated.
   * @resolves to the existing or new GUID.
   * @rejects if the item does not exist.
   */
  ensureGuidForId: Task.async(function* (itemId) {
    let guid;
    try {
      // Use the existing GUID if it exists. `promiseItemGuid` caches the GUID
      // as a side effect, and throws if it's invalid.
      guid = yield PlacesUtils.promiseItemGuid(itemId);
    } catch (ex) {
      BookmarkSyncLog.warn(`ensureGuidForId: Error fetching GUID for ${
        itemId}`, ex);
      if (!isInvalidCachedGuidError(ex)) {
        throw ex;
      }
      // Give the item a GUID if it doesn't have one.
      guid = yield PlacesUtils.withConnectionWrapper(
        "BookmarkSyncUtils: ensureGuidForId", Task.async(function* (db) {
          let guid = yield generateGuid(db);
          BookmarkSyncLog.warn(`ensureGuidForId: Assigning ${
            guid} to item without GUID ${itemId}`);
          return setGuid(db, itemId, guid);
        })
      );

    }
    return guid;
  }),

  /**
   * Changes the GUID of an existing item.
   *
   * @return {Promise} resolved once the GUID has been changed.
   * @resolves to the new GUID.
   * @rejects if the old GUID does not exist.
   */
  changeGuid: Task.async(function* (oldGuid, newGuid) {
    PlacesUtils.SYNC_BOOKMARK_VALIDATORS.guid(oldGuid);

    let itemId = yield PlacesUtils.promiseItemId(oldGuid);
    if (PlacesUtils.isRootItem(itemId)) {
      throw new Error(`Cannot change GUID of Places root ${oldGuid}`);
    }
    return PlacesUtils.withConnectionWrapper("BookmarkSyncUtils: changeGuid",
      db => setGuid(db, itemId, newGuid));
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
      { guid: { required: true }
      , type: { validIf: () => false }
      , index: { validIf: () => false }
      , source: { validIf: () => false }
      });
    updateInfo.source = SOURCE_SYNC;

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

function findChildByGuid(children, guid) {
  return children.find(child => child.guid == guid);
}

function findChildByIndex(children, index) {
  return children.find(child => child.index == index);
}

// Sets a child record's index and updates its sibling's indices.
function updateChildIndex(children, child, newIndex) {
  let siblings = [];
  let lowIndex = Math.min(child.index, newIndex);
  let highIndex = Math.max(child.index, newIndex);
  for (; lowIndex < highIndex; ++lowIndex) {
    let sibling = findChildByIndex(children, lowIndex);
    siblings.push(sibling);
  }

  let sign = newIndex < child.index ? +1 : -1;
  for (let sibling of siblings) {
    sibling.index += sign;
  }
  child.index = newIndex;
}

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

function isInvalidCachedGuidError(error) {
  return error && error.message ==
    "Trying to update the GUIDs cache with an invalid GUID";
}

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
var updateTagQueryFolder = Task.async(function* (item) {
  if (item.kind != BookmarkSyncUtils.KINDS.QUERY || !item.folder || !item.url ||
      item.url.protocol != "place:") {
    return item;
  }

  let params = new URLSearchParams(item.url.pathname);
  let type = +params.get("type");

  if (type != Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_CONTENTS) {
    return item;
  }

  let id = yield getOrCreateTagFolder(item.folder);
  BookmarkSyncLog.debug(`updateTagQueryFolder: Tag query folder: ${
    item.folder} = ${id}`);

  // Rewrite the query to reference the new ID.
  params.set("folder", id);
  item.url = new URL(item.url.protocol + params);

  return item;
});

var annotateOrphan = Task.async(function* (item, requestedParentGuid) {
  let itemId = yield PlacesUtils.promiseItemId(item.guid);
  PlacesUtils.annotations.setItemAnnotation(itemId,
    PARENT_ANNO, requestedParentGuid, 0,
    PlacesUtils.annotations.EXPIRE_NEVER,
    SOURCE_SYNC);
});

var reparentOrphans = Task.async(function* (item) {
  if (item.type != PlacesUtils.bookmarks.TYPE_FOLDER) {
    return;
  }
  let orphanIds = findAnnoItems(PARENT_ANNO, item.guid);
  // The annotations API returns item IDs, but the asynchronous bookmarks
  // API uses GUIDs. We can remove the `promiseItemGuid` calls and parallel
  // arrays once we implement a GUID-aware annotations API.
  let orphanGuids = yield Promise.all(orphanIds.map(id =>
    PlacesUtils.promiseItemGuid(id)));
  BookmarkSyncLog.debug(`reparentOrphans: Reparenting ${
    JSON.stringify(orphanGuids)} to ${item.guid}`);
  for (let i = 0; i < orphanGuids.length; ++i) {
    let isReparented = false;
    try {
      // Reparenting can fail if we have a corrupted or incomplete tree
      // where an item's parent is one of its descendants.
      BookmarkSyncLog.trace(`reparentOrphans: Attempting to move item ${
        orphanGuids[i]} to new parent ${item.guid}`);
      yield PlacesUtils.bookmarks.update({
        guid: orphanGuids[i],
        parentGuid: item.guid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
        source: SOURCE_SYNC,
      });
      isReparented = true;
    } catch (ex) {
      BookmarkSyncLog.error(`reparentOrphans: Failed to reparent item ${
        orphanGuids[i]} to ${item.guid}`, ex);
    }
    if (isReparented) {
      // Remove the annotation once we've reparented the item.
      PlacesUtils.annotations.removeItemAnnotation(orphanIds[i],
        PARENT_ANNO, SOURCE_SYNC);
    }
  }
});

// Inserts a synced bookmark into the database.
var insertSyncBookmark = Task.async(function* (insertInfo) {
  let requestedParentGuid = insertInfo.parentGuid;
  let isOrphan = yield GUIDMissing(insertInfo.parentGuid);

  // Default to "unfiled" for new bookmarks if the parent doesn't exist.
  if (!isOrphan) {
    BookmarkSyncLog.debug(`insertSyncBookmark: Item ${
      insertInfo.guid} is not an orphan`);
  } else {
    BookmarkSyncLog.debug(`insertSyncBookmark: Item ${
      insertInfo.guid} is an orphan: parent ${
      requestedParentGuid} doesn't exist; reparenting to unfiled`);
    insertInfo.parentGuid = PlacesUtils.bookmarks.unfiledGuid;
  }

  // If we're inserting a tag query, make sure the tag exists and fix the
  // folder ID to refer to the local tag folder.
  insertInfo = yield updateTagQueryFolder(insertInfo);

  let newItem;
  if (insertInfo.kind == BookmarkSyncUtils.KINDS.LIVEMARK) {
    newItem = yield insertSyncLivemark(insertInfo);
  } else {
    let item = yield PlacesUtils.bookmarks.insert(insertInfo);
    let newId = yield PlacesUtils.promiseItemId(item.guid);
    newItem = yield insertBookmarkMetadata(newId, item, insertInfo);
  }

  if (!newItem) {
    return null;
  }

  // If the item is an orphan, annotate it with its real parent ID.
  if (isOrphan) {
    yield annotateOrphan(newItem, requestedParentGuid);
  }

  // Reparent all orphans that expect this folder as the parent.
  yield reparentOrphans(newItem);

  return newItem;
});

// Inserts a synced livemark.
var insertSyncLivemark = Task.async(function* (insertInfo) {
  let parentId = yield PlacesUtils.promiseItemId(insertInfo.parentGuid);
  let parentIsLivemark = PlacesUtils.annotations.itemHasAnnotation(parentId,
    PlacesUtils.LMANNO_FEEDURI);
  if (parentIsLivemark) {
    // A livemark can't be a descendant of another livemark.
    BookmarkSyncLog.debug(`insertSyncLivemark: Invalid parent ${
      insertInfo.parentGuid}; skipping livemark record ${insertInfo.guid}`);
    return null;
  }

  let feedURI = PlacesUtils.toURI(insertInfo.feed);
  let siteURI = insertInfo.site ? PlacesUtils.toURI(insertInfo.site) : null;
  let item = yield PlacesUtils.livemarks.addLivemark({
    title: insertInfo.title,
    parentGuid: insertInfo.parentGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    feedURI,
    siteURI,
    guid: insertInfo.guid,
    source: SOURCE_SYNC,
  });

  return insertBookmarkMetadata(item.id, item, insertInfo);
});

// Sets annotations, keywords, and tags on a new synced bookmark.
var insertBookmarkMetadata = Task.async(function* (itemId, item, insertInfo) {
  if (insertInfo.query) {
    PlacesUtils.annotations.setItemAnnotation(itemId,
      SMART_BOOKMARKS_ANNO, insertInfo.query, 0,
      PlacesUtils.annotations.EXPIRE_NEVER,
      SOURCE_SYNC);
    item.query = insertInfo.query;
  }

  try {
    item.tags = yield tagItem(item, insertInfo.tags);
  } catch (ex) {
    BookmarkSyncLog.warn(`insertBookmarkMetadata: Error tagging item ${
      item.guid}`, ex);
  }

  if (insertInfo.keyword) {
    yield PlacesUtils.keywords.insert({
      keyword: insertInfo.keyword,
      url: item.url.href,
      source: SOURCE_SYNC,
    });
    item.keyword = insertInfo.keyword;
  }

  if (insertInfo.description) {
    PlacesUtils.annotations.setItemAnnotation(itemId,
      DESCRIPTION_ANNO, insertInfo.description, 0,
      PlacesUtils.annotations.EXPIRE_NEVER,
      SOURCE_SYNC);
    item.description = insertInfo.description;
  }

  if (insertInfo.loadInSidebar) {
    PlacesUtils.annotations.setItemAnnotation(itemId,
      SIDEBAR_ANNO, insertInfo.loadInSidebar, 0,
      PlacesUtils.annotations.EXPIRE_NEVER,
      SOURCE_SYNC);
    item.loadInSidebar = insertInfo.loadInSidebar;
  }

  return item;
});

// Determines the Sync record kind for an existing bookmark.
var getKindForItem = Task.async(function* (item) {
  switch (item.type) {
    case PlacesUtils.bookmarks.TYPE_FOLDER: {
      let itemId = yield PlacesUtils.promiseItemId(item.guid);
      let isLivemark = PlacesUtils.annotations.itemHasAnnotation(itemId,
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
  let livemark = yield PlacesUtils.livemarks.getLivemark({
    guid: updateInfo.guid,
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
  let oldItem = yield PlacesUtils.bookmarks.fetch(updateInfo.guid);
  if (!oldItem) {
    throw new Error(`Bookmark with GUID ${updateInfo.guid} does not exist`);
  }

  let shouldReinsert = false;
  let oldKind = yield getKindForItem(oldItem);
  if (updateInfo.hasOwnProperty("kind") && updateInfo.kind != oldKind) {
    // If the item's aren't the same kind, we can't update the record;
    // we must remove and reinsert.
    shouldReinsert = true;
    BookmarkSyncLog.warn(`updateSyncBookmark: Local ${
      oldItem.guid} kind = (${oldKind}); remote ${
      updateInfo.guid} kind = ${updateInfo.kind}. Deleting and recreating`);
  } else if (oldKind == BookmarkSyncUtils.KINDS.LIVEMARK) {
    // Similarly, if we're changing a livemark's site or feed URL, we need to
    // reinsert.
    shouldReinsert = yield shouldReinsertLivemark(updateInfo);
    if (shouldReinsert) {
      BookmarkSyncLog.debug(`updateSyncBookmark: Local ${
        oldItem.guid} and remote ${
        updateInfo.guid} livemarks have different URLs`);
    }
  }
  if (shouldReinsert) {
    delete updateInfo.source;
    let newItem = validateNewBookmark(updateInfo);
    yield PlacesUtils.bookmarks.remove({
      guid: oldItem.guid,
      source: SOURCE_SYNC,
    });
    // A reinsertion likely indicates a confused client, since there aren't
    // public APIs for changing livemark URLs or an item's kind (e.g., turning
    // a folder into a separator while preserving its annos and position).
    // This might be a good case to repair later; for now, we assume Sync has
    // passed a complete record for the new item, and don't try to merge
    // `oldItem` with `updateInfo`.
    return insertSyncBookmark(newItem);
  }

  let isOrphan = false, requestedParentGuid;
  if (updateInfo.hasOwnProperty("parentGuid")) {
    requestedParentGuid = updateInfo.parentGuid;
    if (requestedParentGuid != oldItem.parentGuid) {
      let oldId = yield PlacesUtils.promiseItemId(oldItem.guid);
      if (PlacesUtils.isRootItem(oldId)) {
        throw new Error(`Cannot move Places root ${oldId}`);
      }
      isOrphan = yield GUIDMissing(requestedParentGuid);
      if (!isOrphan) {
        BookmarkSyncLog.debug(`updateSyncBookmark: Item ${
          updateInfo.guid} is not an orphan`);
      } else {
        // Don't move the item if the new parent doesn't exist. Instead, mark
        // the item as an orphan. We'll annotate it with its real parent after
        // updating.
        BookmarkSyncLog.trace(`updateSyncBookmark: Item ${
          updateInfo.guid} is an orphan: could not find parent ${
          requestedParentGuid}`);
        delete updateInfo.parentGuid;
      }
      // If we're reparenting the item, pass the default index so that
      // `PlacesUtils.bookmarks.update` doesn't throw. Sync will reorder
      // children at the end of the sync.
      updateInfo.index = PlacesUtils.bookmarks.DEFAULT_INDEX;
    } else {
      // `PlacesUtils.bookmarks.update` requires us to specify an index if we
      // pass a parent, so we remove the parent if it's the same.
      delete updateInfo.parentGuid;
    }
  }

  updateInfo = yield updateTagQueryFolder(updateInfo);

  let newItem = shouldUpdateBookmark(updateInfo) ?
                yield PlacesUtils.bookmarks.update(updateInfo) : oldItem;
  let itemId = yield PlacesUtils.promiseItemId(newItem.guid);

  newItem = yield updateBookmarkMetadata(itemId, oldItem, newItem, updateInfo);

  // If the item is an orphan, annotate it with its real parent ID.
  if (isOrphan) {
    yield annotateOrphan(newItem, requestedParentGuid);
  }

  // Reparent all orphans that expect this folder as the parent.
  yield reparentOrphans(newItem);

  return newItem;
});

var updateBookmarkMetadata = Task.async(function* (itemId, oldItem, newItem, updateInfo) {
  try {
    newItem.tags = yield tagItem(newItem, updateInfo.tags);
  } catch (ex) {
    BookmarkSyncLog.warn(`updateBookmarkMetadata: Error tagging item ${
      newItem.guid}`, ex);
  }

  if (updateInfo.hasOwnProperty("keyword")) {
    // Unconditionally remove the old keyword.
    let entry = yield PlacesUtils.keywords.fetch({
      url: oldItem.url.href,
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
        DESCRIPTION_ANNO, updateInfo.description, 0,
        PlacesUtils.annotations.EXPIRE_NEVER,
        SOURCE_SYNC);
    } else {
      PlacesUtils.annotations.removeItemAnnotation(itemId,
        DESCRIPTION_ANNO, SOURCE_SYNC);
    }
    newItem.description = updateInfo.description;
  }

  if (updateInfo.hasOwnProperty("loadInSidebar")) {
    if (updateInfo.loadInSidebar) {
      PlacesUtils.annotations.setItemAnnotation(itemId,
        SIDEBAR_ANNO, updateInfo.loadInSidebar, 0,
        PlacesUtils.annotations.EXPIRE_NEVER,
        SOURCE_SYNC);
    } else {
      PlacesUtils.annotations.removeItemAnnotation(itemId,
        SIDEBAR_ANNO, SOURCE_SYNC);
    }
    newItem.loadInSidebar = updateInfo.loadInSidebar;
  }

  if (updateInfo.hasOwnProperty("query")) {
    PlacesUtils.annotations.setItemAnnotation(itemId,
      SMART_BOOKMARKS_ANNO, updateInfo.query, 0,
      PlacesUtils.annotations.EXPIRE_NEVER,
      SOURCE_SYNC);
    newItem.query = updateInfo.query;
  }

  return newItem;
});

function generateGuid(db) {
  return db.executeCached("SELECT GENERATE_GUID() AS guid").then(rows =>
    rows[0].getResultByName("guid"));
}

function setGuids(guids) {
  return PlacesUtils.withConnectionWrapper("BookmarkSyncUtils: setGuids",
    db => db.executeTransaction(function* () {
      let promises = [];
      for (let [itemId, newGuid] of guids) {
        promises.push(setGuid(db, itemId, newGuid));
      }
      return Promise.all(promises);
    })
  );
}

var setGuid = Task.async(function* (db, itemId, newGuid) {
  yield db.executeCached(`UPDATE moz_bookmarks SET guid = :newGuid
    WHERE id = :itemId`, { newGuid, itemId });
  PlacesUtils.invalidateCachedGuidFor(itemId);
  return newGuid;
});

function validateNewBookmark(info) {
  let insertInfo = validateSyncBookmarkObject(info,
    { kind: { required: true }
    // Explicitly prevent callers from passing types.
    , type: { validIf: () => false }
    // Because Sync applies bookmarks as it receives them, it doesn't pass
    // an index. Instead, Sync calls `BookmarkSyncUtils.order` at the end of
    // the sync, which orders children according to their placement in the
    // `BookmarkFolder::children` array.
    , index: { validIf: () => false }
    // This module always uses `nsINavBookmarksService::SOURCE_SYNC`.
    , source: { validIf: () => false }
    , guid: { required: true }
    , url: { requiredIf: b => [ BookmarkSyncUtils.KINDS.BOOKMARK
                              , BookmarkSyncUtils.KINDS.MICROSUMMARY
                              , BookmarkSyncUtils.KINDS.QUERY ].includes(b.kind)
           , validIf: b => [ BookmarkSyncUtils.KINDS.BOOKMARK
                           , BookmarkSyncUtils.KINDS.MICROSUMMARY
                           , BookmarkSyncUtils.KINDS.QUERY ].includes(b.kind) }
    , parentGuid: { required: true }
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
                                     , BookmarkSyncUtils.KINDS.MICROSUMMARY ].includes(b.kind) }
    , feed: { requiredIf: b => b.kind == BookmarkSyncUtils.KINDS.LIVEMARK
            , validIf: b => b.kind == BookmarkSyncUtils.KINDS.LIVEMARK }
    , site: { validIf: b => b.kind == BookmarkSyncUtils.KINDS.LIVEMARK }
    });

  // Sync doesn't track modification times, so use the default.
  let time = new Date();
  insertInfo.dateAdded = insertInfo.lastModified = time;

  insertInfo.type = getTypeForKind(insertInfo.kind);
  insertInfo.source = SOURCE_SYNC;

  return insertInfo;
}

function findAnnoItems(anno, val) {
  let annos = PlacesUtils.annotations;
  return annos.getItemsWithAnnotation(anno, {}).filter(id =>
    annos.getItemAnnotation(id, anno) == val);
}

var tagItem = Task.async(function (item, tags) {
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
// but doesn't know about additional Sync record properties. We check this to
// avoid having it throw in case we only pass Sync-specific properties, like
// `{ guid, tags }`.
function shouldUpdateBookmark(updateInfo) {
  let propsToUpdate = 0;
  for (let prop in PlacesUtils.BOOKMARK_VALIDATORS) {
    if (!updateInfo.hasOwnProperty(prop)) {
      continue;
    }
    // We should have at least one more property, in addition to `guid` and
    // `source`.
    if (++propsToUpdate >= 3) {
      return true;
    }
  }
  return false;
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
  });
  return PlacesUtils.promiseItemId(item.guid);
});
