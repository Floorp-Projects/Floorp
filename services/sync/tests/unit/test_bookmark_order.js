_("Making sure after processing incoming bookmarks, they show up in the right order");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/type_records/bookmark.js");
Cu.import("resource://services-sync/util.js");

function getBookmarks(folderId) {
  let bookmarks = [];

  let pos = 0;
  while (true) {
    let itemId = Svc.Bookmark.getIdForItemAt(folderId, pos);
    _("Got itemId", itemId, "under", folderId, "at", pos);
    if (itemId == -1)
      break;

    switch (Svc.Bookmark.getItemType(itemId)) {
      case Svc.Bookmark.TYPE_BOOKMARK:
        bookmarks.push(Svc.Bookmark.getItemTitle(itemId));
        break;
      case Svc.Bookmark.TYPE_FOLDER:
        bookmarks.push(getBookmarks(itemId));
        break;
      default:
        _("Unsupported item type..");
    }

    pos++;
  }

  return bookmarks;
}

function check(expected) {
  let bookmarks = getBookmarks(Svc.Bookmark.unfiledBookmarksFolder);

  _("Checking if the bookmark structure is", JSON.stringify(expected));
  _("Got bookmarks:", JSON.stringify(bookmarks));
  do_check_true(Utils.deepEquals(bookmarks, expected));
}

function run_test() {
  _("Starting with a clean slate of no bookmarks");
  let store = new (new BookmarksEngine())._storeObj();
  store.wipe();
  check([]);

  function $B(name, parent, pred) {
    let bookmark = new Bookmark("http://weave.server/my-bookmark");
    bookmark.id = name;
    bookmark.title = name;
    bookmark.bmkUri = "http://uri/";
    bookmark.parentid = parent || "unfiled";
    bookmark.predecessorid = pred;
    bookmark.tags = [];
    store.applyIncoming(bookmark);
  }

  function $F(name, parent, pred) {
    let folder = new BookmarkFolder("http://weave.server/my-bookmark-folder");
    folder.id = name;
    folder.title = name;
    folder.parentid = parent || "unfiled";
    folder.predecessorid = pred;
    store.applyIncoming(folder);
  }

  _("basic add first bookmark");
  $B("10", "");
  check(["10"]);

  _("basic append behind 10");
  $B("20", "", "10");
  check(["10", "20"]);

  _("basic create in folder");
  $F("f30", "", "20");
  $B("31", "f30");
  check(["10", "20", ["31"]]);

  _("insert missing predecessor -> append");
  $B("50", "", "f40");
  check(["10", "20", ["31"], "50"]);

  _("insert missing parent -> append");
  $B("41", "f40");
  check(["10", "20", ["31"], "50", "41"]);

  _("insert another missing parent -> append");
  $B("42", "f40", "41");
  check(["10", "20", ["31"], "50", "41", "42"]);

  _("insert folder -> move children and followers");
  $F("f40", "", "f30");
  check(["10", "20", ["31"], ["41", "42"], "50"]);

  _("Moving 10 behind 50 -> update 10, 20");
  $B("10", "", "50");
  $B("20", "");
  check(["20", ["31"], ["41", "42"], "50", "10"]);

  _("Moving 10 back to front -> update 10, 20");
  $B("10", "");
  $B("20", "", "10");
  check(["10", "20", ["31"], ["41", "42"], "50"]);

  _("Moving 10 behind 50 in different order -> update 20, 10");
  $B("20", "");
  $B("10", "", "50");
  check(["20", ["31"], ["41", "42"], "50", "10"]);

  _("Moving 10 back to front in different order -> update 20, 10");
  $B("20", "", "10");
  $B("10", "");
  check(["10", "20", ["31"], ["41", "42"], "50"]);

  _("Moving 50 behind 42 in f40 -> update 50");
  $B("50", "f40", "42");
  check(["10", "20", ["31"], ["41", "42", "50"]]);

  _("Moving 10 in front of 31 in f30 -> update 10, 20, 31");
  $B("10", "f30");
  $B("20", "");
  $B("31", "f30", "10");
  check(["20", ["10", "31"], ["41", "42", "50"]]);

  _("Moving 20 between 10 and 31 -> update 20, f30, 31");
  $B("20", "f30", "10");
  $F("f30", "");
  $B("31", "f30", "20");
  check([["10", "20", "31"], ["41", "42", "50"]]);

  _("Move 20 back to front -> update 20, f30, 31");
  $B("20", "");
  $F("f30", "", "20");
  $B("31", "f30", "10");
  check(["20", ["10", "31"], ["41", "42", "50"]]);

  _("Moving 20 between 10 and 31 different order -> update f30, 20, 31");
  $F("f30", "");
  $B("20", "f30", "10");
  $B("31", "f30", "20");
  check([["10", "20", "31"], ["41", "42", "50"]]);

  _("Move 20 back to front different order -> update f30, 31, 20");
  $F("f30", "", "20");
  $B("31", "f30", "10");
  $B("20", "");
  check(["20", ["10", "31"], ["41", "42", "50"]]);

  _("Moving 20 between 10 and 31 different order 2 -> update 31, f30, 20");
  $B("31", "f30", "20");
  $F("f30", "");
  $B("20", "f30", "10");
  check([["10", "20", "31"], ["41", "42", "50"]]);

  _("Move 20 back to front different order 2 -> update 31, f30, 20");
  $B("31", "f30", "10");
  $F("f30", "", "20");
  $B("20", "");
  check(["20", ["10", "31"], ["41", "42", "50"]]);

  _("Move 10, 31 to f40 but update in reverse -> update 41, 31, 10");
  $B("41", "f40", "31");
  $B("31", "f40", "10");
  $B("10", "f40");
  check(["20", [], ["10", "31", "41", "42", "50"]]);

  _("Reparent f40 into f30");
  $F("f40", "f30");
  check(["20", [["10", "31", "41", "42", "50"]]]);
}
