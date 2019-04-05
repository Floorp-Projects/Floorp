/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

var secmoddb;
var skip_enable_buttons = false;

/* Do the initial load of all PKCS# modules and list them. */
function LoadModules() {
  secmoddb = Cc["@mozilla.org/security/pkcs11moduledb;1"].getService(Ci.nsIPKCS11ModuleDB);
  RefreshDeviceList();
}

async function doPrompt(l10n_id) {
  let [msg] = await document.l10n.formatValues([{id: l10n_id}]);
  Services.prompt.alert(window, null, msg);
}

async function doConfirm(l10n_id) {
  let [msg] = await document.l10n.formatValues([{id: l10n_id}]);
  return Services.prompt.confirm(window, null, msg);
}

function RefreshDeviceList() {
  for (let module of secmoddb.listModules()) {
    let slots = module.listSlots();
    AddModule(module, slots);
  }

  // Set the text on the FIPS button.
  SetFIPSButton();
}

function SetFIPSButton() {
  var fipsButton = document.getElementById("fipsbutton");
  if (secmoddb.isFIPSEnabled) {
    document.l10n.setAttributes(fipsButton, "devmgr-button-disable-fips");
  } else {
    document.l10n.setAttributes(fipsButton, "devmgr-button-enable-fips");
  }

  var can_toggle = secmoddb.canToggleFIPS;
  if (can_toggle) {
    fipsButton.removeAttribute("disabled");
  } else {
    fipsButton.setAttribute("disabled", "true");
  }
}

/* Add a module to the tree.  slots is the array of slots in the module,
 * to be represented as children.
 */
function AddModule(module, slots) {
  var tree = document.getElementById("device_list");
  var item  = document.createXULElement("treeitem");
  var row  = document.createXULElement("treerow");
  var cell = document.createXULElement("treecell");
  cell.setAttribute("label", module.name);
  row.appendChild(cell);
  item.appendChild(row);
  var parent = document.createXULElement("treechildren");
  for (let slot of slots) {
    var child_item = document.createXULElement("treeitem");
    var child_row = document.createXULElement("treerow");
    var child_cell = document.createXULElement("treecell");
    child_cell.setAttribute("label", slot.name);
    child_row.appendChild(child_cell);
    child_item.appendChild(child_row);
    child_item.setAttribute("pk11kind", "slot");
    // 'slot' is an attribute on any HTML element, hence 'slotObject' instead.
    child_item.slotObject = slot;
    parent.appendChild(child_item);
  }
  item.appendChild(parent);
  item.setAttribute("pk11kind", "module");
  item.module = module;
  item.setAttribute("open", "true");
  item.setAttribute("container", "true");
  tree.appendChild(item);
}

var selected_slot;
var selected_module;

/* get the slot selected by the user (can only be one-at-a-time) */
function getSelectedItem() {
  let tree = document.getElementById("device_tree");
  if (tree.currentIndex < 0) {
    return;
  }
  let item = tree.view.getItemAtIndex(tree.currentIndex);
  selected_slot = null;
  selected_module = null;
  if (item) {
    let kind = item.getAttribute("pk11kind");
    if (kind == "slot") {
      selected_slot = item.slotObject;
    } else { // (kind == "module")
      selected_module = item.module;
    }
  }
}

function enableButtons() {
  if (skip_enable_buttons) {
    return;
  }

  var login_toggle = "true";
  var logout_toggle = "true";
  var pw_toggle = "true";
  var unload_toggle = "true";
  getSelectedItem();
  if (selected_module) {
    unload_toggle = "false";
    showModuleInfo();
  } else if (selected_slot) {
    // here's the workaround - login functions are all with token,
    // so grab the token type
    var selected_token = selected_slot.getToken();
    if (selected_token != null) {
      if (selected_token.needsLogin() || !(selected_token.needsUserInit)) {
        pw_toggle = "false";
        if (selected_token.needsLogin()) {
          if (selected_token.isLoggedIn()) {
            logout_toggle = "false";
          } else {
            login_toggle = "false";
          }
        }
      }

      if (!Services.policies.isAllowed("createMasterPassword") &&
          selected_token.isInternalKeyToken &&
          !selected_token.hasPassword) {
        pw_toggle = "true";
      }
    }
    showSlotInfo();
  }
  document.getElementById("login_button")
          .setAttribute("disabled", login_toggle);
  document.getElementById("logout_button")
          .setAttribute("disabled", logout_toggle);
  document.getElementById("change_pw_button")
          .setAttribute("disabled", pw_toggle);
  document.getElementById("unload_button")
          .setAttribute("disabled", unload_toggle);
}

// clear the display of information for the slot
function ClearInfoList() {
  let infoList = document.getElementById("info_list");
  while (infoList.hasChildNodes()) {
    infoList.firstChild.remove();
  }
}

function ClearDeviceList() {
  ClearInfoList();

  skip_enable_buttons = true;
  var tree = document.getElementById("device_tree");
  tree.view.selection.clearSelection();
  skip_enable_buttons = false;

  // Remove the existing listed modules so that a refresh doesn't display the
  // module that just changed.
  let deviceList = document.getElementById("device_list");
  while (deviceList.hasChildNodes()) {
    deviceList.firstChild.remove();
  }
}


// show a list of info about a slot
function showSlotInfo() {
  var present = true;
  ClearInfoList();
  switch (selected_slot.status) {
   case Ci.nsIPKCS11Slot.SLOT_DISABLED:
     AddInfoRow("devinfo-status", {l10nID: "devinfo-status-disabled"}, "tok_status");
     present = false;
     break;
   case Ci.nsIPKCS11Slot.SLOT_NOT_PRESENT:
     AddInfoRow("devinfo-status", {l10nID: "devinfo-status-not-present"}, "tok_status");
     present = false;
     break;
   case Ci.nsIPKCS11Slot.SLOT_UNINITIALIZED:
     AddInfoRow("devinfo-status", {l10nID: "devinfo-status-uninitialized"}, "tok_status");
     break;
   case Ci.nsIPKCS11Slot.SLOT_NOT_LOGGED_IN:
     AddInfoRow("devinfo-status", {l10nID: "devinfo-status-not-logged-in"}, "tok_status");
     break;
   case Ci.nsIPKCS11Slot.SLOT_LOGGED_IN:
     AddInfoRow("devinfo-status", {l10nID: "devinfo-status-logged-in"}, "tok_status");
     break;
   case Ci.nsIPKCS11Slot.SLOT_READY:
     AddInfoRow("devinfo-status", {l10nID: "devinfo-status-ready"}, "tok_status");
     break;
   default:
     return;
  }
  AddInfoRow("devinfo-desc", {label: selected_slot.desc}, "slot_desc");
  AddInfoRow("devinfo-man-id", {label: selected_slot.manID}, "slot_manID");
  AddInfoRow("devinfo-hwversion", {label: selected_slot.HWVersion}, "slot_hwv");
  AddInfoRow("devinfo-fwversion", {label: selected_slot.FWVersion}, "slot_fwv");
  if (present) {
     showTokenInfo();
  }
}

function showModuleInfo() {
  ClearInfoList();
  AddInfoRow("devinfo-modname", {label: selected_module.name}, "module_name");
  AddInfoRow("devinfo-modpath", {label: selected_module.libName}, "module_path");
}

// add a row to the info list, as [col1 col2] (ex.: ["status" "logged in"])
function AddInfoRow(l10nID, col2, cell_id) {
  var tree = document.getElementById("info_list");
  var item  = document.createXULElement("treeitem");
  var row  = document.createXULElement("treerow");
  var cell1 = document.createXULElement("treecell");
  document.l10n.setAttributes(cell1, l10nID);
  cell1.setAttribute("crop", "never");
  row.appendChild(cell1);
  var cell2 = document.createXULElement("treecell");
  if (col2.l10nID) {
    document.l10n.setAttributes(cell2, col2.l10nID);
  } else {
    cell2.setAttribute("label", col2.label);
  }
  cell2.setAttribute("crop", "never");
  cell2.setAttribute("id", cell_id);
  row.appendChild(cell2);
  item.appendChild(row);
  tree.appendChild(item);
}

// log in to a slot
function doLogin() {
  getSelectedItem();
  // here's the workaround - login functions are with token
  var selected_token = selected_slot.getToken();
  try {
    selected_token.login(false);
    var tok_status = document.getElementById("tok_status");
    if (selected_token.isLoggedIn()) {
      document.l10n.setAttributes(tok_status, "devinfo-status-logged-in");
    } else {
      document.l10n.setAttributes(tok_status, "devinfo-status-not-logged-in");
    }
  } catch (e) {
    doPrompt("login-failed");
  }
  enableButtons();
}

// log out of a slot
function doLogout() {
  getSelectedItem();
  // here's the workaround - login functions are with token
  var selected_token = selected_slot.getToken();
  try {
    selected_token.logoutAndDropAuthenticatedResources();
    var tok_status = document.getElementById("tok_status");
    if (selected_token.isLoggedIn()) {
      document.l10n.setAttributes(tok_status, "devinfo-status-logged-in");
    } else {
      document.l10n.setAttributes(tok_status, "devinfo-status-not-logged-in");
    }
  } catch (e) {
  }
  enableButtons();
}

// load a new device
function doLoad() {
  window.open("load_device.xul", "loaddevice", "chrome,centerscreen,modal");
  ClearDeviceList();
  RefreshDeviceList();
}

async function deleteSelected() {
  getSelectedItem();
  if (selected_module &&
      (await doConfirm("del-module-warning"))) {
    try {
      secmoddb.deleteModule(selected_module.name);
    } catch (e) {
      doPrompt("del-module-error");
      return false;
    }
    selected_module = null;
    return true;
  }
  return false;
}

async function doUnload() {
  if (await deleteSelected()) {
    ClearDeviceList();
    RefreshDeviceList();
  }
}

function changePassword() {
  getSelectedItem();
  let params = Cc["@mozilla.org/embedcomp/dialogparam;1"]
                 .createInstance(Ci.nsIDialogParamBlock);
  let objects = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
  objects.appendElement(selected_slot.getToken());
  params.objects = objects;
  window.openDialog("changepassword.xul", "", "chrome,centerscreen,modal",
                    params);
  showSlotInfo();
  enableButtons();
}

// -------------------------------------   Old code

function showTokenInfo() {
  var selected_token = selected_slot.getToken();
  AddInfoRow("devinfo-label", {label: selected_token.tokenName}, "tok_label");
  AddInfoRow("devinfo-man-id", {label: selected_token.tokenManID}, "tok_manID");
  AddInfoRow("devinfo-serialnum", {label: selected_token.tokenSerialNumber}, "tok_sNum");
  AddInfoRow("devinfo-hwversion", {label: selected_token.tokenHWVersion}, "tok_hwv");
  AddInfoRow("devinfo-fwversion", {label: selected_token.tokenFWVersion}, "tok_fwv");
}

function toggleFIPS() {
  if (!secmoddb.isFIPSEnabled) {
    // A restriction of FIPS mode is, the password must be set
    // In FIPS mode the password must be non-empty.
    // This is different from what we allow in NON-Fips mode.

    var tokendb = Cc["@mozilla.org/security/pk11tokendb;1"].getService(Ci.nsIPK11TokenDB);
    var internal_token = tokendb.getInternalKeyToken(); // nsIPK11Token
    if (!internal_token.hasPassword) {
      // Token has either no or an empty password.
      doPrompt("fips-nonempty-password-required");
      return;
    }
  }

  try {
    secmoddb.toggleFIPSMode();
  } catch (e) {
    doPrompt("unable-to-toggle-fips");
    return;
  }

  // Remove the existing listed modules so that a refresh doesn't display the
  // module that just changed.
  ClearDeviceList();

  RefreshDeviceList();
}
