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
 * The Original Code is Storage Test Code.
 *
 * The Initial Developer of the Original Code is
 *   Lev Serebryakov <lev@serebryakov.spb.ru>
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

// This file tests the custom aggregate functions

var testNums = [1, 2, 3, 4];

function setup()
{
  getOpenedDatabase().createTable("function_tests", "id INTEGER PRIMARY KEY");

  var stmt = createStatement("INSERT INTO function_tests (id) VALUES(?1)");
  for(var i = 0; i < testNums.length; ++i) {
    stmt.bindInt32Parameter(0, testNums[i]);
    stmt.execute();
  }
  stmt.reset();
  stmt.finalize();
}

var testSquareAndSumFunction = {
  calls: 0,
  _sas: 0,

  reset: function() {
    this.calls = 0;
    this._sas  = 0;
  },

  onStep: function(val) {
    ++this.calls;
    this._sas += val.getInt32(0) * val.getInt32(0);
  },

  onFinal: function() {
    var retval = this._sas;
    this._sas = 0; // Prepare for next group
    return retval;
  }
};

function test_aggregate_registration()
{
  var msc = getOpenedDatabase();
  msc.createAggregateFunction("test_sas_aggr", 1, testSquareAndSumFunction);
}

function test_aggregate_no_double_registration()
{
  var msc = getOpenedDatabase();
  try {
    msc.createAggregateFunction("test_sas_aggr", 2, testSquareAndSumFunction);
    do_throw("We shouldn't get here!");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_FAILURE, e.result);
  }
}

function test_aggregate_removal()
{
  var msc = getOpenedDatabase();
  msc.removeFunction("test_sas_aggr");
  // Should be Ok now
  msc.createAggregateFunction("test_sas_aggr", 1, testSquareAndSumFunction);
}

function test_aggregate_no_aliases()
{
  var msc = getOpenedDatabase();
  try {
    msc.createAggregateFunction("test_sas_aggr2", 1, testSquareAndSumFunction);
    do_throw("We shouldn't get here!");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_FAILURE, e.result);
  }
}

function test_aggregate_call()
{
  var stmt = createStatement("SELECT test_sas_aggr(id) FROM function_tests");
  while(stmt.executeStep());
  do_check_eq(testNums.length, testSquareAndSumFunction.calls);
  testSquareAndSumFunction.reset();
  stmt.finalize();
}

function test_aggregate_result()
{
  var sas = 0;
  for(var i = 0; i < testNums.length; ++i) {
    sas += testNums[i] * testNums[i];
  }
  var stmt = createStatement("SELECT test_sas_aggr(id) FROM function_tests");
  stmt.executeStep();
  do_check_eq(sas, stmt.getInt32(0));
  testSquareAndSumFunction.reset();
  stmt.finalize();
}

var tests = [test_aggregate_registration, test_aggregate_no_double_registration,
             test_aggregate_removal, test_aggregate_no_aliases, test_aggregate_call,
             test_aggregate_result];

function run_test()
{
  setup();

  for (var i = 0; i < tests.length; i++) {
    tests[i]();
  }

  cleanup();
}
