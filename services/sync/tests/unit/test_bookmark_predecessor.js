_("Make sure bad bookmarks can still get their predecessors");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/engines/bookmarks.js");

function run_test() {
  _("Starting with a clean slate of no bookmarks");
  let store = new (new BookmarksEngine())._storeObj();
  store.wipe();

  let unfiled = Svc.Bookmark.unfiledBookmarksFolder;
  let uri = Utils.makeURI("http://uri/");
  function insert(pos) {
    let name = "pos" + pos;
    let bmk = Svc.Bookmark.insertBookmark(unfiled, uri, pos, name);
    Svc.Bookmark.setItemGUID(bmk, name);
    return bmk;
  }

  _("Creating a couple bookmarks that create holes");
  let first = insert(5);
  let second = insert(10);

  _("Making sure the record created for the first has no predecessor");
  let pos5 = store.createRecord("pos5");
  do_check_eq(pos5.predecessorid, undefined);

  _("Making sure the second record has the first as its predecessor");
  let pos10 = store.createRecord("pos10");
  do_check_eq(pos10.predecessorid, "pos5");

  _("Make sure the index of item gets fixed");
  do_check_eq(Svc.Bookmark.getItemIndex(first), 0);
  do_check_eq(Svc.Bookmark.getItemIndex(second), 1);
}
