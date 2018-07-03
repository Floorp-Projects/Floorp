/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

addAutofillTasks(true);

// "example.com/" should match http://example.com/.  i.e., the search string
// should be treated as if it didn't have the trailing slash.
add_task(async function trailingSlash() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com/",
  }]);
  await check_autocomplete({
    search: "example.com/",
    autofilled: "example.com/",
    completed: "http://example.com/",
    matches: [{
      value: "example.com/",
      comment: "example.com/",
      style: ["autofill", "heuristic"],
    }],
  });
  await cleanup();
});

// "example.com/" should match http://www.example.com/.  i.e., the search string
// should be treated as if it didn't have the trailing slash.
add_task(async function trailingSlashWWW() {
  await PlacesTestUtils.addVisits([{
    uri: "http://www.example.com/",
  }]);
  await check_autocomplete({
    search: "example.com/",
    autofilled: "example.com/",
    completed: "http://www.example.com/",
    matches: [{
      value: "example.com/",
      comment: "www.example.com/",
      style: ["autofill", "heuristic"],
    }],
  });
  await cleanup();
});

// "ex" should match http://example.com:8888/, and the port should be completed.
add_task(async function port() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com:8888/",
  }]);
  await check_autocomplete({
    search: "ex",
    autofilled: "example.com:8888/",
    completed: "http://example.com:8888/",
    matches: [{
      value: "example.com:8888/",
      comment: "example.com:8888",
      style: ["autofill", "heuristic"],
    }],
  });
  await cleanup();
});

// "example.com:8" should match http://example.com:8888/, and the port should
// be completed.
add_task(async function portPartial() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com:8888/",
  }]);
  await check_autocomplete({
    search: "example.com:8",
    autofilled: "example.com:8888/",
    completed: "http://example.com:8888/",
    matches: [{
      value: "example.com:8888/",
      comment: "example.com:8888",
      style: ["autofill", "heuristic"],
    }],
  });
  await cleanup();
});

// "example.com:89" should *not* match http://example.com:8888/.
add_task(async function portNoMatch1() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com:8888/",
  }]);
  await check_autocomplete({
    search: "example.com:89",
    matches: [],
  });
  await cleanup();
});

// "example.com:9" should *not* match http://example.com:8888/.
add_task(async function portNoMatch2() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com:8888/",
  }]);
  await check_autocomplete({
    search: "example.com:9",
    matches: [],
  });
  await cleanup();
});

// "example/" should *not* match http://example.com/.
add_task(async function trailingSlash() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com/",
  }]);
  await check_autocomplete({
    search: "example/",
    matches: [],
  });
  await cleanup();
});

// multi.dotted.domain, search up to dot.
add_task(async function multidotted() {
  await PlacesTestUtils.addVisits([{
    uri: "http://www.example.co.jp:8888/",
  }]);
  await check_autocomplete({
    search: "www.example.co.",
    completed: "http://www.example.co.jp:8888/",
    matches: [{
      value: "www.example.co.jp:8888/",
      comment: "www.example.co.jp:8888",
      style: ["autofill", "heuristic"],
    }],
  });
  await cleanup();
});

// When determining which origins should be autofilled, all the origins sharing
// a host should be added together to get their combined frecency -- i.e.,
// prefixes should be collapsed.  And then from that list, the origin with the
// highest frecency should be chosen.
add_task(async function groupByHost() {
  // Add some visits to the same host, example.com.  Add one http and two https
  // so that https has a higher frecency and is therefore the origin that should
  // be autofilled.  Also add another origin that has a higher frecency than
  // both so that alone, neither http nor https would be autofilled, but added
  // together they should be.
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },

    { uri: "https://example.com/" },
    { uri: "https://example.com/" },

    { uri: "https://mozilla.org/" },
    { uri: "https://mozilla.org/" },
    { uri: "https://mozilla.org/" },
  ]);

  let httpFrec = frecencyForUrl("http://example.com/");
  let httpsFrec = frecencyForUrl("https://example.com/");
  let otherFrec = frecencyForUrl("https://mozilla.org/");
  Assert.ok(httpFrec < httpsFrec, "Sanity check");
  Assert.ok(httpsFrec < otherFrec, "Sanity check");

  // Compute the autofill threshold.
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`
    SELECT
      IFNULL((SELECT value FROM moz_meta WHERE key = "origin_frecency_count"), 0),
      IFNULL((SELECT value FROM moz_meta WHERE key = "origin_frecency_sum"), 0),
      IFNULL((SELECT value FROM moz_meta WHERE key = "origin_frecency_sum_of_squares"), 0)
  `);
  let count = rows[0].getResultByIndex(0);
  let sum = rows[0].getResultByIndex(1);
  let squares = rows[0].getResultByIndex(2);
  let threshold =
    (sum / count) + Math.sqrt((squares - ((sum * sum) / count)) / count);

  // Make sure the frecencies of the three origins are as expected in relation
  // to the threshold.
  Assert.ok(httpFrec < threshold, "http origin should be < threshold");
  Assert.ok(httpsFrec < threshold, "https origin should be < threshold");
  Assert.ok(threshold <= otherFrec, "Other origin should cross threshold");

  Assert.ok(threshold <= httpFrec + httpsFrec,
            "http and https origin added together should cross threshold");

  // The https origin should be autofilled.
  await check_autocomplete({
    search: "ex",
    autofilled: "example.com/",
    completed: "https://example.com/",
    matches: [
      {
        value: "example.com/",
        comment: "https://example.com",
        style: ["autofill", "heuristic"],
      },
      {
        value: "http://example.com/",
        comment: "test visit for http://example.com/",
        style: ["favicon"],
      },
    ],
  });

  await cleanup();
});
