/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

var setID=0, setValue=0, getID=0, getValue=0;
function Wallet_ColumnSort(columnPosition, childrenName)
{
  //var startTime = new Date();

  // determine if sort is to be ascending or descending
  //    it is ascending if it is the first sort for this tree
  //    it is ascending if last sort was on a different column
  //    else it is the opposite of whatever the last sort for this column was
  var children = document.getElementById(childrenName);
  var lastColumnPosition = children.getAttribute('lastColumnPosition');
  var ascending = true;
  if (columnPosition == lastColumnPosition) {
    ascending = (children.getAttribute('lastAscending') != 'true');
  } else {
    children.setAttribute('lastColumnPosition', columnPosition);
  }
  children.setAttribute('lastAscending', ascending);
  bubbleSort(columnPosition, ascending, children);
  //dump("bubble sort time="+((new Date())-startTime)+"\n");
  return true;
}

// XXX we would like to use Array.prototype.sort, but the DOM doesn't let it
// XXX swap elements using property sets
function bubbleSort(columnPosition, ascending, children)
{
  var a = children.childNodes;
  var n = a.length, m = n - 1;

  // for efficiencey, read all the value attributes only once and store in an array
  var keys = [];
  for (var x=0; x<n; x++){
    var keyCell = a[x].firstChild.childNodes[columnPosition];
    keys[x] = keyCell.getAttribute('value');
  }

  for (var i = 0; i < m; i++) {
    var key = keys[i];
    var winner = -1;
    for (var j = i + 1; j < n; j++) {
      var nextKey = keys[j];
      if (ascending ? key > nextKey : key < nextKey) {
        key = nextKey;
        winner = j;
      }
    }
    if (winner != -1){

      // get the corresponding menuitems
      var item = a[i];
      var row = item.firstChild;
      var nextItem = a[winner];
      var nextRow = nextItem.firstChild;

      // swap the row values and the id's of the corresponding menuitems
      var temp = item.getAttribute('id');
      item.setAttribute('id', nextItem.getAttribute('id'));
      nextItem.setAttribute('id', temp);

      var cell = row.firstChild;
      var nextCell = nextRow.firstChild;
      var position = 0;
      while (cell) {
        var value_i;
        var value_winner;
        if (position == columnPosition) {
          value_i = keys[i];
          value_winner = keys[winner];
          keys[i] = value_winner;
          keys[winner] = value_i;
        } else {
          value_i = cell.getAttribute('value');
          value_winner = nextCell.getAttribute('value');
        }
        cell.setAttribute('value', value_winner);
        nextCell.setAttribute('value', value_i);
        cell = cell.nextSibling;
        nextCell = nextCell.nextSibling;
        position++;
      }
    }
  }
}