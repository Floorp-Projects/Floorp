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

function DeleteAllFromOutliner
    (outliner, view, table, deletedTable, removeButton, removeAllButton) {

  // remove all items from table and place in deleted table
  for (var i=0; i<table.length; i++) {
    deletedTable[deletedTable.length] = table[i];
  }
  table.length = 0;

  // clear out selections
  outliner.outlinerBoxObject.view.selection.select(-1); 

  // redisplay
  view.rowCount = 0;
  outliner.outlinerBoxObject.invalidate();


  // disable buttons
  document.getElementById(removeButton).setAttribute("disabled", "true")
  document.getElementById(removeAllButton).setAttribute("disabled","true");
}

function DeleteSelectedItemFromOutliner
    (outliner, view, table, deletedTable, removeButton, removeAllButton) {

  // remove selected items from list (by setting them to null) and place in deleted list
  var selections = GetOutlinerSelections(outliner);
  for (var s=selections.length-1; s>= 0; s--) {
    var i = selections[s];
    deletedTable[deletedTable.length] = table[i];
    table[i] = null;
  }

  // collapse list by removing all the null entries
  for (var j=0; j<table.length; j++) {
    if (table[j] == null) {
      var k = j;
      while ((k < table.length) && (table[k] == null)) {
        k++;
      }
      table.splice(j, k-j);
    }
  }

  // redisplay
  var box = outliner.outlinerBoxObject;
  var firstRow = box.getFirstVisibleRow();
  view.rowCount = table.length;
  box.rowCountChanged(0, table.length);
  box.scrollToRow(firstRow)

  // update selection and/or buttons
  if (table.length) {

    // update selection
    // note: we need to deselect before reselecting in order to trigger ...Selected method
    var nextSelection = (selections[0] < table.length) ? selections[0] : table.length-1;
    outliner.outlinerBoxObject.view.selection.select(-1); 
    outliner.outlinerBoxObject.view.selection.select(nextSelection);

  } else {

    // disable buttons
    document.getElementById(removeButton).setAttribute("disabled", "true")
    document.getElementById(removeAllButton).setAttribute("disabled","true");

    // clear out selections
    outliner.outlinerBoxObject.view.selection.select(-1); 
  }
}

function GetOutlinerSelections(outliner) {
  var selections = [];
  var select = outliner.outlinerBoxObject.selection;
  if (select) {
    var count = select.getRangeCount();
    var min = new Object();
    var max = new Object();
    for (var i=0; i<count; i++) {
      select.getRangeAt(i, min, max);
      for (var k=min.value; k<=max.value; k++) {
        if (k != -1) {
          selections[selections.length] = k;
        }
      }
    }
  }
  return selections;
}

function SortOutliner(outliner, view, table, column, lastSortColumn, lastSortAscending) {

  // remember which item was selected so we can restore it after the sort
  var selections = GetOutlinerSelections(outliner);
  var selectedNumber = selections.length ? table[selections[0]].number : -1;

  // determine if sort is to be ascending or descending
  var ascending = (column == lastSortColumn) ? !lastSortAscending : true;

  // do the sort
  BubbleSort(column, ascending, table);

  // restore the selection
  var selectedRow = -1;
  if (selectedNumber>=0) {
    for (var s=0; s<table.length; s++) {
      if (table[s].number == selectedNumber) {
        // update selection
        // note: we need to deselect before reselecting in order to trigger ...Selected()
        outliner.outlinerBoxObject.view.selection.select(-1);
        outliner.outlinerBoxObject.view.selection.select(s);
        selectedRow = s;
        break;
      }
    }
  }

  // display the results
  outliner.outlinerBoxObject.invalidate();
  if (selectedRow>0) {
    outliner.outlinerBoxObject.ensureRowIsVisible(selectedRow)
  }

  return ascending;
}

function BubbleSort(columnName, ascending, table) {
  var len = table.length, len_1 = len - 1;

  for (var i = 0; i < len_1; i++) {
    var key = table[i][columnName];
    var winner = -1;
    for (var j = i + 1; j < len; j++) {
      var nextKey = table[j][columnName];
      if (ascending ? key > nextKey : key < nextKey) {
        key = nextKey;
        winner = j;
      }
    }
    if (winner != -1){
      var temp = table[i];
      table[i] = table[winner];
      table[winner] = temp;
    }
  }
}
