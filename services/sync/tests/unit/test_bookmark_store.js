Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/util.js");

function run_test() {
  let store = new BookmarksEngine()._store;
  store.wipe();

  try {
    _("Ensure the record isn't present yet.");
    let fxuri = Utils.makeURI("http://getfirefox.com/");
    let ids = Svc.Bookmark.getBookmarkIdsForURI(fxuri, {});
    do_check_eq(ids.length, 0);

    _("Let's create a new record.");
    let fxrecord = {id: "{5d81b87c-d5fc-42d9-a114-d69b7342f10e}0",
                    type: "bookmark",
                    bmkUri: fxuri.spec,
                    title: "Get Firefox!",
                    tags: [],
                    keyword: "awesome",
                    loadInSidebar: false,
                    parentName: "Bookmarks Toolbar",
                    parentid: "toolbar"};
    store.applyIncoming(fxrecord);

    _("Verify it has been created correctly.");
    ids = Svc.Bookmark.getBookmarkIdsForURI(fxuri, {});
    do_check_eq(ids.length, 1);
    let id = ids[0];
    do_check_eq(Svc.Bookmark.getItemGUID(id), fxrecord.id);
    do_check_eq(Svc.Bookmark.getItemType(id), Svc.Bookmark.TYPE_BOOKMARK);
    do_check_eq(Svc.Bookmark.getItemTitle(id), fxrecord.title);
    do_check_eq(Svc.Bookmark.getFolderIdForItem(id),
                Svc.Bookmark.toolbarFolder);
    do_check_eq(Svc.Bookmark.getKeywordForBookmark(id), fxrecord.keyword);

    _("Have the store create a new record object. Verify that it has the same data.");
    let newrecord = store.createRecord(fxrecord.id, "http://fake/uri");
    for each (let property in ["type", "bmkUri", "title", "keyword",
                               "parentName", "parentid"])
      do_check_eq(newrecord[property], fxrecord[property]);      

    _("The calculated sort index is based on frecency data.");
    do_check_true(newrecord.sortindex >= 150);

    _("Folders have high sort index to ensure they're synced first.");
    let folder_id = Svc.Bookmark.createFolder(Svc.Bookmark.toolbarFolder,
                                              "Test Folder", 0);
    let folder_guid = Svc.Bookmark.getItemGUID(folder_id);
    let folder_record = store.createRecord(folder_guid, "http://fake/uri");
    do_check_eq(folder_record.sortindex, 1000000);
  } finally {
    _("Clean up.");
    store.wipe();
  }
}
