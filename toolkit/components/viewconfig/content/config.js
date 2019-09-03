// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const nsIPrefLocalizedString = Ci.nsIPrefLocalizedString;
const nsISupportsString = Ci.nsISupportsString;
const nsIPrefBranch = Ci.nsIPrefBranch;
const nsIClipboardHelper = Ci.nsIClipboardHelper;

const nsClipboardHelper_CONTRACTID = "@mozilla.org/widget/clipboardhelper;1";

const gPrefBranch = Services.prefs;
const gClipboardHelper = Cc[nsClipboardHelper_CONTRACTID].getService(
  nsIClipboardHelper
);

var gLockProps = ["default", "user", "locked"];
// we get these from a string bundle
var gLockStrs = [];
var gTypeStrs = [];

const PREF_IS_DEFAULT_VALUE = 0;
const PREF_IS_MODIFIED = 1;
const PREF_IS_LOCKED = 2;

var gPrefHash = {};
var gPrefArray = [];
var gPrefView = gPrefArray; // share the JS array
var gSortedColumn = "prefCol";
var gSortFunction = null;
var gSortDirection = 1; // 1 is ascending; -1 is descending
var gFilter = null;

let gCategoriesRecordedOnce = new Set();

var view = {
  get rowCount() {
    return gPrefView.length;
  },
  getCellText(index, col) {
    if (!(index in gPrefView)) {
      return "";
    }

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
  getRowProperties(index) {
    return "";
  },
  getCellProperties(index, col) {
    if (index in gPrefView) {
      return gLockProps[gPrefView[index].lockCol];
    }

    return "";
  },
  getColumnProperties(col) {
    return "";
  },
  treebox: null,
  selection: null,
  isContainer(index) {
    return false;
  },
  isContainerOpen(index) {
    return false;
  },
  isContainerEmpty(index) {
    return false;
  },
  isSorted() {
    return true;
  },
  canDrop(index, orientation) {
    return false;
  },
  drop(row, orientation) {},
  setTree(out) {
    this.treebox = out;
  },
  getParentIndex(rowIndex) {
    return -1;
  },
  hasNextSibling(rowIndex, afterIndex) {
    return false;
  },
  getLevel(index) {
    return 1;
  },
  getImageSrc(row, col) {
    return "";
  },
  toggleOpenState(index) {},
  cycleHeader(col) {
    var index = this.selection.currentIndex;
    recordTelemetryOnce(gCategoryLabelForSortColumn[col.id]);
    if (col.id == gSortedColumn) {
      gSortDirection = -gSortDirection;
      gPrefArray.reverse();
      if (gPrefView != gPrefArray) {
        gPrefView.reverse();
      }
      if (index >= 0) {
        index = gPrefView.length - index - 1;
      }
    } else {
      var pref = null;
      if (index >= 0) {
        pref = gPrefView[index];
      }

      var old = document.getElementById(gSortedColumn);
      old.removeAttribute("sortDirection");
      gPrefArray.sort((gSortFunction = gSortFunctions[col.id]));
      if (gPrefView != gPrefArray) {
        gPrefView.sort(gSortFunction);
      }
      gSortedColumn = col.id;
      if (pref) {
        index = getViewIndexOfPref(pref);
      }
    }
    col.element.setAttribute(
      "sortDirection",
      gSortDirection > 0 ? "ascending" : "descending"
    );
    this.treebox.invalidate();
    if (index >= 0) {
      this.selection.select(index);
      this.treebox.ensureRowIsVisible(index);
    }
  },
  selectionChanged() {},
  cycleCell(row, col) {},
  isEditable(row, col) {
    return false;
  },
  setCellValue(row, col, value) {},
  setCellText(row, col, value) {},
  isSeparator(index) {
    return false;
  },
};

// find the index in gPrefView of a pref object
// or -1 if it does not exist in the filtered view
function getViewIndexOfPref(pref) {
  var low = -1,
    high = gPrefView.length;
  var index = (low + high) >> 1;
  while (index > low) {
    var mid = gPrefView[index];
    if (mid == pref) {
      return index;
    }
    if (gSortFunction(mid, pref) < 0) {
      low = index;
    } else {
      high = index;
    }
    index = (low + high) >> 1;
  }
  return -1;
}

// find the index in gPrefView where a pref object belongs
function getNearestViewIndexOfPref(pref) {
  var low = -1,
    high = gPrefView.length;
  var index = (low + high) >> 1;
  while (index > low) {
    if (gSortFunction(gPrefView[index], pref) < 0) {
      low = index;
    } else {
      high = index;
    }
    index = (low + high) >> 1;
  }
  return high;
}

// find the index in gPrefArray of a pref object
function getIndexOfPref(pref) {
  var low = -1,
    high = gPrefArray.length;
  var index = (low + high) >> 1;
  while (index > low) {
    var mid = gPrefArray[index];
    if (mid == pref) {
      return index;
    }
    if (gSortFunction(mid, pref) < 0) {
      low = index;
    } else {
      high = index;
    }
    index = (low + high) >> 1;
  }
  return index;
}

function getNearestIndexOfPref(pref) {
  var low = -1,
    high = gPrefArray.length;
  var index = (low + high) >> 1;
  while (index > low) {
    if (gSortFunction(gPrefArray[index], pref) < 0) {
      low = index;
    } else {
      high = index;
    }
    index = (low + high) >> 1;
  }
  return high;
}

var gPrefListener = {
  observe(subject, topic, prefName) {
    if (topic != "nsPref:changed") {
      return;
    }

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
    } else {
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
      } else if (gFilter) {
        // View is filtered, but nothing to update
        return;
      }

      if (addedRow) {
        view.treebox.rowCountChanged(newIndex, 1);
      }

      // Invalidate the changed range in the view
      var low = Math.min(viewIndex, newIndex);
      var high = Math.max(viewIndex, newIndex);
      view.treebox.invalidateRange(low, high);

      if (selectedIndex == viewIndex) {
        selectedIndex = newIndex;
      } else if (selectedIndex >= low && selectedIndex <= high) {
        selectedIndex += newIndex > viewIndex ? -1 : 1;
      }
      if (selectedIndex >= 0) {
        view.selection.select(selectedIndex);
        if (selectedIndex == newIndex) {
          view.treebox.ensureRowIsVisible(selectedIndex);
        }
      }
    }
  },
};

function prefObject(prefName, prefIndex) {
  this.prefCol = prefName;
}

prefObject.prototype = {
  lockCol: PREF_IS_DEFAULT_VALUE,
  typeCol: nsIPrefBranch.PREF_STRING,
  valueCol: "",
};

function fetchPref(prefName, prefIndex) {
  var pref = new prefObject(prefName);

  gPrefHash[prefName] = pref;
  gPrefArray[prefIndex] = pref;

  if (gPrefBranch.prefIsLocked(prefName)) {
    pref.lockCol = PREF_IS_LOCKED;
  } else if (gPrefBranch.prefHasUserValue(prefName)) {
    pref.lockCol = PREF_IS_MODIFIED;
  }

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
        pref.valueCol = gPrefBranch.getStringPref(prefName);
        // Try in case it's a localized string (will throw an exception if not)
        if (
          pref.lockCol == PREF_IS_DEFAULT_VALUE &&
          /^chrome:\/\/.+\/locale\/.+\.properties/.test(pref.valueCol)
        ) {
          pref.valueCol = gPrefBranch.getComplexValue(
            prefName,
            nsIPrefLocalizedString
          ).data;
        }
        break;
    }
  } catch (e) {
    // Also catch obscure cases in which you can't tell in advance
    // that the pref exists but has no user or default value...
  }
}

async function onConfigLoad() {
  let configContext = document.getElementById("configContext");
  configContext.addEventListener("popupshowing", function(event) {
    if (event.target == this) {
      updateContextMenu();
    }
  });

  let commandListeners = {
    toggleSelected: ModifySelected,
    modifySelected: ModifySelected,
    copyPref,
    copyName,
    copyValue,
    resetSelected: ResetSelected,
  };

  configContext.addEventListener("command", e => {
    if (e.target.id in commandListeners) {
      commandListeners[e.target.id]();
    }
  });

  let configString = document.getElementById("configString");
  configString.addEventListener("command", function() {
    NewPref(nsIPrefBranch.PREF_STRING);
  });

  let configInt = document.getElementById("configInt");
  configInt.addEventListener("command", function() {
    NewPref(nsIPrefBranch.PREF_INT);
  });

  let configBool = document.getElementById("configBool");
  configBool.addEventListener("command", function() {
    NewPref(nsIPrefBranch.PREF_BOOL);
  });

  let keyVKReturn = document.getElementById("keyVKReturn");
  keyVKReturn.addEventListener("command", ModifySelected);

  let textBox = document.getElementById("textbox");
  textBox.addEventListener("command", FilterPrefs);

  let configFocuSearch = document.getElementById("configFocuSearch");
  configFocuSearch.addEventListener("command", function() {
    textBox.focus();
  });

  let configFocuSearch2 = document.getElementById("configFocuSearch2");
  configFocuSearch2.addEventListener("command", function() {
    textBox.focus();
  });

  let warningButton = document.getElementById("warningButton");
  warningButton.addEventListener("command", ShowPrefs);

  let configTree = document.getElementById("configTree");
  configTree.addEventListener("select", function() {
    window.updateCommands("select");
  });

  let configTreeBody = document.getElementById("configTreeBody");
  configTreeBody.addEventListener("dblclick", function(event) {
    if (event.button == 0) {
      ModifySelected();
    }
  });

  // Load strings
  let [
    lockDefault,
    lockModified,
    lockLocked,
    typeString,
    typeInt,
    typeBool,
  ] = await document.l10n.formatValues([
    { id: "config-default" },
    { id: "config-modified" },
    { id: "config-locked" },
    { id: "config-property-string" },
    { id: "config-property-int" },
    { id: "config-property-bool" },
  ]);

  gLockStrs[PREF_IS_DEFAULT_VALUE] = lockDefault;
  gLockStrs[PREF_IS_MODIFIED] = lockModified;
  gLockStrs[PREF_IS_LOCKED] = lockLocked;
  gTypeStrs[nsIPrefBranch.PREF_STRING] = typeString;
  gTypeStrs[nsIPrefBranch.PREF_INT] = typeInt;
  gTypeStrs[nsIPrefBranch.PREF_BOOL] = typeBool;

  var showWarning = gPrefBranch.getBoolPref("general.warnOnAboutConfig");

  if (showWarning) {
    document.getElementById("warningButton").focus();
  } else {
    ShowPrefs();
  }
}

// Unhide the warning message
function ShowPrefs() {
  recordTelemetryOnce("Show");
  gPrefBranch.getChildList("").forEach(fetchPref);

  var descending = document.getElementsByAttribute(
    "sortDirection",
    "descending"
  );
  if (descending.item(0)) {
    gSortedColumn = descending[0].id;
    gSortDirection = -1;
  } else {
    var ascending = document.getElementsByAttribute(
      "sortDirection",
      "ascending"
    );
    if (ascending.item(0)) {
      gSortedColumn = ascending[0].id;
    } else {
      document
        .getElementById(gSortedColumn)
        .setAttribute("sortDirection", "ascending");
    }
  }
  gSortFunction = gSortFunctions[gSortedColumn];
  gPrefArray.sort(gSortFunction);

  gPrefBranch.addObserver("", gPrefListener);

  var configTree = document.getElementById("configTree");
  configTree.view = view;
  configTree.controllers.insertControllerAt(0, configController);

  document.getElementById("configDeck").setAttribute("selectedIndex", 1);
  document.getElementById("configTreeKeyset").removeAttribute("disabled");
  if (!document.getElementById("showWarningNextTime").checked) {
    gPrefBranch.setBoolPref("general.warnOnAboutConfig", false);
  }

  // Process about:config?filter=<string>
  var textbox = document.getElementById("textbox");
  // About URIs don't support query params, so do this manually
  var loc = document.location.href;
  var matches = /[?&]filter\=([^&]+)/i.exec(loc);
  if (matches) {
    textbox.value = decodeURIComponent(matches[1]);
  }

  // Even if we did not set the filter string via the URL query,
  // textbox might have been set via some other mechanism
  if (textbox.value) {
    FilterPrefs();
  }
  textbox.focus();
}

function onConfigUnload() {
  if (
    document.getElementById("configDeck").getAttribute("selectedIndex") == 1
  ) {
    gPrefBranch.removeObserver("", gPrefListener);
    var configTree = document.getElementById("configTree");
    configTree.view = null;
    configTree.controllers.removeController(configController);
  }
}

function FilterPrefs() {
  if (
    document.getElementById("configDeck").getAttribute("selectedIndex") != 1
  ) {
    return;
  }

  var substring = document.getElementById("textbox").value;
  // Check for "/regex/[i]"
  if (substring.charAt(0) == "/") {
    recordTelemetryOnce("RegexSearch");
    var r = substring.match(/^\/(.*)\/(i?)$/);
    try {
      gFilter = RegExp(r[1], r[2]);
    } catch (e) {
      return; // Do nothing on incomplete or bad RegExp
    }
  } else if (substring) {
    recordTelemetryOnce("Search");
    gFilter = RegExp(
      substring
        .replace(/([^* \w])/g, "\\$1")
        .replace(/^\*+/, "")
        .replace(/\*+/g, ".*"),
      "i"
    );
  } else {
    gFilter = null;
  }

  var prefCol =
    view.selection && view.selection.currentIndex < 0
      ? null
      : gPrefView[view.selection.currentIndex].prefCol;
  var oldlen = gPrefView.length;
  gPrefView = gPrefArray;
  if (gFilter) {
    gPrefView = [];
    for (var i = 0; i < gPrefArray.length; ++i) {
      if (gFilter.test(gPrefArray[i].prefCol + ";" + gPrefArray[i].valueCol)) {
        gPrefView.push(gPrefArray[i]);
      }
    }
  }
  view.treebox.invalidate();
  view.treebox.rowCountChanged(oldlen, gPrefView.length - oldlen);
  gotoPref(prefCol);
}

function prefColSortFunction(x, y) {
  if (x.prefCol > y.prefCol) {
    return gSortDirection;
  }
  if (x.prefCol < y.prefCol) {
    return -gSortDirection;
  }
  return 0;
}

function lockColSortFunction(x, y) {
  if (x.lockCol != y.lockCol) {
    return gSortDirection * (y.lockCol - x.lockCol);
  }
  return prefColSortFunction(x, y);
}

function typeColSortFunction(x, y) {
  if (x.typeCol != y.typeCol) {
    return gSortDirection * (y.typeCol - x.typeCol);
  }
  return prefColSortFunction(x, y);
}

function valueColSortFunction(x, y) {
  if (x.valueCol > y.valueCol) {
    return gSortDirection;
  }
  if (x.valueCol < y.valueCol) {
    return -gSortDirection;
  }
  return prefColSortFunction(x, y);
}

const gSortFunctions = {
  prefCol: prefColSortFunction,
  lockCol: lockColSortFunction,
  typeCol: typeColSortFunction,
  valueCol: valueColSortFunction,
};

const gCategoryLabelForSortColumn = {
  prefCol: "SortByName",
  lockCol: "SortByStatus",
  typeCol: "SortByType",
  valueCol: "SortByValue",
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
  onEvent: function onEvent(event) {},
};

function updateContextMenu() {
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
  resetSelected.setAttribute("disabled", lockCol != PREF_IS_MODIFIED);

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

function copyPref() {
  recordTelemetryOnce("Copy");
  var pref = gPrefView[view.selection.currentIndex];
  gClipboardHelper.copyString(pref.prefCol + ";" + pref.valueCol);
}

function copyName() {
  recordTelemetryOnce("CopyName");
  gClipboardHelper.copyString(gPrefView[view.selection.currentIndex].prefCol);
}

function copyValue() {
  recordTelemetryOnce("CopyValue");
  gClipboardHelper.copyString(gPrefView[view.selection.currentIndex].valueCol);
}

function ModifySelected() {
  recordTelemetryOnce("ModifyValue");
  if (view.selection.currentIndex >= 0) {
    ModifyPref(gPrefView[view.selection.currentIndex]);
  }
}

function ResetSelected() {
  recordTelemetryOnce("Reset");
  var entry = gPrefView[view.selection.currentIndex];
  gPrefBranch.clearUserPref(entry.prefCol);
}

async function NewPref(type) {
  recordTelemetryOnce("CreateNew");
  var result = { value: "" };
  var dummy = { value: 0 };

  let [newTitle, newPrompt] = await document.l10n.formatValues([
    { id: "config-new-title", args: { type: gTypeStrs[type] } },
    { id: "config-new-prompt" },
  ]);

  if (
    Services.prompt.prompt(window, newTitle, newPrompt, result, null, dummy)
  ) {
    result.value = result.value.trim();
    if (!result.value) {
      return;
    }

    var pref;
    if (result.value in gPrefHash) {
      pref = gPrefHash[result.value];
    } else {
      pref = {
        prefCol: result.value,
        lockCol: PREF_IS_DEFAULT_VALUE,
        typeCol: type,
        valueCol: "",
      };
    }
    if (ModifyPref(pref)) {
      setTimeout(gotoPref, 0, result.value);
    }
  }
}

function gotoPref(pref) {
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

async function ModifyPref(entry) {
  if (entry.lockCol == PREF_IS_LOCKED) {
    return false;
  }

  let [title] = await document.l10n.formatValues([
    { id: "config-modify-title", args: { type: gTypeStrs[entry.typeCol] } },
  ]);

  if (entry.typeCol == nsIPrefBranch.PREF_BOOL) {
    var check = { value: entry.valueCol == "false" };
    if (
      !entry.valueCol &&
      !Services.prompt.select(
        window,
        title,
        entry.prefCol,
        [false, true],
        check
      )
    ) {
      return false;
    }
    gPrefBranch.setBoolPref(entry.prefCol, check.value);
  } else {
    var result = { value: entry.valueCol };
    var dummy = { value: 0 };
    if (
      !Services.prompt.prompt(window, title, entry.prefCol, result, null, dummy)
    ) {
      return false;
    }
    if (entry.typeCol == nsIPrefBranch.PREF_INT) {
      // | 0 converts to integer or 0; - 0 to float or NaN.
      // Thus, this check should catch all cases.
      var val = result.value | 0;
      if (val != result.value - 0) {
        const [err_title, err_text] = await document.l10n.formatValues([
          { id: "config-nan-title" },
          { id: "config-nan-text" },
        ]);

        Services.prompt.alert(window, err_title, err_text);
        return false;
      }
      gPrefBranch.setIntPref(entry.prefCol, val);
    } else {
      gPrefBranch.setStringPref(entry.prefCol, result.value);
    }
  }

  Services.prefs.savePrefFile(null);
  return true;
}

function recordTelemetryOnce(categoryLabel) {
  if (!gCategoriesRecordedOnce.has(categoryLabel)) {
    // Don't raise an exception if Telemetry is not available.
    try {
      Services.telemetry
        .getHistogramById("ABOUT_CONFIG_FEATURES_USAGE")
        .add(categoryLabel);
    } catch (ex) {}
    gCategoriesRecordedOnce.add(categoryLabel);
  }
}

window.onload = onConfigLoad;
window.onunload = onConfigUnload;
