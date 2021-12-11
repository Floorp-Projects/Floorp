// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gTreeUtils = {
  deleteAll(aTree, aView, aItems, aDeletedItems) {
    for (var i = 0; i < aItems.length; ++i) {
      aDeletedItems.push(aItems[i]);
    }
    aItems.splice(0, aItems.length);
    var oldCount = aView.rowCount;
    aView._rowCount = 0;
    aTree.rowCountChanged(0, -oldCount);
  },

  deleteSelectedItems(aTree, aView, aItems, aDeletedItems) {
    var selection = aTree.view.selection;
    selection.selectEventsSuppressed = true;

    var rc = selection.getRangeCount();
    for (var i = 0; i < rc; ++i) {
      var min = {};
      var max = {};
      selection.getRangeAt(i, min, max);
      for (let j = min.value; j <= max.value; ++j) {
        aDeletedItems.push(aItems[j]);
        aItems[j] = null;
      }
    }

    var nextSelection = 0;
    for (i = 0; i < aItems.length; ++i) {
      if (!aItems[i]) {
        let j = i;
        while (j < aItems.length && !aItems[j]) {
          ++j;
        }
        aItems.splice(i, j - i);
        nextSelection = j < aView.rowCount ? j - 1 : j - 2;
        aView._rowCount -= j - i;
        aTree.rowCountChanged(i, i - j);
      }
    }

    if (aItems.length) {
      selection.select(nextSelection);
      aTree.ensureRowIsVisible(nextSelection);
      aTree.focus();
    }
    selection.selectEventsSuppressed = false;
  },

  sort(
    aTree,
    aView,
    aDataSet,
    aColumn,
    aComparator,
    aLastSortColumn,
    aLastSortAscending
  ) {
    var ascending = aColumn == aLastSortColumn ? !aLastSortAscending : true;
    if (!aDataSet.length) {
      return ascending;
    }

    var sortFunction = null;
    if (aComparator) {
      sortFunction = function(a, b) {
        return aComparator(a[aColumn], b[aColumn]);
      };
    }
    aDataSet.sort(sortFunction);
    if (!ascending) {
      aDataSet.reverse();
    }

    aTree.view.selection.clearSelection();
    aTree.view.selection.select(0);
    aTree.invalidate();
    aTree.ensureRowIsVisible(0);

    return ascending;
  },
};
