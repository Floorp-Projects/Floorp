/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test preventive maintenance
 * For every maintenance query create an uncoherent db and check that we take
 * correct fix steps, without polluting valid data.
 */

// Get services and database connection
var hs = PlacesUtils.history;
var bs = PlacesUtils.bookmarks;
var ts = PlacesUtils.tagging;
var fs = PlacesUtils.favicons;

var mDBConn = hs.DBConnection;
var gUnfiledFolderId;
// ------------------------------------------------------------------------------
// Helpers

var defaultBookmarksMaxId = 0;
async function cleanDatabase() {
  // First clear any bookmarks the "proper way" to ensure caches like GuidHelper
  // are properly cleared.
  await PlacesUtils.bookmarks.eraseEverything();
  mDBConn.executeSimpleSQL("DELETE FROM moz_places");
  mDBConn.executeSimpleSQL("DELETE FROM moz_origins");
  mDBConn.executeSimpleSQL("DELETE FROM moz_historyvisits");
  mDBConn.executeSimpleSQL("DELETE FROM moz_anno_attributes");
  mDBConn.executeSimpleSQL("DELETE FROM moz_annos");
  mDBConn.executeSimpleSQL("DELETE FROM moz_items_annos");
  mDBConn.executeSimpleSQL("DELETE FROM moz_inputhistory");
  mDBConn.executeSimpleSQL("DELETE FROM moz_keywords");
  mDBConn.executeSimpleSQL("DELETE FROM moz_icons");
  mDBConn.executeSimpleSQL("DELETE FROM moz_pages_w_icons");
  mDBConn.executeSimpleSQL(
    "DELETE FROM moz_bookmarks WHERE id > " + defaultBookmarksMaxId
  );
  mDBConn.executeSimpleSQL("DELETE FROM moz_bookmarks_deleted");
  mDBConn.executeSimpleSQL("DELETE FROM moz_places_metadata_search_queries");
  mDBConn.executeSimpleSQL("DELETE FROM moz_places_metadata_snapshots");
  mDBConn.executeSimpleSQL("DELETE FROM moz_places_metadata_snapshots_extra");
  mDBConn.executeSimpleSQL("DELETE FROM moz_places_metadata_snapshots_groups");
  mDBConn.executeSimpleSQL(
    "DELETE FROM moz_places_metadata_groups_to_snapshots"
  );
  mDBConn.executeSimpleSQL("DELETE FROM moz_session_metadata");
  mDBConn.executeSimpleSQL("DELETE FROM moz_session_to_places");
}

function addPlace(
  aUrl,
  aFavicon,
  aGuid = PlacesUtils.history.makeGuid(),
  aHash = null
) {
  let href = new URL(
    aUrl || `http://www.mozilla.org/${encodeURIComponent(aGuid)}`
  ).href;
  let stmt = mDBConn.createStatement(
    "INSERT INTO moz_places (url, url_hash, guid) VALUES (:url, IFNULL(:hash, hash(:url)), :guid)"
  );
  stmt.params.url = href;
  stmt.params.hash = aHash;
  stmt.params.guid = aGuid;
  stmt.execute();
  stmt.finalize();
  stmt = mDBConn.createStatement("DELETE FROM moz_updateoriginsinsert_temp");
  stmt.execute();
  stmt.finalize();
  let id = mDBConn.lastInsertRowID;
  if (aFavicon) {
    stmt = mDBConn.createStatement(
      "INSERT INTO moz_pages_w_icons (page_url, page_url_hash) VALUES (:url, IFNULL(:hash, hash(:url)))"
    );
    stmt.params.url = href;
    stmt.params.hash = aHash;
    stmt.execute();
    stmt.finalize();
    stmt = mDBConn.createStatement(
      "INSERT INTO moz_icons_to_pages (page_id, icon_id) " +
        "VALUES ((SELECT id FROM moz_pages_w_icons WHERE page_url_hash = IFNULL(:hash, hash(:url))), :favicon)"
    );
    stmt.params.url = href;
    stmt.params.hash = aHash;
    stmt.params.favicon = aFavicon;
    stmt.execute();
    stmt.finalize();
  }
  return id;
}

function addBookmark(
  aPlaceId,
  aType,
  aParent,
  aKeywordId,
  aFolderType,
  aTitle,
  aGuid = PlacesUtils.history.makeGuid(),
  aSyncStatus = PlacesUtils.bookmarks.SYNC_STATUS.NEW,
  aSyncChangeCounter = 0
) {
  let stmt = mDBConn.createStatement(
    `INSERT INTO moz_bookmarks (fk, type, parent, keyword_id, folder_type,
                                title, guid, syncStatus, syncChangeCounter)
     VALUES (:place_id, :type, :parent, :keyword_id, :folder_type, :title,
             :guid, :sync_status, :change_counter)`
  );
  stmt.params.place_id = aPlaceId || null;
  stmt.params.type = aType || null;
  stmt.params.parent = aParent || gUnfiledFolderId;
  stmt.params.keyword_id = aKeywordId || null;
  stmt.params.folder_type = aFolderType || null;
  stmt.params.title = typeof aTitle == "string" ? aTitle : null;
  stmt.params.guid = aGuid;
  stmt.params.sync_status = aSyncStatus;
  stmt.params.change_counter = aSyncChangeCounter;
  stmt.execute();
  stmt.finalize();
  return mDBConn.lastInsertRowID;
}

// ------------------------------------------------------------------------------
// Tests

var tests = [];

// ------------------------------------------------------------------------------

tests.push({
  name: "A.1",
  desc: "Remove obsolete annotations from moz_annos",

  _obsoleteWeaveAttribute: "weave/test",
  _placeId: null,

  setup() {
    // Add a place to ensure place_id = 1 is valid.
    this._placeId = addPlace();
    // Add an obsolete attribute.
    let stmt = mDBConn.createStatement(
      "INSERT INTO moz_anno_attributes (name) VALUES (:anno)"
    );
    stmt.params.anno = this._obsoleteWeaveAttribute;
    stmt.execute();
    stmt.finalize();
    stmt = mDBConn.createStatement(
      `INSERT INTO moz_annos (place_id, anno_attribute_id)
       VALUES (:place_id,
         (SELECT id FROM moz_anno_attributes WHERE name = :anno)
       )`
    );
    stmt.params.place_id = this._placeId;
    stmt.params.anno = this._obsoleteWeaveAttribute;
    stmt.execute();
    stmt.finalize();
  },

  check() {
    // Check that the obsolete annotation has been removed.
    let stmt = mDBConn.createStatement(
      "SELECT id FROM moz_anno_attributes WHERE name = :anno"
    );
    stmt.params.anno = this._obsoleteWeaveAttribute;
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  },
});

tests.push({
  name: "A.2",
  desc: "Remove obsolete annotations from moz_items_annos",

  _obsoleteSyncAttribute: "sync/children",
  _obsoleteGuidAttribute: "placesInternal/GUID",
  _obsoleteWeaveAttribute: "weave/test",
  _placeId: null,
  _bookmarkId: null,

  setup() {
    // Add a place to ensure place_id = 1 is valid.
    this._placeId = addPlace();
    // Add a bookmark.
    this._bookmarkId = addBookmark(this._placeId, bs.TYPE_BOOKMARK);
    // Add an obsolete attribute.
    let stmt = mDBConn.createStatement(
      `INSERT INTO moz_anno_attributes (name)
       VALUES (:anno1), (:anno2), (:anno3)`
    );
    stmt.params.anno1 = this._obsoleteSyncAttribute;
    stmt.params.anno2 = this._obsoleteGuidAttribute;
    stmt.params.anno3 = this._obsoleteWeaveAttribute;
    stmt.execute();
    stmt.finalize();
    stmt = mDBConn.createStatement(
      `INSERT INTO moz_items_annos (item_id, anno_attribute_id)
       SELECT :item_id, id
       FROM moz_anno_attributes
       WHERE name IN (:anno1, :anno2, :anno3)`
    );
    stmt.params.item_id = this._bookmarkId;
    stmt.params.anno1 = this._obsoleteSyncAttribute;
    stmt.params.anno2 = this._obsoleteGuidAttribute;
    stmt.params.anno3 = this._obsoleteWeaveAttribute;
    stmt.execute();
    stmt.finalize();
  },

  check() {
    // Check that the obsolete annotations have been removed.
    let stmt = mDBConn.createStatement(
      `SELECT id FROM moz_anno_attributes
       WHERE name IN (:anno1, :anno2, :anno3)`
    );
    stmt.params.anno1 = this._obsoleteSyncAttribute;
    stmt.params.anno2 = this._obsoleteGuidAttribute;
    stmt.params.anno3 = this._obsoleteWeaveAttribute;
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  },
});

tests.push({
  name: "A.3",
  desc: "Remove unused attributes",

  _usedPageAttribute: "usedPage",
  _usedItemAttribute: "usedItem",
  _unusedAttribute: "unused",
  _placeId: null,
  _bookmarkId: null,

  setup() {
    // Add a place to ensure place_id = 1 is valid
    this._placeId = addPlace();
    // add a bookmark
    this._bookmarkId = addBookmark(this._placeId, bs.TYPE_BOOKMARK);
    // Add a used attribute and an unused one.
    let stmt = mDBConn.createStatement(
      "INSERT INTO moz_anno_attributes (name) VALUES (:anno)"
    );
    stmt.params.anno = this._usedPageAttribute;
    stmt.execute();
    stmt.reset();
    stmt.params.anno = this._usedItemAttribute;
    stmt.execute();
    stmt.reset();
    stmt.params.anno = this._unusedAttribute;
    stmt.execute();
    stmt.finalize();

    stmt = mDBConn.createStatement(
      "INSERT INTO moz_annos (place_id, anno_attribute_id) VALUES(:place_id, (SELECT id FROM moz_anno_attributes WHERE name = :anno))"
    );
    stmt.params.place_id = this._placeId;
    stmt.params.anno = this._usedPageAttribute;
    stmt.execute();
    stmt.finalize();
    stmt = mDBConn.createStatement(
      "INSERT INTO moz_items_annos (item_id, anno_attribute_id) VALUES(:item_id, (SELECT id FROM moz_anno_attributes WHERE name = :anno))"
    );
    stmt.params.item_id = this._bookmarkId;
    stmt.params.anno = this._usedItemAttribute;
    stmt.execute();
    stmt.finalize();
  },

  check() {
    // Check that used attributes are still there
    let stmt = mDBConn.createStatement(
      "SELECT id FROM moz_anno_attributes WHERE name = :anno"
    );
    stmt.params.anno = this._usedPageAttribute;
    Assert.ok(stmt.executeStep());
    stmt.reset();
    stmt.params.anno = this._usedItemAttribute;
    Assert.ok(stmt.executeStep());
    stmt.reset();
    // Check that unused attribute has been removed
    stmt.params.anno = this._unusedAttribute;
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "B.1",
  desc: "Remove annotations with an invalid attribute",

  _usedPageAttribute: "usedPage",
  _placeId: null,

  setup() {
    // Add a place to ensure place_id = 1 is valid
    this._placeId = addPlace();
    // Add a used attribute.
    let stmt = mDBConn.createStatement(
      "INSERT INTO moz_anno_attributes (name) VALUES (:anno)"
    );
    stmt.params.anno = this._usedPageAttribute;
    stmt.execute();
    stmt.finalize();
    stmt = mDBConn.createStatement(
      "INSERT INTO moz_annos (place_id, anno_attribute_id) VALUES(:place_id, (SELECT id FROM moz_anno_attributes WHERE name = :anno))"
    );
    stmt.params.place_id = this._placeId;
    stmt.params.anno = this._usedPageAttribute;
    stmt.execute();
    stmt.finalize();
    // Add an annotation with a nonexistent attribute
    stmt = mDBConn.createStatement(
      "INSERT INTO moz_annos (place_id, anno_attribute_id) VALUES(:place_id, 1337)"
    );
    stmt.params.place_id = this._placeId;
    stmt.execute();
    stmt.finalize();
  },

  check() {
    // Check that used attribute is still there
    let stmt = mDBConn.createStatement(
      "SELECT id FROM moz_anno_attributes WHERE name = :anno"
    );
    stmt.params.anno = this._usedPageAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // check that annotation with valid attribute is still there
    stmt = mDBConn.createStatement(
      "SELECT id FROM moz_annos WHERE anno_attribute_id = (SELECT id FROM moz_anno_attributes WHERE name = :anno)"
    );
    stmt.params.anno = this._usedPageAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // Check that annotation with bogus attribute has been removed
    stmt = mDBConn.createStatement(
      "SELECT id FROM moz_annos WHERE anno_attribute_id = 1337"
    );
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "B.2",
  desc: "Remove orphan page annotations",

  _usedPageAttribute: "usedPage",
  _placeId: null,

  setup() {
    // Add a place to ensure place_id = 1 is valid
    this._placeId = addPlace();
    // Add a used attribute.
    let stmt = mDBConn.createStatement(
      "INSERT INTO moz_anno_attributes (name) VALUES (:anno)"
    );
    stmt.params.anno = this._usedPageAttribute;
    stmt.execute();
    stmt.finalize();
    stmt = mDBConn.createStatement(
      "INSERT INTO moz_annos (place_id, anno_attribute_id) VALUES(:place_id, (SELECT id FROM moz_anno_attributes WHERE name = :anno))"
    );
    stmt.params.place_id = this._placeId;
    stmt.params.anno = this._usedPageAttribute;
    stmt.execute();
    stmt.reset();
    // Add an annotation to a nonexistent page
    stmt.params.place_id = 1337;
    stmt.params.anno = this._usedPageAttribute;
    stmt.execute();
    stmt.finalize();
  },

  check() {
    // Check that used attribute is still there
    let stmt = mDBConn.createStatement(
      "SELECT id FROM moz_anno_attributes WHERE name = :anno"
    );
    stmt.params.anno = this._usedPageAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // check that annotation with valid attribute is still there
    stmt = mDBConn.createStatement(
      "SELECT id FROM moz_annos WHERE anno_attribute_id = (SELECT id FROM moz_anno_attributes WHERE name = :anno)"
    );
    stmt.params.anno = this._usedPageAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // Check that an annotation to a nonexistent page has been removed
    stmt = mDBConn.createStatement(
      "SELECT id FROM moz_annos WHERE place_id = 1337"
    );
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.9",
  desc: "Remove items without a valid place",

  _validItemId: null,
  _invalidItemId: null,
  _invalidSyncedItemId: null,
  placeId: null,

  _changeCounterStmt: null,
  _menuChangeCounter: -1,

  setup() {
    // Add a place to ensure place_id = 1 is valid
    this.placeId = addPlace();
    // Insert a valid bookmark
    this._validItemId = addBookmark(this.placeId, bs.TYPE_BOOKMARK);
    // Insert a bookmark with an invalid place
    this._invalidItemId = addBookmark(1337, bs.TYPE_BOOKMARK);
    // Insert a synced bookmark with an invalid place. We should write a
    // tombstone when we remove it.
    this._invalidSyncedItemId = addBookmark(
      1337,
      bs.TYPE_BOOKMARK,
      bs.bookmarksMenuFolder,
      null,
      null,
      null,
      "bookmarkAAAA",
      PlacesUtils.bookmarks.SYNC_STATUS.NORMAL
    );
    // Insert a folder referencing a nonexistent place ID. D.5 should convert
    // it to a bookmark; D.9 should remove it.
    this._invalidWrongTypeItemId = addBookmark(1337, bs.TYPE_FOLDER);

    this._changeCounterStmt = mDBConn.createStatement(`
      SELECT syncChangeCounter FROM moz_bookmarks WHERE id = :id`);
    this._changeCounterStmt.params.id = bs.bookmarksMenuFolder;
    Assert.ok(this._changeCounterStmt.executeStep());
    this._menuChangeCounter = this._changeCounterStmt.row.syncChangeCounter;
    this._changeCounterStmt.reset();
  },

  async check() {
    // Check that valid bookmark is still there
    let stmt = mDBConn.createStatement(
      "SELECT id FROM moz_bookmarks WHERE id = :item_id"
    );
    stmt.params.item_id = this._validItemId;
    Assert.ok(stmt.executeStep());
    stmt.reset();
    // Check that invalid bookmarks have been removed
    for (let id of [
      this._invalidItemId,
      this._invalidSyncedItemId,
      this._invalidWrongTypeItemId,
    ]) {
      stmt.params.item_id = id;
      Assert.ok(!stmt.executeStep());
      stmt.reset();
    }
    stmt.finalize();

    this._changeCounterStmt.params.id = bs.bookmarksMenuFolder;
    Assert.ok(this._changeCounterStmt.executeStep());
    Assert.equal(
      this._changeCounterStmt.row.syncChangeCounter,
      this._menuChangeCounter + 1
    );
    this._changeCounterStmt.finalize();

    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    Assert.deepEqual(
      tombstones.map(info => info.guid),
      ["bookmarkAAAA"]
    );
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.1",
  desc: "Remove items that are not uri bookmarks from tag containers",

  _tagId: null,
  _bookmarkId: null,
  _separatorId: null,
  _folderId: null,
  _placeId: null,

  setup() {
    // Add a place to ensure place_id = 1 is valid
    this._placeId = addPlace();
    // Create a tag
    this._tagId = addBookmark(null, bs.TYPE_FOLDER, bs.tagsFolder);
    // Insert a bookmark in the tag
    this._bookmarkId = addBookmark(
      this._placeId,
      bs.TYPE_BOOKMARK,
      this._tagId
    );
    // Insert a separator in the tag
    this._separatorId = addBookmark(null, bs.TYPE_SEPARATOR, this._tagId);
    // Insert a folder in the tag
    this._folderId = addBookmark(null, bs.TYPE_FOLDER, this._tagId);
  },

  check() {
    // Check that valid bookmark is still there
    let stmt = mDBConn.createStatement(
      "SELECT id FROM moz_bookmarks WHERE type = :type AND parent = :parent"
    );
    stmt.params.type = bs.TYPE_BOOKMARK;
    stmt.params.parent = this._tagId;
    Assert.ok(stmt.executeStep());
    stmt.reset();
    // Check that separator is no more there
    stmt.params.type = bs.TYPE_SEPARATOR;
    stmt.params.parent = this._tagId;
    Assert.ok(!stmt.executeStep());
    stmt.reset();
    // Check that folder is no more there
    stmt.params.type = bs.TYPE_FOLDER;
    stmt.params.parent = this._tagId;
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.2",
  desc: "Remove empty tags",

  _tagId: null,
  _bookmarkId: null,
  _emptyTagId: null,
  _placeId: null,

  setup() {
    // Add a place to ensure place_id = 1 is valid
    this._placeId = addPlace();
    // Create a tag
    this._tagId = addBookmark(null, bs.TYPE_FOLDER, bs.tagsFolder);
    // Insert a bookmark in the tag
    this._bookmarkId = addBookmark(
      this._placeId,
      bs.TYPE_BOOKMARK,
      this._tagId
    );
    // Create another tag (empty)
    this._emptyTagId = addBookmark(null, bs.TYPE_FOLDER, bs.tagsFolder);
  },

  check() {
    // Check that valid bookmark is still there
    let stmt = mDBConn.createStatement(
      "SELECT id FROM moz_bookmarks WHERE id = :id AND type = :type AND parent = :parent"
    );
    stmt.params.id = this._bookmarkId;
    stmt.params.type = bs.TYPE_BOOKMARK;
    stmt.params.parent = this._tagId;
    Assert.ok(stmt.executeStep());
    stmt.reset();
    stmt.params.id = this._tagId;
    stmt.params.type = bs.TYPE_FOLDER;
    stmt.params.parent = bs.tagsFolder;
    Assert.ok(stmt.executeStep());
    stmt.reset();
    stmt.params.id = this._emptyTagId;
    stmt.params.type = bs.TYPE_FOLDER;
    stmt.params.parent = bs.tagsFolder;
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.3",
  desc: "Move orphan items to unsorted folder",

  _orphanBookmarkId: null,
  _orphanSeparatorId: null,
  _orphanFolderId: null,
  _bookmarkId: null,
  _placeId: null,

  async setup() {
    // Add a place to ensure place_id = 1 is valid
    this._placeId = addPlace();
    // Insert an orphan bookmark
    this._orphanBookmarkId = addBookmark(this._placeId, bs.TYPE_BOOKMARK, 8888);
    // Insert an orphan separator
    this._orphanSeparatorId = addBookmark(null, bs.TYPE_SEPARATOR, 8888);
    // Insert a orphan folder
    this._orphanFolderId = addBookmark(null, bs.TYPE_FOLDER, 8888);
    // Create a child of the last created folder
    this._bookmarkId = addBookmark(
      this._placeId,
      bs.TYPE_BOOKMARK,
      this._orphanFolderId
    );
  },

  async check() {
    // Check that bookmarks are now children of a real folder (unfiled)
    let expectedInfos = [
      {
        id: this._orphanBookmarkId,
        parent: gUnfiledFolderId,
        syncChangeCounter: 1,
      },
      {
        id: this._orphanSeparatorId,
        parent: gUnfiledFolderId,
        syncChangeCounter: 1,
      },
      {
        id: this._orphanFolderId,
        parent: gUnfiledFolderId,
        syncChangeCounter: 1,
      },
      {
        id: this._bookmarkId,
        parent: this._orphanFolderId,
        syncChangeCounter: 0,
      },
      {
        id: gUnfiledFolderId,
        parent: bs.placesRoot,
        syncChangeCounter: 3,
      },
    ];
    let db = await PlacesUtils.promiseDBConnection();
    for (let { id, parent, syncChangeCounter } of expectedInfos) {
      let rows = await db.executeCached(
        `
        SELECT id, syncChangeCounter
        FROM moz_bookmarks
        WHERE id = :item_id AND parent = :parent`,
        { item_id: id, parent }
      );
      Assert.equal(rows.length, 1);

      let actualChangeCounter = rows[0].getResultByName("syncChangeCounter");
      Assert.equal(actualChangeCounter, syncChangeCounter);
    }
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.5",
  desc: "Fix wrong item types | folders and separators",

  _separatorId: null,
  _separatorGuid: null,
  _folderId: null,
  _folderGuid: null,
  _syncedFolderId: null,
  _syncedFolderGuid: null,
  _placeId: null,

  async setup() {
    // Add a place to ensure place_id = 1 is valid
    this._placeId = addPlace();
    // Add a separator with a fk
    this._separatorId = addBookmark(this._placeId, bs.TYPE_SEPARATOR);
    this._separatorGuid = await PlacesUtils.promiseItemGuid(this._separatorId);
    // Add a folder with a fk
    this._folderId = addBookmark(this._placeId, bs.TYPE_FOLDER);
    this._folderGuid = await PlacesUtils.promiseItemGuid(this._folderId);
    // Add a synced folder with a fk
    this._syncedFolderId = addBookmark(
      this._placeId,
      bs.TYPE_FOLDER,
      bs.toolbarFolder,
      null,
      null,
      null,
      "itemAAAAAAAA",
      PlacesUtils.bookmarks.SYNC_STATUS.NORMAL
    );
    this._syncedFolderGuid = await PlacesUtils.promiseItemGuid(
      this._syncedFolderId
    );
  },

  async check() {
    // Check that items with an fk have been converted to bookmarks
    let stmt = mDBConn.createStatement(
      `SELECT id, guid, syncChangeCounter
       FROM moz_bookmarks
       WHERE id = :item_id AND type = :type`
    );
    stmt.params.item_id = this._separatorId;
    stmt.params.type = bs.TYPE_BOOKMARK;
    Assert.ok(stmt.executeStep());
    stmt.reset();
    let expected = [
      {
        id: this._folderId,
        oldGuid: this._folderGuid,
      },
      {
        id: this._syncedFolderId,
        oldGuid: this._syncedFolderGuid,
      },
    ];
    for (let { id, oldGuid } of expected) {
      stmt.params.item_id = id;
      stmt.params.type = bs.TYPE_BOOKMARK;
      Assert.ok(stmt.executeStep());
      Assert.notEqual(stmt.row.guid, oldGuid);
      Assert.equal(stmt.row.syncChangeCounter, 1);
      await Assert.rejects(
        PlacesUtils.promiseItemId(oldGuid),
        /no item found for the given GUID/
      );
      stmt.reset();
    }
    stmt.finalize();

    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    Assert.deepEqual(
      tombstones.map(info => info.guid),
      ["itemAAAAAAAA"]
    );
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.6",
  desc: "Fix wrong item types | bookmarks",

  _validBookmarkId: null,
  _validBookmarkGuid: null,
  _invalidBookmarkId: null,
  _invalidBookmarkGuid: null,
  _invalidSyncedBookmarkId: null,
  _invalidSyncedBookmarkGuid: null,
  _placeId: null,

  async setup() {
    // Add a place to ensure place_id = 1 is valid
    this._placeId = addPlace();
    // Add a bookmark with a valid place id
    this._validBookmarkId = addBookmark(this._placeId, bs.TYPE_BOOKMARK);
    this._validBookmarkGuid = await PlacesUtils.promiseItemGuid(
      this._validBookmarkId
    );
    // Add a bookmark with a null place id
    this._invalidBookmarkId = addBookmark(null, bs.TYPE_BOOKMARK);
    this._invalidBookmarkGuid = await PlacesUtils.promiseItemGuid(
      this._invalidBookmarkId
    );
    // Add a synced bookmark with a null place id
    this._invalidSyncedBookmarkId = addBookmark(
      null,
      bs.TYPE_BOOKMARK,
      bs.toolbarFolder,
      null,
      null,
      null,
      "bookmarkAAAA",
      PlacesUtils.bookmarks.SYNC_STATUS.NORMAL
    );
    this._invalidSyncedBookmarkGuid = await PlacesUtils.promiseItemGuid(
      this._invalidSyncedBookmarkId
    );
  },

  async check() {
    // Check valid bookmark
    let stmt = mDBConn.createStatement(`
      SELECT id, guid, syncChangeCounter
      FROM moz_bookmarks
      WHERE id = :item_id AND type = :type`);
    stmt.params.item_id = this._validBookmarkId;
    stmt.params.type = bs.TYPE_BOOKMARK;
    Assert.ok(stmt.executeStep());
    Assert.equal(stmt.row.syncChangeCounter, 0);
    Assert.equal(
      await PlacesUtils.promiseItemId(this._validBookmarkGuid),
      this._validBookmarkId
    );
    stmt.reset();

    // Check invalid bookmarks have been converted to folders
    let expected = [
      {
        id: this._invalidBookmarkId,
        oldGuid: this._invalidBookmarkGuid,
      },
      {
        id: this._invalidSyncedBookmarkId,
        oldGuid: this._invalidSyncedBookmarkGuid,
      },
    ];
    for (let { id, oldGuid } of expected) {
      stmt.params.item_id = id;
      stmt.params.type = bs.TYPE_FOLDER;
      Assert.ok(stmt.executeStep());
      Assert.notEqual(stmt.row.guid, oldGuid);
      Assert.equal(stmt.row.syncChangeCounter, 1);
      await Assert.rejects(
        PlacesUtils.promiseItemId(oldGuid),
        /no item found for the given GUID/
      );
      stmt.reset();
    }
    stmt.finalize();

    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    Assert.deepEqual(
      tombstones.map(info => info.guid),
      ["bookmarkAAAA"]
    );
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.7",
  desc: "Fix missing item types",

  _placeId: null,
  _bookmarkId: null,
  _bookmarkGuid: null,
  _syncedBookmarkId: null,
  _syncedBookmarkGuid: null,
  _folderId: null,
  _folderGuid: null,
  _syncedFolderId: null,
  _syncedFolderGuid: null,

  async setup() {
    // Item without a type but with a place ID; should be converted to a
    // bookmark. The synced bookmark should be handled the same way, but with
    // a tombstone.
    this._placeId = addPlace();
    this._bookmarkId = addBookmark(this._placeId);
    this._bookmarkGuid = await PlacesUtils.promiseItemGuid(this._bookmarkId);
    this._syncedBookmarkId = addBookmark(
      this._placeId,
      null,
      bs.toolbarFolder,
      null,
      null,
      null,
      "bookmarkAAAA",
      PlacesUtils.bookmarks.SYNC_STATUS.NORMAL
    );
    this._syncedBookmarkGuid = await PlacesUtils.promiseItemGuid(
      this._syncedBookmarkId
    );

    // Item without a type and without a place ID; should be converted to a
    // folder.
    this._folderId = addBookmark();
    this._folderGuid = await PlacesUtils.promiseItemGuid(this._folderId);
    this._syncedFolderId = addBookmark(
      null,
      null,
      bs.toolbarFolder,
      null,
      null,
      null,
      "folderBBBBBB",
      PlacesUtils.bookmarks.SYNC_STATUS.NORMAL
    );
    this._syncedFolderGuid = await PlacesUtils.promiseItemGuid(
      this._syncedFolderId
    );
  },

  async check() {
    let stmt = mDBConn.createStatement(`
      SELECT id, guid, type, syncChangeCounter
      FROM moz_bookmarks
      WHERE id = :item_id`);
    let expected = [
      {
        id: this._bookmarkId,
        oldGuid: this._bookmarkGuid,
        type: bs.TYPE_BOOKMARK,
        syncChangeCounter: 1,
      },
      {
        id: this._syncedBookmarkId,
        oldGuid: this._syncedBookmarkGuid,
        type: bs.TYPE_BOOKMARK,
        syncChangeCounter: 1,
      },
      {
        id: this._folderId,
        oldGuid: this._folderGuid,
        type: bs.TYPE_FOLDER,
        syncChangeCounter: 1,
      },
      {
        id: this._syncedFolderId,
        oldGuid: this._syncedFolderGuid,
        type: bs.TYPE_FOLDER,
        syncChangeCounter: 1,
      },
    ];
    for (let { id, oldGuid, type, syncChangeCounter } of expected) {
      stmt.params.item_id = id;
      Assert.ok(stmt.executeStep());
      Assert.equal(stmt.row.type, type);
      Assert.equal(stmt.row.syncChangeCounter, syncChangeCounter);
      Assert.notEqual(stmt.row.guid, oldGuid);
      await Assert.rejects(
        PlacesUtils.promiseItemId(oldGuid),
        /no item found for the given GUID/
      );
      stmt.reset();
    }
    stmt.finalize();

    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    Assert.deepEqual(
      tombstones.map(info => info.guid),
      ["bookmarkAAAA", "folderBBBBBB"]
    );
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.8",
  desc: "Fix wrong parents",

  _bookmarkId: null,
  _separatorId: null,
  _bookmarkId1: null,
  _bookmarkId2: null,
  _placeId: null,

  async setup() {
    // Add a place to ensure place_id = 1 is valid
    this._placeId = addPlace();
    // Insert a bookmark
    this._bookmarkId = addBookmark(this._placeId, bs.TYPE_BOOKMARK);
    // Insert a separator
    this._separatorId = addBookmark(null, bs.TYPE_SEPARATOR);
    // Create 3 children of these items
    this._bookmarkId1 = addBookmark(
      this._placeId,
      bs.TYPE_BOOKMARK,
      this._bookmarkId
    );
    this._bookmarkId2 = addBookmark(
      this._placeId,
      bs.TYPE_BOOKMARK,
      this._separatorId
    );
  },

  async check() {
    // Check that bookmarks are now children of a real folder (unfiled)
    let expectedInfos = [
      {
        id: this._bookmarkId1,
        parent: gUnfiledFolderId,
        syncChangeCounter: 1,
      },
      {
        id: this._bookmarkId2,
        parent: gUnfiledFolderId,
        syncChangeCounter: 1,
      },
      {
        id: gUnfiledFolderId,
        parent: bs.placesRoot,
        syncChangeCounter: 2,
      },
    ];
    let db = await PlacesUtils.promiseDBConnection();
    for (let { id, parent, syncChangeCounter } of expectedInfos) {
      let rows = await db.executeCached(
        `
        SELECT id, syncChangeCounter
        FROM moz_bookmarks
        WHERE id = :item_id AND parent = :parent`,
        { item_id: id, parent }
      );
      Assert.equal(rows.length, 1);

      let actualChangeCounter = rows[0].getResultByName("syncChangeCounter");
      Assert.equal(actualChangeCounter, syncChangeCounter);
    }
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.10",
  desc: "Recalculate positions",

  _unfiledBookmarks: [],
  _toolbarBookmarks: [],

  async setup() {
    const NUM_BOOKMARKS = 20;
    let children = [];
    for (let i = 0; i < NUM_BOOKMARKS; i++) {
      children.push({
        title: "testbookmark",
        url: "http://example.com",
      });
    }

    // Add bookmarks to two folders to better perturbate the table.
    await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.unfiledGuid,
      children,
      source: PlacesUtils.bookmarks.SOURCES.SYNC,
    });
    await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.toolbarGuid,
      children,
      source: PlacesUtils.bookmarks.SOURCES.SYNC,
    });

    async function randomize_positions(aParent, aResultArray) {
      let parentId = await PlacesUtils.promiseItemId(aParent);
      let stmt = mDBConn.createStatement(
        `UPDATE moz_bookmarks SET position = :rand
         WHERE id IN (
           SELECT id FROM moz_bookmarks WHERE parent = :parent
           ORDER BY RANDOM() LIMIT 1
         )`
      );
      for (let i = 0; i < NUM_BOOKMARKS / 2; i++) {
        stmt.params.parent = parentId;
        stmt.params.rand = Math.round(Math.random() * (NUM_BOOKMARKS - 1));
        stmt.execute();
        stmt.reset();
      }
      stmt.finalize();

      // Build the expected ordered list of bookmarks.
      stmt = mDBConn.createStatement(
        `SELECT id, position
         FROM moz_bookmarks WHERE parent = :parent
         ORDER BY position ASC, ROWID ASC`
      );
      stmt.params.parent = parentId;
      while (stmt.executeStep()) {
        aResultArray.push(stmt.row.id);
        print(
          stmt.row.id +
            "\t" +
            stmt.row.position +
            "\t" +
            (aResultArray.length - 1)
        );
      }
      stmt.finalize();
    }

    // Set random positions for the added bookmarks.
    await randomize_positions(
      PlacesUtils.bookmarks.unfiledGuid,
      this._unfiledBookmarks
    );
    await randomize_positions(
      PlacesUtils.bookmarks.toolbarGuid,
      this._toolbarBookmarks
    );

    let syncInfos = await PlacesTestUtils.fetchBookmarkSyncFields(
      PlacesUtils.bookmarks.unfiledGuid,
      PlacesUtils.bookmarks.toolbarGuid
    );
    Assert.ok(syncInfos.every(info => info.syncChangeCounter === 0));
  },

  async check() {
    let db = await PlacesUtils.promiseDBConnection();

    async function check_order(aParent, aResultArray) {
      let parentId = await PlacesUtils.promiseItemId(aParent);
      // Build the expected ordered list of bookmarks.
      let childRows = await db.executeCached(
        `SELECT id, position, syncChangeCounter FROM moz_bookmarks
         WHERE parent = :parent
         ORDER BY position ASC`,
        { parent: parentId }
      );
      for (let row of childRows) {
        let id = row.getResultByName("id");
        let position = row.getResultByName("position");
        if (aResultArray.indexOf(id) != position) {
          dump_table("moz_bookmarks");
          do_throw("Unexpected unfiled bookmarks order.");
        }
      }

      let parentRows = await db.executeCached(
        `SELECT syncChangeCounter FROM moz_bookmarks
         WHERE id = :parent`,
        { parent: parentId }
      );
      for (let row of parentRows) {
        let actualChangeCounter = row.getResultByName("syncChangeCounter");
        Assert.ok(actualChangeCounter > 0);
      }
    }

    await check_order(
      PlacesUtils.bookmarks.unfiledGuid,
      this._unfiledBookmarks
    );
    await check_order(
      PlacesUtils.bookmarks.toolbarGuid,
      this._toolbarBookmarks
    );
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.13",
  desc: "Fix empty-named tags",
  _taggedItemIds: {},

  setup() {
    // Add a place to ensure place_id = 1 is valid
    let placeId = addPlace();
    // Create a empty-named tag.
    this._untitledTagId = addBookmark(
      null,
      bs.TYPE_FOLDER,
      bs.tagsFolder,
      null,
      null,
      ""
    );
    // Insert a bookmark in the tag, otherwise it will be removed.
    addBookmark(placeId, bs.TYPE_BOOKMARK, this._untitledTagId);
    // Create a empty-named folder.
    this._untitledFolderId = addBookmark(
      null,
      bs.TYPE_FOLDER,
      bs.toolbarFolder,
      null,
      null,
      ""
    );
    // Create a titled tag.
    this._titledTagId = addBookmark(
      null,
      bs.TYPE_FOLDER,
      bs.tagsFolder,
      null,
      null,
      "titledTag"
    );
    // Insert a bookmark in the tag, otherwise it will be removed.
    addBookmark(placeId, bs.TYPE_BOOKMARK, this._titledTagId);
    // Create a titled folder.
    this._titledFolderId = addBookmark(
      null,
      bs.TYPE_FOLDER,
      bs.toolbarFolder,
      null,
      null,
      "titledFolder"
    );

    // Create two tagged bookmarks in different folders.
    this._taggedItemIds.inMenu = addBookmark(
      placeId,
      bs.TYPE_BOOKMARK,
      bs.bookmarksMenuFolder,
      null,
      null,
      "Tagged bookmark in menu"
    );
    this._taggedItemIds.inToolbar = addBookmark(
      placeId,
      bs.TYPE_BOOKMARK,
      bs.toolbarFolder,
      null,
      null,
      "Tagged bookmark in toolbar"
    );
  },

  check() {
    // Check that valid bookmark is still there
    let stmt = mDBConn.createStatement(
      "SELECT title FROM moz_bookmarks WHERE id = :id"
    );
    stmt.params.id = this._untitledTagId;
    Assert.ok(stmt.executeStep());
    Assert.equal(stmt.row.title, "(notitle)");
    stmt.reset();
    stmt.params.id = this._untitledFolderId;
    Assert.ok(stmt.executeStep());
    Assert.equal(stmt.row.title, "");
    stmt.reset();
    stmt.params.id = this._titledTagId;
    Assert.ok(stmt.executeStep());
    Assert.equal(stmt.row.title, "titledTag");
    stmt.reset();
    stmt.params.id = this._titledFolderId;
    Assert.ok(stmt.executeStep());
    Assert.equal(stmt.row.title, "titledFolder");
    stmt.finalize();

    let taggedItemsStmt = mDBConn.createStatement(`
      SELECT syncChangeCounter FROM moz_bookmarks
      WHERE id IN (:taggedInMenu, :taggedInToolbar)`);
    taggedItemsStmt.params.taggedInMenu = this._taggedItemIds.inMenu;
    taggedItemsStmt.params.taggedInToolbar = this._taggedItemIds.inToolbar;
    while (taggedItemsStmt.executeStep()) {
      Assert.greaterOrEqual(taggedItemsStmt.row.syncChangeCounter, 1);
    }
    taggedItemsStmt.finalize();
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "E.1",
  desc: "Remove orphan icon entries",

  _placeId: null,

  setup() {
    // Insert favicon entries
    let stmt = mDBConn.createStatement(
      "INSERT INTO moz_icons (id, icon_url, fixed_icon_url_hash, root) VALUES(:favicon_id, :url, hash(fixup_url(:url)), :root)"
    );
    stmt.params.favicon_id = 1;
    stmt.params.url = "http://www1.mozilla.org/favicon.ico";
    stmt.params.root = 0;
    stmt.execute();
    stmt.reset();
    stmt.params.favicon_id = 2;
    stmt.params.url = "http://www2.mozilla.org/favicon.ico";
    stmt.params.root = 0;
    stmt.execute();
    stmt.reset();
    stmt.params.favicon_id = 3;
    stmt.params.url = "http://www3.mozilla.org/favicon.ico";
    stmt.params.root = 1;
    stmt.execute();
    stmt.finalize();
    // Insert orphan page.
    stmt = mDBConn.createStatement(
      "INSERT INTO moz_pages_w_icons (id, page_url, page_url_hash) VALUES(:page_id, :url, hash(:url))"
    );
    stmt.params.page_id = 99;
    stmt.params.url = "http://w99.mozilla.org/";
    stmt.execute();
    stmt.finalize();

    // Insert a place using the existing favicon entry
    this._placeId = addPlace("http://www.mozilla.org", 1);
  },

  check() {
    // Check that used icon is still there
    let stmt = mDBConn.createStatement(
      "SELECT id FROM moz_icons WHERE id = :favicon_id"
    );
    stmt.params.favicon_id = 1;
    Assert.ok(stmt.executeStep());
    stmt.reset();
    // Check that unused icon has been removed
    stmt.params.favicon_id = 2;
    Assert.ok(!stmt.executeStep());
    stmt.reset();
    // Check that unused icon has been removed
    stmt.params.favicon_id = 3;
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
    // Check that the orphan page is gone.
    stmt = mDBConn.createStatement(
      "SELECT id FROM moz_pages_w_icons WHERE id = :page_id"
    );
    stmt.params.page_id = 99;
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "F.1",
  desc: "Remove orphan visits",

  _placeId: null,
  _invalidPlaceId: 1337,

  setup() {
    // Add a place to ensure place_id = 1 is valid
    this._placeId = addPlace();
    // Add a valid visit and an invalid one
    let stmt = mDBConn.createStatement(
      "INSERT INTO moz_historyvisits(place_id) VALUES (:place_id)"
    );
    stmt.params.place_id = this._placeId;
    stmt.execute();
    stmt.reset();
    stmt.params.place_id = this._invalidPlaceId;
    stmt.execute();
    stmt.finalize();
  },

  check() {
    // Check that valid visit is still there
    let stmt = mDBConn.createStatement(
      "SELECT id FROM moz_historyvisits WHERE place_id = :place_id"
    );
    stmt.params.place_id = this._placeId;
    Assert.ok(stmt.executeStep());
    stmt.reset();
    // Check that invalid visit has been removed
    stmt.params.place_id = this._invalidPlaceId;
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "G.1",
  desc: "Remove orphan input history",

  _placeId: null,
  _invalidPlaceId: 1337,

  setup() {
    // Add a place to ensure place_id = 1 is valid
    this._placeId = addPlace();
    // Add input history entries
    let stmt = mDBConn.createStatement(
      "INSERT INTO moz_inputhistory (place_id, input) VALUES (:place_id, :input)"
    );
    stmt.params.place_id = this._placeId;
    stmt.params.input = "moz";
    stmt.execute();
    stmt.reset();
    stmt.params.place_id = this._invalidPlaceId;
    stmt.params.input = "moz";
    stmt.execute();
    stmt.finalize();
  },

  check() {
    // Check that inputhistory on valid place is still there
    let stmt = mDBConn.createStatement(
      "SELECT place_id FROM moz_inputhistory WHERE place_id = :place_id"
    );
    stmt.params.place_id = this._placeId;
    Assert.ok(stmt.executeStep());
    stmt.reset();
    // Check that inputhistory on invalid place has gone
    stmt.params.place_id = this._invalidPlaceId;
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "H.1",
  desc: "Remove item annos with an invalid attribute",

  _usedItemAttribute: "usedItem",
  _bookmarkId: null,
  _placeId: null,

  setup() {
    // Add a place to ensure place_id = 1 is valid
    this._placeId = addPlace();
    // Insert a bookmark
    this._bookmarkId = addBookmark(this._placeId, bs.TYPE_BOOKMARK);
    // Add a used attribute.
    let stmt = mDBConn.createStatement(
      "INSERT INTO moz_anno_attributes (name) VALUES (:anno)"
    );
    stmt.params.anno = this._usedItemAttribute;
    stmt.execute();
    stmt.finalize();
    stmt = mDBConn.createStatement(
      "INSERT INTO moz_items_annos (item_id, anno_attribute_id) VALUES(:item_id, (SELECT id FROM moz_anno_attributes WHERE name = :anno))"
    );
    stmt.params.item_id = this._bookmarkId;
    stmt.params.anno = this._usedItemAttribute;
    stmt.execute();
    stmt.finalize();
    // Add an annotation with a nonexistent attribute
    stmt = mDBConn.createStatement(
      "INSERT INTO moz_items_annos (item_id, anno_attribute_id) VALUES(:item_id, 1337)"
    );
    stmt.params.item_id = this._bookmarkId;
    stmt.execute();
    stmt.finalize();
  },

  check() {
    // Check that used attribute is still there
    let stmt = mDBConn.createStatement(
      "SELECT id FROM moz_anno_attributes WHERE name = :anno"
    );
    stmt.params.anno = this._usedItemAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // check that annotation with valid attribute is still there
    stmt = mDBConn.createStatement(
      "SELECT id FROM moz_items_annos WHERE anno_attribute_id = (SELECT id FROM moz_anno_attributes WHERE name = :anno)"
    );
    stmt.params.anno = this._usedItemAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // Check that annotation with bogus attribute has been removed
    stmt = mDBConn.createStatement(
      "SELECT id FROM moz_items_annos WHERE anno_attribute_id = 1337"
    );
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "H.2",
  desc: "Remove orphan item annotations",

  _usedItemAttribute: "usedItem",
  _bookmarkId: null,
  _invalidBookmarkId: 8888,
  _placeId: null,

  setup() {
    // Add a place to ensure place_id = 1 is valid
    this._placeId = addPlace();
    // Insert a bookmark
    this._bookmarkId = addBookmark(this._placeId, bs.TYPE_BOOKMARK);
    // Add a used attribute.
    let stmt = mDBConn.createStatement(
      "INSERT INTO moz_anno_attributes (name) VALUES (:anno)"
    );
    stmt.params.anno = this._usedItemAttribute;
    stmt.execute();
    stmt.finalize();
    stmt = mDBConn.createStatement(
      "INSERT INTO moz_items_annos (item_id, anno_attribute_id) VALUES (:item_id, (SELECT id FROM moz_anno_attributes WHERE name = :anno))"
    );
    stmt.params.item_id = this._bookmarkId;
    stmt.params.anno = this._usedItemAttribute;
    stmt.execute();
    stmt.reset();
    // Add an annotation to a nonexistent item
    stmt.params.item_id = this._invalidBookmarkId;
    stmt.params.anno = this._usedItemAttribute;
    stmt.execute();
    stmt.finalize();
  },

  check() {
    // Check that used attribute is still there
    let stmt = mDBConn.createStatement(
      "SELECT id FROM moz_anno_attributes WHERE name = :anno"
    );
    stmt.params.anno = this._usedItemAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // check that annotation with valid attribute is still there
    stmt = mDBConn.createStatement(
      "SELECT id FROM moz_items_annos WHERE anno_attribute_id = (SELECT id FROM moz_anno_attributes WHERE name = :anno)"
    );
    stmt.params.anno = this._usedItemAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // Check that an annotation to a nonexistent page has been removed
    stmt = mDBConn.createStatement(
      "SELECT id FROM moz_items_annos WHERE item_id = 8888"
    );
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "I.1",
  desc: "Remove unused keywords",

  _bookmarkId: null,
  _placeId: null,

  setup() {
    // Insert 2 keywords
    let stmt = mDBConn.createStatement(
      "INSERT INTO moz_keywords (id, keyword, place_id) VALUES(:id, :keyword, :place_id)"
    );
    stmt.params.id = 1;
    stmt.params.keyword = "unused";
    stmt.params.place_id = 100;
    stmt.execute();
    stmt.finalize();
  },

  check() {
    // Check that "used" keyword is still there
    let stmt = mDBConn.createStatement(
      "SELECT id FROM moz_keywords WHERE keyword = :keyword"
    );
    // Check that "unused" keyword has gone
    stmt.params.keyword = "unused";
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "L.1",
  desc: "remove duplicate URLs",
  _placeA: -1,
  _placeD: -1,
  _placeE: -1,
  _bookmarkIds: [],

  async setup() {
    // Place with visits, an autocomplete history entry, anno, and a bookmark.
    this._placeA = addPlace("http://example.com", null, "placeAAAAAAA");

    // Duplicate Place with different visits and a keyword.
    let placeB = addPlace("http://example.com", null, "placeBBBBBBB");

    // Another duplicate with conflicting autocomplete history entry and
    // two more bookmarks.
    let placeC = addPlace("http://example.com", null, "placeCCCCCCC");

    // Unrelated, unique Place.
    this._placeD = addPlace("http://example.net", null, "placeDDDDDDD", 1234);

    // Another unrelated Place, with the same hash as D, but different URL.
    this._placeE = addPlace("http://example.info", null, "placeEEEEEEE", 1234);

    let visits = [
      {
        placeId: this._placeA,
        date: new Date(2017, 1, 2),
        type: PlacesUtils.history.TRANSITIONS.LINK,
      },
      {
        placeId: this._placeA,
        date: new Date(2018, 3, 4),
        type: PlacesUtils.history.TRANSITIONS.TYPED,
      },
      {
        placeId: placeB,
        date: new Date(2016, 5, 6),
        type: PlacesUtils.history.TRANSITIONS.TYPED,
      },
      {
        // Duplicate visit; should keep both when we merge.
        placeId: placeB,
        date: new Date(2018, 3, 4),
        type: PlacesUtils.history.TRANSITIONS.TYPED,
      },
      {
        placeId: this._placeD,
        date: new Date(2018, 7, 8),
        type: PlacesUtils.history.TRANSITIONS.LINK,
      },
      {
        placeId: this._placeE,
        date: new Date(2018, 8, 9),
        type: PlacesUtils.history.TRANSITIONS.TYPED,
      },
    ];

    let inputs = [
      {
        placeId: this._placeA,
        input: "exam",
        count: 4,
      },
      {
        placeId: placeC,
        input: "exam",
        count: 3,
      },
      {
        placeId: placeC,
        input: "ex",
        count: 5,
      },
      {
        placeId: this._placeD,
        input: "amp",
        count: 3,
      },
    ];

    let annos = [
      {
        name: "anno",
        placeId: this._placeA,
        content: "splish",
      },
      {
        // Anno that's already set on A; should be ignored when we merge.
        name: "anno",
        placeId: placeB,
        content: "oops",
      },
      {
        name: "other-anno",
        placeId: placeB,
        content: "splash",
      },
      {
        name: "other-anno",
        placeId: this._placeD,
        content: "sploosh",
      },
    ];

    let bookmarks = [
      {
        placeId: this._placeA,
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        title: "A",
        guid: "bookmarkAAAA",
      },
      {
        placeId: placeB,
        parentGuid: PlacesUtils.bookmarks.mobileGuid,
        title: "B",
        guid: "bookmarkBBBB",
      },
      {
        placeId: placeC,
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        title: "C1",
        guid: "bookmarkCCC1",
      },
      {
        placeId: placeC,
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        title: "C2",
        guid: "bookmarkCCC2",
      },
      {
        placeId: this._placeD,
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        title: "D",
        guid: "bookmarkDDDD",
      },
      {
        placeId: this._placeE,
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        title: "E",
        guid: "bookmarkEEEE",
      },
    ];

    let keywords = [
      {
        placeId: placeB,
        keyword: "hi",
      },
      {
        placeId: this._placeD,
        keyword: "bye",
      },
    ];

    for (let { placeId, parentGuid, title, guid } of bookmarks) {
      let itemId = addBookmark(
        placeId,
        PlacesUtils.bookmarks.TYPE_BOOKMARK,
        await PlacesUtils.promiseItemId(parentGuid),
        null,
        null,
        title,
        guid
      );
      this._bookmarkIds.push(itemId);
    }

    await PlacesUtils.withConnectionWrapper(
      "L.1: Insert foreign key refs",
      function(db) {
        return db.executeTransaction(async function() {
          for (let { placeId, date, type } of visits) {
            await db.executeCached(
              `
              INSERT INTO moz_historyvisits(place_id, visit_date, visit_type)
              VALUES(:placeId, :date, :type)`,
              { placeId, date: PlacesUtils.toPRTime(date), type }
            );
          }

          for (let params of inputs) {
            await db.executeCached(
              `
              INSERT INTO moz_inputhistory(place_id, input, use_count)
              VALUES(:placeId, :input, :count)`,
              params
            );
          }

          for (let { name, placeId, content } of annos) {
            await db.executeCached(
              `
              INSERT OR IGNORE INTO moz_anno_attributes(name)
              VALUES(:name)`,
              { name }
            );

            await db.executeCached(
              `
              INSERT INTO moz_annos(place_id, anno_attribute_id, content)
              VALUES(:placeId, (SELECT id FROM moz_anno_attributes
                                WHERE name = :name), :content)`,
              { placeId, name, content }
            );
          }

          for (let param of keywords) {
            await db.executeCached(
              `
              INSERT INTO moz_keywords(keyword, place_id)
              VALUES(:keyword, :placeId)`,
              param
            );
          }
        });
      }
    );
  },

  async check() {
    let db = await PlacesUtils.promiseDBConnection();

    let placeRows = await db.execute(`
      SELECT id, guid, foreign_count FROM moz_places
      ORDER BY guid`);
    let placeInfos = placeRows.map(row => ({
      id: row.getResultByName("id"),
      guid: row.getResultByName("guid"),
      foreignCount: row.getResultByName("foreign_count"),
    }));
    Assert.deepEqual(
      placeInfos,
      [
        {
          id: this._placeA,
          guid: "placeAAAAAAA",
          foreignCount: 5, // 4 bookmarks + 1 keyword
        },
        {
          id: this._placeD,
          guid: "placeDDDDDDD",
          foreignCount: 2, // 1 bookmark + 1 keyword
        },
        {
          id: this._placeE,
          guid: "placeEEEEEEE",
          foreignCount: 1, // 1 bookmark
        },
      ],
      "Should remove duplicate Places B and C"
    );

    let visitRows = await db.execute(`
      SELECT place_id, visit_date, visit_type FROM moz_historyvisits
      ORDER BY visit_date`);
    let visitInfos = visitRows.map(row => ({
      placeId: row.getResultByName("place_id"),
      date: PlacesUtils.toDate(row.getResultByName("visit_date")),
      type: row.getResultByName("visit_type"),
    }));
    Assert.deepEqual(
      visitInfos,
      [
        {
          placeId: this._placeA,
          date: new Date(2016, 5, 6),
          type: PlacesUtils.history.TRANSITIONS.TYPED,
        },
        {
          placeId: this._placeA,
          date: new Date(2017, 1, 2),
          type: PlacesUtils.history.TRANSITIONS.LINK,
        },
        {
          placeId: this._placeA,
          date: new Date(2018, 3, 4),
          type: PlacesUtils.history.TRANSITIONS.TYPED,
        },
        {
          placeId: this._placeA,
          date: new Date(2018, 3, 4),
          type: PlacesUtils.history.TRANSITIONS.TYPED,
        },
        {
          placeId: this._placeD,
          date: new Date(2018, 7, 8),
          type: PlacesUtils.history.TRANSITIONS.LINK,
        },
        {
          placeId: this._placeE,
          date: new Date(2018, 8, 9),
          type: PlacesUtils.history.TRANSITIONS.TYPED,
        },
      ],
      "Should merge history visits"
    );

    let inputRows = await db.execute(`
      SELECT place_id, input, use_count FROM moz_inputhistory
      ORDER BY use_count ASC`);
    let inputInfos = inputRows.map(row => ({
      placeId: row.getResultByName("place_id"),
      input: row.getResultByName("input"),
      count: row.getResultByName("use_count"),
    }));
    Assert.deepEqual(
      inputInfos,
      [
        {
          placeId: this._placeD,
          input: "amp",
          count: 3,
        },
        {
          placeId: this._placeA,
          input: "ex",
          count: 5,
        },
        {
          placeId: this._placeA,
          input: "exam",
          count: 7,
        },
      ],
      "Should merge autocomplete history"
    );

    let annoRows = await db.execute(`
      SELECT a.place_id, n.name, a.content FROM moz_annos a
      JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
      ORDER BY n.name, a.content ASC`);
    let annoInfos = annoRows.map(row => ({
      placeId: row.getResultByName("place_id"),
      name: row.getResultByName("name"),
      content: row.getResultByName("content"),
    }));
    Assert.deepEqual(
      annoInfos,
      [
        {
          placeId: this._placeA,
          name: "anno",
          content: "splish",
        },
        {
          placeId: this._placeA,
          name: "other-anno",
          content: "splash",
        },
        {
          placeId: this._placeD,
          name: "other-anno",
          content: "sploosh",
        },
      ],
      "Should merge page annos"
    );

    let itemRows = await db.execute(
      `
      SELECT guid, fk, syncChangeCounter FROM moz_bookmarks
      WHERE id IN (${new Array(this._bookmarkIds.length).fill("?").join(",")})
      ORDER BY guid ASC`,
      this._bookmarkIds
    );
    let itemInfos = itemRows.map(row => ({
      guid: row.getResultByName("guid"),
      placeId: row.getResultByName("fk"),
      syncChangeCounter: row.getResultByName("syncChangeCounter"),
    }));
    Assert.deepEqual(
      itemInfos,
      [
        {
          guid: "bookmarkAAAA",
          placeId: this._placeA,
          syncChangeCounter: 1,
        },
        {
          guid: "bookmarkBBBB",
          placeId: this._placeA,
          syncChangeCounter: 1,
        },
        {
          guid: "bookmarkCCC1",
          placeId: this._placeA,
          syncChangeCounter: 1,
        },
        {
          guid: "bookmarkCCC2",
          placeId: this._placeA,
          syncChangeCounter: 1,
        },
        {
          guid: "bookmarkDDDD",
          placeId: this._placeD,
          syncChangeCounter: 0,
        },
        {
          guid: "bookmarkEEEE",
          placeId: this._placeE,
          syncChangeCounter: 0,
        },
      ],
      "Should merge bookmarks and bump change counter"
    );

    let keywordRows = await db.execute(`
      SELECT keyword, place_id FROM moz_keywords
      ORDER BY keyword ASC`);
    let keywordInfos = keywordRows.map(row => ({
      keyword: row.getResultByName("keyword"),
      placeId: row.getResultByName("place_id"),
    }));
    Assert.deepEqual(
      keywordInfos,
      [
        {
          keyword: "bye",
          placeId: this._placeD,
        },
        {
          keyword: "hi",
          placeId: this._placeA,
        },
      ],
      "Should merge all keywords"
    );
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "L.2",
  desc: "Recalculate visit_count and last_visit_date",

  async setup() {
    function setVisitCount(aURL, aValue) {
      let stmt = mDBConn.createStatement(
        `UPDATE moz_places SET visit_count = :count WHERE url_hash = hash(:url)
                                                      AND url = :url`
      );
      stmt.params.count = aValue;
      stmt.params.url = aURL;
      stmt.execute();
      stmt.finalize();
    }
    function setLastVisitDate(aURL, aValue) {
      let stmt = mDBConn.createStatement(
        `UPDATE moz_places SET last_visit_date = :date WHERE url_hash = hash(:url)
                                                         AND url = :url`
      );
      stmt.params.date = aValue;
      stmt.params.url = aURL;
      stmt.execute();
      stmt.finalize();
    }

    let now = Date.now() * 1000;
    // Add a page with 1 visit.
    let url = "http://1.moz.org/";
    await PlacesTestUtils.addVisits({ uri: uri(url), visitDate: now++ });
    // Add a page with 1 visit and set wrong visit_count.
    url = "http://2.moz.org/";
    await PlacesTestUtils.addVisits({ uri: uri(url), visitDate: now++ });
    setVisitCount(url, 10);
    // Add a page with 1 visit and set wrong last_visit_date.
    url = "http://3.moz.org/";
    await PlacesTestUtils.addVisits({ uri: uri(url), visitDate: now++ });
    setLastVisitDate(url, now++);
    // Add a page with 1 visit and set wrong stats.
    url = "http://4.moz.org/";
    await PlacesTestUtils.addVisits({ uri: uri(url), visitDate: now++ });
    setVisitCount(url, 10);
    setLastVisitDate(url, now++);

    // Add a page without visits.
    url = "http://5.moz.org/";
    addPlace(url);
    // Add a page without visits and set wrong visit_count.
    url = "http://6.moz.org/";
    addPlace(url);
    setVisitCount(url, 10);
    // Add a page without visits and set wrong last_visit_date.
    url = "http://7.moz.org/";
    addPlace(url);
    setLastVisitDate(url, now++);
    // Add a page without visits and set wrong stats.
    url = "http://8.moz.org/";
    addPlace(url);
    setVisitCount(url, 10);
    setLastVisitDate(url, now++);
  },

  check() {
    let stmt = mDBConn.createStatement(
      `SELECT h.id, h.last_visit_date as v_date
       FROM moz_places h
       LEFT JOIN moz_historyvisits v ON v.place_id = h.id AND visit_type NOT IN (0,4,7,8,9)
       GROUP BY h.id HAVING h.visit_count <> count(v.id)
       UNION ALL
       SELECT h.id, MAX(v.visit_date) as v_date
       FROM moz_places h
       LEFT JOIN moz_historyvisits v ON v.place_id = h.id
       GROUP BY h.id HAVING h.last_visit_date IS NOT v_date`
    );
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "L.3",
  desc: "recalculate hidden for redirects.",

  async setup() {
    await PlacesTestUtils.addVisits([
      {
        uri: NetUtil.newURI("http://l3.moz.org/"),
        transition: TRANSITION_TYPED,
      },
      {
        uri: NetUtil.newURI("http://l3.moz.org/redirecting/"),
        transition: TRANSITION_TYPED,
      },
      {
        uri: NetUtil.newURI("http://l3.moz.org/redirecting2/"),
        transition: TRANSITION_REDIRECT_TEMPORARY,
        referrer: NetUtil.newURI("http://l3.moz.org/redirecting/"),
      },
      {
        uri: NetUtil.newURI("http://l3.moz.org/target/"),
        transition: TRANSITION_REDIRECT_PERMANENT,
        referrer: NetUtil.newURI("http://l3.moz.org/redirecting2/"),
      },
    ]);
  },

  check() {
    return new Promise(resolve => {
      let stmt = mDBConn.createAsyncStatement(
        "SELECT h.url FROM moz_places h WHERE h.hidden = 1"
      );
      stmt.executeAsync({
        _count: 0,
        handleResult(aResultSet) {
          for (let row; (row = aResultSet.getNextRow()); ) {
            let url = row.getResultByIndex(0);
            Assert.ok(/redirecting/.test(url));
            this._count++;
          }
        },
        handleError(aError) {},
        handleCompletion(aReason) {
          Assert.equal(
            aReason,
            Ci.mozIStorageStatementCallback.REASON_FINISHED
          );
          Assert.equal(this._count, 2);
          resolve();
        },
      });
      stmt.finalize();
    });
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "L.4",
  desc: "recalculate foreign_count.",

  async setup() {
    this._pageGuid = (
      await PlacesUtils.history.insert({
        url: "http://l4.moz.org/",
        visits: [{ date: new Date() }],
      })
    ).guid;
    await PlacesUtils.bookmarks.insert({
      url: "http://l4.moz.org/",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    });
    await PlacesUtils.keywords.insert({
      url: "http://l4.moz.org/",
      keyword: "kw",
    });
    await PlacesUtils.withConnectionWrapper(
      "add snapshots and sessions",
      async db => {
        await db.execute(
          `
           INSERT INTO moz_places_metadata_snapshots
             (place_id, first_interaction_at, last_interaction_at, document_type, created_at, user_persisted)
           VALUES ((SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url), 0, 0, "MEDIA", 0, 0)
          `,
          { url: "http://l4.moz.org/" }
        );
        await db.execute(
          `INSERT INTO moz_session_metadata (guid, last_saved_at)
           VALUES ("guid", 0)`
        );
        await db.execute(
          `
           INSERT INTO moz_session_to_places (session_id, place_id, position)
           VALUES (
             (SELECT id FROM moz_session_metadata WHERE guid = "guid"),
             (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url),
             1
           )`,
          { url: "http://l4.moz.org/" }
        );
      }
    );
    Assert.equal(await this._getForeignCount(), 2);
  },

  async _getForeignCount() {
    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.execute(
      `SELECT foreign_count FROM moz_places
                                 WHERE guid = :guid`,
      { guid: this._pageGuid }
    );
    return rows[0].getResultByName("foreign_count");
  },

  async check() {
    Assert.equal(await this._getForeignCount(), 4);
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "L.5",
  desc: "recalculate hashes when missing.",

  async setup() {
    this._pageGuid = (
      await PlacesUtils.history.insert({
        url: "http://l5.moz.org/",
        visits: [{ date: new Date() }],
      })
    ).guid;
    Assert.ok((await this._getHash()) > 0);
    await PlacesUtils.withConnectionWrapper("change url hash", async function(
      db
    ) {
      await db.execute(`UPDATE moz_places SET url_hash = 0`);
    });
    Assert.equal(await this._getHash(), 0);
  },

  async _getHash() {
    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.execute(
      `SELECT url_hash FROM moz_places
                                 WHERE guid = :guid`,
      { guid: this._pageGuid }
    );
    return rows[0].getResultByName("url_hash");
  },

  async check() {
    Assert.ok((await this._getHash()) > 0);
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "L.6",
  desc: "fix invalid Place GUIDs",
  _placeIds: [],

  async setup() {
    let placeWithValidGuid = addPlace(
      "http://example.com/a",
      null,
      "placeAAAAAAA"
    );
    this._placeIds.push(placeWithValidGuid);

    let placeWithEmptyGuid = addPlace("http://example.com/b", null, "");
    this._placeIds.push(placeWithEmptyGuid);

    let placeWithoutGuid = addPlace("http://example.com/c", null, null);
    this._placeIds.push(placeWithoutGuid);

    let placeWithInvalidGuid = addPlace(
      "http://example.com/c",
      null,
      "{123456}"
    );
    this._placeIds.push(placeWithInvalidGuid);
  },

  async check() {
    let db = await PlacesUtils.promiseDBConnection();
    let updatedRows = await db.execute(
      `
      SELECT id, guid
      FROM moz_places
      WHERE id IN (?, ?, ?, ?)`,
      this._placeIds
    );

    for (let row of updatedRows) {
      let id = row.getResultByName("id");
      let guid = row.getResultByName("guid");
      if (id == this._placeIds[0]) {
        Assert.equal(guid, "placeAAAAAAA");
      } else {
        Assert.ok(PlacesUtils.isValidGuid(guid));
      }
    }
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "S.1",
  desc: "fix invalid GUIDs for synced bookmarks",
  _bookmarkInfos: [],

  async setup() {
    let folderWithInvalidGuid = addBookmark(
      null,
      PlacesUtils.bookmarks.TYPE_FOLDER,
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      /* aKeywordId */ null,
      /* aFolderType */ null,
      "NORMAL folder with invalid GUID",
      "{123456}",
      PlacesUtils.bookmarks.SYNC_STATUS.NORMAL
    );

    let placeIdForBookmarkWithoutGuid = addPlace();
    let bookmarkWithoutGuid = addBookmark(
      placeIdForBookmarkWithoutGuid,
      PlacesUtils.bookmarks.TYPE_BOOKMARK,
      folderWithInvalidGuid,
      /* aKeywordId */ null,
      /* aFolderType */ null,
      "NEW bookmark without GUID",
      /* aGuid */ null
    );

    let placeIdForBookmarkWithInvalidGuid = addPlace();
    let bookmarkWithInvalidGuid = addBookmark(
      placeIdForBookmarkWithInvalidGuid,
      PlacesUtils.bookmarks.TYPE_BOOKMARK,
      folderWithInvalidGuid,
      /* aKeywordId */ null,
      /* aFolderType */ null,
      "NORMAL bookmark with invalid GUID",
      "bookmarkAAAA\n",
      PlacesUtils.bookmarks.SYNC_STATUS.NORMAL
    );

    let placeIdForBookmarkWithValidGuid = addPlace();
    let bookmarkWithValidGuid = addBookmark(
      placeIdForBookmarkWithValidGuid,
      PlacesUtils.bookmarks.TYPE_BOOKMARK,
      folderWithInvalidGuid,
      /* aKeywordId */ null,
      /* aFolderType */ null,
      "NORMAL bookmark with valid GUID",
      "bookmarkBBBB",
      PlacesUtils.bookmarks.SYNC_STATUS.NORMAL
    );

    this._bookmarkInfos.push(
      {
        id: PlacesUtils.bookmarks.bookmarksMenuFolder,
        syncChangeCounter: 1,
        syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
      },
      {
        id: folderWithInvalidGuid,
        syncChangeCounter: 3,
        syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NEW,
      },
      {
        id: bookmarkWithoutGuid,
        syncChangeCounter: 1,
        syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NEW,
      },
      {
        id: bookmarkWithInvalidGuid,
        syncChangeCounter: 1,
        syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NEW,
      },
      {
        id: bookmarkWithValidGuid,
        syncChangeCounter: 0,
        syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
      }
    );
  },

  async check() {
    let db = await PlacesUtils.promiseDBConnection();
    let updatedRows = await db.execute(
      `
      SELECT id, guid, syncChangeCounter, syncStatus
      FROM moz_bookmarks
      WHERE id IN (?, ?, ?, ?, ?)`,
      this._bookmarkInfos.map(info => info.id)
    );

    for (let row of updatedRows) {
      let id = row.getResultByName("id");
      let guid = row.getResultByName("guid");
      Assert.ok(PlacesUtils.isValidGuid(guid));

      let cachedGuid = await PlacesUtils.promiseItemGuid(id);
      Assert.equal(cachedGuid, guid);

      let expectedInfo = this._bookmarkInfos.find(info => info.id == id);

      let syncChangeCounter = row.getResultByName("syncChangeCounter");
      Assert.equal(syncChangeCounter, expectedInfo.syncChangeCounter);

      let syncStatus = row.getResultByName("syncStatus");
      Assert.equal(syncStatus, expectedInfo.syncStatus);
    }

    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    Assert.deepEqual(
      tombstones.map(info => info.guid),
      ["bookmarkAAAA\n", "{123456}"]
    );
  },
});

tests.push({
  name: "S.2",
  desc: "drop tombstones for bookmarks that aren't deleted",

  async setup() {
    let placeId = addPlace();
    addBookmark(
      placeId,
      bs.TYPE_BOOKMARK,
      bs.bookmarksMenuFolder,
      null,
      null,
      "",
      "bookmarkAAAA"
    );

    await PlacesUtils.withConnectionWrapper("Insert tombstones", db =>
      db.executeTransaction(async function() {
        for (let guid of ["bookmarkAAAA", "bookmarkBBBB"]) {
          await db.executeCached(
            `
            INSERT INTO moz_bookmarks_deleted(guid)
            VALUES(:guid)`,
            { guid }
          );
        }
      })
    );
  },

  async check() {
    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    Assert.deepEqual(
      tombstones.map(info => info.guid),
      ["bookmarkBBBB"]
    );
  },
});

tests.push({
  name: "S.3",
  desc: "set missing added and last modified dates",
  _placeVisits: [],
  _bookmarksWithDates: [],

  async setup() {
    let placeIdWithVisits = addPlace();
    let placeIdWithZeroVisit = addPlace();
    this._placeVisits.push(
      {
        placeId: placeIdWithVisits,
        visitDate: PlacesUtils.toPRTime(new Date(2017, 9, 4)),
      },
      {
        placeId: placeIdWithVisits,
        visitDate: PlacesUtils.toPRTime(new Date(2017, 9, 8)),
      },
      {
        placeId: placeIdWithZeroVisit,
        visitDate: 0,
      }
    );

    this._bookmarksWithDates.push(
      {
        guid: "bookmarkAAAA",
        placeId: null,
        parentId: bs.bookmarksMenuFolder,
        dateAdded: null,
        lastModified: PlacesUtils.toPRTime(new Date(2017, 9, 1)),
      },
      {
        guid: "bookmarkBBBB",
        placeId: null,
        parentId: bs.bookmarksMenuFolder,
        dateAdded: PlacesUtils.toPRTime(new Date(2017, 9, 2)),
        lastModified: null,
      },
      {
        guid: "bookmarkCCCC",
        placeId: null,
        parentId: gUnfiledFolderId,
        dateAdded: null,
        lastModified: null,
      },
      {
        guid: "bookmarkDDDD",
        placeId: placeIdWithVisits,
        parentId: bs.mobileFolder,
        dateAdded: null,
        lastModified: null,
      },
      {
        guid: "bookmarkEEEE",
        placeId: placeIdWithVisits,
        parentId: gUnfiledFolderId,
        dateAdded: PlacesUtils.toPRTime(new Date(2017, 9, 3)),
        lastModified: PlacesUtils.toPRTime(new Date(2017, 9, 6)),
      },
      {
        guid: "bookmarkFFFF",
        placeId: placeIdWithZeroVisit,
        parentId: gUnfiledFolderId,
        dateAdded: 0,
        lastModified: 0,
      }
    );

    await PlacesUtils.withConnectionWrapper(
      "S.3: Insert bookmarks and visits",
      db =>
        db.executeTransaction(async () => {
          await db.execute(
            `
          INSERT INTO moz_historyvisits(place_id, visit_date)
          VALUES(:placeId, :visitDate)`,
            this._placeVisits
          );

          await db.execute(
            `
          INSERT INTO moz_bookmarks(fk, type, parent, guid, dateAdded,
                                    lastModified)
          VALUES(:placeId, 1, :parentId, :guid, :dateAdded,
                 :lastModified)`,
            this._bookmarksWithDates
          );

          await db.execute(
            `
          UPDATE moz_bookmarks SET
            dateAdded = 0,
            lastModified = NULL
          WHERE id = :toolbarFolderId`,
            { toolbarFolderId: bs.toolbarFolder }
          );
        })
    );
  },

  async check() {
    let db = await PlacesUtils.promiseDBConnection();
    let updatedRows = await db.execute(
      `
      SELECT guid, dateAdded, lastModified
      FROM moz_bookmarks
      WHERE guid = :guid`,
      [
        { guid: bs.toolbarGuid },
        ...this._bookmarksWithDates.map(({ guid }) => ({ guid })),
      ]
    );

    for (let row of updatedRows) {
      let guid = row.getResultByName("guid");

      let dateAdded = row.getResultByName("dateAdded");
      Assert.ok(Number.isInteger(dateAdded));

      let lastModified = row.getResultByName("lastModified");
      Assert.ok(Number.isInteger(lastModified));

      switch (guid) {
        // Last modified date exists, so we should use it for date added.
        case "bookmarkAAAA": {
          let expectedInfo = this._bookmarksWithDates[0];
          Assert.equal(dateAdded, expectedInfo.lastModified);
          Assert.equal(lastModified, expectedInfo.lastModified);
          break;
        }

        // Date added exists, so we should use it for last modified date.
        case "bookmarkBBBB": {
          let expectedInfo = this._bookmarksWithDates[1];
          Assert.equal(dateAdded, expectedInfo.dateAdded);
          Assert.equal(lastModified, expectedInfo.dateAdded);
          break;
        }

        // C has no visits, date added, or last modified time, F has zeros for
        // all, and the toolbar has a zero date added and no last modified time.
        // In all cases, we should fall back to the current time.
        case "bookmarkCCCC":
        case "bookmarkFFFF":
        case bs.toolbarGuid: {
          let nowAsPRTime = PlacesUtils.toPRTime(new Date());
          Assert.greater(dateAdded, 0);
          Assert.equal(dateAdded, lastModified);
          Assert.ok(dateAdded <= nowAsPRTime);
          break;
        }

        // Neither date added nor last modified exists, but we have two
        // visits, so we should fall back to the earliest and latest visit
        // dates.
        case "bookmarkDDDD": {
          let oldestVisit = this._placeVisits[0];
          Assert.equal(dateAdded, oldestVisit.visitDate);
          let newestVisit = this._placeVisits[1];
          Assert.equal(lastModified, newestVisit.visitDate);
          break;
        }

        // We have two visits, but both date added and last modified exist,
        // so we shouldn't update them.
        case "bookmarkEEEE": {
          let expectedInfo = this._bookmarksWithDates[4];
          Assert.equal(dateAdded, expectedInfo.dateAdded);
          Assert.equal(lastModified, expectedInfo.lastModified);
          break;
        }

        default:
          throw new Error(`Unexpected row for bookmark ${guid}`);
      }
    }
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "S.4",
  desc: "reset added dates that are ahead of last modified dates",
  _bookmarksWithDates: [],

  async setup() {
    this._bookmarksWithDates.push({
      guid: "bookmarkGGGG",
      parentId: gUnfiledFolderId,
      dateAdded: PlacesUtils.toPRTime(new Date(2017, 9, 6)),
      lastModified: PlacesUtils.toPRTime(new Date(2017, 9, 3)),
    });

    await PlacesUtils.withConnectionWrapper(
      "S.4: Insert bookmarks and visits",
      db =>
        db.executeTransaction(async () => {
          await db.execute(
            `
          INSERT INTO moz_bookmarks(type, parent, guid, dateAdded,
                                    lastModified)
          VALUES(1, :parentId, :guid, :dateAdded, :lastModified)`,
            this._bookmarksWithDates
          );
        })
    );
  },

  async check() {
    let db = await PlacesUtils.promiseDBConnection();
    let updatedRows = await db.execute(
      `
      SELECT guid, dateAdded, lastModified
      FROM moz_bookmarks
      WHERE guid = :guid`,
      this._bookmarksWithDates.map(({ guid }) => ({ guid }))
    );

    for (let row of updatedRows) {
      let guid = row.getResultByName("guid");
      let dateAdded = row.getResultByName("dateAdded");
      let lastModified = row.getResultByName("lastModified");
      switch (guid) {
        case "bookmarkGGGG": {
          let expectedInfo = this._bookmarksWithDates[0];
          Assert.equal(dateAdded, expectedInfo.lastModified);
          Assert.equal(lastModified, expectedInfo.lastModified);
          break;
        }

        default:
          throw new Error(`Unexpected row for bookmark ${guid}`);
      }
    }
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "T.1",
  desc: "history.recalculateOriginFrecencyStats() is called",

  async setup() {
    let urls = [
      "http://example1.com/",
      "http://example2.com/",
      "http://example3.com/",
    ];
    await PlacesTestUtils.addVisits(urls.map(u => ({ uri: u })));

    this._frecencies = urls.map(u => frecencyForUrl(u));

    let stats = await this._promiseStats();
    Assert.equal(stats.count, this._frecencies.length, "Sanity check");
    Assert.equal(stats.sum, this._sum(this._frecencies), "Sanity check");
    Assert.equal(
      stats.squares,
      this._squares(this._frecencies),
      "Sanity check"
    );

    await PlacesUtils.withConnectionWrapper("T.1", db =>
      db.execute(`
        INSERT OR REPLACE INTO moz_meta VALUES
        ('origin_frecency_count', 99),
        ('origin_frecency_sum', 99999),
        ('origin_frecency_sum_of_squares', 99999 * 99999);
      `)
    );

    stats = await this._promiseStats();
    Assert.equal(stats.count, 99);
    Assert.equal(stats.sum, 99999);
    Assert.equal(stats.squares, 99999 * 99999);
  },

  async check() {
    let stats = await this._promiseStats();
    Assert.equal(stats.count, this._frecencies.length);
    Assert.equal(stats.sum, this._sum(this._frecencies));
    Assert.equal(stats.squares, this._squares(this._frecencies));
  },

  _sum(frecs) {
    return frecs.reduce((memo, f) => memo + f, 0);
  },

  _squares(frecs) {
    return frecs.reduce((memo, f) => memo + f * f, 0);
  },

  async _promiseStats() {
    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.execute(`
      SELECT
        IFNULL((SELECT value FROM moz_meta WHERE key = 'origin_frecency_count'), 0),
        IFNULL((SELECT value FROM moz_meta WHERE key = 'origin_frecency_sum'), 0),
        IFNULL((SELECT value FROM moz_meta WHERE key = 'origin_frecency_sum_of_squares'), 0)
    `);
    return {
      count: rows[0].getResultByIndex(0),
      sum: rows[0].getResultByIndex(1),
      squares: rows[0].getResultByIndex(2),
    };
  },
});

// ------------------------------------------------------------------------------

tests.push({
  name: "Z",
  desc: "Sanity: Preventive maintenance does not touch valid items",

  _uri1: uri("http://www1.mozilla.org"),
  _uri2: uri("http://www2.mozilla.org"),
  _folder: null,
  _bookmark: null,
  _bookmarkId: null,
  _separator: null,

  async setup() {
    // use valid api calls to create a bunch of items
    await PlacesTestUtils.addVisits([{ uri: this._uri1 }, { uri: this._uri2 }]);

    let bookmarks = await bs.insertTree({
      guid: bs.toolbarGuid,
      children: [
        {
          title: "testfolder",
          type: bs.TYPE_FOLDER,
          children: [
            {
              title: "testbookmark",
              url: this._uri1,
            },
          ],
        },
      ],
    });

    this._folder = bookmarks[0];
    this._bookmark = bookmarks[1];
    this._bookmarkId = await PlacesUtils.promiseItemId(bookmarks[1].guid);

    this._separator = await bs.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    });

    ts.tagURI(this._uri1, ["testtag"]);
    fs.setAndFetchFaviconForPage(
      this._uri2,
      SMALLPNG_DATA_URI,
      false,
      PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
      null,
      Services.scriptSecurityManager.getSystemPrincipal()
    );
    await PlacesUtils.keywords.insert({
      url: this._uri1.spec,
      keyword: "testkeyword",
    });
    await PlacesUtils.history.update({
      url: this._uri2,
      annotations: new Map([["anno", "anno"]]),
    });
  },

  async check() {
    // Check that all items are correct
    let isVisited = await PlacesUtils.history.hasVisits(this._uri1);
    Assert.ok(isVisited);
    isVisited = await PlacesUtils.history.hasVisits(this._uri2);
    Assert.ok(isVisited);

    Assert.equal((await bs.fetch(this._bookmark.guid)).url, this._uri1.spec);
    let folder = await bs.fetch(this._folder.guid);
    Assert.equal(folder.index, 0);
    Assert.equal(folder.type, bs.TYPE_FOLDER);
    Assert.equal(
      (await bs.fetch(this._separator.guid)).type,
      bs.TYPE_SEPARATOR
    );

    Assert.equal(ts.getTagsForURI(this._uri1).length, 1);
    Assert.equal(
      (await PlacesUtils.keywords.fetch({ url: this._uri1.spec })).keyword,
      "testkeyword"
    );
    let pageInfo = await PlacesUtils.history.fetch(this._uri2, {
      includeAnnotations: true,
    });
    Assert.equal(pageInfo.annotations.get("anno"), "anno");

    await new Promise(resolve => {
      fs.getFaviconURLForPage(this._uri2, aFaviconURI => {
        Assert.ok(aFaviconURI.equals(SMALLPNG_DATA_URI));
        resolve();
      });
    });
  },
});

// ------------------------------------------------------------------------------

add_task(async function test_preventive_maintenance() {
  gUnfiledFolderId = await PlacesUtils.promiseItemId(
    PlacesUtils.bookmarks.unfiledGuid
  );

  // Get current bookmarks max ID for cleanup
  let stmt = mDBConn.createStatement("SELECT MAX(id) FROM moz_bookmarks");
  stmt.executeStep();
  defaultBookmarksMaxId = stmt.getInt32(0);
  stmt.finalize();
  Assert.ok(defaultBookmarksMaxId > 0);

  for (let test of tests) {
    await PlacesTestUtils.markBookmarksAsSynced();

    dump("\nExecuting test: " + test.name + "\n*** " + test.desc + "\n");
    await test.setup();

    Services.prefs.clearUserPref("places.database.lastMaintenance");
    await PlacesDBUtils.maintenanceOnIdle();

    // Check the lastMaintenance time has been saved.
    Assert.notEqual(
      Services.prefs.getIntPref("places.database.lastMaintenance"),
      null
    );

    await test.check();

    await cleanDatabase();
  }

  // Sanity check: all roots should be intact
  Assert.equal(bs.getFolderIdForItem(bs.placesRoot), 0);
  Assert.equal(bs.getFolderIdForItem(bs.bookmarksMenuFolder), bs.placesRoot);
  Assert.equal(bs.getFolderIdForItem(bs.tagsFolder), bs.placesRoot);
  Assert.equal(bs.getFolderIdForItem(gUnfiledFolderId), bs.placesRoot);
  Assert.equal(bs.getFolderIdForItem(bs.toolbarFolder), bs.placesRoot);
});

// ------------------------------------------------------------------------------

add_task(async function test_idle_daily() {
  const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
  const sandbox = sinon.createSandbox();
  sandbox.stub(PlacesDBUtils, "maintenanceOnIdle");
  Services.prefs.clearUserPref("places.database.lastMaintenance");
  Cc["@mozilla.org/places/databaseUtilsIdleMaintenance;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "idle-daily", "");
  Assert.ok(
    PlacesDBUtils.maintenanceOnIdle.calledOnce,
    "maintenanceOnIdle was invoked"
  );
  sandbox.restore();
});
