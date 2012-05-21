/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  var testURI = uri("wyciwyg://nodontjudgeabookbyitscover");

  var placeID = histsvc.addVisit(testURI,
                                 Date.now() * 1000,
                                 null,
                                 histsvc.TRANSITION_LINK,
                                 false, // not redirect
                                 0);
  do_check_false(placeID > 0);

  // test codepath of docshell caller
  histsvc.QueryInterface(Ci.nsIGlobalHistory2);
  placeID = histsvc.addURI(testURI, false, false, null);
  do_check_false(placeID > 0);
}
