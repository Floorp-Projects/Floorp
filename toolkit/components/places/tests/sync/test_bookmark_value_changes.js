/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_value_combo() {
  let buf = await openMirror("value_combo");
  let now = Date.now();

  info("Set up mirror with existing bookmark to update");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "mozBmk______",
        url: "https://mozilla.org",
        title: "Mozilla",
        tags: ["moz", "dot", "org"],
        dateAdded: new Date(now),
      },
    ],
  });
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["mozBmk______"],
      },
      {
        id: "mozBmk______",
        parentid: "menu",
        type: "bookmark",
        title: "Mozilla",
        bmkUri: "https://mozilla.org",
        tags: ["moz", "dot", "org"],
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Insert new local bookmark to upload");
  let [bzBmk] = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [
      {
        guid: "bzBmk_______",
        url: "https://bugzilla.mozilla.org",
        title: "Bugzilla",
        tags: ["new", "tag"],
      },
    ],
  });

  info("Insert remote bookmarks and folder to apply");
  await storeRecords(
    buf,
    shuffle([
      {
        id: "mozBmk______",
        parentid: "menu",
        type: "bookmark",
        title: "Mozilla home page",
        bmkUri: "https://mozilla.org",
        tags: ["browsers"],
      },
      {
        id: "toolbar",
        parentid: "places",
        type: "folder",
        children: ["fxBmk_______", "tFolder_____"],
      },
      {
        id: "fxBmk_______",
        parentid: "toolbar",
        type: "bookmark",
        title: "Get Firefox",
        bmkUri: "http://getfirefox.com",
        tags: ["taggy", "browsers"],
        dateAdded: now,
      },
      {
        id: "tFolder_____",
        parentid: "toolbar",
        type: "folder",
        title: "Mail",
        children: ["tbBmk_______"],
        dateAdded: now,
      },
      {
        id: "tbBmk_______",
        parentid: "tFolder_____",
        type: "bookmark",
        title: "Get Thunderbird",
        bmkUri: "http://getthunderbird.com",
        keyword: "tb",
        dateAdded: now,
      },
    ])
  );

  info("Apply remote");
  let observer = expectBookmarkChangeNotifications({
    skipTags: true,
    ignoreDates: false,
  });
  let localTimeSeconds = Math.floor(now / 1000);
  let changesToUpload = await buf.apply({
    localTimeSeconds,
    notifyInStableOrder: true,
  });
  deepEqual(
    await buf.fetchUnmergedGuids(),
    [PlacesUtils.bookmarks.toolbarGuid],
    "Should leave toolbar with new remote structure unmerged"
  );

  let menuInfo = await PlacesUtils.bookmarks.fetch(
    PlacesUtils.bookmarks.menuGuid
  );
  deepEqual(
    changesToUpload,
    {
      bzBmk_______: {
        tombstone: false,
        counter: 3,
        synced: false,
        cleartext: {
          id: "bzBmk_______",
          type: "bookmark",
          parentid: "toolbar",
          hasDupe: true,
          parentName: BookmarksToolbarTitle,
          dateAdded: bzBmk.dateAdded.getTime(),
          bmkUri: "https://bugzilla.mozilla.org/",
          title: "Bugzilla",
          tags: ["new", "tag"],
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
          dateAdded: menuInfo.dateAdded.getTime(),
          title: BookmarksToolbarTitle,
          children: ["fxBmk_______", "tFolder_____", "bzBmk_______"],
        },
      },
    },
    "Should upload new local bookmarks and parents"
  );

  let localItemIds = await PlacesTestUtils.promiseManyItemIds([
    "fxBmk_______",
    "tFolder_____",
    "tbBmk_______",
    "bzBmk_______",
    "mozBmk______",
    PlacesUtils.bookmarks.toolbarGuid,
  ]);

  observer.check([
    {
      name: "bookmark-added",
      params: {
        itemId: localItemIds.get("fxBmk_______"),
        parentId: localItemIds.get(PlacesUtils.bookmarks.toolbarGuid),
        index: 0,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        urlHref: "http://getfirefox.com/",
        title: "Get Firefox",
        guid: "fxBmk_______",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        dateAdded: now,
        tags: "browsers,taggy",
        frecency: 1,
        hidden: false,
        visitCount: 0,
        lastVisitDate: null,
      },
    },
    {
      name: "bookmark-added",
      params: {
        itemId: localItemIds.get("tFolder_____"),
        parentId: localItemIds.get(PlacesUtils.bookmarks.toolbarGuid),
        index: 1,
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        urlHref: "",
        title: "Mail",
        guid: "tFolder_____",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        dateAdded: now,
        tags: "",
        frecency: 0,
        hidden: false,
        visitCount: 0,
        lastVisitDate: null,
      },
    },
    {
      name: "bookmark-added",
      params: {
        itemId: localItemIds.get("tbBmk_______"),
        parentId: localItemIds.get("tFolder_____"),
        index: 0,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        urlHref: "http://getthunderbird.com/",
        title: "Get Thunderbird",
        guid: "tbBmk_______",
        parentGuid: "tFolder_____",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        dateAdded: now,
        tags: "",
        frecency: 1,
        hidden: false,
        visitCount: 0,
        lastVisitDate: null,
      },
    },
    {
      name: "bookmark-moved",
      params: {
        itemId: localItemIds.get("bzBmk_______"),
        oldIndex: 0,
        newIndex: 2,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "bzBmk_______",
        oldParentGuid: PlacesUtils.bookmarks.toolbarGuid,
        newParentGuid: PlacesUtils.bookmarks.toolbarGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "https://bugzilla.mozilla.org/",
        isTagging: false,
        title: "Bugzilla",
        tags: "new,tag",
        frecency: 1,
        hidden: false,
        visitCount: 0,
        dateAdded: bzBmk.dateAdded.getTime(),
        lastVisitDate: null,
      },
    },
    {
      name: "bookmark-title-changed",
      params: {
        itemId: localItemIds.get("mozBmk______"),
        title: "Mozilla home page",
        guid: "mozBmk______",
        parentGuid: PlacesUtils.bookmarks.menuGuid,
      },
    },
  ]);

  let fxBmk = await PlacesUtils.bookmarks.fetch("fxBmk_______");
  ok(fxBmk, "New Firefox bookmark should exist");
  equal(
    fxBmk.parentGuid,
    PlacesUtils.bookmarks.toolbarGuid,
    "Should add Firefox bookmark to toolbar"
  );
  let fxTags = PlacesUtils.tagging.getTagsForURI(
    Services.io.newURI("http://getfirefox.com")
  );
  deepEqual(fxTags, ["browsers", "taggy"], "Should tag new Firefox bookmark");

  let folder = await PlacesUtils.bookmarks.fetch("tFolder_____");
  ok(folder, "New folder should exist");
  equal(
    folder.parentGuid,
    PlacesUtils.bookmarks.toolbarGuid,
    "Should add new folder to toolbar"
  );

  let tbBmk = await PlacesUtils.bookmarks.fetch("tbBmk_______");
  ok(tbBmk, "Should insert Thunderbird child bookmark");
  equal(
    tbBmk.parentGuid,
    folder.guid,
    "Should add Thunderbird bookmark to new folder"
  );
  let keywordInfo = await PlacesUtils.keywords.fetch("tb");
  equal(
    keywordInfo.url.href,
    "http://getthunderbird.com/",
    "Should set keyword for Thunderbird bookmark"
  );

  let updatedBmk = await PlacesUtils.bookmarks.fetch("mozBmk______");
  equal(
    updatedBmk.title,
    "Mozilla home page",
    "Should rename Mozilla bookmark"
  );
  equal(
    updatedBmk.parentGuid,
    PlacesUtils.bookmarks.menuGuid,
    "Should not move Mozilla bookmark"
  );

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_value_only_changes() {
  let buf = await openMirror("value_only_changes");

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "folderAAAAAA",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "A",
        children: [
          {
            guid: "bookmarkBBBB",
            url: "http://example.com/b",
            title: "B",
          },
          {
            guid: "bookmarkCCCC",
            url: "http://example.com/c",
            title: "C",
          },
          {
            guid: "folderJJJJJJ",
            type: PlacesUtils.bookmarks.TYPE_FOLDER,
            title: "J",
            children: [
              {
                guid: "bookmarkKKKK",
                url: "http://example.com/k",
                title: "K",
              },
            ],
          },
          {
            guid: "bookmarkDDDD",
            url: "http://example.com/d",
            title: "D",
          },
          {
            guid: "bookmarkEEEE",
            url: "http://example.com/e",
            title: "E",
          },
        ],
      },
      {
        guid: "folderFFFFFF",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "F",
        children: [
          {
            guid: "bookmarkGGGG",
            url: "http://example.com/g",
            title: "G",
          },
          {
            guid: "folderHHHHHH",
            type: PlacesUtils.bookmarks.TYPE_FOLDER,
            title: "H",
            children: [
              {
                guid: "bookmarkIIII",
                url: "http://example.com/i",
                title: "I",
              },
            ],
          },
        ],
      },
    ],
  });
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["folderAAAAAA", "folderFFFFFF"],
      },
      {
        id: "folderAAAAAA",
        parentid: "menu",
        type: "folder",
        title: "A",
        children: [
          "bookmarkBBBB",
          "bookmarkCCCC",
          "folderJJJJJJ",
          "bookmarkDDDD",
          "bookmarkEEEE",
        ],
      },
      {
        id: "bookmarkBBBB",
        parentid: "folderAAAAAA",
        type: "bookmark",
        title: "B",
        bmkUri: "http://example.com/b",
      },
      {
        id: "bookmarkCCCC",
        parentid: "folderAAAAAA",
        type: "bookmark",
        title: "C",
        bmkUri: "http://example.com/c",
      },
      {
        id: "folderJJJJJJ",
        parentid: "folderAAAAAA",
        type: "folder",
        title: "J",
        children: ["bookmarkKKKK"],
      },
      {
        id: "bookmarkKKKK",
        parentid: "folderJJJJJJ",
        type: "bookmark",
        title: "K",
        bmkUri: "http://example.com/k",
      },
      {
        id: "bookmarkDDDD",
        parentid: "folderAAAAAA",
        type: "bookmark",
        title: "D",
        bmkUri: "http://example.com/d",
      },
      {
        id: "bookmarkEEEE",
        parentid: "folderAAAAAA",
        type: "bookmark",
        title: "E",
        bmkUri: "http://example.com/e",
      },
      {
        id: "folderFFFFFF",
        parentid: "menu",
        type: "folder",
        title: "F",
        children: ["bookmarkGGGG", "folderHHHHHH"],
      },
      {
        id: "bookmarkGGGG",
        parentid: "folderFFFFFF",
        type: "bookmark",
        title: "G",
        bmkUri: "http://example.com/g",
      },
      {
        id: "folderHHHHHH",
        parentid: "folderFFFFFF",
        type: "folder",
        title: "H",
        children: ["bookmarkIIII"],
      },
      {
        id: "bookmarkIIII",
        parentid: "folderHHHHHH",
        type: "bookmark",
        title: "I",
        bmkUri: "http://example.com/i",
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make remote changes");
  await storeRecords(
    buf,
    shuffle([
      {
        id: "bookmarkCCCC",
        parentid: "folderAAAAAA",
        type: "bookmark",
        title: "C (remote)",
        bmkUri: "http://example.com/c-remote",
      },
      {
        id: "bookmarkEEEE",
        parentid: "folderAAAAAA",
        type: "bookmark",
        title: "E (remote)",
        bmkUri: "http://example.com/e-remote",
      },
      {
        id: "bookmarkIIII",
        parentid: "folderHHHHHH",
        type: "bookmark",
        title: "I (remote)",
        bmkUri: "http://example.com/i-remote",
      },
      {
        id: "folderFFFFFF",
        parentid: "menu",
        type: "folder",
        title: "F (remote)",
        children: ["bookmarkGGGG", "folderHHHHHH"],
      },
    ])
  );

  info("Apply remote");
  let changesToUpload = await buf.apply();

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: [],
      deleted: [],
    },
    "Should not upload records for remote-only value changes"
  );

  await assertLocalTree(
    PlacesUtils.bookmarks.rootGuid,
    {
      guid: PlacesUtils.bookmarks.rootGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: "",
      children: [
        {
          guid: PlacesUtils.bookmarks.menuGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 0,
          title: BookmarksMenuTitle,
          children: [
            {
              guid: "folderAAAAAA",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 0,
              title: "A",
              children: [
                {
                  guid: "bookmarkBBBB",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 0,
                  title: "B",
                  url: "http://example.com/b",
                },
                {
                  guid: "bookmarkCCCC",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 1,
                  title: "C (remote)",
                  url: "http://example.com/c-remote",
                },
                {
                  guid: "folderJJJJJJ",
                  type: PlacesUtils.bookmarks.TYPE_FOLDER,
                  index: 2,
                  title: "J",
                  children: [
                    {
                      guid: "bookmarkKKKK",
                      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                      index: 0,
                      title: "K",
                      url: "http://example.com/k",
                    },
                  ],
                },
                {
                  guid: "bookmarkDDDD",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 3,
                  title: "D",
                  url: "http://example.com/d",
                },
                {
                  guid: "bookmarkEEEE",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 4,
                  title: "E (remote)",
                  url: "http://example.com/e-remote",
                },
              ],
            },
            {
              guid: "folderFFFFFF",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 1,
              title: "F (remote)",
              children: [
                {
                  guid: "bookmarkGGGG",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 0,
                  title: "G",
                  url: "http://example.com/g",
                },
                {
                  guid: "folderHHHHHH",
                  type: PlacesUtils.bookmarks.TYPE_FOLDER,
                  index: 1,
                  title: "H",
                  children: [
                    {
                      guid: "bookmarkIIII",
                      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                      index: 0,
                      title: "I (remote)",
                      url: "http://example.com/i-remote",
                    },
                  ],
                },
              ],
            },
          ],
        },
        {
          guid: PlacesUtils.bookmarks.toolbarGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 1,
          title: BookmarksToolbarTitle,
        },
        {
          guid: PlacesUtils.bookmarks.unfiledGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 3,
          title: UnfiledBookmarksTitle,
        },
        {
          guid: PlacesUtils.bookmarks.mobileGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 4,
          title: MobileBookmarksTitle,
        },
      ],
    },
    "Should not change structure for value-only changes"
  );

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_conflicting_keywords() {
  let buf = await openMirror("conflicting_keywords");
  let dateAdded = new Date();

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "bookmarkAAAA",
        title: "A",
        url: "http://example.com/a",
        keyword: "one",
        dateAdded,
      },
    ],
  });
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["bookmarkAAAA"],
      },
      {
        id: "bookmarkAAAA",
        parentid: "menu",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
        keyword: "one",
        dateAdded: dateAdded.getTime(),
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  {
    let entryByKeyword = await PlacesUtils.keywords.fetch("one");
    equal(
      entryByKeyword.url.href,
      "http://example.com/a",
      "Should return new keyword entry by URL"
    );
    let entryByURL = await PlacesUtils.keywords.fetch({
      url: "http://example.com/a",
    });
    equal(entryByURL.keyword, "one", "Should return new entry by keyword");
  }

  info("Insert new bookmark with same URL and different keyword");
  {
    await storeRecords(
      buf,
      shuffle([
        {
          id: "toolbar",
          parentid: "places",
          type: "folder",
          children: ["bookmarkAAA1"],
        },
        {
          id: "bookmarkAAA1",
          parentid: "toolbar",
          type: "bookmark",
          title: "A1",
          bmkUri: "http://example.com/a",
          keyword: "two",
          dateAdded: dateAdded.getTime(),
        },
      ])
    );
    let changesToUpload = await buf.apply();
    deepEqual(
      await buf.fetchUnmergedGuids(),
      ["bookmarkAAA1"],
      "Should leave A1 with conflicting keyword unmerged"
    );
    deepEqual(
      changesToUpload,
      {
        bookmarkAAAA: {
          tombstone: false,
          counter: 1,
          synced: false,
          cleartext: {
            id: "bookmarkAAAA",
            type: "bookmark",
            parentid: "menu",
            hasDupe: true,
            parentName: BookmarksMenuTitle,
            dateAdded: dateAdded.getTime(),
            bmkUri: "http://example.com/a",
            title: "A",
            keyword: "two",
          },
        },
        bookmarkAAA1: {
          tombstone: false,
          counter: 1,
          synced: false,
          cleartext: {
            id: "bookmarkAAA1",
            type: "bookmark",
            parentid: "toolbar",
            hasDupe: true,
            parentName: BookmarksToolbarTitle,
            dateAdded: dateAdded.getTime(),
            bmkUri: "http://example.com/a",
            title: "A1",
            keyword: "two",
          },
        },
      },
      "Should reupload bookmarks with different keyword"
    );
    await storeChangesInMirror(buf, changesToUpload);

    let entryByOldKeyword = await PlacesUtils.keywords.fetch("one");
    ok(
      !entryByOldKeyword,
      "Should remove old entry when inserting bookmark with different keyword"
    );
    let entryByNewKeyword = await PlacesUtils.keywords.fetch("two");
    equal(
      entryByNewKeyword.url.href,
      "http://example.com/a",
      "Should return new keyword entry by URL"
    );
    let entryByURL = await PlacesUtils.keywords.fetch({
      url: "http://example.com/a",
    });
    equal(entryByURL.keyword, "two", "Should return new entry by URL");
  }

  info("Update bookmark with different keyword");
  {
    await storeRecords(
      buf,
      shuffle([
        {
          id: "bookmarkAAAA",
          parentid: "menu",
          type: "bookmark",
          title: "A",
          bmkUri: "http://example.com/a",
          keyword: "three",
          dateAdded: dateAdded.getTime(),
        },
      ])
    );
    let changesToUpload = await buf.apply();
    deepEqual(
      await buf.fetchUnmergedGuids(),
      ["bookmarkAAAA"],
      "Should leave A with conflicting keyword unmerged"
    );
    deepEqual(
      changesToUpload,
      {
        bookmarkAAAA: {
          tombstone: false,
          counter: 1,
          synced: false,
          cleartext: {
            id: "bookmarkAAAA",
            type: "bookmark",
            parentid: "menu",
            hasDupe: true,
            parentName: BookmarksMenuTitle,
            dateAdded: dateAdded.getTime(),
            bmkUri: "http://example.com/a",
            title: "A",
            keyword: "three",
          },
        },
        bookmarkAAA1: {
          tombstone: false,
          counter: 1,
          synced: false,
          cleartext: {
            id: "bookmarkAAA1",
            type: "bookmark",
            parentid: "toolbar",
            hasDupe: true,
            parentName: BookmarksToolbarTitle,
            dateAdded: dateAdded.getTime(),
            bmkUri: "http://example.com/a",
            title: "A1",
            keyword: "three",
          },
        },
      },
      "Should reupload A and A1 with updated keyword"
    );
    await storeChangesInMirror(buf, changesToUpload);

    let entryByOldKeyword = await PlacesUtils.keywords.fetch("two");
    ok(
      !entryByOldKeyword,
      "Should remove old entry when updating bookmark keyword"
    );
    let entryByNewKeyword = await PlacesUtils.keywords.fetch("three");
    equal(
      entryByNewKeyword.url.href,
      "http://example.com/a",
      "Should return updated keyword entry by URL"
    );
    let entryByURL = await PlacesUtils.keywords.fetch({
      url: "http://example.com/a",
    });
    equal(entryByURL.keyword, "three", "Should return updated entry by URL");
  }

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_keywords() {
  let buf = await openMirror("keywords");
  let now = new Date();

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "bookmarkAAAA",
        title: "A",
        url: "http://example.com/a",
        keyword: "one",
        dateAdded: now,
      },
      {
        guid: "bookmarkBBBB",
        title: "B",
        url: "http://example.com/b",
        keyword: "two",
        dateAdded: now,
      },
      {
        guid: "bookmarkCCCC",
        title: "C",
        url: "http://example.com/c",
        dateAdded: now,
      },
      {
        guid: "bookmarkDDDD",
        title: "D",
        url: "http://example.com/d",
        keyword: "three",
        dateAdded: now,
      },
    ],
  });
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: [
          "bookmarkAAAA",
          "bookmarkBBBB",
          "bookmarkCCCC",
          "bookmarkDDDD",
        ],
      },
      {
        id: "bookmarkAAAA",
        parentid: "menu",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
        keyword: "one",
        dateAdded: now.getTime(),
      },
      {
        id: "bookmarkBBBB",
        parentid: "menu",
        type: "bookmark",
        title: "B",
        bmkUri: "http://example.com/b",
        keyword: "two",
        dateAdded: now.getTime(),
      },
      {
        id: "bookmarkCCCC",
        parentid: "menu",
        type: "bookmark",
        title: "C",
        bmkUri: "http://example.com/c",
        dateAdded: now.getTime(),
      },
      {
        id: "bookmarkDDDD",
        parentid: "menu",
        type: "bookmark",
        title: "D",
        bmkUri: "http://example.com/d",
        keyword: "three",
        dateAdded: now.getTime(),
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Change keywords remotely");
  await storeRecords(
    buf,
    shuffle([
      {
        id: "bookmarkAAAA",
        parentid: "menu",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
        keyword: "two",
        dateAdded: now.getTime(),
      },
      {
        id: "bookmarkBBBB",
        parentid: "menu",
        type: "bookmark",
        title: "B",
        bmkUri: "http://example.com/b",
        dateAdded: now.getTime(),
      },
    ])
  );

  info("Change keywords locally");
  await PlacesUtils.keywords.insert({
    keyword: "four",
    url: "http://example.com/c",
  });
  await PlacesUtils.keywords.remove("three");

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  deepEqual(
    changesToUpload,
    {
      bookmarkCCCC: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "bookmarkCCCC",
          type: "bookmark",
          parentid: "menu",
          hasDupe: true,
          parentName: BookmarksMenuTitle,
          dateAdded: now.getTime(),
          bmkUri: "http://example.com/c",
          title: "C",
          keyword: "four",
        },
      },
      bookmarkDDDD: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "bookmarkDDDD",
          type: "bookmark",
          parentid: "menu",
          hasDupe: true,
          parentName: BookmarksMenuTitle,
          dateAdded: now.getTime(),
          bmkUri: "http://example.com/d",
          title: "D",
        },
      },
    },
    "Should upload C with new keyword, D with keyword removed"
  );

  let entryForOne = await PlacesUtils.keywords.fetch("one");
  ok(!entryForOne, "Should remove existing keyword from A");

  let entriesForTwo = await fetchAllKeywords("two");
  deepEqual(
    entriesForTwo.map(entry => ({
      keyword: entry.keyword,
      url: entry.url.href,
    })),
    [
      {
        keyword: "two",
        url: "http://example.com/a",
      },
    ],
    "Should move keyword for B to A"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_keywords_complex() {
  let buf = await openMirror("keywords_complex");
  let now = new Date();

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "bookmarkBBBB",
        title: "B",
        url: "http://example.com/b",
        keyword: "four",
        dateAdded: now,
      },
      {
        guid: "bookmarkCCCC",
        title: "C",
        url: "http://example.com/c",
        keyword: "five",
        dateAdded: now,
      },
      {
        guid: "bookmarkDDDD",
        title: "D",
        url: "http://example.com/d",
        dateAdded: now,
      },
      {
        guid: "bookmarkEEEE",
        title: "E",
        url: "http://example.com/e",
        keyword: "three",
        dateAdded: now,
      },
    ],
  });
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: [
          "bookmarkBBBB",
          "bookmarkCCCC",
          "bookmarkDDDD",
          "bookmarkEEEE",
        ],
      },
      {
        id: "bookmarkBBBB",
        parentid: "menu",
        type: "bookmark",
        title: "B",
        bmkUri: "http://example.com/b",
        keyword: "four",
        dateAdded: now.getTime(),
      },
      {
        id: "bookmarkCCCC",
        parentid: "menu",
        type: "bookmark",
        title: "C",
        bmkUri: "http://example.com/c",
        keyword: "five",
        dateAdded: now.getTime(),
      },
      {
        id: "bookmarkDDDD",
        parentid: "menu",
        type: "bookmark",
        title: "D",
        bmkUri: "http://example.com/d",
        dateAdded: now.getTime(),
      },
      {
        id: "bookmarkEEEE",
        parentid: "menu",
        type: "bookmark",
        title: "E",
        bmkUri: "http://example.com/e",
        keyword: "three",
        dateAdded: now.getTime(),
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make remote changes");
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: [
          "bookmarkAAAA",
          "bookmarkAAA1",
          "bookmarkBBB1",
          "bookmarkBBBB",
          "bookmarkCCCC",
          "bookmarkDDDD",
          "bookmarkEEEE",
        ],
      },
      {
        id: "bookmarkAAAA",
        parentid: "menu",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
        keyword: "one",
        dateAdded: now.getTime(),
      },
      {
        id: "bookmarkAAA1",
        parentid: "menu",
        type: "bookmark",
        title: "A (copy)",
        bmkUri: "http://example.com/a",
        keyword: "two",
        dateAdded: now.getTime(),
      },
      {
        id: "bookmarkBBB1",
        parentid: "menu",
        type: "bookmark",
        title: "B",
        bmkUri: "http://example.com/b",
        dateAdded: now.getTime(),
      },
      {
        id: "bookmarkCCCC",
        parentid: "menu",
        type: "bookmark",
        title: "C (remote)",
        bmkUri: "http://example.com/c-remote",
        keyword: "six",
        dateAdded: now.getTime(),
      },
    ])
  );

  info("Apply remote");
  let observer = expectBookmarkChangeNotifications();
  let changesToUpload = await buf.apply({
    notifyInStableOrder: true,
  });
  deepEqual(
    await buf.fetchUnmergedGuids(),
    ["bookmarkAAA1", "bookmarkAAAA", "bookmarkBBB1"],
    "Should leave A1, A, B with conflicting keywords unmerged"
  );

  let expectedChangesToUpload = {
    bookmarkBBBB: {
      tombstone: false,
      counter: 1,
      synced: false,
      cleartext: {
        id: "bookmarkBBBB",
        type: "bookmark",
        parentid: "menu",
        hasDupe: true,
        parentName: BookmarksMenuTitle,
        dateAdded: now.getTime(),
        bmkUri: "http://example.com/b",
        title: "B",
      },
    },
    bookmarkBBB1: {
      tombstone: false,
      counter: 1,
      synced: false,
      cleartext: {
        id: "bookmarkBBB1",
        type: "bookmark",
        parentid: "menu",
        hasDupe: true,
        parentName: BookmarksMenuTitle,
        dateAdded: now.getTime(),
        bmkUri: "http://example.com/b",
        title: "B",
      },
    },
    bookmarkAAAA: {
      tombstone: false,
      counter: 1,
      synced: false,
      cleartext: {
        id: "bookmarkAAAA",
        type: "bookmark",
        parentid: "menu",
        hasDupe: true,
        parentName: BookmarksMenuTitle,
        dateAdded: now.getTime(),
        bmkUri: "http://example.com/a",
        title: "A",
      },
    },
    bookmarkAAA1: {
      tombstone: false,
      counter: 1,
      synced: false,
      cleartext: {
        id: "bookmarkAAA1",
        type: "bookmark",
        parentid: "menu",
        hasDupe: true,
        parentName: BookmarksMenuTitle,
        dateAdded: now.getTime(),
        bmkUri: "http://example.com/a",
        title: "A (copy)",
      },
    },
  };

  // We'll take the keyword of either "bookmarkAAAA" or "bookmarkAAA1",
  // depending on which we see first, and reupload the other.
  let entriesForOne = await fetchAllKeywords("one");
  let entriesForTwo = await fetchAllKeywords("two");
  if (entriesForOne.length) {
    ok(!entriesForTwo.length, "Should drop conflicting keyword from A1");
    deepEqual(
      entriesForOne.map(keyword => keyword.url.href),
      ["http://example.com/a"],
      "Should use A keyword for A and A1"
    );
    expectedChangesToUpload.bookmarkAAAA.cleartext.keyword = "one";
    expectedChangesToUpload.bookmarkAAA1.cleartext.keyword = "one";
  } else {
    ok(!entriesForOne.length, "Should drop conflicting keyword from A");
    deepEqual(
      entriesForTwo.map(keyword => keyword.url.href),
      ["http://example.com/a"],
      "Should use A1 keyword for A and A1"
    );
    expectedChangesToUpload.bookmarkAAAA.cleartext.keyword = "two";
    expectedChangesToUpload.bookmarkAAA1.cleartext.keyword = "two";
  }
  deepEqual(
    changesToUpload,
    expectedChangesToUpload,
    "Should reupload all local records with corrected keywords"
  );

  let localItemIds = await PlacesTestUtils.promiseManyItemIds([
    "bookmarkAAAA",
    "bookmarkAAA1",
    "bookmarkBBB1",
    "bookmarkBBBB",
    "bookmarkCCCC",
    "bookmarkDDDD",
    "bookmarkEEEE",
    PlacesUtils.bookmarks.menuGuid,
  ]);
  let expectedNotifications = [
    {
      name: "bookmark-added",
      params: {
        itemId: localItemIds.get("bookmarkAAAA"),
        parentId: localItemIds.get(PlacesUtils.bookmarks.menuGuid),
        index: 0,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        urlHref: "http://example.com/a",
        title: "A",
        guid: "bookmarkAAAA",
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        tags: "",
        frecency: 1,
        hidden: false,
        visitCount: 0,
      },
    },
    {
      name: "bookmark-added",
      params: {
        itemId: localItemIds.get("bookmarkAAA1"),
        parentId: localItemIds.get(PlacesUtils.bookmarks.menuGuid),
        index: 1,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        urlHref: "http://example.com/a",
        title: "A (copy)",
        guid: "bookmarkAAA1",
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        tags: "",
        frecency: 1,
        hidden: false,
        visitCount: 0,
      },
    },
    {
      name: "bookmark-added",
      params: {
        itemId: localItemIds.get("bookmarkBBB1"),
        parentId: localItemIds.get(PlacesUtils.bookmarks.menuGuid),
        index: 2,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        urlHref: "http://example.com/b",
        title: "B",
        guid: "bookmarkBBB1",
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        tags: "",
        frecency: 1,
        hidden: false,
        visitCount: 0,
      },
    },
    {
      // These `bookmark-moved` notifications aren't necessary: we only moved
      // (B C D E) to accomodate (A A1 B1), and Places doesn't usually fire move
      // notifications for repositioned siblings. However, detecting and filtering
      // these out complicates `noteObserverChanges`, so, for simplicity, we
      // record and fire the extra notifications.
      name: "bookmark-moved",
      params: {
        itemId: localItemIds.get("bookmarkBBBB"),
        oldIndex: 0,
        newIndex: 3,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "bookmarkBBBB",
        oldParentGuid: PlacesUtils.bookmarks.menuGuid,
        newParentGuid: PlacesUtils.bookmarks.menuGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "http://example.com/b",
        isTagging: false,
        title: "B",
        tags: "",
        frecency: 1,
        hidden: false,
        visitCount: 0,
        dateAdded: now.getTime(),
        lastVisitDate: null,
      },
    },
    {
      name: "bookmark-moved",
      params: {
        itemId: localItemIds.get("bookmarkCCCC"),
        oldIndex: 1,
        newIndex: 4,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "bookmarkCCCC",
        oldParentGuid: PlacesUtils.bookmarks.menuGuid,
        newParentGuid: PlacesUtils.bookmarks.menuGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "http://example.com/c-remote",
        isTagging: false,
        title: "C (remote)",
        tags: "",
        frecency: -1,
        hidden: false,
        visitCount: 0,
        dateAdded: now.getTime(),
        lastVisitDate: null,
      },
    },
    {
      name: "bookmark-moved",
      params: {
        itemId: localItemIds.get("bookmarkDDDD"),
        oldIndex: 2,
        newIndex: 5,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "bookmarkDDDD",
        oldParentGuid: PlacesUtils.bookmarks.menuGuid,
        newParentGuid: PlacesUtils.bookmarks.menuGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "http://example.com/d",
        isTagging: false,
        title: "D",
        tags: "",
        frecency: 1,
        hidden: false,
        visitCount: 0,
        dateAdded: now.getTime(),
        lastVisitDate: null,
      },
    },
    {
      name: "bookmark-moved",
      params: {
        itemId: localItemIds.get("bookmarkEEEE"),
        oldIndex: 3,
        newIndex: 6,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "bookmarkEEEE",
        oldParentGuid: PlacesUtils.bookmarks.menuGuid,
        newParentGuid: PlacesUtils.bookmarks.menuGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "http://example.com/e",
        isTagging: false,
        title: "E",
        tags: "",
        frecency: 1,
        hidden: false,
        visitCount: 0,
        dateAdded: now.getTime(),
        lastVisitDate: null,
      },
    },
    {
      name: "bookmark-title-changed",
      params: {
        itemId: localItemIds.get("bookmarkCCCC"),
        title: "C (remote)",
        guid: "bookmarkCCCC",
        parentGuid: PlacesUtils.bookmarks.menuGuid,
      },
    },
    {
      name: "bookmark-url-changed",
      params: {
        itemId: localItemIds.get("bookmarkCCCC"),
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        urlHref: "http://example.com/c-remote",
        guid: "bookmarkCCCC",
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        isTagging: false,
      },
    },
  ];
  observer.check(expectedNotifications);

  let entriesForFour = await fetchAllKeywords("four");
  ok(!entriesForFour.length, "Should remove all keywords for B");

  let entriesForOldC = await fetchAllKeywords({
    url: "http://example.com/c",
  });
  ok(!entriesForOldC.length, "Should remove all keywords from old C URL");
  let entriesForNewC = await fetchAllKeywords({
    url: "http://example.com/c-remote",
  });
  deepEqual(
    entriesForNewC.map(entry => entry.keyword),
    ["six"],
    "Should add new keyword to new C URL"
  );

  let entriesForD = await fetchAllKeywords("http://example.com/d");
  ok(!entriesForD.length, "Should not add keywords to D");

  let entriesForThree = await fetchAllKeywords("three");
  deepEqual(
    entriesForThree.map(keyword => keyword.url.href),
    ["http://example.com/e"],
    "Should not change keywords for E"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_tags_complex() {
  let buf = await openMirror("tags_complex");

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "bookmarkAAA1",
        title: "A1",
        url: "http://example.com/a",
        tags: ["one", "two"],
      },
      {
        guid: "bookmarkAAA2",
        title: "A2",
        url: "http://example.com/a",
        tags: ["one", "two"],
      },
      {
        guid: "bookmarkBBB1",
        title: "B1",
        url: "http://example.com/b",
        tags: ["one"],
      },
      {
        guid: "bookmarkBBB2",
        title: "B2",
        url: "http://example.com/b",
        tags: ["one"],
      },
      {
        guid: "bookmarkCCC1",
        title: "C1",
        url: "http://example.com/c",
        tags: ["two", "three"],
      },
      {
        guid: "bookmarkCCC2",
        title: "C2",
        url: "http://example.com/c",
        tags: ["two", "three"],
      },
    ],
  });
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: [
          "bookmarkAAA1",
          "bookmarkAAA2",
          "bookmarkBBB1",
          "bookmarkBBB2",
          "bookmarkCCC1",
          "bookmarkCCC2",
        ],
      },
      {
        id: "bookmarkAAA1",
        parentid: "menu",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
        tags: ["one", "two"],
      },
      {
        id: "bookmarkAAA2",
        parentid: "menu",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
        tags: ["one", "two"],
      },
      {
        id: "bookmarkBBB1",
        parentid: "menu",
        type: "bookmark",
        title: "B1",
        bmkUri: "http://example.com/b",
        tags: ["one"],
      },
      {
        id: "bookmarkBBB2",
        parentid: "menu",
        type: "bookmark",
        title: "B2",
        bmkUri: "http://example.com/b",
        tags: ["one"],
      },
      {
        id: "bookmarkCCC1",
        parentid: "menu",
        type: "bookmark",
        title: "C1",
        bmkUri: "http://example.com/c",
        tags: ["two", "three"],
      },
      {
        id: "bookmarkCCC2",
        parentid: "menu",
        type: "bookmark",
        title: "C2",
        bmkUri: "http://example.com/c",
        tags: ["two", "three"],
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Add tags for B locally");
  PlacesUtils.tagging.tagURI(Services.io.newURI("http://example.com/b"), [
    "four",
    "five",
  ]);

  info("Remove tag from C locally");
  PlacesUtils.tagging.untagURI(Services.io.newURI("http://example.com/c"), [
    "two",
  ]);

  info("Update tags for A remotely");
  await storeRecords(
    buf,
    shuffle([
      {
        id: "bookmarkAAA1",
        parentid: "menu",
        type: "bookmark",
        title: "A1",
        bmkUri: "http://example.com/a",
        tags: ["one", "two", "four", "six"],
      },
      {
        id: "bookmarkAAA2",
        parentid: "menu",
        type: "bookmark",
        title: "A2",
        bmkUri: "http://example.com/a",
        tags: ["one", "two", "four", "six"],
      },
    ])
  );

  info("Apply remote");
  let changesToUpload = await buf.apply();

  let datesAdded = await promiseManyDatesAdded([
    "bookmarkBBB1",
    "bookmarkBBB2",
    "bookmarkCCC1",
    "bookmarkCCC2",
  ]);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");
  deepEqual(
    changesToUpload,
    {
      bookmarkBBB1: {
        counter: 2,
        synced: false,
        tombstone: false,
        cleartext: {
          id: "bookmarkBBB1",
          type: "bookmark",
          parentid: "menu",
          hasDupe: true,
          parentName: BookmarksMenuTitle,
          dateAdded: datesAdded.get("bookmarkBBB1"),
          bmkUri: "http://example.com/b",
          title: "B1",
          tags: ["five", "four", "one"],
        },
      },
      bookmarkBBB2: {
        counter: 2,
        synced: false,
        tombstone: false,
        cleartext: {
          id: "bookmarkBBB2",
          type: "bookmark",
          parentid: "menu",
          hasDupe: true,
          parentName: BookmarksMenuTitle,
          dateAdded: datesAdded.get("bookmarkBBB2"),
          bmkUri: "http://example.com/b",
          title: "B2",
          tags: ["five", "four", "one"],
        },
      },
      bookmarkCCC1: {
        counter: 1,
        synced: false,
        tombstone: false,
        cleartext: {
          id: "bookmarkCCC1",
          type: "bookmark",
          parentid: "menu",
          hasDupe: true,
          parentName: BookmarksMenuTitle,
          dateAdded: datesAdded.get("bookmarkCCC1"),
          bmkUri: "http://example.com/c",
          title: "C1",
          tags: ["three"],
        },
      },
      bookmarkCCC2: {
        counter: 1,
        synced: false,
        tombstone: false,
        cleartext: {
          id: "bookmarkCCC2",
          type: "bookmark",
          parentid: "menu",
          hasDupe: true,
          parentName: BookmarksMenuTitle,
          dateAdded: datesAdded.get("bookmarkCCC2"),
          bmkUri: "http://example.com/c",
          title: "C2",
          tags: ["three"],
        },
      },
    },
    "Should upload local records with new tags"
  );

  await assertLocalTree(
    PlacesUtils.bookmarks.menuGuid,
    {
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
      children: [
        {
          guid: "bookmarkAAA1",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 0,
          title: "A1",
          url: "http://example.com/a",
          tags: ["four", "one", "six", "two"],
        },
        {
          guid: "bookmarkAAA2",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 1,
          title: "A2",
          url: "http://example.com/a",
          tags: ["four", "one", "six", "two"],
        },
        {
          guid: "bookmarkBBB1",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 2,
          title: "B1",
          url: "http://example.com/b",
          tags: ["five", "four", "one"],
        },
        {
          guid: "bookmarkBBB2",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 3,
          title: "B2",
          url: "http://example.com/b",
          tags: ["five", "four", "one"],
        },
        {
          guid: "bookmarkCCC1",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 4,
          title: "C1",
          url: "http://example.com/c",
          tags: ["three"],
        },
        {
          guid: "bookmarkCCC2",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 5,
          title: "C2",
          url: "http://example.com/c",
          tags: ["three"],
        },
      ],
    },
    "Should update local items with new tags"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_tags() {
  let buf = await openMirror("tags");

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "bookmarkAAAA",
        title: "A",
        url: "http://example.com/a",
        tags: ["one", "two", "three", "four"],
      },
      {
        guid: "bookmarkBBBB",
        title: "B",
        url: "http://example.com/b",
        tags: ["five", "six"],
      },
      {
        guid: "bookmarkCCCC",
        title: "C",
        url: "http://example.com/c",
      },
      {
        guid: "bookmarkDDDD",
        title: "D",
        url: "http://example.com/d",
        tags: ["seven", "eight", "nine"],
      },
    ],
  });
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: [
          "bookmarkAAAA",
          "bookmarkBBBB",
          "bookmarkCCCC",
          "bookmarkDDDD",
        ],
      },
      {
        id: "bookmarkAAAA",
        parentid: "menu",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
        tags: ["one", "two", "three", "four"],
      },
      {
        id: "bookmarkBBBB",
        parentid: "menu",
        type: "bookmark",
        title: "B",
        bmkUri: "http://example.com/b",
        tags: ["five", "six"],
      },
      {
        id: "bookmarkCCCC",
        parentid: "menu",
        type: "bookmark",
        title: "C",
        bmkUri: "http://example.com/c",
      },
      {
        id: "bookmarkDDDD",
        parentid: "menu",
        type: "bookmark",
        title: "D",
        bmkUri: "http://example.com/d",
        tags: ["seven", "eight", "nine"],
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Change tags remotely");
  await storeRecords(
    buf,
    shuffle([
      {
        id: "bookmarkAAAA",
        parentid: "menu",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
        tags: ["one", "two", "ten"],
      },
      {
        id: "bookmarkBBBB",
        parentid: "menu",
        type: "bookmark",
        title: "B",
        bmkUri: "http://example.com/b",
        tags: [],
      },
    ])
  );

  info("Change tags locally");
  PlacesUtils.tagging.tagURI(Services.io.newURI("http://example.com/c"), [
    "eleven",
    "twelve",
  ]);

  let wait = PlacesTestUtils.waitForNotification("bookmark-removed", events =>
    events.some(event => event.parentGuid == PlacesUtils.bookmarks.tagsGuid)
  );

  PlacesUtils.tagging.untagURI(
    Services.io.newURI("http://example.com/d"),
    null
  );

  await wait;

  await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));
  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: ["bookmarkCCCC", "bookmarkDDDD"],
      deleted: [],
    },
    "Should upload local records with new tags"
  );

  deepEqual(
    changesToUpload.bookmarkCCCC.cleartext.tags.sort(),
    ["eleven", "twelve"],
    "Should upload record with new tags for C"
  );
  ok(
    !changesToUpload.bookmarkDDDD.cleartext.tags,
    "Should upload record for D with tags removed"
  );

  let tagsForA = PlacesUtils.tagging.getTagsForURI(
    Services.io.newURI("http://example.com/a")
  );
  deepEqual(tagsForA, ["one", "ten", "two"], "Should change tags for A");

  let tagsForB = PlacesUtils.tagging.getTagsForURI(
    Services.io.newURI("http://example.com/b")
  );
  deepEqual(tagsForB, [], "Should remove all tags from B");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_rewrite_tag_queries() {
  let buf = await openMirror("rewrite_tag_queries");

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "bookmarkAAAA",
        title: "A",
        url: "http://example.com/a",
      },
      {
        guid: "bookmarkDDDD",
        title: "D",
        url: "http://example.com/d",
        tags: ["kitty"],
      },
    ],
  });
  await storeRecords(
    buf,
    [
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["bookmarkAAAA", "bookmarkDDDD"],
      },
      {
        id: "bookmarkAAAA",
        parentid: "menu",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
      },
      {
        id: "bookmarkDDDD",
        parentid: "menu",
        type: "bookmark",
        title: "D",
        bmkUri: "http://example.com/d",
        tags: ["kitty"],
      },
    ],
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Add tag queries for new and existing tags");
  await storeRecords(buf, [
    {
      id: "toolbar",
      parentid: "places",
      type: "folder",
      children: ["queryBBBBBBB", "queryCCCCCCC", "bookmarkEEEE"],
    },
    {
      id: "queryBBBBBBB",
      parentid: "toolbar",
      type: "query",
      title: "Tagged stuff",
      bmkUri: "place:type=7&folder=999",
      folderName: "taggy",
    },
    {
      id: "queryCCCCCCC",
      parentid: "toolbar",
      type: "query",
      title: "Cats",
      bmkUri: "place:type=7&folder=888",
      folderName: "kitty",
    },
    {
      id: "bookmarkEEEE",
      parentid: "toolbar",
      type: "bookmark",
      title: "E",
      bmkUri: "http://example.com/e",
      tags: ["taggy"],
    },
  ]);

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(
    await buf.fetchUnmergedGuids(),
    ["queryBBBBBBB", "queryCCCCCCC"],
    "Should leave rewritten queries unmerged"
  );

  deepEqual(
    changesToUpload,
    {
      queryBBBBBBB: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "queryBBBBBBB",
          type: "query",
          parentid: "toolbar",
          hasDupe: true,
          parentName: BookmarksToolbarTitle,
          dateAdded: undefined,
          bmkUri: "place:tag=taggy",
          title: "Tagged stuff",
          folderName: "taggy",
        },
      },
      queryCCCCCCC: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "queryCCCCCCC",
          type: "query",
          parentid: "toolbar",
          hasDupe: true,
          parentName: BookmarksToolbarTitle,
          dateAdded: undefined,
          bmkUri: "place:tag=kitty",
          title: "Cats",
          folderName: "kitty",
        },
      },
    },
    "Should reupload (E C) with rewritten URLs"
  );

  let bmWithTaggy = await PlacesUtils.bookmarks.fetch({ tags: ["taggy"] });
  equal(
    bmWithTaggy.url.href,
    "http://example.com/e",
    "Should insert bookmark with new tag"
  );

  let bmWithKitty = await PlacesUtils.bookmarks.fetch({ tags: ["kitty"] });
  equal(
    bmWithKitty.url.href,
    "http://example.com/d",
    "Should retain existing tag"
  );

  let { root: toolbarContainer } = PlacesUtils.getFolderContents(
    PlacesUtils.bookmarks.toolbarGuid,
    false,
    true
  );
  equal(
    toolbarContainer.childCount,
    3,
    "Should add queries and bookmark to toolbar"
  );

  let containerForB = PlacesUtils.asContainer(toolbarContainer.getChild(0));
  containerForB.containerOpen = true;
  for (let i = 0; i < containerForB.childCount; ++i) {
    let child = containerForB.getChild(i);
    equal(
      child.uri,
      "http://example.com/e",
      `Rewritten tag query B should have tagged child node at ${i}`
    );
  }
  containerForB.containerOpen = false;

  let containerForC = PlacesUtils.asContainer(toolbarContainer.getChild(1));
  containerForC.containerOpen = true;
  for (let i = 0; i < containerForC.childCount; ++i) {
    let child = containerForC.getChild(i);
    equal(
      child.uri,
      "http://example.com/d",
      `Rewritten tag query C should have tagged child node at ${i}`
    );
  }
  containerForC.containerOpen = false;

  toolbarContainer.containerOpen = false;

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_date_added() {
  let buf = await openMirror("date_added");

  let aDateAdded = new Date(Date.now() - 1 * 24 * 60 * 60 * 1000);
  let bDateAdded = new Date();

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "bookmarkAAAA",
        dateAdded: aDateAdded,
        title: "A",
        url: "http://example.com/a",
      },
      {
        guid: "bookmarkBBBB",
        dateAdded: bDateAdded,
        title: "B",
        url: "http://example.com/b",
      },
    ],
  });
  await storeRecords(
    buf,
    [
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["bookmarkAAAA", "bookmarkBBBB"],
      },
      {
        id: "bookmarkAAAA",
        parentid: "menu",
        type: "bookmark",
        title: "A",
        dateAdded: aDateAdded.getTime(),
        bmkUri: "http://example.com/a",
      },
      {
        id: "bookmarkBBBB",
        parentid: "menu",
        type: "bookmark",
        title: "B",
        dateAdded: bDateAdded.getTime(),
        bmkUri: "http://example.com/b",
      },
    ],
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make remote changes");
  let bNewDateAdded = new Date(bDateAdded.getTime() - 1 * 60 * 60 * 1000);
  await storeRecords(buf, [
    {
      id: "bookmarkAAAA",
      parentid: "menu",
      type: "bookmark",
      title: "A (remote)",
      dateAdded: Date.now(),
      bmkUri: "http://example.com/a",
    },
    {
      id: "bookmarkBBBB",
      parentid: "menu",
      type: "bookmark",
      title: "B (remote)",
      dateAdded: bNewDateAdded.getTime(),
      bmkUri: "http://example.com/b",
    },
  ]);

  info("Apply remote");
  let observer = expectBookmarkChangeNotifications();
  let changesToUpload = await buf.apply({
    notifyInStableOrder: true,
  });
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: ["bookmarkAAAA"],
      deleted: [],
    },
    "Should flag A for weak reupload"
  );

  let localItemIds = await PlacesTestUtils.promiseManyItemIds([
    "bookmarkAAAA",
    "bookmarkBBBB",
  ]);
  observer.check([
    {
      name: "bookmark-title-changed",
      params: {
        itemId: localItemIds.get("bookmarkAAAA"),
        title: "A (remote)",
        guid: "bookmarkAAAA",
        parentGuid: PlacesUtils.bookmarks.menuGuid,
      },
    },
    {
      name: "bookmark-title-changed",
      params: {
        itemId: localItemIds.get("bookmarkBBBB"),
        title: "B (remote)",
        guid: "bookmarkBBBB",
        parentGuid: PlacesUtils.bookmarks.menuGuid,
      },
    },
  ]);

  let changeCounter = changesToUpload.bookmarkAAAA.counter;
  strictEqual(changeCounter, 0, "Should not bump change counter for A");

  let aInfo = await PlacesUtils.bookmarks.fetch("bookmarkAAAA");
  equal(aInfo.title, "A (remote)", "Should change local title for A");
  deepEqual(
    aInfo.dateAdded,
    aDateAdded,
    "Should not change date added for A to newer remote date"
  );

  let bInfo = await PlacesUtils.bookmarks.fetch("bookmarkBBBB");
  equal(bInfo.title, "B (remote)", "Should change local title for B");
  deepEqual(
    bInfo.dateAdded,
    bNewDateAdded,
    "Should take older date added for B"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

// Bug 1472435.
add_task(async function test_duplicate_url_rows() {
  let buf = await openMirror("test_duplicate_url_rows");

  let placesToInsert = [
    {
      guid: "placeAAAAAAA",
      href: "http://example.com",
    },
    {
      guid: "placeBBBBBBB",
      href: "http://example.com",
    },
    {
      guid: "placeCCCCCCC",
      href: "http://example.com/c",
    },
  ];

  let itemsToInsert = [
    {
      guid: "bookmarkAAAA",
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      placeGuid: "placeAAAAAAA",
      localTitle: "A",
      remoteTitle: "A (remote)",
    },
    {
      guid: "bookmarkBBBB",
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      placeGuid: "placeBBBBBBB",
      localTitle: "B",
      remoteTitle: "B (remote)",
    },
    {
      guid: "bookmarkCCCC",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      placeGuid: "placeCCCCCCC",
      localTitle: "C",
      remoteTitle: "C (remote)",
    },
  ];

  info("Manually insert local and remote items with duplicate URLs");
  await buf.db.executeTransaction(async function () {
    for (let { guid, href } of placesToInsert) {
      let url = new URL(href);
      await buf.db.executeCached(
        `
        INSERT INTO moz_places(url, url_hash, rev_host, hidden, frecency, guid)
        VALUES(:url, hash(:url), :revHost, 0, -1, :guid)`,
        { url: url.href, revHost: PlacesUtils.getReversedHost(url), guid }
      );

      await buf.db.executeCached(
        `
        INSERT INTO urls(guid, url, hash, revHost)
        VALUES(:guid, :url, hash(:url), :revHost)`,
        { guid, url: url.href, revHost: PlacesUtils.getReversedHost(url) }
      );
    }

    for (let {
      guid,
      parentGuid,
      placeGuid,
      localTitle,
      remoteTitle,
    } of itemsToInsert) {
      await buf.db.executeCached(
        `
        INSERT INTO moz_bookmarks(guid, parent, fk, position, type, title,
                                  syncStatus, syncChangeCounter)
        VALUES(:guid, (SELECT id FROM moz_bookmarks WHERE guid = :parentGuid),
               (SELECT id FROM moz_places WHERE guid = :placeGuid),
               (SELECT count(*) FROM moz_bookmarks b
                JOIN moz_bookmarks p ON p.id = b.parent
                WHERE p.guid = :parentGuid), :type, :localTitle,
                :syncStatus, 1)`,
        {
          guid,
          parentGuid,
          placeGuid,
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          localTitle,
          syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NEW,
        }
      );

      await buf.db.executeCached(
        `
        INSERT INTO items(guid, parentGuid, needsMerge, kind, title, urlId)
        VALUES(:guid, :parentGuid, 1, :kind, :remoteTitle,
               (SELECT id FROM urls WHERE guid = :placeGuid))`,
        {
          guid,
          parentGuid,
          placeGuid,
          kind: Ci.mozISyncedBookmarksMerger.KIND_BOOKMARK,
          remoteTitle,
        }
      );

      await buf.db.executeCached(
        `
        INSERT INTO structure(guid, parentGuid, position)
        VALUES(:guid, :parentGuid,
               IFNULL((SELECT count(*) FROM structure
                       WHERE parentGuid = :parentGuid), 0))`,
        { guid, parentGuid }
      );
    }
  });

  info("Apply mirror");
  let observer = expectBookmarkChangeNotifications();
  let changesToUpload = await buf.apply({
    notifyInStableOrder: true,
  });
  deepEqual(
    await buf.fetchUnmergedGuids(),
    [
      PlacesUtils.bookmarks.menuGuid,
      PlacesUtils.bookmarks.mobileGuid,
      PlacesUtils.bookmarks.toolbarGuid,
      PlacesUtils.bookmarks.unfiledGuid,
    ],
    "Should leave roots unmerged"
  );
  deepEqual(
    Object.keys(changesToUpload).sort(),
    ["menu", "mobile", "toolbar", "unfiled"],
    "Should upload roots"
  );

  await assertLocalTree(
    PlacesUtils.bookmarks.rootGuid,
    {
      guid: PlacesUtils.bookmarks.rootGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: "",
      children: [
        {
          guid: PlacesUtils.bookmarks.menuGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 0,
          title: BookmarksMenuTitle,
          children: [
            {
              guid: "bookmarkAAAA",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              title: "A (remote)",
              url: "http://example.com/",
            },
          ],
        },
        {
          guid: PlacesUtils.bookmarks.toolbarGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 1,
          title: BookmarksToolbarTitle,
          children: [
            {
              guid: "bookmarkBBBB",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              title: "B (remote)",
              url: "http://example.com/",
            },
          ],
        },
        {
          guid: PlacesUtils.bookmarks.unfiledGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 3,
          title: UnfiledBookmarksTitle,
          children: [
            {
              guid: "bookmarkCCCC",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              title: "C (remote)",
              url: "http://example.com/c",
            },
          ],
        },
        {
          guid: PlacesUtils.bookmarks.mobileGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 4,
          title: MobileBookmarksTitle,
        },
      ],
    },
    "Should update titles for items with duplicate URLs"
  );

  let localItemIds = await PlacesTestUtils.promiseManyItemIds([
    "bookmarkAAAA",
    "bookmarkBBBB",
    "bookmarkCCCC",
  ]);
  observer.check([
    {
      name: "bookmark-title-changed",
      params: {
        itemId: localItemIds.get("bookmarkAAAA"),
        title: "A (remote)",
        guid: "bookmarkAAAA",
        parentGuid: PlacesUtils.bookmarks.menuGuid,
      },
    },
    {
      name: "bookmark-title-changed",
      params: {
        itemId: localItemIds.get("bookmarkBBBB"),
        title: "B (remote)",
        guid: "bookmarkBBBB",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      },
    },
    {
      name: "bookmark-title-changed",
      params: {
        itemId: localItemIds.get("bookmarkCCCC"),
        title: "C (remote)",
        guid: "bookmarkCCCC",
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      },
    },
  ]);

  info("Remove duplicate URLs from Places to avoid tripping debug asserts");
  await buf.db.executeTransaction(async function () {
    for (let { guid } of placesToInsert) {
      await buf.db.executeCached(
        `
        DELETE FROM moz_places WHERE guid = :guid`,
        { guid }
      );
    }
  });

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_duplicate_local_tags() {
  let buf = await openMirror("duplicate_local_tags");
  let now = new Date();

  info("Insert A");
  await PlacesUtils.bookmarks.insert({
    guid: "bookmarkAAAA",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "A",
    url: "http://example.com/a",
    dateAdded: now,
  });

  // Each tag folder should have unique tag entries, but the tagging service
  // doesn't enforce this. We should still sync the correct set of tags,
  // though, even if there are duplicates for the same URL.
  info("Manually insert local tags for A");
  for (let [tag, dupes] of [
    ["one", 2],
    ["two", 1],
    ["three", 2],
  ]) {
    let tagFolderInfo = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.tagsGuid,
      title: tag,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
    });
    for (let i = 0; i < dupes; ++i) {
      await PlacesUtils.bookmarks.insert({
        parentGuid: tagFolderInfo.guid,
        url: "http://example.com/a",
      });
    }
  }

  let tagsForA = PlacesUtils.tagging.getTagsForURI(
    Services.io.newURI("http://example.com/a")
  );
  deepEqual(
    tagsForA,
    ["one", "one", "three", "three", "two"],
    "Tagging service should return duplicate tags"
  );

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(
    changesToUpload.bookmarkAAAA.cleartext,
    {
      id: "bookmarkAAAA",
      type: "bookmark",
      parentid: "menu",
      hasDupe: true,
      parentName: BookmarksMenuTitle,
      dateAdded: now.getTime(),
      bmkUri: "http://example.com/a",
      title: "A",
      tags: ["one", "three", "two"],
    },
    "Should upload A with tags"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
