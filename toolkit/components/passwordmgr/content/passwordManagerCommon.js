# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

/*** =================== INITIALISATION CODE =================== ***/

var kObserverService;
var gSelectUserInUse = false;

// interface variables
var passwordmanager     = null;

// password-manager lists
var signons             = [];
var rejects             = [];
var deletedSignons      = [];
var deletedRejects      = [];

var signonsTree;
var rejectsTree;

var showingPasswords = false;

var kLTRAtom;

function Startup() {
  // xpconnect to password manager interfaces
  passwordmanager = Components.classes["@mozilla.org/login-manager;1"]
                        .getService(Components.interfaces.nsILoginManager);

  // be prepared to reload the display if anything changes
  kObserverService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  kObserverService.addObserver(signonReloadDisplay, "signonChanged", false);

  // be prepared to disable the buttons when selectuser dialog is in use
  kObserverService.addObserver(signonReloadDisplay, "signonSelectUser", false);

  signonsTree = document.getElementById("signonsTree");
  rejectsTree = document.getElementById("rejectsTree");

  kLTRAtom = Components.classes["@mozilla.org/atom-service;1"]
                       .getService(Components.interfaces.nsIAtomService)
                       .getAtom("ltr");
}

function Shutdown() {
  kObserverService.removeObserver(signonReloadDisplay, "signonChanged");
  kObserverService.removeObserver(signonReloadDisplay, "signonSelectUser");
}

var signonReloadDisplay = {
  observe: function(subject, topic, state) {
    if (topic == "signonChanged") {
      if (state == "signons") {
        signons.length = 0;
        if (lastSignonSortColumn == "hostname") {
          lastSignonSortAscending = !lastSignonSortAscending; // prevents sort from being reversed
        }
        LoadSignons();
        // apply the filter if needed
        if (document.getElementById("filter") && document.getElementById("filter").value != "") {
          _filterPasswords();
        }
      } else if (state == "rejects") {
        rejects.length = 0;
        if (lastRejectSortColumn == "hostname") {
          lastRejectSortAscending = !lastRejectSortAscending; // prevents sort from being reversed
        }
        LoadRejects();
      }
    } else if (topic == "signonSelectUser") {
      if (state == "suspend") {
        gSelectUserInUse = true;
        document.getElementById("removeSignon").disabled = true;
        document.getElementById("removeAllSignons").disabled = true;
        document.getElementById("togglePasswords").disabled = true;
      } else if (state == "resume") {
        gSelectUserInUse = false;
        var selections = GetTreeSelections(signonsTree);
        if (selections.length > 0) {
          document.getElementById("removeSignon").disabled = false;
        }
        if (signons.length > 0) {
          document.getElementById("removeAllSignons").disabled = false;
          document.getElementById("togglePasswords").disabled = false;
        }
      } else if (state == "inUse") {
        gSelectUserInUse = true;
      }
    }
  }
}

/*** =================== GENERAL CODE =================== ***/

function DeleteAllFromTree(tree, view, table, deletedTable, removeButton, removeAllButton) {

  // remove all items from table and place in deleted table
  for (var i=0; i<table.length; i++) {
    deletedTable[deletedTable.length] = table[i];
  }
  table.length = 0;

  // clear out selections
  view.selection.select(-1); 

  // update the tree view and notify the tree
  view.rowCount = 0;

  var box = tree.treeBoxObject;
  box.rowCountChanged(0, -deletedTable.length);
  box.invalidate();


  // disable buttons
  document.getElementById(removeButton).setAttribute("disabled", "true")
  document.getElementById(removeAllButton).setAttribute("disabled","true");
}

function DeleteSelectedItemFromTree
    (tree, view, table, deletedTable, removeButton, removeAllButton) {

  // Turn off tree selection notifications during the deletion
  tree.view.selection.selectEventsSuppressed = true;

  // remove selected items from list (by setting them to null) and place in deleted list
  var selections = GetTreeSelections(tree);
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
      view.rowCount -= k - j;
      tree.treeBoxObject.rowCountChanged(j, j - k);
    }
  }

  // update selection and/or buttons
  if (table.length) {
    // update selection
    var nextSelection = (selections[0] < table.length) ? selections[0] : table.length-1;
    tree.view.selection.select(nextSelection);
    tree.treeBoxObject.ensureRowIsVisible(nextSelection);
  } else {
    // disable buttons
    document.getElementById(removeButton).setAttribute("disabled", "true")
    document.getElementById(removeAllButton).setAttribute("disabled","true");
  }
  tree.view.selection.selectEventsSuppressed = false;
}

function GetTreeSelections(tree) {
  var selections = [];
  var select = tree.view.selection;
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

function SortTree(tree, view, table, column, lastSortColumn, lastSortAscending, updateSelection) {

  // remember which item was selected so we can restore it after the sort
  var selections = GetTreeSelections(tree);
  var selectedNumber = selections.length ? table[selections[0]].number : -1;

  // determine if sort is to be ascending or descending
  var ascending = (column == lastSortColumn) ? !lastSortAscending : true;

  // do the sort
  var compareFunc;
  if (ascending) {
    compareFunc = function compare(first, second) {
      return CompareLowerCase(first[column], second[column]);
    }
  } else {
    compareFunc = function compare(first, second) {
      return CompareLowerCase(second[column], first[column]);
    }
  }
  table.sort(compareFunc);

  // restore the selection
  var selectedRow = -1;
  if (selectedNumber>=0 && updateSelection) {
    for (var s=0; s<table.length; s++) {
      if (table[s].number == selectedNumber) {
        // update selection
        // note: we need to deselect before reselecting in order to trigger ...Selected()
        tree.view.selection.select(-1);
        tree.view.selection.select(s);
        selectedRow = s;
        break;
      }
    }
  }

  // display the results
  tree.treeBoxObject.invalidate();
  if (selectedRow >= 0) {
    tree.treeBoxObject.ensureRowIsVisible(selectedRow)
  }

  return ascending;
}

/**
 * Case insensitive string comparator.
 */
function CompareLowerCase(first, second) {
  var firstLower, secondLower;

  // Are we sorting nsILoginInfo entries or just strings?
  if (first.hostname) {
    firstLower  = first.hostname.toLowerCase();
    secondLower = second.hostname.toLowerCase();
  } else {
    firstLower  = first.toLowerCase();
    secondLower = second.toLowerCase();
  }

  if (firstLower < secondLower) {
    return -1;
  }

  if (firstLower > secondLower) {
    return 1;
  }

  return 0;
}
