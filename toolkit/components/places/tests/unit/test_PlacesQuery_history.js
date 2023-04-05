/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PlacesQuery } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesQuery.sys.mjs"
);

add_task(async function test_visits_cache_is_updated() {
  const placesQuery = new PlacesQuery();
  const now = new Date();
  info("Insert the first visit.");
  await PlacesUtils.history.insert({
    url: "https://www.example.com/",
    title: "Example Domain",
    visits: [{ date: now }],
  });
  let history = await placesQuery.getHistory();
  Assert.equal(history.length, 1);
  Assert.equal(history[0].url, "https://www.example.com/");
  Assert.equal(history[0].date.getTime(), now.getTime());
  Assert.equal(history[0].title, "Example Domain");

  info("Insert the next visit.");
  let historyUpdated = PromiseUtils.defer();
  placesQuery.observeHistory(newHistory => {
    history = newHistory;
    historyUpdated.resolve();
  });
  await PlacesUtils.history.insert({
    url: "https://example.net/",
    visits: [{ date: now }],
  });
  await historyUpdated.promise;
  Assert.equal(history.length, 2);
  Assert.equal(
    history[0].url,
    "https://example.net/",
    "The most recent visit should come first."
  );
  Assert.equal(history[0].date.getTime(), now.getTime());

  info("Remove the first visit.");
  historyUpdated = PromiseUtils.defer();
  await PlacesUtils.history.remove("https://www.example.com/");
  await historyUpdated.promise;
  Assert.equal(history.length, 1);
  Assert.equal(history[0].url, "https://example.net/");

  info("Remove all visits.");
  historyUpdated = PromiseUtils.defer();
  await PlacesUtils.history.clear();
  await historyUpdated.promise;
  Assert.equal(history.length, 0);
  placesQuery.close();
});

add_task(async function test_filter_visits_by_age() {
  const placesQuery = new PlacesQuery();
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
  const history = await placesQuery.getHistory({ daysOld: 1 });
  Assert.equal(history.length, 1, "The older visit should be excluded.");
  Assert.equal(history[0].url, "https://example.net/");
  await PlacesUtils.history.clear();
  placesQuery.close();
});

add_task(async function test_filter_redirecting_visits() {
  const placesQuery = new PlacesQuery();
  await PlacesUtils.history.insertMany([
    {
      url: "http://google.com/",
      visits: [{ transition: PlacesUtils.history.TRANSITIONS.TYPED }],
    },
    {
      url: "https://www.google.com/",
      visits: [
        {
          transition: PlacesUtils.history.TRANSITIONS.REDIRECT_PERMANENT,
          referrer: "http://google.com/",
        },
      ],
    },
  ]);
  const history = await placesQuery.getHistory();
  Assert.equal(history.length, 1, "Redirecting visits should be excluded.");
  Assert.equal(history[0].url, "https://www.google.com/");
  await PlacesUtils.history.clear();
  placesQuery.close();
});
