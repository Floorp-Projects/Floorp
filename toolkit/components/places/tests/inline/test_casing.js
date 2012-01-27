/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_autocomplete_test([
  "Searching for cased entry 1",
  "MOZ",
  { autoFilled: "MOZilla.org/", completed: "mozilla.org/" },
  function () {
    addBookmark({ url: "http://mozilla.org/test/" });
  }
]);

add_autocomplete_test([
  "Searching for cased entry 2",
  "mozilla.org/T",
  { autoFilled: "mozilla.org/Test/", completed: "mozilla.org/test/" },
  function () {
    addBookmark({ url: "http://mozilla.org/test/" });
  }
]);

add_autocomplete_test([
  "Searching for cased entry 3",
  "mOzilla.org/t",
  { autoFilled: "mOzilla.org/test/", completed: "mozilla.org/Test/" },
  function () {
    addBookmark({ url: "http://mozilla.org/Test/" });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed cased entry",
  "http://mOz",
  { autoFilled: "http://mOzilla.org/", completed: "http://mozilla.org/" },
  function () {
    addBookmark({ url: "http://mozilla.org/Test/" });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed cased entry with www",
  "http://www.mOz",
  { autoFilled: "http://www.mOzilla.org/", completed: "http://www.mozilla.org/" },
  function () {
    addBookmark({ url: "http://www.mozilla.org/Test/" });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed cased entry with path",
  "http://mOzilla.org/t",
  { autoFilled: "http://mOzilla.org/test/", completed: "http://mozilla.org/Test/" },
  function () {
    addBookmark({ url: "http://mozilla.org/Test/" });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed cased entry with www and path",
  "http://www.mOzilla.org/t",
  { autoFilled: "http://www.mOzilla.org/test/", completed: "http://www.mozilla.org/Test/" },
  function () {
    addBookmark({ url: "http://www.mozilla.org/Test/" });
  },
]);
