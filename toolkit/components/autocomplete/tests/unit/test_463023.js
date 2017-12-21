/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// main
function run_test() {
  var result = Cc["@mozilla.org/autocomplete/controller;1"].
               createInstance(Ci.nsIAutoCompleteController);
  Assert.equal(result.searchStatus, Ci.nsIAutoCompleteController.STATUS_NONE);
}
