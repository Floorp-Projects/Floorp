/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_livemarks() {
  let { site, stopServer } = makeLivemarkServer();

  try {
    let buf = await openMirror("livemarks");

    info("Set up mirror");
    await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.menuGuid,
      children: [{
        guid: "livemarkAAAA",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "A",
        annos: [{
          name: PlacesUtils.LMANNO_FEEDURI,
          value: site + "/feed/a",
        }],
      }],
    });
    await buf.store(shuffle([{
      id: "menu",
      type: "folder",
      children: ["livemarkAAAA"],
    }, {
      id: "livemarkAAAA",
      type: "livemark",
      title: "A",
      feedUri: site + "/feed/a",
    }]), { needsMerge: false });
    await PlacesTestUtils.markBookmarksAsSynced();

    info("Make local changes");
    await PlacesUtils.livemarks.addLivemark({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      guid: "livemarkBBBB",
      title: "B",
      feedURI: Services.io.newURI(site + "/feed/b-local"),
      siteURI: Services.io.newURI(site + "/site/b-local"),
    });
    let livemarkD = await PlacesUtils.livemarks.addLivemark({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      guid: "livemarkDDDD",
      title: "D",
      feedURI: Services.io.newURI(site + "/feed/d"),
      siteURI: Services.io.newURI(site + "/site/d"),
    });

    info("Make remote changes");
    await buf.store(shuffle([{
      id: "livemarkAAAA",
      type: "livemark",
      title: "A (remote)",
      feedUri: site + "/feed/a-remote",
    }, {
      id: "toolbar",
      type: "folder",
      children: ["livemarkCCCC", "livemarkB111"],
    }, {
      id: "unfiled",
      type: "folder",
      children: ["livemarkEEEE"],
    }, {
      id: "livemarkCCCC",
      type: "livemark",
      title: "C (remote)",
      feedUri: site + "/feed/c-remote",
    }, {
      id: "livemarkB111",
      type: "livemark",
      title: "B",
      feedUri: site + "/feed/b-remote",
    }, {
      id: "livemarkEEEE",
      type: "livemark",
      title: "E",
      feedUri: site + "/feed/e",
      siteUri: site + "/site/e",
    }]));

    info("Apply remote");
    let changesToUpload = await buf.apply();
    deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

    let menuInfo = await PlacesUtils.bookmarks.fetch(
      PlacesUtils.bookmarks.menuGuid);
    let toolbarInfo = await PlacesUtils.bookmarks.fetch(
      PlacesUtils.bookmarks.toolbarGuid);
    deepEqual(changesToUpload, {
      livemarkDDDD: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "livemarkDDDD",
          type: "livemark",
          parentid: "menu",
          hasDupe: true,
          parentName: BookmarksMenuTitle,
          dateAdded: PlacesUtils.toDate(livemarkD.dateAdded).getTime(),
          title: "D",
          feedUri: site + "/feed/d",
          siteUri: site + "/site/d",
        },
      },
      menu: {
        tombstone: false,
        counter: 2,
        synced: false,
        cleartext: {
          id: "menu",
          type: "folder",
          parentid: "places",
          hasDupe: true,
          parentName: "",
          dateAdded: menuInfo.dateAdded.getTime(),
          title: menuInfo.title,
          children: ["livemarkAAAA", "livemarkDDDD"],
        },
      },
      toolbar: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "toolbar",
          type: "folder",
          parentid: "places",
          hasDupe: true,
          parentName: "",
          dateAdded: toolbarInfo.dateAdded.getTime(),
          title: toolbarInfo.title,
          children: ["livemarkCCCC", "livemarkB111"],
        },
      },
    }, "Should upload new local livemark A");

    await assertLocalTree(PlacesUtils.bookmarks.rootGuid, {
      guid: PlacesUtils.bookmarks.rootGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: "",
      children: [{
        guid: PlacesUtils.bookmarks.menuGuid,
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        index: 0,
        title: BookmarksMenuTitle,
        children: [{
          guid: "livemarkAAAA",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 0,
          title: "A (remote)",
          annos: [{
            name: PlacesUtils.LMANNO_FEEDURI,
            flags: 0,
            expires: PlacesUtils.annotations.EXPIRE_NEVER,
            value: site + "/feed/a-remote",
          }],
        }, {
          guid: "livemarkDDDD",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 1,
          title: "D",
          annos: [{
            name: PlacesUtils.LMANNO_FEEDURI,
            flags: 0,
            expires: PlacesUtils.annotations.EXPIRE_NEVER,
            value: site + "/feed/d",
          }, {
            name: PlacesUtils.LMANNO_SITEURI,
            flags: 0,
            expires: PlacesUtils.annotations.EXPIRE_NEVER,
            value: site + "/site/d",
          }],
        }],
      }, {
        guid: PlacesUtils.bookmarks.toolbarGuid,
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        index: 1,
        title: BookmarksToolbarTitle,
        children: [{
          guid: "livemarkCCCC",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 0,
          title: "C (remote)",
          annos: [{
            name: PlacesUtils.LMANNO_FEEDURI,
            flags: 0,
            expires: PlacesUtils.annotations.EXPIRE_NEVER,
            value: site + "/feed/c-remote",
          }],
        }, {
          guid: "livemarkB111",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 1,
          title: "B",
          annos: [{
            name: PlacesUtils.LMANNO_FEEDURI,
            flags: 0,
            expires: PlacesUtils.annotations.EXPIRE_NEVER,
            value: site + "/feed/b-remote",
          }],
        }],
      }, {
        guid: PlacesUtils.bookmarks.unfiledGuid,
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        index: 3,
        title: UnfiledBookmarksTitle,
        children: [{
          guid: "livemarkEEEE",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 0,
          title: "E",
          annos: [{
            name: PlacesUtils.LMANNO_FEEDURI,
            flags: 0,
            expires: PlacesUtils.annotations.EXPIRE_NEVER,
            value: site + "/feed/e",
          }, {
            name: PlacesUtils.LMANNO_SITEURI,
            flags: 0,
            expires: PlacesUtils.annotations.EXPIRE_NEVER,
            value: site + "/site/e",
          }],
        }],
      }, {
        guid: PlacesUtils.bookmarks.mobileGuid,
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        index: 4,
        title: MobileBookmarksTitle,
      }],
    }, "Should apply and dedupe livemarks");

    let cLivemark = await PlacesUtils.livemarks.getLivemark({
      guid: "livemarkCCCC",
    });
    equal(cLivemark.title, "C (remote)", "Should set livemark C title");
    ok(cLivemark.feedURI.equals(Services.io.newURI(site + "/feed/c-remote")),
      "Should set livemark C feed URL");

    let bLivemark = await PlacesUtils.livemarks.getLivemark({
      guid: "livemarkB111",
    });
    ok(bLivemark.feedURI.equals(Services.io.newURI(site + "/feed/b-remote")),
      "Should set deduped livemark B feed URL");
    strictEqual(bLivemark.siteURI, null,
      "Should remove deduped livemark B site URL");

    await buf.finalize();
  } finally {
    await stopServer();
  }

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_queries() {
  try {
    let buf = await openMirror("queries");

    info("Set up places");

    // create a tag and grab the local folder ID.
    let tag = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.tagsGuid,
      title: "a-tag",
    });
    let tagid = await PlacesUtils.promiseItemId(tag.guid);

    await PlacesTestUtils.markBookmarksAsSynced();

    await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.menuGuid,
      children: [
        {
          // this entry has a folder= query param for a folder that exists.
          guid: "queryAAAAAAA",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          title: "TAG_QUERY query",
          url: `place:type=6&sort=14&maxResults=10&folder=${tagid}`,
        },
        {
          // this entry has a folder= query param for a folder that doesn't exist.
          guid: "queryBBBBBBB",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          title: "TAG_QUERY query but invalid folder id",
          url: `place:type=6&sort=14&maxResults=10&folder=12345`,
        },
        {
          // this entry has no folder= query param.
          guid: "queryCCCCCCC",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          title: "TAG_QUERY without a folder at all",
          url: "place:type=6&sort=14&maxResults=10",
        },

        ],
    });

    info("Create records to upload");
    let changes = await buf.apply();
    Assert.strictEqual(changes.queryAAAAAAA.cleartext.folderName, tag.title);
    Assert.strictEqual(changes.queryBBBBBBB.cleartext.folderName, undefined);
    Assert.strictEqual(changes.queryCCCCCCC.cleartext.folderName, undefined);
  } finally {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesSyncUtils.bookmarks.reset();
  }
});

// Bug 632287.
add_task(async function test_mismatched_but_compatible_folder_types() {
  let buf = await openMirror("mismatched_types");

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      guid: "l1nZZXfB8nC7",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "Innerst i Sneglehode",
    }],
  });
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make remote changes");
  await buf.store([{
    "id": "l1nZZXfB8nC7",
    "type": "livemark",
    "siteUri": "http://sneglehode.wordpress.com/",
    "feedUri": "http://sneglehode.wordpress.com/feed/",
    "parentName": "Bookmarks Toolbar",
    "title": "Innerst i Sneglehode",
    "description": null,
    "children":
      ["HCRq40Rnxhrd", "YeyWCV1RVsYw", "GCceVZMhvMbP", "sYi2hevdArlF",
       "vjbZlPlSyGY8", "UtjUhVyrpeG6", "rVq8WMG2wfZI", "Lx0tcy43ZKhZ",
       "oT74WwV8_j4P", "IztsItWVSo3-"],
    "parentid": "toolbar"
  }]);

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(idsToUpload, {
    updated: [],
    deleted: [],
  }, "Should not reupload merged livemark");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_mismatched_but_incompatible_folder_types() {
  let sawMismatchEvent = false;
  let recordTelemetryEvent = (object, method, value, extra) => {
    // expecting to see a kind-mismatch event.
    if (value == "kind-mismatch" &&
        extra.local && typeof extra.local == "string" &&
        extra.local == "livemark" &&
        extra.remote && typeof extra.remote == "string" &&
        extra.remote == "folder") {
      sawMismatchEvent = true;
    }
  };
  let buf = await openMirror("mismatched_incompatible_types",
                             {recordTelemetryEvent});
  try {
    info("Set up mirror");
    await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.menuGuid,
      children: [{
        guid: "livemarkAAAA",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "LiveMark",
        annos: [{
          name: PlacesUtils.LMANNO_FEEDURI,
          value: "http://example.com/feed/a",
        }],
      }],
    });
    await PlacesTestUtils.markBookmarksAsSynced();

    info("Make remote changes");
    await buf.store([{
      "id": "livemarkAAAA",
      "type": "folder",
      "title": "not really a Livemark",
      "description": null,
      "parentid": "menu"
    }]);

    info("Apply remote, should fail");
    await Assert.rejects(buf.apply(), /Can't merge different item kinds/);
    Assert.ok(sawMismatchEvent, "saw the correct mismatch event");
  } finally {
    await buf.finalize();
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesSyncUtils.bookmarks.reset();
  }
});

add_task(async function test_different_but_compatible_bookmark_types() {
  try {
    let buf = await openMirror("partial_queries");

    await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.menuGuid,
      children: [
        {
          guid: "bookmarkAAAA",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          title: "not yet a query",
          url: "about:blank",
        },
        {
          guid: "bookmarkBBBB",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          title: "a query",
          url: "place:foo",

        }
      ],
    });

    let changes = await buf.apply();
    // We should have an outgoing record for bookmarkA with type=bookmark
    // and bookmarkB with type=query.
    Assert.equal(changes.bookmarkAAAA.cleartext.type, "bookmark");
    Assert.equal(changes.bookmarkBBBB.cleartext.type, "query");

    // Now pretend that same records are already on the server.
    await buf.store([{
      id: "menu",
      type: "folder",
      children: ["bookmarkAAAA", "bookmarkBBBB"],
    }, {
      id: "bookmarkAAAA",
      parentid: "menu",
      type: "bookmark",
      title: "not yet a query",
      bmkUri: "about:blank",
    }, {
      id: "bookmarkBBBB",
      parentid: "menu",
      type: "query",
      title: "a query",
      bmkUri: "place:foo",

    }], { needsMerge: false });
    await PlacesTestUtils.markBookmarksAsSynced();

    // change the url of bookmarkA to be a "real" query and of bookmarkB to
    // no longer be a query.
    await PlacesUtils.bookmarks.update({
      guid: "bookmarkAAAA",
      url: "place:type=6&sort=14&maxResults=10",
    });
    await PlacesUtils.bookmarks.update({
      guid: "bookmarkBBBB",
      url: "about:robots",
    });

    changes = await buf.apply();
    // We should have an outgoing record for bookmarkA with type=query and
    // for bookmarkB with type=bookmark
    Assert.equal(changes.bookmarkAAAA.cleartext.type, "query");
    Assert.equal(changes.bookmarkBBBB.cleartext.type, "bookmark");
  } finally {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesSyncUtils.bookmarks.reset();
  }
});

add_task(async function test_incompatible_types() {
  let sawMismatchEvent = false;
  let recordTelemetryEvent = (object, method, value, extra) => {
    // expecting to see a kind-mismatch event.
    if (value == "kind-mismatch" &&
        extra.local && typeof extra.local == "string" &&
        extra.local == "bookmark" &&
        extra.remote && typeof extra.remote == "string" &&
        extra.remote == "folder") {
      sawMismatchEvent = true;
    }
  };
  try {
    let buf = await openMirror("partial_queries", {recordTelemetryEvent});

    await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.menuGuid,
      children: [
        {
          guid: "AAAAAAAAAAAA",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          title: "a bookmark",
          url: "about:blank",
        },
      ],
    });

    await buf.apply();

    // Now pretend that same records are already on the server with incompatible
    // types.
    await buf.store([{
      id: "menu",
      type: "folder",
      children: ["AAAAAAAAAAAA"],
    }, {
      id: "AAAAAAAAAAAA",
      parentId: PlacesSyncUtils.bookmarks.guidToRecordId(PlacesUtils.bookmarks.menuGuid),
      type: "folder",
      title: "conflicting folder",
    }], { needsMerge: true });
    await PlacesTestUtils.markBookmarksAsSynced();

    await Assert.rejects(buf.apply(), /Can't merge different item kinds/);
    Assert.ok(sawMismatchEvent, "saw expected mismatch event");
  } finally {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesSyncUtils.bookmarks.reset();
  }
});
