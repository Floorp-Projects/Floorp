/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 et
 * ***** BEGIN LICENSE BLOCK *****
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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

/**
 * This file tests that the JS language helpers check their prototype chain for
 * most properties.
 */

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

function test_params_prototype()
{
  let stmt = getOpenedDatabase().createStatement(
    "SELECT * FROM sqlite_master"
  );

  // Set a property on the prototype and make sure it exist (will not be a
  // bindable parameter, however).
  stmt.params.__proto__.test = 2;
  do_check_eq(stmt.params.test, 2);
  stmt.finalize();
}

function test_row_prototype()
{
  // First, create a table so that we actually have data in sqlite_master.
  getOpenedDatabase().createTable("test_table", "id INTEGER PRIMARY KEY");

  let stmt = getOpenedDatabase().createStatement(
    "SELECT * FROM sqlite_master"
  );

  do_check_true(stmt.executeStep());

  // Set a property on the prototype and make sure it exists (will not be in the
  // results, however).
  stmt.row.__proto__.test = 2;
  do_check_eq(stmt.row.test, 2);
  stmt.finalize();
}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

let tests = [
  test_params_prototype,
  test_row_prototype,
];
function run_test()
{
  cleanup();

  // Run the tests.
  tests.forEach(function(test) test());
}
