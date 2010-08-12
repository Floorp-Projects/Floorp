_("Make sure bad bookmarks can still get their predecessors");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/util.js");

function run_test() {
  let baseuri = "http://fake/uri/";

  _("Starting with a clean slate of no bookmarks");
  let store = new (new BookmarksEngine())._storeObj();
  store.wipe();

  let uri = Utils.makeURI("http://uri/");
  function insert(pos, folder) {
    folder = folder || Svc.Bookmark.toolbarFolder;
    let name = "pos" + pos;
    let bmk = Svc.Bookmark.insertBookmark(folder, uri, pos, name);
    Svc.Bookmark.setItemGUID(bmk, name);
    return bmk;
  }

  _("Creating a couple bookmarks that create holes");
  let first = insert(5);
  let second = insert(10);

  _("Making sure the record created for the first has no predecessor");
  let pos5 = store.createRecord("pos5", baseuri + "pos5");
  do_check_eq(pos5.predecessorid, undefined);

  _("Making sure the second record has the first as its predecessor");
  let pos10 = store.createRecord("pos10", baseuri + "pos10");
  do_check_eq(pos10.predecessorid, "pos5");

  _("Make sure the index of item gets fixed");
  do_check_eq(Svc.Bookmark.getItemIndex(first), 0);
  do_check_eq(Svc.Bookmark.getItemIndex(second), 1);

  _("Make sure things that are in unsorted don't set the predecessor");
  insert(0, Svc.Bookmark.unfiledBookmarksFolder);
  insert(1, Svc.Bookmark.unfiledBookmarksFolder);
  do_check_eq(store.createRecord("pos0", baseuri + "pos0").predecessorid,
              undefined);
  do_check_eq(store.createRecord("pos1", baseuri + "pos1").predecessorid,
              undefined);
}
