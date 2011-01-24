/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Dietrich Ayala <dietrich@mozilla.com>
 *  Dan Mills <thunder@mozilla.com>
 *  Seth Spitzer <sspitzer@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
