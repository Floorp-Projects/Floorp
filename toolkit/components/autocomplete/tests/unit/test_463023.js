/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// main
function run_test() {
  var result = Cc["@mozilla.org/autocomplete/controller;1"].
               createInstance(Ci.nsIAutoCompleteController);
  do_check_eq(result.searchStatus, Ci.nsIAutoCompleteController.STATUS_NONE);
}
