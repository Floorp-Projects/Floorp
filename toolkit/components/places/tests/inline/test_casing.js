/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_autocomplete_test([
  "Searching for cased entry",
  "MOZ",
  { autoFilled: "MOZilla.org/", completed: "mozilla.org/"},
  function () {
    addBookmark({ url: "http://mozilla.org/test/" });
  }
]);

add_autocomplete_test([
  "Searching for cased entry",
  "mozilla.org/T",
  { autoFilled: "mozilla.org/Test/", completed: "mozilla.org/test/"},
  function () {
    addBookmark({ url: "http://mozilla.org/test/" });
  }
]);

add_autocomplete_test([
  "Searching for cased entry",
  "mOzilla.org/t",
  { autoFilled: "mOzilla.org/test/", completed: "mozilla.org/Test/"},
  function () {
    addBookmark({ url: "http://mozilla.org/Test/" });
  },
]);
