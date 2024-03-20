/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test PlacesObservers.counts.

add_task(async function test_counts() {
  const url = "http://example.com/title";
  await PlacesUtils.history.insertMany([
    {
      title: "will change",
      url,
      visits: [{ transition: TRANSITION_LINK }],
    },
    {
      title: "changed",
      url,
      referrer: url,
      visits: [{ transition: TRANSITION_LINK }],
    },
    {
      title: "another",
      url: "http://example.com/another",
      visits: [{ transition: TRANSITION_LINK }],
    },
  ]);
  await PlacesUtils.bookmarks.insert({
    url,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything;

  Assert.strictEqual(
    PlacesObservers.counts.get("non-existing"),
    undefined,
    "Check non existing event returns undefined"
  );
  Assert.strictEqual(
    PlacesObservers.counts.get("page-removed"),
    0,
    "Check non fired event returns 0"
  );
  Assert.strictEqual(
    PlacesObservers.counts.get("page-visited"),
    3,
    "Check fired event `page-visited`"
  );
  Assert.strictEqual(
    PlacesObservers.counts.get("history-cleared"),
    1,
    "Check fired event `history-cleared`"
  );
  Assert.strictEqual(
    PlacesObservers.counts.get("bookmark-added"),
    1,
    "Check fired event `bookmark-added`"
  );
});
