/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const nsIFilePicker = Components.interfaces.nsIFilePicker;
const nsFilePicker = "@mozilla.org/filepicker;1";
const nsIPKCS11Slot = Components.interfaces.nsIPKCS11Slot;
const nsIPKCS11Module = Components.interfaces.nsIPKCS11Module;
const nsPKCS11ModuleDB = "@mozilla.org/security/pkcs11moduledb;1";
const nsIPKCS11ModuleDB = Components.interfaces.nsIPKCS11ModuleDB;
const nsIPK11Token = Components.interfaces.nsIPK11Token;
const nsPK11TokenDB = "@mozilla.org/security/pk11tokendb;1";
const nsIPK11TokenDB = Components.interfaces.nsIPK11TokenDB;
const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsDialogParamBlock = "@mozilla.org/embedcomp/dialogparam;1";
const nsIPKCS11 = Components.interfaces.nsIPKCS11;
const nsPKCS11ContractID = "@mozilla.org/security/pkcs11;1";

var { Services } = Components.utils.import("resource://gre/modules/Services.jsm", {});

var bundle;
var secmoddb;
var skip_enable_buttons = false;

var smartCardObserver = {
  observe: function() {
    onSmartCardChange();
  }
};

function DeregisterSmartCardObservers()
{
  Services.obs.removeObserver(smartCardObserver, "smartcard-insert");
  Services.obs.removeObserver(smartCardObserver, "smartcard-remove");
}

/* Do the initial load of all PKCS# modules and list them. */
function LoadModules()
{
  bundle = document.getElementById("pippki_bundle");
  secmoddb = Components.classes[nsPKCS11ModuleDB].getService(nsIPKCS11ModuleDB);
  Services.obs.addObserver(smartCardObserver, "smartcard-insert", false);
  Services.obs.addObserver(smartCardObserver, "smartcard-remove", false);

  RefreshDeviceList();
}

function getPKCS11()
{
  return Components.classes[nsPKCS11ContractID].getService(nsIPKCS11);
}

function getNSSString(name)
{
  return document.getElementById("pipnss_bundle").getString(name);
}

function doPrompt(msg)
{
  let prompts = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].
    getService(Components.interfaces.nsIPromptService);
  prompts.alert(window, null, msg);
}

function doConfirm(msg)
{
  let prompts = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].
    getService(Components.interfaces.nsIPromptService);
  return prompts.confirm(window, null, msg);
}

function RefreshDeviceList()
{
  let modules = secmoddb.listModules();
  while (modules.hasMoreElements()) {
    let module = modules.getNext().QueryInterface(nsIPKCS11Module);
    let slotnames = [];
    let slots = module.listSlots();
    while (slots.hasMoreElements()) {
      let slot = slots.getNext().QueryInterface(nsIPKCS11Slot);
      // Token names are preferred because NSS prefers lookup by token name.
      slotnames.push(slot.tokenName ? slot.tokenName : slot.name);
    }
    AddModule(module.name, slotnames);
  }

  // Set the text on the FIPS button.
  SetFIPSButton();
}

function SetFIPSButton()
{
  var fipsButton = document.getElementById("fipsbutton");
  var label;
  if (secmoddb.isFIPSEnabled) {
   label = bundle.getString("disable_fips");
  } else {
   label = bundle.getString("enable_fips");
  }
  fipsButton.setAttribute("label", label);

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
function AddModule(module, slots)
{
  var tree = document.getElementById("device_list");
  var item  = document.createElement("treeitem");
  var row  = document.createElement("treerow");
  var cell = document.createElement("treecell");
  cell.setAttribute("label", module);
  row.appendChild(cell);
  item.appendChild(row);
  var parent = document.createElement("treechildren");
  for (let slot of slots) {
    var child_item = document.createElement("treeitem");
    var child_row = document.createElement("treerow");
    var child_cell = document.createElement("treecell");
    child_cell.setAttribute("label", slot);
    child_row.appendChild(child_cell);
    child_item.appendChild(child_row);
    child_item.setAttribute("pk11kind", "slot");
    parent.appendChild(child_item);
  }
  item.appendChild(parent);
  item.setAttribute("pk11kind", "module");
  item.setAttribute("open", "true");
  item.setAttribute("container", "true");
  tree.appendChild(item);
}

var selected_slot;
var selected_module;

/* get the slot selected by the user (can only be one-at-a-time) */
function getSelectedItem()
{
  var tree = document.getElementById("device_tree");
  if (tree.currentIndex < 0) return;
  var item = tree.contentView.getItemAtIndex(tree.currentIndex);
  selected_slot = null;
  selected_module = null;
  if (item) {
    var kind = item.getAttribute("pk11kind");
    var module_name;
    if (kind == "slot") {
      // get the module cell for this slot cell
      var cell = item.parentNode.parentNode.firstChild.firstChild;
      module_name = cell.getAttribute("label");
      var module = secmoddb.findModuleByName(module_name);
      // get the cell for the selected row (the slot to display)
      cell = item.firstChild.firstChild;
      var slot_name = cell.getAttribute("label");
      selected_slot = module.findSlotByName(slot_name);
    } else { // (kind == "module")
      // get the cell for the selected row (the module to display)
      cell = item.firstChild.firstChild;
      module_name = cell.getAttribute("label");
      selected_module = secmoddb.findModuleByName(module_name);
    }
  }
}

function enableButtons()
{
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
function ClearInfoList()
{
  let infoList = document.getElementById("info_list");
  while (infoList.hasChildNodes()) {
    infoList.removeChild(infoList.firstChild);
  }
}

function ClearDeviceList()
{
  ClearInfoList();

  skip_enable_buttons = true;
  var tree = document.getElementById("device_tree");
  tree.view.selection.clearSelection();
  skip_enable_buttons = false;

  // Remove the existing listed modules so that a refresh doesn't display the
  // module that just changed.
  let deviceList = document.getElementById("device_list");
  while (deviceList.hasChildNodes()) {
    deviceList.removeChild(deviceList.firstChild);
  }
}


// show a list of info about a slot
function showSlotInfo()
{
  var present = true;
  ClearInfoList();
  switch (selected_slot.status) {
   case nsIPKCS11Slot.SLOT_DISABLED:
     AddInfoRow(bundle.getString("devinfo_status"),
                bundle.getString("devinfo_stat_disabled"),
                "tok_status");
     present = false;
     break;
   case nsIPKCS11Slot.SLOT_NOT_PRESENT:
     AddInfoRow(bundle.getString("devinfo_status"),
                bundle.getString("devinfo_stat_notpresent"),
                "tok_status");
     present = false;
     break;
   case nsIPKCS11Slot.SLOT_UNINITIALIZED:
     AddInfoRow(bundle.getString("devinfo_status"),
                bundle.getString("devinfo_stat_uninitialized"),
                "tok_status");
     break;
   case nsIPKCS11Slot.SLOT_NOT_LOGGED_IN:
     AddInfoRow(bundle.getString("devinfo_status"),
                bundle.getString("devinfo_stat_notloggedin"),
                "tok_status");
     break;
   case nsIPKCS11Slot.SLOT_LOGGED_IN:
     AddInfoRow(bundle.getString("devinfo_status"),
                bundle.getString("devinfo_stat_loggedin"),
                "tok_status");
     break;
   case nsIPKCS11Slot.SLOT_READY:
     AddInfoRow(bundle.getString("devinfo_status"),
                bundle.getString("devinfo_stat_ready"),
                "tok_status");
     break;
  }
  AddInfoRow(bundle.getString("devinfo_desc"),
             selected_slot.desc, "slot_desc");
  AddInfoRow(bundle.getString("devinfo_manID"),
             selected_slot.manID, "slot_manID");
  AddInfoRow(bundle.getString("devinfo_hwversion"),
             selected_slot.HWVersion, "slot_hwv");
  AddInfoRow(bundle.getString("devinfo_fwversion"),
             selected_slot.FWVersion, "slot_fwv");
  if (present) {
     showTokenInfo();
  }
}

function showModuleInfo()
{
  ClearInfoList();
  AddInfoRow(bundle.getString("devinfo_modname"),
             selected_module.name, "module_name");
  AddInfoRow(bundle.getString("devinfo_modpath"),
             selected_module.libName, "module_path");
}

// add a row to the info list, as [col1 col2] (ex.: ["status" "logged in"])
function AddInfoRow(col1, col2, cell_id)
{
  var tree = document.getElementById("info_list");
  var item  = document.createElement("treeitem");
  var row  = document.createElement("treerow");
  var cell1 = document.createElement("treecell");
  cell1.setAttribute("label", col1);
  cell1.setAttribute("crop", "never");
  row.appendChild(cell1);
  var cell2 = document.createElement("treecell");
  cell2.setAttribute("label", col2);
  cell2.setAttribute("crop", "never");
  cell2.setAttribute("id", cell_id);
  row.appendChild(cell2);
  item.appendChild(row);
  tree.appendChild(item);
}

// log in to a slot
function doLogin()
{
  getSelectedItem();
  // here's the workaround - login functions are with token
  var selected_token = selected_slot.getToken();
  try {
    selected_token.login(false);
    var tok_status = document.getElementById("tok_status");
    if (selected_token.isLoggedIn()) {
      tok_status.setAttribute("label",
                              bundle.getString("devinfo_stat_loggedin"));
    } else {
      tok_status.setAttribute("label",
                              bundle.getString("devinfo_stat_notloggedin"));
    }
  } catch (e) {
    doPrompt(bundle.getString("login_failed"));
  }
  enableButtons();
}

// log out of a slot
function doLogout()
{
  getSelectedItem();
  // here's the workaround - login functions are with token
  var selected_token = selected_slot.getToken();
  try {
    selected_token.logoutAndDropAuthenticatedResources();
    var tok_status = document.getElementById("tok_status");
    if (selected_token.isLoggedIn()) {
      tok_status.setAttribute("label",
                              bundle.getString("devinfo_stat_loggedin"));
    } else {
      tok_status.setAttribute("label",
                              bundle.getString("devinfo_stat_notloggedin"));
    }
  } catch (e) {
  }
  enableButtons();
}

// load a new device
function doLoad()
{
  window.open("load_device.xul", "loaddevice", "chrome,centerscreen,modal");
  ClearDeviceList();
  RefreshDeviceList();
}

function deleteSelected()
{
  getSelectedItem();
  if (selected_module &&
      doConfirm(getNSSString("DelModuleWarning"))) {
    try {
      getPKCS11().deleteModule(selected_module.name);
    }
    catch (e) {
      doPrompt(getNSSString("DelModuleError"));
      return false;
    }
    selected_module = null;
    return true;
  }
  return false;
}

function doUnload()
{
  if (deleteSelected()) {
    ClearDeviceList();
    RefreshDeviceList();
  }
}

// handle card insertion and removal
function onSmartCardChange()
{
  var tree = document.getElementById("device_tree");
  var index = tree.currentIndex;
  tree.currentIndex = 0;
  ClearDeviceList();
  RefreshDeviceList();
  tree.currentIndex = index;
  enableButtons();
}

function changePassword()
{
  getSelectedItem();
  let params = Components.classes[nsDialogParamBlock]
                         .createInstance(nsIDialogParamBlock);
  params.SetString(1, selected_slot.tokenName);
  window.openDialog("changepassword.xul", "", "chrome,centerscreen,modal",
                    params);
  showSlotInfo();
  enableButtons();
}

// browse fs for PKCS#11 device
function doBrowseFiles()
{
  var srbundle = document.getElementById("pippki_bundle");
  var fp = Components.classes[nsFilePicker].createInstance(nsIFilePicker);
  fp.init(window,
          srbundle.getString("loadPK11TokenDialog"),
          nsIFilePicker.modeOpen);
  fp.appendFilters(nsIFilePicker.filterAll);
  if (fp.show() == nsIFilePicker.returnOK) {
    var pathbox = document.getElementById("device_path");
    pathbox.setAttribute("value", fp.file.path);
  }
}

function doLoadDevice()
{
  var name_box = document.getElementById("device_name");
  var path_box = document.getElementById("device_path");
  try {
    getPKCS11().addModule(name_box.value, path_box.value, 0, 0);
  } catch (e) {
    if (e.result == Components.results.NS_ERROR_ILLEGAL_VALUE) {
      doPrompt(getNSSString("AddModuleDup"));
    } else {
      doPrompt(getNSSString("AddModuleFailure"));
    }

    return false;
  }
  return true;
}

// -------------------------------------   Old code

function showTokenInfo()
{
  //ClearInfoList();
  var selected_token = selected_slot.getToken();
  AddInfoRow(bundle.getString("devinfo_label"),
             selected_token.tokenLabel, "tok_label");
  AddInfoRow(bundle.getString("devinfo_manID"),
             selected_token.tokenManID, "tok_manID");
  AddInfoRow(bundle.getString("devinfo_serialnum"),
             selected_token.tokenSerialNumber, "tok_sNum");
  AddInfoRow(bundle.getString("devinfo_hwversion"),
             selected_token.tokenHWVersion, "tok_hwv");
  AddInfoRow(bundle.getString("devinfo_fwversion"),
             selected_token.tokenFWVersion, "tok_fwv");
}

function toggleFIPS()
{
  if (!secmoddb.isFIPSEnabled) {
    // A restriction of FIPS mode is, the password must be set
    // In FIPS mode the password must be non-empty.
    // This is different from what we allow in NON-Fips mode.

    var tokendb = Components.classes[nsPK11TokenDB].getService(nsIPK11TokenDB);
    var internal_token = tokendb.getInternalKeyToken(); // nsIPK11Token
    if (!internal_token.hasPassword) {
      // Token has either no or an empty password.
      doPrompt(bundle.getString("fips_nonempty_password_required"));
      return;
    }
  }

  try {
    secmoddb.toggleFIPSMode();
  }
  catch (e) {
    doPrompt(bundle.getString("unable_to_toggle_fips"));
    return;
  }

  // Remove the existing listed modules so that a refresh doesn't display the
  // module that just changed.
  ClearDeviceList();

  RefreshDeviceList();
}
