Cu.import("resource://gre/modules/PlacesSyncUtils.jsm");
Cu.import("resource://testing-common/httpd.js");
Cu.importGlobalProperties(["crypto", "URLSearchParams"]);

const DESCRIPTION_ANNO = "bookmarkProperties/description";
const LOAD_IN_SIDEBAR_ANNO = "bookmarkProperties/loadInSidebar";
const SYNC_PARENT_ANNO = "sync/parent";

function makeGuid() {
  return ChromeUtils.base64URLEncode(crypto.getRandomValues(new Uint8Array(9)), {
    pad: false,
  });
}

function makeLivemarkServer() {
  let server = new HttpServer();
  server.registerPrefixHandler("/feed/", do_get_file("./livemark.xml"));
  server.start(-1);
  return {
    server,
    get site() {
      let { identity } = server;
      let host = identity.primaryHost.includes(":") ?
        `[${identity.primaryHost}]` : identity.primaryHost;
      return `${identity.primaryScheme}://${host}:${identity.primaryPort}`;
    },
    stopServer() {
      return new Promise(resolve => server.stop(resolve));
    },
  };
}

function shuffle(array) {
  let results = [];
  for (let i = 0; i < array.length; ++i) {
    let randomIndex = Math.floor(Math.random() * (i + 1));
    results[i] = results[randomIndex];
    results[randomIndex] = array[i];
  }
  return results;
}

function compareAscending(a, b) {
  if (a > b) {
    return 1;
  }
  if (a < b) {
    return -1;
  }
  return 0;
}

function assertTagForURLs(tag, urls, message) {
  let taggedURLs = PlacesUtils.tagging.getURIsForTag(tag).map(uri => uri.spec);
  deepEqual(taggedURLs.sort(compareAscending), urls.sort(compareAscending), message);
}

function assertURLHasTags(url, tags, message) {
  let actualTags = PlacesUtils.tagging.getTagsForURI(uri(url));
  deepEqual(actualTags.sort(compareAscending), tags, message);
}

var populateTree = Task.async(function* populate(parentGuid, ...items) {
  let guids = {};

  for (let index = 0; index < items.length; index++) {
    let item = items[index];
    let guid = makeGuid();

    switch (item.kind) {
      case "bookmark":
      case "query":
        yield PlacesUtils.bookmarks.insert({
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          url: item.url,
          title: item.title,
          parentGuid, guid, index,
        });
        break;

      case "separator":
        yield PlacesUtils.bookmarks.insert({
          type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
          parentGuid, guid,
        });
        break;

      case "folder":
        yield PlacesUtils.bookmarks.insert({
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          title: item.title,
          parentGuid, guid,
        });
        if (item.children) {
          Object.assign(guids, yield* populate(guid, ...item.children));
        }
        break;

      default:
        throw new Error(`Unsupported item type: ${item.type}`);
    }

    if (item.exclude) {
      let itemId = yield PlacesUtils.promiseItemId(guid);
      PlacesUtils.annotations.setItemAnnotation(
        itemId, PlacesUtils.EXCLUDE_FROM_BACKUP_ANNO, "Don't back this up", 0,
        PlacesUtils.annotations.EXPIRE_NEVER);
    }

    guids[item.title] = guid;
  }

  return guids;
});

var syncIdToId = Task.async(function* syncIdToId(syncId) {
  let guid = yield PlacesSyncUtils.bookmarks.syncIdToGuid(syncId);
  return PlacesUtils.promiseItemId(guid);
});

add_task(function* test_order() {
  do_print("Insert some bookmarks");
  let guids = yield populateTree(PlacesUtils.bookmarks.menuGuid, {
    kind: "bookmark",
    title: "childBmk",
    url: "http://getfirefox.com",
  }, {
    kind: "bookmark",
    title: "siblingBmk",
    url: "http://getthunderbird.com",
  }, {
    kind: "folder",
    title: "siblingFolder",
  }, {
    kind: "separator",
    title: "siblingSep",
  });

  do_print("Reorder inserted bookmarks");
  {
    let order = [guids.siblingFolder, guids.siblingSep, guids.childBmk,
      guids.siblingBmk];
    yield PlacesSyncUtils.bookmarks.order(PlacesUtils.bookmarks.menuGuid, order);
    let childSyncIds = yield PlacesSyncUtils.bookmarks.fetchChildSyncIds(PlacesUtils.bookmarks.menuGuid);
    deepEqual(childSyncIds, order, "New bookmarks should be reordered according to array");
  }

  do_print("Reorder with unspecified children");
  {
    yield PlacesSyncUtils.bookmarks.order(PlacesUtils.bookmarks.menuGuid, [
      guids.siblingSep, guids.siblingBmk,
    ]);
    let childSyncIds = yield PlacesSyncUtils.bookmarks.fetchChildSyncIds(
      PlacesUtils.bookmarks.menuGuid);
    deepEqual(childSyncIds, [guids.siblingSep, guids.siblingBmk,
      guids.siblingFolder, guids.childBmk],
      "Unordered children should be moved to end");
  }

  do_print("Reorder with nonexistent children");
  {
    yield PlacesSyncUtils.bookmarks.order(PlacesUtils.bookmarks.menuGuid, [
      guids.childBmk, makeGuid(), guids.siblingBmk, guids.siblingSep,
      makeGuid(), guids.siblingFolder, makeGuid()]);
    let childSyncIds = yield PlacesSyncUtils.bookmarks.fetchChildSyncIds(
      PlacesUtils.bookmarks.menuGuid);
    deepEqual(childSyncIds, [guids.childBmk, guids.siblingBmk, guids.siblingSep,
      guids.siblingFolder], "Nonexistent children should be ignored");
  }

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_changeGuid_invalid() {
  yield rejects(PlacesSyncUtils.bookmarks.changeGuid(makeGuid()),
    "Should require a new GUID");
  yield rejects(PlacesSyncUtils.bookmarks.changeGuid(makeGuid(), "!@#$"),
    "Should reject invalid GUIDs");
  yield rejects(PlacesSyncUtils.bookmarks.changeGuid(makeGuid(), makeGuid()),
    "Should reject nonexistent item GUIDs");
  yield rejects(
    PlacesSyncUtils.bookmarks.changeGuid(PlacesUtils.bookmarks.menuGuid,
      makeGuid()),
    "Should reject roots");
});

add_task(function* test_changeGuid() {
  let item = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: "https://mozilla.org",
  });
  let id = yield PlacesUtils.promiseItemId(item.guid);

  let newGuid = makeGuid();
  let result = yield PlacesSyncUtils.bookmarks.changeGuid(item.guid, newGuid);
  equal(result, newGuid, "Should return new GUID");

  equal(yield PlacesUtils.promiseItemId(newGuid), id, "Should map ID to new GUID");
  yield rejects(PlacesUtils.promiseItemId(item.guid), "Should not map ID to old GUID");
  equal(yield PlacesUtils.promiseItemGuid(id), newGuid, "Should map new GUID to ID");
});

add_task(function* test_order_roots() {
  let oldOrder = yield PlacesSyncUtils.bookmarks.fetchChildSyncIds(
    PlacesUtils.bookmarks.rootGuid);
  yield PlacesSyncUtils.bookmarks.order(PlacesUtils.bookmarks.rootGuid,
    shuffle(oldOrder));
  let newOrder = yield PlacesSyncUtils.bookmarks.fetchChildSyncIds(
    PlacesUtils.bookmarks.rootGuid);
  deepEqual(oldOrder, newOrder, "Should ignore attempts to reorder roots");

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_update_tags() {
  do_print("Insert item without tags");
  let item = yield PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    url: "https://mozilla.org",
    syncId: makeGuid(),
    parentSyncId: "menu",
  });

  do_print("Add tags");
  {
    let updatedItem = yield PlacesSyncUtils.bookmarks.update({
      syncId: item.syncId,
      tags: ["foo", "bar"],
    });
    deepEqual(updatedItem.tags, ["foo", "bar"], "Should return new tags");
    assertURLHasTags("https://mozilla.org", ["bar", "foo"],
      "Should set new tags for URL");
  }

  do_print("Add new tag, remove existing tag");
  {
    let updatedItem = yield PlacesSyncUtils.bookmarks.update({
      syncId: item.syncId,
      tags: ["foo", "baz"],
    });
    deepEqual(updatedItem.tags, ["foo", "baz"], "Should return updated tags");
    assertURLHasTags("https://mozilla.org", ["baz", "foo"],
      "Should update tags for URL");
    assertTagForURLs("bar", [], "Should remove existing tag");
  }

  do_print("Tags with whitespace");
  {
    let updatedItem = yield PlacesSyncUtils.bookmarks.update({
      syncId: item.syncId,
      tags: [" leading", "trailing ", " baz ", " "],
    });
    deepEqual(updatedItem.tags, ["leading", "trailing", "baz"],
      "Should return filtered tags");
    assertURLHasTags("https://mozilla.org", ["baz", "leading", "trailing"],
      "Should trim whitespace and filter blank tags");
  }

  do_print("Remove all tags");
  {
    let updatedItem = yield PlacesSyncUtils.bookmarks.update({
      syncId: item.syncId,
      tags: null,
    });
    deepEqual(updatedItem.tags, [], "Should return empty tag array");
    assertURLHasTags("https://mozilla.org", [],
      "Should remove all existing tags");
  }

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_update_keyword() {
  do_print("Insert item without keyword");
  let item = yield PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    parentSyncId: "menu",
    url: "https://mozilla.org",
    syncId: makeGuid(),
  });

  do_print("Add item keyword");
  {
    let updatedItem = yield PlacesSyncUtils.bookmarks.update({
      syncId: item.syncId,
      keyword: "moz",
    });
    equal(updatedItem.keyword, "moz", "Should return new keyword");
    let entryByKeyword = yield PlacesUtils.keywords.fetch("moz");
    equal(entryByKeyword.url.href, "https://mozilla.org/",
      "Should set new keyword for URL");
    let entryByURL = yield PlacesUtils.keywords.fetch({
      url: "https://mozilla.org",
    });
    equal(entryByURL.keyword, "moz", "Looking up URL should return new keyword");
  }

  do_print("Change item keyword");
  {
    let updatedItem = yield PlacesSyncUtils.bookmarks.update({
      syncId: item.syncId,
      keyword: "m",
    });
    equal(updatedItem.keyword, "m", "Should return updated keyword");
    let newEntry = yield PlacesUtils.keywords.fetch("m");
    equal(newEntry.url.href, "https://mozilla.org/", "Should update keyword for URL");
    let oldEntry = yield PlacesUtils.keywords.fetch("moz");
    ok(!oldEntry, "Should remove old keyword");
  }

  do_print("Remove existing keyword");
  {
    let updatedItem = yield PlacesSyncUtils.bookmarks.update({
      syncId: item.syncId,
      keyword: null,
    });
    ok(!updatedItem.keyword,
      "Should not include removed keyword in properties");
    let entry = yield PlacesUtils.keywords.fetch({
      url: "https://mozilla.org",
    });
    ok(!entry, "Should remove new keyword from URL");
  }

  do_print("Remove keyword for item without keyword");
  {
    yield PlacesSyncUtils.bookmarks.update({
      syncId: item.syncId,
      keyword: null,
    });
    let entry = yield PlacesUtils.keywords.fetch({
      url: "https://mozilla.org",
    });
    ok(!entry,
      "Removing keyword for URL without existing keyword should succeed");
  }

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_update_annos() {
  let guids = yield populateTree(PlacesUtils.bookmarks.menuGuid, {
    kind: "folder",
    title: "folder",
  }, {
    kind: "bookmark",
    title: "bmk",
    url: "https://example.com",
  });

  do_print("Add folder description");
  {
    let updatedItem = yield PlacesSyncUtils.bookmarks.update({
      syncId: guids.folder,
      description: "Folder description",
    });
    equal(updatedItem.description, "Folder description",
      "Should return new description");
    let id = yield syncIdToId(updatedItem.syncId);
    equal(PlacesUtils.annotations.getItemAnnotation(id, DESCRIPTION_ANNO),
      "Folder description", "Should set description anno");
  }

  do_print("Clear folder description");
  {
    let updatedItem = yield PlacesSyncUtils.bookmarks.update({
      syncId: guids.folder,
      description: null,
    });
    ok(!updatedItem.description, "Should not return cleared description");
    let id = yield syncIdToId(updatedItem.syncId);
    ok(!PlacesUtils.annotations.itemHasAnnotation(id, DESCRIPTION_ANNO),
      "Should remove description anno");
  }

  do_print("Add bookmark sidebar anno");
  {
    let updatedItem = yield PlacesSyncUtils.bookmarks.update({
      syncId: guids.bmk,
      loadInSidebar: true,
    });
    ok(updatedItem.loadInSidebar, "Should return sidebar anno");
    let id = yield syncIdToId(updatedItem.syncId);
    ok(PlacesUtils.annotations.itemHasAnnotation(id, LOAD_IN_SIDEBAR_ANNO),
      "Should set sidebar anno for existing bookmark");
  }

  do_print("Clear bookmark sidebar anno");
  {
    let updatedItem = yield PlacesSyncUtils.bookmarks.update({
      syncId: guids.bmk,
      loadInSidebar: false,
    });
    ok(!updatedItem.loadInSidebar, "Should not return cleared sidebar anno");
    let id = yield syncIdToId(updatedItem.syncId);
    ok(!PlacesUtils.annotations.itemHasAnnotation(id, LOAD_IN_SIDEBAR_ANNO),
      "Should clear sidebar anno for existing bookmark");
  }

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_update_move_root() {
  do_print("Move root to same parent");
  {
    // This should be a no-op.
    let sameRoot = yield PlacesSyncUtils.bookmarks.update({
      syncId: "menu",
      parentSyncId: "places",
    });
    equal(sameRoot.syncId, "menu",
      "Menu root GUID should not change");
    equal(sameRoot.parentSyncId, "places",
      "Parent Places root GUID should not change");
  }

  do_print("Try reparenting root");
  yield rejects(PlacesSyncUtils.bookmarks.update({
    syncId: "menu",
    parentSyncId: "toolbar",
  }));

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_insert() {
  do_print("Insert bookmark");
  {
    let item = yield PlacesSyncUtils.bookmarks.insert({
      kind: "bookmark",
      syncId: makeGuid(),
      parentSyncId: "menu",
      url: "https://example.org",
    });
    let { type } = yield PlacesUtils.bookmarks.fetch({ guid: item.syncId });
    equal(type, PlacesUtils.bookmarks.TYPE_BOOKMARK,
      "Bookmark should have correct type");
  }

  do_print("Insert query");
  {
    let item = yield PlacesSyncUtils.bookmarks.insert({
      kind: "query",
      syncId: makeGuid(),
      parentSyncId: "menu",
      url: "place:terms=term&folder=TOOLBAR&queryType=1",
      folder: "Saved search",
    });
    let { type } = yield PlacesUtils.bookmarks.fetch({ guid: item.syncId });
    equal(type, PlacesUtils.bookmarks.TYPE_BOOKMARK,
      "Queries should be stored as bookmarks");
  }

  do_print("Insert folder");
  {
    let item = yield PlacesSyncUtils.bookmarks.insert({
      kind: "folder",
      syncId: makeGuid(),
      parentSyncId: "menu",
      title: "New folder",
    });
    let { type } = yield PlacesUtils.bookmarks.fetch({ guid: item.syncId });
    equal(type, PlacesUtils.bookmarks.TYPE_FOLDER,
      "Folder should have correct type");
  }

  do_print("Insert separator");
  {
    let item = yield PlacesSyncUtils.bookmarks.insert({
      kind: "separator",
      syncId: makeGuid(),
      parentSyncId: "menu",
    });
    let { type } = yield PlacesUtils.bookmarks.fetch({ guid: item.syncId });
    equal(type, PlacesUtils.bookmarks.TYPE_SEPARATOR,
      "Separator should have correct type");
  }

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_insert_livemark() {
  let { site, stopServer } = makeLivemarkServer();

  try {
    do_print("Insert livemark with feed URL");
    {
      let livemark = yield PlacesSyncUtils.bookmarks.insert({
        kind: "livemark",
        syncId: makeGuid(),
        feed: site + "/feed/1",
        parentSyncId: "menu",
      });
      let bmk = yield PlacesUtils.bookmarks.fetch({
        guid: yield PlacesSyncUtils.bookmarks.syncIdToGuid(livemark.syncId),
      })
      equal(bmk.type, PlacesUtils.bookmarks.TYPE_FOLDER,
        "Livemarks should be stored as folders");
    }

    let livemarkSyncId;
    do_print("Insert livemark with site and feed URLs");
    {
      let livemark = yield PlacesSyncUtils.bookmarks.insert({
        kind: "livemark",
        syncId: makeGuid(),
        site,
        feed: site + "/feed/1",
        parentSyncId: "menu",
      });
      livemarkSyncId = livemark.syncId;
    }

    do_print("Try inserting livemark into livemark");
    {
      let livemark = yield PlacesSyncUtils.bookmarks.insert({
        kind: "livemark",
        syncId: makeGuid(),
        site,
        feed: site + "/feed/1",
        parentSyncId: livemarkSyncId,
      });
      ok(!livemark, "Should not insert livemark as child of livemark");
    }
  } finally {
    yield stopServer();
  }

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_update_livemark() {
  let { site, stopServer } = makeLivemarkServer();
  let feedURI = uri(site + "/feed/1");

  try {
    // We shouldn't reinsert the livemark if the URLs are the same.
    do_print("Update livemark with same URLs");
    {
      let livemark = yield PlacesUtils.livemarks.addLivemark({
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        feedURI,
        siteURI: uri(site),
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      });

      yield PlacesSyncUtils.bookmarks.update({
        syncId: livemark.guid,
        feed: feedURI,
      });
      // `nsLivemarkService` returns references to `Livemark` instances, so we
      // can compare them with `==` to make sure they haven't been replaced.
      equal(yield PlacesUtils.livemarks.getLivemark({
        guid: livemark.guid,
      }), livemark, "Livemark with same feed URL should not be replaced");

      yield PlacesSyncUtils.bookmarks.update({
        syncId: livemark.guid,
        site,
      });
      equal(yield PlacesUtils.livemarks.getLivemark({
        guid: livemark.guid,
      }), livemark, "Livemark with same site URL should not be replaced");

      yield PlacesSyncUtils.bookmarks.update({
        syncId: livemark.guid,
        feed: feedURI,
        site,
      });
      equal(yield PlacesUtils.livemarks.getLivemark({
        guid: livemark.guid,
      }), livemark, "Livemark with same feed and site URLs should not be replaced");
    }

    do_print("Change livemark feed URL");
    {
      let livemark = yield PlacesUtils.livemarks.addLivemark({
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        feedURI,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      });

      // Since we're reinserting, we need to pass all properties required
      // for a new livemark. `update` won't merge the old and new ones.
      yield rejects(PlacesSyncUtils.bookmarks.update({
        syncId: livemark.guid,
        feed: site + "/feed/2",
      }), "Reinserting livemark with changed feed URL requires full record");

      let newLivemark = yield PlacesSyncUtils.bookmarks.update({
        kind: "livemark",
        parentSyncId: "menu",
        syncId: livemark.guid,
        feed: site + "/feed/2",
      });
      equal(newLivemark.syncId, livemark.guid,
        "GUIDs should match for reinserted livemark with changed feed URL");
      equal(newLivemark.feed.href, site + "/feed/2",
        "Reinserted livemark should have changed feed URI");
    }

    do_print("Add livemark site URL");
    {
      let livemark = yield PlacesUtils.livemarks.addLivemark({
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        feedURI,
      });
      ok(livemark.feedURI.equals(feedURI), "Livemark feed URI should match");
      ok(!livemark.siteURI, "Livemark should not have site URI");

      yield rejects(PlacesSyncUtils.bookmarks.update({
        syncId: livemark.guid,
        site,
      }), "Reinserting livemark with new site URL requires full record");

      let newLivemark = yield PlacesSyncUtils.bookmarks.update({
        kind: "livemark",
        parentSyncId: "menu",
        syncId: livemark.guid,
        feed: feedURI,
        site,
      });
      notEqual(newLivemark, livemark,
        "Livemark with new site URL should replace old livemark");
      equal(newLivemark.syncId, livemark.guid,
        "GUIDs should match for reinserted livemark with new site URL");
      equal(newLivemark.site.href, site + "/",
        "Reinserted livemark should have new site URI");
      equal(newLivemark.feed.href, feedURI.spec,
        "Reinserted livemark with new site URL should have same feed URI");
    }

    do_print("Remove livemark site URL");
    {
      let livemark = yield PlacesUtils.livemarks.addLivemark({
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        feedURI,
        siteURI: uri(site),
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      });

      yield rejects(PlacesSyncUtils.bookmarks.update({
        syncId: livemark.guid,
        site: null,
      }), "Reinserting livemark witout site URL requires full record");

      let newLivemark = yield PlacesSyncUtils.bookmarks.update({
        kind: "livemark",
        parentSyncId: "menu",
        syncId: livemark.guid,
        feed: feedURI,
        site: null,
      });
      notEqual(newLivemark, livemark,
        "Livemark without site URL should replace old livemark");
      equal(newLivemark.syncId, livemark.guid,
        "GUIDs should match for reinserted livemark without site URL");
      ok(!newLivemark.site, "Reinserted livemark should not have site URI");
    }

    do_print("Change livemark site URL");
    {
      let livemark = yield PlacesUtils.livemarks.addLivemark({
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        feedURI,
        siteURI: uri(site),
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      });

      yield rejects(PlacesSyncUtils.bookmarks.update({
        syncId: livemark.guid,
        site: site + "/new",
      }), "Reinserting livemark with changed site URL requires full record");

      let newLivemark = yield PlacesSyncUtils.bookmarks.update({
        kind: "livemark",
        parentSyncId: "menu",
        syncId: livemark.guid,
        feed:feedURI,
        site: site + "/new",
      });
      notEqual(newLivemark, livemark,
        "Livemark with changed site URL should replace old livemark");
      equal(newLivemark.syncId, livemark.guid,
        "GUIDs should match for reinserted livemark with changed site URL");
      equal(newLivemark.site.href, site + "/new",
        "Reinserted livemark should have changed site URI");
    }

    // Livemarks are stored as folders, but have different kinds. We should
    // remove the folder and insert a livemark with the same GUID instead of
    // trying to update the folder in-place.
    do_print("Replace folder with livemark");
    {
      let folder = yield PlacesUtils.bookmarks.insert({
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        title: "Plain folder",
      });
      let livemark = yield PlacesSyncUtils.bookmarks.update({
        kind: "livemark",
        parentSyncId: "menu",
        syncId: folder.guid,
        feed: feedURI,
      });
      equal(livemark.guid, folder.syncId,
        "Livemark should have same GUID as replaced folder");
    }
  } finally {
    yield stopServer();
  }

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_insert_tags() {
  yield Promise.all([{
    kind: "bookmark",
    url: "https://example.com",
    syncId: makeGuid(),
    parentSyncId: "menu",
    tags: ["foo", "bar"],
  }, {
    kind: "bookmark",
    url: "https://example.org",
    syncId: makeGuid(),
    parentSyncId: "toolbar",
    tags: ["foo", "baz"],
  }, {
    kind: "query",
    url: "place:queryType=1&sort=12&maxResults=10",
    syncId: makeGuid(),
    parentSyncId: "toolbar",
    folder: "bar",
    tags: ["baz", "qux"],
    title: "bar",
  }].map(info => PlacesSyncUtils.bookmarks.insert(info)));

  assertTagForURLs("foo", ["https://example.com/", "https://example.org/"],
    "2 URLs with new tag");
  assertTagForURLs("bar", ["https://example.com/"], "1 URL with existing tag");
  assertTagForURLs("baz", ["https://example.org/",
    "place:queryType=1&sort=12&maxResults=10"],
    "Should support tagging URLs and tag queries");
  assertTagForURLs("qux", ["place:queryType=1&sort=12&maxResults=10"],
    "Should support tagging tag queries");

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_insert_tags_whitespace() {
  do_print("Untrimmed and blank tags");
  let taggedBlanks = yield PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    url: "https://example.org",
    syncId: makeGuid(),
    parentSyncId: "menu",
    tags: [" untrimmed ", " ", "taggy"],
  });
  deepEqual(taggedBlanks.tags, ["untrimmed", "taggy"],
    "Should not return empty tags");
  assertURLHasTags("https://example.org/", ["taggy", "untrimmed"],
    "Should set trimmed tags and ignore dupes");

  do_print("Dupe tags");
  let taggedDupes = yield PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    url: "https://example.net",
    syncId: makeGuid(),
    parentSyncId: "toolbar",
    tags: [" taggy", "taggy ", " taggy ", "taggy"],
  });
  deepEqual(taggedDupes.tags, ["taggy", "taggy", "taggy", "taggy"],
    "Should return trimmed and dupe tags");
  assertURLHasTags("https://example.net/", ["taggy"],
    "Should ignore dupes when setting tags");

  assertTagForURLs("taggy", ["https://example.net/", "https://example.org/"],
    "Should exclude falsy tags");

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_insert_keyword() {
  do_print("Insert item with new keyword");
  {
    yield PlacesSyncUtils.bookmarks.insert({
      kind: "bookmark",
      parentSyncId: "menu",
      url: "https://example.com",
      keyword: "moz",
      syncId: makeGuid(),
    });
    let entry = yield PlacesUtils.keywords.fetch("moz");
    equal(entry.url.href, "https://example.com/",
      "Should add keyword for item");
  }

  do_print("Insert item with existing keyword");
  {
    yield PlacesSyncUtils.bookmarks.insert({
      kind: "bookmark",
      parentSyncId: "menu",
      url: "https://mozilla.org",
      keyword: "moz",
      syncId: makeGuid(),
    });
    let entry = yield PlacesUtils.keywords.fetch("moz");
    equal(entry.url.href, "https://mozilla.org/",
      "Should reassign keyword to new item");
  }

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_insert_annos() {
  do_print("Bookmark with description");
  let descBmk = yield PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    url: "https://example.com",
    syncId: makeGuid(),
    parentSyncId: "menu",
    description: "Bookmark description",
  });
  {
    equal(descBmk.description, "Bookmark description",
      "Should return new bookmark description");
    let id = yield syncIdToId(descBmk.syncId);
    equal(PlacesUtils.annotations.getItemAnnotation(id, DESCRIPTION_ANNO),
      "Bookmark description", "Should set new bookmark description");
  }

  do_print("Folder with description");
  let descFolder = yield PlacesSyncUtils.bookmarks.insert({
    kind: "folder",
    syncId: makeGuid(),
    parentSyncId: "menu",
    description: "Folder description",
  });
  {
    equal(descFolder.description, "Folder description",
      "Should return new folder description");
    let id = yield syncIdToId(descFolder.syncId);
    equal(PlacesUtils.annotations.getItemAnnotation(id, DESCRIPTION_ANNO),
      "Folder description", "Should set new folder description");
  }

  do_print("Bookmark with sidebar anno");
  let sidebarBmk = yield PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    url: "https://example.com",
    syncId: makeGuid(),
    parentSyncId: "menu",
    loadInSidebar: true,
  });
  {
    ok(sidebarBmk.loadInSidebar, "Should return sidebar anno for new bookmark");
    let id = yield syncIdToId(sidebarBmk.syncId);
    ok(PlacesUtils.annotations.itemHasAnnotation(id, LOAD_IN_SIDEBAR_ANNO),
      "Should set sidebar anno for new bookmark");
  }

  do_print("Bookmark without sidebar anno");
  let noSidebarBmk = yield PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    url: "https://example.org",
    syncId: makeGuid(),
    parentSyncId: "toolbar",
    loadInSidebar: false,
  });
  {
    ok(!noSidebarBmk.loadInSidebar,
      "Should not return sidebar anno for new bookmark");
    let id = yield syncIdToId(noSidebarBmk.syncId);
    ok(!PlacesUtils.annotations.itemHasAnnotation(id, LOAD_IN_SIDEBAR_ANNO),
      "Should not set sidebar anno for new bookmark");
  }

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_insert_tag_query() {
  let tagFolder = -1;

  do_print("Insert tag query for new tag");
  {
    deepEqual(PlacesUtils.tagging.allTags, [], "New tag should not exist yet");
    let query = yield PlacesSyncUtils.bookmarks.insert({
      kind: "query",
      syncId: makeGuid(),
      parentSyncId: "toolbar",
      url: "place:type=7&folder=90",
      folder: "taggy",
      title: "Tagged stuff",
    });
    notEqual(query.url.href, "place:type=7&folder=90",
      "Tag query URL for new tag should differ");

    [, tagFolder] = /\bfolder=(\d+)\b/.exec(query.url.pathname);
    ok(tagFolder > 0, "New tag query URL should contain valid folder");
    deepEqual(PlacesUtils.tagging.allTags, ["taggy"], "New tag should exist");
  }

  do_print("Insert tag query for existing tag");
  {
    let url = "place:type=7&folder=90&maxResults=15";
    let query = yield PlacesSyncUtils.bookmarks.insert({
      kind: "query",
      url,
      folder: "taggy",
      title: "Sorted and tagged",
      syncId: makeGuid(),
      parentSyncId: "menu",
    });
    notEqual(query.url.href, url, "Tag query URL for existing tag should differ");
    let params = new URLSearchParams(query.url.pathname);
    equal(params.get("type"), "7", "Should preserve query type");
    equal(params.get("maxResults"), "15", "Should preserve additional params");
    equal(params.get("folder"), tagFolder, "Should update tag folder");
    deepEqual(PlacesUtils.tagging.allTags, ["taggy"], "Should not duplicate existing tags");
  }

  do_print("Use the public tagging API to ensure we added the tag correctly");
  {
    yield PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "https://mozilla.org",
      title: "Mozilla",
    });
    PlacesUtils.tagging.tagURI(uri("https://mozilla.org"), ["taggy"]);
    assertURLHasTags("https://mozilla.org/", ["taggy"],
      "Should set tags using the tagging API");
  }

  do_print("Removing the tag should clean up the tag folder");
  {
    PlacesUtils.tagging.untagURI(uri("https://mozilla.org"), null);
    deepEqual(PlacesUtils.tagging.allTags, [],
      "Should remove tag folder once last item is untagged");
  }

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_insert_orphans() {
  let grandParentGuid = makeGuid();
  let parentGuid = makeGuid();
  let childGuid = makeGuid();
  let childId;

  do_print("Insert an orphaned child");
  {
    let child = yield PlacesSyncUtils.bookmarks.insert({
      kind: "bookmark",
      parentSyncId: parentGuid,
      syncId: childGuid,
      url: "https://mozilla.org",
    });
    equal(child.syncId, childGuid,
      "Should insert orphan with requested GUID");
    equal(child.parentSyncId, "unfiled",
      "Should reparent orphan to unfiled");

    childId = yield PlacesUtils.promiseItemId(childGuid);
    equal(PlacesUtils.annotations.getItemAnnotation(childId, SYNC_PARENT_ANNO),
      parentGuid, "Should set anno to missing parent GUID");
  }

  do_print("Insert the grandparent");
  {
    yield PlacesSyncUtils.bookmarks.insert({
      kind: "folder",
      parentSyncId: "menu",
      syncId: grandParentGuid,
    });
    equal(PlacesUtils.annotations.getItemAnnotation(childId, SYNC_PARENT_ANNO),
      parentGuid, "Child should still have orphan anno");
  }

  // Note that only `PlacesSyncUtils` reparents orphans, though Sync adds an
  // observer that removes the orphan anno if the orphan is manually moved.
  do_print("Insert the missing parent");
  {
    let parent = yield PlacesSyncUtils.bookmarks.insert({
      kind: "folder",
      parentSyncId: grandParentGuid,
      syncId: parentGuid,
    });
    equal(parent.syncId, parentGuid, "Should insert parent with requested GUID");
    equal(parent.parentSyncId, grandParentGuid,
      "Parent should be child of grandparent");
    ok(!PlacesUtils.annotations.itemHasAnnotation(childId, SYNC_PARENT_ANNO),
      "Orphan anno should be removed after reparenting");

    let child = yield PlacesUtils.bookmarks.fetch({ guid: childGuid });
    equal(child.parentGuid, parentGuid,
      "Should reparent child after inserting missing parent");
  }

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_fetch() {
  let folder = yield PlacesSyncUtils.bookmarks.insert({
    syncId: makeGuid(),
    parentSyncId: "menu",
    kind: "folder",
    description: "Folder description",
  });
  let bmk = yield PlacesSyncUtils.bookmarks.insert({
    syncId: makeGuid(),
    parentSyncId: "menu",
    kind: "bookmark",
    url: "https://example.com",
    description: "Bookmark description",
    loadInSidebar: true,
    tags: ["taggy"],
  });
  let folderBmk = yield PlacesSyncUtils.bookmarks.insert({
    syncId: makeGuid(),
    parentSyncId: folder.syncId,
    kind: "bookmark",
    url: "https://example.org",
    keyword: "kw",
  });
  let folderSep = yield PlacesSyncUtils.bookmarks.insert({
    syncId: makeGuid(),
    parentSyncId: folder.syncId,
    kind: "separator",
  });
  let tagQuery = yield PlacesSyncUtils.bookmarks.insert({
    kind: "query",
    syncId: makeGuid(),
    parentSyncId: "toolbar",
    url: "place:type=7&folder=90",
    folder: "taggy",
    title: "Tagged stuff",
  });
  let [, tagFolderId] = /\bfolder=(\d+)\b/.exec(tagQuery.url.pathname);
  let smartBmk = yield PlacesSyncUtils.bookmarks.insert({
    kind: "query",
    syncId: makeGuid(),
    parentSyncId: "toolbar",
    url: "place:folder=TOOLBAR",
    query: "BookmarksToolbar",
    title: "Bookmarks toolbar query",
  });

  do_print("Fetch empty folder with description");
  {
    let item = yield PlacesSyncUtils.bookmarks.fetch(folder.syncId);
    deepEqual(item, {
      syncId: folder.syncId,
      kind: "folder",
      parentSyncId: "menu",
      description: "Folder description",
      childSyncIds: [folderBmk.syncId, folderSep.syncId],
      parentTitle: "Bookmarks Menu",
    }, "Should include description, children, and parent title in folder");
  }

  do_print("Fetch bookmark with description, sidebar anno, and tags");
  {
    let item = yield PlacesSyncUtils.bookmarks.fetch(bmk.syncId);
    deepEqual(Object.keys(item).sort(), ["syncId", "kind", "parentSyncId", "url",
      "tags", "description", "loadInSidebar", "parentTitle"].sort(),
      "Should include bookmark-specific properties");
    equal(item.syncId, bmk.syncId, "Sync ID should match");
    equal(item.url.href, "https://example.com/", "Should return URL");
    equal(item.parentSyncId, "menu", "Should return parent sync ID");
    deepEqual(item.tags, ["taggy"], "Should return tags");
    equal(item.description, "Bookmark description", "Should return bookmark description");
    strictEqual(item.loadInSidebar, true, "Should return sidebar anno");
    equal(item.parentTitle, "Bookmarks Menu", "Should return parent title");
  }

  do_print("Fetch bookmark with keyword; without parent title or annos");
  {
    let item = yield PlacesSyncUtils.bookmarks.fetch(folderBmk.syncId);
    deepEqual(Object.keys(item).sort(), ["syncId", "kind", "parentSyncId",
      "url", "keyword", "tags", "loadInSidebar"].sort(),
      "Should omit blank bookmark-specific properties");
    strictEqual(item.loadInSidebar, false, "Should not load bookmark in sidebar");
    deepEqual(item.tags, [], "Tags should be empty");
    equal(item.keyword, "kw", "Should return keyword");
  }

  do_print("Fetch separator");
  {
    let item = yield PlacesSyncUtils.bookmarks.fetch(folderSep.syncId);
    strictEqual(item.index, 1, "Should return separator position");
  }

  do_print("Fetch tag query");
  {
    let item = yield PlacesSyncUtils.bookmarks.fetch(tagQuery.syncId);
    deepEqual(Object.keys(item).sort(), ["syncId", "kind", "parentSyncId",
      "url", "title", "folder", "parentTitle"].sort(),
      "Should include query-specific properties");
    equal(item.url.href, `place:type=7&folder=${tagFolderId}`, "Should not rewrite outgoing tag queries");
    equal(item.folder, "taggy", "Should return tag name for tag queries");
  }

  do_print("Fetch smart bookmark");
  {
    let item = yield PlacesSyncUtils.bookmarks.fetch(smartBmk.syncId);
    deepEqual(Object.keys(item).sort(), ["syncId", "kind", "parentSyncId",
      "url", "title", "query", "parentTitle"].sort(),
      "Should include smart bookmark-specific properties");
    equal(item.query, "BookmarksToolbar", "Should return query name for smart bookmarks");
  }

  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_fetch_livemark() {
  let { site, stopServer } = makeLivemarkServer();

  try {
    do_print("Create livemark");
    let livemark = yield PlacesUtils.livemarks.addLivemark({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      feedURI: uri(site + "/feed/1"),
      siteURI: uri(site),
      index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    });
    PlacesUtils.annotations.setItemAnnotation(livemark.id, DESCRIPTION_ANNO,
      "Livemark description", 0, PlacesUtils.annotations.EXPIRE_NEVER);

    do_print("Fetch livemark");
    let item = yield PlacesSyncUtils.bookmarks.fetch(livemark.guid);
    deepEqual(Object.keys(item).sort(), ["syncId", "kind", "parentSyncId",
      "description", "feed", "site", "parentTitle"].sort(),
      "Should include livemark-specific properties");
    equal(item.description, "Livemark description", "Should return description");
    equal(item.feed.href, site + "/feed/1", "Should return feed URL");
    equal(item.site.href, site + "/", "Should return site URL");
  } finally {
    yield stopServer();
  }

  yield PlacesUtils.bookmarks.eraseEverything();
});
