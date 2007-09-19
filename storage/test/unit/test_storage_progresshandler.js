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

// This file tests the custom progress handlers

function setup()
{
  var msc = getOpenedDatabase();
  msc.createTable("handler_tests", "id INTEGER PRIMARY KEY, num INTEGER");
  msc.beginTransaction();

  var stmt = createStatement("INSERT INTO handler_tests (id, num) VALUES(?1, ?2)");
  for(var i = 0; i < 100; ++i) {
    stmt.bindInt32Parameter(0, i);
    stmt.bindInt32Parameter(1, Math.floor(Math.random()*1000));
    stmt.execute();
  }
  stmt.reset();
  msc.commitTransaction();
  stmt.finalize();
}

var testProgressHandler = {
  calls: 0,
  abort: false,

  onProgress: function(comm) {
    ++this.calls;
    return this.abort;
  }
};

function test_handler_registration()
{
  var msc = getOpenedDatabase();
  msc.setProgressHandler(10, testProgressHandler);
}

function test_handler_return()
{
  var msc = getOpenedDatabase();
  var oldH = msc.setProgressHandler(5, testProgressHandler);
  do_check_true(oldH instanceof Ci.mozIStorageProgressHandler);
}

function test_handler_removal()
{
  var msc = getOpenedDatabase();
  msc.removeProgressHandler();
  var oldH = msc.removeProgressHandler();
  do_check_eq(oldH, null);
}

function test_handler_call()
{
  var msc = getOpenedDatabase();
  msc.setProgressHandler(50, testProgressHandler);
  // Some long-executing request
  var stmt = createStatement(
    "SELECT SUM(t1.num * t2.num) FROM handler_tests AS t1, handler_tests AS t2");
  while(stmt.executeStep());
  do_check_true(testProgressHandler.calls > 0);
  stmt.finalize();
}

function test_handler_abort()
{
  var msc = getOpenedDatabase();
  testProgressHandler.abort = true;
  msc.setProgressHandler(50, testProgressHandler);
  // Some long-executing request
  var stmt = createStatement(
    "SELECT SUM(t1.num * t2.num) FROM handler_tests AS t1, handler_tests AS t2");

  const SQLITE_INTERRUPT = 9;
  try {
    while(stmt.executeStep());
    do_throw("We shouldn't get here!");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_ABORT, e.result);
    do_check_eq(SQLITE_INTERRUPT, msc.lastError);
  }
  try {
    stmt.finalize();
    do_throw("We shouldn't get here!");
  } catch (e) {
    // finalize should return the error code since we encountered an error
    do_check_eq(Cr.NS_ERROR_ABORT, e.result);
    do_check_eq(SQLITE_INTERRUPT, msc.lastError);
  }
}

var tests = [test_handler_registration, test_handler_return,
             test_handler_removal, test_handler_call,
             test_handler_abort];

function run_test()
{
  setup();

  for (var i = 0; i < tests.length; i++) {
    tests[i]();
  }

  cleanup();
}
