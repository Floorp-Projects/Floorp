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
const nsICertOutliner = Components.interfaces.nsICertOutliner;
const nsCertOutliner = "@mozilla.org/security/nsCertOutliner;1";
const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsDialogParamBlock = "@mozilla.org/embedcomp/dialogparam;1";
const nsIPKIParamBlock    = Components.interfaces.nsIPKIParamBlock;
const nsPKIParamBlock    = "@mozilla.org/security/pkiparamblock;1";


var helpURL = "chrome://help/content/help.xul";
var key;

var selected_certs = [];
var certdb;

var caOutlinerView;
var serverOutlinerView;
var emailOutlinerView;
var userOutlinerView;

function LoadCerts()
{
  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);

  caOutlinerView = Components.classes[nsCertOutliner]
                    .createInstance(nsICertOutliner);
  caOutlinerView.loadCerts(nsIX509Cert.CA_CERT);
  document.getElementById('ca-outliner')
   .outlinerBoxObject.view = caOutlinerView;

  serverOutlinerView = Components.classes[nsCertOutliner]
                        .createInstance(nsICertOutliner);
  serverOutlinerView.loadCerts(nsIX509Cert.SERVER_CERT);
  document.getElementById('server-outliner')
   .outlinerBoxObject.view = serverOutlinerView;

  emailOutlinerView = Components.classes[nsCertOutliner]
                       .createInstance(nsICertOutliner);
  emailOutlinerView.loadCerts(nsIX509Cert.EMAIL_CERT);
  document.getElementById('email-outliner')
   .outlinerBoxObject.view = emailOutlinerView; 

  userOutlinerView = Components.classes[nsCertOutliner]
                      .createInstance(nsICertOutliner);
  userOutlinerView.loadCerts(nsIX509Cert.USER_CERT);
  document.getElementById('user-outliner')
   .outlinerBoxObject.view = userOutlinerView;

  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var verifiedColText;
  if (certdb.ocspOn) {
    verifiedColText = bundle.GetStringFromName("certmgr.verifiedNoOCSP");
  } else {
    verifiedColText = bundle.GetStringFromName("certmgr.verified");
  }
  var verifiedCol = document.getElementById('verifiedcol');
  verifiedCol.setAttribute('label', verifiedColText);
}

function ReloadCerts()
{
  caOutlinerView.loadCerts(nsIX509Cert.CA_CERT);
  serverOutlinerView.loadCerts(nsIX509Cert.SERVER_CERT);
  emailOutlinerView.loadCerts(nsIX509Cert.EMAIL_CERT);
  userOutlinerView.loadCerts(nsIX509Cert.USER_CERT);
}

function getSelectedTab()
{
  var selTab = document.getElementById('certMgrTabbox').selectedItem;
  var selTabID = selTab.getAttribute('id');
  if (selTabID == 'mine_tab') {
    key = "?my_certs";
  } else if (selTabID == "others_tab") {
    key = "?others_certs";
  } else if (selTabID == "websites_tab") {
    key = "?web_certs";
  } else if (selTabID == "ca_tab") {
    key = "?ca_certs";
  }  
  var context = helpURL + key;
  return context;
}


function doHelpButton() {
   var uri = getSelectedTab();
   openHelp(uri);
}


function getSelectedCerts()
{
  var ca_tab = document.getElementById("ca_tab");
  var mine_tab = document.getElementById("mine_tab");
  var others_tab = document.getElementById("others_tab");
  var websites_tab = document.getElementById("websites_tab");
  var items = null;
  if (ca_tab.selected) {
    items = caOutlinerView.selection;
  } else if (mine_tab.selected) {
    items = userOutlinerView.selection;
  } else if (others_tab.selected) {
    items = emailOutlinerView.selection;
  } else if (websites_tab.selected) {
    items = serverOutlinerView.selection;
  }
  selected_certs = [];
  var cert = null;
  var nr = 0;
  if (items != null) nr = items.getRangeCount();
  if (nr > 0) {
    for (var i=0; i<nr; i++) {
      var o1 = {};
      var o2 = {};
      items.getRangeAt(i, o1, o2);
      var min = o1.value;
      var max = o2.value;
      for (var j=min; j<=max; j++) {
        if (ca_tab.selected) {
          cert = caOutlinerView.getCert(j);
        } else if (mine_tab.selected) {
          cert = userOutlinerView.getCert(j);
        } else if (others_tab.selected) {
          cert = emailOutlinerView.getCert(j);
        } else if (websites_tab.selected) {
          cert = serverOutlinerView.getCert(j);
        }
        if (cert)
          selected_certs[selected_certs.length] = cert;
      }
    }
  }
}

function ca_enableButtons()
{
  var items = caOutlinerView.selection;
  var nr = items.getRangeCount();
  var toggle="false";
  if (nr == 0) {
    toggle="true";
  }
  var edit_toggle=toggle;
/*
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
*/
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

function email_enableButtons()
{
  var items = emailOutlinerView.selection;
  var toggle="false";
  if (items.getRangeCount() == 0) {
    toggle="true";
  }
  var enableViewButton=document.getElementById('email_viewButton');
  enableViewButton.setAttribute("disabled",toggle);
  var enableEditButton=document.getElementById('email_editButton');
  enableEditButton.setAttribute("disabled",toggle);
  var enableDeleteButton=document.getElementById('email_deleteButton');
  enableDeleteButton.setAttribute("disabled",toggle);
}

function backupCerts()
{
  getSelectedCerts();
  var numcerts = selected_certs.length;
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var fp = Components.classes[nsFilePicker].createInstance(nsIFilePicker);
  fp.init(window,
          bundle.GetStringFromName("chooseP12BackupFileDialog"),
          nsIFilePicker.modeSave);
  fp.appendFilter("PKCS12 Files", "*.p12");
  fp.appendFilters(nsIFilePicker.filterAll);
  var rv = fp.show();
  if (rv == nsIFilePicker.returnOK || rv == nsIFilePicker.returnReplace) {
    certdb.exportPKCS12File(null, fp.file, 
                            selected_certs.length, selected_certs);
  }
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
  var numcerts = selected_certs.length;
  for (var t=0; t<numcerts; t++) {
    var cert = selected_certs[t];
    var certkey = cert.dbKey;
    var ca_tab = document.getElementById("ca_tab");
    if (ca_tab.selected) {
      window.openDialog('chrome://pippki/content/editcacert.xul', certkey,
                  'chrome,width=100,resizable=1,modal');
    } else {
      window.openDialog('chrome://pippki/content/editsslcert.xul', certkey,
                  'chrome,width=100,resizable=1,modal');
    }
  }
}

function restoreCerts()
{
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var fp = Components.classes[nsFilePicker].createInstance(nsIFilePicker);
  fp.init(window,
          bundle.GetStringFromName("chooseP12RestoreFileDialog"),
          nsIFilePicker.modeOpen);
  fp.appendFilter("PKCS12 Files", "*.p12;*.pfx");
  fp.appendFilters(nsIFilePicker.filterAll);
  if (fp.show() == nsIFilePicker.returnOK) {
    var certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
    certdb.importPKCS12File(null, fp.file);
  }
  userOutlinerView.loadCerts(nsIX509Cert.USER_CERT);
}

function deleteCerts()
{
  getSelectedCerts();
  
  var params = Components.classes[nsDialogParamBlock].getService(nsIDialogParamBlock);
  
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var selTab = document.getElementById('certMgrTabbox').selectedItem;
  var selTabID = selTab.getAttribute('id');
  if (selTabID == 'mine_tab') 
  {
    params.SetString(1,bundle.GetStringFromName("deleteUserCertFlag"));
  } 
  else if (selTabID == "websites_tab") 
  {
    params.SetString(1,bundle.GetStringFromName("deleteSslCertFlag"));
  } 
  else if (selTabID == "ca_tab") 
  {
    params.SetString(1,bundle.GetStringFromName("deleteCaCertFlag"));
  }
  else if (selTabID == "others_tab") 
  {
    params.SetString(1,bundle.GetStringFromName("deleteEmailCertFlag"));
  }
  else
  {
    return;
  }

  var numcerts = selected_certs.length;
  params.SetInt(2,numcerts);
  for (var t=0; t<numcerts; t++) 
  {
    var cert = selected_certs[t];
    params.SetString(t+3, cert.dbKey);  
  }
   
  window.openDialog('chrome://pippki/content/deletecert.xul', "",
                'chrome,resizable=1,modal',params);
 
  ReloadCerts();
}

function viewCerts()
{
  getSelectedCerts();
  var numcerts = selected_certs.length;
  for (var t=0; t<numcerts; t++) {
    selected_certs[t].view();
  }
}

/* XXX future - import a DER cert from a file? */
function addCerts()
{
  alert("Add cert chosen");
}
