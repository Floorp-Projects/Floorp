function insertTree(tree) {
  return PlacesUtils.bookmarks.insertTree(tree, {
    fixupOrSkipInvalidEntries: true,
  });
}

add_task(async function() {
  let guid = PlacesUtils.bookmarks.unfiledGuid;
  await Assert.throws(
    () => insertTree({ guid, children: [] }),
    /Should have a non-zero number of children to insert./
  );
  await Assert.throws(
    () => insertTree({ guid: "invalid", children: [{}] }),
    /The parent guid is not valid/
  );

  let now = new Date();
  let url = "http://mozilla.com/";
  let obs = {
    count: 0,
    lastIndex: 0,
    handlePlacesEvent(events) {
      for (let event of events) {
        obs.count++;
        let lastIndex = obs.lastIndex;
        obs.lastIndex = event.index;
        if (event.itemType == PlacesUtils.bookmarks.TYPE_BOOKMARK) {
          Assert.equal(event.url, url, "Found the expected url");
        }
        Assert.ok(
          event.index == 0 || event.index == lastIndex + 1,
          "Consecutive indices"
        );
        Assert.ok(event.dateAdded >= now, "Found a valid dateAdded");
        Assert.ok(PlacesUtils.isValidGuid(event.guid), "guid is valid");
      }
    },
  };
  PlacesUtils.observers.addListener(["bookmark-added"], obs.handlePlacesEvent);

  let tree = {
    guid,
    children: [
      {
        // Should be inserted, and the invalid guid should be replaced.
        guid: "test",
        url,
      },
      {
        // Should be skipped, since the url is invalid.
        url: "fake_url",
      },
      {
        // Should be skipped, since the type is invalid.
        url,
        type: 999,
      },
      {
        // Should be skipped, since the type is invalid.
        type: 999,
        children: [
          {
            url,
          },
        ],
      },
      {
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "test",
        children: [
          {
            // Should fix lastModified and dateAdded.
            url,
            lastModified: null,
          },
          {
            // Should be skipped, since the url is invalid.
            url: "fake_url",
            dateAdded: null,
          },
          {
            // Should fix lastModified and dateAdded.
            url,
            dateAdded: undefined,
          },
          {
            // Should be skipped since it's a separator with a url
            url,
            type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
          },
          {
            // Should fix lastModified and dateAdded.
            url,
            dateAdded: new Date(now - 86400000),
            lastModified: new Date(now - 172800000), // less than dateAdded
          },
        ],
      },
    ],
  };

  let bms = await insertTree(tree);
  for (let bm of bms) {
    checkBookmarkObject(bm);
  }
  Assert.equal(bms.length, 5);
  Assert.equal(obs.count, bms.length);

  PlacesUtils.observers.removeListener(
    ["bookmark-added"],
    obs.handlePlacesEvent
  );
});
