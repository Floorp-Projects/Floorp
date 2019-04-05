/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

const gCertFileTypes = "*.p7b; *.crt; *.cert; *.cer; *.pem; *.der";

var { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

var key;

var certdialogs = Cc["@mozilla.org/nsCertificateDialogs;1"].getService(Ci.nsICertificateDialogs);

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

function LoadCerts() {
  certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(Ci.nsIX509CertDB);
  var certcache = certdb.getCerts();

  caTreeView = Cc["@mozilla.org/security/nsCertTree;1"]
                    .createInstance(Ci.nsICertTree);
  caTreeView.loadCertsFromCache(certcache, Ci.nsIX509Cert.CA_CERT);
  document.getElementById("ca-tree").view = caTreeView;

  serverTreeView = Cc["@mozilla.org/security/nsCertTree;1"]
                        .createInstance(Ci.nsICertTree);
  serverTreeView.loadCertsFromCache(certcache, Ci.nsIX509Cert.SERVER_CERT);
  document.getElementById("server-tree").view = serverTreeView;

  emailTreeView = Cc["@mozilla.org/security/nsCertTree;1"]
                       .createInstance(Ci.nsICertTree);
  emailTreeView.loadCertsFromCache(certcache, Ci.nsIX509Cert.EMAIL_CERT);
  document.getElementById("email-tree").view = emailTreeView;

  userTreeView = Cc["@mozilla.org/security/nsCertTree;1"]
                      .createInstance(Ci.nsICertTree);
  userTreeView.loadCertsFromCache(certcache, Ci.nsIX509Cert.USER_CERT);
  document.getElementById("user-tree").view = userTreeView;

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
  var items = null;
  if (ca_tab.selected) {
    items = caTreeView.selection;
  } else if (mine_tab.selected) {
    items = userTreeView.selection;
  } else if (others_tab.selected) {
    items = emailTreeView.selection;
  } else if (websites_tab.selected) {
    items = serverTreeView.selection;
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
  var items = null;
  if (ca_tab.selected) {
    items = caTreeView.selection;
  } else if (mine_tab.selected) {
    items = userTreeView.selection;
  } else if (others_tab.selected) {
    items = emailTreeView.selection;
  } else if (websites_tab.selected) {
    items = serverTreeView.selection;
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

async function promptError(aErrorCode) {
  if (aErrorCode != Ci.nsIX509CertDB.Success) {
    let msgName = "pkcs12-unknown-err";
    switch (aErrorCode) {
      case Ci.nsIX509CertDB.ERROR_PKCS12_NOSMARTCARD_EXPORT:
        msgName = "pkcs12-info-no-smartcard-backup";
        break;
      case Ci.nsIX509CertDB.ERROR_PKCS12_RESTORE_FAILED:
        msgName = "pkcs12-unknown-err-restore";
        break;
      case Ci.nsIX509CertDB.ERROR_PKCS12_BACKUP_FAILED:
        msgName = "pkcs12-unknown-err-backup";
        break;
      case Ci.nsIX509CertDB.ERROR_PKCS12_CERT_COLLISION:
      case Ci.nsIX509CertDB.ERROR_PKCS12_DUPLICATE_DATA:
        msgName = "pkcs12-dup-data";
        break;
      case Ci.nsIX509CertDB.ERROR_BAD_PASSWORD:
        msgName = "pk11-bad-password";
        break;
      case Ci.nsIX509CertDB.ERROR_DECODE_ERROR:
        msgName = "pkcs12-decode-err";
        break;
      default:
        break;
    }
    let [message] = await document.l10n.formatValues([{id: msgName}]);
    let prompter = Services.ww.getNewPrompter(window);
    prompter.alert(null, message);
  }
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

async function backupCerts() {
  getSelectedCerts();
  var numcerts = selected_certs.length;
  if (numcerts == 0) {
    return;
  }

  var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  let [backupFileDialog, filePkcs12Spec] = await document.l10n.formatValues([
    {id: "choose-p12-backup-file-dialog"},
    {id: "file-browse-pkcs12-spec"},
  ]);
  fp.init(window, backupFileDialog, Ci.nsIFilePicker.modeSave);
  fp.appendFilter(filePkcs12Spec, "*.p12");
  fp.appendFilters(Ci.nsIFilePicker.filterAll);
  fp.defaultExtension = "p12";
  fp.open(rv => {
    if (rv == Ci.nsIFilePicker.returnOK || rv == Ci.nsIFilePicker.returnReplace) {
      let password = {};
      if (certdialogs.setPKCS12FilePassword(window, password)) {
        let errorCode = certdb.exportPKCS12File(fp.file, selected_certs.length,
                                                selected_certs, password.value);
        promptError(errorCode);
      }
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

async function restoreCerts() {
  var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  let [restoreFileDialog, filePkcs12Spec, fileCertSpec] = await document.l10n.formatValues([
    {id: "choose-p12-restore-file-dialog"},
    {id: "file-browse-pkcs12-spec"},
    {id: "file-browse-certificate-spec"},
  ]);
  fp.init(window, restoreFileDialog, Ci.nsIFilePicker.modeOpen);
  fp.appendFilter(filePkcs12Spec, "*.p12; *.pfx");
  fp.appendFilter(fileCertSpec, gCertFileTypes);
  fp.appendFilters(Ci.nsIFilePicker.filterAll);
  fp.open(rv => {
    if (rv != Ci.nsIFilePicker.returnOK) {
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
      let fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                      .createInstance(Ci.nsIFileInputStream);
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
        },
      };
      certdb.importUserCertificate(dataArray, dataArray.length, interfaceRequestor);
    } else {
      // Otherwise, assume it's a PKCS12 file and import it that way.
      let password = {};
      let errorCode = Ci.nsIX509CertDB.ERROR_BAD_PASSWORD;
      while (errorCode == Ci.nsIX509CertDB.ERROR_BAD_PASSWORD &&
             certdialogs.getPKCS12FilePassword(window, password)) {
        errorCode = certdb.importPKCS12File(fp.file, password.value);
        if (errorCode == Ci.nsIX509CertDB.ERROR_BAD_PASSWORD &&
            password.value.length == 0) {
          // It didn't like empty string password, try no password.
          errorCode = certdb.importPKCS12File(fp.file, null);
        }
        promptError(errorCode);
      }
    }

    var certcache = certdb.getCerts();
    userTreeView.loadCertsFromCache(certcache, Ci.nsIX509Cert.USER_CERT);
    userTreeView.selection.clearSelection();
    caTreeView.loadCertsFromCache(certcache, Ci.nsIX509Cert.CA_CERT);
    caTreeView.selection.clearSelection();
    enableBackupAllButton();
  });
}

async function exportCerts() {
  getSelectedCerts();

  for (let cert of selected_certs) {
    await exportToFile(window, cert);
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

async function addCACerts() {
  var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  let [importCa, fileCertSpec] = await document.l10n.formatValues([
    {id: "import-ca-certs-prompt"},
    {id: "file-browse-certificate-spec"},
  ]);
  fp.init(window, importCa, Ci.nsIFilePicker.modeOpen);
  fp.appendFilter(fileCertSpec, gCertFileTypes);
  fp.appendFilters(Ci.nsIFilePicker.filterAll);
  fp.open(rv => {
    if (rv == Ci.nsIFilePicker.returnOK) {
      certdb.importCertsFromFile(fp.file, Ci.nsIX509Cert.CA_CERT);
      caTreeView.loadCerts(Ci.nsIX509Cert.CA_CERT);
      caTreeView.selection.clearSelection();
    }
  });
}

async function addEmailCert() {
  var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  let [importEmail, fileCertSpec] = await document.l10n.formatValues([
    {id: "import-email-cert-prompt"},
    {id: "file-browse-certificate-spec"},
  ]);
  fp.init(window, importEmail, Ci.nsIFilePicker.modeOpen);
  fp.appendFilter(fileCertSpec, gCertFileTypes);
  fp.appendFilters(Ci.nsIFilePicker.filterAll);
  fp.open(rv => {
    if (rv == Ci.nsIFilePicker.returnOK) {
      certdb.importCertsFromFile(fp.file, Ci.nsIX509Cert.EMAIL_CERT);
      var certcache = certdb.getCerts();
      emailTreeView.loadCertsFromCache(certcache, Ci.nsIX509Cert.EMAIL_CERT);
      emailTreeView.selection.clearSelection();
      caTreeView.loadCertsFromCache(certcache, Ci.nsIX509Cert.CA_CERT);
      caTreeView.selection.clearSelection();
    }
  });
}

function addException() {
  window.openDialog("chrome://pippki/content/exceptionDialog.xul", "",
                    "chrome,centerscreen,modal");
  var certcache = certdb.getCerts();
  serverTreeView.loadCertsFromCache(certcache, Ci.nsIX509Cert.SERVER_CERT);
  serverTreeView.selection.clearSelection();
}
