/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get history service
try {
  var gh = Cc["@mozilla.org/browser/global-history;2"].
           getService(Ci.nsIGlobalHistory2);
} catch(ex) {
  do_throw("Could not get the global history service\n");
} 

function add_uri_to_history(aURI, aCheckForGuid) {
  var referrer = uri("about:blank");
  gh.addURI(aURI,
            false, // not redirect
            true, // top level 
            referrer);
  if (aCheckForGuid === undefined) {
    do_check_guid_for_uri(aURI);
  }
}

// main
function run_test() {
  // add a http:// uri 
  var uri1 = uri("http://mozilla.com");
  add_uri_to_history(uri1);
  do_check_true(gh.isVisited(uri1));
 
  // add a https:// uri
  var uri2 = uri("https://etrade.com");
  add_uri_to_history(uri2);
  do_check_true(gh.isVisited(uri2));

  // add a ftp:// uri
  var uri3 = uri("ftp://ftp.mozilla.org");
  add_uri_to_history(uri3);
  do_check_true(gh.isVisited(uri3));

  // check if a nonexistent uri is visited
  var uri4 = uri("http://foobarcheese.com");
  do_check_false(gh.isVisited(uri4));

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
  URLS.forEach(function(currentURL) {
    try {
      var cantAddUri = uri(currentURL);
    }
    catch(e) {
      // nsIIOService.newURI() can throw if e.g. our app knows about imap://
      // but the account is not set up and so the URL is invalid for us.
      // Note this in the log but ignore as it's not the subject of this test.
      do_log_info("Could not construct URI for '" + currentURL + "'; ignoring");
    }
    if (cantAddUri) {
      add_uri_to_history(cantAddUri, false);
      do_check_false(gh.isVisited(cantAddUri));
    }
  });
}
