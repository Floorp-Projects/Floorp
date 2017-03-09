/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Ci = Components.interfaces;
var Cr = Components.results;
var Cc = Components.classes;
var CC = Components.Constructor;

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

function test_nsiarrayextensions() {
  // Tests to check that the extensions that make an nsArray act like an
  // nsISupportsArray for iteration purposes works.
  // Note: we do not want to QI here, just want to make sure the magic glue
  // works as a drop-in replacement.

  let fake_nsisupports_array = create_n_element_array(5);

  // Check that |Count| works.
  do_check_eq(5, fake_nsisupports_array.Count());

  for (let i = 0; i < fake_nsisupports_array.Count(); i++) {
    // Check that the generic |GetElementAt| works.
    let elm = fake_nsisupports_array.GetElementAt(i);
    do_check_neq(elm, null);
    let str = elm.QueryInterface(Ci.nsISupportsString);
    do_check_neq(str, null);
    do_check_eq(str.data, "element " + i);
  }
}

var tests = [
  test_appending_null_actually_inserts,
  test_object_gets_appended,
  test_insert_at_beginning,
  test_replace_element,
  test_clear,
  test_enumerate,
  test_nsiarrayextensions,
];

function run_test() {
  for (var i = 0; i < tests.length; i++)
    tests[i]();
}
