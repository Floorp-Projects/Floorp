var bookmarkData = [
  {
    uri: uri("http://www.toastytech.com"),
    title: "Nathan's Toasty Technology Page",
    tags: ["technology", "personal", "retro"],
  },
  {
    uri: uri("http://www.reddit.com"),
    title: "reddit: the front page of the internet",
    tags: ["social media", "news", "humour"],
  },
  {
    uri: uri("http://www.4chan.org"),
    title: "4chan",
    tags: ["discussion", "imageboard", "anime"],
  },
];

/*
  TEST SUMMARY
  - Add bookmarks with tags
  - Export tagged bookmarks as HTML file
  - Delete bookmarks
  - Import bookmarks from HTML file
  - Check that all bookmarks are successfully imported with tags
*/

add_task(async function test_import_tags() {
  // Removes bookmarks.html if the file already exists.
  let HTMLFile = OS.Path.join(OS.Constants.Path.profileDir, "bookmarks.html");
  if (await OS.File.exists(HTMLFile)) {
    await OS.File.remove(HTMLFile);
  }

  // Adds bookmarks and tags to the database.
  let bookmarkList = new Set();
  for (let { uri, title, tags } of bookmarkData) {
    bookmarkList.add(
      await PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        url: uri,
        title,
      })
    );
    PlacesUtils.tagging.tagURI(uri, tags);
  }

  // Exports the bookmarks as a HTML file.
  await BookmarkHTMLUtils.exportToFile(HTMLFile);

  // Deletes bookmarks and tags from the database.
  for (let bookmark of bookmarkList) {
    await PlacesUtils.bookmarks.remove(bookmark.guid);
  }

  // Re-imports the bookmarks from the HTML file.
  await BookmarkHTMLUtils.importFromFile(HTMLFile, { replace: true });

  // Tests to ensure that the tags are still present for each bookmark URI.
  for (let { uri, tags } of bookmarkData) {
    info("Test tags for " + uri.spec + ": " + tags + "\n");
    let foundTags = PlacesUtils.tagging.getTagsForURI(uri);
    Assert.equal(foundTags.length, tags.length);
    Assert.ok(tags.every(tag => foundTags.includes(tag)));
  }
});
