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
    var certstuff = caCertNameList[row];
    var delim = certstuff[0];
    var certstr = certstuff.split(delim);
    if (certstr.length < 4) {
      tokenname = "PSM Certificate Database";
      certname = certstr[1];
      certkey = certstr[2];
    } else {
      tokenname = certstr[1];
      certname = certstr[2];
      certkey = certstr[3];
    }
    if (column=="certcol") return certname;
    else if (column=="tokencol") return tokenname;
    else return certkey;
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
    var certstuff = serverCertNameList[row];
    var delim = certstuff[0];
    var certstr = certstuff.split(delim);
    if (certstr.length < 4) {
      tokenname = "PSM Certificate Database";
      certname = certstr[1];
      certkey = certstr[2];
    } else {
      tokenname = certstr[1];
      certname = certstr[2];
      certkey = certstr[3];
    }
    if (column=="certcol") return certname;
    else if (column=="tokencol") return tokenname;
    else return certkey;
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
    var ki = certname.indexOf(1);
    var keystr = certname.substring(ki+1, certname.length);
    certname = certname.substring(0, ki);
    if (column=="certcol") return certname;
    else if (column=="tokencol") return tokenname;
    else return keystr;
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
    var certstuff = userCertNameList[row];
    var delim = certstuff[0];
    var certstr = certstuff.split(delim);
    if (certstr.length < 4) {
      tokenname = "PSM Certificate Database";
      certname = certstr[1];
      certkey = certstr[2];
    } else {
      tokenname = certstr[1];
      certname = certstr[2];
      certkey = certstr[3];
    }
    if (column=="certcol") return certname;
    else if (column=="tokencol") return tokenname;
    else return certkey;
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
        //var certName = items.outliner.view.getCellText(j, "certcol");
        var certDBKey = items.outliner.view.getCellText(j, "certdbkeycol");
        selected_certs[selected_certs.length] = [tokenName, certDBKey];
      }
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
    //var token = tokendb.findTokenByName(selected_certs[t][0]);
    var token = null;
    if (selected_certs[t][1].length == 0) break; // workaround
    certs[t] = certdb.getCertByDBKey(selected_certs[t][1], token);
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
    certdb.exportPKCS12File(null, fp.file, certs.length, certs);
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
  var numcerts= selected_certs.length;
  for (var t=0; t<numcerts; t++) {
    //var token = tokendb.findTokenByName(selected_certs[t][0]);
    var token = null;
    var certkey = selected_certs[t][1];
    var cert = certdb.getCertByDBKey(certkey, token);
    window.open('chrome://pippki/content/editcerts.xul', certkey,
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
  var numcerts= selected_certs.length;
/*
  var windowName = "";
  for (var t=0; t<selected_certs.length; t++) {
    if (selected_certs[t][0] &&
        selected_certs[t][0] != "PSM Certificate Database") { // token name
      windowName = selected_certs[t].join(":");
    } else {
      windowName = selected_certs[t][1];
    }
*/
  for (var t=0; t<numcerts; t++) {
    //var token = tokendb.findTokenByName(selected_certs[t][0]);
    var token = null;
    var cert = certdb.getCertByDBKey(selected_certs[t][1], token);
    alert("You want to delete \"" + cert.windowTitle + "\"");
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
  var numcerts= selected_certs.length;
  for (var t=0; t<numcerts; t++) {
    //var token = tokendb.findTokenByName(selected_certs[t][0]);
    var token = null;
    var cert = certdb.getCertByDBKey(selected_certs[t][1], token);
    cert.view();
  }
}

/* XXX future - import a DER cert from a file? */
function addCerts()
{
  alert("Add cert chosen");
}
