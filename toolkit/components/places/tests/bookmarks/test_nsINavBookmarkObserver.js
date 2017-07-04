/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that each nsINavBookmarksObserver method gets the correct input.
Cu.import("resource://gre/modules/PromiseUtils.jsm");

const GUID_RE = /^[a-zA-Z0-9\-_]{12}$/;

var gBookmarksObserver = {
  expected: [],
  setup(expected) {
    this.expected = expected;
    this.deferred = PromiseUtils.defer();
    return this.deferred.promise;
  },
  validate(aMethodName, aArguments) {
    do_check_eq(this.expected[0].name, aMethodName);

    let args = this.expected.shift().args;
    do_check_eq(aArguments.length, args.length);
    for (let i = 0; i < aArguments.length; i++) {
      do_check_true(args[i].check(aArguments[i]), aMethodName + "(args[" + i + "]: " + args[i].name + ")");
    }

    if (this.expected.length === 0) {
      this.deferred.resolve();
    }
  },

  // nsINavBookmarkObserver
  onBeginUpdateBatch() {
    return this.validate("onBeginUpdateBatch", arguments);
  },
  onEndUpdateBatch() {
    return this.validate("onEndUpdateBatch", arguments);
  },
  onItemAdded() {
    return this.validate("onItemAdded", arguments);
  },
  onItemRemoved() {
    return this.validate("onItemRemoved", arguments);
  },
  onItemChanged() {
    return this.validate("onItemChanged", arguments);
  },
  onItemVisited() {
    return this.validate("onItemVisited", arguments);
  },
  onItemMoved() {
    return this.validate("onItemMoved", arguments);
  },

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavBookmarkObserver]),
};

var gBookmarkSkipObserver = {
  skipTags: true,
  skipDescendantsOnItemRemoval: true,

  expected: null,
  setup(expected) {
    this.expected = expected;
    this.deferred = PromiseUtils.defer();
    return this.deferred.promise;
  },
  validate(aMethodName) {
    do_check_eq(this.expected.shift(), aMethodName);
    if (this.expected.length === 0) {
      this.deferred.resolve();
    }
  },

  // nsINavBookmarkObserver
  onBeginUpdateBatch() {
    return this.validate("onBeginUpdateBatch", arguments);
  },
  onEndUpdateBatch() {
    return this.validate("onEndUpdateBatch", arguments);
  },
  onItemAdded() {
    return this.validate("onItemAdded", arguments);
  },
  onItemRemoved() {
    return this.validate("onItemRemoved", arguments);
  },
  onItemChanged() {
    return this.validate("onItemChanged", arguments);
  },
  onItemVisited() {
    return this.validate("onItemVisited", arguments);
  },
  onItemMoved() {
    return this.validate("onItemMoved", arguments);
  },

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavBookmarkObserver]),
};


add_task(function setup() {
  PlacesUtils.bookmarks.addObserver(gBookmarksObserver);
  PlacesUtils.bookmarks.addObserver(gBookmarkSkipObserver);
});

add_task(async function batch() {
  let promise = Promise.all([
    gBookmarksObserver.setup([
      { name: "onBeginUpdateBatch",
       args: [] },
      { name: "onEndUpdateBatch",
       args: [] },
    ]),
    gBookmarkSkipObserver.setup([
      "onBeginUpdateBatch", "onEndUpdateBatch"
  ])]);
  PlacesUtils.bookmarks.runInBatchMode({
    runBatched() {
      // Nothing.
    }
  }, null);
  await promise;
});

add_task(async function onItemAdded_bookmark() {
  const TITLE = "Bookmark 1";
  let uri = NetUtil.newURI("http://1.mozilla.org/");
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemAdded"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemAdded",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "uri", check: v => v instanceof Ci.nsIURI && v.equals(uri) },
          { name: "title", check: v => v === TITLE },
          { name: "dateAdded", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       uri, PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       TITLE);
  await promise;
});

add_task(async function onItemAdded_separator() {
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemAdded"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemAdded",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "index", check: v => v === 1 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_SEPARATOR },
          { name: "uri", check: v => v === null },
          { name: "title", check: v => v === "" },
          { name: "dateAdded", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  PlacesUtils.bookmarks.insertSeparator(PlacesUtils.unfiledBookmarksFolderId,
                                        PlacesUtils.bookmarks.DEFAULT_INDEX);
  await promise;
});

add_task(async function onItemAdded_folder() {
  const TITLE = "Folder 1";
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemAdded"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemAdded",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "index", check: v => v === 2 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "uri", check: v => v === null },
          { name: "title", check: v => v === TITLE },
          { name: "dateAdded", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  PlacesUtils.bookmarks.createFolder(PlacesUtils.unfiledBookmarksFolderId,
                                     TITLE,
                                     PlacesUtils.bookmarks.DEFAULT_INDEX);
  await promise;
});

add_task(async function onItemChanged_title_bookmark() {
  let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0);
  const TITLE = "New title";
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemChanged"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemChanged", // This is an unfortunate effect of bug 653910.
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "property", check: v => v === "title" },
          { name: "isAnno", check: v => v === false },
          { name: "newValue", check: v => v === TITLE },
          { name: "lastModified", check: v => typeof(v) == "number" && v > 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "parentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "oldValue", check: v => typeof(v) == "string" },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  PlacesUtils.bookmarks.setItemTitle(id, TITLE);
  await promise;
});

add_task(async function onItemChanged_tags_bookmark() {
  let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0);
  let uri = PlacesUtils.bookmarks.getBookmarkURI(id);
  const TAG = "tag";
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemChanged", "onItemChanged"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemAdded", // This is the tag folder.
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => v === PlacesUtils.tagsFolderId },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "uri", check: v => v === null },
          { name: "title", check: v => v === TAG },
          { name: "dateAdded", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemAdded", // This is the tag.
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "uri", check: v => v instanceof Ci.nsIURI && v.equals(uri) },
          { name: "title", check: v => v === "" },
          { name: "dateAdded", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemChanged",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "property", check: v => v === "tags" },
          { name: "isAnno", check: v => v === false },
          { name: "newValue", check: v => v === "" },
          { name: "lastModified", check: v => typeof(v) == "number" && v > 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "parentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "oldValue", check: v => typeof(v) == "string" },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemRemoved", // This is the tag.
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "uri", check: v => v instanceof Ci.nsIURI && v.equals(uri) },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemRemoved", // This is the tag folder.
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => v === PlacesUtils.tagsFolderId },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "uri", check: v => v === null },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemChanged",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "property", check: v => v === "tags" },
          { name: "isAnno", check: v => v === false },
          { name: "newValue", check: v => v === "" },
          { name: "lastModified", check: v => typeof(v) == "number" && v > 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "parentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "oldValue", check: v => typeof(v) == "string" },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  PlacesUtils.tagging.tagURI(uri, [TAG]);
  PlacesUtils.tagging.untagURI(uri, [TAG]);
  await promise;
});

add_task(async function onItemMoved_bookmark() {
  let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0);
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemMoved", "onItemMoved"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemMoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "oldParentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "oldIndex", check: v => v === 0 },
          { name: "newParentId", check: v => v === PlacesUtils.toolbarFolderId },
          { name: "newIndex", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "oldParentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "newParentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemMoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "oldParentId", check: v => v === PlacesUtils.toolbarFolderId },
          { name: "oldIndex", check: v => v === 0 },
          { name: "newParentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "newIndex", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "oldParentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "newParentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  PlacesUtils.bookmarks.moveItem(id, PlacesUtils.toolbarFolderId, 0);
  PlacesUtils.bookmarks.moveItem(id, PlacesUtils.unfiledBookmarksFolderId, 0);
  await promise;
});

add_task(async function onItemMoved_bookmark() {
  let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0);
  let uri = PlacesUtils.bookmarks.getBookmarkURI(id);
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemVisited"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemVisited",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "visitId", check: v => typeof(v) == "number" && v > 0 },
          { name: "time", check: v => typeof(v) == "number" && v > 0 },
          { name: "transitionType", check: v => v === PlacesUtils.history.TRANSITION_TYPED },
          { name: "uri", check: v => v instanceof Ci.nsIURI && v.equals(uri) },
          { name: "parentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
        ] },
  ])]);
  PlacesTestUtils.addVisits({ uri, transition: TRANSITION_TYPED });
  await promise;
});

add_task(async function onItemRemoved_bookmark() {
  let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0);
  let uri = PlacesUtils.bookmarks.getBookmarkURI(id);
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemChanged", "onItemRemoved"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemChanged", // This is an unfortunate effect of bug 653910.
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "property", check: v => v === "" },
          { name: "isAnno", check: v => v === true },
          { name: "newValue", check: v => v === "" },
          { name: "lastModified", check: v => typeof(v) == "number" && v > 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "parentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "oldValue", check: v => typeof(v) == "string" },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemRemoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "uri", check: v => v instanceof Ci.nsIURI && v.equals(uri) },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  PlacesUtils.bookmarks.removeItem(id);
  await promise;
});

add_task(async function onItemRemoved_separator() {
  let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0);
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemChanged", "onItemRemoved"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemChanged", // This is an unfortunate effect of bug 653910.
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "property", check: v => v === "" },
          { name: "isAnno", check: v => v === true },
          { name: "newValue", check: v => v === "" },
          { name: "lastModified", check: v => typeof(v) == "number" && v > 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_SEPARATOR },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "oldValue", check: v => typeof(v) == "string" },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemRemoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_SEPARATOR },
          { name: "uri", check: v => v === null },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  PlacesUtils.bookmarks.removeItem(id);
  await promise;
});

add_task(async function onItemRemoved_folder() {
  let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0);
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemChanged", "onItemRemoved"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemChanged", // This is an unfortunate effect of bug 653910.
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "property", check: v => v === "" },
          { name: "isAnno", check: v => v === true },
          { name: "newValue", check: v => v === "" },
          { name: "lastModified", check: v => typeof(v) == "number" && v > 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "oldValue", check: v => typeof(v) == "string" },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemRemoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "uri", check: v => v === null },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  PlacesUtils.bookmarks.removeItem(id);
  await promise;
});

add_task(async function onItemRemoved_folder_recursive() {
  const TITLE = "Folder 3";
  const BMTITLE = "Bookmark 1";
  let uri = NetUtil.newURI("http://1.mozilla.org/");
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemAdded", "onItemAdded", "onItemAdded", "onItemAdded",
      "onItemChanged", "onItemRemoved"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemAdded",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "uri", check: v => v === null },
          { name: "title", check: v => v === TITLE },
          { name: "dateAdded", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemAdded",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => v === PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0) },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "uri", check: v => v instanceof Ci.nsIURI && v.equals(uri) },
          { name: "title", check: v => v === BMTITLE },
          { name: "dateAdded", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemAdded",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => v === PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0) },
          { name: "index", check: v => v === 1 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "uri", check: v => v === null },
          { name: "title", check: v => v === TITLE },
          { name: "dateAdded", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemAdded",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => v === PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0), 1) },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "uri", check: v => v instanceof Ci.nsIURI && v.equals(uri) },
          { name: "title", check: v => v === BMTITLE },
          { name: "dateAdded", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemChanged", // This is an unfortunate effect of bug 653910.
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "property", check: v => v === "" },
          { name: "isAnno", check: v => v === true },
          { name: "newValue", check: v => v === "" },
          { name: "lastModified", check: v => typeof(v) == "number" && v > 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "oldValue", check: v => typeof(v) == "string" },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemRemoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "uri", check: v => v instanceof Ci.nsIURI && v.equals(uri) },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemRemoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 1 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "uri", check: v => v === null },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemRemoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "uri", check: v => v instanceof Ci.nsIURI && v.equals(uri) },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemRemoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "uri", check: v => v === null },
          { name: "guid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && GUID_RE.test(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  let folder = PlacesUtils.bookmarks.createFolder(PlacesUtils.unfiledBookmarksFolderId,
                                                  TITLE,
                                                  PlacesUtils.bookmarks.DEFAULT_INDEX);
  PlacesUtils.bookmarks.insertBookmark(folder,
                                       uri, PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       BMTITLE);
  let folder2 = PlacesUtils.bookmarks.createFolder(folder, TITLE,
                                                   PlacesUtils.bookmarks.DEFAULT_INDEX);
  PlacesUtils.bookmarks.insertBookmark(folder2,
                                       uri, PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       BMTITLE);

  PlacesUtils.bookmarks.removeItem(folder);
  await promise;
});

add_task(function cleanup() {
  PlacesUtils.bookmarks.removeObserver(gBookmarksObserver);
  PlacesUtils.bookmarks.removeObserver(gBookmarkSkipObserver);
});
