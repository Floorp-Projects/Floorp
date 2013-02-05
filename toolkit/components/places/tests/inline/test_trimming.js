/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_autocomplete_test([
  "Searching for untrimmed https://www entry",
  "mo",
  { autoFilled: "mozilla.org/", completed: "https://www.mozilla.org/" },
  function () {
    addBookmark({ url: "https://www.mozilla.org/test/" });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed https://www entry with path",
  "mozilla.org/t",
  { autoFilled: "mozilla.org/test/", completed: "https://www.mozilla.org/test/" },
  function () {
    addBookmark({ url: "https://www.mozilla.org/test/" });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed https:// entry",
  "mo",
  { autoFilled: "mozilla.org/", completed: "https://mozilla.org/" },
  function () {
    addBookmark({ url: "https://mozilla.org/test/" });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed https:// entry with path",
  "mozilla.org/t",
  { autoFilled: "mozilla.org/test/", completed: "https://mozilla.org/test/" },
  function () {
    addBookmark({ url: "https://mozilla.org/test/" });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed http://www entry",
  "mo",
  { autoFilled: "mozilla.org/", completed: "www.mozilla.org/" },
  function () {
    addBookmark({ url: "http://www.mozilla.org/test/" });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed http://www entry with path",
  "mozilla.org/t",
  { autoFilled: "mozilla.org/test/", completed: "http://www.mozilla.org/test/" },
  function () {
    addBookmark({ url: "http://www.mozilla.org/test/" });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed ftp:// entry",
  "mo",
  { autoFilled: "mozilla.org/", completed: "ftp://mozilla.org/" },
  function () {
    addBookmark({ url: "ftp://mozilla.org/test/" });
  },
]);

add_autocomplete_test([
  "Searching for untrimmed ftp:// entry with path",
  "mozilla.org/t",
  { autoFilled: "mozilla.org/test/", completed: "ftp://mozilla.org/test/" },
  function () {
    addBookmark({ url: "ftp://mozilla.org/test/" });
  },
]);

add_autocomplete_test([
  "Ensuring correct priority 1",
  "mo",
  { autoFilled: "mozilla.org/", completed: "https://www.mozilla.org/" },
  function () {
    addBookmark({ url: "https://www.mozilla.org/test/" });
    addBookmark({ url: "https://mozilla.org/test/" });
    addBookmark({ url: "ftp://mozilla.org/test/" });
    addBookmark({ url: "http://www.mozilla.org/test/" });
    addBookmark({ url: "http://mozilla.org/test/" });
  },
]);

add_autocomplete_test([
  "Ensuring correct priority 2",
  "mo",
  { autoFilled: "mozilla.org/", completed: "https://mozilla.org/" },
  function () {
    addBookmark({ url: "https://mozilla.org/test/" });
    addBookmark({ url: "ftp://mozilla.org/test/" });
    addBookmark({ url: "http://www.mozilla.org/test/" });
    addBookmark({ url: "http://mozilla.org/test/" });
  },
]);

add_autocomplete_test([
  "Ensuring correct priority 3",
  "mo",
  { autoFilled: "mozilla.org/", completed: "ftp://mozilla.org/" },
  function () {
    addBookmark({ url: "ftp://mozilla.org/test/" });
    addBookmark({ url: "http://www.mozilla.org/test/" });
    addBookmark({ url: "http://mozilla.org/test/" });
  },
]);

add_autocomplete_test([
  "Ensuring correct priority 4",
  "mo",
  { autoFilled: "mozilla.org/", completed: "www.mozilla.org/" },
  function () {
    addBookmark({ url: "http://www.mozilla.org/test/" });
    addBookmark({ url: "http://mozilla.org/test/" });
  },
]);

add_autocomplete_test([
  "Ensuring longer domain can't match",
  "mo",
  { autoFilled: "mozilla.co/", completed: "mozilla.co/" },
  function () {
    // The .co should be preferred, but should not get the https from the .com.
    // The .co domain must be added later to activate the trigger bug.
    addBookmark({ url: "https://mozilla.com/" });
    addBookmark({ url: "http://mozilla.co/" });
    addBookmark({ url: "http://mozilla.co/" });
  },
]);

add_autocomplete_test([
  "Searching for URL with characters that are normally escaped",
  "https://www.mozilla.org/啊-test",
  { autoFilled: "https://www.mozilla.org/啊-test", completed: "https://www.mozilla.org/啊-test" },
  function () {
    promiseAddVisits({ uri: NetUtil.newURI("https://www.mozilla.org/啊-test"),
                       transition: TRANSITION_TYPED });
  },
]);

add_autocomplete_test([
  "Don't return unsecure URL when searching for secure ones",
  "https://test.moz.org/t",
  { autoFilled: "https://test.moz.org/test/", completed: "https://test.moz.org/test/" },
  function () {
    promiseAddVisits({ uri: NetUtil.newURI("http://test.moz.org/test/"),
                       transition: TRANSITION_TYPED });
  },
]);

add_autocomplete_test([
  "Don't return unsecure domain when searching for secure ones",
  "https://test.moz",
  { autoFilled: "https://test.moz.org/", completed: "https://test.moz.org/" },
  function () {
    promiseAddVisits({ uri: NetUtil.newURI("http://test.moz.org/test/"),
                       transition: TRANSITION_TYPED });
  },
]);
