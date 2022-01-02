/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that each bookmark event gets the correct input.

var gUnfiledFolderId;

var gBookmarksObserver = {
  expected: [],
  setup(expected) {
    this.expected = expected;
    this.deferred = PromiseUtils.defer();
    return this.deferred.promise;
  },

  // Even though this isn't technically testing nsINavBookmarkObserver,
  // this is the simplest place to keep this. Once all of the notifications
  // are converted, we can just rename the file.
  validateEvents(events) {
    Assert.greaterOrEqual(this.expected.length, events.length);
    for (let event of events) {
      let expected = this.expected.shift();
      Assert.equal(expected.eventType, event.type);
      let args = expected.args;
      for (let i = 0; i < args.length; i++) {
        Assert.ok(
          args[i].check(event[args[i].name]),
          event.type + "(args[" + i + "]: " + args[i].name + ")"
        );
      }
    }

    if (this.expected.length === 0) {
      this.deferred.resolve();
    }
  },

  handlePlacesEvents(events) {
    this.validateEvents(events);
  },
};

var gBookmarkSkipObserver = {
  skipTags: true,

  expected: null,
  setup(expected) {
    this.expected = expected;
    this.deferred = PromiseUtils.defer();
    return this.deferred.promise;
  },

  validateEvents(events) {
    events = events.filter(e => !e.isTagging);
    Assert.greaterOrEqual(this.expected.length, events.length);
    for (let event of events) {
      let expectedEventType = this.expected.shift();
      Assert.equal(expectedEventType, event.type);
    }

    if (this.expected.length === 0) {
      this.deferred.resolve();
    }
  },

  handlePlacesEvents(events) {
    this.validateEvents(events);
  },
};

add_task(async function setup() {
  gUnfiledFolderId = await PlacesUtils.promiseItemId(
    PlacesUtils.bookmarks.unfiledGuid
  );
  gBookmarksObserver.handlePlacesEvents = gBookmarksObserver.handlePlacesEvents.bind(
    gBookmarksObserver
  );
  gBookmarkSkipObserver.handlePlacesEvents = gBookmarkSkipObserver.handlePlacesEvents.bind(
    gBookmarkSkipObserver
  );
  PlacesUtils.observers.addListener(
    [
      "bookmark-added",
      "bookmark-removed",
      "bookmark-moved",
      "bookmark-tags-changed",
      "bookmark-title-changed",
    ],
    gBookmarksObserver.handlePlacesEvents
  );
  PlacesUtils.observers.addListener(
    [
      "bookmark-added",
      "bookmark-removed",
      "bookmark-moved",
      "bookmark-tags-changed",
      "bookmark-title-changed",
    ],
    gBookmarkSkipObserver.handlePlacesEvents
  );
});

add_task(async function bookmarkItemAdded_bookmark() {
  const title = "Bookmark 1";
  let uri = Services.io.newURI("http://1.mozilla.org/");
  let promise = Promise.all([
    gBookmarkSkipObserver.setup(["bookmark-added"]),
    gBookmarksObserver.setup([
      {
        eventType: "bookmark-added",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => v === gUnfiledFolderId },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK,
          },
          { name: "url", check: v => v == uri.spec },
          { name: "title", check: v => v === title },
          { name: "dateAdded", check: v => typeof v == "number" && v > 0 },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
    ]),
  ]);
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: uri,
    title,
  });
  await promise;
});

add_task(async function bookmarkItemAdded_separator() {
  let promise = Promise.all([
    gBookmarkSkipObserver.setup(["bookmark-added"]),
    gBookmarksObserver.setup([
      {
        eventType: "bookmark-added",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => v === gUnfiledFolderId },
          { name: "index", check: v => v === 1 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_SEPARATOR,
          },
          { name: "url", check: v => v === "" },
          { name: "title", check: v => v === "" },
          { name: "dateAdded", check: v => typeof v == "number" && v > 0 },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
    ]),
  ]);
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
  });
  await promise;
});

add_task(async function bookmarkItemAdded_folder() {
  const title = "Folder 1";
  let promise = Promise.all([
    gBookmarkSkipObserver.setup(["bookmark-added"]),
    gBookmarksObserver.setup([
      {
        eventType: "bookmark-added",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => v === gUnfiledFolderId },
          { name: "index", check: v => v === 2 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER,
          },
          { name: "url", check: v => v === "" },
          { name: "title", check: v => v === title },
          { name: "dateAdded", check: v => typeof v == "number" && v > 0 },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
    ]),
  ]);
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });
  await promise;
});

add_task(async function bookmarkTitleChanged() {
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0,
  });
  const title = "New title";
  let promise = Promise.all([
    gBookmarkSkipObserver.setup(["bookmark-title-changed"]),
    gBookmarksObserver.setup([
      {
        eventType: "bookmark-title-changed",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "title", check: v => v === title },
          { name: "lastModified", check: v => typeof v == "number" && v > 0 },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
    ]),
  ]);
  await PlacesUtils.bookmarks.update({ guid: bm.guid, title });
  await promise;
});

add_task(async function bookmarkTagsChanged() {
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0,
  });
  let uri = Services.io.newURI(bm.url.href);
  const TAG = "tag";
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "bookmark-tags-changed",
      "bookmark-tags-changed",
    ]),
    gBookmarksObserver.setup([
      {
        eventType: "bookmark-added", // This is the tag folder.
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => v === PlacesUtils.tagsFolderId },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER,
          },
          { name: "url", check: v => v === "" },
          { name: "title", check: v => v === TAG },
          { name: "dateAdded", check: v => typeof v == "number" && v > 0 },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
      {
        eventType: "bookmark-added", // This is the tag.
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => typeof v == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK,
          },
          { name: "url", check: v => v == uri.spec },
          { name: "title", check: v => v === "" },
          { name: "dateAdded", check: v => typeof v == "number" && v > 0 },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
      {
        eventType: "bookmark-tags-changed",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK,
          },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          { name: "lastModified", check: v => typeof v == "number" && v > 0 },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
          {
            name: "isTagging",
            check: v => v === false,
          },
        ],
      },
      {
        eventType: "bookmark-removed", // This is the tag.
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => typeof v == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK,
          },
          { name: "url", check: v => v == uri.spec },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
      {
        eventType: "bookmark-tags-changed",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK,
          },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          { name: "lastModified", check: v => typeof v == "number" && v > 0 },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
          {
            name: "isTagging",
            check: v => v === false,
          },
        ],
      },
      {
        eventType: "bookmark-removed", // This is the tag folder.
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => v === PlacesUtils.tagsFolderId },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER,
          },
          { name: "url", check: v => v === "" },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
    ]),
  ]);
  PlacesUtils.tagging.tagURI(uri, [TAG]);
  PlacesUtils.tagging.untagURI(uri, [TAG]);
  await promise;
});

add_task(async function bookmarkItemMoved_bookmark() {
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0,
  });
  let promise = Promise.all([
    gBookmarkSkipObserver.setup(["bookmark-moved", "bookmark-moved"]),
    gBookmarksObserver.setup([
      {
        eventType: "bookmark-moved",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "oldIndex", check: v => v === 0 },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK,
          },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "oldParentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
          { name: "url", check: v => typeof v == "string" },
        ],
      },
      {
        eventType: "bookmark-moved",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "oldIndex", check: v => v === 0 },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK,
          },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "oldParentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
          { name: "url", check: v => typeof v == "string" },
        ],
      },
    ]),
  ]);
  await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0,
  });
  await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0,
  });
  await promise;
});

add_task(async function bookmarkItemRemoved_bookmark() {
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0,
  });
  let uri = Services.io.newURI(bm.url.href);
  let promise = Promise.all([
    gBookmarkSkipObserver.setup(["bookmark-removed"]),
    gBookmarksObserver.setup([
      {
        eventType: "bookmark-removed",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => v === gUnfiledFolderId },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK,
          },
          { name: "url", check: v => v === uri.spec },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
    ]),
  ]);
  await PlacesUtils.bookmarks.remove(bm);
  await promise;
});

add_task(async function bookmarkItemRemoved_separator() {
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0,
  });
  let promise = Promise.all([
    gBookmarkSkipObserver.setup(["bookmark-removed"]),
    gBookmarksObserver.setup([
      {
        eventType: "bookmark-removed",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => typeof v == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_SEPARATOR,
          },
          { name: "url", check: v => v === "" },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
    ]),
  ]);
  await PlacesUtils.bookmarks.remove(bm);
  await promise;
});

add_task(async function bookmarkItemRemoved_folder() {
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0,
  });
  let promise = Promise.all([
    gBookmarkSkipObserver.setup(["bookmark-removed"]),
    gBookmarksObserver.setup([
      {
        eventType: "bookmark-removed",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => typeof v == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER,
          },
          { name: "url", check: v => v === "" },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
    ]),
  ]);
  await PlacesUtils.bookmarks.remove(bm);
  await promise;
});

add_task(async function bookmarkItemRemoved_folder_recursive() {
  const title = "Folder 3";
  const BMTITLE = "Bookmark 1";
  let uri = Services.io.newURI("http://1.mozilla.org/");
  let promise = Promise.all([
    gBookmarkSkipObserver.setup([
      "bookmark-added",
      "bookmark-added",
      "bookmark-added",
      "bookmark-added",
      "bookmark-removed",
      "bookmark-removed",
      "bookmark-removed",
      "bookmark-removed",
    ]),
    gBookmarksObserver.setup([
      {
        eventType: "bookmark-added",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => v === gUnfiledFolderId },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER,
          },
          { name: "url", check: v => v === "" },
          { name: "title", check: v => v === title },
          { name: "dateAdded", check: v => typeof v == "number" && v > 0 },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
      {
        eventType: "bookmark-added",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => typeof v == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK,
          },
          { name: "url", check: v => v == uri.spec },
          { name: "title", check: v => v === BMTITLE },
          { name: "dateAdded", check: v => typeof v == "number" && v > 0 },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
      {
        eventType: "bookmark-added",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => typeof v == "number" && v > 0 },
          { name: "index", check: v => v === 1 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER,
          },
          { name: "url", check: v => v === "" },
          { name: "title", check: v => v === title },
          { name: "dateAdded", check: v => typeof v == "number" && v > 0 },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
      {
        eventType: "bookmark-added",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => typeof v == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK,
          },
          { name: "url", check: v => v == uri.spec },
          { name: "title", check: v => v === BMTITLE },
          { name: "dateAdded", check: v => typeof v == "number" && v > 0 },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
      {
        eventType: "bookmark-removed",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => typeof v == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK,
          },
          { name: "url", check: v => v === uri.spec },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
      {
        eventType: "bookmark-removed",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => typeof v == "number" && v > 0 },
          { name: "index", check: v => v === 1 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER,
          },
          { name: "url", check: v => v === "" },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
      {
        eventType: "bookmark-removed",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => typeof v == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_BOOKMARK,
          },
          { name: "url", check: v => v === uri.spec },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
      {
        eventType: "bookmark-removed",
        args: [
          { name: "id", check: v => typeof v == "number" && v > 0 },
          { name: "parentId", check: v => typeof v == "number" && v > 0 },
          { name: "index", check: v => v === 0 },
          {
            name: "itemType",
            check: v => v === PlacesUtils.bookmarks.TYPE_FOLDER,
          },
          { name: "url", check: v => v === "" },
          {
            name: "guid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "parentGuid",
            check: v => typeof v == "string" && PlacesUtils.isValidGuid(v),
          },
          {
            name: "source",
            check: v =>
              Object.values(PlacesUtils.bookmarks.SOURCES).includes(v),
          },
        ],
      },
    ]),
  ]);
  let folder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: folder.guid,
    url: uri,
    title: BMTITLE,
  });
  let folder2 = await PlacesUtils.bookmarks.insert({
    parentGuid: folder.guid,
    title,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: folder2.guid,
    url: uri,
    title: BMTITLE,
  });

  await PlacesUtils.bookmarks.remove(folder);
  await promise;
});

add_task(function cleanup() {
  PlacesUtils.observers.removeListener(
    [
      "bookmark-added",
      "bookmark-removed",
      "bookmark-moved",
      "bookmark-tags-changed",
      "bookmark-title-changed",
    ],
    gBookmarksObserver.handlePlacesEvents
  );
  PlacesUtils.observers.removeListener(
    [
      "bookmark-added",
      "bookmark-removed",
      "bookmark-moved",
      "bookmark-tags-changed",
      "bookmark-title-changed",
    ],
    gBookmarkSkipObserver.handlePlacesEvents
  );
});
