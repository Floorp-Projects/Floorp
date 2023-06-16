/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests frecency of unvisited bookmarks.

add_task(async function () {
  // Randomly sorted by date.
  const now = new Date();
  const bookmarks = [
    {
      url: "https://example.com/1",
      date: new Date(new Date().setDate(now.getDate() - 30)),
    },
    {
      url: "https://example.com/2",
      date: new Date(new Date().setDate(now.getDate() - 1)),
    },
    {
      url: "https://example.com/3",
      date: new Date(new Date().setDate(now.getDate() - 100)),
    },
    {
      url: "https://example.com/1", // Same url but much older.
      date: new Date(new Date().setDate(now.getDate() - 120)),
    },
  ];

  for (let bookmark of bookmarks) {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: bookmark.url,
      dateAdded: bookmark.date,
    });
  }
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  // The newest bookmark should have an higher frecency.
  Assert.greater(
    await PlacesTestUtils.getDatabaseValue("moz_places", "frecency", {
      url: bookmarks[1].url,
    }),
    await PlacesTestUtils.getDatabaseValue("moz_places", "frecency", {
      url: bookmarks[0].url,
    })
  );
  Assert.greater(
    await PlacesTestUtils.getDatabaseValue("moz_places", "frecency", {
      url: bookmarks[0].url,
    }),
    await PlacesTestUtils.getDatabaseValue("moz_places", "frecency", {
      url: bookmarks[2].url,
    })
  );
});
