/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_autocomplete_test([
  "Searching for cased entry 1",
  "MOZ",
  { autoFilled: "MOZilla.org/", completed: "mozilla.org/" },
  function () {
    PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/test/") });
  }
]);

add_autocomplete_test([
  "Searching for cased entry 2",
  "mozilla.org/T",
  { autoFilled: "mozilla.org/T", completed: "mozilla.org/T" },
  function () {
    PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/test/") });
  }
]);

add_autocomplete_test([
  "Searching for cased entry 3",
  "mozilla.org/T",
  { autoFilled: "mozilla.org/Test/", completed: "http://mozilla.org/Test/" },
  function () {
    PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/Test/") });
  }
]);

add_autocomplete_test([
  "Searching for cased entry 4",
  "mOzilla.org/t",
  { autoFilled: "mOzilla.org/t", completed: "mOzilla.org/t" },
  function () {
    PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/Test/") });
  },
]);

add_autocomplete_test([
  "Searching for cased entry 5",
  "mOzilla.org/T",
  { autoFilled: "mOzilla.org/Test/", completed: "http://mozilla.org/Test/" },
  function () {
    PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/Test/") });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed cased entry",
  "http://mOz",
  { autoFilled: "http://mOzilla.org/", completed: "http://mozilla.org/" },
  function () {
    PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/Test/") });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed cased entry with www",
  "http://www.mOz",
  { autoFilled: "http://www.mOzilla.org/", completed: "http://www.mozilla.org/" },
  function () {
    PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://www.mozilla.org/Test/") });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed cased entry with path",
  "http://mOzilla.org/t",
  { autoFilled: "http://mOzilla.org/t", completed: "http://mOzilla.org/t" },
  function () {
    PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/Test/") });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed cased entry with path 2",
  "http://mOzilla.org/T",
  { autoFilled: "http://mOzilla.org/Test/", completed: "http://mozilla.org/Test/" },
  function () {
    PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/Test/") });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed cased entry with www and path",
  "http://www.mOzilla.org/t",
  { autoFilled: "http://www.mOzilla.org/t", completed: "http://www.mOzilla.org/t" },
  function () {
    PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://www.mozilla.org/Test/") });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed cased entry with www and path 2",
  "http://www.mOzilla.org/T",
  { autoFilled: "http://www.mOzilla.org/Test/", completed: "http://www.mozilla.org/Test/" },
  function () {
    PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://www.mozilla.org/Test/") });
  },
]);
