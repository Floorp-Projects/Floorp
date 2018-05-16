/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that each nsINavBookmarksObserver method gets the correct input.

var gBookmarksObserver = {
  expected: [],
  setup(expected) {
    this.expected = expected;
    this.deferred = PromiseUtils.defer();
    return this.deferred.promise;
  },
  validate(aMethodName, aArguments) {
    Assert.equal(this.expected[0].name, aMethodName);

    let args = this.expected.shift().args;
    Assert.equal(aArguments.length, args.length);
    for (let i = 0; i < aArguments.length; i++) {
      Assert.ok(args[i].check(aArguments[i]), aMethodName + "(args[" + i + "]: " + args[i].name + ")");
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
  QueryInterface: ChromeUtils.generateQI([Ci.nsINavBookmarkObserver]),
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
    Assert.equal(this.expected.shift(), aMethodName);
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
  QueryInterface: ChromeUtils.generateQI([Ci.nsINavBookmarkObserver]),
};


add_task(function setup() {
  PlacesUtils.bookmarks.addObserver(gBookmarksObserver);
  PlacesUtils.bookmarks.addObserver(gBookmarkSkipObserver);
});

add_task(async function onItemAdded_bookmark() {
  const title = "Bookmark 1";
  let uri = Services.io.newURI("http://1.mozilla.org/");
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
          { name: "title", check: v => v === title },
          { name: "dateAdded", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: uri,
    title
  });
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
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR
  });
  await promise;
});

add_task(async function onItemAdded_folder() {
  const title = "Folder 1";
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
          { name: "title", check: v => v === title },
          { name: "dateAdded", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title,
    type: PlacesUtils.bookmarks.TYPE_FOLDER
  });
  await promise;
});

add_task(async function onItemChanged_title_bookmark() {
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0
  });
  const title = "New title";
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemChanged"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemChanged",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "property", check: v => v === "title" },
          { name: "isAnno", check: v => v === false },
          { name: "newValue", check: v => v === title },
          { name: "lastModified", check: v => typeof(v) == "number" && v > 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "parentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "oldValue", check: v => typeof(v) == "string" },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  await PlacesUtils.bookmarks.update({ guid: bm.guid, title });
  await promise;
});

add_task(async function onItemChanged_tags_bookmark() {
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0
  });
  let uri = Services.io.newURI(bm.url.href);
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
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
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
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
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
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
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
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemRemoved", // This is the tag folder.
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => v === PlacesUtils.tagsFolderId },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "uri", check: v => v === null },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
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
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "oldValue", check: v => typeof(v) == "string" },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  PlacesUtils.tagging.tagURI(uri, [TAG]);
  PlacesUtils.tagging.untagURI(uri, [TAG]);
  await promise;
});

add_task(async function onItemMoved_bookmark() {
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0
  });
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
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "oldParentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "newParentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
          { name: "url", check: v => typeof(v) == "string" },
        ] },
      { name: "onItemMoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "oldParentId", check: v => v === PlacesUtils.toolbarFolderId },
          { name: "oldIndex", check: v => v === 0 },
          { name: "newParentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "newIndex", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "oldParentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "newParentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
          { name: "url", check: v => typeof(v) == "string" },
        ] },
  ])]);
  await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  });
  await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0
  });
  await promise;
});

add_task(async function onItemMoved_bookmark() {
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0
  });
  let uri = Services.io.newURI(bm.url.href);
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
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
        ] },
  ])]);
  await PlacesTestUtils.addVisits({ uri, transition: TRANSITION_TYPED });
  await promise;
});

add_task(async function onItemRemoved_bookmark() {
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0
  });
  let uri = Services.io.newURI(bm.url.href);
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemRemoved"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemRemoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "uri", check: v => v instanceof Ci.nsIURI && v.equals(uri) },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  await PlacesUtils.bookmarks.remove(bm);
  await promise;
});

add_task(async function onItemRemoved_separator() {
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0
  });
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemRemoved"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemRemoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_SEPARATOR },
          { name: "uri", check: v => v === null },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  await PlacesUtils.bookmarks.remove(bm);
  await promise;
});

add_task(async function onItemRemoved_folder() {
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0
  });
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemRemoved"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemRemoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "uri", check: v => v === null },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  await PlacesUtils.bookmarks.remove(bm);
  await promise;
});

add_task(async function onItemRemoved_folder_recursive() {
  const title = "Folder 3";
  const BMTITLE = "Bookmark 1";
  let uri = Services.io.newURI("http://1.mozilla.org/");
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "onItemAdded", "onItemAdded", "onItemAdded", "onItemAdded",
      "onItemRemoved"
    ]),
    gBookmarksObserver.setup([
      { name: "onItemAdded",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => v === PlacesUtils.unfiledBookmarksFolderId },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "uri", check: v => v === null },
          { name: "title", check: v => v === title },
          { name: "dateAdded", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemAdded",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "uri", check: v => v instanceof Ci.nsIURI && v.equals(uri) },
          { name: "title", check: v => v === BMTITLE },
          { name: "dateAdded", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemAdded",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 1 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "uri", check: v => v === null },
          { name: "title", check: v => v === title },
          { name: "dateAdded", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemAdded",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "uri", check: v => v instanceof Ci.nsIURI && v.equals(uri) },
          { name: "title", check: v => v === BMTITLE },
          { name: "dateAdded", check: v => typeof(v) == "number" && v > 0 },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemRemoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "uri", check: v => v instanceof Ci.nsIURI && v.equals(uri) },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemRemoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 1 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "uri", check: v => v === null },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemRemoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK },
          { name: "uri", check: v => v instanceof Ci.nsIURI && v.equals(uri) },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
      { name: "onItemRemoved",
        args: [
          { name: "itemId", check: v => typeof(v) == "number" && v > 0 },
          { name: "parentId", check: v => typeof(v) == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          { name: "itemType", check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER },
          { name: "uri", check: v => v === null },
          { name: "guid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "parentGuid", check: v => typeof(v) == "string" && PlacesUtils.isValidGuid(v) },
          { name: "source", check: v => Object.values(PlacesUtils.bookmarks.SOURCES).includes(v) },
        ] },
  ])]);
  let folder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title,
    type: PlacesUtils.bookmarks.TYPE_FOLDER
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: folder.guid,
    url: uri,
    title: BMTITLE
  });
  let folder2 = await PlacesUtils.bookmarks.insert({
    parentGuid: folder.guid,
    title,
    type: PlacesUtils.bookmarks.TYPE_FOLDER
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: folder2.guid,
    url: uri,
    title: BMTITLE
  });

  await PlacesUtils.bookmarks.remove(folder);
  await promise;
});

add_task(function cleanup() {
  PlacesUtils.bookmarks.removeObserver(gBookmarksObserver);
  PlacesUtils.bookmarks.removeObserver(gBookmarkSkipObserver);
});
