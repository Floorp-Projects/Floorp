// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*** =================== REJECTED SIGNONS CODE =================== ***/

function RejectsStartup() {
  LoadRejects();
}

var rejectsTreeView = {
  rowCount : 0,
  setTree : function(tree){},
  getImageSrc : function(row,column) {},
  getProgressMode : function(row,column) {},
  getCellValue : function(row,column) {},
  getCellText : function(row,column){
    var rv="";
    if (column.id=="rejectCol") {
      rv = rejects[row].host;
    }
    return rv;
  },
  isSeparator : function(index) {return false;},
  isSorted: function() { return false; },
  isContainer : function(index) {return false;},
  cycleHeader : function(column) {},
  getRowProperties : function(row){ return ""; },
  getColumnProperties : function(column){ return ""; },
  getCellProperties : function(row,column){
    if (column.element.getAttribute("id") == "rejectCol")
      return "ltr";

    return "";
  }
};

function Reject(number, host) {
  this.number = number;
  this.host = host;
}

function LoadRejects() {
  var hosts = passwordmanager.getAllDisabledHosts();
  rejects = hosts.map(function(host, i) { return new Reject(i, host); });
  rejectsTreeView.rowCount = rejects.length;

  // sort and display the table
  rejectsTree.view = rejectsTreeView;
  RejectColumnSort(lastRejectSortColumn);

  var element = document.getElementById("removeAllRejects");
  if (rejects.length == 0) {
    element.setAttribute("disabled","true");
  } else {
    element.removeAttribute("disabled");
  }
}

function RejectSelected() {
  var selections = GetTreeSelections(rejectsTree);
  if (selections.length) {
    document.getElementById("removeReject").removeAttribute("disabled");
  }
}

function DeleteReject() {
  DeleteSelectedItemFromTree(rejectsTree, rejectsTreeView,
                                 rejects, deletedRejects,
                                 "removeReject", "removeAllRejects");
  FinalizeRejectDeletions();
}

function DeleteAllRejects() {
  DeleteAllFromTree(rejectsTree, rejectsTreeView,
                        rejects, deletedRejects,
                        "removeReject", "removeAllRejects");
  FinalizeRejectDeletions();
}

function FinalizeRejectDeletions() {
  for (var r=0; r<deletedRejects.length; r++) {
    passwordmanager.setLoginSavingEnabled(deletedRejects[r].host, true);
  }
  deletedRejects.length = 0;
}

function HandleRejectKeyPress(e) {
  if (e.keyCode == KeyEvent.DOM_VK_DELETE
#ifdef XP_MACOSX
      || e.keyCode == KeyEvent.DOM_VK_BACK_SPACE
#endif
     ) {
    DeleteReject();
  }
}

var lastRejectSortColumn = "host";
var lastRejectSortAscending = false;

function RejectColumnSort(column) {
  lastRejectSortAscending =
    SortTree(rejectsTree, rejectsTreeView, rejects,
                 column, lastRejectSortColumn, lastRejectSortAscending);
  lastRejectSortColumn = column;

  // set the sortDirection attribute to get the styling going
  var sortedCol = document.getElementById("rejectCol");
  sortedCol.setAttribute("sortDirection", lastRejectSortAscending ?
                                          "ascending" : "descending");
}
