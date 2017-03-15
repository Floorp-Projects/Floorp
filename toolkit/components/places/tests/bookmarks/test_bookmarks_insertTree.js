add_task(function* invalid_input_rejects() {
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(), /Should be provided a valid tree object./);
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(null), /Should be provided a valid tree object./);
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree("foo"), /Should be provided a valid tree object./);

  // All subsequent tests pass a valid parent guid.
  let guid = PlacesUtils.bookmarks.unfiledGuid;

  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree({guid, children: []}),
                      /Should have a non-zero number of children to insert./);
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree({guid}),
                      /Should have a non-zero number of children to insert./);
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree({children: [{}], guid}),
                      /The following properties were expected: url/);

  // Reuse another variable to make this easier to read:
  let tree = {guid, children: [{ guid: "test" }]};
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'guid'/);
  tree.children = [{ guid: null }];
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'guid'/);
  tree.children = [{ guid: 123 }];
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'guid'/);

  tree.children = [{ dateAdded: -10 }];
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'dateAdded'/);
  tree.children = [{ dateAdded: "today" }];
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'dateAdded'/);
  tree.children = [{ dateAdded: Date.now() }];
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'dateAdded'/);

  tree.children = [{lastModified: -10}];
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'lastModified'/);
  tree.children = [{lastModified: "today"}];
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'lastModified'/);
  tree.children = [{lastModified: Date.now()}];
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'lastModified'/);

  let time = new Date();
  let future = new Date(time + 86400000);
  tree.children = [{ dateAdded: future, lastModified: time }];
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'dateAdded'/);
  let past = new Date(time - 86400000);
  tree.children = [{ lastModified: past }];
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'lastModified'/);

  tree.children = [{type: -1}];
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'type'/);
  tree.children = [{type: 100}];
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'type'/);
  tree.children = [{type: "bookmark"}];
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'type'/);

  tree.children = [{ type: PlacesUtils.bookmarks.TYPE_BOOKMARK, title: -1 }];
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'title'/);

  tree.children = [{ type: PlacesUtils.bookmarks.TYPE_BOOKMARK, url: 10 }];
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(tree),
                      /Invalid value for property 'url'/);

  let treeWithBrokenURL = {
    children: [{ type: PlacesUtils.bookmarks.TYPE_BOOKMARK, url: "http://te st" }],
    guid: PlacesUtils.bookmarks.unfiledGuid
  };
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(treeWithBrokenURL),
                      /Invalid value for property 'url'/);
  let longurl = "http://www.example.com/" + "a".repeat(65536);
  let treeWithLongURL = {
    children: [{ type: PlacesUtils.bookmarks.TYPE_BOOKMARK, url: longurl }],
    guid: PlacesUtils.bookmarks.unfiledGuid
  };
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(treeWithLongURL),
                      /Invalid value for property 'url'/);
  let treeWithLongURI = {
    children: [{ type: PlacesUtils.bookmarks.TYPE_BOOKMARK, url: NetUtil.newURI(longurl) }],
    guid: PlacesUtils.bookmarks.unfiledGuid
  };
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(treeWithLongURI),
                      /Invalid value for property 'url'/);
  let treeWithOtherBrokenURL = {
    children: [{ type: PlacesUtils.bookmarks.TYPE_BOOKMARK, url: "te st" }],
    guid: PlacesUtils.bookmarks.unfiledGuid
  };
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(treeWithOtherBrokenURL),
                      /Invalid value for property 'url'/);
});

add_task(function* invalid_properties_for_bookmark_type() {
  let folderWithURL = {
    children: [{ type: PlacesUtils.bookmarks.TYPE_FOLDER, url: "http://www.moz.com/" }],
    guid: PlacesUtils.bookmarks.unfiledGuid
  };
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(folderWithURL),
                      /Invalid value for property 'url'/);
  let separatorWithURL = {
    children: [{ type: PlacesUtils.bookmarks.TYPE_SEPARATOR, url: "http://www.moz.com/" }],
    guid: PlacesUtils.bookmarks.unfiledGuid
  };
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(separatorWithURL),
                      /Invalid value for property 'url'/);
  let separatorWithTitle = {
    children: [{ type: PlacesUtils.bookmarks.TYPE_SEPARATOR, title: "test" }],
    guid: PlacesUtils.bookmarks.unfiledGuid
  };
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree(separatorWithTitle),
                      /Invalid value for property 'title'/);
});

add_task(function* create_separator() {
  let [bm] = yield PlacesUtils.bookmarks.insertTree({children: [{
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR
  }], guid: PlacesUtils.bookmarks.unfiledGuid});
  checkBookmarkObject(bm);
  Assert.equal(bm.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm.index, 0);
  Assert.equal(bm.dateAdded, bm.lastModified);
  Assert.equal(bm.type, PlacesUtils.bookmarks.TYPE_SEPARATOR);
  Assert.ok(!("title" in bm), "title should not be set");
});

add_task(function* create_plain_bm() {
  let [bm] = yield PlacesUtils.bookmarks.insertTree({children: [{
    url: "http://www.example.com/",
    title: "Test"
  }], guid: PlacesUtils.bookmarks.unfiledGuid});
  checkBookmarkObject(bm);
  Assert.equal(bm.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm.index, 1);
  Assert.equal(bm.dateAdded, bm.lastModified);
  Assert.equal(bm.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
  Assert.equal(bm.title, "Test");
  Assert.equal(bm.url.href, "http://www.example.com/");
});

add_task(function* create_folder() {
  let [bm] = yield PlacesUtils.bookmarks.insertTree({children: [{
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "Test"
  }], guid: PlacesUtils.bookmarks.unfiledGuid});
  checkBookmarkObject(bm);
  Assert.equal(bm.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm.index, 2);
  Assert.equal(bm.dateAdded, bm.lastModified);
  Assert.equal(bm.type, PlacesUtils.bookmarks.TYPE_FOLDER);
  Assert.equal(bm.title, "Test");
});


add_task(function* create_in_tags() {
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree({children: [{
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: "http://www.example.com/",
    title: "Test adding a tag",
  }], guid: PlacesUtils.bookmarks.tagsGuid}), /Can't use insertTree to insert tags/);
  let guidForTag = (yield PlacesUtils.bookmarks.insert({
    title: "test-tag",
    url: "http://www.unused.com/",
    parentGuid: PlacesUtils.bookmarks.tagsGuid,
  })).guid;
  yield Assert.rejects(PlacesUtils.bookmarks.insertTree({children: [{
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: "http://www.example.com/",
    title: "Test adding an item to a tag",
  }], guid: guidForTag}), /Can't use insertTree to insert tags/);
  yield PlacesUtils.bookmarks.remove(guidForTag);
  yield PlacesTestUtils.promiseAsyncUpdates();
});

add_task(function* insert_into_root() {
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree({children: [{
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: "http://www.example.com/",
    title: "Test inserting into root",
  }], guid: PlacesUtils.bookmarks.rootGuid}), /Can't insert into the root/);
});

add_task(function* tree_where_separator_or_folder_has_kids() {
  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree({children: [{
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    children: [{
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "http://www.example.com/",
      title: "Test inserting into separator",
    }],
  }], guid: PlacesUtils.bookmarks.unfiledGuid}), /Invalid value for property 'children'/);

  yield Assert.throws(() => PlacesUtils.bookmarks.insertTree({children: [{
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    children: [{
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "http://www.example.com/",
      title: "Test inserting into separator",
    }],
  }], guid: PlacesUtils.bookmarks.unfiledGuid}), /Invalid value for property 'children'/);
});

add_task(function* create_hierarchy() {
  let obsInvoked = 0;
  let obs = {
    onItemAdded(itemId, parentId, index, type, uri, title, dateAdded, guid, parentGuid) {
      obsInvoked++;
      Assert.greater(itemId, 0, "Should have a valid itemId");
    },
  };
  PlacesUtils.bookmarks.addObserver(obs, false);
  let bms = yield PlacesUtils.bookmarks.insertTree({children: [{
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "Root item",
    children: [
      {
        url: "http://www.example.com/1",
        title: "BM 1",
      },
      {
        url: "http://www.example.com/2",
        title: "BM 2",
      },
      {
        title: "Sub",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        children: [
          {
            title: "Sub BM 1",
            url: "http://www.example.com/sub/1",
          },
          {
            title: "Sub BM 2",
            url: "http://www.example.com/sub/2",
          },
        ],
      },
    ]
  }], guid: PlacesUtils.bookmarks.unfiledGuid});
  yield PlacesTestUtils.promiseAsyncUpdates();
  PlacesUtils.bookmarks.removeObserver(obs);
  let parentFolder = null, subFolder = null;
  let prevBM = null;
  for (let bm of bms) {
    checkBookmarkObject(bm);
    if (prevBM && prevBM.parentGuid == bm.parentGuid) {
      Assert.equal(prevBM.index + 1, bm.index, "Indices should be subsequent");
      Assert.equal((yield PlacesUtils.bookmarks.fetch(bm.guid)).index, bm.index, "Index reflects inserted index");
    }
    prevBM = bm;
    if (bm.type == PlacesUtils.bookmarks.TYPE_BOOKMARK) {
      Assert.greater(frecencyForUrl(bm.url), 0, "Check frecency has been updated for bookmark " + bm.url);
    }
    if (bm.title == "Root item") {
      Assert.equal(bm.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
      parentFolder = bm;
    } else if (!bm.title.startsWith("Sub BM")) {
      Assert.equal(bm.parentGuid, parentFolder.guid);
      if (bm.type == PlacesUtils.bookmarks.TYPE_FOLDER) {
        subFolder = bm;
      }
    } else {
      Assert.equal(bm.parentGuid, subFolder.guid);
    }
  }
  Assert.equal(obsInvoked, bms.length);
  Assert.equal(obsInvoked, 6);
});

add_task(function* insert_many_non_nested() {
  let obsInvoked = 0;
  let obs = {
    onItemAdded(itemId, parentId, index, type, uri, title, dateAdded, guid, parentGuid) {
      obsInvoked++;
      Assert.greater(itemId, 0, "Should have a valid itemId");
    },
  };
  PlacesUtils.bookmarks.addObserver(obs, false);
  let bms = yield PlacesUtils.bookmarks.insertTree({children: [{
      url: "http://www.example.com/1",
      title: "Item 1",
    },
    {
      url: "http://www.example.com/2",
      title: "Item 2",
    },
    {
      url: "http://www.example.com/3",
      title: "Item 3",
    },
    {
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR
    },
    {
      title: "Item 4",
      url: "http://www.example.com/4",
    },
    {
      title: "Item 5",
      url: "http://www.example.com/5",
    },
  ], guid: PlacesUtils.bookmarks.unfiledGuid});
  yield PlacesTestUtils.promiseAsyncUpdates();
  PlacesUtils.bookmarks.removeObserver(obs);
  let startIndex = -1;
  for (let bm of bms) {
    checkBookmarkObject(bm);
    if (startIndex == -1) {
      startIndex = bm.index;
    } else {
      Assert.equal(++startIndex, bm.index, "Indices should be subsequent");
    }
    Assert.equal((yield PlacesUtils.bookmarks.fetch(bm.guid)).index, bm.index, "Index reflects inserted index");
    if (bm.type == PlacesUtils.bookmarks.TYPE_BOOKMARK) {
      Assert.greater(frecencyForUrl(bm.url), 0, "Check frecency has been updated for bookmark " + bm.url);
    }
    Assert.equal(bm.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  }
  Assert.equal(obsInvoked, bms.length);
  Assert.equal(obsInvoked, 6);
});
