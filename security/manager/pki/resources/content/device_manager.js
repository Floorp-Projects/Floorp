/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Bob Lord <lord@netscape.com>
 *  Ian McGreer <mcgreer@netscape.com>
 */

const nsIFilePicker = Components.interfaces.nsIFilePicker;
const nsFilePicker = "@mozilla.org/filepicker;1";
const nsIPKCS11Slot = Components.interfaces.nsIPKCS11Slot;
const nsIPKCS11Module = Components.interfaces.nsIPKCS11Module;
const nsPKCS11ModuleDB = "@mozilla.org/security/pkcs11moduledb;1";
const nsIPKCS11ModuleDB = Components.interfaces.nsIPKCS11ModuleDB;
const nsIPK11Token = Components.interfaces.nsIPK11Token;
const nsPK11TokenDB = "@mozilla.org/security/pk11tokendb;1";
const nsIPK11TokenDB = Components.interfaces.nsIPK11TokenDB;

var bundle;
var secmoddb;

/* Do the initial load of all PKCS# modules and list them. */
function LoadModules()
{
  bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  secmoddb = Components.classes[nsPKCS11ModuleDB].getService(nsIPKCS11ModuleDB);
  var modules = secmoddb.listModules();
  var done = false;
  try {
    modules.isDone();
  } catch (e) { done = true; }
  while (!done) {
    var module = modules.currentItem().QueryInterface(nsIPKCS11Module);
    if (module) {
      var slotnames = [];
      var slots = module.listSlots();
      var slots_done = false;
      try {
        slots.isDone();
      } catch (e) { slots_done = true; }
      while (!slots_done) {
        var slot = slots.currentItem().QueryInterface(nsIPKCS11Slot);
        // in the ongoing discussion of whether slot names or token names
        // are to be shown, I've gone with token names because NSS will
        // prefer lookup by token name.  However, the token may not be
        // present, so maybe slot names should be listed, while token names
        // are "remembered" for lookup?
        if (slot.tokenName)
          slotnames[slotnames.length] = slot.tokenName;
        else
          slotnames[slotnames.length] = slot.name;
        try {
          slots.next();
        } catch (e) { slots_done = true; }
      }
      AddModule(module.name, slotnames);
    }
    try {
      modules.next();
    } catch (e) { done = true; }
  }
  /* Set the text on the fips button */
  SetFIPSButtonText();
}

function SetFIPSButtonText()
{
  var fipsButton = document.getElementById("fipsbutton");
  var label;
  if (secmoddb.isFIPSEnabled) {
   label = bundle.GetStringFromName("disable_fips"); 
  } else {
   label = bundle.GetStringFromName("enable_fips"); 
  }
  fipsButton.setAttribute("label", label);
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
  cell.setAttribute("class", "propertylist");
  cell.setAttribute("label", module);
  cell.setAttribute("style", "font-weight: bold");
  cell.setAttribute("crop", "never");
  row.appendChild(cell);
  item.appendChild(row);
  var parent = document.createElement("treechildren");
  for (var i = 0; i<slots.length; i++) {
    var child_item = document.createElement("treeitem");
    var child_row = document.createElement("treerow");
    var child_cell = document.createElement("treecell");
    child_cell.setAttribute("label", slots[i]);
    child_cell.setAttribute("class", "treecell-indent");
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
  var tree = document.getElementById('device_tree');
  var items = tree.selectedItems;
  selected_slot = null;
  selected_module = null;
  if (items.length > 0) {
    var kind = items[0].getAttribute("pk11kind");
    var module_name;
    if (kind == "slot") {
      // get the module cell for this slot cell
      var cell = items[0].parentNode.parentNode.firstChild.firstChild;
      module_name = cell.getAttribute("label");
      var module = secmoddb.findModuleByName(module_name);
      // get the cell for the selected row (the slot to display)
      cell = items[0].firstChild.firstChild;
      var slot_name = cell.getAttribute("label");
      selected_slot = module.findSlotByName(slot_name);
    } else { // (kind == "module")
      // get the cell for the selected row (the module to display)
      cell = items[0].firstChild.firstChild;
      module_name = cell.getAttribute("label");
      selected_module = secmoddb.findModuleByName(module_name);
    }
  }
}

function enableButtons()
{
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
        if(selected_token.needsLogin()) {
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
  var thebutton = document.getElementById('login_button');
  thebutton.setAttribute("disabled", login_toggle);
  thebutton = document.getElementById('logout_button');
  thebutton.setAttribute("disabled", logout_toggle);
  thebutton = document.getElementById('change_pw_button');
  thebutton.setAttribute("disabled", pw_toggle);
  thebutton = document.getElementById('unload_button');
  thebutton.setAttribute("disabled", unload_toggle);
  // not implemented
  //thebutton = document.getElementById('change_slotname_button');
  //thebutton.setAttribute("disabled", toggle);
}

// clear the display of information for the slot
function ClearInfoList()
{
  var info_list = document.getElementById("info_list");
  while (info_list.firstChild)
      info_list.removeChild(info_list.firstChild);
}

// show a list of info about a slot
function showSlotInfo()
{
  ClearInfoList();
  switch (selected_slot.status) {
   case nsIPKCS11Slot.SLOT_DISABLED:
     AddInfoRow(bundle.GetStringFromName("devinfo_status"), 
                bundle.GetStringFromName("devinfo_stat_disabled"), 
                "tok_status");
     break;
   case nsIPKCS11Slot.SLOT_NOT_PRESENT:
     AddInfoRow(bundle.GetStringFromName("devinfo_status"), 
                bundle.GetStringFromName("devinfo_stat_notpresent"), 
                "tok_status");
     break;
   case nsIPKCS11Slot.SLOT_UNINITIALIZED:
     AddInfoRow(bundle.GetStringFromName("devinfo_status"), 
                bundle.GetStringFromName("devinfo_stat_uninitialized"), 
                "tok_status");
     break;
   case nsIPKCS11Slot.SLOT_NOT_LOGGED_IN:
     AddInfoRow(bundle.GetStringFromName("devinfo_status"), 
                bundle.GetStringFromName("devinfo_stat_notloggedin"), 
                "tok_status");
     break;
   case nsIPKCS11Slot.SLOT_LOGGED_IN:
     AddInfoRow(bundle.GetStringFromName("devinfo_status"), 
                bundle.GetStringFromName("devinfo_stat_loggedin"), 
                "tok_status");
     break;
   case nsIPKCS11Slot.SLOT_READY:
     AddInfoRow(bundle.GetStringFromName("devinfo_status"), 
                bundle.GetStringFromName("devinfo_stat_ready"), 
                "tok_status");
     break;
  }
  AddInfoRow(bundle.GetStringFromName("devinfo_desc"), 
             selected_slot.desc, "slot_desc");
  AddInfoRow(bundle.GetStringFromName("devinfo_manID"), 
             selected_slot.manID, "slot_manID");
  AddInfoRow(bundle.GetStringFromName("devinfo_hwversion"),
             selected_slot.HWVersion, "slot_hwv");
  AddInfoRow(bundle.GetStringFromName("devinfo_fwversion"),
             selected_slot.FWVersion, "slot_fwv");
}

function showModuleInfo()
{
  ClearInfoList();
  AddInfoRow(bundle.GetStringFromName("devinfo_modname"),
             selected_module.name, "module_name");
  AddInfoRow(bundle.GetStringFromName("devinfo_modpath"),
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
                          bundle.GetStringFromName("devinfo_stat_loggedin"));
    } else {
      tok_status.setAttribute("label",
                       bundle.GetStringFromName("devinfo_stat_notloggedin"));
    }
  } catch (e) {
    var alertStr = bundle.GetStringFromName("login_failed"); 
    alert(alertStr);
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
    selected_token.logout(false);
    var tok_status = document.getElementById("tok_status");
    if (selected_token.isLoggedIn()) {
      tok_status.setAttribute("label", 
                          bundle.GetStringFromName("devinfo_stat_loggedin"));
    } else {
      tok_status.setAttribute("label",
                       bundle.GetStringFromName("devinfo_stat_notloggedin"));
    }
  } catch (e) {
  }
  enableButtons();
}

// load a new device
function doLoad()
{
  //device.loaddlg.width=300
  //device.loaddlg.height=200
  var dlgWidth = bundle.GetStringFromName("device.loaddlg.width");
  var dlgHeight = bundle.GetStringFromName("device.loaddlg.height");
  //
  window.open("load_device.xul", "loaddevice", 
              "chrome,width=" + dlgWidth + ",height="+ dlgHeight+ ",resizable=1,dialog=1,modal=1");
  var device_list = document.getElementById("device_list");
  while (device_list.firstChild)
    device_list.removeChild(device_list.firstChild);
  LoadModules();
}

function doUnload()
{
  getSelectedItem();
  if (selected_module) {
    pkcs11.deletemodule(selected_module.name);
    var device_list = document.getElementById("device_list");
    while (device_list.firstChild)
      device_list.removeChild(device_list.firstChild);
    LoadModules();
  }
}

function changePassword()
{
  getSelectedItem();
  token = selected_slot.getToken();
  window.open("changepassword.xul",
              selected_slot.tokenName, 
              "chrome,resizable=1,modal=1,dialog=1");
  showSlotInfo();
  enableButtons();
}

// browse fs for PKCS#11 device
function doBrowseFiles()
{
  var srbundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var fp = Components.classes[nsFilePicker].createInstance(nsIFilePicker);
  fp.init(window,
          srbundle.GetStringFromName("loadPK11TokenDialog"),
          nsIFilePicker.modeOpen);
  fp.appendFilters(nsIFilePicker.filterAll);
  if (fp.show() == nsIFilePicker.returnOK) {
    var pathbox = document.getElementById("device_path");
    pathbox.setAttribute("value", fp.file.persistentDescriptor);
  }
}

function doLoadDevice()
{
  var name_box = document.getElementById("device_name");
  var path_box = document.getElementById("device_path");
  pkcs11.addmodule(name_box.value, path_box.value, 0,0);
  window.close();
}

// -------------------------------------   Old code

function showTokenInfo()
{
  ClearInfoList();
  getSelectedToken();
  AddInfoRow(bundle.GetStringFromName("devinfo_label"), 
             selected_token.tokenLabel, "tok_label");
  AddInfoRow(bundle.GetStringFromName("devinfo_manID"),
             selected_token.tokenManID, "tok_manID");
  AddInfoRow(bundle.GetStringFromName("devinfo_serialnum"), 
             selected_token.tokenSerialNumber, "tok_sNum");
  AddInfoRow(bundle.GetStringFromName("devinfo_hwversion"),
             selected_token.tokenHWVersion, "tok_hwv");
  AddInfoRow(bundle.GetStringFromName("devinfo_fwversion"),
             selected_token.tokenFWVersion, "tok_fwv");
}

function toggleFIPS()
{
  secmoddb.toggleFIPSMode();
  //Remove the existing listed modules so that re-fresh doesn't 
  //display the module that just changed.
  var device_list = document.getElementById("device_list");
  while (device_list.firstChild)
    device_list.removeChild(device_list.firstChild);

  LoadModules();
}
