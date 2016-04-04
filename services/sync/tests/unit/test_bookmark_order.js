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
  let id10 = "10_aaaaaaaaa";
  _("basic add first bookmark");
  apply(bookmark(id10, ""));
  check([id10]);
  let id20 = "20_aaaaaaaaa";
  _("basic append behind 10");
  apply(bookmark(id20, ""));
  check([id10, id20]);

  let id31 = "31_aaaaaaaaa";
  let id30 = "f30_aaaaaaaa";
  _("basic create in folder");
  apply(bookmark(id31, id30));
  let f30 = folder(id30, "", [id31]);
  apply(f30);
  check([id10, id20, [id31]]);

  let id41 = "41_aaaaaaaaa";
  let id40 = "f40_aaaaaaaa";
  _("insert missing parent -> append to unfiled");
  apply(bookmark(id41, id40));
  check([id10, id20, [id31], id41]);

  let id42 = "42_aaaaaaaaa";

  _("insert another missing parent -> append");
  apply(bookmark(id42, id40));
  check([id10, id20, [id31], id41, id42]);

  _("insert folder -> move children and followers");
  let f40 = folder(id40, "", [id41, id42]);
  apply(f40);
  check([id10, id20, [id31], [id41, id42]]);

  _("Moving 41 behind 42 -> update f40");
  f40.children = [id42, id41];
  apply(f40);
  check([id10, id20, [id31], [id42, id41]]);

  _("Moving 10 back to front -> update 10, 20");
  f40.children = [id41, id42];
  apply(f40);
  check([id10, id20, [id31], [id41, id42]]);

  _("Moving 20 behind 42 in f40 -> update 50");
  apply(bookmark(id20, id40));
  check([id10, [id31], [id41, id42, id20]]);

  _("Moving 10 in front of 31 in f30 -> update 10, f30");
  apply(bookmark(id10, id30));
  f30.children = [id10, id31];
  apply(f30);
  check([[id10, id31], [id41, id42, id20]]);

  _("Moving 20 from f40 to f30 -> update 20, f30");
  apply(bookmark(id20, id30));
  f30.children = [id10, id20, id31];
  apply(f30);
  check([[id10, id20, id31], [id41, id42]]);

  _("Move 20 back to front -> update 20, f30");
  apply(bookmark(id20, ""));
  f30.children = [id10, id31];
  apply(f30);
  check([[id10, id31], [id41, id42], id20]);

}
