/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gResultListener = {
  _lastResult: null,
  _lastValue: "",

  onValueRemoved(aResult, aValue) {
    this._lastResult = aResult;
    this._lastValue = aValue;
  },
};

// main
function run_test() {
  var result = Cc["@mozilla.org/autocomplete/simple-result;1"].createInstance(
    Ci.nsIAutoCompleteSimpleResult
  );
  result.appendMatch("a", "");
  result.appendMatch("b", "");
  result.appendMatch("c", "");
  result.setListener(gResultListener);
  Assert.equal(result.matchCount, 3);
  result.removeValueAt(0);
  Assert.equal(result.matchCount, 2);
  Assert.equal(gResultListener._lastResult, result);
  Assert.equal(gResultListener._lastValue, "a");

  result.removeValueAt(0);
  Assert.equal(result.matchCount, 1);
  Assert.equal(gResultListener._lastValue, "b");

  // check that we don't get notified if the listener is unset
  result.setListener(null);
  result.removeValueAt(0); // "c"
  Assert.equal(result.matchCount, 0);
  Assert.equal(gResultListener._lastValue, "b");
}
