/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var MutableArray = CC("@mozilla.org/array;1", "nsIMutableArray");
var SupportsString = CC("@mozilla.org/supports-string;1", "nsISupportsString");

function create_n_element_array(n) {
  var arr = new MutableArray();
  for (let i = 0; i < n; i++) {
    let str = new SupportsString();
    str.data = "element " + i;
    arr.appendElement(str);
  }
  return arr;
}

function test_appending_null_actually_inserts() {
  var arr = new MutableArray();
  Assert.equal(0, arr.length);
  arr.appendElement(null);
  Assert.equal(1, arr.length);
}

function test_object_gets_appended() {
  var arr = new MutableArray();
  var str = new SupportsString();
  str.data = "hello";
  arr.appendElement(str);
  Assert.equal(1, arr.length);
  var obj = arr.queryElementAt(0, Ci.nsISupportsString);
  Assert.equal(str, obj);
}

function test_insert_at_beginning() {
  var arr = create_n_element_array(5);
  // just a sanity check
  Assert.equal(5, arr.length);
  var str = new SupportsString();
  str.data = "hello";
  arr.insertElementAt(str, 0);
  Assert.equal(6, arr.length);
  var obj = arr.queryElementAt(0, Ci.nsISupportsString);
  Assert.equal(str, obj);
  // check the data of all the other objects
  for (let i = 1; i < arr.length; i++) {
    let obj2 = arr.queryElementAt(i, Ci.nsISupportsString);
    Assert.equal("element " + (i - 1), obj2.data);
  }
}

function test_replace_element() {
  var arr = create_n_element_array(5);
  // just a sanity check
  Assert.equal(5, arr.length);
  var str = new SupportsString();
  str.data = "hello";
  // replace first element
  arr.replaceElementAt(str, 0);
  Assert.equal(5, arr.length);
  var obj = arr.queryElementAt(0, Ci.nsISupportsString);
  Assert.equal(str, obj);
  // replace last element
  arr.replaceElementAt(str, arr.length - 1);
  Assert.equal(5, arr.length);
  obj = arr.queryElementAt(arr.length - 1, Ci.nsISupportsString);
  Assert.equal(str, obj);
  // replace after last element, should insert empty elements
  arr.replaceElementAt(str, 9);
  Assert.equal(10, arr.length);
  obj = arr.queryElementAt(9, Ci.nsISupportsString);
  Assert.equal(str, obj);
  // AFAIK there's no way to check the empty elements, since you can't QI them.
}

function test_clear() {
  var arr = create_n_element_array(5);
  // just a sanity check
  Assert.equal(5, arr.length);
  arr.clear();
  Assert.equal(0, arr.length);
}

function test_enumerate() {
  var arr = create_n_element_array(5);
  Assert.equal(5, arr.length);
  var en = arr.enumerate();
  var i = 0;
  while (en.hasMoreElements()) {
    let str = en.getNext();
    Assert.ok(str instanceof Ci.nsISupportsString);
    Assert.equal(str.data, "element " + i);
    i++;
  }
  Assert.equal(arr.length, i);
}

function test_nsiarrayextensions() {
  // Tests to check that the extensions that make an nsArray act like an
  // nsISupportsArray for iteration purposes works.
  // Note: we do not want to QI here, just want to make sure the magic glue
  // works as a drop-in replacement.

  let fake_nsisupports_array = create_n_element_array(5);

  // Check that |Count| works.
  Assert.equal(5, fake_nsisupports_array.Count());

  for (let i = 0; i < fake_nsisupports_array.Count(); i++) {
    // Check that the generic |GetElementAt| works.
    let elm = fake_nsisupports_array.GetElementAt(i);
    Assert.notEqual(elm, null);
    let str = elm.QueryInterface(Ci.nsISupportsString);
    Assert.notEqual(str, null);
    Assert.equal(str.data, "element " + i);
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
