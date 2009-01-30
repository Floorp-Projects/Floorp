# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben "Count XULula" Goodger
#   Brian Ryner <bryner@brianryner.com>
#   Ehsan Akhgari <ehsan.akhgari@gmail.com>
#   Ronny Perinke <ronny.perinke@gmx.de>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

/*** =================== SAVED SIGNONS CODE =================== ***/

var kSignonBundle;

function SignonsStartup() {
  kSignonBundle = document.getElementById("signonBundle");
  document.getElementById("togglePasswords").label = kSignonBundle.getString("showPasswords");
  document.getElementById("togglePasswords").accessKey = kSignonBundle.getString("showPasswordsAccessKey");
  document.getElementById("signonsIntro").value = kSignonBundle.getString("loginsSpielAll");
  LoadSignons();

  // filter the table if requested by caller
  if (window.arguments && window.arguments[0] &&
      window.arguments[0].filterString) {
    document.getElementById("filter").value = window.arguments[0].filterString;
    _filterPasswords();
  }

  FocusFilterBox();
}

var signonsTreeView = {
  _filterSet : [],
  _lastSelectedRanges : [],
  selection: null, 

  rowCount : 0,
  setTree : function(tree) {},
  getImageSrc : function(row,column) {},
  getProgressMode : function(row,column) {},
  getCellValue : function(row,column) {},
  getCellText : function(row,column) {
    var signon = this._filterSet.length ? this._filterSet[row] : signons[row];
    switch (column.id) {
      case "siteCol":
        return signon.httpRealm ?
               (signon.hostname + " (" + signon.httpRealm + ")"):
               signon.hostname;
      case "userCol":
        return signon.username || "";
      case "passwordCol":
        return signon.password || "";
      default:
        return "";
    }
  },
  isSeparator : function(index) { return false; },
  isSorted : function() { return false; },
  isContainer : function(index) { return false; },
  cycleHeader : function(column) {},
  getRowProperties : function(row,prop) {},
  getColumnProperties : function(column,prop) {},
  getCellProperties : function(row,column,prop) {}
 };


function LoadSignons() {
  // loads signons into table
  try {
    signons = passwordmanager.getAllLogins({});
  } catch (e) {
    signons = [];
  }
  signonsTreeView.rowCount = signons.length;

  // sort and display the table
  signonsTree.treeBoxObject.view = signonsTreeView;
  SignonColumnSort('hostname');

  // disable "remove all signons" button if there are no signons
  var element = document.getElementById("removeAllSignons");
  var toggle = document.getElementById("togglePasswords");
  if (signons.length == 0 || gSelectUserInUse) {
    element.setAttribute("disabled","true");
    toggle.setAttribute("disabled","true");
  } else {
    element.removeAttribute("disabled");
    toggle.removeAttribute("disabled");
  }
 
  return true;
}

function SignonSelected() {
  var selections = GetTreeSelections(signonsTree);
  if (selections.length && !gSelectUserInUse) {
    document.getElementById("removeSignon").removeAttribute("disabled");
  }
}

function DeleteSignon() {
  var syncNeeded = (signonsTreeView._filterSet.length != 0);
  DeleteSelectedItemFromTree(signonsTree, signonsTreeView,
                             signonsTreeView._filterSet.length ? signonsTreeView._filterSet : signons,
                             deletedSignons, "removeSignon", "removeAllSignons");
  FinalizeSignonDeletions(syncNeeded);
}

function DeleteAllSignons() {
  var prompter = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                           .getService(Components.interfaces.nsIPromptService);

  // Confirm the user wants to remove all passwords
  var dummy = { value: false };
  if (prompter.confirmEx(window,
                         kSignonBundle.getString("removeAllPasswordsTitle"),
                         kSignonBundle.getString("removeAllPasswordsPrompt"),
                         prompter.STD_YES_NO_BUTTONS + prompter.BUTTON_POS_1_DEFAULT,
                         null, null, null, null, dummy) == 1) // 1 == "No" button
    return;

  var syncNeeded = (signonsTreeView._filterSet.length != 0);
  DeleteAllFromTree(signonsTree, signonsTreeView,
                        signonsTreeView._filterSet.length ? signonsTreeView._filterSet : signons,
                        deletedSignons, "removeSignon", "removeAllSignons");
  FinalizeSignonDeletions(syncNeeded);
}

function TogglePasswordVisible() {
  if (!showingPasswords && !ConfirmShowPasswords())
    return;

  showingPasswords = !showingPasswords;
  document.getElementById("togglePasswords").label = kSignonBundle.getString(showingPasswords ? "hidePasswords" : "showPasswords");
  document.getElementById("togglePasswords").accessKey = kSignonBundle.getString(showingPasswords ? "hidePasswordsAccessKey" : "showPasswordsAccessKey");
  document.getElementById("passwordCol").hidden = !showingPasswords;
  _filterPasswords();
}

function AskUserShowPasswords() {
  var prompter = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
  var dummy = { value: false };

  // Confirm the user wants to display passwords
  return prompter.confirmEx(window,
          null,
          kSignonBundle.getString("noMasterPasswordPrompt"), prompter.STD_YES_NO_BUTTONS,
          null, null, null, null, dummy) == 0;    // 0=="Yes" button
}

function ConfirmShowPasswords() {
  // This doesn't harm if passwords are not encrypted
  var tokendb = Components.classes["@mozilla.org/security/pk11tokendb;1"]
                    .createInstance(Components.interfaces.nsIPK11TokenDB);
  var token = tokendb.getInternalKeyToken();

  // If there is no master password, still give the user a chance to opt-out of displaying passwords
  if (token.checkPassword(""))
    return AskUserShowPasswords();

  // So there's a master password. But since checkPassword didn't succeed, we're logged out (per nsIPK11Token.idl).
  try {
    // Relogin and ask for the master password.
    token.login(true);  // 'true' means always prompt for token password. User will be prompted until
                        // clicking 'Cancel' or entering the correct password.
  } catch (e) {
    // An exception will be thrown if the user cancels the login prompt dialog.
    // User is also logged out of Software Security Device.
  }

  return token.isLoggedIn();
}

function FinalizeSignonDeletions(syncNeeded) {
  for (var s=0; s<deletedSignons.length; s++) {
    passwordmanager.removeLogin(deletedSignons[s]);
  }
  // If the deletion has been performed in a filtered view, reflect the deletion in the unfiltered table.
  // See bug 405389.
  if (syncNeeded) {
    try {
      signons = passwordmanager.getAllLogins({});
    } catch (e) {
      signons = [];
    }
  }
  deletedSignons.length = 0;
}

function HandleSignonKeyPress(e) {
  if (e.keyCode == 46) {
    DeleteSignon();
  }
}

var lastSignonSortColumn = "";
var lastSignonSortAscending = false;

function SignonColumnSort(column) {
  lastSignonSortAscending =
    SortTree(signonsTree, signonsTreeView,
                 signonsTreeView._filterSet.length ? signonsTreeView._filterSet : signons,
                 column, lastSignonSortColumn, lastSignonSortAscending);
  lastSignonSortColumn = column;
}

function SignonClearFilter() {
  var singleSelection = (signonsTreeView.selection.count == 1);

  // Clear the Tree Display
  signonsTreeView.rowCount = 0;
  signonsTree.treeBoxObject.rowCountChanged(0, -signonsTreeView._filterSet.length);
  signonsTreeView._filterSet = [];

  // Just reload the list to make sure deletions are respected
  lastSignonSortColumn = "";
  lastSignonSortAscending = false;
  LoadSignons();
    
  // Restore selection
  if (singleSelection) {
    signonsTreeView.selection.clearSelection();
    for (i = 0; i < signonsTreeView._lastSelectedRanges.length; ++i) {
      var range = signonsTreeView._lastSelectedRanges[i];
      signonsTreeView.selection.rangedSelect(range.min, range.max, true);
    }
  } else {
    signonsTreeView.selection.select(0);
  }
  signonsTreeView._lastSelectedRanges = [];

  document.getElementById("signonsIntro").value = kSignonBundle.getString("loginsSpielAll");
}

function FocusFilterBox() {
  var filterBox = document.getElementById("filter");
  if (filterBox.getAttribute("focused") != "true")
    filterBox.focus();
}

function SignonMatchesFilter(aSignon, aFilterValue) {
  if (aSignon.hostname.toLowerCase().indexOf(aFilterValue) != -1)
    return true;
  if (aSignon.username &&
      aSignon.username.toLowerCase().indexOf(aFilterValue) != -1)
    return true;
  if (aSignon.httpRealm &&
      aSignon.httpRealm.toLowerCase().indexOf(aFilterValue) != -1)
    return true;
  if (showingPasswords && aSignon.password &&
      aSignon.password.toLowerCase().indexOf(aFilterValue) != -1)
    return true;

  return false;
}

function FilterPasswords(aFilterValue, view) {
  aFilterValue = aFilterValue.toLowerCase();
  return signons.filter(function (s) SignonMatchesFilter(s, aFilterValue));
}

function SignonSaveState() {
  // Save selection
  var seln = signonsTreeView.selection;
  signonsTreeView._lastSelectedRanges = [];
  var rangeCount = seln.getRangeCount();
  for (var i = 0; i < rangeCount; ++i) {
    var min = {}; var max = {};
    seln.getRangeAt(i, min, max);
    signonsTreeView._lastSelectedRanges.push({ min: min.value, max: max.value });
  }
}

function _filterPasswords()
{
  var filter = document.getElementById("filter").value;
  if (filter == "") {
    SignonClearFilter();
    return;
  }

  var newFilterSet = FilterPasswords(filter, signonsTreeView);
  if (!signonsTreeView._filterSet.length) {
    // Save Display Info for the Non-Filtered mode when we first
    // enter Filtered mode. 
    SignonSaveState();
  }
  signonsTreeView._filterSet = newFilterSet;

  // Clear the display
  signonsTree.treeBoxObject.rowCountChanged(0, -signonsTreeView.rowCount);
  // Set up the filtered display
  signonsTreeView.rowCount = signonsTreeView._filterSet.length;
  signonsTree.treeBoxObject.rowCountChanged(0, signonsTreeView.rowCount);

  // if the view is not empty then select the first item
  if (signonsTreeView.rowCount > 0)
    signonsTreeView.selection.select(0);

  document.getElementById("signonsIntro").value = kSignonBundle.getString("loginsSpielFiltered");
}
