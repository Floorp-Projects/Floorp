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

var NC_NS = "http://home.netscape.com/NC-rdf#";

var gDownloadView = null;
var gDownloadViewChildren = null;
var gRDFService = null;

var gNC_File = null;

function NODE_ID(aElement)
{
  return aElement.getAttribute("ref") || aElement.id;
}

function Startup()
{
  const rdfSvcContractID = "@mozilla.org/rdf/rdf-service;1";
  const rdfSvcIID = Components.interfaces.nsIRDFService;
  gRDFService = Components.classes[rdfSvcContractID].getService(rdfSvcIID);
  
  gNC_File = gRDFService.GetResource(NC_NS + "File");

  gDownloadView = document.getElementById("downloadView");
  gDownloadViewChildren = document.getElementById("downloadViewChildren");
  
  // Select the first item in the view, if any. 
  if (gDownloadViewChildren.hasChildNodes()) 
    gDownloadView.selectItem(gDownloadViewChildren.firstChild);
   
  gDownloadView.controllers.appendController(downloadViewController);
}

function Test()
{  
  // Test code:
  var uriContractID = "@mozilla.org/network/standard-url;1";
  var uriIID = Components.interfaces.nsIURI;
  var uri = Components.classes[uriContractID].createInstance(uriIID);
  uri.spec = "http://www.silverstone.net.nz/mozilla/fsmoz.png";

  var lfContractID = "@mozilla.org/file/local;1";
  var lfIID = Components.interfaces.nsILocalFile;
  var lf = Components.classes[lfContractID].createInstance(lfIID);
  lf.initWithPath("C:\\GOATS\\goat.png");

  var dlmgrContractID = "@mozilla.org/download-manager;1";
  var dlmgrIID = Components.interfaces.nsIDownloadManager;
  var dlmgr = Components.classes[dlmgrContractID].getService(dlmgrIID);
  
  var ds = dlmgr.QueryInterface(Components.interfaces.nsIRDFDataSource);
  gDownloadView.database.AddDataSource(ds);
  gDownloadView.builder.rebuild();
  dlmgr.addItem("Some File", uri, lf, null, null);
  gDownloadView.builder.rebuild();  
  dump("*** goat\n");
}

function Shutdown()
{

}

var downloadView = {
  onClick: function downloadView_click(aEvent)
  {
  
  },
  
  onSelect: function downloadView_select(aEvent)
  {
  
  },
  
};

var downloadViewController = {
  supportsCommand: function dVC_supportsCommand (aCommand)
  {
    switch (aCommand) {
    case "cmd_downloadFile":
    case "cmd_properties":
    case "cmd_pause":
    case "cmd_delete":
    case "cmd_openfile":
    case "cmd_showinshell":
    case "cmd_selectAll":
      return true;
    }
    return false;
  },
  
  isCommandEnabled: function dVC_isCommandEnabled (aCommand)
  {
    var cmds = ["cmd_properties", "cmd_pause", "cmd_delete",
                "cmd_openfile", "cmd_showinshell"];
    var selectionCount = gDownloadView.selectedItems.length;
    switch (aCommand) {
    case "cmd_downloadFile":
      downloadFile();
      return true;
    case "cmd_openfile":
    case "cmd_showinshell":
      if (selectionCount != 1)
        return false;
      var file = getFileForItem(gDownloadView.selectedItems[0]);
      return file.exists();
    case "cmd_properties":
      return selectionCount == 1;
    case "cmd_pause":
    case "cmd_delete":
      return selectionCount > 0;
    case "cmd_selectAll":
      return gDownloadViewChildren.childNodes.length != selectionCount;
    default:
    }
  },
  
  doCommand: function dVC_doCommand (aCommand)
  {
    dump("*** command = " + aCommand + "\n");
    var selection = gDownloadView.selectedItems;
    switch (aCommand) {
    case "cmd_downloadFile":
      dump("*** show a dialog that lets a user specify a URL to download\n");
      break;
    case "cmd_properties":
      dump("*** show properties for selected item\n");
      break;
    case "cmd_openfile":
      dump("*** launch the file for the selected item\n");
      var file = getFileForItem(selection[0]);
      file.launch();
      break;
    case "cmd_showinshell":
      dump("*** show the containing folder for the selected item\n");
      var file = getFileForItem(selection[0]);
      file.reveal();
      break;
    case "cmd_pause":
      dump("*** pause the transfer for the selected item\n");
      break;
    case "cmd_delete":
      dump("*** delete entries for the selection\n");
      // a) Prompt user to confirm end of transfers in progress
      // b) End transfers
      // c) Delete entries from datasource
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
    var cmds = ["cmd_properties", "cmd_pause", "cmd_delete",
                "cmd_openfile", "cmd_showinshell"];
    for (var command in cmds)
      goUpdateCommand(cmds[command]);
  }
};

function downloadFile()
{
  var bundle = document.getElementById("downloadBundle");
  
  // Select a file to download
  const promptContractID = "@mozilla.org/embedcomp/prompt-service;1";
  const promptIID = Components.interfaces.nsIPromptService;
  var promptSvc = Components.classes[promptContractID].getService(promptIID);
  
  var downloadFileTitle = bundle.getString("downloadFileTitle");
  var downloadFileMsg = bundle.getString("downloadFileMsg");
  
  var rv = { value: "" };
  var accept = promptSvc.prompt(window, downloadFileTitle, downloadFileMsg, 
                                rv, null, { });
  
  if (accept && rv.value != "") {
    // Now select a location to save it to
    const fpContractID = "@mozilla.org/filepicker;1";
    const fpIID = Components.interfaces.nsIFilePicker;
    var fp = Components.classes[fpContractID].getService(fpIID);
    
    // XXX-todo: make this file picker use the user's download folder
    var title = bundle.getString("chooseDestinationTitle");
    fp.init(window, title, fpIID.modeSave);
    fp.appendFilters(fpIID.filterAll);
    if (fp.show() == fpIID.returnOK) {
      var uriContractID = "@mozilla.org/network/standard-url;1";
      var uriIID = Components.interfaces.nsIURI;
      var uri = Components.classes[uriContractID].createInstance(uriIID);
      uri.spec = rv.value;
    
      var dlmgrContractID = "@mozilla.org/download-manager;1";
      var dlmgrIID = Components.interfaces.nsIDownloadManager;
      var dlmgr = Components.classes[dlmgrContractID].getService(dlmgrIID);
      dlmgr.addItem(fp.file.leafName, uri, fp.file, null, null);
    }
  }
}

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
  dump("*** aPath = " + aFilePath + "\n");
  lf.initWithPath(aFilePath);
  return lf;
}

