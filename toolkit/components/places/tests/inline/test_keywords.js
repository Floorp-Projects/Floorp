/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_autocomplete_test([
  "Searching for non-keyworded entry should autoFill it",
  "moz",
  "mozilla.org/",
  function () {
    addBookmark({ url: "http://mozilla.org/test/" });
  }
]);

add_autocomplete_test([
  "Searching for keyworded entry should not autoFill it",
  "moz",
  "moz",
  function () {
    addBookmark({ url: "http://mozilla.org/test/", keyword: "moz" });
  }
]);

add_autocomplete_test([
  "Searching for more than keyworded entry should autoFill it",
  "mozi",
  "mozilla.org/",
  function () {
    addBookmark({ url: "http://mozilla.org/test/", keyword: "moz" });
  }
]);

add_autocomplete_test([
  "Searching for less than keyworded entry should autoFill it",
  "mo",
  "mozilla.org/",
  function () {
    addBookmark({ url: "http://mozilla.org/test/", keyword: "moz" });
  }
]);

add_autocomplete_test([
  "Searching for keyworded entry is case-insensitive",
  "MoZ",
  "MoZ",
  function () {
    addBookmark({ url: "http://mozilla.org/test/", keyword: "moz" });
  }
]);
