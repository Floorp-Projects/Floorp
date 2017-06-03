/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/◦
*/

"use strict"

function checkValue(uri, id, attr, reference) {
  let value = XULStore.getValue(uri, id, attr);
  do_check_eq(value, reference);
}

function checkValueExists(uri, id, attr, exists) {
  do_check_eq(XULStore.hasValue(uri, id, attr), exists);
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
  do_check_true(a.toString() == b.toString());
}

