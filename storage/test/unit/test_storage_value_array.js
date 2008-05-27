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
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

// This file tests the functions of mozIStorageValueArray

function setup()
{
  getOpenedDatabase().createTable("test", "id INTEGER PRIMARY KEY, name TEXT," +
                                          "number REAL, nuller NULL, blobber BLOB");
  
  var stmt = createStatement("INSERT INTO test (name, number, blobber) " +
                             "VALUES (?1, ?2, ?3)");
  stmt.bindUTF8StringParameter(0, "foo");
  stmt.bindDoubleParameter(1, 2.34);
  stmt.bindBlobParameter(2, [], 0);
  stmt.execute();
  
  stmt.bindStringParameter(0, "");
  stmt.bindDoubleParameter(1, 1.23);
  stmt.bindBlobParameter(2, [1, 2], 2);
  stmt.execute();

  stmt.reset();
  stmt.finalize();
}

function test_getIsNull_for_null()
{
  var stmt = createStatement("SELECT nuller, blobber FROM test WHERE id = ?1");
  stmt.bindInt32Parameter(0, 1);
  do_check_true(stmt.executeStep());
  
  do_check_true(stmt.getIsNull(0)); // null field
  do_check_true(stmt.getIsNull(1)); // data is null if size is 0
  stmt.reset();
  stmt.finalize();
}

function test_getIsNull_for_non_null()
{
  var stmt = createStatement("SELECT name, blobber FROM test WHERE id = ?1");
  stmt.bindInt32Parameter(0, 2);
  do_check_true(stmt.executeStep());

  do_check_false(stmt.getIsNull(0));
  do_check_false(stmt.getIsNull(1));
  stmt.reset();
  stmt.finalize();
}

function test_value_type_null()
{
  var stmt = createStatement("SELECT nuller FROM test WHERE id = ?1");
  stmt.bindInt32Parameter(0, 1);
  do_check_true(stmt.executeStep());

  do_check_eq(Ci.mozIStorageValueArray.VALUE_TYPE_NULL,
              stmt.getTypeOfIndex(0));
  stmt.reset();
  stmt.finalize();
}

function test_value_type_integer()
{
  var stmt = createStatement("SELECT id FROM test WHERE id = ?1");
  stmt.bindInt32Parameter(0, 1);
  do_check_true(stmt.executeStep());

  do_check_eq(Ci.mozIStorageValueArray.VALUE_TYPE_INTEGER,
              stmt.getTypeOfIndex(0));
  stmt.reset();
  stmt.finalize();
}

function test_value_type_float()
{
  var stmt = createStatement("SELECT number FROM test WHERE id = ?1");
  stmt.bindInt32Parameter(0, 1);
  do_check_true(stmt.executeStep());

  do_check_eq(Ci.mozIStorageValueArray.VALUE_TYPE_FLOAT,
              stmt.getTypeOfIndex(0));
  stmt.reset();
  stmt.finalize();
}

function test_value_type_text()
{
  var stmt = createStatement("SELECT name FROM test WHERE id = ?1");
  stmt.bindInt32Parameter(0, 1);
  do_check_true(stmt.executeStep());

  do_check_eq(Ci.mozIStorageValueArray.VALUE_TYPE_TEXT,
              stmt.getTypeOfIndex(0));
  stmt.reset();
  stmt.finalize();
}

function test_value_type_blob()
{
  var stmt = createStatement("SELECT blobber FROM test WHERE id = ?1");
  stmt.bindInt32Parameter(0, 2);
  do_check_true(stmt.executeStep());

  do_check_eq(Ci.mozIStorageValueArray.VALUE_TYPE_BLOB,
              stmt.getTypeOfIndex(0));
  stmt.reset();
  stmt.finalize();
}

function test_numEntries_one()
{
  var stmt = createStatement("SELECT blobber FROM test WHERE id = ?1");
  stmt.bindInt32Parameter(0, 2);
  do_check_true(stmt.executeStep());

  do_check_eq(1, stmt.numEntries);
  stmt.reset();
  stmt.finalize();
}

function test_numEntries_all()
{
  var stmt = createStatement("SELECT * FROM test WHERE id = ?1");
  stmt.bindInt32Parameter(0, 2);
  do_check_true(stmt.executeStep());

  do_check_eq(5, stmt.numEntries);
  stmt.reset();
  stmt.finalize();
}

function test_getInt()
{
  var stmt = createStatement("SELECT id FROM test WHERE id = ?1");
  stmt.bindInt32Parameter(0, 2);
  do_check_true(stmt.executeStep());

  do_check_eq(2, stmt.getInt32(0));
  do_check_eq(2, stmt.getInt64(0));
  stmt.reset();
  stmt.finalize();
}

function test_getDouble()
{
  var stmt = createStatement("SELECT number FROM test WHERE id = ?1");
  stmt.bindInt32Parameter(0, 2);
  do_check_true(stmt.executeStep());

  do_check_eq(1.23, stmt.getDouble(0));
  stmt.reset();
  stmt.finalize();
}

function test_getUTF8String()
{
  var stmt = createStatement("SELECT name FROM test WHERE id = ?1");
  stmt.bindInt32Parameter(0, 1);
  do_check_true(stmt.executeStep());

  do_check_eq("foo", stmt.getUTF8String(0));
  stmt.reset();
  stmt.finalize();
}

function test_getString()
{
  var stmt = createStatement("SELECT name FROM test WHERE id = ?1");
  stmt.bindInt32Parameter(0, 2);
  do_check_true(stmt.executeStep());

  do_check_eq("", stmt.getString(0));
  stmt.reset();
  stmt.finalize();
}

function test_getBlob()
{
  var stmt = createStatement("SELECT blobber FROM test WHERE id = ?1");
  stmt.bindInt32Parameter(0, 2);
  do_check_true(stmt.executeStep());

  var count = { value: 0 };
  var arr = { value: null };
  stmt.getBlob(0, count, arr);
  do_check_eq(2, count.value);
  do_check_eq(1, arr.value[0]);
  do_check_eq(2, arr.value[1]);
  stmt.reset();
  stmt.finalize();
}

var tests = [test_getIsNull_for_null, test_getIsNull_for_non_null,
             test_value_type_null, test_value_type_integer,
             test_value_type_float, test_value_type_text, test_value_type_blob,
             test_numEntries_one, test_numEntries_all, test_getInt,
             test_getDouble, test_getUTF8String, test_getString, test_getBlob];

function run_test()
{
  setup();

  for (var i = 0; i < tests.length; i++)
    tests[i]();
    
  cleanup();
}

