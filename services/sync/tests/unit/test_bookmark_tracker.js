Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/util.js");

function run_test() {
  let engine = new BookmarksEngine();
  engine._store.wipe();

  _("Verify we've got an empty tracker to work with.");
  let tracker = engine._tracker;
  do_check_eq([id for (id in tracker.changedIDs)].length, 0);

  let folder = Svc.Bookmark.createFolder(Svc.Bookmark.bookmarksMenuFolder,
                                         "Test Folder",
                                         Svc.Bookmark.DEFAULT_INDEX);
  function createBmk() {
    Svc.Bookmark.insertBookmark(folder,
                                Utils.makeURI("http://getfirefox.com"),
                                Svc.Bookmark.DEFAULT_INDEX,
                                "Get Firefox!");
  }

  try {
    _("Create bookmark. Won't show because we haven't started tracking yet");
    createBmk();
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);

    _("Tell the tracker to start tracking changes.");
    Svc.Obs.notify("weave:engine:start-tracking");
    createBmk();
    do_check_eq([id for (id in tracker.changedIDs)].length, 1);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:start-tracking");
    createBmk();
    do_check_eq([id for (id in tracker.changedIDs)].length, 2);

    _("Let's stop tracking again.");
    tracker.clearChangedIDs();
    Svc.Obs.notify("weave:engine:stop-tracking");
    createBmk();
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:stop-tracking");
    createBmk();
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);
  } finally {
    _("Clean up.");
    engine._store.wipe();
  }
}
