/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Ben Goodger <ben@netscape.com> (Original Author)
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
    // If we're being opened as a folder selection window
    if (window.arguments[4] == "selectFolder") {
      document.getElementById("bookmarknamegrid").setAttribute("hidden", "true");
      toggleCreateIn();
      document.getElementById("createinlabel").setAttribute("collapsed", "true");
      document.getElementById("createinbuttonbox").setAttribute("hidden", "true");
      document.getElementById("dontaskagain").setAttribute("hidden", "true");
      document.getElementById("createinseparator").setAttribute("hidden", "true");
      sizeToContent();
      const kWindowNode = document.getElementById("newBookmarkWindow");
      kWindowNode.setAttribute("title", kWindowNode.getAttribute("title-selectFolder"));
      shouldSetOKButton = false;
    }
    else {
      gFld_Name.value = window.arguments[0] || "";
      gFld_URL.value = window.arguments[1] || "";
      gBookmarkCharset = window.arguments [3] || null;
      if (window.arguments[2]) {
        gCreateInFolder = window.arguments[2];
        document.getElementById("createin").setAttribute("hidden", "true");
        document.getElementById("folderbox").setAttribute("hidden", "true");
      }
    }
  }
  
  if (shouldSetOKButton)
    onLocationInput();
  gFld_Name.focus();
} 

function toggleCreateIn()
{
  var folderbox = document.getElementById("folderbox");
  var createInButton = document.getElementById("createin");
  var dontaskagain = document.getElementById("dontaskagain");
  var oldID, newID;
  if (folderbox.getAttribute("hidden") == "true") {
    createInButton.value = createInButton.getAttribute("value2");
    folderbox.removeAttribute("hidden");
    dontaskagain.removeAttribute("hidden");
    oldID = "buttonsparent";
    newID = "openParent";
  }
  else {
    createInButton.value = createInButton.getAttribute("value1");
    folderbox.setAttribute("hidden", "true");
    dontaskagain.setAttribute("hidden", "true");
    oldID = "openParent";
    newID = "buttonsparent";
  }
  var oldParent = document.getElementById(oldID);
  var newParent = document.getElementById(newID);
  var buttons = oldParent.firstChild.cloneNode(true);
  oldParent.removeChild(oldParent.firstChild);
  newParent.appendChild(buttons);
  sizeToContent();
}

function onLocationInput ()
{
  var ok = document.getElementById("ok");
  ok.disabled = gFld_URL.value == "";
}

function onOK()
{
  if (window.arguments[4] == "selectFolder")
    window.arguments[5].selectedFolder = gCreateInFolder;
  else {
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
      url = kLF.URL;
    }
    catch (e) {
    }
    
    kBMS.AddBookmarkToFolder(url, rFolder, gFld_Name.value, gBookmarkCharset);
  
    // Persist the 'show this dialog again' preference.   
    var checkbox = document.getElementById("dontaskagain");
    const kPrefContractID = "@mozilla.org/preferences;1";
    const kPrefIID = Components.interfaces.nsIPref;
    const kPrefSvc = Components.classes[kPrefContractID].getService(kPrefIID);
    try {
      kPrefSvc.SetBoolPref("browser.bookmarks.add_without_dialog", checkbox.checked);
    }
    catch (e) {
    }
  }
  close();
}

function onTreeSelect ()
{
  if (gFolderTree.selectedItems.length < 1) 
    gCreateInFolder = "NC:NewBookmarkFolder";
  var selectedItem = gFolderTree.selectedItems[0];
  gCreateInFolder = selectedItem.id;
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




