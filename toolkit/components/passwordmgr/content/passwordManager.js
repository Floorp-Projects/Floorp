/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*** =================== SAVED SIGNONS CODE =================== ***/
const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask",
                                  "resource://gre/modules/DeferredTask.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

let kSignonBundle;
let kObserverService;

// interface variables
let passwordmanager = null;

let showingPasswords = false;

// password-manager lists
let signons = [];
let deletedSignons = [];

let signonsTree;
let signonReloadDisplay = {
  observe: function(subject, topic, data) {
    if (topic == "passwordmgr-storage-changed") {
      switch (data) {
        case "addLogin":
        case "modifyLogin":
        case "removeLogin":
        case "removeAllLogins":
          if (!signonsTree) {
            return;
          }
          signons.length = 0;
          LoadSignons();
          // apply the filter if needed
          if (document.getElementById("filter") && document.getElementById("filter").value != "") {
            _filterPasswords();
          }
          break;
      }
      kObserverService.notifyObservers(null, "passwordmgr-dialog-updated", null);
    }
  }
}

// Formatter for localization.
let dateFormatter = new Intl.DateTimeFormat(undefined,
                      { day: "numeric", month: "short", year: "numeric" });
let dateAndTimeFormatter = new Intl.DateTimeFormat(undefined,
                             { day: "numeric", month: "short", year: "numeric",
                               hour: "numeric", minute: "numeric" });

function Startup() {
  // xpconnect to password manager interfaces
  passwordmanager = Components.classes["@mozilla.org/login-manager;1"]
                        .getService(Components.interfaces.nsILoginManager);

  // be prepared to reload the display if anything changes
  kObserverService = Components.classes["@mozilla.org/observer-service;1"]
                               .getService(Components.interfaces.nsIObserverService);
  kObserverService.addObserver(signonReloadDisplay, "passwordmgr-storage-changed", false);

  signonsTree = document.getElementById("signonsTree");
}


// TODO: Integragte to SignonsStartup?
function HandleTreeColumnClick(sortFunction, event) {
  if (event.target.nodeName != "treecol" || event.button != 0) {
    return;
  }

  let sortField = event.target.getAttribute("data-field-name");
  if (!sortField) {
    return;
  }

  sortFunction(sortField);
  Services.telemetry.getKeyedHistogramById("PWMGR_MANAGE_SORTED").add(sortField);
}

function SignonsStartup() {
  kSignonBundle = document.getElementById("signonBundle");
  document.getElementById("togglePasswords").label = kSignonBundle.getString("showPasswords");
  document.getElementById("togglePasswords").accessKey = kSignonBundle.getString("showPasswordsAccessKey");
  document.getElementById("signonsIntro").textContent = kSignonBundle.getString("loginsDescriptionAll");

  let treecols = document.getElementsByTagName("treecols")[0];
  treecols.addEventListener("click", HandleTreeColumnClick.bind(null, SignonColumnSort));

  LoadSignons();

  // filter the table if requested by caller
  if (window.arguments &&
      window.arguments[0] &&
      window.arguments[0].filterString) {
    setFilter(window.arguments[0].filterString);
    Services.telemetry.getHistogramById("PWMGR_MANAGE_OPENED").add(1);
  } else {
    Services.telemetry.getHistogramById("PWMGR_MANAGE_OPENED").add(0);
  }

  FocusFilterBox();
}

function Shutdown() {
  kObserverService.removeObserver(signonReloadDisplay, "passwordmgr-storage-changed");
}

function setFilter(aFilterString) {
  document.getElementById("filter").value = aFilterString;
  _filterPasswords();
}

let signonsTreeView = {
  // Keep track of which favicons we've fetched or started fetching.
  // Maps a login origin to a favicon URL.
  _faviconMap: new Map(),
  _filterSet: [],
  // Coalesce invalidations to avoid repeated flickering.
  _invalidateTask: new DeferredTask(() => {
    signonsTree.treeBoxObject.invalidateColumn(signonsTree.columns.siteCol);
  }, 10),
  _lastSelectedRanges: [],
  selection: null,

  rowCount: 0,
  setTree(tree) {},
  getImageSrc(row, column) {
    if (column.element.getAttribute("id") !== "siteCol") {
      return "";
    }

    const signon = this._filterSet.length ? this._filterSet[row] : signons[row];

    // We already have the favicon URL or we started to fetch (value is null).
    if (this._faviconMap.has(signon.hostname)) {
      return this._faviconMap.get(signon.hostname);
    }

    // Record the fact that we already starting fetching a favicon for this
    // origin in order to avoid multiple requests for the same origin.
    this._faviconMap.set(signon.hostname, null);

    PlacesUtils.promiseFaviconLinkUrl(signon.hostname)
      .then(faviconURI => {
        this._faviconMap.set(signon.hostname, faviconURI.spec);
        this._invalidateTask.arm();
      }).catch(Cu.reportError);

    return "";
  },
  getProgressMode(row, column) {},
  getCellValue(row, column) {},
  getCellText(row, column) {
    let time;
    let signon = this._filterSet.length ? this._filterSet[row] : signons[row];
    switch (column.id) {
      case "siteCol":
        return signon.httpRealm ?
               (signon.hostname + " (" + signon.httpRealm + ")"):
               signon.hostname;
      case "userCol":
        return signon.username || "";
      case "passwordCol":
        return signon.password || "";
      case "timeCreatedCol":
        time = new Date(signon.timeCreated);
        return dateFormatter.format(time);
      case "timeLastUsedCol":
        time = new Date(signon.timeLastUsed);
        return dateAndTimeFormatter.format(time);
      case "timePasswordChangedCol":
        time = new Date(signon.timePasswordChanged);
        return dateFormatter.format(time);
      case "timesUsedCol":
        return signon.timesUsed;
      default:
        return "";
    }
  },
  isEditable(row, col) {
    if (col.id == "userCol" || col.id == "passwordCol") {
      return true;
    }
    return false;
  },
  isSeparator(index) { return false; },
  isSorted() { return false; },
  isContainer(index) { return false; },
  cycleHeader(column) {},
  getRowProperties(row) { return ""; },
  getColumnProperties(column) { return ""; },
  getCellProperties(row, column) {
    if (column.element.getAttribute("id") == "siteCol")
      return "ltr";

    return "";
  },
  setCellText(row, col, value) {
    // If there is a filter, _filterSet needs to be used, otherwise signons is used.
    let table = signonsTreeView._filterSet.length ? signonsTreeView._filterSet : signons;
    function _editLogin(field) {
      if (value == table[row][field]) {
        return;
      }
      let existingLogin = table[row].clone();
      table[row][field] = value;
      table[row].timePasswordChanged = Date.now();
      passwordmanager.modifyLogin(existingLogin, table[row]);
      signonsTree.treeBoxObject.invalidateRow(row);
    }

    if (col.id == "userCol") {
     _editLogin("username");

    } else if (col.id == "passwordCol") {
      if (!value) {
        return;
      }
      _editLogin("password");
    }
  },
};

function SortTree(tree, view, table, column, lastSortColumn, lastSortAscending, updateSelection) {

  // remember which item was selected so we can restore it after the sort
  let selections = GetTreeSelections(tree);
  let selectedNumber = selections.length ? table[selections[0]].number : -1;

  // determine if sort is to be ascending or descending
  let ascending = (column == lastSortColumn) ? !lastSortAscending : true;

  function compareFunc(a, b) {
    let valA, valB;
    switch (column) {
      case "hostname":
        let realmA = a.httpRealm;
        let realmB = b.httpRealm;
        realmA = realmA == null ? "" : realmA.toLowerCase();
        realmB = realmB == null ? "" : realmB.toLowerCase();

        valA = a[column].toLowerCase() + realmA;
        valB = b[column].toLowerCase() + realmB;
        break;
      case "username":
      case "password":
        valA = a[column].toLowerCase();
        valB = b[column].toLowerCase();
        break;

      default:
        valA = a[column];
        valB = b[column];
    }

    if (valA < valB)
      return -1;
    if (valA > valB)
      return 1;
    return 0;
  }

  // do the sort
  table.sort(compareFunc);
  if (!ascending)
    table.reverse();

  // restore the selection
  let selectedRow = -1;
  if (selectedNumber>=0 && updateSelection) {
    for (let s=0; s<table.length; s++) {
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

function LoadSignons() {
  // loads signons into table
  try {
    signons = passwordmanager.getAllLogins();
  } catch (e) {
    signons = [];
  }
  signons.forEach(login => login.QueryInterface(Components.interfaces.nsILoginMetaInfo));
  signonsTreeView.rowCount = signons.length;

  // sort and display the table
  signonsTree.view = signonsTreeView;
  // The sort column didn't change. SortTree (called by
  // SignonColumnSort) assumes we want to toggle the sort
  // direction but here we don't so we have to trick it
  lastSignonSortAscending = !lastSignonSortAscending;
  SignonColumnSort(lastSignonSortColumn);

  // disable "remove all signons" button if there are no signons
  let element = document.getElementById("removeAllSignons");
  let toggle = document.getElementById("togglePasswords");
  if (signons.length == 0) {
    element.setAttribute("disabled", "true");
    toggle.setAttribute("disabled", "true");
  } else {
    element.removeAttribute("disabled");
    toggle.removeAttribute("disabled");
  }

  return true;
}

function GetTreeSelections(tree) {
  let selections = [];
  let select = tree.view.selection;
  if (select) {
    let count = select.getRangeCount();
    let min = new Object();
    let max = new Object();
    for (let i=0; i<count; i++) {
      select.getRangeAt(i, min, max);
      for (let k=min.value; k<=max.value; k++) {
        if (k != -1) {
          selections[selections.length] = k;
        }
      }
    }
  }
  return selections;
}

function SignonSelected() {
  let selections = GetTreeSelections(signonsTree);
  if (selections.length) {
    document.getElementById("removeSignon").removeAttribute("disabled");
  } else {
    document.getElementById("removeSignon").setAttribute("disabled", true);
  }
}

function DeleteSelectedItemFromTree
    (tree, view, table, deletedTable, removeButton, removeAllButton) {

  // Turn off tree selection notifications during the deletion
  tree.view.selection.selectEventsSuppressed = true;

  // remove selected items from list (by setting them to null) and place in deleted list
  let selections = GetTreeSelections(tree);
  for (let s=selections.length-1; s>= 0; s--) {
    let i = selections[s];
    deletedTable[deletedTable.length] = table[i];
    table[i] = null;
  }

  // collapse list by removing all the null entries
  for (let j=0; j<table.length; j++) {
    if (table[j] == null) {
      let k = j;
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
    let nextSelection = (selections[0] < table.length) ? selections[0] : table.length-1;
    tree.view.selection.select(nextSelection);
    tree.treeBoxObject.ensureRowIsVisible(nextSelection);
  } else {
    // disable buttons
    document.getElementById(removeButton).setAttribute("disabled", "true")
    document.getElementById(removeAllButton).setAttribute("disabled", "true");
  }
  tree.view.selection.selectEventsSuppressed = false;
}

function DeleteSignon() {
  let syncNeeded = (signonsTreeView._filterSet.length != 0);
  DeleteSelectedItemFromTree(signonsTree, signonsTreeView,
                             signonsTreeView._filterSet.length ? signonsTreeView._filterSet : signons,
                             deletedSignons, "removeSignon", "removeAllSignons");
  FinalizeSignonDeletions(syncNeeded);
}

function DeleteAllFromTree(tree, view, table, deletedTable, removeButton, removeAllButton) {

  // remove all items from table and place in deleted table
  for (let i=0; i<table.length; i++) {
    deletedTable[deletedTable.length] = table[i];
  }
  table.length = 0;

  // clear out selections
  view.selection.select(-1);

  // update the tree view and notify the tree
  view.rowCount = 0;

  let box = tree.treeBoxObject;
  box.rowCountChanged(0, -deletedTable.length);
  box.invalidate();


  // disable buttons
  document.getElementById(removeButton).setAttribute("disabled", "true")
  document.getElementById(removeAllButton).setAttribute("disabled", "true");
}

function DeleteAllSignons() {
  let prompter = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                           .getService(Components.interfaces.nsIPromptService);

  // Confirm the user wants to remove all passwords
  let dummy = { value: false };
  if (prompter.confirmEx(window,
                         kSignonBundle.getString("removeAllPasswordsTitle"),
                         kSignonBundle.getString("removeAllPasswordsPrompt"),
                         prompter.STD_YES_NO_BUTTONS + prompter.BUTTON_POS_1_DEFAULT,
                         null, null, null, null, dummy) == 1) // 1 == "No" button
    return;

  let syncNeeded = (signonsTreeView._filterSet.length != 0);
  DeleteAllFromTree(signonsTree, signonsTreeView,
                        signonsTreeView._filterSet.length ? signonsTreeView._filterSet : signons,
                        deletedSignons, "removeSignon", "removeAllSignons");
  FinalizeSignonDeletions(syncNeeded);
  Services.telemetry.getHistogramById("PWMGR_MANAGE_DELETED_ALL").add(1);
}

function TogglePasswordVisible() {
  if (showingPasswords || masterPasswordLogin(AskUserShowPasswords)) {
    showingPasswords = !showingPasswords;
    document.getElementById("togglePasswords").label = kSignonBundle.getString(showingPasswords ? "hidePasswords" : "showPasswords");
    document.getElementById("togglePasswords").accessKey = kSignonBundle.getString(showingPasswords ? "hidePasswordsAccessKey" : "showPasswordsAccessKey");
    document.getElementById("passwordCol").hidden = !showingPasswords;
    _filterPasswords();
  }

  // Notify observers that the password visibility toggling is
  // completed.  (Mostly useful for tests)
  kObserverService.notifyObservers(null, "passwordmgr-password-toggle-complete", null);
  Services.telemetry.getHistogramById("PWMGR_MANAGE_VISIBILITY_TOGGLED").add(showingPasswords);
}

function AskUserShowPasswords() {
  let prompter = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
  let dummy = { value: false };

  // Confirm the user wants to display passwords
  return prompter.confirmEx(window,
          null,
          kSignonBundle.getString("noMasterPasswordPrompt"), prompter.STD_YES_NO_BUTTONS,
          null, null, null, null, dummy) == 0;    // 0=="Yes" button
}

function FinalizeSignonDeletions(syncNeeded) {
  for (let s = 0; s < deletedSignons.length; s++) {
    passwordmanager.removeLogin(deletedSignons[s]);
    Services.telemetry.getHistogramById("PWMGR_MANAGE_DELETED").add(1);
  }
  // If the deletion has been performed in a filtered view, reflect the deletion in the unfiltered table.
  // See bug 405389.
  if (syncNeeded) {
    try {
      signons = passwordmanager.getAllLogins();
    } catch (e) {
      signons = [];
    }
  }
  deletedSignons.length = 0;
}

function HandleSignonKeyPress(e) {
  // If editing is currently performed, don't do anything.
  if (signonsTree.getAttribute("editing")) {
    return;
  }
  if (e.keyCode == KeyEvent.DOM_VK_DELETE ||
      (AppConstants.platform == "macosx" &&
       e.keyCode == KeyEvent.DOM_VK_BACK_SPACE))
  {
    DeleteSignon();
  }
}

function getColumnByName(column) {
  switch (column) {
    case "hostname":
      return document.getElementById("siteCol");
    case "username":
      return document.getElementById("userCol");
    case "password":
      return document.getElementById("passwordCol");
    case "timeCreated":
      return document.getElementById("timeCreatedCol");
    case "timeLastUsed":
      return document.getElementById("timeLastUsedCol");
    case "timePasswordChanged":
      return document.getElementById("timePasswordChangedCol");
    case "timesUsed":
      return document.getElementById("timesUsedCol");
  }
  return undefined;
}

let lastSignonSortColumn = "hostname";
let lastSignonSortAscending = true;

function SignonColumnSort(column) {
  // clear out the sortDirection attribute on the old column
  let lastSortedCol = getColumnByName(lastSignonSortColumn);
  lastSortedCol.removeAttribute("sortDirection");

  // sort
  lastSignonSortAscending =
    SortTree(signonsTree, signonsTreeView,
                 signonsTreeView._filterSet.length ? signonsTreeView._filterSet : signons,
                 column, lastSignonSortColumn, lastSignonSortAscending);
  lastSignonSortColumn = column;

  // set the sortDirection attribute to get the styling going
  // first we need to get the right element
  let sortedCol = getColumnByName(column);
  sortedCol.setAttribute("sortDirection", lastSignonSortAscending ?
                                          "ascending" : "descending");
}

function SignonClearFilter() {
  let singleSelection = (signonsTreeView.selection.count == 1);

  // Clear the Tree Display
  signonsTreeView.rowCount = 0;
  signonsTree.treeBoxObject.rowCountChanged(0, -signonsTreeView._filterSet.length);
  signonsTreeView._filterSet = [];

  // Just reload the list to make sure deletions are respected
  LoadSignons();

  // Restore selection
  if (singleSelection) {
    signonsTreeView.selection.clearSelection();
    for (let i = 0; i < signonsTreeView._lastSelectedRanges.length; ++i) {
      let range = signonsTreeView._lastSelectedRanges[i];
      signonsTreeView.selection.rangedSelect(range.min, range.max, true);
    }
  } else {
    signonsTreeView.selection.select(0);
  }
  signonsTreeView._lastSelectedRanges = [];

  document.getElementById("signonsIntro").textContent = kSignonBundle.getString("loginsDescriptionAll");
}

function FocusFilterBox() {
  let filterBox = document.getElementById("filter");
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
  return signons.filter(s => SignonMatchesFilter(s, aFilterValue));
}

function SignonSaveState() {
  // Save selection
  let seln = signonsTreeView.selection;
  signonsTreeView._lastSelectedRanges = [];
  let rangeCount = seln.getRangeCount();
  for (let i = 0; i < rangeCount; ++i) {
    let min = {}; let max = {};
    seln.getRangeAt(i, min, max);
    signonsTreeView._lastSelectedRanges.push({ min: min.value, max: max.value });
  }
}

function _filterPasswords() {
  let filter = document.getElementById("filter").value;
  if (filter == "") {
    SignonClearFilter();
    return;
  }

  let newFilterSet = FilterPasswords(filter, signonsTreeView);
  if (!signonsTreeView._filterSet.length) {
    // Save Display Info for the Non-Filtered mode when we first
    // enter Filtered mode.
    SignonSaveState();
  }
  signonsTreeView._filterSet = newFilterSet;

  // Clear the display
  let oldRowCount = signonsTreeView.rowCount;
  signonsTreeView.rowCount = 0;
  signonsTree.treeBoxObject.rowCountChanged(0, -oldRowCount);
  // Set up the filtered display
  signonsTreeView.rowCount = signonsTreeView._filterSet.length;
  signonsTree.treeBoxObject.rowCountChanged(0, signonsTreeView.rowCount);

  // if the view is not empty then select the first item
  if (signonsTreeView.rowCount > 0)
    signonsTreeView.selection.select(0);

  document.getElementById("signonsIntro").textContent = kSignonBundle.getString("loginsDescriptionFiltered");
}

function CopyPassword() {
  // Don't copy passwords if we aren't already showing the passwords & a master
  // password hasn't been entered.
  if (!showingPasswords && !masterPasswordLogin())
    return;
  // Copy selected signon's password to clipboard
  let clipboard = Components.classes["@mozilla.org/widget/clipboardhelper;1"].
                  getService(Components.interfaces.nsIClipboardHelper);
  let row = document.getElementById("signonsTree").currentIndex;
  let password = signonsTreeView.getCellText(row, {id : "passwordCol" });
  clipboard.copyString(password);
  Services.telemetry.getHistogramById("PWMGR_MANAGE_COPIED_PASSWORD").add(1);
}

function CopyUsername() {
  // Copy selected signon's username to clipboard
  let clipboard = Components.classes["@mozilla.org/widget/clipboardhelper;1"].
                  getService(Components.interfaces.nsIClipboardHelper);
  let row = document.getElementById("signonsTree").currentIndex;
  let username = signonsTreeView.getCellText(row, {id : "userCol" });
  clipboard.copyString(username);
  Services.telemetry.getHistogramById("PWMGR_MANAGE_COPIED_USERNAME").add(1);
}

function EditCellInSelectedRow(columnName) {
  let row = signonsTree.currentIndex;
  let columnElement = getColumnByName(columnName);
  signonsTree.startEditing(row, signonsTree.columns.getColumnFor(columnElement));
}

function UpdateContextMenu() {
  let singleSelection = (signonsTreeView.selection.count == 1);
  let menuItems = new Map();
  let menupopup = document.getElementById("signonsTreeContextMenu");
  for (let menuItem of menupopup.querySelectorAll("menuitem")) {
    menuItems.set(menuItem.id, menuItem);
  }

  if (!singleSelection) {
    for (let menuItem of menuItems.values()) {
      menuItem.setAttribute("disabled", "true");
    }
    return;
  }

  let selectedRow = signonsTree.currentIndex;

  // Disable "Copy Username" if the username is empty.
  if (signonsTreeView.getCellText(selectedRow, { id: "userCol" }) != "") {
    menuItems.get("context-copyusername").removeAttribute("disabled");
  } else {
    menuItems.get("context-copyusername").setAttribute("disabled", "true");
  }

  menuItems.get("context-editusername").removeAttribute("disabled");
  menuItems.get("context-copypassword").removeAttribute("disabled");

  // Disable "Edit Password" if the password column isn't showing.
  if (!document.getElementById("passwordCol").hidden) {
    menuItems.get("context-editpassword").removeAttribute("disabled");
  } else {
    menuItems.get("context-editpassword").setAttribute("disabled", "true");
  }
}

function masterPasswordLogin(noPasswordCallback) {
  // This doesn't harm if passwords are not encrypted
  let tokendb = Components.classes["@mozilla.org/security/pk11tokendb;1"]
                    .createInstance(Components.interfaces.nsIPK11TokenDB);
  let token = tokendb.getInternalKeyToken();

  // If there is no master password, still give the user a chance to opt-out of displaying passwords
  if (token.checkPassword(""))
    return noPasswordCallback ? noPasswordCallback() : true;

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

function escapeKeyHandler() {
  // If editing is currently performed, don't do anything.
  if (signonsTree.getAttribute("editing")) {
    return;
  }
  window.close();
}

function OpenMigrator() {
  const { MigrationUtils } = Cu.import("resource:///modules/MigrationUtils.jsm", {});
  // We pass in the type of source we're using for use in telemetry:
  MigrationUtils.showMigrationWizard(window, [MigrationUtils.MIGRATION_ENTRYPOINT_PASSWORDS]);
}
