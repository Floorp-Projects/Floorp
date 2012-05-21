/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the custom aggregate functions

var testNums = [1, 2, 3, 4];

function setup()
{
  getOpenedDatabase().createTable("function_tests", "id INTEGER PRIMARY KEY");

  var stmt = createStatement("INSERT INTO function_tests (id) VALUES(?1)");
  for(var i = 0; i < testNums.length; ++i) {
    stmt.bindByIndex(0, testNums[i]);
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
