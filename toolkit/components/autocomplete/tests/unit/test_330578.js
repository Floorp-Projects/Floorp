/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gResultListener = {
  _lastResult: null,
  _lastValue: "",
  _lastRemoveFromDb: false,

  onValueRemoved(aResult, aValue, aRemoveFromDb) {
    this._lastResult = aResult;
    this._lastValue = aValue;
    this._lastRemoveFromDb = aRemoveFromDb;
  }
};


// main
function run_test() {
  var result = Cc["@mozilla.org/autocomplete/simple-result;1"].
               createInstance(Ci.nsIAutoCompleteSimpleResult);
  result.appendMatch("a", "");
  result.appendMatch("b", "");
  result.appendMatch("c", "");
  result.setListener(gResultListener);
  do_check_eq(result.matchCount, 3);
  result.removeValueAt(0, true);
  do_check_eq(result.matchCount, 2);
  do_check_eq(gResultListener._lastResult, result);
  do_check_eq(gResultListener._lastValue, "a");
  do_check_eq(gResultListener._lastRemoveFromDb, true);

  result.removeValueAt(0, false);
  do_check_eq(result.matchCount, 1);
  do_check_eq(gResultListener._lastValue, "b");
  do_check_eq(gResultListener._lastRemoveFromDb, false);

  // check that we don't get notified if the listener is unset
  result.setListener(null);
  result.removeValueAt(0, true); // "c"
  do_check_eq(result.matchCount, 0);
  do_check_eq(gResultListener._lastValue, "b");
}
