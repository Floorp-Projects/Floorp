/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PlacesQuery } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesQuery.sys.mjs"
);

const placesQuery = new PlacesQuery();

registerCleanupFunction(() => placesQuery.close());

async function waitForUpdateHistoryTask(updateTask) {
  const promise = new Promise(resolve =>
    placesQuery.observeHistory(newHistory => resolve(newHistory))
  );
  await updateTask();
  return promise;
}

add_task(async function test_visits_cache_is_updated() {
  const now = new Date();
  info("Insert the first visit.");
  await PlacesUtils.history.insert({
    url: "https://www.example.com/",
    title: "Example Domain",
    visits: [{ date: now }],
  });
  let history = await placesQuery.getHistory();
  const mapKey = placesQuery.getStartOfDayTimestamp(now);
  history = history.get(mapKey);
  Assert.equal(history.length, 1);
  Assert.equal(history[0].url, "https://www.example.com/");
  Assert.equal(history[0].date.getTime(), now.getTime());
  Assert.equal(history[0].title, "Example Domain");

  info("Insert the next visit.");
  history = await waitForUpdateHistoryTask(() =>
    PlacesUtils.history.insert({
      url: "https://example.net/",
      visits: [{ date: now }],
    })
  );
  history = history.get(mapKey);
  Assert.equal(history.length, 2);
  Assert.equal(
    history[0].url,
    "https://example.net/",
    "The most recent visit should come first."
  );
  Assert.equal(history[0].date.getTime(), now.getTime());

  info("Remove the first visit.");
  history = await waitForUpdateHistoryTask(() =>
    PlacesUtils.history.remove("https://www.example.com/")
  );
  history = history.get(mapKey);
  Assert.equal(history.length, 1);
  Assert.equal(history[0].url, "https://example.net/");

  info("Remove all visits.");
  history = await waitForUpdateHistoryTask(() => PlacesUtils.history.clear());
  Assert.equal(history.size, 0);
});

add_task(async function test_filter_visits_by_age() {
  await PlacesUtils.history.insertMany([
    {
      url: "https://www.example.com/",
      visits: [{ date: new Date("2000-01-01T12:00:00") }],
    },
    {
      url: "https://example.net/",
      visits: [{ date: new Date() }],
    },
  ]);
  let history = await placesQuery.getHistory({ daysOld: 1 });
  history = [...history.values()].flat();
  Assert.equal(history.length, 1, "The older visit should be excluded.");
  Assert.equal(history[0].url, "https://example.net/");
  await PlacesUtils.history.clear();
});

add_task(async function test_visits_sort_option() {
  const now = new Date();
  info("Get history sorted by site.");
  await PlacesUtils.history.insertMany([
    { url: "https://www.reddit.com/", visits: [{ date: now }] },
    { url: "https://twitter.com/", visits: [{ date: now }] },
  ]);
  let history = await placesQuery.getHistory({ sortBy: "site" });
  ["reddit.com", "twitter.com"].forEach(domain =>
    Assert.ok(history.has(domain))
  );

  info("Insert the next visit.");
  history = await waitForUpdateHistoryTask(() =>
    PlacesUtils.history.insert({
      url: "https://www.wikipedia.org/",
      visits: [{ date: now }],
    })
  );
  ["reddit.com", "twitter.com", "wikipedia.org"].forEach(domain =>
    Assert.ok(history.has(domain))
  );

  info("Update the sort order.");
  history = await placesQuery.getHistory({ sortBy: "date" });
  const mapKey = placesQuery.getStartOfDayTimestamp(now);
  Assert.equal(history.get(mapKey).length, 3);

  info("Insert the next visit.");
  history = await waitForUpdateHistoryTask(() =>
    PlacesUtils.history.insert({
      url: "https://www.youtube.com/",
      visits: [{ date: now }],
    })
  );
  Assert.equal(history.get(mapKey).length, 4);
  await PlacesUtils.history.clear();
});

add_task(async function test_visits_limit_option() {
  await PlacesUtils.history.insertMany([
    {
      url: "https://www.example.com/",
      visits: [{ date: new Date() }],
    },
    {
      url: "https://example.net/",
      visits: [{ date: new Date() }],
    },
  ]);
  let history = await placesQuery.getHistory({ limit: 1 });
  history = [...history.values()].flat();
  Assert.equal(history.length, 1, "Number of visits should be limited to 1.");
  await PlacesUtils.history.clear();
});
