# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is Mozilla.org Code.
# 
# The Initial Developer of the Original Code is
# Doron Rosenberg.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@bengoodger.com>
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

#ifdef MOZ_PHOENIX
var _elementIDs = ["askOnSave", "downloadFolderList", "downloadFolder", "showWhenStarting", "closeWhenDone"];
#else
var _elementIDs = ["askOnSave", "downloadFolderList", "downloadFolder"];
#endif

var gLastSelectedIndex = 0;
var gHelperApps = null;

var gEditFileHandler, gRemoveFileHandler, gHandlersList;

var downloadDirPref = "browser.download.dir"; 
var downloadModePref = "browser.download.folderList";
const nsILocalFile = Components.interfaces.nsILocalFile;

function selectFolder()
{ 
  const nsIFilePicker = Components.interfaces.nsIFilePicker;
  var fp = Components.classes["@mozilla.org/filepicker;1"]
                      .createInstance(nsIFilePicker);
  var pref = Components.classes["@mozilla.org/preferences-service;1"]
                      .getService(Components.interfaces.nsIPrefBranch);

  var bundle = document.getElementById("strings");
  var description = bundle.getString("selectDownloadDir");
  fp.init(window, description, nsIFilePicker.modeGetFolder);
  try 
  {
    var initialDir = pref.getComplexValue(downloadDirPref, nsILocalFile);
    if (initialDir)
      fp.displayDirectory = initialDir;
  }
  catch (ex)
  {
    // ignore exception: file picker will open at default location
  }
  fp.appendFilters(nsIFilePicker.filterAll);
  var ret = fp.show();

  if (ret == nsIFilePicker.returnOK) {
    var localFile = fp.file.QueryInterface(nsILocalFile);
    pref.setComplexValue(downloadDirPref, nsILocalFile, localFile)
    selectCustomFolder(true);
  }
  else {
    var folderList = document.getElementById("downloadFolderList");
    folderList.selectedIndex = gLastSelectedIndex;
  }
}

function doEnabling(aSelectedItem)
{
  var textbox = document.getElementById("downloadFolderList");
  var button = document.getElementById("showFolder");
  var disable = aSelectedItem.id == "alwaysAsk";
  textbox.disabled = disable;
  button.disabled = disable;
}

function Startup() 
{
  var folderList = document.getElementById("downloadFolderList");
  
  const nsILocalFile = Components.interfaces.nsILocalFile;
  var pref = Components.classes["@mozilla.org/preferences-service;1"]
                      .getService(Components.interfaces.nsIPrefBranch);
                      
  try {
    var downloadDir = pref.getComplexValue(downloadDirPref, nsILocalFile);  
    
    var desktop = getDownloadsFolder("Desktop");
    var downloads = getDownloadsFolder("Documents");
    
    // Check to see if the user-entered download dir is actually one of our
    // enumerated values (Desktop, My Downloads) and if so select that
    // item instead of the user selected one. 
    // XXX - It's lame that I should have to compare the path directly. The 
    // win32 implementation of nsIFile should know that Windows is not case 
    // sensitive.
    var downloadPath = downloadDir.path.toUpperCase();
    if (downloadPath == desktop.path.toUpperCase()) {
      pref.clearUserPref(downloadDirPref);
      pref.setIntPref(downloadModePref, 0);
      folderList.selectedIndex = 0;
    }
    else if (downloadPath == downloads.path.toUpperCase()) {
      pref.clearUserPref(downloadDirPref);
      pref.setIntPref(downloadModePref, 1);
      folderList.selectedIndex = 1;
    }
  }
  catch (e) {
  }
  
  try {
    selectCustomFolder(false);
  }
  catch (e) {
  }
  
  gLastSelectedIndex = folderList.selectedIndex;
  
  // Initialize the File Type list
  gHelperApps = new HelperApps();
  
  gHandlersList = document.getElementById("fileHandlersList");
  gHandlersList.database.AddDataSource(gHelperApps);
  gHandlersList.setAttribute("ref", "urn:mimetypes");
  
  (gEditFileHandler = document.getElementById("editFileHandler")).disabled = true;
  (gRemoveFileHandler = document.getElementById("removeFileHandler")).disabled = true;
  
  parent.hPrefWindow.registerOKCallbackFunc(updateSaveToFolder);
  // XXXben such a hack. Should really update the OKCallbackFunction thing a bit to 
  //        let it support holding arbitrary data. 
  parent.hPrefWindow.getSpecialFolderKey = getSpecialFolderKey;

  // XXXben menulist hack #43. When initializing the display to the custom
  //        download path field, the field is blank. 
  var downloadFolderList = document.getElementById("downloadFolderList");
  downloadFolderList.parentNode.removeChild(downloadFolderList);
  var showFolder = document.getElementById("showFolder");
  showFolder.parentNode.insertBefore(downloadFolderList, showFolder);
  downloadFolderList.hidden = false;

#ifdef MOZ_PHOENIX
  toggleDMPrefUI(document.getElementById("showWhenStarting"));
#endif
  
  setTimeout("postStart()", 0);
}

function postStart()
{
  var downloadFolderList = document.getElementById("downloadFolderList");
  downloadFolderList.label = downloadFolderList.selectedItem.label;
}

function uninit()
{
  gHandlersList.database.RemoveDataSource(gHelperApps);
  
  gHelperApps.destroy();
}

// WARNING WARNING WARNING
// This is a Options OK Callback
// When this function is called the Downloads panel's document object 
// MAY NOT BE AVAILABLE. As a result referring to any item in it in this
// function will probably cause the Options window not to close when OK
// is pressed. 
function updateSaveToFolder()
{
  var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  function getPref(aPrefName, aPrefGetter, aDefVal) 
  {
    try {
      var val = prefs[aPrefGetter](aPrefName);
    }
    catch (e) {
      val = aDefVal;
    }
    return val;                
  }

  var defaultFolderPref = "browser.download.defaultFolder";
  var downloadDirPref = "browser.download.dir";

  var data = parent.hPrefWindow.wsm.dataManager.pageData["chrome://mozapps/content/downloads/pref-downloads.xul"].elementIDs;
  // Don't let the variable names here fool you. This code executes if the 
  // user chooses to have all files auto-download to a specific folder.
  if (data.askOnSave.value == "true") {
    var fileLocator = Components.classes["@mozilla.org/file/directory_service;1"].getService(Components.interfaces.nsIProperties);
    
    var bundle = Components.classes["@mozilla.org/intl/stringbundle;1"].getService(Components.interfaces.nsIStringBundleService);
    bundle = bundle.createBundle("chrome://mozapps/locale/downloads/unknownContentType.properties");
    var description = bundle.GetStringFromName("myDownloads");
    var targetFolder = null;

    switch (parseInt(data.downloadFolderList.value)) {
    case 1:
      targetFolder = fileLocator.get(parent.hPrefWindow.getSpecialFolderKey("Documents"), 
                                     Components.interfaces.nsIFile);
      targetFolder.append(description);
      break;
    case 2:
      targetFolder = prefs.getComplexValue(downloadDirPref, 
                                           Components.interfaces.nsILocalFile);  
      break;
    case 0:
    default:          
      targetFolder = fileLocator.get(parent.hPrefWindow.getSpecialFolderKey("Desktop"), 
                                      Components.interfaces.nsIFile);
      break;
    }
    prefs.setComplexValue(defaultFolderPref,
                          Components.interfaces.nsILocalFile,
                          targetFolder);
  }
  else if (prefs.prefHasUserValue(defaultFolderPref))
    prefs.clearUserPref(defaultFolderPref);  
}

function selectCustomFolder(aShouldSelectItem)
{
  var pref = Components.classes["@mozilla.org/preferences-service;1"]
                      .getService(Components.interfaces.nsIPrefBranch);
  var downloadDir = pref.getComplexValue(downloadDirPref, nsILocalFile);
  
  var folder = document.getElementById("downloadFolder");
  folder.label = downloadDir.path;
  folder.setAttribute("path", downloadDir.path);
  folder.hidden = false;
  
  var folderList = document.getElementById("downloadFolderList");
  if (aShouldSelectItem)
    folderList.selectedIndex = 2;
}

function folderListCommand(aEvent)
{
  var folderList = document.getElementById("downloadFolderList");
  if (folderList.selectedItem.getAttribute("value") == 9)
    selectFolder();
  
  var selectedIndex = folderList.selectedIndex;

  if (selectedIndex == 1) {
    var downloads = getDownloadsFolder("Documents");
    if (!downloads.exists())
      downloads.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  }
  
  // folderList.inputField.readonly = (selectedIndex == 0 || selectedIndex == 1);

  gLastSelectedIndex = folderList.selectedIndex;
}

function showFolder()
{
  var folderList = document.getElementById("downloadFolderList");

  var folder = null;
  
  switch (folderList.selectedIndex) {
  case 0:
    folder = getDownloadsFolder("Desktop");
    break;
  case 1:
    folder = getDownloadsFolder("Downloads");
    break;
  case 2:
    var path = document.getElementById("downloadFolder").getAttribute("path");
    folder = Components.classes["@mozilla.org/file/local;1"].createInstance(Components.interfaces.nsILocalFile);
    folder.initWithPath(path);
    break;
  }

  folder.reveal();
}

function getSpecialFolderKey(aFolderType) 
{
#ifdef XP_WIN
  return aFolderType == "Desktop" ? "DeskV" : "Pers";
#endif
#ifdef XP_MACOSX
  return aFolderType == "Desktop" ? "UsrDsk" : "UsrDocs";
#endif
  return "Home";
}

function getDownloadsFolder(aFolder)
{
  var fileLocator = Components.classes["@mozilla.org/file/directory_service;1"].getService(Components.interfaces.nsIProperties);

  var dir = fileLocator.get(getSpecialFolderKey(aFolder), Components.interfaces.nsILocalFile);
  var bundle = document.getElementById("strings");
  var description = bundle.getString("myDownloads");
  if (aFolder != "Desktop")
    dir.append(description);
    
  return dir;
}

function fileHandlerListSelectionChanged(aEvent)
{
  var selection = gHandlersList.view.selection; 
  var selected = selection.count;
  gRemoveFileHandler.disabled = selected == 0;
  gEditFileHandler.disabled = selected != 1;
  
  var canRemove = true;
  
  var cv = gHandlersList.contentView;
  var rangeCount = selection.getRangeCount();
  var min = { }, max = { };
  for (var i = 0; i < rangeCount; ++i) {
    selection.getRangeAt(i, min, max);
    
    for (var j = min.value; j <= max.value; ++j) {
      var item = cv.getItemAtIndex(j);
      var editable = gHelperApps.getLiteralValue(item.id, "editable") == "true";
      var handleInternal = gHelperApps.getLiteralValue(item.id, "handleInternal");
      
      if (!editable || handleInternal) 
        canRemove = false;
    }
  }
  
  if (!canRemove) {
    gRemoveFileHandler.disabled = true;
    gEditFileHandler.disabled = true;
  }
}

function removeFileHandler()
{
  const nsIPS = Components.interfaces.nsIPromptService;
  var ps = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(nsIPS);
  
  var bundle = document.getElementById("strings");
  var title = bundle.getString("removeActions");
  var msg = bundle.getString("removeActionsMsg");

  var buttons = (nsIPS.BUTTON_TITLE_YES * nsIPS.BUTTON_POS_0) + (nsIPS.BUTTON_TITLE_NO * nsIPS.BUTTON_POS_1);

  if (ps.confirmEx(window, title, msg, buttons, "", "", "", "", { }) == 1) 
    return;
  
  var c = Components.classes["@mozilla.org/rdf/container;1"].createInstance(Components.interfaces.nsIRDFContainer);
  c.Init(gHelperApps, gRDF.GetResource("urn:mimetypes:root"));
  
  var cv = gHandlersList.contentView;
  var selection = gHandlersList.view.selection; 
  var rangeCount = selection.getRangeCount();
  var min = { }, max = { };
  
  var lastAdjacent = -1;
  for (var i = 0; i < rangeCount; ++i) {
    selection.getRangeAt(i, min, max);
    
    if (i == (rangeCount - 1)) { 
      if (min.value >= (gHandlersList.view.rowCount - selection.count)) 
        lastAdjacent = min.value - 1;
      else
        lastAdjacent = min.value;
    }
    
    for (var j = max.value; j >= min.value; --j) {
      var item = cv.getItemAtIndex(j);
      var itemResource = gRDF.GetResource(item.id);
      c.RemoveElement(itemResource, j == min.value);
      
      cleanResource(itemResource);
    }
  }

  if (lastAdjacent != -1) {
    selection.select(lastAdjacent);
    gHandlersList.focus();
  }
  
  gHelperApps.flush();
}

function cleanResource(aResource)
{
  var handlerProp = gHelperApps.GetTarget(aResource, gHelperApps._handlerPropArc, true);
  if (handlerProp) {
    var extApp = gHelperApps.GetTarget(handlerProp, gHelperApps._externalAppArc, true);
    if (extApp)
      disconnect(extApp);
    disconnect(handlerProp);
  }
  disconnect(aResource);
}

function disconnect(aResource)
{
  var arcs = gHelperApps.ArcLabelsOut(aResource);
  while (arcs.hasMoreElements()) {
    var arc = arcs.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
    var val = gHelperApps.GetTarget(aResource, arc, true);
    gHelperApps.Unassert(aResource, arc, val, true);
  }
}

function editFileHandler()
{
  var selection = gHandlersList.view.selection; 
  
  var cv = gHandlersList.contentView;
  var item = cv.getItemAtIndex(selection.currentIndex);
  var itemResource = gRDF.GetResource(item.id);
  
  openDialog("chrome://mozapps/content/downloads/editAction.xul", "", "modal=yes", itemResource);
}

# XXXben we should handle this a little better. 
function showPlugins()
{
  openDialog("chrome://browser/content/pref/plugins.xul", "", "modal,resizable");
}

function toggleDMPrefUI(aCheckbox)
{
  if (aCheckbox.checked) 
    document.getElementById("closeWhenDone").removeAttribute("disabled");
  else
    document.getElementById("closeWhenDone").setAttribute("disabled", "true");
}

