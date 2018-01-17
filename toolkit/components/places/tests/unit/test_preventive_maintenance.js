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

// Include PlacesDBUtils module
Components.utils.import("resource://gre/modules/PlacesDBUtils.jsm");

// Get services and database connection
var hs = PlacesUtils.history;
var bs = PlacesUtils.bookmarks;
var ts = PlacesUtils.tagging;
var as = PlacesUtils.annotations;
var fs = PlacesUtils.favicons;

var mDBConn = hs.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;

// ------------------------------------------------------------------------------
// Helpers

var defaultBookmarksMaxId = 0;
function cleanDatabase() {
  mDBConn.executeSimpleSQL("DELETE FROM moz_places");
  mDBConn.executeSimpleSQL("DELETE FROM moz_historyvisits");
  mDBConn.executeSimpleSQL("DELETE FROM moz_anno_attributes");
  mDBConn.executeSimpleSQL("DELETE FROM moz_annos");
  mDBConn.executeSimpleSQL("DELETE FROM moz_items_annos");
  mDBConn.executeSimpleSQL("DELETE FROM moz_inputhistory");
  mDBConn.executeSimpleSQL("DELETE FROM moz_keywords");
  mDBConn.executeSimpleSQL("DELETE FROM moz_icons");
  mDBConn.executeSimpleSQL("DELETE FROM moz_pages_w_icons");
  mDBConn.executeSimpleSQL("DELETE FROM moz_bookmarks WHERE id > " + defaultBookmarksMaxId);
  mDBConn.executeSimpleSQL("DELETE FROM moz_bookmarks_deleted");
}

function addPlace(aUrl, aFavicon, aGuid = PlacesUtils.history.makeGuid()) {
  let href = new URL(aUrl || "http://www.mozilla.org").href;
  let stmt = mDBConn.createStatement(
    "INSERT INTO moz_places (url, url_hash, guid) VALUES (:url, hash(:url), :guid)");
  stmt.params.url = href;
  stmt.params.guid = aGuid;
  stmt.execute();
  stmt.finalize();
  stmt = mDBConn.createStatement(
    "DELETE FROM moz_updatehostsinsert_temp");
  stmt.execute();
  stmt.finalize();
  let id = mDBConn.lastInsertRowID;
  if (aFavicon) {
    stmt = mDBConn.createStatement(
      "INSERT INTO moz_pages_w_icons (page_url, page_url_hash) VALUES (:url, hash(:url))");
    stmt.params.url = href;
    stmt.execute();
    stmt.finalize();
    stmt = mDBConn.createStatement(
      "INSERT INTO moz_icons_to_pages (page_id, icon_id) " +
      "VALUES ((SELECT id FROM moz_pages_w_icons WHERE page_url_hash = hash(:url)), :favicon)");
    stmt.params.url = href;
    stmt.params.favicon = aFavicon;
    stmt.execute();
    stmt.finalize();
  }
  return id;
}

function addBookmark(aPlaceId, aType, aParent, aKeywordId, aFolderType, aTitle,
                     aGuid = PlacesUtils.history.makeGuid(),
                     aSyncStatus = PlacesUtils.bookmarks.SYNC_STATUS.NEW,
                     aSyncChangeCounter = 0) {
  let stmt = mDBConn.createStatement(
    `INSERT INTO moz_bookmarks (fk, type, parent, keyword_id, folder_type,
                                title, guid, syncStatus, syncChangeCounter)
     VALUES (:place_id, :type, :parent, :keyword_id, :folder_type, :title,
             :guid, :sync_status, :change_counter)`);
  stmt.params.place_id = aPlaceId || null;
  stmt.params.type = aType || bs.TYPE_BOOKMARK;
  stmt.params.parent = aParent || bs.unfiledBookmarksFolder;
  stmt.params.keyword_id = aKeywordId || null;
  stmt.params.folder_type = aFolderType || null;
  stmt.params.title = typeof(aTitle) == "string" ? aTitle : null;
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
  }
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
    this._bookmarkId = addBookmark(this._placeId);
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
  }
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
    this._bookmarkId = addBookmark(this._placeId);
    // Add a used attribute and an unused one.
    let stmt = mDBConn.createStatement("INSERT INTO moz_anno_attributes (name) VALUES (:anno)");
    stmt.params.anno = this._usedPageAttribute;
    stmt.execute();
    stmt.reset();
    stmt.params.anno = this._usedItemAttribute;
    stmt.execute();
    stmt.reset();
    stmt.params.anno = this._unusedAttribute;
    stmt.execute();
    stmt.finalize();

    stmt = mDBConn.createStatement("INSERT INTO moz_annos (place_id, anno_attribute_id) VALUES(:place_id, (SELECT id FROM moz_anno_attributes WHERE name = :anno))");
    stmt.params.place_id = this._placeId;
    stmt.params.anno = this._usedPageAttribute;
    stmt.execute();
    stmt.finalize();
    stmt = mDBConn.createStatement("INSERT INTO moz_items_annos (item_id, anno_attribute_id) VALUES(:item_id, (SELECT id FROM moz_anno_attributes WHERE name = :anno))");
    stmt.params.item_id = this._bookmarkId;
    stmt.params.anno = this._usedItemAttribute;
    stmt.execute();
    stmt.finalize();
  },

  check() {
    // Check that used attributes are still there
    let stmt = mDBConn.createStatement("SELECT id FROM moz_anno_attributes WHERE name = :anno");
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
  }
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
    let stmt = mDBConn.createStatement("INSERT INTO moz_anno_attributes (name) VALUES (:anno)");
    stmt.params.anno = this._usedPageAttribute;
    stmt.execute();
    stmt.finalize();
    stmt = mDBConn.createStatement("INSERT INTO moz_annos (place_id, anno_attribute_id) VALUES(:place_id, (SELECT id FROM moz_anno_attributes WHERE name = :anno))");
    stmt.params.place_id = this._placeId;
    stmt.params.anno = this._usedPageAttribute;
    stmt.execute();
    stmt.finalize();
    // Add an annotation with a nonexistent attribute
    stmt = mDBConn.createStatement("INSERT INTO moz_annos (place_id, anno_attribute_id) VALUES(:place_id, 1337)");
    stmt.params.place_id = this._placeId;
    stmt.execute();
    stmt.finalize();
  },

  check() {
    // Check that used attribute is still there
    let stmt = mDBConn.createStatement("SELECT id FROM moz_anno_attributes WHERE name = :anno");
    stmt.params.anno = this._usedPageAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // check that annotation with valid attribute is still there
    stmt = mDBConn.createStatement("SELECT id FROM moz_annos WHERE anno_attribute_id = (SELECT id FROM moz_anno_attributes WHERE name = :anno)");
    stmt.params.anno = this._usedPageAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // Check that annotation with bogus attribute has been removed
    stmt = mDBConn.createStatement("SELECT id FROM moz_annos WHERE anno_attribute_id = 1337");
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  }
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
    let stmt = mDBConn.createStatement("INSERT INTO moz_anno_attributes (name) VALUES (:anno)");
    stmt.params.anno = this._usedPageAttribute;
    stmt.execute();
    stmt.finalize();
    stmt = mDBConn.createStatement("INSERT INTO moz_annos (place_id, anno_attribute_id) VALUES(:place_id, (SELECT id FROM moz_anno_attributes WHERE name = :anno))");
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
    let stmt = mDBConn.createStatement("SELECT id FROM moz_anno_attributes WHERE name = :anno");
    stmt.params.anno = this._usedPageAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // check that annotation with valid attribute is still there
    stmt = mDBConn.createStatement("SELECT id FROM moz_annos WHERE anno_attribute_id = (SELECT id FROM moz_anno_attributes WHERE name = :anno)");
    stmt.params.anno = this._usedPageAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // Check that an annotation to a nonexistent page has been removed
    stmt = mDBConn.createStatement("SELECT id FROM moz_annos WHERE place_id = 1337");
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  }
});

// ------------------------------------------------------------------------------
tests.push({
  name: "C.1",
  desc: "fix missing Places root",

  setup() {
    // Sanity check: ensure that roots are intact.
    Assert.equal(bs.getFolderIdForItem(bs.placesRoot), 0);
    Assert.equal(bs.getFolderIdForItem(bs.bookmarksMenuFolder), bs.placesRoot);
    Assert.equal(bs.getFolderIdForItem(bs.tagsFolder), bs.placesRoot);
    Assert.equal(bs.getFolderIdForItem(bs.unfiledBookmarksFolder), bs.placesRoot);
    Assert.equal(bs.getFolderIdForItem(bs.toolbarFolder), bs.placesRoot);

    // Remove the root.
    mDBConn.executeSimpleSQL("DELETE FROM moz_bookmarks WHERE parent = 0");
    let stmt = mDBConn.createStatement("SELECT id FROM moz_bookmarks WHERE parent = 0");
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  },

  check() {
    // Ensure the roots have been correctly restored.
    Assert.equal(bs.getFolderIdForItem(bs.placesRoot), 0);
    Assert.equal(bs.getFolderIdForItem(bs.bookmarksMenuFolder), bs.placesRoot);
    Assert.equal(bs.getFolderIdForItem(bs.tagsFolder), bs.placesRoot);
    Assert.equal(bs.getFolderIdForItem(bs.unfiledBookmarksFolder), bs.placesRoot);
    Assert.equal(bs.getFolderIdForItem(bs.toolbarFolder), bs.placesRoot);
  }
});

// ------------------------------------------------------------------------------
tests.push({
  name: "C.2",
  desc: "Fix roots titles",

  setup() {
    // Sanity check: ensure that roots titles are correct. We can use our check.
    this.check();
    // Change some roots' titles.
    bs.setItemTitle(bs.placesRoot, "bad title");
    Assert.equal(bs.getItemTitle(bs.placesRoot), "bad title");
    bs.setItemTitle(bs.unfiledBookmarksFolder, "bad title");
    Assert.equal(bs.getItemTitle(bs.unfiledBookmarksFolder), "bad title");
  },

  check() {
    // Ensure all roots titles are correct.
    Assert.equal(bs.getItemTitle(bs.placesRoot), "");
    Assert.equal(bs.getItemTitle(bs.bookmarksMenuFolder),
                 PlacesUtils.getString("BookmarksMenuFolderTitle"));
    Assert.equal(bs.getItemTitle(bs.tagsFolder),
                 PlacesUtils.getString("TagsFolderTitle"));
    Assert.equal(bs.getItemTitle(bs.unfiledBookmarksFolder),
                 PlacesUtils.getString("OtherBookmarksFolderTitle"));
    Assert.equal(bs.getItemTitle(bs.toolbarFolder),
                 PlacesUtils.getString("BookmarksToolbarFolderTitle"));
  }
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.1",
  desc: "Remove items without a valid place",

  _validItemId: null,
  _invalidItemId: null,
  _invalidSyncedItemId: null,
  placeId: null,

  setup() {
    // Add a place to ensure place_id = 1 is valid
    this.placeId = addPlace();
    // Insert a valid bookmark
    this._validItemId = addBookmark(this.placeId);
    // Insert a bookmark with an invalid place
    this._invalidItemId = addBookmark(1337);
    // Insert a synced bookmark with an invalid place. We should write a
    // tombstone when we remove it.
    this._invalidSyncedItemId = addBookmark(1337, null, null, null, null, null,
      "bookmarkAAAA", PlacesUtils.bookmarks.SYNC_STATUS.NORMAL);
  },

  async check() {
    // Check that valid bookmark is still there
    let stmt = mDBConn.createStatement("SELECT id FROM moz_bookmarks WHERE id = :item_id");
    stmt.params.item_id = this._validItemId;
    Assert.ok(stmt.executeStep());
    stmt.reset();
    // Check that invalid bookmark has been removed
    stmt.params.item_id = this._invalidItemId;
    Assert.ok(!stmt.executeStep());
    stmt.reset();
    stmt.params.item_id = this._invalidSyncedItemId;
    Assert.ok(!stmt.executeStep());
    stmt.finalize();

    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    Assert.deepEqual(tombstones.map(info => info.guid), ["bookmarkAAAA"]);
  }
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.2",
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
    this._bookmarkId = addBookmark(this._placeId, bs.TYPE_BOOKMARK, this._tagId);
    // Insert a separator in the tag
    this._separatorId = addBookmark(null, bs.TYPE_SEPARATOR, this._tagId);
    // Insert a folder in the tag
    this._folderId = addBookmark(null, bs.TYPE_FOLDER, this._tagId);
  },

  check() {
    // Check that valid bookmark is still there
    let stmt = mDBConn.createStatement("SELECT id FROM moz_bookmarks WHERE type = :type AND parent = :parent");
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
  }
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.3",
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
    this._bookmarkId = addBookmark(this._placeId, bs.TYPE_BOOKMARK, this._tagId);
    // Create another tag (empty)
    this._emptyTagId = addBookmark(null, bs.TYPE_FOLDER, bs.tagsFolder);
  },

  check() {
    // Check that valid bookmark is still there
    let stmt = mDBConn.createStatement("SELECT id FROM moz_bookmarks WHERE id = :id AND type = :type AND parent = :parent");
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
  }
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.4",
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
    this._bookmarkId = addBookmark(this._placeId, bs.TYPE_BOOKMARK, this._orphanFolderId);
  },

  async check() {
    // Check that bookmarks are now children of a real folder (unfiled)
    let expectedInfos = [{
      id: this._orphanBookmarkId,
      parent: bs.unfiledBookmarksFolder,
      syncChangeCounter: 1,
    }, {
      id: this._orphanSeparatorId,
      parent: bs.unfiledBookmarksFolder,
      syncChangeCounter: 1,
    }, {
      id: this._orphanFolderId,
      parent: bs.unfiledBookmarksFolder,
      syncChangeCounter: 1,
    }, {
      id: this._bookmarkId,
      parent: this._orphanFolderId,
      syncChangeCounter: 0,
    }, {
      id: bs.unfiledBookmarksFolder,
      parent: bs.placesRoot,
      syncChangeCounter: 3,
    }];
    let db = await PlacesUtils.promiseDBConnection();
    for (let { id, parent, syncChangeCounter } of expectedInfos) {
      let rows = await db.executeCached(`
        SELECT id, syncChangeCounter
        FROM moz_bookmarks
        WHERE id = :item_id AND parent = :parent`,
        { item_id: id, parent });
      Assert.equal(rows.length, 1);

      let actualChangeCounter = rows[0].getResultByName("syncChangeCounter");
      Assert.equal(actualChangeCounter, syncChangeCounter);
    }
  }
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.6",
  desc: "Fix wrong item types | bookmarks",

  _separatorId: null,
  _folderId: null,
  _placeId: null,

  setup() {
    // Add a place to ensure place_id = 1 is valid
    this._placeId = addPlace();
    // Add a separator with a fk
    this._separatorId = addBookmark(this._placeId, bs.TYPE_SEPARATOR);
    // Add a folder with a fk
    this._folderId = addBookmark(this._placeId, bs.TYPE_FOLDER);
  },

  check() {
    // Check that items with an fk have been converted to bookmarks
    let stmt = mDBConn.createStatement("SELECT id FROM moz_bookmarks WHERE id = :item_id AND type = :type");
    stmt.params.item_id = this._separatorId;
    stmt.params.type = bs.TYPE_BOOKMARK;
    Assert.ok(stmt.executeStep());
    stmt.reset();
    stmt.params.item_id = this._folderId;
    stmt.params.type = bs.TYPE_BOOKMARK;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
  }
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.7",
  desc: "Fix wrong item types | bookmarks",

  _validBookmarkId: null,
  _invalidBookmarkId: null,
  _placeId: null,

  setup() {
    // Add a place to ensure place_id = 1 is valid
    this._placeId = addPlace();
    // Add a bookmark with a valid place id
    this._validBookmarkId = addBookmark(this._placeId, bs.TYPE_BOOKMARK);
    // Add a bookmark with a null place id
    this._invalidBookmarkId = addBookmark(null, bs.TYPE_BOOKMARK);
  },

  check() {
    // Check valid bookmark
    let stmt = mDBConn.createStatement(`
      SELECT id, syncChangeCounter
      FROM moz_bookmarks
      WHERE id = :item_id AND type = :type`);
    stmt.params.item_id = this._validBookmarkId;
    stmt.params.type = bs.TYPE_BOOKMARK;
    Assert.ok(stmt.executeStep());
    Assert.equal(stmt.row.syncChangeCounter, 0);
    stmt.reset();
    // Check invalid bookmark has been converted to a folder
    stmt.params.item_id = this._invalidBookmarkId;
    stmt.params.type = bs.TYPE_FOLDER;
    Assert.ok(stmt.executeStep());
    Assert.equal(stmt.row.syncChangeCounter, 1);
    stmt.finalize();
  }
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.9",
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
    this._bookmarkId1 = addBookmark(this._placeId, bs.TYPE_BOOKMARK, this._bookmarkId);
    this._bookmarkId2 = addBookmark(this._placeId, bs.TYPE_BOOKMARK, this._separatorId);
  },

  async check() {
    // Check that bookmarks are now children of a real folder (unfiled)
    let expectedInfos = [{
      id: this._bookmarkId1,
      parent: bs.unfiledBookmarksFolder,
      syncChangeCounter: 1,
    }, {
      id: this._bookmarkId2,
      parent: bs.unfiledBookmarksFolder,
      syncChangeCounter: 1,
    }, {
      id: bs.unfiledBookmarksFolder,
      parent: bs.placesRoot,
      syncChangeCounter: 2,
    }];
    let db = await PlacesUtils.promiseDBConnection();
    for (let { id, parent, syncChangeCounter } of expectedInfos) {
      let rows = await db.executeCached(`
        SELECT id, syncChangeCounter
        FROM moz_bookmarks
        WHERE id = :item_id AND parent = :parent`,
        { item_id: id, parent });
      Assert.equal(rows.length, 1);

      let actualChangeCounter = rows[0].getResultByName("syncChangeCounter");
      Assert.equal(actualChangeCounter, syncChangeCounter);
    }
  }
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.10",
  desc: "Recalculate positions",

  _unfiledBookmarks: [],
  _toolbarBookmarks: [],

  async setup() {
    const NUM_BOOKMARKS = 20;
    bs.runInBatchMode({
      runBatched(aUserData) {
        // Add bookmarks to two folders to better perturbate the table.
        for (let i = 0; i < NUM_BOOKMARKS; i++) {
          bs.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                            NetUtil.newURI("http://example.com/"),
                            bs.DEFAULT_INDEX, "testbookmark", null,
                            PlacesUtils.bookmarks.SOURCES.SYNC);
        }
        for (let i = 0; i < NUM_BOOKMARKS; i++) {
          bs.insertBookmark(PlacesUtils.toolbarFolderId,
                            NetUtil.newURI("http://example.com/"),
                            bs.DEFAULT_INDEX, "testbookmark", null,
                            PlacesUtils.bookmarks.SOURCES.SYNC);
        }
      }
    }, null);

    function randomize_positions(aParent, aResultArray) {
      let stmt = mDBConn.createStatement(
        `UPDATE moz_bookmarks SET position = :rand
         WHERE id IN (
           SELECT id FROM moz_bookmarks WHERE parent = :parent
           ORDER BY RANDOM() LIMIT 1
         )`
      );
      for (let i = 0; i < (NUM_BOOKMARKS / 2); i++) {
        stmt.params.parent = aParent;
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
      stmt.params.parent = aParent;
      while (stmt.executeStep()) {
        aResultArray.push(stmt.row.id);
        print(stmt.row.id + "\t" + stmt.row.position + "\t" +
              (aResultArray.length - 1));
      }
      stmt.finalize();
    }

    // Set random positions for the added bookmarks.
    randomize_positions(PlacesUtils.unfiledBookmarksFolderId,
                        this._unfiledBookmarks);
    randomize_positions(PlacesUtils.toolbarFolderId, this._toolbarBookmarks);

    let syncInfos = await PlacesTestUtils.fetchBookmarkSyncFields(
      PlacesUtils.bookmarks.unfiledGuid, PlacesUtils.bookmarks.toolbarGuid);
    Assert.ok(syncInfos.every(info => info.syncChangeCounter === 0));
  },

  async check() {
    let db = await PlacesUtils.promiseDBConnection();

    async function check_order(aParent, aResultArray) {
      // Build the expected ordered list of bookmarks.
      let childRows = await db.executeCached(
        `SELECT id, position, syncChangeCounter FROM moz_bookmarks
         WHERE parent = :parent
         ORDER BY position ASC`,
        { parent: aParent }
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
        { parent: aParent });
      for (let row of parentRows) {
        let actualChangeCounter = row.getResultByName("syncChangeCounter");
        Assert.ok(actualChangeCounter > 0);
      }
    }

    await check_order(PlacesUtils.unfiledBookmarksFolderId, this._unfiledBookmarks);
    await check_order(PlacesUtils.toolbarFolderId, this._toolbarBookmarks);
  }
});

// ------------------------------------------------------------------------------

tests.push({
  name: "D.12",
  desc: "Fix empty-named tags",
  _taggedItemIds: {},

  setup() {
    // Add a place to ensure place_id = 1 is valid
    let placeId = addPlace();
    // Create a empty-named tag.
    this._untitledTagId = addBookmark(null, bs.TYPE_FOLDER, bs.tagsFolder, null, null, "");
    // Insert a bookmark in the tag, otherwise it will be removed.
    addBookmark(placeId, bs.TYPE_BOOKMARK, this._untitledTagId);
    // Create a empty-named folder.
    this._untitledFolderId = addBookmark(null, bs.TYPE_FOLDER, bs.toolbarFolder, null, null, "");
    // Create a titled tag.
    this._titledTagId = addBookmark(null, bs.TYPE_FOLDER, bs.tagsFolder, null, null, "titledTag");
    // Insert a bookmark in the tag, otherwise it will be removed.
    addBookmark(placeId, bs.TYPE_BOOKMARK, this._titledTagId);
    // Create a titled folder.
    this._titledFolderId = addBookmark(null, bs.TYPE_FOLDER, bs.toolbarFolder, null, null, "titledFolder");

    // Create two tagged bookmarks in different folders.
    this._taggedItemIds.inMenu = addBookmark(placeId, bs.TYPE_BOOKMARK,
      bs.bookmarksMenuFolder, null, null, "Tagged bookmark in menu");
    this._taggedItemIds.inToolbar = addBookmark(placeId, bs.TYPE_BOOKMARK,
      bs.toolbarFolder, null, null, "Tagged bookmark in toolbar");
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
  }
});

// ------------------------------------------------------------------------------

tests.push({
  name: "E.1",
  desc: "Remove orphan icon entries",

  _placeId: null,

  setup() {
    // Insert favicon entries
    let stmt = mDBConn.createStatement("INSERT INTO moz_icons (id, icon_url, fixed_icon_url_hash) VALUES(:favicon_id, :url, hash(fixup_url(:url)))");
    stmt.params.favicon_id = 1;
    stmt.params.url = "http://www1.mozilla.org/favicon.ico";
    stmt.execute();
    stmt.reset();
    stmt.params.favicon_id = 2;
    stmt.params.url = "http://www2.mozilla.org/favicon.ico";
    stmt.execute();
    stmt.finalize();
    // Insert orphan page.
    stmt = mDBConn.createStatement("INSERT INTO moz_pages_w_icons (id, page_url, page_url_hash) VALUES(:page_id, :url, hash(:url))");
    stmt.params.page_id = 99;
    stmt.params.url = "http://w99.mozilla.org/";
    stmt.execute();
    stmt.finalize();

    // Insert a place using the existing favicon entry
    this._placeId = addPlace("http://www.mozilla.org", 1);
  },

  check() {
    // Check that used icon is still there
    let stmt = mDBConn.createStatement("SELECT id FROM moz_icons WHERE id = :favicon_id");
    stmt.params.favicon_id = 1;
    Assert.ok(stmt.executeStep());
    stmt.reset();
    // Check that unused icon has been removed
    stmt.params.favicon_id = 2;
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
    // Check that the orphan page is gone.
    stmt = mDBConn.createStatement("SELECT id FROM moz_pages_w_icons WHERE id = :page_id");
    stmt.params.page_id = 99;
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  }
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
    let stmt = mDBConn.createStatement("INSERT INTO moz_historyvisits(place_id) VALUES (:place_id)");
    stmt.params.place_id = this._placeId;
    stmt.execute();
    stmt.reset();
    stmt.params.place_id = this._invalidPlaceId;
    stmt.execute();
    stmt.finalize();
  },

  check() {
    // Check that valid visit is still there
    let stmt = mDBConn.createStatement("SELECT id FROM moz_historyvisits WHERE place_id = :place_id");
    stmt.params.place_id = this._placeId;
    Assert.ok(stmt.executeStep());
    stmt.reset();
    // Check that invalid visit has been removed
    stmt.params.place_id = this._invalidPlaceId;
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  }
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
    let stmt = mDBConn.createStatement("INSERT INTO moz_inputhistory (place_id, input) VALUES (:place_id, :input)");
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
    let stmt = mDBConn.createStatement("SELECT place_id FROM moz_inputhistory WHERE place_id = :place_id");
    stmt.params.place_id = this._placeId;
    Assert.ok(stmt.executeStep());
    stmt.reset();
    // Check that inputhistory on invalid place has gone
    stmt.params.place_id = this._invalidPlaceId;
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  }
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
    this._bookmarkId = addBookmark(this._placeId);
    // Add a used attribute.
    let stmt = mDBConn.createStatement("INSERT INTO moz_anno_attributes (name) VALUES (:anno)");
    stmt.params.anno = this._usedItemAttribute;
    stmt.execute();
    stmt.finalize();
    stmt = mDBConn.createStatement("INSERT INTO moz_items_annos (item_id, anno_attribute_id) VALUES(:item_id, (SELECT id FROM moz_anno_attributes WHERE name = :anno))");
    stmt.params.item_id = this._bookmarkId;
    stmt.params.anno = this._usedItemAttribute;
    stmt.execute();
    stmt.finalize();
    // Add an annotation with a nonexistent attribute
    stmt = mDBConn.createStatement("INSERT INTO moz_items_annos (item_id, anno_attribute_id) VALUES(:item_id, 1337)");
    stmt.params.item_id = this._bookmarkId;
    stmt.execute();
    stmt.finalize();
  },

  check() {
    // Check that used attribute is still there
    let stmt = mDBConn.createStatement("SELECT id FROM moz_anno_attributes WHERE name = :anno");
    stmt.params.anno = this._usedItemAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // check that annotation with valid attribute is still there
    stmt = mDBConn.createStatement("SELECT id FROM moz_items_annos WHERE anno_attribute_id = (SELECT id FROM moz_anno_attributes WHERE name = :anno)");
    stmt.params.anno = this._usedItemAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // Check that annotation with bogus attribute has been removed
    stmt = mDBConn.createStatement("SELECT id FROM moz_items_annos WHERE anno_attribute_id = 1337");
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  }
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
    this._bookmarkId = addBookmark(this._placeId);
    // Add a used attribute.
    let stmt = mDBConn.createStatement("INSERT INTO moz_anno_attributes (name) VALUES (:anno)");
    stmt.params.anno = this._usedItemAttribute;
    stmt.execute();
    stmt.finalize();
    stmt = mDBConn.createStatement("INSERT INTO moz_items_annos (item_id, anno_attribute_id) VALUES (:item_id, (SELECT id FROM moz_anno_attributes WHERE name = :anno))");
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
    let stmt = mDBConn.createStatement("SELECT id FROM moz_anno_attributes WHERE name = :anno");
    stmt.params.anno = this._usedItemAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // check that annotation with valid attribute is still there
    stmt = mDBConn.createStatement("SELECT id FROM moz_items_annos WHERE anno_attribute_id = (SELECT id FROM moz_anno_attributes WHERE name = :anno)");
    stmt.params.anno = this._usedItemAttribute;
    Assert.ok(stmt.executeStep());
    stmt.finalize();
    // Check that an annotation to a nonexistent page has been removed
    stmt = mDBConn.createStatement("SELECT id FROM moz_items_annos WHERE item_id = 8888");
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  }
});


// ------------------------------------------------------------------------------

tests.push({
  name: "I.1",
  desc: "Remove unused keywords",

  _bookmarkId: null,
  _placeId: null,

  setup() {
    // Insert 2 keywords
    let stmt = mDBConn.createStatement("INSERT INTO moz_keywords (id, keyword, place_id) VALUES(:id, :keyword, :place_id)");
    stmt.params.id = 1;
    stmt.params.keyword = "unused";
    stmt.params.place_id = 100;
    stmt.execute();
    stmt.finalize();
  },

  check() {
    // Check that "used" keyword is still there
    let stmt = mDBConn.createStatement("SELECT id FROM moz_keywords WHERE keyword = :keyword");
    // Check that "unused" keyword has gone
    stmt.params.keyword = "unused";
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  }
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
      `SELECT h.id FROM moz_places h
       JOIN moz_historyvisits v ON v.place_id = h.id AND visit_type NOT IN (0,4,7,8,9)
       GROUP BY h.id HAVING h.visit_count <> count(*)
       UNION ALL
       SELECT h.id FROM moz_places h
       JOIN moz_historyvisits v ON v.place_id = h.id
       GROUP BY h.id HAVING h.last_visit_date <> MAX(v.visit_date)`
    );
    Assert.ok(!stmt.executeStep());
    stmt.finalize();
  }
});

// ------------------------------------------------------------------------------

tests.push({
  name: "L.3",
  desc: "recalculate hidden for redirects.",

  async setup() {
    await PlacesTestUtils.addVisits([
      { uri: NetUtil.newURI("http://l3.moz.org/"),
        transition: TRANSITION_TYPED },
      { uri: NetUtil.newURI("http://l3.moz.org/redirecting/"),
        transition: TRANSITION_TYPED },
      { uri: NetUtil.newURI("http://l3.moz.org/redirecting2/"),
        transition: TRANSITION_REDIRECT_TEMPORARY,
        referrer: NetUtil.newURI("http://l3.moz.org/redirecting/") },
      { uri: NetUtil.newURI("http://l3.moz.org/target/"),
        transition: TRANSITION_REDIRECT_PERMANENT,
        referrer: NetUtil.newURI("http://l3.moz.org/redirecting2/") },
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
          for (let row; (row = aResultSet.getNextRow());) {
            let url = row.getResultByIndex(0);
            Assert.ok(/redirecting/.test(url));
            this._count++;
          }
        },
        handleError(aError) {
        },
        handleCompletion(aReason) {
          Assert.equal(aReason, Ci.mozIStorageStatementCallback.REASON_FINISHED);
          Assert.equal(this._count, 2);
          resolve();
        }
      });
      stmt.finalize();
    });
  }
});

// ------------------------------------------------------------------------------

tests.push({
  name: "L.4",
  desc: "recalculate foreign_count.",

  async setup() {
    this._pageGuid = (await PlacesUtils.history.insert({ url: "http://l4.moz.org/",
                                                         visits: [{ date: new Date() }] })).guid;
    await PlacesUtils.bookmarks.insert({ url: "http://l4.moz.org/",
                                         parentGuid: PlacesUtils.bookmarks.unfiledGuid});
    await PlacesUtils.keywords.insert({ url: "http://l4.moz.org/", keyword: "kw" });
    Assert.equal((await this._getForeignCount()), 2);
  },

  async _getForeignCount() {
    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.execute(`SELECT foreign_count FROM moz_places
                                 WHERE guid = :guid`, { guid: this._pageGuid });
    return rows[0].getResultByName("foreign_count");
  },

  async check() {
    Assert.equal((await this._getForeignCount()), 2);
  }
});

// ------------------------------------------------------------------------------

tests.push({
  name: "L.5",
  desc: "recalculate hashes when missing.",

  async setup() {
    this._pageGuid = (await PlacesUtils.history.insert({ url: "http://l5.moz.org/",
                                                         visits: [{ date: new Date() }] })).guid;
    Assert.ok((await this._getHash()) > 0);
    await PlacesUtils.withConnectionWrapper("change url hash", async function(db) {
      await db.execute(`UPDATE moz_places SET url_hash = 0`);
    });
    Assert.equal((await this._getHash()), 0);
  },

  async _getHash() {
    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.execute(`SELECT url_hash FROM moz_places
                                 WHERE guid = :guid`, { guid: this._pageGuid });
    return rows[0].getResultByName("url_hash");
  },

  async check() {
    Assert.ok((await this._getHash()) > 0);
  }
});

// ------------------------------------------------------------------------------

tests.push({
  name: "L.6",
  desc: "fix invalid Place GUIDs",
  _placeIds: [],

  async setup() {
    let placeWithValidGuid = addPlace("http://example.com/a", null,
                                      "placeAAAAAAA");
    this._placeIds.push(placeWithValidGuid);

    let placeWithEmptyGuid = addPlace("http://example.com/b", null, "");
    this._placeIds.push(placeWithEmptyGuid);

    let placeWithoutGuid = addPlace("http://example.com/c", null, null);
    this._placeIds.push(placeWithoutGuid);

    let placeWithInvalidGuid = addPlace("http://example.com/c", null,
                                        "{123456}");
    this._placeIds.push(placeWithInvalidGuid);
  },

  async check() {
    let db = await PlacesUtils.promiseDBConnection();
    let updatedRows = await db.execute(`
      SELECT id, guid
      FROM moz_places
      WHERE id IN (?, ?, ?, ?)`,
      this._placeIds);

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
      null, PlacesUtils.bookmarks.TYPE_FOLDER,
      PlacesUtils.bookmarks.bookmarksMenuFolder, /* aKeywordId */ null,
      /* aFolderType */ null, "NORMAL folder with invalid GUID",
      "{123456}", PlacesUtils.bookmarks.SYNC_STATUS.NORMAL);

    let placeIdForBookmarkWithoutGuid = addPlace();
    let bookmarkWithoutGuid = addBookmark(
      placeIdForBookmarkWithoutGuid, PlacesUtils.bookmarks.TYPE_BOOKMARK,
      folderWithInvalidGuid, /* aKeywordId */ null,
      /* aFolderType */ null, "NEW bookmark without GUID",
      /* aGuid */ null);

    let placeIdForBookmarkWithInvalidGuid = addPlace();
    let bookmarkWithInvalidGuid = addBookmark(
      placeIdForBookmarkWithInvalidGuid, PlacesUtils.bookmarks.TYPE_BOOKMARK,
      folderWithInvalidGuid, /* aKeywordId */ null,
      /* aFolderType */ null, "NORMAL bookmark with invalid GUID",
      "bookmarkAAAA\n", PlacesUtils.bookmarks.SYNC_STATUS.NORMAL);

    let placeIdForBookmarkWithValidGuid = addPlace();
    let bookmarkWithValidGuid = addBookmark(
      placeIdForBookmarkWithValidGuid, PlacesUtils.bookmarks.TYPE_BOOKMARK,
      folderWithInvalidGuid, /* aKeywordId */ null,
      /* aFolderType */ null, "NORMAL bookmark with valid GUID",
      "bookmarkBBBB", PlacesUtils.bookmarks.SYNC_STATUS.NORMAL);

    this._bookmarkInfos.push({
      id: PlacesUtils.bookmarks.bookmarksMenuFolder,
      syncChangeCounter: 1,
      syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
    }, {
      id: folderWithInvalidGuid,
      syncChangeCounter: 3,
      syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NEW,
    }, {
      id: bookmarkWithoutGuid,
      syncChangeCounter: 1,
      syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NEW,
    }, {
      id: bookmarkWithInvalidGuid,
      syncChangeCounter: 1,
      syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NEW,
    }, {
      id: bookmarkWithValidGuid,
      syncChangeCounter: 0,
      syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
    });
  },

  async check() {
    let db = await PlacesUtils.promiseDBConnection();
    let updatedRows = await db.execute(`
      SELECT id, guid, syncChangeCounter, syncStatus
      FROM moz_bookmarks
      WHERE id IN (?, ?, ?, ?, ?)`,
      this._bookmarkInfos.map(info => info.id));

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
    Assert.deepEqual(tombstones.map(info => info.guid),
      ["bookmarkAAAA\n", "{123456}"]);
  },
});

tests.push({
  name: "S.2",
  desc: "drop tombstones for bookmarks that aren't deleted",

  async setup() {
    addBookmark(null, bs.TYPE_BOOKMARK, bs.bookmarksMenuFolder, null, null,
                "", "bookmarkAAAA");

    await PlacesUtils.withConnectionWrapper("Insert tombstones", db =>
      db.executeTransaction(async function() {
        for (let guid of ["bookmarkAAAA", "bookmarkBBBB"]) {
          await db.executeCached(`
            INSERT INTO moz_bookmarks_deleted(guid)
            VALUES(:guid)`,
            { guid });
        }
      })
    );
  },

  async check() {
    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    Assert.deepEqual(tombstones.map(info => info.guid), ["bookmarkBBBB"]);
  },
});

tests.push({
  name: "S.3",
  desc: "set missing added and last modified dates",
  _placeVisits: [],
  _bookmarksWithDates: [],

  async setup() {
    let placeIdWithVisits = addPlace();
    this._placeVisits.push({
      placeId: placeIdWithVisits,
      visitDate: PlacesUtils.toPRTime(new Date(2017, 9, 4)),
    }, {
      placeId: placeIdWithVisits,
      visitDate: PlacesUtils.toPRTime(new Date(2017, 9, 8)),
    });

    this._bookmarksWithDates.push({
      guid: "bookmarkAAAA",
      placeId: null,
      parentId: bs.bookmarksMenuFolder,
      dateAdded: null,
      lastModified: PlacesUtils.toPRTime(new Date(2017, 9, 1)),
    }, {
      guid: "bookmarkBBBB",
      placeId: null,
      parentId: bs.bookmarksMenuFolder,
      dateAdded: PlacesUtils.toPRTime(new Date(2017, 9, 2)),
      lastModified: null,
    }, {
      guid: "bookmarkCCCC",
      placeId: null,
      parentId: bs.unfiledBookmarksFolder,
      dateAdded: null,
      lastModified: null,
    }, {
      guid: "bookmarkDDDD",
      placeId: placeIdWithVisits,
      parentId: bs.mobileFolder,
      dateAdded: null,
      lastModified: null,
    }, {
      guid: "bookmarkEEEE",
      placeId: placeIdWithVisits,
      parentId: bs.unfiledBookmarksFolder,
      dateAdded: PlacesUtils.toPRTime(new Date(2017, 9, 3)),
      lastModified: PlacesUtils.toPRTime(new Date(2017, 9, 6)),
    });

    await PlacesUtils.withConnectionWrapper(
      "Insert bookmarks and visits with dates",
      db => db.executeTransaction(async () => {
        await db.executeCached(`
          INSERT INTO moz_historyvisits(place_id, visit_date)
          VALUES(:placeId, :visitDate)`,
          this._placeVisits);

        await db.executeCached(`
          INSERT INTO moz_bookmarks(fk, type, parent, guid, dateAdded,
                                    lastModified)
          VALUES(:placeId, 1, :parentId, :guid, :dateAdded,
                 :lastModified)`,
          this._bookmarksWithDates);
      })
    );
  },

  async check() {
    let db = await PlacesUtils.promiseDBConnection();
    let updatedRows = await db.executeCached(`
      SELECT guid, dateAdded, lastModified
      FROM moz_bookmarks
      WHERE guid IN (?, ?, ?, ?, ?)`,
      this._bookmarksWithDates.map(info => info.guid));

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

        // Neither date added nor last modified exists, and no visits, so we
        // should fall back to the current time for both.
        case "bookmarkCCCC": {
          let nowAsPRTime = PlacesUtils.toPRTime(new Date());
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
      }
    }
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
    await PlacesTestUtils.addVisits([
      { uri: this._uri1 },
      { uri: this._uri2 },
    ]);

    let bookmarks = await bs.insertTree({
      guid: bs.toolbarGuid,
      children: [{
        title: "testfolder",
        type: bs.TYPE_FOLDER,
        children: [{
          title: "testbookmark",
          url: this._uri1,
        }]
      }]
    });

    this._folder = bookmarks[0];
    this._bookmark = bookmarks[1];
    this._bookmarkId = await PlacesUtils.promiseItemId(bookmarks[1].guid);

    this._separator = await bs.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    });

    ts.tagURI(this._uri1, ["testtag"]);
    fs.setAndFetchFaviconForPage(this._uri2, SMALLPNG_DATA_URI, false,
                                 PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
                                 null,
                                 Services.scriptSecurityManager.getSystemPrincipal());
    await PlacesUtils.keywords.insert({ url: this._uri1.spec, keyword: "testkeyword" });
    as.setPageAnnotation(this._uri2, "anno", "anno", 0, as.EXPIRE_NEVER);
    as.setItemAnnotation(this._bookmarkId, "anno", "anno", 0, as.EXPIRE_NEVER);
  },

  async check() {
    // Check that all items are correct
    let isVisited = await promiseIsURIVisited(this._uri1);
    Assert.ok(isVisited);
    isVisited = await promiseIsURIVisited(this._uri2);
    Assert.ok(isVisited);

    Assert.equal((await bs.fetch(this._bookmark.guid)).url, this._uri1.spec);
    let folder = await bs.fetch(this._folder.guid);
    Assert.equal(folder.index, 0);
    Assert.equal(folder.type, bs.TYPE_FOLDER);
    Assert.equal((await bs.fetch(this._separator.guid)).type, bs.TYPE_SEPARATOR);

    Assert.equal(ts.getTagsForURI(this._uri1).length, 1);
    Assert.equal((await PlacesUtils.keywords.fetch({ url: this._uri1.spec })).keyword, "testkeyword");
    Assert.equal(as.getPageAnnotation(this._uri2, "anno"), "anno");
    Assert.equal(as.getItemAnnotation(this._bookmarkId, "anno"), "anno");

    await new Promise(resolve => {
      fs.getFaviconURLForPage(this._uri2, aFaviconURI => {
        Assert.ok(aFaviconURI.equals(SMALLPNG_DATA_URI));
        resolve();
      });
    });
  }
});

// ------------------------------------------------------------------------------

add_task(async function test_preventive_maintenance() {
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
    Assert.notEqual(Services.prefs.getIntPref("places.database.lastMaintenance"), null);

    await test.check();

    cleanDatabase();
  }

  // Sanity check: all roots should be intact
  Assert.equal(bs.getFolderIdForItem(bs.placesRoot), 0);
  Assert.equal(bs.getFolderIdForItem(bs.bookmarksMenuFolder), bs.placesRoot);
  Assert.equal(bs.getFolderIdForItem(bs.tagsFolder), bs.placesRoot);
  Assert.equal(bs.getFolderIdForItem(bs.unfiledBookmarksFolder), bs.placesRoot);
  Assert.equal(bs.getFolderIdForItem(bs.toolbarFolder), bs.placesRoot);
});
