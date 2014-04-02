/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

_("Making sure after processing incoming bookmarks, they show up in the right order");
Cu.import("resource://gre/modules/PlacesUtils.jsm", this);
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

function getBookmarks(folderId) {
  let bookmarks = [];

  let pos = 0;
  while (true) {
    let itemId = PlacesUtils.bookmarks.getIdForItemAt(folderId, pos);
    _("Got itemId", itemId, "under", folderId, "at", pos);
    if (itemId == -1)
      break;

    switch (PlacesUtils.bookmarks.getItemType(itemId)) {
      case PlacesUtils.bookmarks.TYPE_BOOKMARK:
        bookmarks.push(PlacesUtils.bookmarks.getItemTitle(itemId));
        break;
      case PlacesUtils.bookmarks.TYPE_FOLDER:
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
  let bookmarks = getBookmarks(PlacesUtils.bookmarks.unfiledBookmarksFolder);

  _("Checking if the bookmark structure is", JSON.stringify(expected));
  _("Got bookmarks:", JSON.stringify(bookmarks));
  do_check_true(Utils.deepEquals(bookmarks, expected));
}

function run_test() {
  let store = new BookmarksEngine(Service)._store;
  initTestLogging("Trace");

  _("Starting with a clean slate of no bookmarks");
  store.wipe();
  check([]);

  function bookmark(name, parent) {
    let bookmark = new Bookmark("http://weave.server/my-bookmark");
    bookmark.id = name;
    bookmark.title = name;
    bookmark.bmkUri = "http://uri/";
    bookmark.parentid = parent || "unfiled";
    bookmark.tags = [];
    return bookmark;
  }

  function folder(name, parent, children) {
    let folder = new BookmarkFolder("http://weave.server/my-bookmark-folder");
    folder.id = name;
    folder.title = name;
    folder.parentid = parent || "unfiled";
    folder.children = children;
    return folder;
  }

  function apply(record) {
    store._childrenToOrder = {};
    store.applyIncoming(record);
    store._orderChildren();
    delete store._childrenToOrder;
  }

  _("basic add first bookmark");
  apply(bookmark("10", ""));
  check(["10"]);

  _("basic append behind 10");
  apply(bookmark("20", ""));
  check(["10", "20"]);

  _("basic create in folder");
  apply(bookmark("31", "f30"));
  let f30 = folder("f30", "", ["31"]);
  apply(f30);
  check(["10", "20", ["31"]]);

  _("insert missing parent -> append to unfiled");
  apply(bookmark("41", "f40"));
  check(["10", "20", ["31"], "41"]);

  _("insert another missing parent -> append");
  apply(bookmark("42", "f40"));
  check(["10", "20", ["31"], "41", "42"]);

  _("insert folder -> move children and followers");
  let f40 = folder("f40", "", ["41", "42"]);
  apply(f40);
  check(["10", "20", ["31"], ["41", "42"]]);

  _("Moving 41 behind 42 -> update f40");
  f40.children = ["42", "41"];
  apply(f40);
  check(["10", "20", ["31"], ["42", "41"]]);

  _("Moving 10 back to front -> update 10, 20");
  f40.children = ["41", "42"];
  apply(f40);
  check(["10", "20", ["31"], ["41", "42"]]);

  _("Moving 20 behind 42 in f40 -> update 50");
  apply(bookmark("20", "f40"));
  check(["10", ["31"], ["41", "42", "20"]]);

  _("Moving 10 in front of 31 in f30 -> update 10, f30");
  apply(bookmark("10", "f30"));
  f30.children = ["10", "31"];
  apply(f30);
  check([["10", "31"], ["41", "42", "20"]]);

  _("Moving 20 from f40 to f30 -> update 20, f30");
  apply(bookmark("20", "f30"));
  f30.children = ["10", "20", "31"];
  apply(f30);
  check([["10", "20", "31"], ["41", "42"]]);

  _("Move 20 back to front -> update 20, f30");
  apply(bookmark("20", ""));
  f30.children = ["10", "31"];
  apply(f30);
  check([["10", "31"], ["41", "42"], "20"]);

}
