/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

const nsIFilePicker = Components.interfaces.nsIFilePicker;
const nsFilePicker = "@mozilla.org/filepicker;1";
const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const nsIX509Cert = Components.interfaces.nsIX509Cert;
const nsICertTree = Components.interfaces.nsICertTree;
const nsCertTree = "@mozilla.org/security/nsCertTree;1";

const gCertFileTypes = "*.p7b; *.crt; *.cert; *.cer; *.pem; *.der";

var { NetUtil } = Components.utils.import("resource://gre/modules/NetUtil.jsm", {});
var { Services } = Components.utils.import("resource://gre/modules/Services.jsm", {});

var key;

/**
 * List of certs currently selected in the active tab.
 * @type nsIX509Cert[]
 */
var selected_certs = [];
var selected_tree_items = [];
var selected_index = [];
var certdb;

/**
 * Cert tree for the "Authorities" tab.
 * @type nsICertTree
 */
var caTreeView;
/**
 * Cert tree for the "Servers" tab.
 * @type nsICertTree
 */
var serverTreeView;
/**
 * Cert tree for the "People" tab.
 * @type nsICertTree
 */
var emailTreeView;
/**
 * Cert tree for the "Your Certificates" tab.
 * @type nsICertTree
 */
var userTreeView;
/**
 * Cert tree for the "Other" tab.
 * @type nsICertTree
 */
var orphanTreeView;

var smartCardObserver = {
  observe() {
    onSmartCardChange();
  }
};

function DeregisterSmartCardObservers() {
  Services.obs.removeObserver(smartCardObserver, "smartcard-insert");
  Services.obs.removeObserver(smartCardObserver, "smartcard-remove");
}

function LoadCerts() {
  Services.obs.addObserver(smartCardObserver, "smartcard-insert");
  Services.obs.addObserver(smartCardObserver, "smartcard-remove");

  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  var certcache = certdb.getCerts();

  caTreeView = Components.classes[nsCertTree]
                    .createInstance(nsICertTree);
  caTreeView.loadCertsFromCache(certcache, nsIX509Cert.CA_CERT);
  document.getElementById("ca-tree").view = caTreeView;

  serverTreeView = Components.classes[nsCertTree]
                        .createInstance(nsICertTree);
  serverTreeView.loadCertsFromCache(certcache, nsIX509Cert.SERVER_CERT);
  document.getElementById("server-tree").view = serverTreeView;

  emailTreeView = Components.classes[nsCertTree]
                       .createInstance(nsICertTree);
  emailTreeView.loadCertsFromCache(certcache, nsIX509Cert.EMAIL_CERT);
  document.getElementById("email-tree").view = emailTreeView;

  userTreeView = Components.classes[nsCertTree]
                      .createInstance(nsICertTree);
  userTreeView.loadCertsFromCache(certcache, nsIX509Cert.USER_CERT);
  document.getElementById("user-tree").view = userTreeView;

  orphanTreeView = Components.classes[nsCertTree]
                      .createInstance(nsICertTree);
  orphanTreeView.loadCertsFromCache(certcache, nsIX509Cert.UNKNOWN_CERT);
  document.getElementById("orphan-tree").view = orphanTreeView;

  enableBackupAllButton();
}

function enableBackupAllButton() {
  let backupAllButton = document.getElementById("mine_backupAllButton");
  backupAllButton.disabled = userTreeView.rowCount < 1;
}

function getSelectedCerts() {
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
    for (let i = 0; i < nr; i++) {
      var o1 = {};
      var o2 = {};
      items.getRangeAt(i, o1, o2);
      var min = o1.value;
      var max = o2.value;
      for (let j = min; j <= max; j++) {
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

function getSelectedTreeItems() {
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
    for (let i = 0; i < nr; i++) {
      var o1 = {};
      var o2 = {};
      items.getRangeAt(i, o1, o2);
      var min = o1.value;
      var max = o2.value;
      for (let j = min; j <= max; j++) {
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

/**
 * Returns true if nothing in the given cert tree is selected or if the
 * selection includes a container. Returns false otherwise.
 *
 * @param {nsICertTree} certTree
 * @returns {Boolean}
 */
function nothingOrContainerSelected(certTree) {
  var certTreeSelection = certTree.selection;
  var numSelectionRanges = certTreeSelection.getRangeCount();

  if (numSelectionRanges == 0) {
    return true;
  }

  for (var i = 0; i < numSelectionRanges; i++) {
    var o1 = {};
    var o2 = {};
    certTreeSelection.getRangeAt(i, o1, o2);
    var minIndex = o1.value;
    var maxIndex = o2.value;
    for (var j = minIndex; j <= maxIndex; j++) {
      if (certTree.isContainer(j)) {
        return true;
      }
    }
  }

  return false;
}

/**
 * Enables or disables buttons corresponding to a cert tree depending on what
 * is selected in the cert tree.
 *
 * @param {nsICertTree} certTree
 * @param {Array} idList A list of string identifiers for button elements to
 *    enable or disable.
 */
function enableButtonsForCertTree(certTree, idList) {
  let disableButtons = nothingOrContainerSelected(certTree);

  for (let id of idList) {
    document.getElementById(id).setAttribute("disabled", disableButtons);
  }
}

function ca_enableButtons() {
  let idList = [
    "ca_viewButton",
    "ca_editButton",
    "ca_exportButton",
    "ca_deleteButton",
  ];
  enableButtonsForCertTree(caTreeView, idList);
}

function mine_enableButtons() {
  let idList = [
    "mine_viewButton",
    "mine_backupButton",
    "mine_deleteButton",
  ];
  enableButtonsForCertTree(userTreeView, idList);
}

function websites_enableButtons() {
  let idList = [
    "websites_viewButton",
    "websites_exportButton",
    "websites_deleteButton",
  ];
  enableButtonsForCertTree(serverTreeView, idList);
}

function email_enableButtons() {
  let idList = [
    "email_viewButton",
    "email_exportButton",
    "email_deleteButton",
  ];
  enableButtonsForCertTree(emailTreeView, idList);
}

function orphan_enableButtons() {
  let idList = [
    "orphan_viewButton",
    "orphan_exportButton",
    "orphan_deleteButton",
  ];
  enableButtonsForCertTree(orphanTreeView, idList);
}

function backupCerts() {
  getSelectedCerts();
  var numcerts = selected_certs.length;
  if (numcerts == 0) {
    return;
  }

  var bundle = document.getElementById("pippki_bundle");
  var fp = Components.classes[nsFilePicker].createInstance(nsIFilePicker);
  fp.init(window,
          bundle.getString("chooseP12BackupFileDialog"),
          nsIFilePicker.modeSave);
  fp.appendFilter(bundle.getString("file_browse_PKCS12_spec"),
                  "*.p12");
  fp.appendFilters(nsIFilePicker.filterAll);
  fp.open(rv => {
    if (rv == nsIFilePicker.returnOK || rv == nsIFilePicker.returnReplace) {
      certdb.exportPKCS12File(fp.file, selected_certs.length, selected_certs);
    }
  });
}

function backupAllCerts() {
  // Select all rows, then call doBackup()
  userTreeView.selection.selectAll();
  backupCerts();
}

function editCerts() {
  getSelectedCerts();

  for (let cert of selected_certs) {
    window.openDialog("chrome://pippki/content/editcacert.xul", "",
                      "chrome,centerscreen,modal", cert);
  }
}

function restoreCerts() {
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
  fp.open(rv => {
    if (rv != nsIFilePicker.returnOK) {
      return;
    }

    // If this is an X509 user certificate, import it as one.

    var isX509FileType = false;
    var fileTypesList = gCertFileTypes.slice(1).split("; *");
    for (var type of fileTypesList) {
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
        getInterface() {
          return prompter;
        }
      };
      certdb.importUserCertificate(dataArray, dataArray.length, interfaceRequestor);
    } else {
      // Otherwise, assume it's a PKCS12 file and import it that way.
      certdb.importPKCS12File(fp.file);
    }

    var certcache = certdb.getCerts();
    userTreeView.loadCertsFromCache(certcache, nsIX509Cert.USER_CERT);
    userTreeView.selection.clearSelection();
    caTreeView.loadCertsFromCache(certcache, nsIX509Cert.CA_CERT);
    caTreeView.selection.clearSelection();
    enableBackupAllButton();
  });
}

function exportCerts() {
  getSelectedCerts();

  for (let cert of selected_certs) {
    exportToFile(window, cert);
  }
}

/**
 * Deletes the selected certs in the active tab.
 */
function deleteCerts() {
  getSelectedTreeItems();
  let numcerts = selected_tree_items.length;
  if (numcerts == 0) {
    return;
  }

  const treeViewMap = {
    "mine_tab": userTreeView,
    "websites_tab": serverTreeView,
    "ca_tab": caTreeView,
    "others_tab": emailTreeView,
    "orphan_tab": orphanTreeView,
  };
  let selTab = document.getElementById("certMgrTabbox").selectedItem;
  let selTabID = selTab.getAttribute("id");

  if (!(selTabID in treeViewMap)) {
    return;
  }

  let retVals = {
    deleteConfirmed: false,
  };
  window.openDialog("chrome://pippki/content/deletecert.xul", "",
                    "chrome,centerscreen,modal", selTabID, selected_tree_items,
                    retVals);

  if (retVals.deleteConfirmed) {
    let treeView = treeViewMap[selTabID];

    for (let t = numcerts - 1; t >= 0; t--) {
      treeView.deleteEntryObject(selected_index[t]);
    }

    selected_tree_items = [];
    selected_index = [];
    treeView.selection.clearSelection();
    if (selTabID == "mine_tab") {
      enableBackupAllButton();
    }
  }
}

function viewCerts() {
  getSelectedCerts();

  for (let cert of selected_certs) {
    viewCertHelper(window, cert);
  }
}

function addCACerts() {
  var bundle = document.getElementById("pippki_bundle");
  var fp = Components.classes[nsFilePicker].createInstance(nsIFilePicker);
  fp.init(window,
          bundle.getString("importCACertsPrompt"),
          nsIFilePicker.modeOpen);
  fp.appendFilter(bundle.getString("file_browse_Certificate_spec"),
                  gCertFileTypes);
  fp.appendFilters(nsIFilePicker.filterAll);
  fp.open(rv => {
    if (rv == nsIFilePicker.returnOK) {
      certdb.importCertsFromFile(fp.file, nsIX509Cert.CA_CERT);
      caTreeView.loadCerts(nsIX509Cert.CA_CERT);
      caTreeView.selection.clearSelection();
    }
  });
}

function onSmartCardChange() {
  var certcache = certdb.getCerts();
  // We've change the state of the smart cards inserted or removed
  // that means the available certs may have changed. Update the display
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

function addEmailCert() {
  var bundle = document.getElementById("pippki_bundle");
  var fp = Components.classes[nsFilePicker].createInstance(nsIFilePicker);
  fp.init(window,
          bundle.getString("importEmailCertPrompt"),
          nsIFilePicker.modeOpen);
  fp.appendFilter(bundle.getString("file_browse_Certificate_spec"),
                  gCertFileTypes);
  fp.appendFilters(nsIFilePicker.filterAll);
  fp.open(rv => {
    if (rv == nsIFilePicker.returnOK) {
      certdb.importCertsFromFile(fp.file, nsIX509Cert.EMAIL_CERT);
      var certcache = certdb.getCerts();
      emailTreeView.loadCertsFromCache(certcache, nsIX509Cert.EMAIL_CERT);
      emailTreeView.selection.clearSelection();
      caTreeView.loadCertsFromCache(certcache, nsIX509Cert.CA_CERT);
      caTreeView.selection.clearSelection();
    }
  });
}

function addException() {
  window.openDialog("chrome://pippki/content/exceptionDialog.xul", "",
                    "chrome,centerscreen,modal");
  var certcache = certdb.getCerts();
  serverTreeView.loadCertsFromCache(certcache, nsIX509Cert.SERVER_CERT);
  serverTreeView.selection.clearSelection();
  orphanTreeView.loadCertsFromCache(certcache, nsIX509Cert.UNKNOWN_CERT);
  orphanTreeView.selection.clearSelection();
}
