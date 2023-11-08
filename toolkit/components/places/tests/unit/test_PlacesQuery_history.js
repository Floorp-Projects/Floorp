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
  const now = new Date();
  await PlacesUtils.history.insertMany([
    {
      url: "https://www.example.com/",
      visits: [{ date: new Date("2000-01-01T12:00:00") }],
    },
    {
      url: "https://example.net/",
      visits: [{ date: now }],
    },
  ]);
  let history = await placesQuery.getHistory({ daysOld: 1 });
  Assert.equal(history.size, 1, "The older visit should be excluded.");
  Assert.equal(
    history.get(placesQuery.getStartOfDayTimestamp(now))[0].url,
    "https://example.net/"
  );
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
  Assert.equal(
    [...history.values()].reduce((acc, { length }) => acc + length, 0),
    1,
    "Number of visits should be limited to 1."
  );
  await PlacesUtils.history.clear();
});

add_task(async function test_dedupe_visits_by_url() {
  const today = new Date();
  const yesterday = new Date(
    today.getFullYear(),
    today.getMonth(),
    today.getDate() - 1
  );
  await PlacesUtils.history.insertMany([
    {
      url: "https://www.example.com/",
      visits: [{ date: yesterday }],
    },
    {
      url: "https://www.example.com/",
      visits: [{ date: today }],
    },
    {
      url: "https://www.example.com/",
      visits: [{ date: today }],
    },
  ]);
  info("Get history sorted by date.");
  let history = await placesQuery.getHistory({ sortBy: "date" });
  Assert.equal(
    history.get(placesQuery.getStartOfDayTimestamp(yesterday)).length,
    1,
    "There was only one visit from yesterday."
  );
  Assert.equal(
    history.get(placesQuery.getStartOfDayTimestamp(today)).length,
    1,
    "The duplicate visit from today should be removed."
  );
  history = await waitForUpdateHistoryTask(() =>
    PlacesUtils.history.insert({
      url: "https://www.example.com/",
      visits: [{ date: today }],
    })
  );
  Assert.equal(
    history.get(placesQuery.getStartOfDayTimestamp(today)).length,
    1,
    "Visits inserted from `page-visited` events should be deduped."
  );

  info("Get history sorted by site.");
  history = await placesQuery.getHistory({ sortBy: "site" });
  Assert.equal(
    history.get("example.com").length,
    1,
    "The duplicate visits for this site should be removed."
  );
  history = await waitForUpdateHistoryTask(() =>
    PlacesUtils.history.insert({
      url: "https://www.example.com/",
      visits: [{ date: yesterday }],
    })
  );
  const visits = history.get("example.com");
  Assert.equal(
    visits.length,
    1,
    "Visits inserted from `page-visited` events should be deduped."
  );
  Assert.equal(
    visits[0].date.getTime(),
    today.getTime(),
    "Deduping keeps the most recent visit."
  );

  await PlacesUtils.history.clear();
});

add_task(async function test_search_visits() {
  const now = new Date();
  await PlacesUtils.history.insertMany([
    {
      url: "https://www.example.com/",
      title: "First Visit",
      visits: [{ date: now }],
    },
    {
      url: "https://example.net/",
      title: "Second Visit",
      visits: [{ date: now }],
    },
  ]);

  let results = await placesQuery.searchHistory("Visit");
  Assert.equal(results.length, 2, "Both visits match the search query.");

  results = await placesQuery.searchHistory("First Visit");
  Assert.equal(results.length, 1, "One visit matches the search query.");

  results = await placesQuery.searchHistory("Bogus");
  Assert.equal(results.length, 0, "Neither visit matches the search query.");

  await PlacesUtils.history.clear();
});
