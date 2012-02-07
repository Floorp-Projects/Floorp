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
 * The Original Code is Bug 330578 unit test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Asaf Romano <mano@mozilla.com>
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

var gResultListener = {
  _lastResult: null,
  _lastValue: "",
  _lastRemoveFromDb: false,

  onValueRemoved: function(aResult, aValue, aRemoveFromDb) {
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
