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
 */

const nsILocalFile        = Components.interfaces.nsILocalFile;
const nsILocalFile_PROGID = "component://mozilla/file/local";
const nsIFilePicker       = Components.interfaces.nsIFilePicker;
const nsIDirectoryServiceProvider = Components.interfaces.nsIDirectoryServiceProvider;
const nsIDirectoryServiceProvider_PROGID = "component://netscape/file/directory_service";

var sfile = Components.classes[nsILocalFile_PROGID].createInstance(nsILocalFile);
var retvals;
var filePickerMode;
var currentFilter;
var lastClicked;

var directoryTree;
var textInput;

var bundle = srGetStrBundle("chrome://global/locale/filepicker.properties");   

function onLoad() {

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
    for (var i = 0; i < numFilters; i++) {
      var menuItem = document.createElement("menuitem");
      menuItem.setAttribute("value", filterTitles[i] + " (" + filterTypes[i] + ")");
      menuItem.setAttribute("filters", filterTypes[i]);
      filterPopup.appendChild(menuItem);
    }

    var filterMenuList = document.getElementById("filterMenuList");
    filterMenuList.appendChild(filterPopup);
  }

  // setup the dialogOverlay.xul button handlers
  doSetOKCancel(onOK, onCancel);

  if (directory)
    sfile.initWithPath(directory);
  if (!directory || !(sfile.exists() && sfile.isDirectory())) {
    // Start in the user's home directory
    var dirServiceProvider = Components.classes[nsIDirectoryServiceProvider_PROGID].getService().QueryInterface(nsIDirectoryServiceProvider);
    var persistent = new Object();
    var homeDir = dirServiceProvider.getFile("Home", persistent);
    sfile.initWithPath(homeDir.path);
  }

  retvals.buttonStatus = nsIFilePicker.returnCancel;
  addToHistory(sfile.path);

  getDirectoryContents(directoryTree, sfile.directoryEntries);

  textInput.focus();
}

function onFilterChanged(target)
{
  var filterTypes = target.getAttribute("filters");
  currentFilter = filterTypes;

  loadDirectory();
}

function onOK()
{
  var ret = nsIFilePicker.returnCancel;
  textInput = document.getElementById("textInput");

  var file = Components.classes[nsILocalFile_PROGID].createInstance(nsILocalFile);
  file.initWithPath(textInput.value);

  var isDir = false;
  var isFile = false;

  if (file.exists()) {
    isDir = file.isDirectory();
    isFile = file.isFile();
  } else {
    /* look for something in our current directory */
    var nfile = sfile.clone().QueryInterface(nsILocalFile);
    if (file.path[0] == '/')   /* an absolute path was entered */
      nfile.initWithPath(file.path)
    else
      nfile.appendRelativePath(file.path);
    /*    dump(nfile.path); */
    if (nfile.exists()) {
      file = nfile;
    } else {
      if (filePickerMode == nsIFilePicker.modeSave)
        file = nfile;
      else
        file = null;
    }
  }

  if (!file)
    return false;

  if (file.exists()) {
    var isDir = file.isDirectory();
    var isFile = file.isFile();
  } else { /* we are saving a new file */
    isDir = false;
    isFile = false;
  }

  switch(filePickerMode) {
  case nsIFilePicker.modeOpen:
    if (isFile) {
      retvals.directory = file.parent.path;
      ret = nsIFilePicker.returnOK;
    } else if (isDir) {
      if (!sfile.equals(file)) {
        gotoDirectory(file.path);
      }
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
  return true;
}

// node corresponds to treerow
function doConfirm(node) {
  var file = Components.classes[nsILocalFile_PROGID].createInstance(nsILocalFile);
  file.initWithPath(node.getAttribute("path"));

  if (file.isDirectory()) {
    gotoDirectory(file.path);
  }
  else if (file.isFile()) {
    /* what about symlinks? what if they symlink to a directory? */
    return doOKButton();
  }
}

function onClick(e) {
  if ( e.detail == 2 )
    doConfirm(e.target.parentNode);
}

function onKeyup(e) {
  if (directoryTree.selectedItems.length == 0 ) {
    directoryTree.selectItem(directoryTree.getItemAtIndex(0)); 
    return;
  }
  
  if (e.keyCode == 13) {
    doConfirm(e.target.selectedItems[0].firstChild);
  }
  else if (e.keyCode == 8)
    goUp();
}

function onSelect(e) {
  if (e.target.selectedItems.length != 1)
    return;
  var file = Components.classes[nsILocalFile_PROGID].createInstance(nsILocalFile);
  var path = e.target.selectedItems[0].firstChild.getAttribute("path");
  file.initWithPath(path);
  if (file.isFile()) {
    textInput = document.getElementById("textInput");
    textInput.value = file.leafName;
    lastClicked = file.leafName;
  }
}

function dirSort(e1, e2)
{
  if (e1.leafName == e2.leafName)
    return 0;

  if (e1.leafName > e2.leafName)
    return 1;

  if (e1.leafName < e2.leafName)
    return -1;
}

function createTree(parentElement, dirArray)
{
  var treeChildren = document.createElement("treechildren");
  treeChildren.setAttribute("flex", "1");

  var len = dirArray.length;
  var file;

  /* create the elements in the tree */
  for (var i=0; i < len; i++)
  {
    file = dirArray[i];

    var styleClass = "";

    var isSymlink = false;
    var isHidden = false;
    var isDirectory = false;
    var isFile = false;

    try {
      if (file.isSymlink()) {
        isSymlink = true;
      }
      if (file.isHidden()) {
        isHidden = true;
      }
      if (file.isDirectory()) {
        isDirectory = true;
      }
      if (file.isHidden()) {
        isHidden = true;
      }
      if (file.isFile()) {
        isFile = true;
      }
    } catch(ex) { dump("couldn't stat one of the files\n"); }

    /* treeItem */
    var treeItem = document.createElement("treeitem");
    /* set hidden on the tree item so that we use grey text for the entire row */
    if (isHidden)
      treeItem.setAttribute("class", "hidden");

    /* treeRow */
    var treeRow = document.createElement("treerow");
    treeRow.setAttribute("path", file.path);
    treeRow.setAttribute("onclick", "onClick(event)");

    /* treeCell -- name */
    var treeCell = document.createElement("treecell");
    /*    treeCell.setAttribute("indent", "true");*/
    treeCell.setAttribute("value", file.leafName);
    if (isDirectory)
      treeCell.setAttribute("class", "directory treecell-iconic");
    else if (isFile)
      treeCell.setAttribute("class", "file treecell-iconic");
    treeRow.appendChild(treeCell);

    /* treeCell -- size */
    treeCell = document.createElement("treecell");
    try {
      if (file.fileSize != 0) {
        treeCell.setAttribute("value", file.fileSize);
      }
    } catch(ex) { }
    treeRow.appendChild(treeCell);

    /* treeCell -- permissions */
    treeCell = document.createElement("treecell");
    try {
      const p = file.permissions;
      var perms = "";
      /*      dump(p + " "); */
      if (isSymlink) {
        perms += "lrwxrwxrwx";
      } else {
        perms += (isDirectory) ? "d" : "-";
        perms += (p & 00400) ? "r" : "-";
        perms += (p & 00200) ? "w" : "-";
        perms += (p & 00100) ? "x" : "-";
        perms += (p & 00040) ? "r" : "-";
        perms += (p & 00020) ? "w" : "-";
        perms += (p & 00010) ? "x" : "-";
        perms += (p & 00004) ? "r" : "-";
        perms += (p & 00002) ? "w" : "-";
        perms += (p & 00001) ? "x" : "-";
        /*        dump(perms + "\n"); */
      }
      treeCell.setAttribute("value", perms);
    } catch(ex) { }
    treeRow.appendChild(treeCell);

    /* append treeRow to treeItem */
    treeItem.appendChild(treeRow);

    /* append treeItem to treeChildren */
    treeChildren.appendChild(treeItem);
  }

  /* append treeChildren to parent (tree) */
  parentElement.appendChild(treeChildren);
}

function getDirectoryContents(parentElement, dirContents)
{
  /* split up the current filter since there might be more than one thing in it */
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
    
  var i = 0;
  var array = new Array();

  while (dirContents.hasMoreElements()) {
    var file = dirContents.getNext().QueryInterface(nsILocalFile);

    try {
      /* always add directories */
      if (file.isDirectory() || matchAllFiles)
        array[i++] = file;
      else {
        for (var k = 0; k < extensions.length; k++) {
          /* index where the match should take place */
          var matchIndex = file.leafName.length - extensions[k].length;

          if ((matchIndex >=0 ) &&
              file.leafName.lastIndexOf(extensions[k]) == matchIndex) {
            array[i++] = file;
            break;
          }
        }
      }
    } catch(ex) { }
  }

  if (array.length > 0) {
    /* sort the array */
    array.sort(dirSort);
    
    createTree(parentElement, array);
  }
}

function clearTree() {
  /* lets make an assumption that the tree children are at the end of the tree... */
  if (directoryTree.lastChild)
    directoryTree.removeChild(directoryTree.lastChild);
}


function addToHistory(directoryName) {
  /* The history list is not hooked up yet, so we'll just use
     a text element for the current directory */
/*
  var menuList = document.getElementById("lookInMenuList");
  var menu = document.getElementById("lookInMenu");
  var menuItem = document.createElement("menuitem");
  menuItem.setAttribute("value", directoryName);
  menu.appendChild(menuItem);

  menuList.selectedItem = menuItem; */

  var dirText = document.getElementById("curLocation");
  dirText.setAttribute("value", directoryName);
}

function goUp() {
  try {
    var parent = sfile.parent;
  } catch(ex) { dump("can't get parent directory\n"); }

  if (parent) {
    sfile = parent.QueryInterface(Components.interfaces.nsILocalFile);
    loadDirectory();
  }
}

function loadDirectory() {
  try {
    if (sfile.isDirectory()) {
      clearTree();
      try {
        getDirectoryContents(directoryTree, sfile.directoryEntries);
      } catch(ex) { dump("getDirectoryContents() failed\n"); }
      addToHistory(sfile.path);
      textInput = document.getElementById("textInput");
      if (lastClicked == textInput.value) {
        textInput.value = "";
      }
      lastClicked = "";
    }
  } catch(ex) { dump("isDirectory failed\n"); }
}

function gotoDirectory(directoryName) {
  sfile.initWithPath(directoryName);
  sfile.normalize();
  loadDirectory();
}
