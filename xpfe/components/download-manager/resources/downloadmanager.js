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
var gDownloadViewChildren = null;
var gDownloadManager = null;
var gRDFService = null;
var gNC_File = null;
var gStatusBar = null;

function NODE_ID(aElement)
{
  return aElement.id || aElement.getAttribute("ref");
}

function Startup()
{
  if (!window.arguments.length)
    return;

  const rdfSvcContractID = "@mozilla.org/rdf/rdf-service;1";
  const rdfSvcIID = Components.interfaces.nsIRDFService;
  gRDFService = Components.classes[rdfSvcContractID].getService(rdfSvcIID);
  
  gNC_File = gRDFService.GetResource(NC_NS + "File");

  gDownloadView = document.getElementById("downloadView");
  gDownloadViewChildren = document.getElementById("downloadViewChildren");
  
  gDownloadView.controllers.appendController(downloadViewController);
  const dlmgrContractID = "@mozilla.org/download-manager;1";
  const dlmgrIID = Components.interfaces.nsIDownloadManager;
  gDownloadManager = Components.classes[dlmgrContractID].getService(dlmgrIID);

  var ds = window.arguments[0];
  gDownloadView.database.AddDataSource(ds);
  gDownloadView.builder.rebuild();
  gDownloadView.focus();
  // Select the first item in the view, if any. 
  if (gDownloadViewChildren.hasChildNodes()) 
    gDownloadView.selectItem(gDownloadViewChildren.firstChild);
  
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

function openPropertiesDialog()
{
  var selection = gDownloadView.selectedItems;
  gDownloadManager.openProgressDialogFor(selection[0].id, window);
}

function onSelect(aEvent) {
  if (!gStatusBar)
    gStatusBar = document.getElementById("statusbar-text");
  if (gDownloadView.getRowCount() && gDownloadView.selectedItems.length)
    gStatusBar.label = gDownloadView.selectedItems[0].id;
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
    var cmds = ["cmd_properties", "cmd_pause", "cmd_cancel",
                "cmd_openfile", "cmd_showinshell"];
    var selectionCount = gDownloadView.selectedItems.length;
    if (!selectionCount || !gDownloadView.getRowCount()) return false;

    var isDownloading = gDownloadManager.getDownload(gDownloadView.selectedItems[0].id);
    switch (aCommand) {
    case "cmd_openfile":
      if (!isDownloading && getFileForItem(gDownloadView.selectedItems[0]).isExecutable())
        return false;

    case "cmd_showinshell":
      // some apps like kazaa/morpheus let you "preview" in-progress downloads because
      // that's possible for movies and music. for now, just disable indiscriminately.
      return selectionCount == 1;
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
      return !isDownloading;
    case "cmd_selectAll":
      return gDownloadViewChildren.childNodes.length != selectionCount;
    default:
      return false;
    }
  },
  
  doCommand: function dVC_doCommand (aCommand)
  {
    var selection = gDownloadView.selectedItems;
    var i, file;
    switch (aCommand) {
    case "cmd_properties":
      openPropertiesDialog();
      break;
    case "cmd_openfile":
      file = getFileForItem(selection[0]);
      file.launch();
      break;
    case "cmd_showinshell":
      file = getFileForItem(selection[0]);
      
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
      break;
    case "cmd_pause":
      break;
    case "cmd_cancel":
      // XXX we should probably prompt the user
      for (i = 0; i < selection.length; ++i)
        gDownloadManager.cancelDownload(selection[i].id);
      window.updateCommands("tree-select");
      break;
    case "cmd_remove":
      for (i = 0; i < selection.length; ++i)
        gDownloadManager.removeDownload(selection[i].id);
      window.updateCommands("tree-select");
      break;
    case "cmd_selectAll":
      gDownloadView.selectAll();
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

function getFileForItem(aElement)
{
  var itemResource = gRDFService.GetResource(NODE_ID(aElement));
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

