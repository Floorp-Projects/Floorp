/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@netscape.com> (Original Author)
 *   Blake Ross <blakeross@telocity.com>
 *   Jan Varga <varga@ku.sk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

const NC_NS = "http://home.netscape.com/NC-rdf#";

var gDownloadView = null;
var gDownloadManager = null;
var gRDFService = null;
var gNC_File = null;
var gFileHandler = null;
var gStatusBar = null;
var gCannotLaunch = ((navigator.platform.indexOf("Win") == -1) &&
                     (navigator.platform.indexOf("OS/2") == -1) &&
                     (navigator.platform.indexOf("Mac") == -1));

const dlObserver = {
  observe: function(subject, topic, state) {
    if (topic != "download-starting") return;
    selectDownload(subject.QueryInterface(Components.interfaces.nsIDownload));
  }
};

function selectDownload(aDownload)
{
  var dlElt = document.getElementById(aDownload.targetFile.path);
  var dlIndex = gDownloadView.contentView.getIndexOfItem(dlElt);
  gDownloadView.view.selection.select(dlIndex);
  gDownloadView.treeBoxObject.ensureRowIsVisible(dlIndex);
}

function DLManagerStartup()
{  
  if (!window.arguments.length)
    return;

  try {
    var observerService = Components.classes[kObserverServiceProgID]
                                    .getService(Components.interfaces.nsIObserverService);
    observerService.addObserver(dlObserver, "download-starting", false);
  }
  catch (ex) {
  }

  const rdfSvcContractID = "@mozilla.org/rdf/rdf-service;1";
  const rdfSvcIID = Components.interfaces.nsIRDFService;
  gRDFService = Components.classes[rdfSvcContractID].getService(rdfSvcIID);
  
  gNC_File = gRDFService.GetResource(NC_NS + "File");

  gDownloadView = document.getElementById("downloadView");
  setSortVariables(gDownloadView);

  const dlmgrContractID = "@mozilla.org/download-manager;1";
  const dlmgrIID = Components.interfaces.nsIDownloadManager;
  gDownloadManager = Components.classes[dlmgrContractID].getService(dlmgrIID);

  const ioSvcContractID = "@mozilla.org/network/io-service;1";
  const ioSvcIID = Components.interfaces.nsIIOService;
  var ioService = Components.classes[ioSvcContractID].getService(ioSvcIID);

  const fileHandlerIID = Components.interfaces.nsIFileProtocolHandler;
  gFileHandler = ioService.getProtocolHandler("file")
                          .QueryInterface(fileHandlerIID);

  var ds = window.arguments[0];
  gDownloadView.database.AddDataSource(ds);
  gDownloadView.builder.rebuild();
  window.setTimeout(onRebuild, 0);

  var key;
  document.getElementById("btn_openfile").hidden = gCannotLaunch;
  document.getElementById("downloadPaneContext-openfile").hidden = gCannotLaunch;
}

function onRebuild() {
  gDownloadView.controllers.appendController(downloadViewController);
  gDownloadView.focus();

  // If the window was opened automatically because
  // a download started, select the new download
  if (window.arguments.length > 1 && window.arguments[1]) {
    var dl = window.arguments[1];
    selectDownload(dl.QueryInterface(Components.interfaces.nsIDownload));
  }
  else if (gDownloadView.view && gDownloadView.view.rowCount > 0) {
    // Select the first item in the view, if any.
    gDownloadView.view.selection.select(0);
  }
}

function onSelect(aEvent) {
  if (!gStatusBar)
    gStatusBar = document.getElementById("statusbar-text");

  var selectionCount = gDownloadView.view.selection.count;
  if (selectionCount == 1)
    gStatusBar.label = createLocalFile(getSelectedItem().id).path;
  else
    gStatusBar.label = "";

  window.updateCommands("tree-select");
}

function onTrigger() {
  if (downloadViewController.isCommandEnabled('cmd_properties'))
    goDoCommand('cmd_properties');
  else if (downloadViewController.isCommandEnabled('cmd_openfile'))
    goDoCommand('cmd_openfile');
  else if (downloadViewController.isCommandEnabled('cmd_showinshell'))
    goDoCommand('cmd_showinshell');
}

var downloadViewController = {
  supportsCommand: function dVC_supportsCommand (aCommand)
  {
    switch (aCommand) {
    case "cmd_properties":
    case "cmd_pause":
    case "cmd_cancel":
    case "cmd_remove":
    case "cmd_copyurl":
    case "cmd_openfile":
    case "cmd_showinshell":
    case "cmd_selectAll":
      return true;
    }
    return false;
  },

  isCommandEnabled: function dVC_isCommandEnabled (aCommand)
  {
    if (!gDownloadView.view || !gDownloadView.view.selection) return false;
    var selectionCount = gDownloadView.view.selection.count;
    if (!selectionCount) return false;

    var selectedItem = getSelectedItem();
    var isDownloading = selectedItem && gDownloadManager.getDownload(selectedItem.id);

    switch (aCommand) {
    case "cmd_openfile":
      if (gCannotLaunch)
        return false;
    case "cmd_showinshell":
      // we can't reveal until the download is complete, because we have not given
      // the file its final name until them.
      return selectionCount == 1 && !isDownloading && selectedItem &&
             createLocalFile(selectedItem.id).exists();
    case "cmd_properties":
      return selectionCount == 1 && isDownloading;
    case "cmd_pause":
      return false;
    case "cmd_cancel":
      // XXX check if selection is still in progress
      //     how to handle multiple selection?
      return isDownloading;
    case "cmd_remove":
      // XXX ensure selection isn't still in progress
      //     and how to handle multiple selection?
      return selectionCount > 0 && !isDownloading;
    case "cmd_copyurl":
      return selectionCount > 0;
    case "cmd_selectAll":
      return gDownloadView.view.rowCount != selectionCount;
    default:
      return false;
    }
  },

  doCommand: function dVC_doCommand (aCommand)
  {
    var selectedItem, selectedItems;
    var file, i;

    switch (aCommand) {
    case "cmd_properties":
      selectedItem = getSelectedItem();
      var dl;
      if (selectedItem)
        dl = gDownloadManager.getDownload(selectedItem.id);
      if (dl)
        gDownloadManager.openProgressDialogFor(dl, window, false);
      break;
    case "cmd_openfile":
      selectedItem = getSelectedItem();
      if (selectedItem) {
        file = createLocalFile(selectedItem.id);
        const kDontAskAgainPref  = "browser.download.progressDnlgDialog.dontAskForLaunch";
        try {
          var pref = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefBranch);
          var dontAskAgain = pref.getBoolPref(kDontAskAgainPref);
        } catch (e) {
          // dontAskAgain not set - then we need to ask user
          dontAskAgain = false;
        }
        if (!dontAskAgain && file.isExecutable()) {
          try {
            var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                      .getService(Components.interfaces.nsIPromptService);
          } catch (ex) {
            break;
          }
          var strBundle = document.getElementById("dlProgressDlgBundle");
          var title = strBundle.getFormattedString("openingAlertTitle", [file.leafName]);
          var msg = strBundle.getFormattedString("securityAlertMsg", [file.leafName]);
          var dontaskmsg = strBundle.getString("dontAskAgain");
          var checkbox = {value:0};
          var okToProceed = promptService.confirmCheck(window, title, msg, dontaskmsg, checkbox);
          try {
            if (checkbox.value != dontAskAgain)
              pref.setBoolPref(kDontAskAgainPref, checkbox.value);
          } catch (ex) {}
          if (!okToProceed)
            return;
        }
        file.launch();
      }
      break;
    case "cmd_showinshell":
      selectedItem = getSelectedItem();
      if (selectedItem) {
        file = createLocalFile(selectedItem.id);

        // on unix, open a browser window rooted at the parent
        if ((navigator.platform.indexOf("Win") == -1) &&
            (navigator.platform.indexOf("Mac") == -1) &&
            (navigator.platform.indexOf("OS/2") == -1)) {
          file = file.QueryInterface(Components.interfaces.nsIFile);
          var parent = file.parent;
          if (parent) {
            const browserURL = "chrome://navigator/content/navigator.xul";
            window.openDialog(browserURL, "_blank", "chrome,all,dialog=no", parent.path);
          }
        }
        else {
          file.reveal();
        }
      }
      break;
    case "cmd_pause":
      break;
    case "cmd_cancel":
      // XXX we should probably prompt the user
      selectedItems = getSelectedItems();
      for (i = 0; i < selectedItems.length; i++)
        gDownloadManager.cancelDownload(selectedItems[i].id);
      window.updateCommands("tree-select");
      break;
    case "cmd_remove":
      selectedItems = getSelectedItems();
      // Figure out where to place the selection after deletion
      var newSelectionPos = gDownloadView.contentView.getIndexOfItem(selectedItems[0]);
      gDownloadManager.startBatchUpdate();

      // Notify the datasource that we're about to begin a batch operation
      var ds = window.arguments[0]
                     .QueryInterface(Components.interfaces.nsIRDFDataSource);
      ds.beginUpdateBatch();
      for (i = 0; i <= selectedItems.length - 1; ++i) {
        gDownloadManager.removeDownload(selectedItems[i].id);
      }
      ds.endUpdateBatch();

      gDownloadManager.endBatchUpdate();
      // If there's nothing on the panel now, skip setting the selection
      if (gDownloadView.treeBoxObject.view && gDownloadView.treeBoxObject.view.rowCount > 0) {
        // Select the item that replaced the first deleted one
        if (newSelectionPos >= gDownloadView.treeBoxObject.view.rowCount)
          newSelectionPos = gDownloadView.treeBoxObject.view.rowCount - 1;
        gDownloadView.view.selection.select(newSelectionPos);
        gDownloadView.treeBoxObject.ensureRowIsVisible(newSelectionPos);
        gStatusBar.label = createLocalFile(getSelectedItem().id).path;
      }
      else {
        // Nothing on the panel, so clear the Status Bar
        gStatusBar.label = "";
      }
      window.updateCommands("tree-select");
      break;
    case "cmd_selectAll":
      gDownloadView.view.selection.selectAll();
      break;
    case "cmd_copyurl":
      selectedItems = getSelectedItems();
      if (selectedItems.length > 0) {
        gStatusBar.label = copyToClipboard(selectedItems);
      }
      break;
    default:
    }
  },  

  onEvent: function dVC_onEvent (aEvent)
  {
    switch (aEvent) {
    case "tree-select":
      this.onCommandUpdate();
    }
  },

  onCommandUpdate: function dVC_onCommandUpdate ()
  {
    var cmds = ["cmd_properties", "cmd_pause", "cmd_cancel", "cmd_remove",
                "cmd_copyurl", "cmd_openfile", "cmd_showinshell"];
    for (var command in cmds)
      goUpdateCommand(cmds[command]);
  }
};

function getSelectedItem()
{
  if (gDownloadView.currentIndex != -1)
    return gDownloadView.contentView.getItemAtIndex(gDownloadView.currentIndex);
  return null;
}

function getSelectedItems()
{
  var items = [];
  var k = 0;

  var selection = gDownloadView.view.selection;
  var rangeCount = selection.getRangeCount();
  for (var i = 0; i < rangeCount; i++) {
    var startIndex = {};
    var endIndex = {};
    selection.getRangeAt(i, startIndex, endIndex);
    for (var j = startIndex.value; j <= endIndex.value; j++)
      items[k++] = gDownloadView.contentView.getItemAtIndex(j);
  }

  return items;
}

function createLocalFile(aFilePath) 
{
  const lfIID = Components.interfaces.nsILocalFile;
  // XXXvarga We should fix the download manager to be consistent, that is,
  // use urls instead of local paths when adding new items to the list.
  // Once it's fixed, the code below can be removed.
  // See bug 208113 for more details.

  var file;
  if (aFilePath.substring(0,5) == 'file:') {
    file = gFileHandler.getFileFromURLSpec(aFilePath)
                       .QueryInterface(lfIID);
  }
  else {
    const lfContractID = "@mozilla.org/file/local;1";
    file = Components.classes[lfContractID].createInstance(lfIID);
    file.initWithPath(aFilePath);
  }

  return file;
}

function DLManagerShutdown()
{
  try {
    var observerService = Components.classes[kObserverServiceProgID]
                     .getService(Components.interfaces.nsIObserverService);
    observerService.removeObserver(dlObserver, "download-starting");
  }
  catch (ex) {
  }
}

function setSortVariables(tree)
{
  var node;
  for (node = document.getElementById("Name"); node; node = node.nextSibling) {
    if (node.getAttribute("sortActive") == "true")
      break;
  }
  if (!node) {
    node = document.getElementById("Progress");
    node.setAttribute("sortActive", "true");
    node.setAttribute("sortDirection", "descending");
  }

  tree.setAttribute("sortActive", "true");
  tree.setAttribute("sortDirection", node.getAttribute("sortDirection"));
  tree.setAttribute("sortResource", node.getAttribute("resource"));
}

function doSort(node)
{
  if (node.localName != "treecol")
    return;

  var sortResource = node.getAttribute("resource");

  var sortDirection = node.getAttribute("sortDirection");
  sortDirection = sortDirection == "ascending" ? "descending" : "ascending";

  try {
    var sortService = Components.classes["@mozilla.org/xul/xul-sort-service;1"]
                      .getService(Components.interfaces.nsIXULSortService);
    sortService.sort(node, sortResource, sortDirection);
  }
  catch(ex) {
  }
}

function copyToClipboard(selectedItems)
{
  var urlArray = new Array(selectedItems.length);
  for (var i = 0; i < selectedItems.length; ++i) {
    urlArray[i] = selectedItems[i].firstChild.lastChild.getAttribute("label");
  }

  var clipboardHelper = Components.classes["@mozilla.org/widget/clipboardhelper;1"]
                                  .getService(Components.interfaces.nsIClipboardHelper);
  clipboardHelper.copyString(urlArray.join("\n"));

  return urlArray.join(" "); // for status text
}
