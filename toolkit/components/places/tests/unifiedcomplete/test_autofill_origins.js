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
