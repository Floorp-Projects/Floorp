/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bob Lord <lord@netscape.com>
 *   Ian McGreer <mcgreer@netscape.com>
 *   Kai Engert <kengert@redhat.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

var CertTree = Components.Constructor(nsCertTree, nsICertTree);
var CertCache = Components.Constructor(nsNSSCertCache, nsINSSCertCache, "cacheAllCerts");
var FilePicker = Components.Constructor(nsFilePicker, nsIFilePicker, "init");

function LoadCerts()
{
  window.crypto.enableSmartCardEvents = true;
  document.addEventListener("smartcard-insert", onSmartCardChange, false);
  document.addEventListener("smartcard-remove", onSmartCardChange, false);

  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  var certcache = new CertCache;

  caTreeView = new TreeFilter(certcache, nsIX509Cert.CA_CERT);
  var treeBoxObject = document.getElementById('ca-tree').treeBoxObject;
  treeBoxObject.view = caTreeView;

  serverTreeView = new TreeFilter(certcache, nsIX509Cert.SERVER_CERT);
  treeBoxObject = document.getElementById('server-tree').treeBoxObject;
  treeBoxObject.view = serverTreeView;

  emailTreeView = new TreeFilter(certcache, nsIX509Cert.EMAIL_CERT);
  treeBoxObject = document.getElementById('email-tree').treeBoxObject
  treeBoxObject.view = emailTreeView;

  userTreeView = new TreeFilter(certcache, nsIX509Cert.USER_CERT);
  treeBoxObject = document.getElementById('user-tree').treeBoxObject;
  treeBoxObject.view = userTreeView;

  orphanTreeView = new TreeFilter(certcache, nsIX509Cert.UNKNOWN_CERT);
  treeBoxObject = document.getElementById('orphan-tree').treeBoxObject;
  treeBoxObject.view = orphanTreeView;

  var rowCnt = userTreeView.rowCount;
  var enableBackupAllButton=document.getElementById('mine_backupAllButton');
  if (rowCnt < 1) {
    enableBackupAllButton.setAttribute("disabled",true);
  } else  {
    enableBackupAllButton.setAttribute("enabled",true);
  }
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
  var enable_edit = false;

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
          // Trust editing is not possible for override
          // entries that are bound to host:port,
          // where the cert is stored for convenince only.
          if (!ti.hostPort.length) {
            enable_edit = true;
          }
        }
      }
      catch (e) {
      }
    }
  }

  var enableViewButton=document.getElementById('websites_viewButton');
  enableViewButton.setAttribute("disabled", !enable_view);
  var enableEditButton=document.getElementById('websites_editButton');
  enableEditButton.setAttribute("disabled", !enable_edit);
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
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var fp = new FilePicker(window,
          bundle.GetStringFromName("chooseP12BackupFileDialog"),
          nsIFilePicker.modeSave);
  fp.appendFilter(bundle.GetStringFromName("file_browse_PKCS12_spec"),
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
    } else if (!document.getElementById("websites_tab").selected
               || !serverTreeView.isHostPortOverride(selected_index[t])) {
      // If the web sites tab is select, trust editing is only allowed
      // if the entry refers to a real cert, but not if it's
      // a host:port override, where the cert is stored for convenince only.
      window.openDialog('chrome://pippki/content/editsslcert.xul', certkey,
                        'chrome,centerscreen,modal');
    }
  }
}

function restoreCerts()
{
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var fp = new FilePicker(window,
          bundle.GetStringFromName("chooseP12RestoreFileDialog"),
          nsIFilePicker.modeOpen);
  fp.appendFilter(bundle.GetStringFromName("file_browse_PKCS12_spec"),
                  "*.p12; *.pfx");
  fp.appendFilters(nsIFilePicker.filterAll);
  if (fp.show() == nsIFilePicker.returnOK) {
    certdb.importPKCS12File(null, fp.file);

    var certcache = new CertCache;
    userTreeView.loadCertsFromCache(certcache, nsIX509Cert.USER_CERT);
    userTreeView.selection.clearSelection();
    caTreeView.loadCertsFromCache(certcache, nsIX509Cert.CA_CERT);
    caTreeView.selection.clearSelection();
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

  var params = Components.classes[nsDialogParamBlock]
                         .createInstance(nsIDialogParamBlock);
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
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
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var fp = new FilePicker(window,
          bundle.GetStringFromName("importCACertsPrompt"),
          nsIFilePicker.modeOpen);
  fp.appendFilter(bundle.GetStringFromName("file_browse_Certificate_spec"),
                  "*.crt; *.cert; *.cer; *.pem; *.der");
  fp.appendFilters(nsIFilePicker.filterAll);
  if (fp.show() == nsIFilePicker.returnOK) {
    certdb.importCertsFromFile(null, fp.file, nsIX509Cert.CA_CERT);
    caTreeView.loadCerts(nsIX509Cert.CA_CERT);
    caTreeView.selection.clearSelection();
  }
}

function onSmartCardChange()
{
  var certcache = new CertCache;
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

function addEmailCert()
{
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var fp = new FilePicker(window,
          bundle.GetStringFromName("importEmailCertPrompt"),
          nsIFilePicker.modeOpen);
  fp.appendFilter(bundle.GetStringFromName("file_browse_Certificate_spec"),
                  "*.crt; *.cert; *.cer; *.pem; *.der");
  fp.appendFilters(nsIFilePicker.filterAll);
  if (fp.show() == nsIFilePicker.returnOK) {
    certdb.importCertsFromFile(null, fp.file, nsIX509Cert.EMAIL_CERT);
    var certcache = new CertCache;
    emailTreeView.loadCertsFromCache(certcache, nsIX509Cert.EMAIL_CERT);
    emailTreeView.selection.clearSelection();
    caTreeView.loadCertsFromCache(certcache, nsIX509Cert.CA_CERT);
    caTreeView.selection.clearSelection();
  }
}

function addWebSiteCert()
{
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var fp = new FilePicker(window,
          bundle.GetStringFromName("importServerCertPrompt"),
          nsIFilePicker.modeOpen);
  fp.appendFilter(bundle.GetStringFromName("file_browse_Certificate_spec"),
                  "*.crt; *.cert; *.cer; *.pem; *.der");
  fp.appendFilters(nsIFilePicker.filterAll);
  if (fp.show() == nsIFilePicker.returnOK) {
    certdb.importCertsFromFile(null, fp.file, nsIX509Cert.SERVER_CERT);

    var certcache = new CertCache;
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
  var certcache = new CertCache;
  serverTreeView.loadCertsFromCache(certcache, nsIX509Cert.SERVER_CERT);
  serverTreeView.selection.clearSelection();
  orphanTreeView.loadCertsFromCache(certcache, nsIX509Cert.UNKNOWN_CERT);
  orphanTreeView.selection.clearSelection();
}

function Row(row, text, level) {
  this.row = row;
  this.text = text;
  this.level = level;
  this.collapsed = false;
  this.children = [];
}

function TreeFilter(aCache, aType)
{
  this.rowMap = [];
  this.rows = []
  this.treeView = new CertTree;
  this.delayed = function () {
    this.loadCertsFromCache(aCache, aType);
    this.delayed = null;
  }
}

TreeFilter.prototype = {
  constructor: TreeFilter,
  searchSpace: null,
  delayed: null,
  keys:('nickname,emailAddress,subjectName,commonName,organization,' +
        'organizationalUnit,sha1Fingerprint,md5Fingerprint,issuerName,' +
        'issuerCommonName,issuerOrganization,issuerOrganizationUnit')
        .split(/,/g),
  init: function init() {
    if (this.delayed)
      this.delayed();
    var searchSpace = this.searchSpace = {};
    this.rows.length = this.rowMap.length = 0;
    if (!this.tree)
      return;
    var i = 0, count = this.treeView.rowCount;
    var pc = this.tree.columns.getPrimaryColumn();
    var tc = pc.getNext();
    var pk11db = Components.classes["@mozilla.org/security/pk11tokendb;1"]
                           .getService(Components.interfaces.nsIPK11TokenDB);

    var parent = -1;
    for (; i < count; ++i) {
      this.rowMap[i] = i;
      var level = this.treeView.getLevel(i);
      var text = this.treeView.getCellText(i, pc);
      var row = new Row(i, text, level);
      switch (level) {
      case 0 /* issuer */:
        parent = i;
        break;
      case 1 /* cert */:
        row.parent = row.trueParent = parent;
        this.rows[parent].children.push(i);
        var cert = row.cert = this.treeView.getCert(i);
        this.enhanceSpace(searchSpace, cert, i);
        break;
      }
      this.rows[i] = row;
    }
  },
  fixParents: function fixParents(i) {
    if (!i)
      i = 0;
    var count = this.rowMap.length;
    for (; i < count; ++i) {
      var row = this.rows[this.rowMap[i]];
      var level = row.level;
      switch (level) {
      case 0 /* issuer */:
        var parent = i;
        break;
      case 1 /* cert */:
        row.parent = parent;
        break;
      }
    }
  },
  enhanceSpace: function enhanceSpace(searchSpace, cert, i) {
    for each (key in this.keys) {
      var val = cert[key];
      if (val) {
        var ary;
        if (val in searchSpace) {
          ary = searchSpace[val];
        } else {
          searchSpace[val] = ary = [];
        }
        ary.push(i);
      }
    }
  },
  search: function search(node) {
    var hint = node.value;
    var clazz = node.id.replace(/-search$/,"");
    var descNode = document.getElementById(clazz + "-description");
    descNode.firstChild.data =
      document.getElementById(
        "certmgr." + 
        clazz + 
        (hint == "" ? ".description" : ".filtered")
      ).getAttribute("value");
    /* TODO: convert to using a timeout otherwise the ui could hang */
    var rowList = [], rowMap = [];
    var searchSpace = this.searchSpace;
    var pattern = new RegExp(hint, 'i');
    for (var key in searchSpace) {
      if (pattern.test(key)) {
        var rows = searchSpace[key];
        var j;
        for each (j in rows)
          rowList[j] = true;
      }
    }
    var i = 0;
    var lastParent = -1;
    var lastIndex = -2;
    var siblings;
    for (j = 0; j < this.rows.length; ++j) {
      var row = this.rows[j];
      if ((row.collapsed = rowList[j])) {
        if (j != lastIndex + 1) {
          var parentNode = row.trueParent;
          if (parentNode != lastParent) {
            lastParent = i;
            rowMap[i++] = lastParent = parentNode;
            siblings = this.rows[parentNode].children;
            siblings.length = 0;
          }
        }
        siblings.push(j);
        row = this.rows[row.parent = lastParent];
        if (!row.collapsed)
          rowMap[i++] = lastIndex = j;
      } else {
        row.children.length = 0;
      }
    }
    this.tree.beginUpdateBatch();
    j = rowMap.length - 1;
    var last = 0;
    for (i = this.rowMap.length - 1; i >= 0; --i) {
      var val = this.rowMap[i];
      if (val == last) {
        last = rowList[--j];
        continue;
      }
      if (val > last) {
        var deleted = 1;
        while (this.rowMap[--i] > last)
          ++deleted;
        this.tree.rowCountChanged(i, -deleted);
        continue;
      }
      /* val < last */
      var added = 1;
      while (val < (last = rowMap[--j]))
        ++added;
      this.tree.rowCountChanged(i, -added);
    }
    this.rowMap = rowMap;
    this.tree.endUpdateBatch();
  },
  /* nsITreeView */
  get rowCount() {
    return this.rowMap.length;
  },
  get selection() {
    return this.mSelection;
  },
  set selection(sel) {
    this.mSelection = sel;
  },
  getRowProperties: function getRowProperties(aRow, aProp) {
    /* the underlying thing doesn't do anything,
     * in order for us to share, we'd need to implement
     * nsITreeBoxObject ourselves */
  },
  getCellProperties: function getCellProperties(aRow, aColumn, aProp) {
    /* the underlying thing doesn't do anything,
     * in order for us to share, we'd need to implement
     * nsITreeBoxObject ourselves */
  },
  getColumnProperties: function getColumnProperties(aColumn, aProp) {
    /* the underlying thing doesn't do anything,
     * in order for us to share, we'd need to implement
     * nsITreeBoxObject ourselves */
  },
  isContainer: function isContainer(aRow) {
    var children = this.rows[this.rowMap[aRow]].children;
    return children.length;
  },
  isContainerOpen: function isContainerOpen(aRow) {
    var row = this.rows[this.rowMap[aRow]];
    var children = row.children;
    return children.length && !row.collapsed;
  },
  isContainerEmpty: function isContainerEmpty(aRow) {
    var row = this.rowMap[aRow];
    var children = this.rows[row].children;
    return !children.length;
  },
  isSeparator: function isSeparator(aRow) {
    return false;
  },
  isSorted: function isSorted() {
    return false;
  },
  canDrop: function canDrop(aRow, aOrientation) {
    return false;
  },
  drop: function drop(aRow, aOrientation) {
    return false;
  },
  getParentIndex: function getParentIndex(aRow) {
    var row = this.rowMap[aRow];
    return this.rows[row].level == 1 ? this.rows[row].parent : -1;
  },
  hasNextSibling: function hasNextSibling(aRow, aAfterIndex) {
    if (this.getLevel(aAfterIndex) < 1 ||
        this.getParentIndex(aAfterIndex) >
        this.getParentIndex(aRow))
      return false;
    return true;
  },
  getLevel: function getLevel(aRow) {
    return this.rows[this.rowMap[aRow]].level;
  },
  getImageSrc: function getImageSrc(aRow, aColumn) {
    /* XXX OptimizeMe*/
    return this.treeView.getImageSrc(this.rowMap[aRow], aColumn);
  },
  getProgressMode: function getProgressMode(aRow, aColumn) {
    /* XXX OptimizeMe*/
    return this.treeView.getProgressMode(this.rowMap[aRow], aColumn);
  },
  getCellValue: function getCellValue(aRow, aColumn) {
    /* XXX OptimizeMe*/
    return this.treeView.getCellValue(this.rowMap[aRow], aColumn);
  },
  getCellText: function getCellText(aRow, aColumn) {
    /* XXX OptimizeMe*/
    return this.treeView.getCellText(this.rowMap[aRow], aColumn);
  },
  setTree: function setTree(aTree) {
    this.tree = aTree;
    this.init();
    /* the underlying thing doesn't actually do much,
     * in order for us to share, we'd need to implement
     * nsITreeBoxObject ourselves */
    /* we actually need to do this to support deleteEntryObject properly */
  },
  toggleOpenState: function toggleOpenState(aRow) {
    var row = this.rows[this.rowMap[aRow]];
    /* we only allow opening parents */
    if (row.level)
      return;
    var i = aRow + 1;
    var offset = row.children.length;
    this.tree.beginUpdateBatch();
    if (row.collapsed) {
      for (var j = 0; j < offset; ++j)
        this.rowMap.splice(i++, 0, row.children[j]);
      row.collapsed = false;
    } else {
      this.rowMap.splice(i, offset);
      offset = -offset;
      row.collapsed = true;
    }
    this.tree.rowCountChanged(i, offset);
    this.fixParents(aRow);
    this.tree.endUpdateBatch();
  },
  cycleHeader: function cycleHeader(column) {
    /* the underlying thing doesn't do anything,
     * in order for us to share, we'd need to implement
     * nsITreeBoxObject ourselves */
  },
  selectionChanged: function selectionChanged() {
  },
  cycleCell: function cycleCell(aRow, aColumn) {
    this.treeView.cycleCell(this.rowMap[aRow], aColumn);
  },
  isEditable: function isEditable(aRow, aColumn) {
    return this.treeView.isEditable(this.rowMap[aRow], aColumn);
  },
  isSelectable: function isSelectable(aRow, aColumn) {
    return this.treeView.isSelectable(this.rowMap[aRow], aColumn);
  },
  setCellValue: function setCellValue(aRow, aColumn, aValue) {
    this.treeView.setCellValue(this.rowMap[aRow], aColumn, aValue);
  },
  setCellText: function setCellText(aRow, aColumn, aValue) {
    this.treeView.setCellText(this.rowMap[aRow], aColumn, aValue);
  },
  performAction: function performAction(aAction) {
    this.treeView.performAction(aAction);
  },
  performActionOnRow: function performActionOnRow(aAction, aRow) {
    this.treeView.performActionOnRow(aAction, this.rowMap[aRow]);
  },
  performActionOnCell: function performActionOnCell(aAction, aRow, aColumn) {
    this.treeView.performActionOnCell(aAction, this.rowMap[aRow], aColumn);
  },
  /* nsICertTree */
  loadCerts: function loadCerts(aType) {
    this.treeView.loadCerts(aType);
    this.init();
  },
  loadCertsFromCache: function loadCertsFromCache(aCache, aType) {
    this.treeView.loadCertsFromCache(aCache, aType);
  },
  getCert: function getCert(aRow) {
    return this.rows[this.rowMap[aRow]].cert;
  },
  getTreeItem: function getTreeItem(aRow) {
    return this.treeView.getTreeItem(this.rowMap[aRow]);
  },
  isHostPortOverride: function isHostPortOverride(aRow) {
    return this.treeView.isHostPortOverride(this.rowMap[aRow]);
  },
  deleteEntryObject: function deleteEntryObject(aRow) {
    this.treeView.deleteEntryObject(this.rowMap[aRow]);
    this.init();
  }
};
