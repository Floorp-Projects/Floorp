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

/**
 * Add Bookmark Dialog. 
 * ====================
 * 
 * This is a generic bookmark dialog that allows for bookmark addition
 * and folder selection. It can be opened with various parameters that 
 * result in appearance/purpose differences and initial state. 
 * 
 * Use: Open with 'openDialog', with the flags 
 *        'centerscreen,chrome,dialog=no,resizable=yes'
 * 
 * Parameters: 
 *   Apart from the standard openDialog parameters, this dialog can 
 *   be passed additional information, which gets mapped to the 
 *   window.arguments array:
 * 
 *   window.arguments[0]: Bookmark Name. The value to be prefilled
 *                        into the "Name: " field (if visible).
 *   window.arguments[1]: Bookmark URL: The location of the bookmark.
 *                        The value to be filled in the "Location: "
 *                        field (if visible).
 *   window.arguments[2]: Bookmark Folder. The RDF Resource URI of the
 *                        folder that this bookmark should be created in.
 *   window.arguments[3]: Bookmark Charset. The charset that should be
 *                        used when adding a bookmark to the specified
 *                        URL. (Usually the charset of the current 
 *                        document when launching this window). 
 *   window.arguments[4]: The mode of operation. See notes for details.
 *
 * Mode of Operation Notes:
 * ------------------------
 * This dialog can be opened in four different ways by using a parameter
 * passed through the call to openDialog. The 'mode' of operation
 * of the window is expressed in window.arguments[4]. The valid modes are:
 *
 * 1) <default> (no fifth open parameter).
 *      Opens this dialog with the bookmark Name, URL and folder selection
 *      components visible. 
 * 2) "newBookmark" (fifth open parameter = String("newBookmark"))
 *      Opens the dialog as in (1) above except the folder selection tree
 *      is hidden. This type of mode is useful when the creation folder 
 *      is pre-determined.
 * 3) "selectFolder" (fifth open parameter = String("selectFolder"))
 *      Opens the dialog as in (1) above except the Name/Location section
 *      is hidden, and the dialog takes on the utility of a Folder chooser.
 *      Used when the user must select a Folder for some purpose. 
 */

var gFld_Name   = null;
var gFld_URL    = null; 
var gFolderTree = null;

var gBookmarkCharset = null;

const kRDFSContractID = "@mozilla.org/rdf/rdf-service;1";
const kRDFSIID = Components.interfaces.nsIRDFService;
const kRDF = Components.classes[kRDFSContractID].getService(kRDFSIID);

var gSelectItemObserver = null;

var gCreateInFolder = "NC:NewBookmarkFolder";

function Startup()
{
  doSetOKCancel(onOK);
  gFld_Name = document.getElementById("name");
  gFld_URL = document.getElementById("url");
  gFolderTree = document.getElementById("folders");
  
  var shouldSetOKButton = true;
  if ("arguments" in window) {
    switch (window.arguments[4]) {
    case "selectFolder":
      // If we're being opened as a folder selection window
      document.getElementById("bookmarknamegrid").setAttribute("hidden", "true");
      document.getElementById("createinseparator").setAttribute("hidden", "true");
      document.getElementById("nameseparator").setAttribute("hidden", "true");
      sizeToContent();
      var windowNode = document.getElementById("newBookmarkWindow");
      windowNode.setAttribute("title", windowNode.getAttribute("title-selectFolder"));
      shouldSetOKButton = false;
      var folderItem = document.getElementById(window.arguments[2]);
      if (folderItem)
        gFolderTree.selectItem(folderItem);
      break;
    case "newBookmark":
      setupFields();
      if (window.arguments[2])
        gCreateInFolder = window.arguments[2];
      document.getElementById("folderbox").setAttribute("hidden", "true");
      windowNode = document.getElementById("newBookmarkWindow");
      windowNode.removeAttribute("persist");
      windowNode.removeAttribute("height");
      windowNode.removeAttribute("width");
      windowNode.setAttribute("style", windowNode.getAttribute("style"));
      sizeToContent();
      break;
    default:
      // Regular Add Bookmark
      setupFields();
      if (window.arguments[2]) {
        gCreateInFolder = window.arguments[2];
        var folderItem = document.getElementById(gCreateInFolder);
        if (folderItem)
          gFolderTree.selectItem(folderItem);
      }
    }
  }
  
  if (shouldSetOKButton)
    onLocationInput();
  gFld_Name.select();
  gFld_Name.focus();
} 

function setupFields()
{
  // New bookmark in predetermined folder. 
  gFld_Name.value = window.arguments[0] || "";
  gFld_URL.value = window.arguments[1] || "";
  onLocationInput();
  gFld_Name.select();
  gFld_Name.focus();
  gBookmarkCharset = window.arguments [3] || null;
}

function onLocationInput ()
{
  var ok = document.getElementById("ok");
  ok.disabled = gFld_URL.value == "";
}

function onOK()
{
  // In Select Folder Mode, do nothing but tell our caller what
  // folder was selected. 
  if (window.arguments[4] == "selectFolder")
    window.arguments[5].selectedFolder = gCreateInFolder;
  else {
    // Otherwise add a bookmark to the selected folder. 

    const kBMDS = kRDF.GetDataSource("rdf:bookmarks");
    const kBMSContractID = "@mozilla.org/browser/bookmarks-service;1";
    const kBMSIID = Components.interfaces.nsIBookmarksService;
    const kBMS = Components.classes[kBMSContractID].getService(kBMSIID);
    var rFolder = kRDF.GetResource(gCreateInFolder, true);
    const kRDFCContractID = "@mozilla.org/rdf/container;1";
    const kRDFIID = Components.interfaces.nsIRDFContainer;
    const kRDFC = Components.classes[kRDFCContractID].getService(kRDFIID);
    try {
      kRDFC.Init(kBMDS, rFolder);
    }
    catch (e) {
      // No "NC:NewBookmarkFolder" exists, just append to the root.
      rFolder = kRDF.GetResource("NC:BookmarksRoot", true);
      kRDFC.Init(kBMDS, rFolder);
    }
    if (!gFld_URL.value) return;
    
    // Check to see if the item is a local directory path, and if so, convert
    // to a file URL so that aggregation with rdf:files works
    var url = gFld_URL.value;
    try {
      const kLFContractID = "@mozilla.org/file/local;1";
      const kLFIID = Components.interfaces.nsILocalFile;
      const kLF = Components.classes[kLFContractID].createInstance(kLFIID);
      kLF.initWithUnicodePath(url);
      if (kLF.exists()) 
          url = kLF.URL;
    }
    catch (e) {
    }
    
    kBMS.AddBookmarkToFolder(url, rFolder, gFld_Name.value, gBookmarkCharset);
  }
  close();
}

function onTreeSelect ()
{
  if (gFolderTree.selectedItems.length < 1) 
    gCreateInFolder = "NC:NewBookmarkFolder";
  else {
    var selectedItem = gFolderTree.selectedItems[0];
    gCreateInFolder = selectedItem.id;
  }
}

var gBookmarksShell = null;
function createNewFolder ()
{
  // ick. 
  gBookmarksShell = new BookmarksTree("folders");
  var item = null;
  var folderKids = document.getElementById("folderKids");
  if (gFolderTree.selectedItems.length < 1)
    item = folderKids.firstChild;
  item = gFolderTree.selectedItems.length < 1 ? folderKids.firstChild : gFolderTree.selectedItems[0];
  gBookmarksShell.commands.createBookmarkItem("folder", item);
}

function useDefaultFolder ()
{
  const kBMDS = kRDF.GetDataSource("rdf:bookmarks");
  var newBookmarkFolder = document.getElementById("NC:NewBookmarkFolder");
  if (newBookmarkFolder) {
    gFolderTree.selectItem(newBookmarkFolder);
    gCreateInFolder = "NC:NewBookmarkFolder";
  }
  else {
    gFolderTree.clearItemSelection();
    gCreateInFolder = "NC:BookmarksRoot";
  }
}


