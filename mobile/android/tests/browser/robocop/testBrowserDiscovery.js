// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

// We use a global variable to track the <browser> where the tests are happening
var browser;

function setHandlerFunc(handler, test) {
  browser.addEventListener("DOMLinkAdded", function(event) {
    Services.tm.mainThread.dispatch(handler.bind(this, test), Ci.nsIThread.DISPATCH_NORMAL);
  }, {once: true});
}

add_test(function setup_browser() {
  let BrowserApp = Services.wm.getMostRecentWindow("navigator:browser").BrowserApp;
  do_register_cleanup(function cleanup() {
    BrowserApp.closeTab(BrowserApp.getTabForBrowser(browser));
  });

  let url = "http://mochi.test:8888/tests/robocop/link_discovery.html";
  browser = BrowserApp.addTab(url, { selected: true, parentId: BrowserApp.selectedTab.id }).browser;
  browser.addEventListener("load", function(event) {
    Services.tm.mainThread.dispatch(run_next_test, Ci.nsIThread.DISPATCH_NORMAL);
  }, {capture: true, once: true});
});

var searchDiscoveryTests = [
  { text: "rel search discovered" },
  { rel: "SEARCH", text: "rel is case insensitive" },
  { rel: "-search-", pass: false, text: "rel -search- not discovered" },
  { rel: "foo bar baz search quux", text: "rel may contain additional rels separated by spaces" },
  { href: "https://not.mozilla.com", text: "HTTPS ok" },
  { href: "ftp://not.mozilla.com", text: "FTP ok" },
  { href: "data:text/foo,foo", pass: false, text: "data URI not permitted" },
  { href: "javascript:alert(0)", pass: false, text: "JS URI not permitted" },
  { type: "APPLICATION/OPENSEARCHDESCRIPTION+XML", text: "type is case insensitve" },
  { type: " application/opensearchdescription+xml ", text: "type may contain extra whitespace" },
  { type: "application/opensearchdescription+xml; charset=utf-8", text: "type may have optional parameters (RFC2046)" },
  { type: "aapplication/opensearchdescription+xml", pass: false, text: "type should not be loosely matched" },
  { rel: "search search search", count: 1, text: "only one engine should be added" }
];

function execute_search_test(test) {
  if (browser.engines) {
    let matchCount = (!("count" in test) || browser.engines.length === test.count);
    let matchTitle = (test.title == browser.engines[0].title);
    ok(matchCount && matchTitle, test.text);
    browser.engines = null;
  } else {
    ok(!test.pass, test.text);
  }
  run_next_test();
}

function prep_search_test(test) {
  // Syncrhonously load the search service.
  Services.search.getVisibleEngines();

  setHandlerFunc(execute_search_test, test);

  let rel = test.rel || "search";
  let href = test.href || "http://so.not.here.mozilla.com/search.xml";
  let type = test.type || "application/opensearchdescription+xml";
  let title = test.title;
  if (!("pass" in test)) {
    test.pass = true;
  }

  let head = browser.contentDocument.getElementById("linkparent");
  let link = browser.contentDocument.createElement("link");
  link.rel = rel;
  link.href = href;
  link.type = type;
  link.title = title;
  head.appendChild(link);
}

var feedDiscoveryTests = [
  { text: "rel feed discovered" },
  { rel: "ALTERNATE", text: "rel is case insensitive" },
  { rel: "-alternate-", pass: false, text: "rel -alternate- not discovered" },
  { rel: "foo bar baz alternate quux", text: "rel may contain additional rels separated by spaces" },
  { href: "https://not.mozilla.com", text: "HTTPS ok" },
  { href: "ftp://not.mozilla.com", text: "FTP ok" },
  { href: "data:text/foo,foo", pass: false, text: "data URI not permitted" },
  { href: "javascript:alert(0)", pass: false, text: "JS URI not permitted" },
  { type: "application/rss+xml", text: "type can be RSS" },
  { type: "aPPliCAtion/RSS+xml", text: "type is case insensitve" },
  { type: " application/atom+xml ", text: "type may contain extra whitespace" },
  { type: "application/atom+xml; charset=utf-8", text: "type may have optional parameters (RFC2046)" },
  { type: "aapplication/atom+xml", pass: false, text: "type should not be loosely matched" },
  { rel: "alternate alternate alternate", count: 1, text: "only one feed should be added" }
];

function execute_feed_test(test) {
  if (browser.feeds) {
    let matchCount = (!("count" in test) || browser.feeds.length === test.count);
    let matchTitle = (test.title == browser.feeds[0].title);
    ok(matchCount && matchTitle, test.text);
    browser.feeds = null;
  } else {
    ok(!test.pass, test.text);
  }
  run_next_test();
}

function prep_feed_test(test) {
  setHandlerFunc(execute_feed_test, test);

  let rel = test.rel || "alternate";
  let href = test.href || "http://so.not.here.mozilla.com/feed.xml";
  let type = test.type || "application/atom+xml";
  let title = test.title;
  if (!("pass" in test)) {
    test.pass = true;
  }

  let head = browser.contentDocument.getElementById("linkparent");
  let link = browser.contentDocument.createElement("link");
  link.rel = rel;
  link.href = href;
  link.type = type;
  link.title = title;
  head.appendChild(link);
}

var searchTest;
while ((searchTest = searchDiscoveryTests.shift())) {
  let title = searchTest.title || searchDiscoveryTests.length;
  searchTest.title = title;
  add_test(prep_search_test.bind(this, searchTest));
}

var feedTest;
while ((feedTest = feedDiscoveryTests.shift())) {
  let title = feedTest.title || feedDiscoveryTests.length;
  feedTest.title = title;
  add_test(prep_feed_test.bind(this, feedTest));
}

run_next_test();
