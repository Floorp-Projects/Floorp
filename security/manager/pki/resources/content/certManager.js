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
const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
const nsX509CertDB = "@mozilla.org/security/x509certdb;1";

var selected_certs = [];
var certdb;

function getSelectedCerts()
{
  var mine_tab = document.getElementById("mine_tab");
  //var others_tab = document.getElementById("others_tab");
  var websites_tab = document.getElementById("websites_tab");
  var tree = document.getElementById('ca_treeset');
  if (mine_tab.selected) {
    tree = document.getElementById('mine_treeset');
  } else if (websites_tab.selected) {
    tree = document.getElementById('websites_treeset');
  }
  var items = tree.selectedItems;
  if (items.length > 0) {
    selected_certs = [];
    for (var t=0; t<items.length; t++) {
      var tokenName = items[t].firstChild.lastChild.getAttribute('label');
      var certName  = items[t].firstChild.firstChild.getAttribute('label');
      selected_certs[selected_certs.length] = [tokenName, certName];
    }
  }
}

function AddItem(children, cells, prefix, idfier)
{
  var kids = document.getElementById(children);
  var item = document.createElement("treeitem");
  var row  = document.createElement("treerow");
  for (var i=0; i<cells.length; i++) {
    var cell = document.createElement("treecell");
    cell.setAttribute("class", "propertylist");
    cell.setAttribute("label", cells[i]);
    row.appendChild(cell);
  }
  item.appendChild(row);
  item.setAttribute("id", prefix + idfier);
  kids.appendChild(item);
}

function AddNameWithToken(children, cells, prefix, idfier)
{
  var kids = document.getElementById(children);
  var item = document.createElement("treeitem");
  var row  = document.createElement("treerow");
  for (var i=0; i<2; i++) {
    var cell = document.createElement("treecell");
    cell.setAttribute("class", "propertylist");
    cell.setAttribute("label", cells[i]);
    if (i==1) {
      cell.setAttribute("collapsed", "true");
    }
    row.appendChild(cell);
  }
  item.appendChild(row);
  item.setAttribute("id", prefix + idfier);
  kids.appendChild(item);
}

function GetNameList(type, node)
{
  var obj1 = {};
  var obj2 = {};
  certdb.getCertNicknames(null, type, obj1, obj2);
  var count = obj1.value;
  var certNameList = obj2.value;
  if (certNameList.length > 0) {
    certNameList.sort();
    for (var i=0; i<certNameList.length; i++) {
      var certname = certNameList[i];
      var ti = certname.indexOf(":");
      var token = "";
      if (ti > 0) {
        token = certname.substring(0, ti);
        certname = certname.substring(ti+1, certname.length);
      }
      AddNameWithToken(node, [certname, token], node + "_", i);
    }
  }
}

function LoadCertNames()
{
  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  var obj1 = {};
  var obj2 = {};
  certdb.getCertNicknames(null, 
                          Components.interfaces.nsIX509Cert.CA_CERT, 
                          obj1, obj2);
  var count = obj1.value;
  var certNameList = obj2.value;
  if (certNameList.length > 0) {
    certNameList.sort();
    var nb = 0;
    var nm = 0;
    for (var i=0; i<certNameList.length; i++) {
      var certname = certNameList[i];
      var ti = certname.indexOf(":");
      var token = "";
      if (ti > 0) {
        token = certname.substring(0, ti);
        certname = certname.substring(ti+1, certname.length);
      }
      if (token == "Builtin Object Token") {
        AddNameWithToken("builtins", [certname, token], "builtin_", nb);
        nb++;
      } else {
        AddNameWithToken("mycas", [certname, token], "myca_", nm);
        nm++;
      }
    }
  }
  GetNameList(Components.interfaces.nsIX509Cert.USER_CERT, "mine");
  GetNameList(Components.interfaces.nsIX509Cert.SERVER_CERT, "servers");
}

function ca_enableButtons()
{
  var tree = document.getElementById('ca_treeset');
  var items = tree.selectedItems;
  var toggle="false";
  if (items.length == 0) {
    toggle="true";
  }
  var enableViewButton=document.getElementById('ca_viewButton');
  enableViewButton.setAttribute("disabled",toggle);
  var enableEditButton=document.getElementById('ca_editButton');
  enableEditButton.setAttribute("disabled",toggle);
  var enableDeleteButton=document.getElementById('ca_deleteButton');
  enableDeleteButton.setAttribute("disabled",toggle);
}

function mine_enableButtons()
{
  var tree = document.getElementById('mine_treeset');
  var items = tree.selectedItems;
  var toggle="false";
  if (items.length == 0) {
    toggle="true";
  }
  var enableViewButton=document.getElementById('mine_viewButton');
  enableViewButton.setAttribute("disabled",toggle);
  var enableBackupButton=document.getElementById('mine_backupButton');
  enableBackupButton.setAttribute("disabled",toggle);
  var enableDeleteButton=document.getElementById('mine_deleteButton');
  enableDeleteButton.setAttribute("disabled",toggle);
}

function websites_enableButtons()
{
  var tree = document.getElementById('websites_treeset');
  var items = tree.selectedItems;
  var toggle="false";
  if (items.length == 0) {
    toggle="true";
  }
  var enableViewButton=document.getElementById('websites_viewButton');
  enableViewButton.setAttribute("disabled",toggle);
  var enableEditButton=document.getElementById('websites_editButton');
  enableEditButton.setAttribute("disabled",toggle);
  var enableDeleteButton=document.getElementById('websites_deleteButton');
  enableDeleteButton.setAttribute("disabled",toggle);
}

function backupCerts()
{
  getSelectedCerts();
  var numcerts= selected_certs.length;
  var certs = [];
  var windowName = "";
  for (var t=0; t<numcerts; t++) {
    if (selected_certs[t][0]) { // token name
      windowName = selected_certs[t].join(":");
    } else {
      windowName = selected_certs[t][1];
    }
    certs[t] = windowName;
  }
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var fp = Components.classes[nsFilePicker].createInstance(nsIFilePicker);
  fp.init(window,
          bundle.GetStringFromName("chooseP12BackupFileDialog"),
          nsIFilePicker.modeSave);
  fp.appendFilter("PKCS12 Files (*.p12)", "*.p12");
  fp.appendFilters(nsIFilePicker.filterAll);
  if (fp.show() == nsIFilePicker.returnOK ||
      fp.show() == nsIFilePicker.returnReplace) {
    certdb.exportPKCS12File(null, fp.file, numcerts, certs);
  }
  // don't really know it was successful...
  alert(bundle.GetStringFromName("SuccessfulP12Backup"));
}

function backupAllCerts()
{
  // Select all rows, then call doBackup()
  var tree = document.getElementById('mine_treeset');
  // XXX need to catch this in UI
  if (tree.getRowCount() == 0) {
    alert("You have no certs to backup");
  } else {
    tree.selectAll();
    backupCerts();
  }
}

function editCerts()
{
  getSelectedCerts();
  var windowName = "";
  for (var t=0; t<selected_certs.length; t++) {
    if (selected_certs[t][0]) { // token name
      windowName = selected_certs[t].join(":");
    } else {
      windowName = selected_certs[t][1];
    }
    window.open('chrome://pippki/content/editcerts.xul', windowName,
                'chrome,width=500,height=400,resizable=1');
  }
}

function restoreCerts()
{
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var fp = Components.classes[nsFilePicker].createInstance(nsIFilePicker);
  fp.init(window,
          bundle.GetStringFromName("chooseP12RestoreFileDialog"),
          nsIFilePicker.modeOpen);
  fp.appendFilter("PKCS12 Files (*.p12)", "*.p12");
  fp.appendFilters(nsIFilePicker.filterAll);
  if (fp.show() == nsIFilePicker.returnOK) {
    var certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
    certdb.importPKCS12File(null, fp.file);
  }
  // don't really know it was successful...
  alert(bundle.GetStringFromName("SuccessfulP12Restore"));
}

function deleteCerts()
{
  getSelectedCerts();
  var windowName = "";
  for (var t=0; t<selected_certs.length; t++) {
    if (selected_certs[t][0]) { // token name
      windowName = selected_certs[t].join(":");
    } else {
      windowName = selected_certs[t][1];
    }
    alert("You want to delete \"" + windowName + "\"");
/*
    window.open('chrome://pippki/content/deleteCert.xul', windowName,
                'chrome,width=500,height=400,resizable=1');
*/
  }
}

function viewCerts()
{
  getSelectedCerts();
  var windowName = "";
  for (var t=0; t<selected_certs.length; t++) {
    if (selected_certs[t][0]) { // token name
      windowName = selected_certs[t].join(":");
    } else {
      windowName = selected_certs[t][1];
    }
    window.open('chrome://pippki/content/certViewer.xul', windowName,
                'chrome');
  }
}

/* XXX future - import a DER cert from a file? */
function addCerts()
{
  alert("Add cert chosen");
}
