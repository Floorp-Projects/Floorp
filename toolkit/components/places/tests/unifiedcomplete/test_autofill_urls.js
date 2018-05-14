/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

addAutofillTasks(false);

// "example.com/foo/" should match http://example.com/foo/.
add_task(async function multipleSlashes() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com/foo/",
  }]);
  await check_autocomplete({
    search: "example.com/foo/",
    autofilled: "example.com/foo/",
    completed: "http://example.com/foo/",
    matches: [{
      value: "example.com/foo/",
      comment: "example.com/foo/",
      style: ["autofill", "heuristic"],
    }],
  });
  await cleanup();
});

// "example.com:8888/f" should match http://example.com:8888/foo.
add_task(async function port() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com:8888/foo",
  }]);
  await check_autocomplete({
    search: "example.com:8888/f",
    autofilled: "example.com:8888/foo",
    completed: "http://example.com:8888/foo",
    matches: [{
      value: "example.com:8888/foo",
      comment: "example.com:8888/foo",
      style: ["autofill", "heuristic"],
    }],
  });
  await cleanup();
});

// "example.com:8999/f" should *not* match http://example.com:8888/foo.
add_task(async function portNoMatch() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com:8888/foo",
  }]);
  await check_autocomplete({
    search: "example.com:8999/f",
    matches: [],
  });
  await cleanup();
});
