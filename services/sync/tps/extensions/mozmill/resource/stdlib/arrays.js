/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ['inArray', 'getSet', 'indexOf',
                        'remove', 'rindexOf', 'compare'];


function remove(array, from, to) {
  var rest = array.slice((to || from) + 1 || array.length);
  array.length = from < 0 ? array.length + from : from;

  return array.push.apply(array, rest);
}

function inArray(array, value) {
  for (var i in array) {
    if (value == array[i]) {
      return true;
    }
  }

  return false;
}

function getSet(array) {
  var narray = [];

  for (var i in array) {
    if (!inArray(narray, array[i])) {
      narray.push(array[i]);
    }
  }

  return narray;
}

function indexOf(array, v, offset) {
  for (var i in array) {
    if (offset == undefined || i >= offset) {
      if (!isNaN(i) && array[i] == v) {
        return new Number(i);
      }
    }
  }

  return -1;
}

function rindexOf (array, v) {
  var l = array.length;

  for (var i in array) {
    if (!isNaN(i)) {
      var i = new Number(i);
    }

    if (!isNaN(i) && array[l - i] == v) {
      return l - i;
    }
  }

  return -1;
}

function compare (array, carray) {
  if (array.length != carray.length) {
    return false;
  }

  for (var i in array) {
    if (array[i] != carray[i]) {
      return false;
    }
  }

  return true;
}
