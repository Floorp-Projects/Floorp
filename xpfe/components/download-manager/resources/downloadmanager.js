/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Jan Varga <varga@utcru.sk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const NC_NS = "http://home.netscape.com/NC-rdf#";

var gDownloadView = null;
var gDownloadManager = null;
var gRDFService = null;
var gNC_File = null;
var gStatusBar = null;

const dlObserver = {
  observe: function(subject, topic, state) {
    if (topic != "download-starting") return;
    selectDownload(subject.QueryInterface(Components.interfaces.nsIDownload));
  }
};

function selectDownload(aDownload)
{
  var dlElt = document.getElementById(aDownload.target.path);
  var dlIndex = gDownloadView.contentView.getIndexOfItem(dlElt);
  gDownloadView.treeBoxObject.selection.select(dlIndex);
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

  var ds = window.arguments[0];
  gDownloadView.database.AddDataSource(ds);
  gDownloadView.builder.rebuild();
  window.setTimeout(onRebuild, 0);

  var key;
  if (navigator.platform.indexOf("Win") != -1)
    key = "Win";
  else if (navigator.platform.indexOf("Mac") != -1)
    key = "Mac";
  else {
    key = "Unix";
    document.getElementById("btn_openfile").hidden = true;
  }

  var bundle = document.getElementById("dlMgrBundle")
  var label = bundle.getString("showInShellLabel" + key);
  var accesskey = bundle.getString("showInShellAccesskey" + key);
  var showBtn = document.getElementById("btn_showinshell");
  showBtn.setAttribute("label", label);
  showBtn.setAttribute("accesskey", accesskey);
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
    gDownloadView.treeBoxObject.selection.select(0);
  }
}

function onSelect(aEvent) {
  if (!gStatusBar)
    gStatusBar = document.getElementById("statusbar-text");
  
  var selectionCount = gDownloadView.treeBoxObject.selection.count;
  if (selectionCount == 1)
    gStatusBar.label = getSelectedItem().id;
  else
    gStatusBar.label = "";

  window.updateCommands("tree-select");
}
  
var downloadViewController = {
  supportsCommand: function dVC_supportsCommand (aCommand)
  {
    switch (aCommand) {
    case "cmd_properties":
    case "cmd_pause":
    case "cmd_cancel":
    case "cmd_remove":
    case "cmd_openfile":
    case "cmd_showinshell":
    case "cmd_selectAll":
      return true;
    }
    return false;
  },
  
  isCommandEnabled: function dVC_isCommandEnabled (aCommand)
  {
    if (!gDownloadView.treeBoxObject.selection) return false;
    var selectionCount = gDownloadView.treeBoxObject.selection.count;
    if (!selectionCount) return false;

    var selectedItem = getSelectedItem();
    var isDownloading = selectedItem && gDownloadManager.getDownload(selectedItem.id);

    switch (aCommand) {
    case "cmd_openfile":
      try {
        if (isDownloading || getFileForItem(selectedItem).isExecutable())
          return false;
      } catch(e) {
        // Exception means file doesn't exist; launch is not allowed.
        return false;
      }      
    case "cmd_showinshell":
      // we can't reveal until the download is complete, because we have not given
      // the file its final name until them.
      return selectionCount == 1 && !isDownloading;
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
      if (selectedItem && gDownloadManager.getDownload(selectedItem.id))
        gDownloadManager.openProgressDialogFor(selectedItem.id, window);
      break;
    case "cmd_openfile":
      selectedItem = getSelectedItem();
      if (selectedItem) {
        file = getFileForItem(selectedItem);
        file.launch();
      }
      break;
    case "cmd_showinshell":
      selectedItem = getSelectedItem();
      if (selectedItem) {
        file = getFileForItem(selectedItem);
        
        // on unix, open a browser window rooted at the parent
        if (navigator.platform.indexOf("Win") == -1 && navigator.platform.indexOf("Mac") == -1) {
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
      var observer = gDownloadView.builder.QueryInterface(Components.interfaces.nsIRDFObserver);
      var ds = gDownloadView.database;
      observer.beginUpdateBatch(ds);
      
      for (i = 0; i <= selectedItems.length - 1; ++i) {
        gDownloadManager.removeDownload(selectedItems[i].id);
      }

      gDownloadManager.endBatchUpdate();
      observer.endUpdateBatch(ds);
      var remote = window.arguments[0].QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
      remote.Flush();
      gDownloadView.builder.rebuild();
      // If there's nothing on the panel now, skip setting the selection
      if (gDownloadView.treeBoxObject.view && gDownloadView.treeBoxObject.view.rowCount > 0) {
        // Select the item that replaced the first deleted one
        if (newSelectionPos >= gDownloadView.treeBoxObject.view.rowCount)
          newSelectionPos = gDownloadView.treeBoxObject.view.rowCount - 1;
        gDownloadView.treeBoxObject.selection.select(newSelectionPos);
        gDownloadView.treeBoxObject.ensureRowIsVisible(newSelectionPos);
      }
      else {
        // Nothing on the panel, so clear the Status Bar
        gStatusBar.label = "";
      }
      break;
    case "cmd_selectAll":
      gDownloadView.treeBoxObject.selection.selectAll();
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
                "cmd_openfile", "cmd_showinshell"];
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

  var selection = gDownloadView.treeBoxObject.selection;
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

function getFileForItem(aElement)
{
  var itemResource = gRDFService.GetUnicodeResource(aElement.id);
  var fileResource = gDownloadView.database.GetTarget(itemResource, gNC_File, true);
  fileResource = fileResource.QueryInterface(Components.interfaces.nsIRDFResource);
  return createLocalFile(fileResource.Value);
}

function createLocalFile(aFilePath) 
{
  var lfContractID = "@mozilla.org/file/local;1";
  var lfIID = Components.interfaces.nsILocalFile;
  var lf = Components.classes[lfContractID].createInstance(lfIID);
  lf.initWithPath(aFilePath);
  return lf;
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
