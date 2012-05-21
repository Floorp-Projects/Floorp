/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Added with bug 508102 to make sure that calling stopSearch on our
 * AutoComplete implementation does not throw.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals and Constants

let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
          getService(Ci.nsINavHistoryService);
let ac = Cc["@mozilla.org/autocomplete/search;1?name=history"].
         getService(Ci.nsIAutoCompleteSearch);

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

function test_stopSearch()
{
  try {
    ac.stopSearch();
  }
  catch (e) {
    do_throw("we should not have caught anything!");
  }
}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

let tests = [
  test_stopSearch,
];
function run_test()
{
  tests.forEach(function(test) test());
}
