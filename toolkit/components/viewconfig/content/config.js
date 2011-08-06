# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
# The Original Code is mozilla.org configuration viewer.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chip Clark <chipc@netscape.com>
#   Seth Spitzer <sspitzer@netscape.com>
#   Neil Rashbrook <neil@parkwaycc.co.uk>
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

const nsIPrefLocalizedString = Components.interfaces.nsIPrefLocalizedString;
const nsISupportsString = Components.interfaces.nsISupportsString;
const nsIPromptService = Components.interfaces.nsIPromptService;
const nsIPrefService = Components.interfaces.nsIPrefService;
const nsIPrefBranch = Components.interfaces.nsIPrefBranch;
const nsIClipboardHelper = Components.interfaces.nsIClipboardHelper;
const nsIAtomService = Components.interfaces.nsIAtomService;

const nsSupportsString_CONTRACTID = "@mozilla.org/supports-string;1";
const nsPrompt_CONTRACTID = "@mozilla.org/embedcomp/prompt-service;1";
const nsPrefService_CONTRACTID = "@mozilla.org/preferences-service;1";
const nsClipboardHelper_CONTRACTID = "@mozilla.org/widget/clipboardhelper;1";
const nsAtomService_CONTRACTID = "@mozilla.org/atom-service;1";

const gPromptService = Components.classes[nsPrompt_CONTRACTID].getService(nsIPromptService);
const gPrefService = Components.classes[nsPrefService_CONTRACTID].getService(nsIPrefService);
const gPrefBranch = gPrefService.getBranch(null).QueryInterface(Components.interfaces.nsIPrefBranch2);
const gClipboardHelper = Components.classes[nsClipboardHelper_CONTRACTID].getService(nsIClipboardHelper);
const gAtomService = Components.classes[nsAtomService_CONTRACTID].getService(nsIAtomService);

var gLockAtoms = [gAtomService.getAtom("default"), gAtomService.getAtom("user"), gAtomService.getAtom("locked")];
// we get these from a string bundle
var gLockStrs = [];
var gTypeStrs = [];

const PREF_IS_DEFAULT_VALUE = 0;
const PREF_IS_USER_SET = 1;
const PREF_IS_LOCKED = 2;

var gPrefHash = {};
var gPrefArray = [];
var gPrefView = gPrefArray; // share the JS array
var gSortedColumn = "prefCol";
var gSortFunction = null;
var gSortDirection = 1; // 1 is ascending; -1 is descending
var gConfigBundle = null;
var gFilter = null;

var view = {
  get rowCount() { return gPrefView.length; },
  getCellText : function(index, col) {
    if (!(index in gPrefView))
      return "";
    
    var value = gPrefView[index][col.id];

    switch (col.id) {
      case "lockCol":           
        return gLockStrs[value];
      case "typeCol":
        return gTypeStrs[value];
      default:
        return value;
    }
  },
  getRowProperties : function(index, prop) {},
  getCellProperties : function(index, col, prop) {
    if (index in gPrefView)
      prop.AppendElement(gLockAtoms[gPrefView[index].lockCol]);
  },
  getColumnProperties : function(col, prop) {},
  treebox : null,
  selection : null,
  isContainer : function(index) { return false; },
  isContainerOpen : function(index) { return false; },
  isContainerEmpty : function(index) { return false; },
  isSorted : function() { return true; },
  canDrop : function(index, orientation) { return false; },
  drop : function(row, orientation) {},
  setTree : function(out) { this.treebox = out; },
  getParentIndex: function(rowIndex) { return -1; },
  hasNextSibling: function(rowIndex, afterIndex) { return false; },
  getLevel: function(index) { return 1; },
  getImageSrc: function(row, col) { return ""; },
  toggleOpenState : function(index) {},
  cycleHeader: function(col) {
    var index = this.selection.currentIndex;
    if (col.id == gSortedColumn) {
      gSortDirection = -gSortDirection;
      gPrefArray.reverse();
      if (gPrefView != gPrefArray)
        gPrefView.reverse();
      if (index >= 0)
        index = gPrefView.length - index - 1;
    }
    else {
      var pref = null;
      if (index >= 0)
        pref = gPrefView[index];

      var old = document.getElementById(gSortedColumn);
      old.setAttribute("sortDirection", "");
      gPrefArray.sort(gSortFunction = gSortFunctions[col.id]);
      if (gPrefView != gPrefArray)
        gPrefView.sort(gSortFunction);
      gSortedColumn = col.id;
      if (pref)
        index = getViewIndexOfPref(pref);
    }
    col.element.setAttribute("sortDirection", gSortDirection > 0 ? "ascending" : "descending");
    this.treebox.invalidate();
    if (index >= 0) {
      this.selection.select(index);
      this.treebox.ensureRowIsVisible(index);
    }
  },
  selectionChanged : function() {},
  cycleCell: function(row, col) {},
  isEditable: function(row, col) {return false; },
  isSelectable: function(row, col) {return false; },
  setCellValue: function(row, col, value) {},
  setCellText: function(row, col, value) {},
  performAction: function(action) {},
  performActionOnRow: function(action, row) {},
  performActionOnCell: function(action, row, col) {},
  isSeparator: function(index) {return false; }
};

// find the index in gPrefView of a pref object
// or -1 if it does not exist in the filtered view
function getViewIndexOfPref(pref)
{
  var low = -1, high = gPrefView.length;
  var index = (low + high) >> 1;
  while (index > low) {
    var mid = gPrefView[index];
    if (mid == pref)
      return index;
    if (gSortFunction(mid, pref) < 0)
      low = index;
    else
      high = index;
    index = (low + high) >> 1;
  }
  return -1;
}

// find the index in gPrefView where a pref object belongs
function getNearestViewIndexOfPref(pref)
{
  var low = -1, high = gPrefView.length;
  var index = (low + high) >> 1;
  while (index > low) {
    if (gSortFunction(gPrefView[index], pref) < 0)
      low = index;
    else
      high = index;
    index = (low + high) >> 1;
  }
  return high;
}

// find the index in gPrefArray of a pref object
function getIndexOfPref(pref)
{
  var low = -1, high = gPrefArray.length;
  var index = (low + high) >> 1;
  while (index > low) {
    var mid = gPrefArray[index];
    if (mid == pref)
      return index;
    if (gSortFunction(mid, pref) < 0)
      low = index;
    else
      high = index;
    index = (low + high) >> 1;
  }
  return index;
}

function getNearestIndexOfPref(pref)
{
  var low = -1, high = gPrefArray.length;
  var index = (low + high) >> 1;
  while (index > low) {
    if (gSortFunction(gPrefArray[index], pref) < 0)
      low = index;
    else
      high = index;
    index = (low + high) >> 1;
  }
  return high;
}

var gPrefListener =
{
  observe: function(subject, topic, prefName)
  {
    if (topic != "nsPref:changed")
      return;

    if (/^capability\./.test(prefName)) // avoid displaying "private" preferences
      return;

    var arrayIndex = gPrefArray.length;
    var viewIndex = arrayIndex;
    var selectedIndex = view.selection.currentIndex;
    var pref;
    var updateView = false;
    var updateArray = false;
    var addedRow = false;
    if (prefName in gPrefHash) {
      pref = gPrefHash[prefName];
      viewIndex = getViewIndexOfPref(pref);
      arrayIndex = getIndexOfPref(pref);
      fetchPref(prefName, arrayIndex);
      // fetchPref replaces the existing pref object
      pref = gPrefHash[prefName];
      if (viewIndex >= 0) {
        // Might need to update the filtered view
        gPrefView[viewIndex] = gPrefHash[prefName];
        view.treebox.invalidateRow(viewIndex);
      }
      if (gSortedColumn == "lockCol" || gSortedColumn == "valueCol") {
        updateArray = true;
        gPrefArray.splice(arrayIndex, 1);
        if (gFilter && gFilter.test(pref.prefCol + ";" + pref.valueCol)) {
          updateView = true;
          gPrefView.splice(viewIndex, 1);
        }
      }
    }
    else {
      fetchPref(prefName, arrayIndex);
      pref = gPrefArray.pop();
      updateArray = true;
      addedRow = true;
      if (gFilter && gFilter.test(pref.prefCol + ";" + pref.valueCol)) {
        updateView = true;
      }
    }
    if (updateArray) {
      // Reinsert in the data array
      var newIndex = getNearestIndexOfPref(pref);
      gPrefArray.splice(newIndex, 0, pref);

      if (updateView) {
        // View is filtered, reinsert in the view separately
        newIndex = getNearestViewIndexOfPref(pref);
        gPrefView.splice(newIndex, 0, pref);
      }
      else if (gFilter) {
        // View is filtered, but nothing to update
        return;
      }

      if (addedRow)
        view.treebox.rowCountChanged(newIndex, 1);

      // Invalidate the changed range in the view
      var low = Math.min(viewIndex, newIndex);
      var high = Math.max(viewIndex, newIndex);
      view.treebox.invalidateRange(low, high);

      if (selectedIndex == viewIndex) {
        selectedIndex = newIndex;
      }
      else if (selectedIndex >= low && selectedIndex <= high) {
        selectedIndex += (newIndex > viewIndex) ? -1 : 1;
      }
      if (selectedIndex >= 0) {
        view.selection.select(selectedIndex);
        if (selectedIndex == newIndex)
          view.treebox.ensureRowIsVisible(selectedIndex);
      }
    }
  }
};

function prefObject(prefName, prefIndex)
{
  this.prefCol = prefName;
}

prefObject.prototype =
{
  lockCol: PREF_IS_DEFAULT_VALUE,
  typeCol: nsIPrefBranch.PREF_STRING,
  valueCol: ""
};

function fetchPref(prefName, prefIndex)
{
  var pref = new prefObject(prefName);

  gPrefHash[prefName] = pref;
  gPrefArray[prefIndex] = pref;

  if (gPrefBranch.prefIsLocked(prefName))
    pref.lockCol = PREF_IS_LOCKED;
  else if (gPrefBranch.prefHasUserValue(prefName))
    pref.lockCol = PREF_IS_USER_SET;

  try {
    switch (gPrefBranch.getPrefType(prefName)) {
      case gPrefBranch.PREF_BOOL:
        pref.typeCol = gPrefBranch.PREF_BOOL;
        // convert to a string
        pref.valueCol = gPrefBranch.getBoolPref(prefName).toString();
        break;
      case gPrefBranch.PREF_INT:
        pref.typeCol = gPrefBranch.PREF_INT;
        // convert to a string
        pref.valueCol = gPrefBranch.getIntPref(prefName).toString();
        break;
      default:
      case gPrefBranch.PREF_STRING:
        pref.valueCol = gPrefBranch.getComplexValue(prefName, nsISupportsString).data;
        // Try in case it's a localized string (will throw an exception if not)
        if (pref.lockCol == PREF_IS_DEFAULT_VALUE &&
            /^chrome:\/\/.+\/locale\/.+\.properties/.test(pref.valueCol))
          pref.valueCol = gPrefBranch.getComplexValue(prefName, nsIPrefLocalizedString).data;
        break;
    }
  } catch (e) {
    // Also catch obscure cases in which you can't tell in advance
    // that the pref exists but has no user or default value...
  }
}

function onConfigLoad()
{
  // Load strings
  gConfigBundle = document.getElementById("configBundle");

  gLockStrs[PREF_IS_DEFAULT_VALUE] = gConfigBundle.getString("default");
  gLockStrs[PREF_IS_USER_SET] = gConfigBundle.getString("user");
  gLockStrs[PREF_IS_LOCKED] = gConfigBundle.getString("locked");

  gTypeStrs[nsIPrefBranch.PREF_STRING] = gConfigBundle.getString("string");
  gTypeStrs[nsIPrefBranch.PREF_INT] = gConfigBundle.getString("int");
  gTypeStrs[nsIPrefBranch.PREF_BOOL] = gConfigBundle.getString("bool");

  var showWarning = gPrefBranch.getBoolPref("general.warnOnAboutConfig");

  if (showWarning)
    document.getElementById("warningButton").focus();
  else
    ShowPrefs();
}

// Unhide the warning message
function ShowPrefs()
{
  var prefArray = gPrefBranch.getChildList("");

  prefArray.forEach(function (prefName) {
    if (/^capability\./.test(prefName)) // avoid displaying "private" preferences
      return;

    fetchPref(prefName, gPrefArray.length);
  });

  var descending = document.getElementsByAttribute("sortDirection", "descending");
  if (descending.item(0)) {
    gSortedColumn = descending[0].id;
    gSortDirection = -1;
  }
  else {
    var ascending = document.getElementsByAttribute("sortDirection", "ascending");
    if (ascending.item(0))
      gSortedColumn = ascending[0].id;
    else
      document.getElementById(gSortedColumn).setAttribute("sortDirection", "ascending");
  }
  gSortFunction = gSortFunctions[gSortedColumn];
  gPrefArray.sort(gSortFunction);
  
  gPrefBranch.addObserver("", gPrefListener, false);

  var configTree = document.getElementById("configTree");
  configTree.view = view;
  configTree.controllers.insertControllerAt(0, configController);

  document.getElementById("configDeck").setAttribute("selectedIndex", 1);
  document.getElementById("configTreeKeyset").removeAttribute("disabled");
  if (!document.getElementById("showWarningNextTime").checked)
    gPrefBranch.setBoolPref("general.warnOnAboutConfig", false);

  // Process about:config?filter=<string>
  var textbox = document.getElementById("textbox");
  var uri = document.documentURIObject;
  var matches = /[?&]filter\=([^&]+)/i.exec(uri.path);
  if (matches)
    textbox.value = decodeURIComponent(matches[1]);

  // Even if we did not set the filter string via the URL query,
  // textbox might have been set via some other mechanism
  if (textbox.value)
    FilterPrefs();
  textbox.focus();
}

function onConfigUnload()
{
  if (document.getElementById("configDeck").getAttribute("selectedIndex") == 1) {
    gPrefBranch.removeObserver("", gPrefListener);
    var configTree = document.getElementById("configTree");
    configTree.view = null;
    configTree.controllers.removeController(configController);
  }
}

function FilterPrefs()
{
  var substring = document.getElementById("textbox").value;
  // Check for "/regex/[i]"
  if (substring.charAt(0) == '/') {
    var r = substring.match(/^\/(.*)\/(i?)$/);
    try {
      gFilter = RegExp(r[1], r[2]);
    }
    catch (e) {
      return; // Do nothing on incomplete or bad RegExp
    }
  }
  else if (substring) {
    gFilter = RegExp(substring.replace(/([^* \w])/g, "\\$1")
                              .replace(/^\*+/, "").replace(/\*+/g, ".*"), "i");
  } else {
    gFilter = null;
  }

  var prefCol = view.selection.currentIndex < 0 ? null : gPrefView[view.selection.currentIndex].prefCol;
  var oldlen = gPrefView.length;
  gPrefView = gPrefArray;
  if (gFilter) {
    gPrefView = [];
    for (var i = 0; i < gPrefArray.length; ++i)
      if (gFilter.test(gPrefArray[i].prefCol + ";" + gPrefArray[i].valueCol))
        gPrefView.push(gPrefArray[i]);
  }
  view.treebox.invalidate();
  view.treebox.rowCountChanged(oldlen, gPrefView.length - oldlen);
  gotoPref(prefCol);
}

function prefColSortFunction(x, y)
{
  if (x.prefCol > y.prefCol)
    return gSortDirection;
  if (x.prefCol < y.prefCol) 
    return -gSortDirection;
  return 0;
}

function lockColSortFunction(x, y)
{
  if (x.lockCol != y.lockCol)
    return gSortDirection * (y.lockCol - x.lockCol);
  return prefColSortFunction(x, y);
}

function typeColSortFunction(x, y)
{
  if (x.typeCol != y.typeCol) 
    return gSortDirection * (y.typeCol - x.typeCol);
  return prefColSortFunction(x, y);
}

function valueColSortFunction(x, y)
{
  if (x.valueCol > y.valueCol)
    return gSortDirection;
  if (x.valueCol < y.valueCol) 
    return -gSortDirection;
  return prefColSortFunction(x, y);
}

const gSortFunctions =
{
  prefCol: prefColSortFunction, 
  lockCol: lockColSortFunction, 
  typeCol: typeColSortFunction, 
  valueCol: valueColSortFunction
};

const configController = {
  supportsCommand: function supportsCommand(command) {
    return command == "cmd_copy";
  },
  isCommandEnabled: function isCommandEnabled(command) {
    return view.selection && view.selection.currentIndex >= 0;
  },
  doCommand: function doCommand(command) {
    copyPref();
  },
  onEvent: function onEvent(event) {
  }
}

function updateContextMenu()
{
  var lockCol = PREF_IS_LOCKED;
  var typeCol = nsIPrefBranch.PREF_STRING;
  var valueCol = "";
  var copyDisabled = true;
  var prefSelected = view.selection.currentIndex >= 0;

  if (prefSelected) {
    var prefRow = gPrefView[view.selection.currentIndex];
    lockCol = prefRow.lockCol;
    typeCol = prefRow.typeCol;
    valueCol = prefRow.valueCol;
    copyDisabled = false;
  }

  var copyPref = document.getElementById("copyPref");
  copyPref.setAttribute("disabled", copyDisabled);

  var copyName = document.getElementById("copyName");
  copyName.setAttribute("disabled", copyDisabled);

  var copyValue = document.getElementById("copyValue");
  copyValue.setAttribute("disabled", copyDisabled);

  var resetSelected = document.getElementById("resetSelected");
  resetSelected.setAttribute("disabled", lockCol != PREF_IS_USER_SET);

  var canToggle = typeCol == nsIPrefBranch.PREF_BOOL && valueCol != "";
  // indicates that a pref is locked or no pref is selected at all
  var isLocked = lockCol == PREF_IS_LOCKED;

  var modifySelected = document.getElementById("modifySelected");
  modifySelected.setAttribute("disabled", isLocked);
  modifySelected.hidden = canToggle;

  var toggleSelected = document.getElementById("toggleSelected");
  toggleSelected.setAttribute("disabled", isLocked);
  toggleSelected.hidden = !canToggle;
}

function copyPref()
{
  var pref = gPrefView[view.selection.currentIndex];
  gClipboardHelper.copyString(pref.prefCol + ';' + pref.valueCol);
}

function copyName()
{
  gClipboardHelper.copyString(gPrefView[view.selection.currentIndex].prefCol);
}

function copyValue()
{
  gClipboardHelper.copyString(gPrefView[view.selection.currentIndex].valueCol);
}

function ModifySelected()
{
  if (view.selection.currentIndex >= 0)
    ModifyPref(gPrefView[view.selection.currentIndex]);
}

function ResetSelected()
{
  var entry = gPrefView[view.selection.currentIndex];
  gPrefBranch.clearUserPref(entry.prefCol);
}

function NewPref(type)
{
  var result = { value: "" };
  var dummy = { value: 0 };
  if (gPromptService.prompt(window,
                            gConfigBundle.getFormattedString("new_title", [gTypeStrs[type]]),
                            gConfigBundle.getString("new_prompt"),
                            result,
                            null,
                            dummy) && result.value) {
    var pref;
    if (result.value in gPrefHash)
      pref = gPrefHash[result.value];
    else
      pref = { prefCol: result.value, lockCol: PREF_IS_DEFAULT_VALUE, typeCol: type, valueCol: "" };
    if (ModifyPref(pref))
      setTimeout(gotoPref, 0, result.value);
  }
}

function gotoPref(pref)
{
  // make sure the pref exists and is displayed in the current view
  var index = pref in gPrefHash ? getViewIndexOfPref(gPrefHash[pref]) : -1;
  if (index >= 0) {
    view.selection.select(index);
    view.treebox.ensureRowIsVisible(index);
  } else {
    view.selection.clearSelection();
    view.selection.currentIndex = -1;
  }
}

function ModifyPref(entry)
{
  if (entry.lockCol == PREF_IS_LOCKED)
    return false;
  var title = gConfigBundle.getFormattedString("modify_title", [gTypeStrs[entry.typeCol]]);
  if (entry.typeCol == nsIPrefBranch.PREF_BOOL) {
    var check = { value: entry.valueCol == "false" };
    if (!entry.valueCol && !gPromptService.select(window, title, entry.prefCol, 2, [false, true], check))
      return false;
    gPrefBranch.setBoolPref(entry.prefCol, check.value);
  } else {
    var result = { value: entry.valueCol };
    var dummy = { value: 0 };
    if (!gPromptService.prompt(window, title, entry.prefCol, result, null, dummy))
      return false;
    if (entry.typeCol == nsIPrefBranch.PREF_INT) {
      // | 0 converts to integer or 0; - 0 to float or NaN.
      // Thus, this check should catch all cases.
      var val = result.value | 0;
      if (val != result.value - 0) {
        var err_title = gConfigBundle.getString("nan_title");
        var err_text = gConfigBundle.getString("nan_text");
        gPromptService.alert(window, err_title, err_text);
        return false;
      }
      gPrefBranch.setIntPref(entry.prefCol, val);
    } else {
      var supportsString = Components.classes[nsSupportsString_CONTRACTID].createInstance(nsISupportsString);
      supportsString.data = result.value;
      gPrefBranch.setComplexValue(entry.prefCol, nsISupportsString, supportsString);
    }
  }

  gPrefService.savePrefFile(null);
  return true;
}
