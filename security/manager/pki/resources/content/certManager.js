/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsIFilePicker = Components.interfaces.nsIFilePicker;
const nsFilePicker = "@mozilla.org/filepicker;1";
const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const nsIX509Cert = Components.interfaces.nsIX509Cert;
const nsICertTree = Components.interfaces.nsICertTree;
const nsCertTree = "@mozilla.org/security/nsCertTree;1";
const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsDialogParamBlock = "@mozilla.org/embedcomp/dialogparam;1";
const nsIPKIParamBlock    = Components.interfaces.nsIPKIParamBlock;
const nsPKIParamBlock    = "@mozilla.org/security/pkiparamblock;1";
const nsINSSCertCache = Components.interfaces.nsINSSCertCache;
const nsNSSCertCache = "@mozilla.org/security/nsscertcache;1";

const gCertFileTypes = "*.p7b; *.crt; *.cert; *.cer; *.pem; *.der";

let { NetUtil } = Components.utils.import("resource://gre/modules/NetUtil.jsm", {});
let { Services } = Components.utils.import("resource://gre/modules/Services.jsm", {});

var key;

var selected_certs = [];
var selected_tree_items = [];
var selected_index = [];
var certdb;

var caTreeView;
var serverTreeView;
var emailTreeView;
var userTreeView;
var orphanTreeView;

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

function LoadCerts()
{
  Services.obs.addObserver(smartCardObserver, "smartcard-insert", false);
  Services.obs.addObserver(smartCardObserver, "smartcard-remove", false);

  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  var certcache = Components.classes[nsNSSCertCache].createInstance(nsINSSCertCache);
  
  certcache.cacheAllCerts();

  caTreeView = Components.classes[nsCertTree]
                    .createInstance(nsICertTree);
  caTreeView.loadCertsFromCache(certcache, nsIX509Cert.CA_CERT);
  document.getElementById('ca-tree').view = caTreeView;

  serverTreeView = Components.classes[nsCertTree]
                        .createInstance(nsICertTree);
  serverTreeView.loadCertsFromCache(certcache, nsIX509Cert.SERVER_CERT);
  document.getElementById('server-tree').view = serverTreeView;

  emailTreeView = Components.classes[nsCertTree]
                       .createInstance(nsICertTree);
  emailTreeView.loadCertsFromCache(certcache, nsIX509Cert.EMAIL_CERT);
  document.getElementById('email-tree').view = emailTreeView;

  userTreeView = Components.classes[nsCertTree]
                      .createInstance(nsICertTree);
  userTreeView.loadCertsFromCache(certcache, nsIX509Cert.USER_CERT);
  document.getElementById('user-tree').view = userTreeView;

  orphanTreeView = Components.classes[nsCertTree]
                      .createInstance(nsICertTree);
  orphanTreeView.loadCertsFromCache(certcache, nsIX509Cert.UNKNOWN_CERT);
  document.getElementById('orphan-tree').view = orphanTreeView;

  enableBackupAllButton();
}

function enableBackupAllButton()
{
  var rowCnt = userTreeView.rowCount;
  var backupAllButton=document.getElementById('mine_backupAllButton');
  backupAllButton.disabled = (rowCnt < 1);
}

function getSelectedCerts()
{
  var ca_tab = document.getElementById("ca_tab");
  var mine_tab = document.getElementById("mine_tab");
  var others_tab = document.getElementById("others_tab");
  var websites_tab = document.getElementById("websites_tab");
  var orphan_tab = document.getElementById("orphan_tab");
  var items = null;
  if (ca_tab.selected) {
    items = caTreeView.selection;
  } else if (mine_tab.selected) {
    items = userTreeView.selection;
  } else if (others_tab.selected) {
    items = emailTreeView.selection;
  } else if (websites_tab.selected) {
    items = serverTreeView.selection;
  } else if (orphan_tab.selected) {
    items = orphanTreeView.selection;
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
          cert = caTreeView.getCert(j);
        } else if (mine_tab.selected) {
          cert = userTreeView.getCert(j);
        } else if (others_tab.selected) {
          cert = emailTreeView.getCert(j);
        } else if (websites_tab.selected) {
          cert = serverTreeView.getCert(j);
        } else if (orphan_tab.selected) {
          cert = orphanTreeView.getCert(j);
        }
        if (cert) {
          var sc = selected_certs.length;
          selected_certs[sc] = cert;
          selected_index[sc] = j;
        }
      }
    }
  }
}

function getSelectedTreeItems()
{
  var ca_tab = document.getElementById("ca_tab");
  var mine_tab = document.getElementById("mine_tab");
  var others_tab = document.getElementById("others_tab");
  var websites_tab = document.getElementById("websites_tab");
  var orphan_tab = document.getElementById("orphan_tab");
  var items = null;
  if (ca_tab.selected) {
    items = caTreeView.selection;
  } else if (mine_tab.selected) {
    items = userTreeView.selection;
  } else if (others_tab.selected) {
    items = emailTreeView.selection;
  } else if (websites_tab.selected) {
    items = serverTreeView.selection;
  } else if (orphan_tab.selected) {
    items = orphanTreeView.selection;
  }
  selected_certs = [];
  selected_tree_items = [];
  selected_index = [];
  var tree_item = null;
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
          tree_item = caTreeView.getTreeItem(j);
        } else if (mine_tab.selected) {
          tree_item = userTreeView.getTreeItem(j);
        } else if (others_tab.selected) {
          tree_item = emailTreeView.getTreeItem(j);
        } else if (websites_tab.selected) {
          tree_item = serverTreeView.getTreeItem(j);
        } else if (orphan_tab.selected) {
          tree_item = orphanTreeView.getTreeItem(j);
        }
        if (tree_item) {
          var sc = selected_tree_items.length;
          selected_tree_items[sc] = tree_item;
          selected_index[sc] = j;
        }
      }
    }
  }
}

function ca_enableButtons()
{
  var items = caTreeView.selection;
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
        var tokenName = items.tree.view.getCellText(j, "tokencol");
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
  var enableExportButton=document.getElementById('ca_exportButton');
  enableExportButton.setAttribute("disabled",toggle);
  var enableDeleteButton=document.getElementById('ca_deleteButton');
  enableDeleteButton.setAttribute("disabled",toggle);
}

function mine_enableButtons()
{
  var items = userTreeView.selection;
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
  var items = serverTreeView.selection;
  var count_ranges = items.getRangeCount();

  var enable_delete = false;
  var enable_view = false;

  if (count_ranges > 0) {
    enable_delete = true;
  }

  if (count_ranges == 1) {
    var o1 = {};
    var o2 = {};
    items.getRangeAt(0, o1, o2); // the first range
    if (o1.value == o2.value) {
      // only a single item is selected
      try {
        var ti = serverTreeView.getTreeItem(o1.value);
        if (ti) {
          if (ti.cert) {
            enable_view = true;
          }
        }
      }
      catch (e) {
      }
    }
  }

  var enableViewButton=document.getElementById('websites_viewButton');
  enableViewButton.setAttribute("disabled", !enable_view);
  var enableExportButton=document.getElementById('websites_exportButton');
  enableExportButton.setAttribute("disabled", !enable_view);
  var enableDeleteButton=document.getElementById('websites_deleteButton');
  enableDeleteButton.setAttribute("disabled", !enable_delete);
}

function email_enableButtons()
{
  var items = emailTreeView.selection;
  var toggle="false";
  if (items.getRangeCount() == 0) {
    toggle="true";
  }
  var enableViewButton=document.getElementById('email_viewButton');
  enableViewButton.setAttribute("disabled",toggle);
  var enableEditButton=document.getElementById('email_editButton');
  enableEditButton.setAttribute("disabled",toggle);
  var enableExportButton=document.getElementById('email_exportButton');
  enableExportButton.setAttribute("disabled",toggle);
  var enableDeleteButton=document.getElementById('email_deleteButton');
  enableDeleteButton.setAttribute("disabled",toggle);
}

function orphan_enableButtons()
{
  var items = orphanTreeView.selection;
  var toggle="false";
  if (items.getRangeCount() == 0) {
    toggle="true";
  }
  var enableViewButton=document.getElementById('orphan_viewButton');
  enableViewButton.setAttribute("disabled",toggle);
  var enableExportButton=document.getElementById('orphan_exportButton');
  enableExportButton.setAttribute("disabled",toggle);
  var enableDeleteButton=document.getElementById('orphan_deleteButton');
  enableDeleteButton.setAttribute("disabled",toggle);
}

function backupCerts()
{
  getSelectedCerts();
  var numcerts = selected_certs.length;
  if (!numcerts)
    return;
  var bundle = document.getElementById("pippki_bundle");
  var fp = Components.classes[nsFilePicker].createInstance(nsIFilePicker);
  fp.init(window,
          bundle.getString("chooseP12BackupFileDialog"),
          nsIFilePicker.modeSave);
  fp.appendFilter(bundle.getString("file_browse_PKCS12_spec"),
                  "*.p12");
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
  var items = userTreeView.selection.selectAll();
  backupCerts();
}

function editCerts()
{
  getSelectedCerts();
  var numcerts = selected_certs.length;
  if (!numcerts)
    return;
  for (var t=0; t<numcerts; t++) {
    var cert = selected_certs[t];
    var certkey = cert.dbKey;
    if (document.getElementById("ca_tab").selected) {
      window.openDialog('chrome://pippki/content/editcacert.xul', certkey,
                        'chrome,centerscreen,modal');
    } else if (document.getElementById("others_tab").selected) {
      window.openDialog('chrome://pippki/content/editemailcert.xul', certkey,
                        'chrome,centerscreen,modal');
    }
  }
}

function restoreCerts()
{
  var bundle = document.getElementById("pippki_bundle");
  var fp = Components.classes[nsFilePicker].createInstance(nsIFilePicker);
  fp.init(window,
          bundle.getString("chooseP12RestoreFileDialog2"),
          nsIFilePicker.modeOpen);
  fp.appendFilter(bundle.getString("file_browse_PKCS12_spec"),
                  "*.p12; *.pfx");
  fp.appendFilter(bundle.getString("file_browse_Certificate_spec"),
                  gCertFileTypes);
  fp.appendFilters(nsIFilePicker.filterAll);
  if (fp.show() == nsIFilePicker.returnOK) {
    // If this is an X509 user certificate, import it as one.

    var isX509FileType = false;
    var fileTypesList = gFileTypesList.split('; ');
    for (var type in fileTypesList) {
      if (fp.file.path.endsWith(type)) {
        isX509FileType = true;
  	break;
      }
    }

    if (isX509FileType) {
      let fstream = Components.classes["@mozilla.org/network/file-input-stream;1"]
                      .createInstance(Components.interfaces.nsIFileInputStream);
      fstream.init(fp.file, -1, 0, 0);
      let dataString = NetUtil.readInputStreamToString(fstream, fstream.available());
      let dataArray = [];
      for (let i = 0; i < dataString.length; i++) {
        dataArray.push(dataString.charCodeAt(i));
      }
      fstream.close();
      let prompter = Services.ww.getNewPrompter(window);
      let interfaceRequestor = {
        getInterface: function() {
          return prompter;
        }
      };
      certdb.importUserCertificate(dataArray, dataArray.length, interfaceRequestor);
    } else {
      // Otherwise, assume it's a PKCS12 file and import it that way.
      certdb.importPKCS12File(null, fp.file);
    }

    var certcache = Components.classes[nsNSSCertCache].createInstance(nsINSSCertCache);
    certcache.cacheAllCerts();
    userTreeView.loadCertsFromCache(certcache, nsIX509Cert.USER_CERT);
    userTreeView.selection.clearSelection();
    caTreeView.loadCertsFromCache(certcache, nsIX509Cert.CA_CERT);
    caTreeView.selection.clearSelection();
    enableBackupAllButton();
  }
}

function exportCerts()
{
  getSelectedCerts();
  var numcerts = selected_certs.length;
  if (!numcerts)
    return;

  for (var t=0; t<numcerts; t++) {
    exportToFile(window, selected_certs[t]);
  }
}

function deleteCerts()
{
  getSelectedTreeItems();
  var numcerts = selected_tree_items.length;
  if (!numcerts)
    return;

  var params = Components.classes[nsDialogParamBlock].createInstance(nsIDialogParamBlock);

  var selTab = document.getElementById('certMgrTabbox').selectedItem;
  var selTabID = selTab.getAttribute('id');
  var t;

  params.SetNumberStrings(numcerts+1);

  if (selTabID == 'mine_tab') 
  {
    params.SetString(0, selTabID);
  } 
  else if (selTabID == "websites_tab") 
  {
    params.SetString(0, selTabID);
  } 
  else if (selTabID == "ca_tab") 
  {
    params.SetString(0, selTabID);
  }
  else if (selTabID == "others_tab") 
  {
    params.SetString(0, selTabID);
  }
  else if (selTabID == "orphan_tab") 
  {
    params.SetString(0, selTabID);
  }
  else
  {
    return;
  }

  params.SetInt(0,numcerts);
  for (t=0; t<numcerts; t++) 
  {
    var tree_item = selected_tree_items[t];
    var c = tree_item.cert;
    if (!c) {
      params.SetString(t+1, tree_item.hostPort);
    }
    else {
      params.SetString(t+1, c.commonName);
    }

  }

  window.openDialog('chrome://pippki/content/deletecert.xul', "",
                    'chrome,centerscreen,modal', params);
 
  if (params.GetInt(1) == 1) {
    // user closed dialog with OK
    var treeView = null;
    var loadParam = null;

    selTab = document.getElementById('certMgrTabbox').selectedItem;
    selTabID = selTab.getAttribute('id');
    if (selTabID == 'mine_tab') {
      treeView = userTreeView;
    } else if (selTabID == "others_tab") {
      treeView = emailTreeView;
    } else if (selTabID == "websites_tab") {
      treeView = serverTreeView;
    } else if (selTabID == "ca_tab") {
      treeView = caTreeView;
    } else if (selTabID == "orphan_tab") {
      treeView = orphanTreeView;
    }

    for (t=numcerts-1; t>=0; t--)
    {
      treeView.deleteEntryObject(selected_index[t]);
    }

    selected_tree_items = [];
    selected_index = [];
    treeView.selection.clearSelection();
    if (selTabID == 'mine_tab') {
      enableBackupAllButton();
    }
  }
}

function viewCerts()
{
  getSelectedCerts();
  var numcerts = selected_certs.length;
  if (!numcerts)
    return;

  for (var t=0; t<numcerts; t++) {
    viewCertHelper(window, selected_certs[t]);
  }
}

function addCACerts()
{
  var bundle = document.getElementById("pippki_bundle");
  var fp = Components.classes[nsFilePicker].createInstance(nsIFilePicker);
  fp.init(window,
          bundle.getString("importCACertsPrompt"),
          nsIFilePicker.modeOpen);
  fp.appendFilter(bundle.getString("file_browse_Certificate_spec"),
                  gCertFileTypes);
  fp.appendFilters(nsIFilePicker.filterAll);
  if (fp.show() == nsIFilePicker.returnOK) {
    certdb.importCertsFromFile(null, fp.file, nsIX509Cert.CA_CERT);
    caTreeView.loadCerts(nsIX509Cert.CA_CERT);
    caTreeView.selection.clearSelection();
  }
}

function onSmartCardChange()
{
  var certcache = Components.classes[nsNSSCertCache].createInstance(nsINSSCertCache);
  // We've change the state of the smart cards inserted or removed
  // that means the available certs may have changed. Update the display
  certcache.cacheAllCerts();
  userTreeView.loadCertsFromCache(certcache, nsIX509Cert.USER_CERT);
  userTreeView.selection.clearSelection();
  caTreeView.loadCertsFromCache(certcache, nsIX509Cert.CA_CERT);
  caTreeView.selection.clearSelection();
  serverTreeView.loadCertsFromCache(certcache, nsIX509Cert.SERVER_CERT);
  serverTreeView.selection.clearSelection();
  emailTreeView.loadCertsFromCache(certcache, nsIX509Cert.EMAIL_CERT);
  emailTreeView.selection.clearSelection();
  orphanTreeView.loadCertsFromCache(certcache, nsIX509Cert.UNKNOWN_CERT);
  orphanTreeView.selection.clearSelection();
}

function addEmailCert()
{
  var bundle = document.getElementById("pippki_bundle");
  var fp = Components.classes[nsFilePicker].createInstance(nsIFilePicker);
  fp.init(window,
          bundle.getString("importEmailCertPrompt"),
          nsIFilePicker.modeOpen);
  fp.appendFilter(bundle.getString("file_browse_Certificate_spec"),
                  gCertFileTypes);
  fp.appendFilters(nsIFilePicker.filterAll);
  if (fp.show() == nsIFilePicker.returnOK) {
    certdb.importCertsFromFile(null, fp.file, nsIX509Cert.EMAIL_CERT);
    var certcache = Components.classes[nsNSSCertCache].createInstance(nsINSSCertCache);
    certcache.cacheAllCerts();
    emailTreeView.loadCertsFromCache(certcache, nsIX509Cert.EMAIL_CERT);
    emailTreeView.selection.clearSelection();
    caTreeView.loadCertsFromCache(certcache, nsIX509Cert.CA_CERT);
    caTreeView.selection.clearSelection();
  }
}

function addWebSiteCert()
{
  var bundle = document.getElementById("pippki_bundle");
  var fp = Components.classes[nsFilePicker].createInstance(nsIFilePicker);
  fp.init(window,
          bundle.getString("importServerCertPrompt"),
          nsIFilePicker.modeOpen);
  fp.appendFilter(bundle.getString("file_browse_Certificate_spec"),
                  gCertFileTypes);
  fp.appendFilters(nsIFilePicker.filterAll);
  if (fp.show() == nsIFilePicker.returnOK) {
    certdb.importCertsFromFile(null, fp.file, nsIX509Cert.SERVER_CERT);

    var certcache = Components.classes[nsNSSCertCache].createInstance(nsINSSCertCache);
    certcache.cacheAllCerts();
    serverTreeView.loadCertsFromCache(certcache, nsIX509Cert.SERVER_CERT);
    serverTreeView.selection.clearSelection();
    caTreeView.loadCertsFromCache(certcache, nsIX509Cert.CA_CERT);
    caTreeView.selection.clearSelection();
  }
}

function addException()
{
  window.openDialog('chrome://pippki/content/exceptionDialog.xul', "",
                    'chrome,centerscreen,modal');
  var certcache = Components.classes[nsNSSCertCache].createInstance(nsINSSCertCache);
  certcache.cacheAllCerts();
  serverTreeView.loadCertsFromCache(certcache, nsIX509Cert.SERVER_CERT);
  serverTreeView.selection.clearSelection();
  orphanTreeView.loadCertsFromCache(certcache, nsIX509Cert.UNKNOWN_CERT);
  orphanTreeView.selection.clearSelection();
}
