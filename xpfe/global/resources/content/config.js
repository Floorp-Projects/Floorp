/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributors:
 *  Chip Clark <chipc@netscape.com>
 *  Seth Spitzer <sspitzer@netscape.com>
 *  Neil Rashbrook <neil@parkwaycc.co.uk>
 */

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
const gPrefBranch = gPrefService.getBranch(null).QueryInterface(Components.interfaces.nsIPrefBranchInternal);
const gClipboardHelper = Components.classes[nsClipboardHelper_CONTRACTID].getService(nsIClipboardHelper);
const gAtomService = Components.classes[nsAtomService_CONTRACTID].getService(nsIAtomService);

var gLockAtoms = [gAtomService.getAtom("default"), gAtomService.getAtom("user"), gAtomService.getAtom("locked")];
// XXX get these from a string bundle
var gLockStrs = ["default", "user set", "locked"];
var gTypeStrs = [];
gTypeStrs[nsIPrefBranch.PREF_STRING] = "string";
gTypeStrs[nsIPrefBranch.PREF_INT] = "integer";
gTypeStrs[nsIPrefBranch.PREF_BOOL] = "boolean";

const PREF_IS_DEFAULT_VALUE = 0;
const PREF_IS_USER_SET = 1;
const PREF_IS_LOCKED = 2;

var gPrefHash = {};
var gPrefArray = [];
var gFastIndex = 0;
var gSortedColumn = "prefCol";
var gSortFunction = null;
var gSortDirection = 1; // 1 is ascending; -1 is descending

var view = {
  get rowCount() { return gPrefArray.length; },
  getCellText : function(k, col) {
    if (!(k in gPrefArray))
      return "";
    
    var value = gPrefArray[k][col];

    switch (col) {
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
    prop.AppendElement(gLockAtoms[gPrefArray[index].lockCol]);
  },
  getColumnProperties : function(col, elt, prop) {},
  treebox : null,
  selection : null,
  isContainer : function(index) { return false; },
  isContainerOpen : function(index) { return false; },
  isContainerEmpty : function(index) { return false; },
  isSorted : function() { return true; },
  canDropOn : function(index) { return false; },
  canDropBeforeAfter : function(index, before) { return false; },
  drop : function(row,orientation) {},
  setTree : function(out) { this.treebox = out; },
  getParentIndex: function(rowIndex) { return -1; },
  hasNextSibling: function(rowIndex, afterIndex) { return false; },
  getLevel: function(index) { return 1; },
  getImageSrc: function(row, colID) { return ""; },
  toggleOpenState : function(index) {},
  cycleHeader: function(colID, elt) {
    var index = this.selection.currentIndex;
    if (colID == gSortedColumn)
      gSortDirection = -gSortDirection;
    if (colID == gSortedColumn && gFastIndex == gPrefArray.length) {
      gPrefArray.reverse();
      if (index >= 0)
        index = gPrefArray.length - index - 1;
    }
    else {
      var pref = null;
      if (index >= 0)
        pref = gPrefArray[index];
      var old = document.getElementById(gSortedColumn);
      old.setAttribute("sortDirection", "");
      gPrefArray.sort(gSortFunction = gSortFunctions[colID]);
      gSortedColumn = colID;
      if (pref)
        index = getIndexOfPref(pref);
    }
    elt.setAttribute("sortDirection", gSortDirection > 0 ? "ascending" : "descending");
    this.treebox.invalidate();
    if (index >= 0) {
      this.selection.select(index);
      this.treebox.ensureRowIsVisible(index);
    }
    gFastIndex = gPrefArray.length;
  },
  selectionChanged : function() {},
  cycleCell: function(row, colID) {},
  isEditable: function(row, colID) {return false; },
  setCellText: function(row, colID, value) {},
  performAction: function(action) {},
  performActionOnRow: function(action, row) {},
  performActionOnCell: function(action, row, colID) {},
  isSeparator: function(index) {return false; }
};

// find the index in gPrefArray of a pref object
// either one that was looked up in gPrefHash
// or in case it was moved after sorting
function getIndexOfPref(pref)
{
  var low = -1, high = gFastIndex;
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

  for (index = gFastIndex; index < gPrefArray.length; ++index)
    if (gPrefArray[index] == pref)
      break;
  return index;
}

function getNearestIndexOfPref(pref)
{
  var low = -1, high = gFastIndex;
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

    if (/capability/.test(prefName)) // avoid displaying "private" preferences
      return;

    var index = gPrefArray.length;
    if (prefName in gPrefHash) {
      index = getIndexOfPref(gPrefHash[prefName]);
      fetchPref(prefName, index);
      view.treebox.invalidateRow(index);
      if (gSortedColumn == "lockCol" || gSortedColumn == "valueCol")
        gFastIndex = 1; // TODO: reinsert and invalidate range
    } else {
      fetchPref(prefName, index);
      if (index == gFastIndex) {
        // Keep the array sorted by reinserting the pref object
        var pref = gPrefArray.pop();
        index = getNearestIndexOfPref(pref);
        gPrefArray.splice(index, 0, pref);
        gFastIndex = gPrefArray.length;
      }
      view.treebox.rowCountChanged(index, 1);
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
        if (pref.lockCol == PREF_IS_DEFAULT_VALUE)
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
  document.title = "about:config";

  var prefCount = { value: 0 };
  var prefArray = gPrefBranch.getChildList("", prefCount);

  for (var i = 0; i < prefCount.value; ++i) 
  {
    var prefName = prefArray[i];
    if (/capability/.test(prefName)) // avoid displaying "private" preferences
      continue;

    fetchPref(prefName, gPrefArray.length);
  }

  var descending = document.getElementsByAttribute("sortDirection", "descending");
  if (descending.length) {
    gSortedColumn = descending[0].id;
    gSortDirection = -1;
  }
  else {
    var ascending = document.getElementsByAttribute("sortDirection", "ascending");
    if (ascending.length)
      gSortedColumn = ascending[0].id;
    else
      document.getElementById(gSortedColumn).setAttribute("sortDirection", "ascending");
  }
  gSortFunction = gSortFunctions[gSortedColumn];
  gPrefArray.sort(gSortFunction);
  gFastIndex = gPrefArray.length;
  
  gPrefBranch.addObserver("", gPrefListener, false);

  document.getElementById("configTree").view = view;
}

function onConfigUnload()
{
  gPrefBranch.removeObserver("", gPrefListener);
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

function updateContextMenu(popup) {
  if (view.selection.currentIndex < 0)
    return false;
  var pref = gPrefArray[view.selection.currentIndex];
  var reset = popup.lastChild;
  reset.setAttribute("disabled", pref.lockCol != PREF_IS_USER_SET);
  var modify = reset.previousSibling;
  modify.setAttribute("disabled", pref.lockCol == PREF_IS_LOCKED);
  return true;
}

function copyName()
{
  gClipboardHelper.copyString(gPrefArray[view.selection.currentIndex].prefCol);
}

function copyValue()
{
  gClipboardHelper.copyString(gPrefArray[view.selection.currentIndex].valueCol);
}

function ModifySelected()
{
  ModifyPref(gPrefArray[view.selection.currentIndex]);
}

function ResetSelected()
{
  var entry = gPrefArray[view.selection.currentIndex];
  gPrefBranch.clearUserPref(entry.prefCol);
}

function NewPref(type)
{
  var result = { value: "" };
  var dummy = { value: 0 };
  // XXX get these from a string bundle
  if (gPromptService.prompt(window,
                           "New " + gTypeStrs[type] + " value",
                           "Enter the preference name",
                           result,
                           null,
                           dummy)) {
    var pref;
    if (result.value in gPrefHash)
      pref = gPrefHash[result.value];
    else
      pref = { prefCol: result.value, lockCol: PREF_IS_DEFAULT_VALUE, typeCol: type, valueCol: "" };
    if (ModifyPref(pref))
      setTimeout(gotoPref, 0, result.value);
  }
}

function gotoPref(prefCol) {
  var index = getIndexOfPref(gPrefHash[prefCol]);
  view.selection.select(index);
  view.treebox.ensureRowIsVisible(index);
}

function ModifyPref(entry)
{
  if (entry.lockCol == PREF_IS_LOCKED)
    return false;
  var result = { value: entry.valueCol };
  var dummy = { value: 0 };
  // XXX get this from a string bundle
  if (!gPromptService.prompt(window,
                            "Enter " + gTypeStrs[entry.typeCol] + " value",
                            entry.prefCol,
                            result,
                            null,
                            dummy))
    return false;
  switch (entry.typeCol) {
    case nsIPrefBranch.PREF_BOOL:
      gPrefBranch.setBoolPref(entry.prefCol, eval(result.value));
      break;
    case nsIPrefBranch.PREF_INT:
      gPrefBranch.setIntPref(entry.prefCol, eval(result.value));
      break;
    default:
    case nsIPrefBranch.PREF_STRING:
      var supportsString = Components.classes[nsSupportsString_CONTRACTID].createInstance(nsISupportsString);
      supportsString.data = result.value;
      gPrefBranch.setComplexValue(entry.prefCol, nsISupportsString, supportsString);
      break;
  }
  return true;
}
