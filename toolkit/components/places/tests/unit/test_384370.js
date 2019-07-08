var tagData = [
  { uri: uri("http://slint.us"), tags: ["indie", "kentucky", "music"] },
  {
    uri: uri("http://en.wikipedia.org/wiki/Diplodocus"),
    tags: ["dinosaur", "dj", "rad word"],
  },
];

var bookmarkData = [
  { uri: uri("http://slint.us"), title: "indie, kentucky, music" },
  {
    uri: uri("http://en.wikipedia.org/wiki/Diplodocus"),
    title: "dinosaur, dj, rad word",
  },
];

/*
  HTML+FEATURES SUMMARY:
  - import legacy bookmarks
  - export as json, import, test (tests integrity of html > json)
  - export as html, import, test (tests integrity of json > html)

  BACKUP/RESTORE SUMMARY:
  - create a bookmark in each root
  - tag multiple URIs with multiple tags
  - export as json, import, test
*/
add_task(async function() {
  // Remove eventual bookmarks.exported.json.
  let jsonFile = OS.Path.join(
    OS.Constants.Path.profileDir,
    "bookmarks.exported.json"
  );
  if (await OS.File.exists(jsonFile)) {
    await OS.File.remove(jsonFile);
  }

  // Test importing a pre-Places canonical bookmarks file.
  // Note: we do not empty the db before this import to catch bugs like 380999
  let htmlFile = OS.Path.join(do_get_cwd().path, "bookmarks.preplaces.html");
  await BookmarkHTMLUtils.importFromFile(htmlFile, { replace: true });

  // Populate the database.
  for (let { uri, tags } of tagData) {
    PlacesUtils.tagging.tagURI(uri, tags);
  }
  for (let { uri, title } of bookmarkData) {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: uri,
      title,
    });
  }
  for (let { uri, title } of bookmarkData) {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: uri,
      title,
    });
  }

  await validate("initial database");

  // Test exporting a Places canonical json file.
  // 1. export to bookmarks.exported.json
  await BookmarkJSONUtils.exportToFile(jsonFile);
  info("exported json");

  // 2. empty bookmarks db
  // 3. import bookmarks.exported.json
  await BookmarkJSONUtils.importFromFile(jsonFile, { replace: true });
  info("imported json");

  // 4. run the test-suite
  await validate("re-imported json");
  info("validated import");
});

async function validate(infoMsg) {
  info(`Validating ${infoMsg}: testMenuBookmarks`);
  await testMenuBookmarks();
  info(`Validating ${infoMsg}: testToolbarBookmarks`);
  await testToolbarBookmarks();
  info(`Validating ${infoMsg}: testUnfiledBookmarks`);
  testUnfiledBookmarks();
  info(`Validating ${infoMsg}: testTags`);
  testTags();
  await PlacesTestUtils.promiseAsyncUpdates();
}

// Tests a bookmarks datastore that has a set of bookmarks, etc
// that flex each supported field and feature.
async function testMenuBookmarks() {
  let root = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.menuGuid).root;
  Assert.equal(root.childCount, 3);

  let separatorNode = root.getChild(1);
  Assert.equal(separatorNode.type, separatorNode.RESULT_TYPE_SEPARATOR);

  let folderNode = root.getChild(2);
  Assert.equal(folderNode.type, folderNode.RESULT_TYPE_FOLDER);
  Assert.equal(folderNode.title, "test");
  let folder = await PlacesUtils.bookmarks.fetch(folderNode.bookmarkGuid);
  Assert.equal(folder.dateAdded.getTime(), 1177541020000);

  Assert.equal(PlacesUtils.asQuery(folderNode).hasChildren, true);

  // open test folder, and test the children
  folderNode.containerOpen = true;
  Assert.equal(folderNode.childCount, 1);

  let bookmarkNode = folderNode.getChild(0);
  Assert.equal("http://test/post", bookmarkNode.uri);
  Assert.equal("test post keyword", bookmarkNode.title);
  Assert.equal(bookmarkNode.dateAdded, 1177375336000000);

  let entry = await PlacesUtils.keywords.fetch({ url: bookmarkNode.uri });
  Assert.equal("test", entry.keyword);
  Assert.equal("hidden1%3Dbar&text1%3D%25s", entry.postData);

  let pageInfo = await PlacesUtils.history.fetch(bookmarkNode.uri, {
    includeAnnotations: true,
  });
  Assert.equal(
    pageInfo.annotations.get(PlacesUtils.CHARSET_ANNO),
    "ISO-8859-1",
    "Should have the correct charset"
  );

  folderNode.containerOpen = false;
  root.containerOpen = false;
}

async function testToolbarBookmarks() {
  let root = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.toolbarGuid)
    .root;

  // child count (add 2 for pre-existing items, one of the feeds is skipped
  // because it doesn't have href)
  Assert.equal(root.childCount, bookmarkData.length + 2);

  // Livemarks are no more supported but may still exist in old html files.
  let legacyLivemarkNode = root.getChild(1);
  Assert.equal("Latest Headlines", legacyLivemarkNode.title);
  Assert.equal(
    "http://en-us.fxfeeds.mozilla.com/en-US/firefox/livebookmarks/",
    legacyLivemarkNode.uri
  );
  Assert.equal(
    legacyLivemarkNode.type,
    Ci.nsINavHistoryResultNode.RESULT_TYPE_URI
  );

  // test added bookmark data
  let bookmarkNode = root.getChild(2);
  Assert.equal(bookmarkNode.uri, bookmarkData[0].uri.spec);
  Assert.equal(bookmarkNode.title, bookmarkData[0].title);
  bookmarkNode = root.getChild(3);
  Assert.equal(bookmarkNode.uri, bookmarkData[1].uri.spec);
  Assert.equal(bookmarkNode.title, bookmarkData[1].title);

  root.containerOpen = false;
}

function testUnfiledBookmarks() {
  let root = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.unfiledGuid)
    .root;
  // child count (add 1 for pre-existing item)
  Assert.equal(root.childCount, bookmarkData.length + 1);
  for (let i = 1; i < root.childCount; ++i) {
    let child = root.getChild(i);
    Assert.equal(child.uri, bookmarkData[i - 1].uri.spec);
    Assert.equal(child.title, bookmarkData[i - 1].title);
    if (child.tags) {
      Assert.equal(child.tags, bookmarkData[i - 1].title);
    }
  }
  root.containerOpen = false;
}

function testTags() {
  for (let { uri, tags } of tagData) {
    info("Test tags for " + uri.spec + ": " + tags + "\n");
    let foundTags = PlacesUtils.tagging.getTagsForURI(uri);
    Assert.equal(foundTags.length, tags.length);
    Assert.ok(tags.every(tag => foundTags.includes(tag)));
  }
}
