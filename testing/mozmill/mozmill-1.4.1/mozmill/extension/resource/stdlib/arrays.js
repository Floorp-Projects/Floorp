// ***** BEGIN LICENSE BLOCK *****// ***** BEGIN LICENSE BLOCK *****
// Version: MPL 1.1/GPL 2.0/LGPL 2.1
// 
// The contents of this file are subject to the Mozilla Public License Version
// 1.1 (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
// http://www.mozilla.org/MPL/
// 
// Software distributed under the License is distributed on an "AS IS" basis,
// WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
// for the specific language governing rights and limitations under the
// License.
// 
// The Original Code is Mozilla Corporation Code.
// 
// The Initial Developer of the Original Code is
// Mikeal Rogers.
// Portions created by the Initial Developer are Copyright (C) 2008
// the Initial Developer. All Rights Reserved.
// 
// Contributor(s):
//  Mikeal Rogers <mikeal.rogers@gmail.com>
// 
// Alternatively, the contents of this file may be used under the terms of
// either the GNU General Public License Version 2 or later (the "GPL"), or
// the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
// in which case the provisions of the GPL or the LGPL are applicable instead
// of those above. If you wish to allow use of your version of this file only
// under the terms of either the GPL or the LGPL, and not to allow others to
// use your version of this file under the terms of the MPL, indicate your
// decision by deleting the provisions above and replace them with the notice
// and other provisions required by the GPL or the LGPL. If you do not delete
// the provisions above, a recipient may use your version of this file under
// the terms of any one of the MPL, the GPL or the LGPL.
// 
// ***** END LICENSE BLOCK *****

var EXPORTED_SYMBOLS = ['inArray', 'getSet', 'indexOf', 'rindexOf', 'compare'];

function inArray (array, value) {
  for (i in array) {
    if (value == array[i]) {
      return true;
    }
  }
  return false;
}

function getSet (array) {
  var narray = [];
  for (i in array) {
    if ( !inArray(narray, array[i]) ) {
      narray.push(array[i]);
    } 
  }
  return narray;
}

function indexOf (array, v, offset) {
  for (i in array) {
    if (offset == undefined || i >= offset) {
      if ( !isNaN(i) && array[i] == v) {
        return new Number(i);
      }
    }
  }
  return -1;
}

function rindexOf (array, v) {
  var l = array.length;
  for (i in array) {
    if (!isNaN(i)) {
      var i = new Number(i)
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
  for (i in array) {
    if (array[i] != carray[i]) {
      return false;
    }
  }
  return true;
}