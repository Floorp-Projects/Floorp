/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const Cu = Components.utils;
const { require } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});

Object.defineProperty(this, "NetworkHelper", {
  get: function() {
    return require("devtools/toolkit/webconsole/network-helper");
  },
  configurable: true,
  writeable: false,
  enumerable: true
});

function run_test() {
  test_isTextMimeType();
}

function test_isTextMimeType () {
  do_check_eq(NetworkHelper.isTextMimeType("text/plain"), true);
  do_check_eq(NetworkHelper.isTextMimeType("application/javascript"), true);
  do_check_eq(NetworkHelper.isTextMimeType("application/json"), true);
  do_check_eq(NetworkHelper.isTextMimeType("text/css"), true);
  do_check_eq(NetworkHelper.isTextMimeType("text/html"), true);
  do_check_eq(NetworkHelper.isTextMimeType("image/svg+xml"), true);
  do_check_eq(NetworkHelper.isTextMimeType("application/xml"), true);

  // Test custom JSON subtype
  do_check_eq(NetworkHelper.isTextMimeType("application/vnd.tent.posts-feed.v0+json"), true);
  do_check_eq(NetworkHelper.isTextMimeType("application/vnd.tent.posts-feed.v0-json"), true);
  // Test custom XML subtype
  do_check_eq(NetworkHelper.isTextMimeType("application/vnd.tent.posts-feed.v0+xml"), true);
  do_check_eq(NetworkHelper.isTextMimeType("application/vnd.tent.posts-feed.v0-xml"), false);
  // Test case-insensitive
  do_check_eq(NetworkHelper.isTextMimeType("application/vnd.BIG-CORP+json"), true);
  // Test non-text type
  do_check_eq(NetworkHelper.isTextMimeType("image/png"), false);
  // Test invalid types
  do_check_eq(NetworkHelper.isTextMimeType("application/foo-+json"), false);
  do_check_eq(NetworkHelper.isTextMimeType("application/-foo+json"), false);
  do_check_eq(NetworkHelper.isTextMimeType("application/foo--bar+json"), false);

  // Test we do not cause internal errors with unoptimized regex. Bug 961097
  do_check_eq(NetworkHelper.isTextMimeType("application/vnd.google.safebrowsing-chunk"), false);
}
