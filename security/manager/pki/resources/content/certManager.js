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
const nsIX509Cert = Components.interfaces.nsIX509Cert;

var selected_certs = [];
var certdb;

var caCertNameList;
var serverCertNameList;
//var emailCertNameList;
var userCertNameList;

var caOutlinerView = {
  rowCount : 50,
  setOutliner : function(outliner) {},
  getCellText : function(row, column) {
    if (row >= caCertNameList.length) return "";
    var certname = caCertNameList[row];
    var ti = certname.indexOf(":");
    var tokenname = "PSM Certificate Database";
    if (ti > 0) {
      tokenname = certname.substring(0, ti);
      certname = certname.substring(ti+1, certname.length);
    }
    if (column=="certcol") return certname;
    else return tokenname;
  },
  getRowProperties : function(row, prop) {},
  getColumnProperties : function(column, prop) {},
  getCellProperties : function(cell, prop) {},
  isContainer : function(index) { return false; }
};

var serverOutlinerView = {
  rowCount : 10,
  setOutliner : function(outliner) {},
  getCellText : function(row, column) {
    if (row >= serverCertNameList.length) return "";
    var certname = serverCertNameList[row];
    var ti = certname.indexOf(":");
    var tokenname = "PSM Certificate Database";
    if (ti > 0) {
      tokenname = certname.substring(0, ti);
      certname = certname.substring(ti+1, certname.length);
    }
    if (column=="certcol") return certname;
    else return tokenname;
  },
  getRowProperties : function(row, prop) {},
  getColumnProperties : function(column, prop) {},
  getCellProperties : function(cell, prop) {},
  isContainer : function(index) { return false; }
};

/*
var emailOutlinerView = {
  rowCount : 10,
  setOutliner : function(outliner) {},
  getCellText : function(row, column) {
    if (row >= emailCertNameList.length) return "";
    var certname = emailCertertNameList[row];
    var ti = certname.indexOf(":");
    var tokenname = "PSM Certificate Database";
    if (ti > 0) {
      tokenname = certname.substring(0, ti);
      certname = certname.substring(ti+1, certname.length);
    }
    if (column=="certcol") return certname;
    else return tokenname;
  },
  getRowProperties : function(row, prop) {},
  getColumnProperties : function(column, prop) {},
  getCellProperties : function(cell, prop) {},
  isContainer : function(index) { return false; }
};
*/

var userOutlinerView = {
  rowCount : 10,
  setOutliner : function(outliner) {},
  getCellText : function(row, column) {
    if (row >= userCertNameList.length) return "";
    var certname = userCertNameList[row];
    var ti = certname.indexOf(":");
    var tokenname = "PSM Certificate Database";
    if (ti > 0) {
      tokenname = certname.substring(0, ti);
      certname = certname.substring(ti+1, certname.length);
    }
    if (column=="certcol") return certname;
    else return tokenname;
  },
  getRowProperties : function(row, prop) {},
  getColumnProperties : function(column, prop) {},
  getCellProperties : function(cell, prop) {},
  isContainer : function(index) { return false; }
};

function getSelectedCerts()
{
  var mine_tab = document.getElementById("mine_tab");
  //var others_tab = document.getElementById("others_tab");
  var websites_tab = document.getElementById("websites_tab");
  var items = caOutlinerView.selection;
  if (mine_tab.selected) {
    items = userOutlinerView.selection;
  } else if (websites_tab.selected) {
    items = serverOutlinerView.selection;
  }
  var nr = items.getRangeCount();
  if (nr > 0) {
    selected_certs = [];
    for (var i=0; i<nr; i++) {
      var o1 = {};
      var o2 = {};
      items.getRangeAt(i, o1, o2);
      var min = o1.value;
      var max = o2.value;
      for (var j=min; j<=max; j++) {
        var tokenName = items.outliner.view.getCellText(j, "tokencol");
        var certName = items.outliner.view.getCellText(j, "certcol");
        selected_certs[selected_certs.length] = [tokenName, certName];
      }
    }
  }
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

function LoadCertNamesByType(type)
{
  var obj1 = {};
  var obj2 = {};
  certdb.getCertNicknames(null, type, obj1, obj2);
  var count = obj1.value;
  if (type == nsIX509Cert.CA_CERT) {
    caCertNameList = obj2.value;
    caCertNameList.sort();
  } else if (type == nsIX509Cert.SERVER_CERT) {
    serverCertNameList = obj2.value;
    serverCertNameList.sort();
    /*
  } else if (type == nsIX509Cert.EMAIL_CERT) {
    emailCertNameList = obj2.value;
    emailCertNameList.sort();
    */
  } else { /* if (type == nsIX509Cert.USER_CERT) */
    userCertNameList = obj2.value;
    userCertNameList.sort();
  }
}

function LoadCertNames()
{
  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  LoadCertNamesByType(nsIX509Cert.CA_CERT);
  LoadCertNamesByType(nsIX509Cert.SERVER_CERT);
  //LoadCertNamesByType(nsIX509Cert.EMAIL_CERT);
  LoadCertNamesByType(nsIX509Cert.USER_CERT);
  document.getElementById('ca-outliner')
   .outlinerBoxObject.view = caOutlinerView;
  document.getElementById('server-outliner')
   .outlinerBoxObject.view = serverOutlinerView;
  /*document.getElementById('email-outliner')
   .outlinerBoxObject.view = emailOutlinerView; */
  document.getElementById('user-outliner')
   .outlinerBoxObject.view = userOutlinerView;
}

function ca_enableButtons()
{
  var items = caOutlinerView.selection;
  var nr = items.getRangeCount();
  var toggle="false";
  if (nr == 0) {
    toggle="true";
  }
  var edit_toggle="true";
  if (nr > 0) {
    for (var i=0; i<nr; i++) {
      var o1 = {};
      var o2 = {};
      items.getRangeAt(i, o1, o2);
      var min = o1.value;
      var max = o2.value;
      var stop = false;
      for (var j=min; j<=max; j++) {
        var tokenName = items.outliner.view.getCellText(j, "tokencol");
	if (tokenName == "Builtin Object Token") { stop = true; } break;
      }
      if (stop) break;
    }
    if (i == nr) {
      edit_toggle="false";
    }
  }
  var enableViewButton=document.getElementById('ca_viewButton');
  enableViewButton.setAttribute("disabled",toggle);
  var enableEditButton=document.getElementById('ca_editButton');
  enableEditButton.setAttribute("disabled",edit_toggle);
  var enableDeleteButton=document.getElementById('ca_deleteButton');
  enableDeleteButton.setAttribute("disabled",toggle);
}

function mine_enableButtons()
{
  var items = userOutlinerView.selection;
  var toggle="false";
  if (items.getRangeCount() == 0) {
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
  var items = serverOutlinerView.selection;
  var toggle="false";
  if (items.getRangeCount() == 0) {
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
    if (selected_certs[t][0] &&
        selected_certs[t][0] != "PSM Certificate Database") { // token name
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
  var items = userOutlinerView.selection.selectAll();
  backupCerts();
}

function editCerts()
{
  getSelectedCerts();
  var windowName = "";
  for (var t=0; t<selected_certs.length; t++) {
    if (selected_certs[t][0] &&
        selected_certs[t][0] != "PSM Certificate Database") { // token name
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
  LoadCertNames();
}

function deleteCerts()
{
  getSelectedCerts();
  var windowName = "";
  for (var t=0; t<selected_certs.length; t++) {
    if (selected_certs[t][0] &&
        selected_certs[t][0] != "PSM Certificate Database") { // token name
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
  LoadCertNames();
}

function viewCerts()
{
  getSelectedCerts();
  var windowName = "";
  for (var t=0; t<selected_certs.length; t++) {
    if (selected_certs[t][0] && 
        selected_certs[t][0] != "PSM Certificate Database") { // token name
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
