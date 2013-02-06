/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that each nsINavBookmarksObserver method gets the correct input.

let gBookmarksObserver = {
  expected: [],
  validate: function (aMethodName, aArguments) {
    do_check_eq(this.expected[0].name, aMethodName);

    let args = this.expected.shift().args;
    do_check_eq(aArguments.length, args.length);
    for (let i = 0; i < aArguments.length; i++) {
      do_log_info(aMethodName + "(args[" + i + "]: " + args[i].name + ")");
      do_check_true(args[i].check(aArguments[i]));
    }

    if (this.expected.length == 0) {
      run_next_test();
    }
  },

  // nsINavBookmarkObserver
  onBeginUpdateBatch: function onBeginUpdateBatch()
    this.validate(arguments.callee.name, arguments),
  onEndUpdateBatch: function onEndUpdateBatch()
    this.validate(arguments.callee.name, arguments),
  onItemAdded: function onItemAdded()
    this.validate(arguments.callee.name, arguments),
  onItemRemoved: function onItemRemoved()
    this.validate(arguments.callee.name, arguments),
  onItemChanged: function onItemChanged()
    this.validate(arguments.callee.name, arguments),
  onItemVisited: function onItemVisited()
    this.validate(arguments.callee.name, arguments),
  onItemMoved: function onItemMoved()
    this.validate(arguments.callee.name, arguments),

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavBookmarkObserver]),
}

add_test(function batch() {
  gBookmarksObserver.expected = [
    { name: "onBeginUpdateBatch",
     args: [] },
    { name: "onEndUpdateBatch",
     args: [] },
  ];
  PlacesUtils.bookmarks.runInBatchMode({
    runBatched: function () {
      // Nothing.
    }
  }, null);
});

add_test(function onItemAdded_bookmark() {
  const TITLE = "Bookmark 1";
  let uri = NetUtil.newURI("http://1.mozilla.org/");
  gBookmarksObserver.expected = [
    { name: "onItemAdded",
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "parentId", check: function (v) v === PlacesUtils.unfiledBookmarksFolderId },
        { name: "index", check: function (v) v === 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
        { name: "uri", check: function (v) v instanceof Ci.nsIURI && v.equals(uri) },
        { name: "title", check: function (v) v === TITLE },
        { name: "dateAdded", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
  ];
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       uri, PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       TITLE);
});

add_test(function onItemAdded_separator() {
  gBookmarksObserver.expected = [
    { name: "onItemAdded",
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "parentId", check: function (v) v === PlacesUtils.unfiledBookmarksFolderId },
        { name: "index", check: function (v) v === 1 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_SEPARATOR },
        { name: "uri", check: function (v) v === null },
        { name: "title", check: function (v) v === null },
        { name: "dateAdded", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
  ];
  PlacesUtils.bookmarks.insertSeparator(PlacesUtils.unfiledBookmarksFolderId,
                                        PlacesUtils.bookmarks.DEFAULT_INDEX);
});

add_test(function onItemAdded_folder() {
  const TITLE = "Folder 1";
  gBookmarksObserver.expected = [
    { name: "onItemAdded",
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "parentId", check: function (v) v === PlacesUtils.unfiledBookmarksFolderId },
        { name: "index", check: function (v) v === 2 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_FOLDER },
        { name: "uri", check: function (v) v === null },
        { name: "title", check: function (v) v === TITLE },
        { name: "dateAdded", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
  ];
  PlacesUtils.bookmarks.createFolder(PlacesUtils.unfiledBookmarksFolderId,
                                     TITLE,
                                     PlacesUtils.bookmarks.DEFAULT_INDEX);
});

add_test(function onItemChanged_title_bookmark() {
  let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0);
  let uri = PlacesUtils.bookmarks.getBookmarkURI(id);
  const TITLE = "New title";
  gBookmarksObserver.expected = [
    { name: "onItemChanged", // This is an unfortunate effect of bug 653910.
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "property", check: function (v) v === "title" },
        { name: "isAnno", check: function (v) v === false },
        { name: "newValue", check: function (v) v === TITLE },
        { name: "lastModified", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
        { name: "parentId", check: function (v) v === PlacesUtils.unfiledBookmarksFolderId },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
  ];
  PlacesUtils.bookmarks.setItemTitle(id, TITLE);
});

add_test(function onItemChanged_tags_bookmark() {
  let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0);
  let uri = PlacesUtils.bookmarks.getBookmarkURI(id);
  const TITLE = "New title";
  const TAG = "tag"
  gBookmarksObserver.expected = [
    { name: "onBeginUpdateBatch", // Tag addition uses a batch.
     args: [] },
    { name: "onItemAdded", // This is the tag folder.
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "parentId", check: function (v) v === PlacesUtils.tagsFolderId },
        { name: "index", check: function (v) v === 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_FOLDER },
        { name: "uri", check: function (v) v === null },
        { name: "title", check: function (v) v === TAG },
        { name: "dateAdded", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
    { name: "onItemAdded", // This is the tag.
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "parentId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "index", check: function (v) v === 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
        { name: "uri", check: function (v) v instanceof Ci.nsIURI && v.equals(uri) },
        { name: "title", check: function (v) v === null },
        { name: "dateAdded", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
    { name: "onItemChanged",
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "property", check: function (v) v === "tags" },
        { name: "isAnno", check: function (v) v === false },
        { name: "newValue", check: function (v) v === "" },
        { name: "lastModified", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
        { name: "parentId", check: function (v) v === PlacesUtils.unfiledBookmarksFolderId },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
    { name: "onEndUpdateBatch",
      args: [] },
    { name: "onBeginUpdateBatch", // Tag removal uses a batch.
     args: [] },
    { name: "onItemRemoved", // This is the tag.
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "parentId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "index", check: function (v) v === 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
        { name: "uri", check: function (v) v instanceof Ci.nsIURI && v.equals(uri) },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
    { name: "onItemRemoved", // This is the tag folder.
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "parentId", check: function (v) v === PlacesUtils.tagsFolderId },
        { name: "index", check: function (v) v === 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_FOLDER },
        { name: "uri", check: function (v) v === null },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
    { name: "onItemChanged",
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "property", check: function (v) v === "tags" },
        { name: "isAnno", check: function (v) v === false },
        { name: "newValue", check: function (v) v === "" },
        { name: "lastModified", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
        { name: "parentId", check: function (v) v === PlacesUtils.unfiledBookmarksFolderId },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
    { name: "onEndUpdateBatch",
      args: [] },
  ];
  PlacesUtils.tagging.tagURI(uri, [TAG]);
  PlacesUtils.tagging.untagURI(uri, [TAG]);
});

add_test(function onItemMoved_bookmark() {
  let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0);
  let uri = PlacesUtils.bookmarks.getBookmarkURI(id);
  gBookmarksObserver.expected = [
    { name: "onItemMoved",
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "oldParentId", check: function (v) v === PlacesUtils.unfiledBookmarksFolderId },
        { name: "oldIndex", check: function (v) v === 0 },
        { name: "newParentId", check: function (v) v === PlacesUtils.toolbarFolderId },
        { name: "newIndex", check: function (v) v === 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "oldParentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "newParentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
    { name: "onItemMoved",
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "oldParentId", check: function (v) v === PlacesUtils.toolbarFolderId },
        { name: "oldIndex", check: function (v) v === 0 },
        { name: "newParentId", check: function (v) v === PlacesUtils.unfiledBookmarksFolderId },
        { name: "newIndex", check: function (v) v === 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "oldParentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "newParentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
  ];
  PlacesUtils.bookmarks.moveItem(id, PlacesUtils.toolbarFolderId, 0);
  PlacesUtils.bookmarks.moveItem(id, PlacesUtils.unfiledBookmarksFolderId, 0);
});

add_test(function onItemMoved_bookmark() {
  let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0);
  let uri = PlacesUtils.bookmarks.getBookmarkURI(id);
  gBookmarksObserver.expected = [
    { name: "onItemVisited",
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "visitId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "time", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "transitionType", check: function (v) v === PlacesUtils.history.TRANSITION_TYPED },
        { name: "uri", check: function (v) v instanceof Ci.nsIURI && v.equals(uri) },
        { name: "parentId", check: function (v) v === PlacesUtils.unfiledBookmarksFolderId },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
  ];
  promiseAddVisits({ uri: uri, transition: TRANSITION_TYPED });
});

add_test(function onItemRemoved_bookmark() {
  let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0);
  let uri = PlacesUtils.bookmarks.getBookmarkURI(id);
  gBookmarksObserver.expected = [
    { name: "onItemChanged", // This is an unfortunate effect of bug 653910.
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "property", check: function (v) v === "" },
        { name: "isAnno", check: function (v) v === true },
        { name: "newValue", check: function (v) v === "" },
        { name: "lastModified", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
        { name: "parentId", check: function (v) v === PlacesUtils.unfiledBookmarksFolderId },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
    { name: "onItemRemoved",
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "parentId", check: function (v) v === PlacesUtils.unfiledBookmarksFolderId },
        { name: "index", check: function (v) v === 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
        { name: "uri", check: function (v) v instanceof Ci.nsIURI && v.equals(uri) },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
  ];
  PlacesUtils.bookmarks.removeItem(id);
});

add_test(function onItemRemoved_separator() {
  let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0);
  gBookmarksObserver.expected = [
    { name: "onItemChanged", // This is an unfortunate effect of bug 653910.
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "property", check: function (v) v === "" },
        { name: "isAnno", check: function (v) v === true },
        { name: "newValue", check: function (v) v === "" },
        { name: "lastModified", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_SEPARATOR },
        { name: "parentId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
    { name: "onItemRemoved",
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "parentId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "index", check: function (v) v === 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_SEPARATOR },
        { name: "uri", check: function (v) v === null },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
  ];
  PlacesUtils.bookmarks.removeItem(id);
});

add_test(function onItemRemoved_folder() {
  let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0);
  const TITLE = "Folder 2";
  gBookmarksObserver.expected = [
    { name: "onItemChanged", // This is an unfortunate effect of bug 653910.
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "property", check: function (v) v === "" },
        { name: "isAnno", check: function (v) v === true },
        { name: "newValue", check: function (v) v === "" },
        { name: "lastModified", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_FOLDER },
        { name: "parentId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
    { name: "onItemRemoved",
      args: [
        { name: "itemId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "parentId", check: function (v) typeof(v) == "number" && v > 0 },
        { name: "index", check: function (v) v === 0 },
        { name: "itemType", check: function (v) v === PlacesUtils.bookmarks.TYPE_FOLDER },
        { name: "uri", check: function (v) v === null },
        { name: "guid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
        { name: "parentGuid", check: function (v) typeof(v) == "string" && /^[a-zA-Z0-9\-_]{12}$/.test(v) },
      ] },
  ];
  PlacesUtils.bookmarks.removeItem(id);
});

function run_test() {
  PlacesUtils.bookmarks.addObserver(gBookmarksObserver, false);
  run_next_test();
}

do_register_cleanup(function () {
  PlacesUtils.bookmarks.removeObserver(gBookmarksObserver);
});
