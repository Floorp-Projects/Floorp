Cu.import("resource://gre/modules/ObjectUtils.jsm");
Cu.import("resource://gre/modules/PlacesSyncUtils.jsm");
Cu.import("resource://testing-common/httpd.js");
Cu.importGlobalProperties(["URLSearchParams"]);

const DESCRIPTION_ANNO = "bookmarkProperties/description";
const LOAD_IN_SIDEBAR_ANNO = "bookmarkProperties/loadInSidebar";
const SYNC_PARENT_ANNO = "sync/parent";

var makeGuid = PlacesUtils.history.makeGuid;

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

function assertTagForURLs(tag, urls, message) {
  let taggedURLs = PlacesUtils.tagging.getURIsForTag(tag).map(uri => uri.spec);
  deepEqual(taggedURLs.sort(compareAscending), urls.sort(compareAscending), message);
}

function assertURLHasTags(url, tags, message) {
  let actualTags = PlacesUtils.tagging.getTagsForURI(uri(url));
  deepEqual(actualTags.sort(compareAscending), tags, message);
}

var populateTree = async function populate(parentGuid, ...items) {
  let guids = {};

  for (let index = 0; index < items.length; index++) {
    let item = items[index];
    let guid = makeGuid();

    switch (item.kind) {
      case "bookmark":
      case "query":
        await PlacesUtils.bookmarks.insert({
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          url: item.url,
          title: item.title,
          parentGuid, guid, index,
        });
        break;

      case "separator":
        await PlacesUtils.bookmarks.insert({
          type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
          parentGuid, guid,
        });
        break;

      case "folder":
        await PlacesUtils.bookmarks.insert({
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          title: item.title,
          parentGuid, guid,
        });
        if (item.children) {
          Object.assign(guids, await populate(guid, ...item.children));
        }
        break;

      default:
        throw new Error(`Unsupported item type: ${item.type}`);
    }

    if (item.exclude) {
      let itemId = await PlacesUtils.promiseItemId(guid);
      PlacesUtils.annotations.setItemAnnotation(
        itemId, PlacesUtils.EXCLUDE_FROM_BACKUP_ANNO, "Don't back this up", 0,
        PlacesUtils.annotations.EXPIRE_NEVER);
    }

    guids[item.title] = guid;
  }

  return guids;
};

var recordIdToId = async function recordIdToId(recordId) {
  let guid = await PlacesSyncUtils.bookmarks.recordIdToGuid(recordId);
  return PlacesUtils.promiseItemId(guid);
};

var moveSyncedBookmarksToUnsyncedParent = async function() {
  do_print("Insert synced bookmarks");
  let syncedGuids = await populateTree(PlacesUtils.bookmarks.menuGuid, {
    kind: "folder",
    title: "folder",
    children: [{
      kind: "bookmark",
      title: "childBmk",
      url: "https://example.org",
    }],
  }, {
    kind: "bookmark",
    title: "topBmk",
    url: "https://example.com",
  });
  // Pretend we've synced each bookmark at least once.
  await PlacesTestUtils.setBookmarkSyncFields(...Object.values(syncedGuids).map(
    guid => ({ guid, syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL })
  ));

  do_print("Make new folder");
  let unsyncedFolder = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "unsyncedFolder",
  });

  do_print("Move synced bookmarks into unsynced new folder");
  for (let guid of Object.values(syncedGuids)) {
    await PlacesUtils.bookmarks.update({
      guid,
      parentGuid: unsyncedFolder.guid,
      index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    });
  }

  return { syncedGuids, unsyncedFolder };
};

var setChangesSynced = async function(changes) {
  for (let recordId in changes) {
    changes[recordId].synced = true;
  }
  await PlacesSyncUtils.bookmarks.pushChanges(changes);
};

var ignoreChangedRoots = async function() {
  let changes = await PlacesSyncUtils.bookmarks.pullChanges();
  let expectedRoots = ["menu", "mobile", "toolbar", "unfiled"];
  if (!ObjectUtils.deepEqual(Object.keys(changes).sort(), expectedRoots)) {
    // Make sure the previous test cleaned up.
    throw new Error(`Unexpected changes at start of test: ${
      JSON.stringify(changes)}`);
  }
  await setChangesSynced(changes);
};

add_task(async function test_fetchURLFrecency() {
  // Add visits to the following URLs and then check if frecency for those URLs is not -1.
  let arrayOfURLsToVisit = ["https://www.mozilla.org/en-US/", "http://getfirefox.com", "http://getthunderbird.com"];
  for (let url of arrayOfURLsToVisit) {
    await PlacesTestUtils.addVisits(url);
  }
  for (let url of arrayOfURLsToVisit) {
    let frecency = await PlacesSyncUtils.history.fetchURLFrecency(url);
    equal(typeof frecency, "number", "The frecency should be of type: number");
    notEqual(frecency, -1, "The frecency of this url should be different than -1");
  }
  // Do not add visits to the following URLs, and then check if frecency for those URLs is -1.
  let arrayOfURLsNotVisited = ["https://bugzilla.org", "https://example.org"];
  for (let url of arrayOfURLsNotVisited) {
    let frecency = await PlacesSyncUtils.history.fetchURLFrecency(url);
    equal(frecency, -1, "The frecency of this url should be -1");
  }

  // Remove the visits added during this test.
 await PlacesTestUtils.clearHistory();
});

add_task(async function test_determineNonSyncableGuids() {
  // Add visits to the following URLs with different transition types.
  let arrayOfVisits = [{ uri: "https://www.mozilla.org/en-US/", transition: TRANSITION_TYPED },
                       { uri: "http://getfirefox.com/", transition: TRANSITION_LINK },
                       { uri: "http://getthunderbird.com/", transition: TRANSITION_FRAMED_LINK }];
  for (let visit of arrayOfVisits) {
    await PlacesTestUtils.addVisits(visit);
  }

  // Fetch the guid for each visit.
  let guids = [];
  let dictURLGuid = {};
  for (let visit of arrayOfVisits) {
    let guid = await PlacesSyncUtils.history.fetchGuidForURL(visit.uri);
    guids.push(guid);
    dictURLGuid[visit.uri] = guid;
  }

  // Filter the visits.
  let filteredGuids = await PlacesSyncUtils.history.determineNonSyncableGuids(guids);

  // Check if the filtered visits are of type TRANSITION_FRAMED_LINK.
  for (let visit of arrayOfVisits) {
    if (visit.transition === TRANSITION_FRAMED_LINK) {
      ok(filteredGuids.includes(dictURLGuid[visit.uri]), "This url should be one of the filtered guids.");
    } else {
      ok(!filteredGuids.includes(dictURLGuid[visit.uri]), "This url should not be one of the filtered guids.");
    }
  }

  // Remove the visits added during this test.
  await PlacesTestUtils.clearHistory();
});

add_task(async function test_changeGuid() {
  // Add some visits of the following URLs.
  let arrayOfURLsToVisit = ["https://www.mozilla.org/en-US/", "http://getfirefox.com/", "http://getthunderbird.com/"];
  for (let url of arrayOfURLsToVisit) {
    await PlacesTestUtils.addVisits(url);
  }

  for (let url of arrayOfURLsToVisit) {
    let originalGuid = await PlacesSyncUtils.history.fetchGuidForURL(url);
    let newGuid = makeGuid();

    // Change the original GUID for the new GUID.
    await PlacesSyncUtils.history.changeGuid(url, newGuid);

    // Fetch the GUID for this URL.
    let newGuidFetched = await PlacesSyncUtils.history.fetchGuidForURL(url);

    // Check that the URL has the new GUID as its GUID and not the original one.
    equal(newGuid, newGuidFetched, "These should be equal since we changed the guid for the visit.");
    notEqual(originalGuid, newGuidFetched, "These should be different since we changed the guid for the visit.");
  }

  // Remove the visits added during this test.
  await PlacesTestUtils.clearHistory();
});

add_task(async function test_fetchVisitsForURL() {
  // Get the date for this moment and a date for a minute ago.
  let now = new Date();
  let aMinuteAgo = new Date(now.getTime() - (1 * 60000));

  // Add some visits of the following URLs, specifying the transition and the visit date.
  let arrayOfVisits = [{ uri: "https://www.mozilla.org/en-US/", transition: TRANSITION_TYPED, visitDate: aMinuteAgo },
                       { uri: "http://getfirefox.com/", transition: TRANSITION_LINK, visitDate: aMinuteAgo },
                       { uri: "http://getthunderbird.com/", transition: TRANSITION_LINK, visitDate: aMinuteAgo }];
  for (let elem of arrayOfVisits) {
    await PlacesTestUtils.addVisits(elem);
  }

  for (let elem of arrayOfVisits) {
    // Fetch all the visits for this URL.
    let visits = await PlacesSyncUtils.history.fetchVisitsForURL(elem.uri);
    // Since the visit we added will be the last one in the collection of visits, we get the index of it.
    let iLast = visits.length - 1;

    // The date is saved in _micro_seconds, here we change it to milliseconds.
    let dateInMilliseconds = visits[iLast].date * 0.001;

    // Check that the info we provided for this URL is the same one retrieved.
    equal(dateInMilliseconds, elem.visitDate.getTime(), "The date we provided should be the same we retrieved.");
    equal(visits[iLast].type, elem.transition, "The transition type we provided should be the same we retrieved.");
  }

  // Remove the visits added during this test.
  await PlacesTestUtils.clearHistory();
});

add_task(async function test_fetchGuidForURL() {
  // Add some visits of the following URLs.
  let arrayOfURLsToVisit = ["https://www.mozilla.org/en-US/", "http://getfirefox.com/", "http://getthunderbird.com/"];
  for (let url of arrayOfURLsToVisit) {
    await PlacesTestUtils.addVisits(url);
  }

  // This tries to test fetchGuidForURL in two ways:
  // 1- By fetching the GUID, and then using that GUID to retrieve the info of the visit.
  //    It then compares the URL with the URL that is on the visits info.
  // 2- By creating a new GUID, changing the GUID for the visit, fetching the GUID and comparing them.
  for (let url of arrayOfURLsToVisit) {
    let guid = await PlacesSyncUtils.history.fetchGuidForURL(url);
    let info = await PlacesSyncUtils.history.fetchURLInfoForGuid(guid);

    let newGuid = makeGuid();
    await PlacesSyncUtils.history.changeGuid(url, newGuid);
    let newGuid2 = await PlacesSyncUtils.history.fetchGuidForURL(url);

    equal(url, info.url, "The url provided and the url retrieved should be the same.");
    equal(newGuid, newGuid2, "The changed guid and the retrieved guid should be the same.");
  }

  // Remove the visits added during this test.
  await PlacesTestUtils.clearHistory();
});

add_task(async function test_fetchURLInfoForGuid() {
  // Add some visits of the following URLs. specifying the title.
  let visits = [{ uri: "https://www.mozilla.org/en-US/", title: "mozilla" },
                { uri: "http://getfirefox.com/", title: "firefox" },
                { uri: "http://getthunderbird.com/", title: "thunderbird" },
                { uri: "http://quantum.mozilla.com/", title: null}];
  for (let visit of visits) {
    await PlacesTestUtils.addVisits(visit);
  }

  for (let visit of visits) {
    let guid = await PlacesSyncUtils.history.fetchGuidForURL(visit.uri);
    let info = await PlacesSyncUtils.history.fetchURLInfoForGuid(guid);

    // Compare the info returned by fetchURLInfoForGuid,
    // URL and title should match while frecency must be different than -1.
    equal(info.url, visit.uri, "The url provided should be the same as the url retrieved.");
    equal(info.title, visit.title || "", "The title provided should be the same as the title retrieved.");
    notEqual(info.frecency, -1, "The frecency of the visit should be different than -1.");
  }

  // Create a "fake" GUID and check that the result of fetchURLInfoForGuid is null.
  let guid = makeGuid();
  let info = await PlacesSyncUtils.history.fetchURLInfoForGuid(guid);

  equal(info, null, "The information object of a non-existent guid should be null.");

  // Remove the visits added during this test.
  await PlacesTestUtils.clearHistory();
});

add_task(async function test_getAllURLs() {
  // Add some visits of the following URLs.
  let arrayOfURLsToVisit = ["https://www.mozilla.org/en-US/", "http://getfirefox.com/", "http://getthunderbird.com/"];
  for (let url of arrayOfURLsToVisit) {
    await PlacesTestUtils.addVisits(url);
  }

  // Get all URLs.
  let allURLs = await PlacesSyncUtils.history.getAllURLs({ since: new Date(Date.now() - 2592000000), limit: 5000 });

  // The amount of URLs must be the same in both collections.
  equal(allURLs.length, arrayOfURLsToVisit.length, "The amount of urls retrived should match the amount of urls provided.");

  // Check that the correct URLs were retrived.
  for (let url of arrayOfURLsToVisit) {
    ok(allURLs.includes(url), "The urls retrieved should match the ones used in this test.");
  }

  // Remove the visits added during this test.
  await PlacesTestUtils.clearHistory();
});

add_task(async function test_order() {
  do_print("Insert some bookmarks");
  let guids = await populateTree(PlacesUtils.bookmarks.menuGuid, {
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
    await PlacesSyncUtils.bookmarks.order(PlacesUtils.bookmarks.menuGuid, order);
    let childRecordIds = await PlacesSyncUtils.bookmarks.fetchChildRecordIds(PlacesUtils.bookmarks.menuGuid);
    deepEqual(childRecordIds, order, "New bookmarks should be reordered according to array");
  }

  do_print("Same order with unspecified children");
  {
    await PlacesSyncUtils.bookmarks.order(PlacesUtils.bookmarks.menuGuid, [
      guids.siblingSep, guids.siblingBmk,
    ]);
    let childRecordIds = await PlacesSyncUtils.bookmarks.fetchChildRecordIds(
      PlacesUtils.bookmarks.menuGuid);
    deepEqual(childRecordIds, [guids.siblingFolder, guids.siblingSep,
      guids.childBmk, guids.siblingBmk],
      "Current order should be respected if possible");
  }

  do_print("New order with unspecified children");
  {
    await PlacesSyncUtils.bookmarks.order(PlacesUtils.bookmarks.menuGuid, [
      guids.siblingBmk, guids.siblingSep,
    ]);
    let childRecordIds = await PlacesSyncUtils.bookmarks.fetchChildRecordIds(
      PlacesUtils.bookmarks.menuGuid);
    deepEqual(childRecordIds, [guids.siblingBmk, guids.siblingSep,
      guids.siblingFolder, guids.childBmk],
      "Unordered children should be moved to end if current order can't be respected");
  }

  do_print("Reorder with nonexistent children");
  {
    await PlacesSyncUtils.bookmarks.order(PlacesUtils.bookmarks.menuGuid, [
      guids.childBmk, makeGuid(), guids.siblingBmk, guids.siblingSep,
      makeGuid(), guids.siblingFolder, makeGuid()]);
    let childRecordIds = await PlacesSyncUtils.bookmarks.fetchChildRecordIds(
      PlacesUtils.bookmarks.menuGuid);
    deepEqual(childRecordIds, [guids.childBmk, guids.siblingBmk, guids.siblingSep,
      guids.siblingFolder], "Nonexistent children should be ignored");
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_dedupe() {
  await ignoreChangedRoots();

  let parentFolder = await PlacesSyncUtils.bookmarks.insert({
    kind: "folder",
    recordId: makeGuid(),
    parentRecordId: "menu",
  });
  let differentParentFolder = await PlacesSyncUtils.bookmarks.insert({
    kind: "folder",
    recordId: makeGuid(),
    parentRecordId: "menu",
  });
  let mozBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: parentFolder.recordId,
    url: "https://mozilla.org",
  });
  let fxBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: parentFolder.recordId,
    url: "http://getfirefox.com",
  });
  let tbBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: parentFolder.recordId,
    url: "http://getthunderbird.com",
  });

  await Assert.rejects(
    PlacesSyncUtils.bookmarks.dedupe(makeGuid(), makeGuid(), makeGuid()),
    "Should reject attempts to de-dupe nonexistent items"
  );
  await Assert.rejects(PlacesSyncUtils.bookmarks.dedupe("menu", makeGuid(), "places"),
    "Should reject attempts to de-dupe local roots");

  do_print("De-dupe with same remote parent");
  {
    let localId = await PlacesUtils.promiseItemId(mozBmk.recordId);
    let newRemoteRecordId = makeGuid();

    let changes = await PlacesSyncUtils.bookmarks.dedupe(
      mozBmk.recordId, newRemoteRecordId, parentFolder.recordId);
    deepEqual(Object.keys(changes).sort(), [
      parentFolder.recordId, // Parent.
      mozBmk.recordId, // Tombstone for old sync ID.
    ].sort(), "Should bump change counter of parent");
    ok(changes[mozBmk.recordId].tombstone,
      "Should write tombstone for old local sync ID");
    ok(Object.values(changes).every(change => change.counter === 1),
      "Change counter for every bookmark should be 1");

    ok(!(await PlacesUtils.bookmarks.fetch(mozBmk.recordId)),
      "Bookmark with old local sync ID should not exist");
    await Assert.rejects(PlacesUtils.promiseItemId(mozBmk.recordId),
      "Should invalidate GUID cache entry for old local sync ID");

    let newMozBmk = await PlacesUtils.bookmarks.fetch(newRemoteRecordId);
    equal(newMozBmk.guid, newRemoteRecordId,
      "Should change local sync ID to remote sync ID");
    equal(await PlacesUtils.promiseItemId(newRemoteRecordId), localId,
      "Should add new remote sync ID to GUID cache");

    await setChangesSynced(changes);
  }

  do_print("De-dupe with different remote parent");
  {
    let localId = await PlacesUtils.promiseItemId(fxBmk.recordId);
    let newRemoteRecordId = makeGuid();

    let changes = await PlacesSyncUtils.bookmarks.dedupe(
      fxBmk.recordId, newRemoteRecordId, differentParentFolder.recordId);
    deepEqual(Object.keys(changes).sort(), [
      parentFolder.recordId, // Old local parent.
      differentParentFolder.recordId, // New remote parent.
      fxBmk.recordId, // Tombstone for old sync ID.
    ].sort(), "Should bump change counter of old parent and new parent");
    ok(changes[fxBmk.recordId].tombstone,
      "Should write tombstone for old local sync ID");
    ok(Object.values(changes).every(change => change.counter === 1),
      "Change counter for every bookmark should be 1");

    let newFxBmk = await PlacesUtils.bookmarks.fetch(newRemoteRecordId);
    equal(newFxBmk.parentGuid, parentFolder.recordId,
      "De-duping should not move bookmark to new parent");
    equal(await PlacesUtils.promiseItemId(newRemoteRecordId), localId,
      "De-duping with different remote parent should cache new sync ID");

    await setChangesSynced(changes);
  }

  do_print("De-dupe with nonexistent remote parent");
  {
    let localId = await PlacesUtils.promiseItemId(tbBmk.recordId);
    let newRemoteRecordId = makeGuid();
    let remoteParentRecordId = makeGuid();

    let changes = await PlacesSyncUtils.bookmarks.dedupe(
      tbBmk.recordId, newRemoteRecordId, remoteParentRecordId);
    deepEqual(Object.keys(changes).sort(), [
      parentFolder.recordId, // Old local parent.
      tbBmk.recordId, // Tombstone for old sync ID.
    ].sort(), "Should bump change counter of old parent");
    ok(changes[tbBmk.recordId].tombstone,
      "Should write tombstone for old local sync ID");
    ok(Object.values(changes).every(change => change.counter === 1),
      "Change counter for every bookmark should be 1");

    equal(await PlacesUtils.promiseItemId(newRemoteRecordId), localId,
      "De-duping with nonexistent remote parent should cache new sync ID");

    await setChangesSynced(changes);
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_order_roots() {
  let oldOrder = await PlacesSyncUtils.bookmarks.fetchChildRecordIds(
    PlacesUtils.bookmarks.rootGuid);
  await PlacesSyncUtils.bookmarks.order(PlacesUtils.bookmarks.rootGuid,
    shuffle(oldOrder));
  let newOrder = await PlacesSyncUtils.bookmarks.fetchChildRecordIds(
    PlacesUtils.bookmarks.rootGuid);
  deepEqual(oldOrder, newOrder, "Should ignore attempts to reorder roots");

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_update_tags() {
  do_print("Insert item without tags");
  let item = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    url: "https://mozilla.org",
    recordId: makeGuid(),
    parentRecordId: "menu",
  });

  do_print("Add tags");
  {
    let updatedItem = await PlacesSyncUtils.bookmarks.update({
      recordId: item.recordId,
      tags: ["foo", "bar"],
    });
    deepEqual(updatedItem.tags, ["foo", "bar"], "Should return new tags");
    assertURLHasTags("https://mozilla.org", ["bar", "foo"],
      "Should set new tags for URL");
  }

  do_print("Add new tag, remove existing tag");
  {
    let updatedItem = await PlacesSyncUtils.bookmarks.update({
      recordId: item.recordId,
      tags: ["foo", "baz"],
    });
    deepEqual(updatedItem.tags, ["foo", "baz"], "Should return updated tags");
    assertURLHasTags("https://mozilla.org", ["baz", "foo"],
      "Should update tags for URL");
    assertTagForURLs("bar", [], "Should remove existing tag");
  }

  do_print("Tags with whitespace");
  {
    let updatedItem = await PlacesSyncUtils.bookmarks.update({
      recordId: item.recordId,
      tags: [" leading", "trailing ", " baz ", " "],
    });
    deepEqual(updatedItem.tags, ["leading", "trailing", "baz"],
      "Should return filtered tags");
    assertURLHasTags("https://mozilla.org", ["baz", "leading", "trailing"],
      "Should trim whitespace and filter blank tags");
  }

  do_print("Remove all tags");
  {
    let updatedItem = await PlacesSyncUtils.bookmarks.update({
      recordId: item.recordId,
      tags: null,
    });
    deepEqual(updatedItem.tags, [], "Should return empty tag array");
    assertURLHasTags("https://mozilla.org", [],
      "Should remove all existing tags");
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_pullChanges_tags() {
  await ignoreChangedRoots();

  do_print("Insert untagged items with same URL");
  let firstItem = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: "menu",
    url: "https://example.org",
  });
  let secondItem = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: "menu",
    url: "https://example.org",
  });
  let untaggedItem = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: "menu",
    url: "https://bugzilla.org",
  });
  let taggedItem = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: "menu",
    url: "https://mozilla.org",
  });

  do_print("Create tag");
  PlacesUtils.tagging.tagURI(uri("https://example.org"), ["taggy"]);
  let tagFolderId = PlacesUtils.bookmarks.getIdForItemAt(
    PlacesUtils.tagsFolderId, 0);
  let tagFolderGuid = await PlacesUtils.promiseItemGuid(tagFolderId);

  do_print("Tagged bookmarks should be in changeset");
  {
    let changes = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(Object.keys(changes).sort(),
      [firstItem.recordId, secondItem.recordId].sort(),
      "Should include tagged bookmarks in changeset");
    await setChangesSynced(changes);
  }

  do_print("Change tag case");
  {
    PlacesUtils.tagging.tagURI(uri("https://mozilla.org"), ["TaGgY"]);
    let changes = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(Object.keys(changes).sort(),
      [firstItem.recordId, secondItem.recordId, taggedItem.recordId].sort(),
      "Should include tagged bookmarks after changing case");
    assertTagForURLs("TaGgY", ["https://example.org/", "https://mozilla.org/"],
      "Should add tag for new URL");
    await setChangesSynced(changes);
  }

  // These tests change a tag item directly, without going through the tagging
  // service. This behavior isn't supported, but the tagging service registers
  // an observer to handle these cases, so we make sure we handle them
  // correctly.

  do_print("Rename tag folder using Bookmarks.setItemTitle");
  {
    PlacesUtils.bookmarks.setItemTitle(tagFolderId, "sneaky");
    deepEqual(PlacesUtils.tagging.allTags, ["sneaky"],
      "Tagging service should update cache with new title");
    let changes = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(Object.keys(changes).sort(),
      [firstItem.recordId, secondItem.recordId].sort(),
      "Should include tagged bookmarks after renaming tag folder");
    await setChangesSynced(changes);
  }

  do_print("Rename tag folder using Bookmarks.update");
  {
    await PlacesUtils.bookmarks.update({
      guid: tagFolderGuid,
      title: "tricky",
    });
    deepEqual(PlacesUtils.tagging.allTags, ["tricky"],
      "Tagging service should update cache after updating tag folder");
    let changes = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(Object.keys(changes).sort(),
      [firstItem.recordId, secondItem.recordId].sort(),
      "Should include tagged bookmarks after updating tag folder");
    await setChangesSynced(changes);
  }

  do_print("Change tag entry URI using Bookmarks.changeBookmarkURI");
  {
    let tagId = PlacesUtils.bookmarks.getIdForItemAt(tagFolderId, 0);
    PlacesUtils.bookmarks.changeBookmarkURI(tagId, uri("https://bugzilla.org"));
    let changes = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(Object.keys(changes).sort(),
      [firstItem.recordId, secondItem.recordId, untaggedItem.recordId].sort(),
      "Should include tagged bookmarks after changing tag entry URI");
    assertTagForURLs("tricky", ["https://bugzilla.org/", "https://mozilla.org/"],
      "Should remove tag entry for old URI");
    await setChangesSynced(changes);
  }

  do_print("Change tag entry URL using Bookmarks.update");
  {
    let tagGuid = await PlacesUtils.promiseItemGuid(
      PlacesUtils.bookmarks.getIdForItemAt(tagFolderId, 0));
    await PlacesUtils.bookmarks.update({
      guid: tagGuid,
      url: "https://example.com",
    });
    let changes = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(Object.keys(changes).sort(),
      [untaggedItem.recordId].sort(),
      "Should include tagged bookmarks after changing tag entry URL");
    assertTagForURLs("tricky", ["https://example.com/", "https://mozilla.org/"],
      "Should remove tag entry for old URL");
    await setChangesSynced(changes);
  }

  do_print("Remove all tag folders");
  {
    deepEqual(PlacesUtils.tagging.allTags, ["tricky"], "Should have existing tags");

    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.tagsFolderId);
    let changes = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(Object.keys(changes).sort(), [taggedItem.recordId].sort(),
      "Should include tagged bookmarks after removing all tags");

    deepEqual(PlacesUtils.tagging.allTags, [], "Should remove all tags from tag service");
    await setChangesSynced(changes);
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_update_keyword() {
  do_print("Insert item without keyword");
  let item = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    parentRecordId: "menu",
    url: "https://mozilla.org",
    recordId: makeGuid(),
  });

  do_print("Add item keyword");
  {
    let updatedItem = await PlacesSyncUtils.bookmarks.update({
      recordId: item.recordId,
      keyword: "moz",
    });
    equal(updatedItem.keyword, "moz", "Should return new keyword");
    let entryByKeyword = await PlacesUtils.keywords.fetch("moz");
    equal(entryByKeyword.url.href, "https://mozilla.org/",
      "Should set new keyword for URL");
    let entryByURL = await PlacesUtils.keywords.fetch({
      url: "https://mozilla.org",
    });
    equal(entryByURL.keyword, "moz", "Looking up URL should return new keyword");
  }

  do_print("Change item keyword");
  {
    let updatedItem = await PlacesSyncUtils.bookmarks.update({
      recordId: item.recordId,
      keyword: "m",
    });
    equal(updatedItem.keyword, "m", "Should return updated keyword");
    let newEntry = await PlacesUtils.keywords.fetch("m");
    equal(newEntry.url.href, "https://mozilla.org/", "Should update keyword for URL");
    let oldEntry = await PlacesUtils.keywords.fetch("moz");
    ok(!oldEntry, "Should remove old keyword");
  }

  do_print("Remove existing keyword");
  {
    let updatedItem = await PlacesSyncUtils.bookmarks.update({
      recordId: item.recordId,
      keyword: null,
    });
    ok(!updatedItem.keyword,
      "Should not include removed keyword in properties");
    let entry = await PlacesUtils.keywords.fetch({
      url: "https://mozilla.org",
    });
    ok(!entry, "Should remove new keyword from URL");
  }

  do_print("Remove keyword for item without keyword");
  {
    await PlacesSyncUtils.bookmarks.update({
      recordId: item.recordId,
      keyword: null,
    });
    let entry = await PlacesUtils.keywords.fetch({
      url: "https://mozilla.org",
    });
    ok(!entry,
      "Removing keyword for URL without existing keyword should succeed");
  }

  let item2;
  do_print("Insert removes other item's keyword if they are the same");
  {
    let updatedItem = await PlacesSyncUtils.bookmarks.update({
      recordId: item.recordId,
      keyword: "test",
    });
    equal(updatedItem.keyword, "test", "Initial update succeeds");
    item2 = await PlacesSyncUtils.bookmarks.insert({
      kind: "bookmark",
      parentRecordId: "menu",
      url: "https://mozilla.org/1",
      recordId: makeGuid(),
      keyword: "test",
    });
    equal(item2.keyword, "test", "insert with existing should succeed");
    updatedItem = await PlacesSyncUtils.bookmarks.fetch(item.recordId);
    ok(!updatedItem.keyword, "initial item no longer has keyword");
    let entry = await PlacesUtils.keywords.fetch({
      url: "https://mozilla.org",
    });
    ok(!entry, "Direct check for original url keyword gives nothing");
    let newEntry = await PlacesUtils.keywords.fetch("test");
    ok(newEntry, "Keyword should exist for new item");
    equal(newEntry.url.href, "https://mozilla.org/1", "Keyword should point to new url");
  }

  do_print("Insert updates other item's keyword if they are the same url");
  {
    let updatedItem = await PlacesSyncUtils.bookmarks.update({
      recordId: item.recordId,
      keyword: "test2",
    });
    equal(updatedItem.keyword, "test2", "Initial update succeeds");
    let newItem = await PlacesSyncUtils.bookmarks.insert({
      kind: "bookmark",
      parentRecordId: "menu",
      url: "https://mozilla.org",
      recordId: makeGuid(),
      keyword: "test3",
    });
    equal(newItem.keyword, "test3", "insert with existing should succeed");
    updatedItem = await PlacesSyncUtils.bookmarks.fetch(item.recordId);
    equal(updatedItem.keyword, "test3", "initial item has new keyword");
  }

  do_print("Update removes other item's keyword if they are the same");
  {
    let updatedItem = await PlacesSyncUtils.bookmarks.update({
      recordId: item.recordId,
      keyword: "test4",
    });
    equal(updatedItem.keyword, "test4", "Initial update succeeds");
    let updatedItem2 = await PlacesSyncUtils.bookmarks.update({
      recordId: item2.recordId,
      keyword: "test4",
    });
    equal(updatedItem2.keyword, "test4", "New update succeeds");
    updatedItem = await PlacesSyncUtils.bookmarks.fetch(item.recordId);
    ok(!updatedItem.keyword, "initial item no longer has keyword");
    let entry = await PlacesUtils.keywords.fetch({
      url: "https://mozilla.org",
    });
    ok(!entry, "Direct check for original url keyword gives nothing");
    let newEntry = await PlacesUtils.keywords.fetch("test4");
    ok(newEntry, "Keyword should exist for new item");
    equal(newEntry.url.href, "https://mozilla.org/1", "Keyword should point to new url");
  }

  do_print("Update url updates it's keyword if url already has keyword");
  {
    let updatedItem = await PlacesSyncUtils.bookmarks.update({
      recordId: item.recordId,
      keyword: "test4",
    });
    equal(updatedItem.keyword, "test4", "Initial update succeeds");
    let updatedItem2 = await PlacesSyncUtils.bookmarks.update({
      recordId: item2.recordId,
      keyword: "test5",
    });
    equal(updatedItem2.keyword, "test5", "New update succeeds");
    await PlacesSyncUtils.bookmarks.update({
      recordId: item2.recordId,
      url: item.url.href,
    });
    updatedItem2 = await PlacesSyncUtils.bookmarks.fetch(item2.recordId);
    equal(updatedItem2.keyword, "test4", "Item keyword has been updated");
    updatedItem = await PlacesSyncUtils.bookmarks.fetch(item.recordId);
    equal(updatedItem.keyword, "test4", "Initial item still has keyword");
  }


  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_conflicting_keywords() {
  await ignoreChangedRoots();

  do_print("Insert bookmark with new keyword");
  let tbBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: "unfiled",
    url: "http://getthunderbird.com",
    keyword: "tbird",
  });
  {
    let entryByKeyword = await PlacesUtils.keywords.fetch("tbird");
    equal(entryByKeyword.url.href, "http://getthunderbird.com/",
      "Should return new keyword entry by URL");
    let entryByURL = await PlacesUtils.keywords.fetch({
      url: "http://getthunderbird.com",
    });
    equal(entryByURL.keyword, "tbird", "Should return new entry by keyword");
    let changes = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(changes, {},
      "Should not bump change counter for new keyword entry");
  }

  do_print("Insert bookmark with same URL and different keyword");
  let dupeTbBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: "toolbar",
    url: "http://getthunderbird.com",
    keyword: "tb",
  });
  {
    let oldKeywordByURL = await PlacesUtils.keywords.fetch("tbird");
    ok(!oldKeywordByURL,
      "Should remove old entry when inserting bookmark with different keyword");
    let entryByKeyword = await PlacesUtils.keywords.fetch("tb");
    equal(entryByKeyword.url.href, "http://getthunderbird.com/",
      "Should return different keyword entry by URL");
    let entryByURL = await PlacesUtils.keywords.fetch({
      url: "http://getthunderbird.com",
    });
    equal(entryByURL.keyword, "tb", "Should return different entry by keyword");
    let changes = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(Object.keys(changes).sort(), [
      tbBmk.recordId,
      dupeTbBmk.recordId,
    ].sort(), "Should bump change counter for bookmarks with different keyword");
    await setChangesSynced(changes);
  }

  do_print("Update bookmark with different keyword");
  await PlacesSyncUtils.bookmarks.update({
    kind: "bookmark",
    recordId: tbBmk.recordId,
    url: "http://getthunderbird.com",
    keyword: "thunderbird",
  });
  {
    let oldKeywordByURL = await PlacesUtils.keywords.fetch("tb");
    ok(!oldKeywordByURL,
      "Should remove old entry when updating bookmark keyword");
    let entryByKeyword = await PlacesUtils.keywords.fetch("thunderbird");
    equal(entryByKeyword.url.href, "http://getthunderbird.com/",
      "Should return updated keyword entry by URL");
    let entryByURL = await PlacesUtils.keywords.fetch({
      url: "http://getthunderbird.com",
    });
    equal(entryByURL.keyword, "thunderbird",
      "Should return entry by updated keyword");
    let changes = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(Object.keys(changes).sort(), [
      tbBmk.recordId,
      dupeTbBmk.recordId,
    ].sort(), "Should bump change counter for bookmarks with updated keyword");
    await setChangesSynced(changes);
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_update_annos() {
  let guids = await populateTree(PlacesUtils.bookmarks.menuGuid, {
    kind: "folder",
    title: "folder",
  }, {
    kind: "bookmark",
    title: "bmk",
    url: "https://example.com",
  });

  do_print("Add folder description");
  {
    let updatedItem = await PlacesSyncUtils.bookmarks.update({
      recordId: guids.folder,
      description: "Folder description",
    });
    equal(updatedItem.description, "Folder description",
      "Should return new description");
    let id = await recordIdToId(updatedItem.recordId);
    equal(PlacesUtils.annotations.getItemAnnotation(id, DESCRIPTION_ANNO),
      "Folder description", "Should set description anno");
  }

  do_print("Clear folder description");
  {
    let updatedItem = await PlacesSyncUtils.bookmarks.update({
      recordId: guids.folder,
      description: null,
    });
    ok(!updatedItem.description, "Should not return cleared description");
    let id = await recordIdToId(updatedItem.recordId);
    ok(!PlacesUtils.annotations.itemHasAnnotation(id, DESCRIPTION_ANNO),
      "Should remove description anno");
  }

  do_print("Add bookmark sidebar anno");
  {
    let updatedItem = await PlacesSyncUtils.bookmarks.update({
      recordId: guids.bmk,
      loadInSidebar: true,
    });
    ok(updatedItem.loadInSidebar, "Should return sidebar anno");
    let id = await recordIdToId(updatedItem.recordId);
    ok(PlacesUtils.annotations.itemHasAnnotation(id, LOAD_IN_SIDEBAR_ANNO),
      "Should set sidebar anno for existing bookmark");
  }

  do_print("Clear bookmark sidebar anno");
  {
    let updatedItem = await PlacesSyncUtils.bookmarks.update({
      recordId: guids.bmk,
      loadInSidebar: false,
    });
    ok(!updatedItem.loadInSidebar, "Should not return cleared sidebar anno");
    let id = await recordIdToId(updatedItem.recordId);
    ok(!PlacesUtils.annotations.itemHasAnnotation(id, LOAD_IN_SIDEBAR_ANNO),
      "Should clear sidebar anno for existing bookmark");
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_update_move_root() {
  do_print("Move root to same parent");
  {
    // This should be a no-op.
    let sameRoot = await PlacesSyncUtils.bookmarks.update({
      recordId: "menu",
      parentRecordId: "places",
    });
    equal(sameRoot.recordId, "menu",
      "Menu root GUID should not change");
    equal(sameRoot.parentRecordId, "places",
      "Parent Places root GUID should not change");
  }

  do_print("Try reparenting root");
  await Assert.rejects(PlacesSyncUtils.bookmarks.update({
    recordId: "menu",
    parentRecordId: "toolbar",
  }));

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_insert() {
  do_print("Insert bookmark");
  {
    let item = await PlacesSyncUtils.bookmarks.insert({
      kind: "bookmark",
      recordId: makeGuid(),
      parentRecordId: "menu",
      url: "https://example.org",
    });
    let { type } = await PlacesUtils.bookmarks.fetch({ guid: item.recordId });
    equal(type, PlacesUtils.bookmarks.TYPE_BOOKMARK,
      "Bookmark should have correct type");
  }

  do_print("Insert query");
  {
    let item = await PlacesSyncUtils.bookmarks.insert({
      kind: "query",
      recordId: makeGuid(),
      parentRecordId: "menu",
      url: "place:terms=term&folder=TOOLBAR&queryType=1",
      folder: "Saved search",
    });
    let { type } = await PlacesUtils.bookmarks.fetch({ guid: item.recordId });
    equal(type, PlacesUtils.bookmarks.TYPE_BOOKMARK,
      "Queries should be stored as bookmarks");
  }

  do_print("Insert folder");
  {
    let item = await PlacesSyncUtils.bookmarks.insert({
      kind: "folder",
      recordId: makeGuid(),
      parentRecordId: "menu",
      title: "New folder",
    });
    let { type } = await PlacesUtils.bookmarks.fetch({ guid: item.recordId });
    equal(type, PlacesUtils.bookmarks.TYPE_FOLDER,
      "Folder should have correct type");
  }

  do_print("Insert separator");
  {
    let item = await PlacesSyncUtils.bookmarks.insert({
      kind: "separator",
      recordId: makeGuid(),
      parentRecordId: "menu",
    });
    let { type } = await PlacesUtils.bookmarks.fetch({ guid: item.recordId });
    equal(type, PlacesUtils.bookmarks.TYPE_SEPARATOR,
      "Separator should have correct type");
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_insert_livemark() {
  let { site, stopServer } = makeLivemarkServer();

  try {
    do_print("Insert livemark with feed URL");
    {
      let livemark = await PlacesSyncUtils.bookmarks.insert({
        kind: "livemark",
        recordId: makeGuid(),
        feed: site + "/feed/1",
        parentRecordId: "menu",
      });
      let bmk = await PlacesUtils.bookmarks.fetch({
        guid: await PlacesSyncUtils.bookmarks.recordIdToGuid(livemark.recordId),
      });
      equal(bmk.type, PlacesUtils.bookmarks.TYPE_FOLDER,
        "Livemarks should be stored as folders");
    }

    let livemarkRecordId;
    do_print("Insert livemark with site and feed URLs");
    {
      let livemark = await PlacesSyncUtils.bookmarks.insert({
        kind: "livemark",
        recordId: makeGuid(),
        site,
        feed: site + "/feed/1",
        parentRecordId: "menu",
      });
      livemarkRecordId = livemark.recordId;
    }

    do_print("Try inserting livemark into livemark");
    {
      let livemark = await PlacesSyncUtils.bookmarks.insert({
        kind: "livemark",
        recordId: makeGuid(),
        site,
        feed: site + "/feed/1",
        parentRecordId: livemarkRecordId,
      });
      ok(!livemark, "Should not insert livemark as child of livemark");
    }
  } finally {
    await stopServer();
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_update_livemark() {
  let { site, stopServer } = makeLivemarkServer();
  let feedURI = uri(site + "/feed/1");

  try {
    // We shouldn't reinsert the livemark if the URLs are the same.
    do_print("Update livemark with same URLs");
    {
      let livemark = await PlacesUtils.livemarks.addLivemark({
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        feedURI,
        siteURI: uri(site),
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      });

      await PlacesSyncUtils.bookmarks.update({
        recordId: livemark.guid,
        feed: feedURI,
      });
      // `nsLivemarkService` returns references to `Livemark` instances, so we
      // can compare them with `==` to make sure they haven't been replaced.
      equal(await PlacesUtils.livemarks.getLivemark({
        guid: livemark.guid,
      }), livemark, "Livemark with same feed URL should not be replaced");

      await PlacesSyncUtils.bookmarks.update({
        recordId: livemark.guid,
        site,
      });
      equal(await PlacesUtils.livemarks.getLivemark({
        guid: livemark.guid,
      }), livemark, "Livemark with same site URL should not be replaced");

      await PlacesSyncUtils.bookmarks.update({
        recordId: livemark.guid,
        feed: feedURI,
        site,
      });
      equal(await PlacesUtils.livemarks.getLivemark({
        guid: livemark.guid,
      }), livemark, "Livemark with same feed and site URLs should not be replaced");
    }

    do_print("Change livemark feed URL");
    {
      let livemark = await PlacesUtils.livemarks.addLivemark({
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        feedURI,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      });

      // Since we're reinserting, we need to pass all properties required
      // for a new livemark. `update` won't merge the old and new ones.
      await Assert.rejects(PlacesSyncUtils.bookmarks.update({
        recordId: livemark.guid,
        feed: site + "/feed/2",
      }), "Reinserting livemark with changed feed URL requires full record");

      let newLivemark = await PlacesSyncUtils.bookmarks.update({
        kind: "livemark",
        parentRecordId: "menu",
        recordId: livemark.guid,
        feed: site + "/feed/2",
      });
      equal(newLivemark.recordId, livemark.guid,
        "IDs should match for reinserted livemark with changed feed URL");
      equal(newLivemark.feed.href, site + "/feed/2",
        "Reinserted livemark should have changed feed URI");
    }

    do_print("Add livemark site URL");
    {
      let livemark = await PlacesUtils.livemarks.addLivemark({
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        feedURI,
      });
      ok(livemark.feedURI.equals(feedURI), "Livemark feed URI should match");
      ok(!livemark.siteURI, "Livemark should not have site URI");

      await Assert.rejects(PlacesSyncUtils.bookmarks.update({
        recordId: livemark.guid,
        site,
      }), "Reinserting livemark with new site URL requires full record");

      let newLivemark = await PlacesSyncUtils.bookmarks.update({
        kind: "livemark",
        parentRecordId: "menu",
        recordId: livemark.guid,
        feed: feedURI,
        site,
      });
      notEqual(newLivemark, livemark,
        "Livemark with new site URL should replace old livemark");
      equal(newLivemark.recordId, livemark.guid,
        "IDs should match for reinserted livemark with new site URL");
      equal(newLivemark.site.href, site + "/",
        "Reinserted livemark should have new site URI");
      equal(newLivemark.feed.href, feedURI.spec,
        "Reinserted livemark with new site URL should have same feed URI");
    }

    do_print("Remove livemark site URL");
    {
      let livemark = await PlacesUtils.livemarks.addLivemark({
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        feedURI,
        siteURI: uri(site),
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      });

      await Assert.rejects(PlacesSyncUtils.bookmarks.update({
        recordId: livemark.guid,
        site: null,
      }), "Reinserting livemark witout site URL requires full record");

      let newLivemark = await PlacesSyncUtils.bookmarks.update({
        kind: "livemark",
        parentRecordId: "menu",
        recordId: livemark.guid,
        feed: feedURI,
        site: null,
      });
      notEqual(newLivemark, livemark,
        "Livemark without site URL should replace old livemark");
      equal(newLivemark.recordId, livemark.guid,
        "IDs should match for reinserted livemark without site URL");
      ok(!newLivemark.site, "Reinserted livemark should not have site URI");
    }

    do_print("Change livemark site URL");
    {
      let livemark = await PlacesUtils.livemarks.addLivemark({
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        feedURI,
        siteURI: uri(site),
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      });

      await Assert.rejects(PlacesSyncUtils.bookmarks.update({
        recordId: livemark.guid,
        site: site + "/new",
      }), "Reinserting livemark with changed site URL requires full record");

      let newLivemark = await PlacesSyncUtils.bookmarks.update({
        kind: "livemark",
        parentRecordId: "menu",
        recordId: livemark.guid,
        feed: feedURI,
        site: site + "/new",
      });
      notEqual(newLivemark, livemark,
        "Livemark with changed site URL should replace old livemark");
      equal(newLivemark.recordId, livemark.guid,
        "IDs should match for reinserted livemark with changed site URL");
      equal(newLivemark.site.href, site + "/new",
        "Reinserted livemark should have changed site URI");
    }

    // Livemarks are stored as folders, but have different kinds. We should
    // remove the folder and insert a livemark with the same GUID instead of
    // trying to update the folder in-place.
    do_print("Replace folder with livemark");
    {
      let folder = await PlacesUtils.bookmarks.insert({
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        title: "Plain folder",
      });
      let livemark = await PlacesSyncUtils.bookmarks.update({
        kind: "livemark",
        parentRecordId: "menu",
        recordId: folder.guid,
        feed: feedURI,
      });
      equal(livemark.guid, folder.recordId,
        "Livemark should have same GUID as replaced folder");
    }
  } finally {
    await stopServer();
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_insert_tags() {
  await Promise.all([{
    kind: "bookmark",
    url: "https://example.com",
    recordId: makeGuid(),
    parentRecordId: "menu",
    tags: ["foo", "bar"],
  }, {
    kind: "bookmark",
    url: "https://example.org",
    recordId: makeGuid(),
    parentRecordId: "toolbar",
    tags: ["foo", "baz"],
  }, {
    kind: "query",
    url: "place:queryType=1&sort=12&maxResults=10",
    recordId: makeGuid(),
    parentRecordId: "toolbar",
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

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_insert_tags_whitespace() {
  do_print("Untrimmed and blank tags");
  let taggedBlanks = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    url: "https://example.org",
    recordId: makeGuid(),
    parentRecordId: "menu",
    tags: [" untrimmed ", " ", "taggy"],
  });
  deepEqual(taggedBlanks.tags, ["untrimmed", "taggy"],
    "Should not return empty tags");
  assertURLHasTags("https://example.org/", ["taggy", "untrimmed"],
    "Should set trimmed tags and ignore dupes");

  do_print("Dupe tags");
  let taggedDupes = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    url: "https://example.net",
    recordId: makeGuid(),
    parentRecordId: "toolbar",
    tags: [" taggy", "taggy ", " taggy ", "taggy"],
  });
  deepEqual(taggedDupes.tags, ["taggy", "taggy", "taggy", "taggy"],
    "Should return trimmed and dupe tags");
  assertURLHasTags("https://example.net/", ["taggy"],
    "Should ignore dupes when setting tags");

  assertTagForURLs("taggy", ["https://example.net/", "https://example.org/"],
    "Should exclude falsy tags");

  PlacesUtils.tagging.untagURI(uri("https://example.org"), ["untrimmed", "taggy"]);
  PlacesUtils.tagging.untagURI(uri("https://example.net"), ["taggy"]);
  deepEqual(PlacesUtils.tagging.allTags, [], "Should clean up all tags");

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_insert_keyword() {
  do_print("Insert item with new keyword");
  {
    await PlacesSyncUtils.bookmarks.insert({
      kind: "bookmark",
      parentRecordId: "menu",
      url: "https://example.com",
      keyword: "moz",
      recordId: makeGuid(),
    });
    let entry = await PlacesUtils.keywords.fetch("moz");
    equal(entry.url.href, "https://example.com/",
      "Should add keyword for item");
  }

  do_print("Insert item with existing keyword");
  {
    await PlacesSyncUtils.bookmarks.insert({
      kind: "bookmark",
      parentRecordId: "menu",
      url: "https://mozilla.org",
      keyword: "moz",
      recordId: makeGuid(),
    });
    let entry = await PlacesUtils.keywords.fetch("moz");
    equal(entry.url.href, "https://mozilla.org/",
      "Should reassign keyword to new item");
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_insert_annos() {
  do_print("Bookmark with description");
  let descBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    url: "https://example.com",
    recordId: makeGuid(),
    parentRecordId: "menu",
    description: "Bookmark description",
  });
  {
    equal(descBmk.description, "Bookmark description",
      "Should return new bookmark description");
    let id = await recordIdToId(descBmk.recordId);
    equal(PlacesUtils.annotations.getItemAnnotation(id, DESCRIPTION_ANNO),
      "Bookmark description", "Should set new bookmark description");
  }

  do_print("Folder with description");
  let descFolder = await PlacesSyncUtils.bookmarks.insert({
    kind: "folder",
    recordId: makeGuid(),
    parentRecordId: "menu",
    description: "Folder description",
  });
  {
    equal(descFolder.description, "Folder description",
      "Should return new folder description");
    let id = await recordIdToId(descFolder.recordId);
    equal(PlacesUtils.annotations.getItemAnnotation(id, DESCRIPTION_ANNO),
      "Folder description", "Should set new folder description");
  }

  do_print("Bookmark with sidebar anno");
  let sidebarBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    url: "https://example.com",
    recordId: makeGuid(),
    parentRecordId: "menu",
    loadInSidebar: true,
  });
  {
    ok(sidebarBmk.loadInSidebar, "Should return sidebar anno for new bookmark");
    let id = await recordIdToId(sidebarBmk.recordId);
    ok(PlacesUtils.annotations.itemHasAnnotation(id, LOAD_IN_SIDEBAR_ANNO),
      "Should set sidebar anno for new bookmark");
  }

  do_print("Bookmark without sidebar anno");
  let noSidebarBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    url: "https://example.org",
    recordId: makeGuid(),
    parentRecordId: "toolbar",
    loadInSidebar: false,
  });
  {
    ok(!noSidebarBmk.loadInSidebar,
      "Should not return sidebar anno for new bookmark");
    let id = await recordIdToId(noSidebarBmk.recordId);
    ok(!PlacesUtils.annotations.itemHasAnnotation(id, LOAD_IN_SIDEBAR_ANNO),
      "Should not set sidebar anno for new bookmark");
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_insert_tag_query() {
  let tagFolder = -1;

  do_print("Insert tag query for new tag");
  {
    deepEqual(PlacesUtils.tagging.allTags, [], "New tag should not exist yet");
    let query = await PlacesSyncUtils.bookmarks.insert({
      kind: "query",
      recordId: makeGuid(),
      parentRecordId: "toolbar",
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
    let query = await PlacesSyncUtils.bookmarks.insert({
      kind: "query",
      url,
      folder: "taggy",
      title: "Sorted and tagged",
      recordId: makeGuid(),
      parentRecordId: "menu",
    });
    notEqual(query.url.href, url, "Tag query URL for existing tag should differ");
    let params = new URLSearchParams(query.url.pathname);
    equal(params.get("type"), "7", "Should preserve query type");
    equal(params.get("maxResults"), "15", "Should preserve additional params");
    equal(params.get("folder"), tagFolder, "Should update tag folder");
    deepEqual(PlacesUtils.tagging.allTags, ["taggy"], "Should not duplicate existing tags");
  }

  do_print("Use the public tagging API to ensure we added the tag correctly");
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: "https://mozilla.org",
    title: "Mozilla",
  });
  PlacesUtils.tagging.tagURI(uri("https://mozilla.org"), ["taggy"]);
  assertURLHasTags("https://mozilla.org/", ["taggy"],
    "Should set tags using the tagging API");

  do_print("Removing the tag should clean up the tag folder");
  PlacesUtils.tagging.untagURI(uri("https://mozilla.org"), null);
  deepEqual(PlacesUtils.tagging.allTags, [],
    "Should remove tag folder once last item is untagged");

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_insert_orphans() {
  await ignoreChangedRoots();

  let grandParentGuid = makeGuid();
  let parentGuid = makeGuid();
  let childGuid = makeGuid();
  let childId;

  do_print("Insert an orphaned child");
  {
    let child = await PlacesSyncUtils.bookmarks.insert({
      kind: "bookmark",
      parentRecordId: parentGuid,
      recordId: childGuid,
      url: "https://mozilla.org",
    });
    equal(child.recordId, childGuid,
      "Should insert orphan with requested GUID");
    equal(child.parentRecordId, "unfiled",
      "Should reparent orphan to unfiled");

    childId = await PlacesUtils.promiseItemId(childGuid);
    equal(PlacesUtils.annotations.getItemAnnotation(childId, SYNC_PARENT_ANNO),
      parentGuid, "Should set anno to missing parent GUID");
  }

  do_print("Insert the grandparent");
  await PlacesSyncUtils.bookmarks.insert({
    kind: "folder",
    parentRecordId: "menu",
    recordId: grandParentGuid,
  });
  equal(PlacesUtils.annotations.getItemAnnotation(childId, SYNC_PARENT_ANNO),
    parentGuid, "Child should still have orphan anno");

  do_print("Insert the missing parent");
  {
    let parent = await PlacesSyncUtils.bookmarks.insert({
      kind: "folder",
      parentRecordId: grandParentGuid,
      recordId: parentGuid,
    });
    equal(parent.recordId, parentGuid, "Should insert parent with requested GUID");
    equal(parent.parentRecordId, grandParentGuid,
      "Parent should be child of grandparent");
    ok(!PlacesUtils.annotations.itemHasAnnotation(childId, SYNC_PARENT_ANNO),
      "Orphan anno should be removed after reparenting");

    let child = await PlacesUtils.bookmarks.fetch({ guid: childGuid });
    equal(child.parentGuid, parentGuid,
      "Should reparent child after inserting missing parent");
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_move_orphans() {
  let nonexistentRecordId = makeGuid();
  let fxBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: nonexistentRecordId,
    url: "http://getfirefox.com",
  });
  let tbBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: nonexistentRecordId,
    url: "http://getthunderbird.com",
  });

  do_print("Verify synced orphan annos match");
  {
    let orphanGuids = await PlacesSyncUtils.bookmarks.fetchGuidsWithAnno(
      SYNC_PARENT_ANNO, nonexistentRecordId);
    deepEqual(orphanGuids.sort(), [fxBmk.recordId, tbBmk.recordId].sort(),
      "Orphaned bookmarks should match before moving");
  }

  do_print("Move synced orphan using async API");
  {
    await PlacesUtils.bookmarks.update({
      guid: fxBmk.recordId,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    });
    let orphanGuids = await PlacesSyncUtils.bookmarks.fetchGuidsWithAnno(
      SYNC_PARENT_ANNO, nonexistentRecordId);
    deepEqual(orphanGuids, [tbBmk.recordId],
      "Should remove orphan annos from updated bookmark");
  }

  do_print("Move synced orphan using sync API");
  {
    let tbId = await recordIdToId(tbBmk.recordId);
    PlacesUtils.bookmarks.moveItem(tbId, PlacesUtils.toolbarFolderId,
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let orphanGuids = await PlacesSyncUtils.bookmarks.fetchGuidsWithAnno(
      SYNC_PARENT_ANNO, nonexistentRecordId);
    deepEqual(orphanGuids, [],
      "Should remove orphan annos from moved bookmark");
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_reorder_orphans() {
  let nonexistentRecordId = makeGuid();
  let fxBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: nonexistentRecordId,
    url: "http://getfirefox.com",
  });
  let tbBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: nonexistentRecordId,
    url: "http://getthunderbird.com",
  });
  let mozBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: nonexistentRecordId,
    url: "https://mozilla.org",
  });

  do_print("Verify synced orphan annos match");
  {
    let orphanGuids = await PlacesSyncUtils.bookmarks.fetchGuidsWithAnno(
      SYNC_PARENT_ANNO, nonexistentRecordId);
    deepEqual(orphanGuids.sort(), [
      fxBmk.recordId,
      tbBmk.recordId,
      mozBmk.recordId,
    ].sort(), "Orphaned bookmarks should match before reordering");
  }

  do_print("Reorder synced orphans");
  {
    await PlacesUtils.bookmarks.reorder(PlacesUtils.bookmarks.unfiledGuid,
      [tbBmk.recordId, fxBmk.recordId]);
    let orphanGuids = await PlacesSyncUtils.bookmarks.fetchGuidsWithAnno(
      SYNC_PARENT_ANNO, nonexistentRecordId);
    deepEqual(orphanGuids, [mozBmk.recordId],
      "Should remove orphan annos from explicitly reordered bookmarks");
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_set_orphan_indices() {
  let nonexistentRecordId = makeGuid();
  let fxBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: nonexistentRecordId,
    url: "http://getfirefox.com",
  });
  let tbBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: nonexistentRecordId,
    url: "http://getthunderbird.com",
  });

  do_print("Verify synced orphan annos match");
  {
    let orphanGuids = await PlacesSyncUtils.bookmarks.fetchGuidsWithAnno(
      SYNC_PARENT_ANNO, nonexistentRecordId);
    deepEqual(orphanGuids.sort(), [fxBmk.recordId, tbBmk.recordId].sort(),
      "Orphaned bookmarks should match before changing indices");
  }

  do_print("Set synced orphan indices");
  {
    let fxId = await recordIdToId(fxBmk.recordId);
    let tbId = await recordIdToId(tbBmk.recordId);
    PlacesUtils.bookmarks.runInBatchMode(_ => {
      PlacesUtils.bookmarks.setItemIndex(fxId, 1);
      PlacesUtils.bookmarks.setItemIndex(tbId, 0);
    }, null);
    await PlacesTestUtils.promiseAsyncUpdates();
    let orphanGuids = await PlacesSyncUtils.bookmarks.fetchGuidsWithAnno(
      SYNC_PARENT_ANNO, nonexistentRecordId);
    deepEqual(orphanGuids, [],
      "Should remove orphan annos after updating indices");
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_unsynced_orphans() {
  let nonexistentRecordId = makeGuid();
  let newBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: nonexistentRecordId,
    url: "http://getfirefox.com",
  });
  let unknownBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: makeGuid(),
    parentRecordId: nonexistentRecordId,
    url: "http://getthunderbird.com",
  });
  await PlacesTestUtils.setBookmarkSyncFields({
    guid: newBmk.recordId,
    syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NEW,
  }, {
    guid: unknownBmk.recordId,
    syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.UNKNOWN,
  });

  do_print("Move unsynced orphan");
  {
    let unknownId = await recordIdToId(unknownBmk.recordId);
    PlacesUtils.bookmarks.moveItem(unknownId, PlacesUtils.toolbarFolderId,
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let orphanGuids = await PlacesSyncUtils.bookmarks.fetchGuidsWithAnno(
      SYNC_PARENT_ANNO, nonexistentRecordId);
    deepEqual(orphanGuids.sort(), [newBmk.recordId].sort(),
      "Should remove orphan annos from moved unsynced bookmark");
  }

  do_print("Reorder unsynced orphans");
  {
    await PlacesUtils.bookmarks.reorder(PlacesUtils.bookmarks.unfiledGuid,
      [newBmk.recordId]);
    let orphanGuids = await PlacesSyncUtils.bookmarks.fetchGuidsWithAnno(
      SYNC_PARENT_ANNO, nonexistentRecordId);
    deepEqual(orphanGuids, [],
      "Should remove orphan annos from reordered unsynced bookmarks");
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_fetch() {
  let folder = await PlacesSyncUtils.bookmarks.insert({
    recordId: makeGuid(),
    parentRecordId: "menu",
    kind: "folder",
    description: "Folder description",
  });
  let bmk = await PlacesSyncUtils.bookmarks.insert({
    recordId: makeGuid(),
    parentRecordId: "menu",
    kind: "bookmark",
    url: "https://example.com",
    description: "Bookmark description",
    loadInSidebar: true,
    tags: ["taggy"],
  });
  let folderBmk = await PlacesSyncUtils.bookmarks.insert({
    recordId: makeGuid(),
    parentRecordId: folder.recordId,
    kind: "bookmark",
    url: "https://example.org",
    keyword: "kw",
  });
  let folderSep = await PlacesSyncUtils.bookmarks.insert({
    recordId: makeGuid(),
    parentRecordId: folder.recordId,
    kind: "separator",
  });
  let tagQuery = await PlacesSyncUtils.bookmarks.insert({
    kind: "query",
    recordId: makeGuid(),
    parentRecordId: "toolbar",
    url: "place:type=7&folder=90",
    folder: "taggy",
    title: "Tagged stuff",
  });
  let [, tagFolderId] = /\bfolder=(\d+)\b/.exec(tagQuery.url.pathname);
  let smartBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "query",
    recordId: makeGuid(),
    parentRecordId: "toolbar",
    url: "place:folder=TOOLBAR",
    query: "BookmarksToolbar",
    title: "Bookmarks toolbar query",
  });

  do_print("Fetch empty folder with description");
  {
    let item = await PlacesSyncUtils.bookmarks.fetch(folder.recordId);
    deepEqual(item, {
      recordId: folder.recordId,
      kind: "folder",
      parentRecordId: "menu",
      description: "Folder description",
      childRecordIds: [folderBmk.recordId, folderSep.recordId],
      parentTitle: "Bookmarks Menu",
      dateAdded: item.dateAdded,
      title: "",
    }, "Should include description, children, title, and parent title in folder");
  }

  do_print("Fetch bookmark with description, sidebar anno, and tags");
  {
    let item = await PlacesSyncUtils.bookmarks.fetch(bmk.recordId);
    deepEqual(Object.keys(item).sort(), ["recordId", "kind", "parentRecordId",
      "url", "tags", "description", "loadInSidebar", "parentTitle", "title", "dateAdded"].sort(),
      "Should include bookmark-specific properties");
    equal(item.recordId, bmk.recordId, "Sync ID should match");
    equal(item.url.href, "https://example.com/", "Should return URL");
    equal(item.parentRecordId, "menu", "Should return parent sync ID");
    deepEqual(item.tags, ["taggy"], "Should return tags");
    equal(item.description, "Bookmark description", "Should return bookmark description");
    strictEqual(item.loadInSidebar, true, "Should return sidebar anno");
    equal(item.parentTitle, "Bookmarks Menu", "Should return parent title");
    strictEqual(item.title, "", "Should return empty title");
  }

  do_print("Fetch bookmark with keyword; without parent title or annos");
  {
    let item = await PlacesSyncUtils.bookmarks.fetch(folderBmk.recordId);
    deepEqual(Object.keys(item).sort(), ["recordId", "kind", "parentRecordId",
      "url", "keyword", "tags", "loadInSidebar", "parentTitle", "title", "dateAdded"].sort(),
      "Should omit blank bookmark-specific properties");
    strictEqual(item.loadInSidebar, false, "Should not load bookmark in sidebar");
    deepEqual(item.tags, [], "Tags should be empty");
    equal(item.keyword, "kw", "Should return keyword");
    strictEqual(item.parentTitle, "", "Should include parent title even if empty");
    strictEqual(item.title, "", "Should include bookmark title even if empty");
  }

  do_print("Fetch separator");
  {
    let item = await PlacesSyncUtils.bookmarks.fetch(folderSep.recordId);
    strictEqual(item.index, 1, "Should return separator position");
  }

  do_print("Fetch tag query");
  {
    let item = await PlacesSyncUtils.bookmarks.fetch(tagQuery.recordId);
    deepEqual(Object.keys(item).sort(), ["recordId", "kind", "parentRecordId",
      "url", "title", "folder", "parentTitle", "dateAdded"].sort(),
      "Should include query-specific properties");
    equal(item.url.href, `place:type=7&folder=${tagFolderId}`, "Should not rewrite outgoing tag queries");
    equal(item.folder, "taggy", "Should return tag name for tag queries");
  }

  do_print("Fetch smart bookmark");
  {
    let item = await PlacesSyncUtils.bookmarks.fetch(smartBmk.recordId);
    deepEqual(Object.keys(item).sort(), ["recordId", "kind", "parentRecordId",
      "url", "title", "query", "parentTitle", "dateAdded"].sort(),
      "Should include smart bookmark-specific properties");
    equal(item.query, "BookmarksToolbar", "Should return query name for smart bookmarks");
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_fetch_livemark() {
  let { site, stopServer } = makeLivemarkServer();

  try {
    do_print("Create livemark");
    let livemark = await PlacesUtils.livemarks.addLivemark({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      feedURI: uri(site + "/feed/1"),
      siteURI: uri(site),
      index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    });
    PlacesUtils.annotations.setItemAnnotation(livemark.id, DESCRIPTION_ANNO,
      "Livemark description", 0, PlacesUtils.annotations.EXPIRE_NEVER);

    do_print("Fetch livemark");
    let item = await PlacesSyncUtils.bookmarks.fetch(livemark.guid);
    deepEqual(Object.keys(item).sort(), ["recordId", "kind", "parentRecordId",
      "description", "feed", "site", "parentTitle", "title", "dateAdded"].sort(),
      "Should include livemark-specific properties");
    equal(item.description, "Livemark description", "Should return description");
    equal(item.feed.href, site + "/feed/1", "Should return feed URL");
    equal(item.site.href, site + "/", "Should return site URL");
    strictEqual(item.title, "", "Should include livemark title even if empty");
  } finally {
    await stopServer();
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_pullChanges_new_parent() {
  await ignoreChangedRoots();

  let { syncedGuids, unsyncedFolder } = await moveSyncedBookmarksToUnsyncedParent();

  do_print("Unsynced parent and synced items should be tracked");
  let changes = await PlacesSyncUtils.bookmarks.pullChanges();
  deepEqual(Object.keys(changes).sort(),
    [syncedGuids.folder, syncedGuids.topBmk, syncedGuids.childBmk,
      unsyncedFolder.guid, "menu"].sort(),
    "Should return change records for moved items and new parent"
  );

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_pullChanges_deleted_folder() {
  await ignoreChangedRoots();

  let { syncedGuids, unsyncedFolder } = await moveSyncedBookmarksToUnsyncedParent();

  do_print("Remove unsynced new folder");
  await PlacesUtils.bookmarks.remove(unsyncedFolder.guid);

  do_print("Deleted synced items should be tracked; unsynced folder should not");
  let changes = await PlacesSyncUtils.bookmarks.pullChanges();
  deepEqual(Object.keys(changes).sort(),
    [syncedGuids.folder, syncedGuids.topBmk, syncedGuids.childBmk,
      "menu"].sort(),
    "Should return change records for all deleted items"
  );
  for (let guid of Object.values(syncedGuids)) {
    strictEqual(changes[guid].tombstone, true,
      `Tombstone flag should be set for deleted item ${guid}`);
    equal(changes[guid].counter, 1,
      `Change counter should be 1 for deleted item ${guid}`);
    equal(changes[guid].status, PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
      `Sync status should be normal for deleted item ${guid}`);
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_pullChanges_import_html() {
  await ignoreChangedRoots();

  do_print("Add unsynced bookmark");
  let unsyncedBmk = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "https://example.com",
  });

  {
    let fields = await PlacesTestUtils.fetchBookmarkSyncFields(
      unsyncedBmk.guid);
    ok(fields.every(field =>
      field.syncStatus == PlacesUtils.bookmarks.SYNC_STATUS.NEW
    ), "Unsynced bookmark statuses should match");
  }

  do_print("Import new bookmarks from HTML");
  let { path } = do_get_file("./sync_utils_bookmarks.html");
  await BookmarkHTMLUtils.importFromFile(path, false);

  // Bookmarks.html doesn't store IDs, so we need to look these up.
  let mozBmk = await PlacesUtils.bookmarks.fetch({
    url: "https://www.mozilla.org/",
  });
  let fxBmk = await PlacesUtils.bookmarks.fetch({
    url: "https://www.mozilla.org/en-US/firefox/",
  });
  // All Bookmarks.html bookmarks are stored under the menu. For toolbar
  // bookmarks, this means they're imported into a "Bookmarks Toolbar"
  // subfolder under the menu, instead of the real toolbar root.
  let toolbarSubfolder = (await PlacesUtils.bookmarks.search({
    title: "Bookmarks Toolbar",
  })).find(item => item.guid != PlacesUtils.bookmarks.toolbarGuid);
  let importedFields = await PlacesTestUtils.fetchBookmarkSyncFields(
    mozBmk.guid, fxBmk.guid, toolbarSubfolder.guid);
  ok(importedFields.every(field =>
    field.syncStatus == PlacesUtils.bookmarks.SYNC_STATUS.NEW
  ), "Sync statuses should match for HTML imports");

  do_print("Fetch new HTML imports");
  let newChanges = await PlacesSyncUtils.bookmarks.pullChanges();
  deepEqual(Object.keys(newChanges).sort(), [mozBmk.guid, fxBmk.guid,
    toolbarSubfolder.guid, "menu",
    unsyncedBmk.guid].sort(),
    "Should return new IDs imported from HTML file"
  );
  let newFields = await PlacesTestUtils.fetchBookmarkSyncFields(
    unsyncedBmk.guid, mozBmk.guid, fxBmk.guid, toolbarSubfolder.guid);
  ok(newFields.every(field =>
    field.syncStatus == PlacesUtils.bookmarks.SYNC_STATUS.NEW
  ), "Pulling new HTML imports should not mark them as syncing");

  do_print("Mark new HTML imports as syncing");
  await PlacesSyncUtils.bookmarks.markChangesAsSyncing(newChanges);
  let normalFields = await PlacesTestUtils.fetchBookmarkSyncFields(
    unsyncedBmk.guid, mozBmk.guid, fxBmk.guid, toolbarSubfolder.guid);
  ok(normalFields.every(field =>
    field.syncStatus == PlacesUtils.bookmarks.SYNC_STATUS.NORMAL
  ), "Marking new HTML imports as syncing should update their statuses");

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_pullChanges_import_json() {
  await ignoreChangedRoots();

  do_print("Add synced folder");
  let syncedFolder = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "syncedFolder",
  });
  await PlacesTestUtils.setBookmarkSyncFields({
    guid: syncedFolder.guid,
    syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
  });

  do_print("Import new bookmarks from JSON");
  let { path } = do_get_file("./sync_utils_bookmarks.json");
  await BookmarkJSONUtils.importFromFile(path, false);
  {
    let fields = await PlacesTestUtils.fetchBookmarkSyncFields(
      syncedFolder.guid, "NnvGl3CRA4hC", "APzP8MupzA8l");
    deepEqual(fields.map(field => field.syncStatus), [
      PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
      PlacesUtils.bookmarks.SYNC_STATUS.NEW,
      PlacesUtils.bookmarks.SYNC_STATUS.NEW,
    ], "Sync statuses should match for JSON imports");
  }

  do_print("Fetch new JSON imports");
  let newChanges = await PlacesSyncUtils.bookmarks.pullChanges();
  deepEqual(Object.keys(newChanges).sort(), ["NnvGl3CRA4hC", "APzP8MupzA8l",
    "menu", "toolbar", syncedFolder.guid].sort(),
    "Should return items imported from JSON backup"
  );
  let existingFields = await PlacesTestUtils.fetchBookmarkSyncFields(
    syncedFolder.guid, "NnvGl3CRA4hC", "APzP8MupzA8l");
  deepEqual(existingFields.map(field => field.syncStatus), [
    PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
    PlacesUtils.bookmarks.SYNC_STATUS.NEW,
    PlacesUtils.bookmarks.SYNC_STATUS.NEW,
  ], "Pulling new JSON imports should not mark them as syncing");

  do_print("Mark new JSON imports as syncing");
  await PlacesSyncUtils.bookmarks.markChangesAsSyncing(newChanges);
  let normalFields = await PlacesTestUtils.fetchBookmarkSyncFields(
    syncedFolder.guid, "NnvGl3CRA4hC", "APzP8MupzA8l");
  ok(normalFields.every(field =>
    field.syncStatus == PlacesUtils.bookmarks.SYNC_STATUS.NORMAL
  ), "Marking new JSON imports as syncing should update their statuses");

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_pullChanges_restore_json_tracked() {
  await ignoreChangedRoots();

  let unsyncedBmk = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "https://example.com",
  });
  do_print(`Unsynced bookmark GUID: ${unsyncedBmk.guid}`);
  let syncedFolder = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "syncedFolder",
  });
  do_print(`Synced folder GUID: ${syncedFolder.guid}`);
  await PlacesTestUtils.setBookmarkSyncFields({
    guid: syncedFolder.guid,
    syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
  });
  {
    let fields = await PlacesTestUtils.fetchBookmarkSyncFields(
      unsyncedBmk.guid, syncedFolder.guid);
    deepEqual(fields.map(field => field.syncStatus), [
      PlacesUtils.bookmarks.SYNC_STATUS.NEW,
      PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
    ], "Sync statuses should match before restoring from JSON");
  }

  do_print("Restore from JSON, replacing existing items");
  let { path } = do_get_file("./sync_utils_bookmarks.json");
  await BookmarkJSONUtils.importFromFile(path, true);
  {
    let fields = await PlacesTestUtils.fetchBookmarkSyncFields(
      "NnvGl3CRA4hC", "APzP8MupzA8l");
    ok(fields.every(field =>
      field.syncStatus == PlacesUtils.bookmarks.SYNC_STATUS.UNKNOWN
    ), "All bookmarks should be UNKNOWN after restoring from JSON");
  }

  do_print("Fetch new items restored from JSON");
  {
    let changes = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(Object.keys(changes).sort(), [
      "menu",
      "toolbar",
      "unfiled",
      "mobile",
      syncedFolder.guid, // Tombstone.
      "NnvGl3CRA4hC",
      "APzP8MupzA8l",
    ].sort(), "Should restore items from JSON backup");

    let existingFields = await PlacesTestUtils.fetchBookmarkSyncFields(
      PlacesUtils.bookmarks.menuGuid, PlacesUtils.bookmarks.toolbarGuid,
      PlacesUtils.bookmarks.unfiledGuid, PlacesUtils.bookmarks.mobileGuid,
      "NnvGl3CRA4hC", "APzP8MupzA8l");
    deepEqual(existingFields.map(field => field.syncStatus), [
      PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
      PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
      PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
      PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
      PlacesUtils.bookmarks.SYNC_STATUS.UNKNOWN,
      PlacesUtils.bookmarks.SYNC_STATUS.UNKNOWN,
    ], "Pulling items restored from JSON backup should not mark them as syncing");

    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    deepEqual(tombstones.map(({ guid }) => guid), [syncedFolder.guid],
      "Tombstones should exist after restoring from JSON backup");

    await PlacesSyncUtils.bookmarks.markChangesAsSyncing(changes);
    let normalFields = await PlacesTestUtils.fetchBookmarkSyncFields(
      PlacesUtils.bookmarks.menuGuid, PlacesUtils.bookmarks.toolbarGuid,
      PlacesUtils.bookmarks.unfiledGuid, "NnvGl3CRA4hC", "APzP8MupzA8l");
    ok(normalFields.every(field =>
      field.syncStatus == PlacesUtils.bookmarks.SYNC_STATUS.NORMAL
    ), "NEW and UNKNOWN roots restored from JSON backup should be marked as NORMAL");

    strictEqual(changes[syncedFolder.guid].tombstone, true,
      `Should include tombstone for overwritten synced bookmark ${
      syncedFolder.guid}`);
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_pullChanges_custom_roots() {
  await ignoreChangedRoots();

  do_print("Append items to Places root");
  let unsyncedGuids = await populateTree(PlacesUtils.bookmarks.rootGuid, {
    kind: "folder",
    title: "rootFolder",
    children: [{
      kind: "bookmark",
      title: "childBmk",
      url: "https://example.com",
    }, {
      kind: "folder",
      title: "childFolder",
      children: [{
        kind: "bookmark",
        title: "grandChildBmk",
        url: "https://example.org",
      }],
    }],
  }, {
    kind: "bookmark",
    title: "rootBmk",
    url: "https://example.net",
  });

  do_print("Append items to menu");
  let syncedGuids = await populateTree(PlacesUtils.bookmarks.menuGuid, {
    kind: "folder",
    title: "childFolder",
    children: [{
      kind: "bookmark",
      title: "grandChildBmk",
      url: "https://example.info",
    }],
  });

  {
    let newChanges = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(Object.keys(newChanges).sort(), ["menu", syncedGuids.childFolder,
      syncedGuids.grandChildBmk].sort(),
      "Pulling changes should ignore custom roots");
    await setChangesSynced(newChanges);
  }

  do_print("Append sibling to custom root");
  {
    let unsyncedSibling = await PlacesUtils.bookmarks.insert({
      parentGuid: unsyncedGuids.rootFolder,
      url: "https://example.club",
    });
    let changes = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(changes, {}, `Pulling changes should ignore unsynced sibling ${
      unsyncedSibling.guid}`);
  }

  do_print("Clear custom root using old API");
  {
    let unsyncedRootId = await PlacesUtils.promiseItemId(unsyncedGuids.rootFolder);
    PlacesUtils.bookmarks.removeFolderChildren(unsyncedRootId);
    let changes = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(changes, {}, "Clearing custom root should not write tombstones for children");
  }

  do_print("Remove custom root");
  {
    await PlacesUtils.bookmarks.remove(unsyncedGuids.rootFolder);
    let changes = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(changes, {}, "Removing custom root should not write tombstone");
  }

  do_print("Append sibling to menu");
  {
    let syncedSibling = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "https://example.ninja",
    });
    let changes = await PlacesSyncUtils.bookmarks.pullChanges();
    deepEqual(Object.keys(changes).sort(), ["menu", syncedSibling.guid].sort(),
      "Pulling changes should track synced sibling and parent");
    await setChangesSynced(changes);
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_pullChanges_tombstones() {
  await ignoreChangedRoots();

  do_print("Insert new bookmarks");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      guid: "bookmarkAAAA",
      url: "http://example.com/a",
      title: "A",
    }, {
      guid: "bookmarkBBBB",
      url: "http://example.com/b",
      title: "B",
    }],
  });

  do_print("Manually insert conflicting tombstone for new bookmark");
  await PlacesUtils.withConnectionWrapper("test_pullChanges_tombstones",
    async function(db) {
      await db.executeCached(`
        INSERT INTO moz_bookmarks_deleted(guid)
        VALUES(:guid)`,
        { guid: "bookmarkAAAA" });
    }
  );

  let changes = await PlacesSyncUtils.bookmarks.pullChanges();
  deepEqual(Object.keys(changes).sort(), ["bookmarkAAAA", "bookmarkBBBB",
    "menu"], "Should handle undeleted items when returning changes");
  strictEqual(changes.bookmarkAAAA.tombstone, false,
    "Should replace tombstone for A with undeleted item");
  strictEqual(changes.bookmarkBBBB.tombstone, false,
    "Should not report B as deleted");

  await setChangesSynced(changes);

  let newChanges = await PlacesSyncUtils.bookmarks.pullChanges();
  deepEqual(newChanges, {},
    "Should not return changes after marking undeleted items as synced");

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_pushChanges() {
  await ignoreChangedRoots();

  do_print("Populate test bookmarks");
  let guids = await populateTree(PlacesUtils.bookmarks.menuGuid, {
    kind: "bookmark",
    title: "unknownBmk",
    url: "https://example.org",
  }, {
    kind: "bookmark",
    title: "syncedBmk",
    url: "https://example.com",
  }, {
    kind: "bookmark",
    title: "newBmk",
    url: "https://example.info",
  }, {
    kind: "bookmark",
    title: "deletedBmk",
    url: "https://example.edu",
  }, {
    kind: "bookmark",
    title: "unchangedBmk",
    url: "https://example.systems",
  });

  do_print("Update sync statuses");
  await PlacesTestUtils.setBookmarkSyncFields({
    guid: guids.syncedBmk,
    syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
  }, {
    guid: guids.unknownBmk,
    syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.UNKNOWN,
  }, {
    guid: guids.deletedBmk,
    syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
  }, {
    guid: guids.unchangedBmk,
    syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
    syncChangeCounter: 0,
  });

  do_print("Change synced bookmark; should bump change counter");
  await PlacesUtils.bookmarks.update({
    guid: guids.syncedBmk,
    url: "https://example.ninja",
  });

  do_print("Remove synced bookmark");
  {
    await PlacesUtils.bookmarks.remove(guids.deletedBmk);
    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    ok(tombstones.some(({ guid }) => guid == guids.deletedBmk),
      "Should write tombstone for deleted synced bookmark");
  }

  do_print("Pull changes");
  let changes = await PlacesSyncUtils.bookmarks.pullChanges();
  {
    let actualChanges = Object.entries(changes).map(([recordId, change]) => ({
      recordId,
      syncChangeCounter: change.counter,
    }));
    let expectedChanges = [{
      recordId: guids.unknownBmk,
      syncChangeCounter: 1,
    }, {
      // Parent of changed bookmarks.
      recordId: "menu",
      syncChangeCounter: 6,
    }, {
      recordId: guids.syncedBmk,
      syncChangeCounter: 2,
    }, {
      recordId: guids.newBmk,
      syncChangeCounter: 1,
    }, {
      recordId: guids.deletedBmk,
      syncChangeCounter: 1,
    }];
    deepEqual(sortBy(actualChanges, "recordId"), sortBy(expectedChanges, "recordId"),
      "Should return deleted, new, and unknown bookmarks"
    );
  }

  do_print("Modify changed bookmark to bump its counter");
  await PlacesUtils.bookmarks.update({
    guid: guids.newBmk,
    url: "https://example.club",
  });

  do_print("Mark some bookmarks as synced");
  for (let title of ["unknownBmk", "newBmk", "deletedBmk"]) {
    let guid = guids[title];
    strictEqual(changes[guid].synced, false,
      "All bookmarks should not be marked as synced yet");
    changes[guid].synced = true;
  }

  await PlacesSyncUtils.bookmarks.pushChanges(changes);

  {
    let fields = await PlacesTestUtils.fetchBookmarkSyncFields(
      guids.newBmk, guids.unknownBmk);
    ok(fields.every(field =>
      field.syncStatus == PlacesUtils.bookmarks.SYNC_STATUS.NORMAL
    ), "Should update sync statuses for synced bookmarks");
  }

  {
    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    ok(!tombstones.some(({ guid }) => guid == guids.deletedBmk),
      "Should remove tombstone after syncing");

    let syncFields = await PlacesTestUtils.fetchBookmarkSyncFields(
      guids.unknownBmk, guids.syncedBmk, guids.newBmk);
    {
      let info = syncFields.find(field => field.guid == guids.unknownBmk);
      equal(info.syncStatus, PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
        "Syncing an UNKNOWN bookmark should set its sync status to NORMAL");
      strictEqual(info.syncChangeCounter, 0,
        "Syncing an UNKNOWN bookmark should reduce its change counter");
    }
    {
      let info = syncFields.find(field => field.guid == guids.syncedBmk);
      equal(info.syncStatus, PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
        "Syncing a NORMAL bookmark should not update its sync status");
      equal(info.syncChangeCounter, 2,
        "Should not reduce counter for NORMAL bookmark not marked as synced");
    }
    {
      let info = syncFields.find(field => field.guid == guids.newBmk);
      equal(info.syncStatus, PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
        "Syncing a NEW bookmark should update its sync status");
      strictEqual(info.syncChangeCounter, 1,
        "Updating new bookmark after pulling changes should bump change counter");
    }
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_touch() {
  await ignoreChangedRoots();

  strictEqual(await PlacesSyncUtils.bookmarks.touch(makeGuid()), null,
    "Should not revive nonexistent items");

  {
    let folder = await PlacesSyncUtils.bookmarks.insert({
      kind: "folder",
      recordId: makeGuid(),
      parentRecordId: "menu",
    });
    strictEqual(await PlacesSyncUtils.bookmarks.touch(folder.recordId), null,
      "Should not revive folders");
  }

  {
    let bmk = await PlacesSyncUtils.bookmarks.insert({
      kind: "bookmark",
      recordId: makeGuid(),
      parentRecordId: "menu",
      url: "https://mozilla.org",
    });

    let changes = await PlacesSyncUtils.bookmarks.touch(bmk.recordId);
    deepEqual(Object.keys(changes).sort(), [bmk.recordId, "menu"].sort(),
      "Should return change records for revived bookmark and parent");
    equal(changes[bmk.recordId].counter, 1,
      "Change counter for revived bookmark should be 1");

    await setChangesSynced(changes);
  }

  // Livemarks are stored as folders, but their kinds are different, so we
  // should still bump their change counters.
  let { site, stopServer } = makeLivemarkServer();
  try {
    let livemark = await PlacesSyncUtils.bookmarks.insert({
      kind: "livemark",
      recordId: makeGuid(),
      feed: site + "/feed/1",
      parentRecordId: "unfiled",
    });

    let changes = await PlacesSyncUtils.bookmarks.touch(livemark.recordId);
    deepEqual(Object.keys(changes).sort(), [livemark.recordId, "unfiled"].sort(),
      "Should return change records for revived livemark and parent");
    equal(changes[livemark.recordId].counter, 1,
      "Change counter for revived livemark should be 1");
  } finally {
    await stopServer();
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_separator() {
  await ignoreChangedRoots();

  await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    parentRecordId: "menu",
    recordId: makeGuid(),
    url: "https://example.com",
  });
  let childBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    parentRecordId: "menu",
    recordId: makeGuid(),
    url: "https://foo.bar",
  });
  let separatorRecordId = makeGuid();
  let separator = await PlacesSyncUtils.bookmarks.insert({
    kind: "separator",
    parentRecordId: "menu",
    recordId: separatorRecordId
  });
  await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    parentRecordId: "menu",
    recordId: makeGuid(),
    url: "https://bar.foo",
  });
  let child2Id = await recordIdToId(childBmk.recordId);
  let parentId = await recordIdToId("menu");
  let separatorId = await recordIdToId(separator.recordId);
  let separatorGuid = PlacesSyncUtils.bookmarks.recordIdToGuid(separatorRecordId);

  do_print("Move a bookmark around the separator");
  PlacesUtils.bookmarks.moveItem(child2Id, parentId, separator + 1);
  let changes = await PlacesSyncUtils.bookmarks.pullChanges();
  deepEqual(Object.keys(changes).sort(),
    [separator.recordId, "menu"].sort());

  await setChangesSynced(changes);

  do_print("Move a separator around directly");
  PlacesUtils.bookmarks.moveItem(separatorId, parentId, 0);
  changes = await PlacesSyncUtils.bookmarks.pullChanges();
  deepEqual(Object.keys(changes).sort(),
    [separator.recordId, "menu"].sort());

  await setChangesSynced(changes);

  do_print("Move a separator around directly using update");
  await PlacesUtils.bookmarks.update({ guid: separatorGuid, index: 2 });
  changes = await PlacesSyncUtils.bookmarks.pullChanges();
  deepEqual(Object.keys(changes).sort(),
    [separator.recordId, "menu"].sort());

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_remove() {
  await ignoreChangedRoots();

  do_print("Insert subtree for removal");
  let parentFolder = await PlacesSyncUtils.bookmarks.insert({
    kind: "folder",
    parentRecordId: "menu",
    recordId: makeGuid(),
  });
  let childBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    parentRecordId: parentFolder.recordId,
    recordId: makeGuid(),
    url: "https://example.com",
  });
  let childFolder = await PlacesSyncUtils.bookmarks.insert({
    kind: "folder",
    parentRecordId: parentFolder.recordId,
    recordId: makeGuid(),
  });
  let grandChildBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    parentRecordId: childFolder.recordId,
    recordId: makeGuid(),
    url: "https://example.edu",
  });

  do_print("Remove entire subtree");
  await PlacesSyncUtils.bookmarks.remove([
    parentFolder.recordId,
    childFolder.recordId,
    childBmk.recordId,
    grandChildBmk.recordId,
  ]);

  /**
   * Even though we've removed the entire subtree, we still track the menu
   * because we 1) removed `parentFolder`, 2) reparented `childFolder` to
   * `menu`, and 3) removed `childFolder`.
   *
   * This depends on the order of the folders passed to `remove`. If we
   * removed `childFolder` *before* `parentFolder`, we wouldn't reparent
   * anything to `menu`.
   *
   * `deleteSyncedFolder` could check if it's reparenting an item that will
   * eventually be removed, and avoid bumping the new parent's change counter.
   * Unfortunately, that introduces inconsistencies if `deleteSyncedFolder` is
   * interrupted by shutdown. If the server changes before the next sync,
   * we'll never upload records for the reparented item or the new parent.
   *
   * Another alternative: we can try to remove folders in level order, instead
   * of the order passed to `remove`. But that means we need a recursive query
   * to determine the order. This is already enough of an edge case that
   * occasionally reuploading the closest living ancestor is the simplest
   * solution.
   */
  let changes = await PlacesSyncUtils.bookmarks.pullChanges();
  deepEqual(Object.keys(changes), ["menu"],
    "Should track closest living ancestor of removed subtree");

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_remove_partial() {
  await ignoreChangedRoots();

  do_print("Insert subtree for partial removal");
  let parentFolder = await PlacesSyncUtils.bookmarks.insert({
    kind: "folder",
    parentRecordId: PlacesUtils.bookmarks.menuGuid,
    recordId: makeGuid(),
  });
  let prevSiblingBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    parentRecordId: parentFolder.recordId,
    recordId: makeGuid(),
    url: "https://example.net",
  });
  let childBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    parentRecordId: parentFolder.recordId,
    recordId: makeGuid(),
    url: "https://example.com",
  });
  let nextSiblingBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    parentRecordId: parentFolder.recordId,
    recordId: makeGuid(),
    url: "https://example.org",
  });
  let childFolder = await PlacesSyncUtils.bookmarks.insert({
    kind: "folder",
    parentRecordId: parentFolder.recordId,
    recordId: makeGuid(),
  });
  let grandChildBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    parentRecordId: parentFolder.recordId,
    recordId: makeGuid(),
    url: "https://example.edu",
  });
  let grandChildSiblingBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    parentRecordId: parentFolder.recordId,
    recordId: makeGuid(),
    url: "https://mozilla.org",
  });
  let grandChildFolder = await PlacesSyncUtils.bookmarks.insert({
    kind: "folder",
    parentRecordId: childFolder.recordId,
    recordId: makeGuid(),
  });
  let greatGrandChildPrevSiblingBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    parentRecordId: grandChildFolder.recordId,
    recordId: makeGuid(),
    url: "http://getfirefox.com",
  });
  let greatGrandChildNextSiblingBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    parentRecordId: grandChildFolder.recordId,
    recordId: makeGuid(),
    url: "http://getthunderbird.com",
  });
  let menuBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    parentRecordId: "menu",
    recordId: makeGuid(),
    url: "https://example.info",
  });

  do_print("Remove subset of folders and items in subtree");
  let changes = await PlacesSyncUtils.bookmarks.remove([
    parentFolder.recordId,
    childBmk.recordId,
    grandChildFolder.recordId,
    grandChildBmk.recordId,
    childFolder.recordId,
  ]);
  deepEqual(Object.keys(changes).sort(), [
    // Closest living ancestor.
    "menu",
    // Reparented bookmarks.
    prevSiblingBmk.recordId,
    nextSiblingBmk.recordId,
    grandChildSiblingBmk.recordId,
    greatGrandChildPrevSiblingBmk.recordId,
    greatGrandChildNextSiblingBmk.recordId,
  ].sort(), "Should track reparented bookmarks and their closest living ancestor");

  /**
   * Reparented bookmarks should maintain their order relative to their
   * siblings: `prevSiblingBmk` (0) should precede `nextSiblingBmk` (2) in the
   * menu, and `greatGrandChildPrevSiblingBmk` (0) should precede
   * `greatGrandChildNextSiblingBmk` (1).
   */
  let menuChildren = await PlacesSyncUtils.bookmarks.fetchChildRecordIds(
    PlacesUtils.bookmarks.menuGuid);
  deepEqual(menuChildren, [
    // Existing bookmark.
    menuBmk.recordId,
    // 1) Moved out of `parentFolder` to `menu`.
    prevSiblingBmk.recordId,
    nextSiblingBmk.recordId,
    // 3) Moved out of `childFolder` to `menu`. After this step, `childFolder`
    // is deleted.
    grandChildSiblingBmk.recordId,
    // 2) Moved out of `grandChildFolder` to `childFolder`, because we remove
    // `grandChildFolder` *before* `childFolder`. After this step,
    // `grandChildFolder` is deleted and `childFolder`'s children are
    // `[grandChildSiblingBmk, greatGrandChildPrevSiblingBmk,
    // greatGrandChildNextSiblingBmk]`.
    greatGrandChildPrevSiblingBmk.recordId,
    greatGrandChildNextSiblingBmk.recordId,
  ], "Should move descendants to closest living ancestor");

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_migrateOldTrackerEntries() {
  let unknownBmk = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "http://getfirefox.com",
    title: "Get Firefox!",
  });
  let newBmk = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "http://getthunderbird.com",
    title: "Get Thunderbird!",
  });
  let normalBmk = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "https://mozilla.org",
    title: "Mozilla",
  });

  await PlacesTestUtils.setBookmarkSyncFields({
    guid: unknownBmk.guid,
    syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.UNKNOWN,
    syncChangeCounter: 0,
  }, {
    guid: normalBmk.guid,
    syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
  });
  PlacesUtils.tagging.tagURI(uri("http://getfirefox.com"), ["taggy"]);

  let tombstoneRecordId = makeGuid();
  await PlacesSyncUtils.bookmarks.migrateOldTrackerEntries([{
    recordId: normalBmk.guid,
    modified: Date.now(),
  }, {
    recordId: tombstoneRecordId,
    modified: 1479162463976,
  }]);

  let changes = await PlacesSyncUtils.bookmarks.pullChanges();
  deepEqual(Object.keys(changes).sort(), [normalBmk.guid, tombstoneRecordId].sort(),
    "Should return change records for migrated bookmark and tombstone");

  let fields = await PlacesTestUtils.fetchBookmarkSyncFields(
    unknownBmk.guid, newBmk.guid, normalBmk.guid);
  for (let field of fields) {
    if (field.guid == normalBmk.guid) {
      ok(field.lastModified > normalBmk.lastModified,
        `Should bump last modified date for migrated bookmark ${field.guid}`);
      equal(field.syncChangeCounter, 1,
        `Should bump change counter for migrated bookmark ${field.guid}`);
    } else {
      strictEqual(field.syncChangeCounter, 0,
        `Should not bump change counter for ${field.guid}`);
    }
    equal(field.syncStatus, PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
      `Should set sync status for ${field.guid} to NORMAL`);
  }

  let tombstones = await PlacesTestUtils.fetchSyncTombstones();
  deepEqual(tombstones, [{
    guid: tombstoneRecordId,
    dateRemoved: new Date(1479162463976),
  }], "Should write tombstone for nonexistent migrated item");

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_ensureMobileQuery() {
  do_print("Ensure we correctly create the mobile query");

  let PlacesUIUtils;
  try {
    PlacesUIUtils = Cu.import("resource:///modules/PlacesUIUtils.jsm", {}).PlacesUIUtils;
    PlacesUIUtils.maybeRebuildLeftPane();
  } catch (ex) {
    do_print("Can't build left pane roots; skipping test");
    return;
  }

  await PlacesUtils.bookmarks.insert({
    guid: "bookmarkAAAA",
    parentGuid: PlacesUtils.bookmarks.mobileGuid,
    url: "http://example.com/a",
    title: "A",
  });

  await PlacesUtils.bookmarks.insert({
    guid: "bookmarkBBBB",
    parentGuid: PlacesUtils.bookmarks.mobileGuid,
    url: "http://example.com/b",
    title: "B",
  });

  // Creates the organizer queries as a side effect.
  let leftPaneId = PlacesUIUtils.leftPaneFolderId;
  do_print(`Left pane root ID: ${leftPaneId}`);

  let allBookmarksGuids = await PlacesSyncUtils.bookmarks.fetchGuidsWithAnno(
    "PlacesOrganizer/OrganizerQuery", "AllBookmarks");
  equal(allBookmarksGuids.length, 1, "Should create folder with all bookmarks queries");
  let allBookmarkGuid = allBookmarksGuids[0];

  do_print("Try creating query after organizer is ready");
  await PlacesSyncUtils.bookmarks.ensureMobileQuery();
  let queryGuids = await PlacesSyncUtils.bookmarks.fetchGuidsWithAnno(
    "PlacesOrganizer/OrganizerQuery", "MobileBookmarks");
  equal(queryGuids.length, 1, "Should create query because we have bookmarks A and B");

  let queryGuid = queryGuids[0];

  let queryInfo = await PlacesUtils.bookmarks.fetch(queryGuid);
  equal(queryInfo.url, `place:folder=MOBILE_BOOKMARKS`, "Query should point to mobile root");
  equal(queryInfo.title, "Mobile Bookmarks", "Query title should be localized");
  equal(queryInfo.parentGuid, allBookmarkGuid, "Should append mobile query to all bookmarks queries");

  do_print("Rename root and query, then recreate");
  await PlacesUtils.bookmarks.update({
    guid: PlacesUtils.bookmarks.mobileGuid,
    title: "renamed root",
  });
  await PlacesUtils.bookmarks.update({
    guid: queryGuid,
    title: "renamed query",
  });
  await PlacesSyncUtils.bookmarks.ensureMobileQuery();
  let rootInfo = await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.mobileGuid);
  equal(rootInfo.title, "Mobile Bookmarks", "Should fix root title");
  queryInfo = await PlacesUtils.bookmarks.fetch(queryGuid);
  equal(queryInfo.title, "Mobile Bookmarks", "Should fix query title");

  do_print("Point query to different folder");
  await PlacesUtils.bookmarks.update({
    guid: queryGuid,
    url: "place:folder=BOOKMARKS_MENU",
  });
  await PlacesSyncUtils.bookmarks.ensureMobileQuery();
  queryInfo = await PlacesUtils.bookmarks.fetch(queryGuid);
  equal(queryInfo.url.href, `place:folder=MOBILE_BOOKMARKS`,
    "Should fix query URL to point to mobile root");

  do_print("We shouldn't track the query or the left pane root");

  let changes = await PlacesSyncUtils.bookmarks.pullChanges();
  ok(!(queryGuid in changes), "Should not track mobile query");

  await PlacesUtils.bookmarks.remove("bookmarkAAAA");
  await PlacesUtils.bookmarks.remove("bookmarkBBBB");
  await PlacesSyncUtils.bookmarks.ensureMobileQuery();

  queryGuids = await PlacesSyncUtils.bookmarks.fetchGuidsWithAnno(
    "PlacesOrganizer/OrganizerQuery", "MobileBookmarks");
  equal(queryGuids.length, 0, "Should delete query since there are no bookmarks");

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_remove_stale_tombstones() {
  do_print("Insert and delete synced bookmark");
  {
    await PlacesUtils.bookmarks.insert({
      guid: "bookmarkAAAA",
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "http://example.com/a",
      title: "A",
      source: PlacesUtils.bookmarks.SOURCES.SYNC,
    });
    await PlacesUtils.bookmarks.remove("bookmarkAAAA");
    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    deepEqual(tombstones.map(({ guid }) => guid), ["bookmarkAAAA"],
      "Should store tombstone for deleted synced bookmark");
  }

  do_print("Reinsert deleted bookmark");
  {
    // Different parent, URL, and title, but same GUID.
    await PlacesUtils.bookmarks.insert({
      guid: "bookmarkAAAA",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: "http://example.com/a-restored",
      title: "A (Restored)",
    });

    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    deepEqual(tombstones, [],
      "Should remove tombstone for reinserted bookmark");
  }

  do_print("Insert tree and erase everything");
  {
    await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.menuGuid,
      source: PlacesUtils.bookmarks.SOURCES.SYNC,
      children: [{
        guid: "bookmarkBBBB",
        url: "http://example.com/b",
        title: "B",
      }, {
        guid: "bookmarkCCCC",
        url: "http://example.com/c",
        title: "C",
      }],
    });
    await PlacesUtils.bookmarks.eraseEverything();
    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    deepEqual(tombstones.map(({ guid }) => guid).sort(), ["bookmarkBBBB",
      "bookmarkCCCC"], "Should store tombstones after erasing everything");
  }

  do_print("Reinsert tree");
  {
    await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.mobileGuid,
      children: [{
        guid: "bookmarkBBBB",
        url: "http://example.com/b",
        title: "B",
      }, {
        guid: "bookmarkCCCC",
        url: "http://example.com/c",
        title: "C",
      }],
    });
    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    deepEqual(tombstones.map(({ guid }) => guid).sort(), [],
      "Should remove tombstones after reinserting tree");
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
