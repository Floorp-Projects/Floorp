Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/type_records/bookmark.js");
Cu.import("resource://services-sync/util.js");

let store = new BookmarksEngine()._store;
let fxuri = Utils.makeURI("http://getfirefox.com/");
let tburi = Utils.makeURI("http://getthunderbird.com/");

function test_bookmark_create() {
  try {
    _("Ensure the record isn't present yet.");
    let ids = Svc.Bookmark.getBookmarkIdsForURI(fxuri, {});
    do_check_eq(ids.length, 0);

    _("Let's create a new record.");
    let fxrecord = new Bookmark("bookmarks",
                                "{5d81b87c-d5fc-42d9-a114-d69b7342f10e}0");
    fxrecord.bmkUri        = fxuri.spec;
    fxrecord.description   = "Firefox is awesome.";
    fxrecord.title         = "Get Firefox!";
    fxrecord.tags          = ["firefox", "awesome", "browser"];
    fxrecord.keyword       = "awesome";
    fxrecord.loadInSidebar = false;
    fxrecord.parentName    = "Bookmarks Toolbar";
    fxrecord.parentid      = "toolbar";
    store.applyIncoming(fxrecord);

    _("Verify it has been created correctly.");
    let id = Svc.Bookmark.getItemIdForGUID(fxrecord.id);
    do_check_eq(Svc.Bookmark.getItemGUID(id), fxrecord.id);
    do_check_eq(Svc.Bookmark.getItemType(id), Svc.Bookmark.TYPE_BOOKMARK);
    do_check_eq(Svc.Bookmark.getItemTitle(id), fxrecord.title);
    do_check_eq(Svc.Bookmark.getFolderIdForItem(id),
                Svc.Bookmark.toolbarFolder);
    do_check_eq(Svc.Bookmark.getKeywordForBookmark(id), fxrecord.keyword);

    _("Have the store create a new record object. Verify that it has the same data.");
    let newrecord = store.createRecord(fxrecord.id);
    do_check_true(newrecord instanceof Bookmark);
    for each (let property in ["type", "bmkUri", "description", "title",
                               "keyword", "parentName", "parentid"])
      do_check_eq(newrecord[property], fxrecord[property]);
    do_check_true(Utils.deepEquals(newrecord.tags.sort(),
                                   fxrecord.tags.sort()));

    _("The calculated sort index is based on frecency data.");
    do_check_true(newrecord.sortindex >= 150);
  } finally {
    _("Clean up.");
    store.wipe();
  }
}

function test_folder_create() {
  try {
    _("Create a folder.");
    let folder = new BookmarkFolder("bookmarks",
                                    "{5d81b87c-d5fc-42d9-a114-d69b7342f10e}0");
    folder.parentName = "Bookmarks Toolbar";
    folder.parentid   = "toolbar";
    folder.title      = "Test Folder";
    store.applyIncoming(folder);

    _("Verify it has been created correctly.");
    let id = Svc.Bookmark.getItemIdForGUID(folder.id);
    do_check_eq(Svc.Bookmark.getItemType(id), Svc.Bookmark.TYPE_FOLDER);
    do_check_eq(Svc.Bookmark.getItemTitle(id), folder.title);
    do_check_eq(Svc.Bookmark.getFolderIdForItem(id),
                Svc.Bookmark.toolbarFolder);

    _("Have the store create a new record object. Verify that it has the same data.");
    let newrecord = store.createRecord(folder.id);
    do_check_true(newrecord instanceof BookmarkFolder);
    for each (let property in ["title","title", "parentName", "parentid"])
      do_check_eq(newrecord[property], folder[property]);      

    _("Folders have high sort index to ensure they're synced first.");
    do_check_eq(newrecord.sortindex, 1000000);
  } finally {
    _("Clean up.");
    store.wipe();
  }
}

function test_move_folder() {
  try {
    _("Create two folders and a bookmark in one of them.");
    let folder1_id = Svc.Bookmark.createFolder(
      Svc.Bookmark.toolbarFolder, "Folder1", 0);
    let folder1_guid = Svc.Bookmark.getItemGUID(folder1_id);
    let folder2_id = Svc.Bookmark.createFolder(
      Svc.Bookmark.toolbarFolder, "Folder2", 0);
    let folder2_guid = Svc.Bookmark.getItemGUID(folder2_id);
    let bmk_id = Svc.Bookmark.insertBookmark(
      folder1_id, fxuri, Svc.Bookmark.DEFAULT_INDEX, "Get Firefox!");
    let bmk_guid = Svc.Bookmark.getItemGUID(bmk_id);

    _("Get a record, reparent it and apply it to the store.");
    let record = store.createRecord(bmk_guid);
    do_check_eq(record.parentid, folder1_guid);
    record.parentid = folder2_guid;
    record.description = ""; //TODO for some reason we need this
    store.applyIncoming(record);

    _("Verify the new parent.");
    let new_folder_id = Svc.Bookmark.getFolderIdForItem(bmk_id);
    do_check_eq(Svc.Bookmark.getItemGUID(new_folder_id), folder2_guid);
  } finally {
    _("Clean up.");
    store.wipe();
  }
}

function test_move_order() {
  try {
    _("Create two bookmarks");
    let bmk1_id = Svc.Bookmark.insertBookmark(
      Svc.Bookmark.toolbarFolder, fxuri, Svc.Bookmark.DEFAULT_INDEX,
      "Get Firefox!");
    let bmk1_guid = Svc.Bookmark.getItemGUID(bmk1_id);
    let bmk2_id = Svc.Bookmark.insertBookmark(
      Svc.Bookmark.toolbarFolder, tburi, Svc.Bookmark.DEFAULT_INDEX,
      "Get Thunderbird!");
    let bmk2_guid = Svc.Bookmark.getItemGUID(bmk2_id);

    _("Verify order.");
    do_check_eq(Svc.Bookmark.getItemIndex(bmk1_id), 0);
    do_check_eq(Svc.Bookmark.getItemIndex(bmk2_id), 1);
    let rec1 = store.createRecord(bmk1_guid);
    let rec2 = store.createRecord(bmk2_guid);
    do_check_eq(rec1.predecessorid, undefined);
    do_check_eq(rec2.predecessorid, rec1.id);

    _("Move bookmarks around.");
    rec2.predecessorid = null;
    rec1.predecessorid = rec2.id;
    rec2.description = rec1.description = ""; //TODO for some reason we need this
    store.applyIncoming(rec2);
    store.applyIncoming(rec1);

    _("Verify new order.");
    do_check_eq(Svc.Bookmark.getItemIndex(bmk2_id), 0);
    do_check_eq(Svc.Bookmark.getItemIndex(bmk1_id), 1);

  } finally {
    _("Clean up.");
    store.wipe();
  }
}

function run_test() {
  test_bookmark_create();
  test_folder_create();
  test_move_folder();
  test_move_order();
}
