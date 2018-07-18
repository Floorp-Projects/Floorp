/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/◦
*/

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/osfile.jsm");

var XULStore = null;
var browserURI = "chrome://browser/content/browser.xul";
var aboutURI = "about:config";

function run_test() {
  do_get_profile();
  run_next_test();
}

function checkValue(uri, id, attr, reference) {
  let value = XULStore.getValue(uri, id, attr);
  Assert.equal(value, reference);
}

function checkValueExists(uri, id, attr, exists) {
  Assert.equal(XULStore.hasValue(uri, id, attr), exists);
}

function getIDs(uri) {
  let it = XULStore.getIDsEnumerator(uri);
  let result = [];

  while (it.hasMore()) {
    let value = it.getNext();
    result.push(value);
  }

  result.sort();
  return result;
}

function getAttributes(uri, id) {
  let it = XULStore.getAttributeEnumerator(uri, id);

  let result = [];

  while (it.hasMore()) {
    let value = it.getNext();
    result.push(value);
  }

  result.sort();
  return result;
}

function checkArrays(a, b) {
  a.sort();
  b.sort();
  Assert.equal(a.toString(), b.toString());
}

add_task(async function setup() {
  // Set a value that a future test depends on manually
  XULStore = Services.xulStore;
  XULStore.setValue(browserURI, "main-window", "width", "994");
});

add_task(async function testTruncation() {
  let dos = Array(8192).join("~");
  // Long id names should trigger an exception
  Assert.throws(() => XULStore.setValue(browserURI, dos, "foo", "foo"), /NS_ERROR_ILLEGAL_VALUE/);

  // Long attr names should trigger an exception
  Assert.throws(() => XULStore.setValue(browserURI, "foo", dos, "foo"), /NS_ERROR_ILLEGAL_VALUE/);

  // Long values should be truncated
  XULStore.setValue(browserURI, "dos", "dos", dos);
  dos = XULStore.getValue(browserURI, "dos", "dos");
  Assert.ok(dos.length == 4096);
  XULStore.removeValue(browserURI, "dos", "dos");
});

add_task(async function testGetValue() {
  // Get non-existing property
  checkValue(browserURI, "side-window", "height", "");

  // Get existing property
  checkValue(browserURI, "main-window", "width", "994");
});

add_task(async function testHasValue() {
  // Check non-existing property
  checkValueExists(browserURI, "side-window", "height", false);

  // Check existing property
  checkValueExists(browserURI, "main-window", "width", true);
});

add_task(async function testSetValue() {
  // Set new attribute
  checkValue(browserURI, "side-bar", "width", "");
  XULStore.setValue(browserURI, "side-bar", "width", "1000");
  checkValue(browserURI, "side-bar", "width", "1000");
  checkArrays(["main-window", "side-bar"], getIDs(browserURI));
  checkArrays(["width"], getAttributes(browserURI, "side-bar"));

  // Modify existing property
  checkValue(browserURI, "side-bar", "width", "1000");
  XULStore.setValue(browserURI, "side-bar", "width", "1024");
  checkValue(browserURI, "side-bar", "width", "1024");
  checkArrays(["main-window", "side-bar"], getIDs(browserURI));
  checkArrays(["width"], getAttributes(browserURI, "side-bar"));

  // Add another attribute
  checkValue(browserURI, "side-bar", "height", "");
  XULStore.setValue(browserURI, "side-bar", "height", "1000");
  checkValue(browserURI, "side-bar", "height", "1000");
  checkArrays(["main-window", "side-bar"], getIDs(browserURI));
  checkArrays(["width", "height"], getAttributes(browserURI, "side-bar"));
});

add_task(async function testRemoveValue() {
  // Remove first attribute
  checkValue(browserURI, "side-bar", "width", "1024");
  XULStore.removeValue(browserURI, "side-bar", "width");
  checkValue(browserURI, "side-bar", "width", "");
  checkValueExists(browserURI, "side-bar", "width", false);
  checkArrays(["main-window", "side-bar"], getIDs(browserURI));
  checkArrays(["height"], getAttributes(browserURI, "side-bar"));

  // Remove second attribute
  checkValue(browserURI, "side-bar", "height", "1000");
  XULStore.removeValue(browserURI, "side-bar", "height");
  checkValue(browserURI, "side-bar", "height", "");
  checkArrays(["main-window"], getIDs(browserURI));

  // Removing an attribute that doesn't exists shouldn't fail
  XULStore.removeValue(browserURI, "main-window", "bar");

  // Removing from an id that doesn't exists shouldn't fail
  XULStore.removeValue(browserURI, "foo", "bar");

  // Removing from a document that doesn't exists shouldn't fail
  let nonDocURI = "chrome://example/content/other.xul";
  XULStore.removeValue(nonDocURI, "foo", "bar");

  // Remove all attributes in browserURI
  XULStore.removeValue(browserURI, "addon-bar", "collapsed");
  checkArrays([], getAttributes(browserURI, "addon-bar"));
  XULStore.removeValue(browserURI, "main-window", "width");
  XULStore.removeValue(browserURI, "main-window", "height");
  XULStore.removeValue(browserURI, "main-window", "screenX");
  XULStore.removeValue(browserURI, "main-window", "screenY");
  XULStore.removeValue(browserURI, "main-window", "sizemode");
  checkArrays([], getAttributes(browserURI, "main-window"));
  XULStore.removeValue(browserURI, "sidebar-title", "value");
  checkArrays([], getAttributes(browserURI, "sidebar-title"));
  checkArrays([], getIDs(browserURI));

  // Remove all attributes in aboutURI
  XULStore.removeValue(aboutURI, "prefCol", "ordinal");
  XULStore.removeValue(aboutURI, "prefCol", "sortDirection");
  checkArrays([], getAttributes(aboutURI, "prefCol"));
  XULStore.removeValue(aboutURI, "lockCol", "ordinal");
  checkArrays([], getAttributes(aboutURI, "lockCol"));
  checkArrays([], getIDs(aboutURI));
});
