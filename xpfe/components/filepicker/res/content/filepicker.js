/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Stuart Parmenter <pavlov@netscape.com>
 *  Brian Ryner <bryner@netscape.com>
 *  Jan Varga <varga@utcru.sk>
 *  Peter Annema <disttsc@bart.nl>
 */

const nsILocalFile        = Components.interfaces.nsILocalFile;
const nsILocalFile_PROGID = "component://mozilla/file/local";
const nsIFilePicker       = Components.interfaces.nsIFilePicker;
const nsIDirectoryServiceProvider = Components.interfaces.nsIDirectoryServiceProvider;
const nsIDirectoryServiceProvider_PROGID = "component://netscape/file/directory_service";
const nsStdURL_PROGID     = "component://netscape/network/standard-url";
const nsIFileURL          = Components.interfaces.nsIFileURL;
const NC_NAMESPACE_URI = "http://home.netscape.com/NC-rdf#";

var sfile = Components.classes[nsILocalFile_PROGID].createInstance(nsILocalFile);
var retvals;
var filePickerMode;
var currentFilter;
var lastClicked;
var dirHistory;
var homeDir;

var directoryTree;
var textInput;

var bundle = srGetStrBundle("chrome://global/locale/filepicker.properties");   

function onLoad() {
  dirHistory = new Array();

  directoryTree = document.getElementById("directoryTree");
  textInput = document.getElementById("textInput");

  if (window.arguments) {
    var o = window.arguments[0];
    retvals = o.retvals; /* set this to a global var so we can set return values */
    const title = o.title;
    filePickerMode = o.mode;
    if (o.displayDirectory)
      const directory = o.displayDirectory.path;
    const initialText = o.defaultString;
    const filterTitles = o.filters.titles;
    const filterTypes = o.filters.types;
    const numFilters = filterTitles.length;

    window.title = title;

    if (initialText) {
      textInput.value = initialText;
    }
    /* build filter popup */
    var filterPopup = document.createElement("menupopup");

    currentFilter = filterTypes[0];
    applyFilter();
    for (var i = 0; i < numFilters; i++) {
      var menuItem = document.createElement("menuitem");
      menuItem.setAttribute("value", filterTitles[i] + " (" + filterTypes[i] + ")");
      menuItem.setAttribute("filters", filterTypes[i]);
      filterPopup.appendChild(menuItem);
    }

    var filterMenuList = document.getElementById("filterMenuList");
    filterMenuList.appendChild(filterPopup);
  }
  
  try {
    var buttonLabel;
    switch (filePickerMode) {
      case nsIFilePicker.modeOpen:
        buttonLabel = bundle.GetStringFromName("openButtonLabel");
        break;
      case nsIFilePicker.modeSave:
        buttonLabel = bundle.GetStringFromName("saveButtonLabel");
        break;
      case nsIFilePicker.modeGetFolder:
        buttonLabel = bundle.GetStringFromName("selectFolderButtonLabel");
        break;
    }

    if (buttonLabel) {
      var okButton = document.getElementById("ok");
      okButton.setAttribute("value", buttonLabel);
    }
  } catch (exception) {
    // keep it set to "OK"
  }

  // setup the dialogOverlay.xul button handlers
  doSetOKCancel(onOK, onCancel);

  // get the home dir
  var dirServiceProvider = Components.classes[nsIDirectoryServiceProvider_PROGID].getService().QueryInterface(nsIDirectoryServiceProvider);
  var persistent = new Object();
  homeDir = dirServiceProvider.getFile("Home", persistent);

  if (directory)
    sfile.initWithPath(directory);
  if (!directory || !(sfile.exists() && sfile.isDirectory())) {
    // Start in the user's home directory
    sfile.initWithPath(homeDir.path);
  }

  retvals.buttonStatus = nsIFilePicker.returnCancel;

  gotoDirectory(sfile);
  textInput.focus();
}

function onFilterChanged(target)
{
  var filterTypes = target.getAttribute("filters");
  currentFilter = filterTypes;
  applyFilter();
}

function applyFilter()
{
  /* This is where we manipulate the DOM to create new <rule>s */
  var splitFilters = currentFilter.split("; ");
  var matchAllFiles = false;

  /* get just the extensions for each of the filters */
  var extensions = new Array(splitFilters.length);
  for (var j = 0; j < splitFilters.length; j++) {
    var tmpStr = splitFilters[j];
    if (tmpStr == "*") {
      matchAllFiles = true;
      break;
    } else
      extensions[j] = tmpStr.substring(1); /* chop off the '*' */
  }

  /* delete all rules except the first one */
  for (var j = 1;; j++) {
    var ruleNode = document.getElementById("matchRule."+j);
    if (ruleNode) {
      ruleNode.parentNode.removeChild(ruleNode);
    } else {
      break;
    }
  }

  /* if we are matching all files, just clear the extension attribute
     on the first match rule and we're done */
  var rule0 = document.getElementById("matchRule.0");
  if (matchAllFiles) {
    rule0.removeAttributeNS(NC_NAMESPACE_URI, "extension");
    directoryTree.builder.rebuild();
    return;
  }

  /* rule 0 is special */
  rule0.setAttributeNS(NC_NAMESPACE_URI, "extension" , extensions[0]);

  /* iterate through the remaining extensions, creating new rules */
  var ruleNode = document.getElementById("fileFilter");

  for (var k=1; k < extensions.length; k++) {
    var newRule = rule0.cloneNode(true);
    newRule.setAttribute("id", "matchRule."+k);
    newRule.setAttributeNS(NC_NAMESPACE_URI, "extension", extensions[k]);
    ruleNode.appendChild(newRule);
  }

  directoryTree.builder.rebuild();
}

function onOK()
{
  var ret = nsIFilePicker.returnCancel;

  var isDir = false;
  var isFile = false;

  var input = textInput.value;
  if (input[0] == '~') // XXX XP?
    input  = homeDir.path + input.substring(1);

  var file = sfile.clone().QueryInterface(nsILocalFile);
  if (!file)
    return false;

  /* XXX we need an XP way to test for an absolute path! */
  if (input[0] == '/')   /* an absolute path was entered */
    file.initWithPath(input);
  else {
    try {
      file.appendRelativePath(input);
    } catch (e) {
      dump("Can't append relative path '"+input+"':\n");
      return false;
    }
  }

  if (!file.exists() && (filePickerMode != nsIFilePicker.modeSave)) {
    return false;
  }

  if (file.exists()) {
    var isDir = file.isDirectory();
    var isFile = file.isFile();
  }

  switch(filePickerMode) {
  case nsIFilePicker.modeOpen:
    if (isFile) {
      retvals.directory = file.parent.path;
      ret = nsIFilePicker.returnOK;
    } else if (isDir) {
      if (!sfile.equals(file)) {
        gotoDirectory(file);
      }
      textInput.value = "";
      ret = nsIFilePicker.returnCancel;
    }
    break;
  case nsIFilePicker.modeSave:
    if (isFile) {
      // we need to pop up a dialog asking if you want to save
      rv = window.confirm(file.path + " " + bundle.GetStringFromName("confirmFileReplacing"));
      if (rv)
        ret = nsIFilePicker.returnReplace;
      else
        ret = nsIFilePicker.returnCancel;
      retvals.directory = file.parent.path;
    } else if (!file.exists()) {
      ret = nsIFilePicker.returnOK;
      retvals.directory = file.parent.path;
    }
    break;
  case nsIFilePicker.modeGetFolder:
    if (isDir) {
      retvals.directory = file.parent.path;
      ret = nsIFilePicker.returnOK;
    }
    break;
  }

  retvals.file = file;
  retvals.buttonStatus = ret;

  if (ret == nsIFilePicker.returnCancel)
    return false;
  else
    return true;
}

function onCancel()
{
  // Close the window.
  retvals.buttonStatus = nsIFilePicker.returnCancel;
  retvals.file = null;
  return true;
}

function onClick(e) {
  if ( e.detail == 2 ) {
    var file = URLpathToFile(e.target.parentNode.getAttribute("path"));

    if (file.isDirectory()) {
      gotoDirectory(file);
    }
    else if (file.isFile()) {
      /* what about symlinks? what if they symlink to a directory? */
      return doOKButton();
    }
  }
}

function onKeypress(e) {
  if (e.keyCode == 8) /* backspace */
    goUp();
}

function onSelect(e) {
  if (e.target.selectedItems.length != 1)
    return;
  var file = URLpathToFile(e.target.selectedItems[0].firstChild.getAttribute("path"));

  if (file.isFile()) {
    textInput.value = file.leafName;
    lastClicked = file.leafName;
  }
}

function onDirectoryChanged(target)
{
  var path = target.getAttribute("value");

  var file = Components.classes[nsILocalFile_PROGID].createInstance(nsILocalFile);
  file.initWithPath(path);

  gotoDirectory(file);
}

function addToHistory(directoryName) {
  var found = false;
  var i = 0;
  while (!found && i<dirHistory.length) {
    if (dirHistory[i] == directoryName)
      found = true;
    else
      i++;
  }

  if (found) {
    if (i!=0) {
      dirHistory.splice(i, 1);
      dirHistory.splice(0, 0, directoryName);
    }
  } else {
    dirHistory.splice(0, 0, directoryName);
  }

  var menu = document.getElementById("lookInMenu");

  var children = menu.childNodes;
  for (var i=0; i < children.length; i++)
    menu.removeChild(children[i]);

  for (var i=0; i < dirHistory.length; i++) {
    var menuItem = document.createElement("menuitem");
    menuItem.setAttribute("value", dirHistory[i]);
    menu.appendChild(menuItem);
  }

  var menuList = document.getElementById("lookInMenuList");
  menuList.selectedIndex = 0;
}

function goUp() {
  try {
    var parent = sfile.parent;
  } catch(ex) { dump("can't get parent directory\n"); }

  if (parent) {
    gotoDirectory(parent);
  }
}

function gotoDirectory(directory) {
  var newURL = fileToURL(directory);
  addToHistory(directory.path);
  directoryTree.setAttribute("ref", fileToURL(directory).spec);
  sfile = directory;
}

function fileToURL(aFile) {
  var newDirectoryURL = Components.classes[nsStdURL_PROGID].createInstance().QueryInterface(nsIFileURL);
  newDirectoryURL.file = aFile;
  return newDirectoryURL;
}

function URLpathToFile(aURLstr) {
  var fileURL = Components.classes[nsStdURL_PROGID].createInstance().QueryInterface(nsIFileURL);
  fileURL.spec = aURLstr;
  return fileURL.file;
}
