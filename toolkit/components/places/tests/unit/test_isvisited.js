/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test()
{
  run_next_test();
}

add_task(function test_execute()
{
  var referrer = uri("about:blank");

  // add a http:// uri 
  var uri1 = uri("http://mozilla.com");
  yield PlacesTestUtils.addVisits({uri: uri1, referrer: referrer});
  do_check_guid_for_uri(uri1);
  do_check_true(yield promiseIsURIVisited(uri1));
 
  // add a https:// uri
  var uri2 = uri("https://etrade.com");
  yield PlacesTestUtils.addVisits({uri: uri2, referrer: referrer});
  do_check_guid_for_uri(uri2);
  do_check_true(yield promiseIsURIVisited(uri2));

  // add a ftp:// uri
  var uri3 = uri("ftp://ftp.mozilla.org");
  yield PlacesTestUtils.addVisits({uri: uri3, referrer: referrer});
  do_check_guid_for_uri(uri3);
  do_check_true(yield promiseIsURIVisited(uri3));

  // check if a nonexistent uri is visited
  var uri4 = uri("http://foobarcheese.com");
  do_check_false(yield promiseIsURIVisited(uri4));

  // check that certain schemes never show up as visited
  // even if we attempt to add them to history
  // see CanAddURI() in nsNavHistory.cpp
  const URLS = [
    "about:config",
    "imap://cyrus.andrew.cmu.edu/archive.imap",
    "news://new.mozilla.org/mozilla.dev.apps.firefox",
    "mailbox:Inbox",
    "moz-anno:favicon:http://mozilla.org/made-up-favicon",
    "view-source:http://mozilla.org",
    "chrome://browser/content/browser.xul",
    "resource://gre-resources/hiddenWindow.html",
    "data:,Hello%2C%20World!",
    "wyciwyg:/0/http://mozilla.org",
    "javascript:alert('hello wolrd!');",
  ];
  for (let currentURL of URLS) {
    try {
      var cantAddUri = uri(currentURL);
    }
    catch(e) {
      // nsIIOService.newURI() can throw if e.g. our app knows about imap://
      // but the account is not set up and so the URL is invalid for us.
      // Note this in the log but ignore as it's not the subject of this test.
      do_print("Could not construct URI for '" + currentURL + "'; ignoring");
    }
    if (cantAddUri) {
      try {
        yield PlacesTestUtils.addVisits({uri: cantAddUri, referrer: referrer});
        do_throw("Should have generated an exception.");
      } catch(ex if ex && ex.result == Cr.NS_ERROR_ILLEGAL_VALUE) {
      }
      do_check_false(yield promiseIsURIVisited(cantAddUri));
    }
  }
});

