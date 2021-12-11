/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

const gCertFileTypes = "*.p7b; *.crt; *.cert; *.cer; *.pem; *.der";

var { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

var key;

var certdialogs = Cc["@mozilla.org/nsCertificateDialogs;1"].getService(
  Ci.nsICertificateDialogs
);

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

var overrideService;

function createRichlistItem(item) {
  let innerHbox = document.createXULElement("hbox");
  innerHbox.setAttribute("align", "center");
  innerHbox.setAttribute("flex", "1");

  let row = document.createXULElement("label");
  row.setAttribute("flex", "1");
  row.setAttribute("crop", "right");
  row.setAttribute("style", "margin-inline-start: 15px;");
  if ("raw" in item) {
    row.setAttribute("value", item.raw);
  } else {
    document.l10n.setAttributes(row, item.l10nid);
  }
  row.setAttribute("ordinal", "1");
  innerHbox.appendChild(row);

  return innerHbox;
}

var serverRichList = {
  richlist: undefined,

  buildRichList() {
    let overrides = overrideService.getOverrides().map(item => {
      let cert = null;
      if (item.dbKey !== "") {
        cert = certdb.findCertByDBKey(item.dbKey);
      }
      return {
        hostPort: item.hostPort,
        dbKey: item.dbKey,
        asciiHost: item.asciiHost,
        port: item.port,
        originAttributes: item.originAttributes,
        isTemporary: item.isTemporary,
        displayName: cert !== null ? cert.displayName : "",
      };
    });
    overrides.sort((a, b) => {
      let criteria = ["hostPort", "displayName"];
      for (let c of criteria) {
        let res = a[c].localeCompare(b[c]);
        if (res !== 0) {
          return res;
        }
      }
      return 0;
    });

    this.richlist.textContent = "";
    this.richlist.clearSelection();

    let frag = document.createDocumentFragment();
    for (let override of overrides) {
      let richlistitem = this._richBoxAddItem(override);
      frag.appendChild(richlistitem);
    }
    this.richlist.appendChild(frag);

    this._setButtonState();
    this.richlist.addEventListener("select", () => this._setButtonState());
  },

  _richBoxAddItem(item) {
    let richlistitem = document.createXULElement("richlistitem");

    richlistitem.setAttribute("dbKey", item.dbKey);
    richlistitem.setAttribute("host", item.asciiHost);
    richlistitem.setAttribute("port", item.port);
    richlistitem.setAttribute("hostPort", item.hostPort);
    richlistitem.setAttribute(
      "originAttributes",
      JSON.stringify(item.originAttributes)
    );

    let hbox = document.createXULElement("hbox");
    hbox.setAttribute("flex", "1");
    hbox.setAttribute("equalsize", "always");

    hbox.appendChild(createRichlistItem({ raw: item.hostPort }));
    hbox.appendChild(
      createRichlistItem(
        item.displayName !== ""
          ? { raw: item.displayName }
          : { l10nid: "no-cert-stored-for-override" }
      )
    );
    hbox.appendChild(
      createRichlistItem({
        l10nid: item.isTemporary ? "temporary-override" : "permanent-override",
      })
    );

    richlistitem.appendChild(hbox);

    return richlistitem;
  },

  deleteSelectedRichListItem() {
    let selectedItem = this.richlist.selectedItem;
    if (!selectedItem) {
      return;
    }

    let retVals = {
      deleteConfirmed: false,
    };
    window.browsingContext.topChromeWindow.openDialog(
      "chrome://pippki/content/deletecert.xhtml",
      "",
      "chrome,centerscreen,modal",
      "websites_tab",
      [
        {
          hostPort: selectedItem.attributes.hostPort.value,
        },
      ],
      retVals
    );

    if (retVals.deleteConfirmed) {
      overrideService.clearValidityOverride(
        selectedItem.attributes.host.value,
        selectedItem.attributes.port.value,
        JSON.parse(selectedItem.attributes.originAttributes.value)
      );
      this.buildRichList();
    }
  },

  viewSelectedRichListItem() {
    let selectedItem = this.richlist.selectedItem;
    if (!selectedItem) {
      return;
    }

    let dbKey = selectedItem.getAttribute("dbKey");
    if (dbKey) {
      let cert = certdb.findCertByDBKey(dbKey);
      viewCertHelper(window, cert);
    }
  },

  exportSelectedRichListItem() {
    let selectedItem = this.richlist.selectedItem;
    if (!selectedItem) {
      return;
    }

    let dbKey = selectedItem.getAttribute("dbKey");
    if (dbKey) {
      let cert = certdb.findCertByDBKey(dbKey);
      exportToFile(window, cert);
    }
  },

  addException() {
    let retval = {
      exceptionAdded: false,
    };
    window.browsingContext.topChromeWindow.openDialog(
      "chrome://pippki/content/exceptionDialog.xhtml",
      "",
      "chrome,centerscreen,modal",
      retval
    );
    if (retval.exceptionAdded) {
      this.buildRichList();
    }
  },

  _setButtonState() {
    let websiteViewButton = document.getElementById("websites_viewButton");
    let websiteExportButton = document.getElementById("websites_exportButton");
    let websiteDeleteButton = document.getElementById("websites_deleteButton");

    let certKey = this.richlist.selectedItem?.getAttribute("dbKey");
    let cert = certKey && certdb.findCertByDBKey(certKey);

    websiteDeleteButton.disabled = this.richlist.selectedIndex < 0;
    websiteExportButton.disabled = !cert;
    websiteViewButton.disabled = websiteExportButton.disabled;
  },
};
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

var clientAuthRememberService;

var rememberedDecisionsRichList = {
  richlist: undefined,

  buildRichList() {
    let rememberedDecisions = clientAuthRememberService.getDecisions();

    let oldItems = this.richlist.querySelectorAll("richlistitem");
    for (let item of oldItems) {
      item.remove();
    }

    let frag = document.createDocumentFragment();
    for (let decision of rememberedDecisions) {
      let richlistitem = this._richBoxAddItem(decision);
      frag.appendChild(richlistitem);
    }
    this.richlist.appendChild(frag);

    this.richlist.addEventListener("select", () => this.setButtonState());
  },

  _richBoxAddItem(item) {
    let richlistitem = document.createXULElement("richlistitem");

    richlistitem.setAttribute("entryKey", item.entryKey);
    richlistitem.setAttribute("dbKey", item.dbKey);

    let hbox = document.createXULElement("hbox");
    hbox.setAttribute("flex", "1");
    hbox.setAttribute("equalsize", "always");

    hbox.appendChild(createRichlistItem({ raw: item.asciiHost }));
    if (item.dbKey == "") {
      hbox.appendChild(
        createRichlistItem({ l10nid: "send-no-client-certificate" })
      );

      hbox.appendChild(createRichlistItem({ raw: "" }));
    } else {
      let tmpCert = certdb.findCertByDBKey(item.dbKey);
      // The certificate corresponding to this item's dbKey may not be
      // available (for example, if it was stored on a token that's been
      // removed, or if it was deleted).
      if (tmpCert) {
        hbox.appendChild(createRichlistItem({ raw: tmpCert.commonName }));
        hbox.appendChild(createRichlistItem({ raw: tmpCert.serialNumber }));
      } else {
        hbox.appendChild(
          createRichlistItem({ l10nid: "certificate-not-available" })
        );
        hbox.appendChild(
          createRichlistItem({ l10nid: "certificate-not-available" })
        );
      }
    }

    richlistitem.appendChild(hbox);

    return richlistitem;
  },

  deleteSelectedRichListItem() {
    let selectedItem = this.richlist.selectedItem;
    let index = this.richlist.selectedIndex;
    if (index < 0) {
      return;
    }

    clientAuthRememberService.forgetRememberedDecision(
      selectedItem.attributes.entryKey.value
    );

    this.buildRichList();
    this.setButtonState();
  },

  viewSelectedRichListItem() {
    let selectedItem = this.richlist.selectedItem;
    let index = this.richlist.selectedIndex;
    if (index < 0) {
      return;
    }

    if (selectedItem.attributes.dbKey.value != "") {
      let cert = certdb.findCertByDBKey(selectedItem.attributes.dbKey.value);
      viewCertHelper(window, cert);
    }
  },

  setButtonState() {
    let rememberedDeleteButton = document.getElementById(
      "remembered_deleteButton"
    );
    let rememberedViewButton = document.getElementById("remembered_viewButton");

    rememberedDeleteButton.disabled = this.richlist.selectedIndex < 0;
    rememberedViewButton.disabled =
      this.richlist.selectedItem == null
        ? true
        : this.richlist.selectedItem.attributes.dbKey.value == "";
  },
};

function LoadCerts() {
  certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  var certcache = certdb.getCerts();

  caTreeView = Cc["@mozilla.org/security/nsCertTree;1"].createInstance(
    Ci.nsICertTree
  );
  caTreeView.loadCertsFromCache(certcache, Ci.nsIX509Cert.CA_CERT);
  document.getElementById("ca-tree").view = caTreeView;

  emailTreeView = Cc["@mozilla.org/security/nsCertTree;1"].createInstance(
    Ci.nsICertTree
  );
  emailTreeView.loadCertsFromCache(certcache, Ci.nsIX509Cert.EMAIL_CERT);
  document.getElementById("email-tree").view = emailTreeView;

  userTreeView = Cc["@mozilla.org/security/nsCertTree;1"].createInstance(
    Ci.nsICertTree
  );
  userTreeView.loadCertsFromCache(certcache, Ci.nsIX509Cert.USER_CERT);
  document.getElementById("user-tree").view = userTreeView;

  clientAuthRememberService = Cc[
    "@mozilla.org/security/clientAuthRememberService;1"
  ].getService(Ci.nsIClientAuthRememberService);

  overrideService = Cc["@mozilla.org/security/certoverride;1"].getService(
    Ci.nsICertOverrideService
  );

  rememberedDecisionsRichList.richlist = document.getElementById(
    "rememberedList"
  );
  serverRichList.richlist = document.getElementById("serverList");

  rememberedDecisionsRichList.buildRichList();
  serverRichList.buildRichList();

  rememberedDecisionsRichList.setButtonState();

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
  var items = null;
  if (ca_tab.selected) {
    items = caTreeView.selection;
  } else if (mine_tab.selected) {
    items = userTreeView.selection;
  } else if (others_tab.selected) {
    items = emailTreeView.selection;
  }
  selected_certs = [];
  var cert = null;
  var nr = 0;
  if (items != null) {
    nr = items.getRangeCount();
  }
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
  var items = null;
  if (ca_tab.selected) {
    items = caTreeView.selection;
  } else if (mine_tab.selected) {
    items = userTreeView.selection;
  } else if (others_tab.selected) {
    items = emailTreeView.selection;
  }
  selected_certs = [];
  selected_tree_items = [];
  selected_index = [];
  var tree_item = null;
  var nr = 0;
  if (items != null) {
    nr = items.getRangeCount();
  }
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
    let [message] = await document.l10n.formatValues([{ id: msgName }]);
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
  let idList = ["mine_viewButton", "mine_backupButton", "mine_deleteButton"];
  enableButtonsForCertTree(userTreeView, idList);
}

function email_enableButtons() {
  let idList = ["email_viewButton", "email_exportButton", "email_deleteButton"];
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
    { id: "choose-p12-backup-file-dialog" },
    { id: "file-browse-pkcs12-spec" },
  ]);
  fp.init(window, backupFileDialog, Ci.nsIFilePicker.modeSave);
  fp.appendFilter(filePkcs12Spec, "*.p12");
  fp.appendFilters(Ci.nsIFilePicker.filterAll);
  fp.defaultExtension = "p12";
  fp.open(rv => {
    if (
      rv == Ci.nsIFilePicker.returnOK ||
      rv == Ci.nsIFilePicker.returnReplace
    ) {
      let password = {};
      if (certdialogs.setPKCS12FilePassword(window, password)) {
        let errorCode = certdb.exportPKCS12File(
          fp.file,
          selected_certs,
          password.value
        );
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
    window.browsingContext.topChromeWindow.openDialog(
      "chrome://pippki/content/editcacert.xhtml",
      "",
      "chrome,centerscreen,modal",
      cert
    );
  }
}

async function restoreCerts() {
  var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  let [
    restoreFileDialog,
    filePkcs12Spec,
    fileCertSpec,
  ] = await document.l10n.formatValues([
    { id: "choose-p12-restore-file-dialog" },
    { id: "file-browse-pkcs12-spec" },
    { id: "file-browse-certificate-spec" },
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
      let fstream = Cc[
        "@mozilla.org/network/file-input-stream;1"
      ].createInstance(Ci.nsIFileInputStream);
      fstream.init(fp.file, -1, 0, 0);
      let dataString = NetUtil.readInputStreamToString(
        fstream,
        fstream.available()
      );
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
      certdb.importUserCertificate(
        dataArray,
        dataArray.length,
        interfaceRequestor
      );
    } else {
      // Otherwise, assume it's a PKCS12 file and import it that way.
      let password = {};
      let errorCode = Ci.nsIX509CertDB.ERROR_BAD_PASSWORD;
      while (
        errorCode == Ci.nsIX509CertDB.ERROR_BAD_PASSWORD &&
        certdialogs.getPKCS12FilePassword(window, password)
      ) {
        errorCode = certdb.importPKCS12File(fp.file, password.value);
        if (
          errorCode == Ci.nsIX509CertDB.ERROR_BAD_PASSWORD &&
          password.value.length == 0
        ) {
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
    mine_tab: userTreeView,
    ca_tab: caTreeView,
    others_tab: emailTreeView,
  };
  let selTab = document.getElementById("certMgrTabbox").selectedItem;
  let selTabID = selTab.getAttribute("id");

  if (!(selTabID in treeViewMap)) {
    return;
  }

  let retVals = {
    deleteConfirmed: false,
  };
  window.browsingContext.topChromeWindow.openDialog(
    "chrome://pippki/content/deletecert.xhtml",
    "",
    "chrome,centerscreen,modal",
    selTabID,
    selected_tree_items,
    retVals
  );

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
    { id: "import-ca-certs-prompt" },
    { id: "file-browse-certificate-spec" },
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
    { id: "import-email-cert-prompt" },
    { id: "file-browse-certificate-spec" },
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
