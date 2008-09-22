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
 * The Original Code is XPCOM tests.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
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

const Ci = Components.interfaces;
const Cr = Components.results;
const Cc = Components.classes;
const CC = Components.Constructor;

var MutableArray = CC("@mozilla.org/array;1", "nsIMutableArray");
var SupportsString = CC("@mozilla.org/supports-string;1", "nsISupportsString");

function create_n_element_array(n)
{
  var arr = new MutableArray();
  for (let i=0; i<n; i++) {
    let str = new SupportsString();
    str.data = "element " + i;
    arr.appendElement(str, false);
  }
  return arr;
}

function test_appending_null_actually_inserts()
{
  var arr = new MutableArray();
  do_check_eq(0, arr.length);
  arr.appendElement(null, false);
  do_check_eq(1, arr.length);
}

function test_object_gets_appended()
{
  var arr = new MutableArray();
  var str = new SupportsString();
  str.data = "hello";
  arr.appendElement(str, false);
  do_check_eq(1, arr.length);
  var obj = arr.queryElementAt(0, Ci.nsISupportsString);
  do_check_eq(str, obj);
}

function test_insert_at_beginning()
{
  var arr = create_n_element_array(5);
  // just a sanity check
  do_check_eq(5, arr.length);
  var str = new SupportsString();
  str.data = "hello";
  arr.insertElementAt(str, 0, false);
  do_check_eq(6, arr.length);
  var obj = arr.queryElementAt(0, Ci.nsISupportsString);
  do_check_eq(str, obj);
  // check the data of all the other objects
  for (let i=1; i<arr.length; i++) {
    let obj = arr.queryElementAt(i, Ci.nsISupportsString);
    do_check_eq("element " + (i-1), obj.data);
  }
}

function test_replace_element()
{
  var arr = create_n_element_array(5);
  // just a sanity check
  do_check_eq(5, arr.length);
  var str = new SupportsString();
  str.data = "hello";
  // replace first element
  arr.replaceElementAt(str, 0, false);
  do_check_eq(5, arr.length);
  var obj = arr.queryElementAt(0, Ci.nsISupportsString);
  do_check_eq(str, obj);
  // replace last element
  arr.replaceElementAt(str, arr.length - 1, false);
  do_check_eq(5, arr.length);
  obj = arr.queryElementAt(arr.length - 1, Ci.nsISupportsString);
  do_check_eq(str, obj);
  // replace after last element, should insert empty elements
  arr.replaceElementAt(str, 9, false);
  do_check_eq(10, arr.length);
  obj = arr.queryElementAt(9, Ci.nsISupportsString);
  do_check_eq(str, obj);
  // AFAIK there's no way to check the empty elements, since you can't QI them.
}

function test_clear()
{
  var arr = create_n_element_array(5);
  // just a sanity check
  do_check_eq(5, arr.length);
  arr.clear();
  do_check_eq(0, arr.length);
}

function test_enumerate()
{
  var arr = create_n_element_array(5);
  do_check_eq(5, arr.length);
  var en = arr.enumerate();
  var i = 0;
  while (en.hasMoreElements()) {
    let str = en.getNext();
    do_check_true(str instanceof Ci.nsISupportsString);
    do_check_eq(str.data, "element " + i);
    i++;
  }
  do_check_eq(arr.length, i);
}

var tests = [
  test_appending_null_actually_inserts,
  test_object_gets_appended,
  test_insert_at_beginning,
  test_replace_element,
  test_clear,
  test_enumerate,
];

function run_test() {
  for (var i = 0; i < tests.length; i++)
    tests[i]();
}
