_("Make sure various combinations of anno arguments do the right get/set for pages/items");
Cu.import("resource://services-sync/util.js");

function run_test() {
  _("create a bookmark to a url so it exists");
  let url = "about:";
  let bmkid = Svc.Bookmark.insertBookmark(Svc.Bookmark.unfiledBookmarksFolder,
                                          Utils.makeURI(url), -1, "");

  _("set an anno on the bookmark ");
  Utils.anno(bmkid, "anno", "hi");
  do_check_eq(Utils.anno(bmkid, "anno"), "hi");

  _("set an anno on a url");
  Utils.anno(url, "tation", "hello");
  do_check_eq(Utils.anno(url, "tation"), "hello");

  _("make sure getting it also works with a nsIURI");
  let uri = Utils.makeURI(url);
  do_check_eq(Utils.anno(uri, "tation"), "hello");

  _("make sure annotations get updated");
  Utils.anno(uri, "tation", "bye!");
  do_check_eq(Utils.anno(url, "tation"), "bye!");

  _("sanity check that the item anno is still there");
  do_check_eq(Utils.anno(bmkid, "anno"), "hi");

  _("invalid uris don't get annos");
  let didThrow = false;
  try {
    Utils.anno("foo/bar/baz", "bad");
  }
  catch(ex) {
    didThrow = true;
  }
  do_check_true(didThrow);

  _("cleaning up the bookmark we created");
  Svc.Bookmark.removeItem(bmkid);
}
