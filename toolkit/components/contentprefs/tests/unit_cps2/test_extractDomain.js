/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  let tests = {
    "blob:": "blob:",
    "blob:https://chat.mozilla.org/35d6a992-6e18-4957-8216-070c53b9bc83":
      "chat.mozilla.org",
    "blob:resource://pdf.js/ed645567-3eea-4ff1-94fd-efb04812afe0":
      "resource://pdf.js",
    "http://example.com": "example.com",
    "http://example.com/": "example.com",
    "http://example.com/foo/bar/baz": "example.com",
    "http://subdomain.example.com/foo/bar/baz": "subdomain.example.com",
    "http://qix.quux.example.com/foo/bar/baz": "qix.quux.example.com",
    "file:///home/foo/bar": "file:///home/foo/bar",
    "not a url": "not a url",
  };
  let cps = Cc["@mozilla.org/content-pref/service;1"].getService(
    Ci.nsIContentPrefService2
  );
  for (let url in tests) {
    Assert.equal(cps.extractDomain(url), tests[url]);
  }
}
