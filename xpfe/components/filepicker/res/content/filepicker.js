/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *  Johann Petrak <johann@ai.univie.ac.at>
 *  Akkana Peck <akkana@netscape.com>
 */

const nsIFilePicker       = Components.interfaces.nsIFilePicker;
const nsIDirectoryServiceProvider = Components.interfaces.nsIDirectoryServiceProvider;
const nsIDirectoryServiceProvider_CONTRACTID = "@mozilla.org/file/directory_service;1";
const nsITreeBoxObject = Components.interfaces.nsITreeBoxObject;
const nsIFileView = Components.interfaces.nsIFileView;
const nsFileView_CONTRACTID = "@mozilla.org/filepicker/fileview;1";
const nsITreeView = Components.interfaces.nsITreeView;
const nsILocalFile = Components.interfaces.nsILocalFile;
const nsLocalFile_CONTRACTID = "@mozilla.org/file/local;1";
const nsIPromptService_CONTRACTID = "@mozilla.org/embedcomp/prompt-service;1";

var sfile = Components.classes[nsLocalFile_CONTRACTID].createInstance(nsILocalFile);
var retvals;
var filePickerMode;
var homeDir;
var treeView;

var textInput;
var okButton;

var gFilePickerBundle;

// name of new directory entered by the user to be remembered
// for next call of newDir() in case something goes wrong with creation
var gNewDirName = { value: "" };

function filepickerLoad() {
  gFilePickerBundle = document.getElementById("bundle_filepicker");

  textInput = document.getElementById("textInput");
  okButton = document.documentElement.getButton("accept");
  treeView = Components.classes[nsFileView_CONTRACTID].createInstance(nsIFileView);

  if (window.arguments) {
    var o = window.arguments[0];
    retvals = o.retvals; /* set this to a global var so we can set return values */
    const title = o.title;
    filePickerMode = o.mode;
    if (o.displayDirectory) {
      const directory = o.displayDirectory.path;
    }
    const initialText = o.defaultString;
    const filterTitles = o.filters.titles;
    const filterTypes = o.filters.types;
    const numFilters = filterTitles.length;

    window.title = title;

    if (initialText) {
      textInput.value = initialText;
    }
  }

  if (filePickerMode != nsIFilePicker.modeOpen && filePickerMode != nsIFilePicker.modeOpenMultiple) {
    var newDirButton = document.getElementById("newDirButton");
    newDirButton.removeAttribute("hidden");
  }

  if (filePickerMode == nsIFilePicker.modeGetFolder) {
    var textInputLabel = document.getElementById("textInputLabel");
    textInputLabel.value = gFilePickerBundle.getString("dirTextInputLabel");
  }
  
  if ((filePickerMode == nsIFilePicker.modeOpen) ||
      (filePickerMode == nsIFilePicker.modeOpenMultiple) ||
      (filePickerMode == nsIFilePicker.modeSave)) {

    treeView.setFilter(filterTypes[0]);

    /* build filter popup */
    var filterPopup = document.createElement("menupopup");

    for (var i = 0; i < numFilters; i++) {
      var menuItem = document.createElement("menuitem");
      if (filterTypes[i] == "..apps")
        menuItem.setAttribute("label", filterTitles[i]);
      else
        menuItem.setAttribute("label", filterTitles[i] + " (" + filterTypes[i] + ")");
      menuItem.setAttribute("filters", filterTypes[i]);
      filterPopup.appendChild(menuItem);
    }

    var filterMenuList = document.getElementById("filterMenuList");
    filterMenuList.appendChild(filterPopup);
    if (numFilters > 0)
      filterMenuList.selectedIndex = 0;
    var filterBox = document.getElementById("filterBox");
    filterBox.removeAttribute("hidden");

    filterMenuList.selectedIndex = o.filterIndex;
  } else if (filePickerMode == nsIFilePicker.modeGetFolder) {
    treeView.showOnlyDirectories = true;
  }

  // start out with a filename sort
  handleColumnClick("FilenameColumn");

  document.documentElement.setAttribute("ondialogcancel", "return onCancel();");
  try {
    var buttonLabel = getOKAction();
    okButton.setAttribute("label", buttonLabel);
  } catch (exception) {
    // keep it set to "OK"
  }

  // setup the dialogOverlay.xul button handlers
  retvals.buttonStatus = nsIFilePicker.returnCancel;

  var tree = document.getElementById("directoryTree");
  tree.treeBoxObject.view = treeView;

  // Start out with the ok button disabled since nothing will be
  // selected and nothing will be in the text field.
  okButton.disabled = true;
  textInput.focus();

  // This allows the window to show onscreen before we begin
  // loading the file list

  setTimeout(setInitialDirectory, 0, directory);
}

function setInitialDirectory(directory)
{
  // get the home dir
  var dirServiceProvider = Components.classes[nsIDirectoryServiceProvider_CONTRACTID]
                                     .getService(nsIDirectoryServiceProvider);
  var persistent = new Object();
  homeDir = dirServiceProvider.getFile("Home", persistent);

  if (directory) {
    sfile.initWithPath(directory);
  }
  if (!directory || !(sfile.exists() && sfile.isDirectory())) {
    // Start in the user's home directory
    sfile.initWithPath(homeDir.path);
  }

  gotoDirectory(sfile);
}

function onFilterChanged(target)
{
  // Do this on a timeout callback so the filter list can roll up
  // and we don't keep the mouse grabbed while we are refiltering.

  setTimeout(changeFilter, 0, target.getAttribute("filters"));
}

function changeFilter(filterTypes)
{
  window.setCursor("wait");
  treeView.setFilter(filterTypes);
  window.setCursor("auto");
}

function showErrorDialog(titleStrName, messageStrName, file)
{
  var errorTitle =
    gFilePickerBundle.getFormattedString(titleStrName, [file.path]);
  var errorMessage =
    gFilePickerBundle.getFormattedString(messageStrName, [file.path]);
  var promptService =
    Components.classes[nsIPromptService_CONTRACTID].getService(Components.interfaces.nsIPromptService);

  promptService.alert(window, errorTitle, errorMessage);
}

function openOnOK()
{
  var dir = treeView.getSelectedFile();
  if (!dir.isReadable()) {
    showErrorDialog("errorOpenFileDoesntExistTitle",
                    "errorDirNotReadableMessage",
                    dir);
    return false;
  }

  if (dir)
    gotoDirectory(dir);

  retvals.file = dir;

  retvals.buttonStatus = nsIFilePicker.returnCancel;
  
  var filterMenuList = document.getElementById("filterMenuList");
  retvals.filterIndex = filterMenuList.selectedIndex;
  
  return false;
}

function selectOnOK()
{
  var errorTitle, errorMessage, promptService;
  var ret = nsIFilePicker.returnCancel;

  var isDir = false;
  var isFile = false;

  var file = processPath(textInput.value);

  if (!file) { // generic error message, should probably never happen
    showErrorDialog("errorPathProblemTitle",
                    "errorPathProblemMessage",
                    textInput.value);
    return false;
  }

  // try to normalize - if this fails we will ignore the error
  // because we will notice the
  // error later and show a fitting error alert.
  try{
    file.normalize();
  } catch(e) {
    //promptService.alert(window, "Problem", "normalize failed, continuing");
  }

  var fileExists = file.exists();
  
  if (!fileExists && (filePickerMode == nsIFilePicker.modeOpen ||
                      filePickerMode == nsIFilePicker.modeOpenMultiple)) {
    showErrorDialog("errorOpenFileDoesntExistTitle",
                    "errorOpenFileDoesntExistMessage",
                    file);
    return false;
  }

  if (!fileExists && filePickerMode == nsIFilePicker.modeGetFolder) {
    showErrorDialog("errorDirDoesntExistTitle",
                    "errorDirDoesntExistMessage",
                    file);
    return false;
  }

  if (fileExists) {
    isDir = file.isDirectory();
    isFile = file.isFile();
  }

  switch(filePickerMode) {
  case nsIFilePicker.modeOpen:
  case nsIFilePicker.modeOpenMultiple:
    if (isFile) {
      if (file.isReadable()) {
        retvals.directory = file.parent.path;
        ret = nsIFilePicker.returnOK;
      } else {
        showErrorDialog("errorOpeningFileTitle",
                        "openWithoutPermissionMessage_file",
                        file);
        ret = nsIFilePicker.returnCancel;
      }
    } else if (isDir) {
      if (!sfile.equals(file)) {
        gotoDirectory(file);
      }
      textInput.value = "";
      doEnabling();
      ret = nsIFilePicker.returnCancel;
    }
    break;
  case nsIFilePicker.modeSave:
    if (isFile) { // can only be true if file.exists()
      if (!file.isWritable()) {
        showErrorDialog("errorSavingFileTitle",
                        "saveWithoutPermissionMessage_file",
                        file);
        ret = nsIFilePicker.returnCancel;
      } else {
        // we need to pop up a dialog asking if you want to save
        var confirmTitle = gFilePickerBundle.getString("confirmTitle");
        var message =
          gFilePickerBundle.getFormattedString("confirmFileReplacing",
                                               [file.path]);

        promptService = Components.classes[nsIPromptService_CONTRACTID].getService(Components.interfaces.nsIPromptService);
        var rv = promptService.confirm(window, title, message)
        if (rv) {
          ret = nsIFilePicker.returnReplace;
          retvals.directory = file.parent.path;
        } else {
          ret = nsIFilePicker.returnCancel;
        }
      }
    } else if (isDir) {
      if (!sfile.equals(file)) {
        gotoDirectory(file);
      }
      textInput.value = "";
      doEnabling();
      ret = nsIFilePicker.returnCancel;
    } else {
      var parent = file.parent;
      if (parent.exists() && parent.isDirectory() && parent.isWritable()) {
        ret = nsIFilePicker.returnOK;
        retvals.directory = parent.path;
      } else {
        var oldParent = parent;
        while (!parent.exists()) {
          oldParent = parent;
          parent = parent.parent;
        }
        errorTitle =
          gFilePickerBundle.getFormattedString("errorSavingFileTitle",
                                               [file.path]);
        if (parent.isFile()) {
          errorMessage =
            gFilePickerBundle.getFormattedString("saveParentIsFileMessage",
                                                 [parent.path, file.path]);
        } else {
          errorMessage =
            gFilePickerBundle.getFormattedString("saveParentDoesntExistMessage",
                                                 [oldParent.path, file.path]);
        }
        if (!parent.isWritable()) {
          errorMessage =
            gFilePickerBundle.getFormattedString("saveWithoutPermissionMessage_dir", [parent.path]);
        }
        promptService = Components.classes[nsIPromptService_CONTRACTID].getService(Components.interfaces.nsIPromptService);
        promptService.alert(window, errorTitle, errorMessage);
        ret = nsIFilePicker.returnCancel;
      }
    }
    break;
  case nsIFilePicker.modeGetFolder:
    if (isDir) {
      retvals.directory = file.parent.path;
    } else { // if nothing selected, the current directory will be fine
      retvals.directory = sfile.path;
    }
    ret = nsIFilePicker.returnOK;
    break;
  }

  retvals.file = file;

  gFilesEnumerator.mFile = file;
  gFilesEnumerator.mHasMore = true;

  retvals.files = gFilesEnumerator;
  retvals.buttonStatus = ret;

  var filterMenuList = document.getElementById("filterMenuList");
  retvals.filterIndex = filterMenuList.selectedIndex;
  
  return (ret != nsIFilePicker.returnCancel);
}

var gFilesEnumerator = {
  mHasMore: false,
  mFile: null,

  hasMoreElements: function()
  {
    return this.mHasMore;
  },
  getNext: function()
  {
    this.mHasMore = false;
    return this.mFile;
  }
};

function onCancel()
{
  // Close the window.
  retvals.buttonStatus = nsIFilePicker.returnCancel;
  retvals.file = null;
  retvals.files = null;
  return true;
}

function onDblClick(e) {
  var t = e.originalTarget;
  if (t.localName != "treechildren")
    return;

  openSelectedFile();
}

function openSelectedFile() {
  var file = treeView.getSelectedFile();
  if (!file)
    return;

  if (file.isDirectory())
    gotoDirectory(file);
  else if (file.isFile())
    document.documentElement.acceptDialog();
}

function onClick(e) {
  var t = e.originalTarget;
  if (t.localName == "treecol")
    handleColumnClick(t.id);
}

function convertColumnIDtoSortType(columnID) {
  var sortKey;
  
  switch (columnID) {
  case "FilenameColumn":
    sortKey = nsIFileView.sortName;
    break;
  case "FileSizeColumn":
    sortKey = nsIFileView.sortSize;
    break;
  case "LastModifiedColumn":
    sortKey = nsIFileView.sortDate;
    break;
  default:
    dump("unsupported sort column: " + columnID + "\n");
    sortKey = 0;
    break;
  }
  
  return sortKey;
}

function handleColumnClick(columnID) {
  var sortType = convertColumnIDtoSortType(columnID);
  var sortOrder = (treeView.sortType == sortType) ? !treeView.reverseSort : false;
  treeView.sort(sortType, sortOrder);
  
  // set the sort indicator on the column we are sorted by
  var sortedColumn = document.getElementById(columnID);
  if (treeView.reverseSort) {
    sortedColumn.setAttribute("sortDirection", "descending");
  } else {
    sortedColumn.setAttribute("sortDirection", "ascending");
  }
  
  // remove the sort indicator from the rest of the columns
  var currCol = sortedColumn.parentNode.firstChild;
  while (currCol) {
    if (currCol != sortedColumn && currCol.localName == "treecol")
      currCol.removeAttribute("sortDirection");
    currCol = currCol.nextSibling;
  }
}

function onKeypress(e) {
  if (e.keyCode == 8) /* backspace */
    goUp();
  else if (e.keyCode == 13) { /* enter */
    var file = treeView.getSelectedFile();
    if (file) {
      if (file.isDirectory()) {
        gotoDirectory(file);
        e.preventDefault();
      }
    }
  }
}

function doEnabling() {
  // Maybe add check if textInput.value would resolve to an existing
  // file or directory in .modeOpen. Too costly I think.
  var enable = (textInput.value != "");

  okButton.disabled = !enable;
}

function onTreeFocus(event) {
  // Reset the button label and enabled/disabled state.
  onFileSelected(treeView.getSelectedFile());
}

function getOKAction(file) {
  var buttonLabel;

  if (file && file.isDirectory() && filePickerMode != nsIFilePicker.modeGetFolder) {
    document.documentElement.setAttribute("ondialogaccept", "return openOnOK();");
    buttonLabel = gFilePickerBundle.getString("openButtonLabel");
  }
  else {
    document.documentElement.setAttribute("ondialogaccept", "return selectOnOK();");
    switch(filePickerMode) {
    case nsIFilePicker.modeGetFolder:
      buttonLabel = gFilePickerBundle.getString("selectFolderButtonLabel");
      break;
    case nsIFilePicker.modeOpen:
    case nsIFilePicker.modeOpenMultiple:
      buttonLabel = gFilePickerBundle.getString("openButtonLabel");
      break;
    case nsIFilePicker.modeSave:
      buttonLabel = gFilePickerBundle.getString("saveButtonLabel");
      break;
    }
  }

  return buttonLabel;
}

function onSelect(event) {
  onFileSelected(treeView.getSelectedFile());
}

function onFileSelected(file) {
  if (file) {
    var path = file.leafName;
    
    if (path) {
      if ((filePickerMode == nsIFilePicker.modeGetFolder) || !file.isDirectory())
        textInput.value = path;
      
      var buttonLabel = getOKAction(file);
      okButton.setAttribute("label", buttonLabel);
      okButton.disabled = false;
      return;
    }
  }
  okButton.disabled = (textInput.value == "");
}

function onTextFieldFocus() {
  var buttonLabel = getOKAction(null);
  okButton.setAttribute("label", buttonLabel);
  doEnabling();
}

function onDirectoryChanged(target)
{
  var path = target.getAttribute("label");

  var file = Components.classes[nsLocalFile_CONTRACTID].createInstance(nsILocalFile);
  file.initWithPath(path);

  if (!sfile.equals(file)) {
    // Do this on a timeout callback so the directory list can roll up
    // and we don't keep the mouse grabbed while we are loading.

    setTimeout(gotoDirectory, 0, file);
  }
}

function populateAncestorList(directory) {
  var menu = document.getElementById("lookInMenu");

  while (menu.hasChildNodes()) {
    menu.removeChild(menu.firstChild);
  }
  
  var menuItem = document.createElement("menuitem");
  menuItem.setAttribute("label", directory.path);
  menuItem.setAttribute("crop", "start");
  menu.appendChild(menuItem);

  // .parent is _sometimes_ null, see bug 121489.  Do a dance around that.
  var parent = directory.parent;
  while (parent && !parent.equals(directory)) {
    menuItem = document.createElement("menuitem");
    menuItem.setAttribute("label", parent.path);
    menuItem.setAttribute("crop", "start");
    menu.appendChild(menuItem);
    directory = parent;
    parent = directory.parent;
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

function goHome() {
  gotoDirectory(homeDir);
}

function newDir() {
  var file;
  var promptService =
    Components.classes[nsIPromptService_CONTRACTID].getService(Components.interfaces.nsIPromptService);
  var dialogTitle =
    gFilePickerBundle.getString("promptNewDirTitle");
  var dialogMsg =
    gFilePickerBundle.getString("promptNewDirMessage");
  var ret = promptService.prompt(window, dialogTitle, dialogMsg, gNewDirName, null, {value:0});

  if (ret) {
    file = processPath(gNewDirName.value);
    if (!file) {
      showErrorDialog("errorCreateNewDirTitle",
                      "errorCreateNewDirMessage",
                      file);
      return false;
    }
    
    if (file.exists()) {
      showErrorDialog("errorNewDirDoesExistTitle",
                      "errorNewDirDoesExistMessage",
                      file);
      return false;
    }

    var parent = file.parent;
    if (!(parent.exists() && parent.isDirectory() && parent.isWritable())) {
      var oldParent = parent;
      while (!parent.exists()) {
        oldParent = parent;
        parent = parent.parent;
      }
      if (parent.isFile()) {
        showErrorDialog("errorCreateNewDirTitle",
                        "errorCreateNewDirIsFileMessage",
                        parent);
        return false;
      }
      if (!parent.isWritable()) {
        showErrorDialog("errorCreateNewDirTitle",
                        "errorCreateNewDirPermissionMessage",
                        parent);
        return false;
      }
    }

    try {
      file.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755); 
    } catch (e) {
      showErrorDialog("errorCreateNewDirTitle",
                      "errorCreateNewDirMessage",
                      file);
      return false;
    }
    file.normalize(); // ... in case ".." was used in the path
    gotoDirectory(file);
    // we remember and reshow a dirname if something goes wrong
    // so that errors can be corrected more easily. If all went well,
    // reset the default value to blank
    gNewDirName = { value: "" }; 
  }
  return true;
}

function gotoDirectory(directory) {
  window.setCursor("wait");
  try {
    populateAncestorList(directory);
    treeView.setDirectory(directory);
    document.getElementById("errorShower").selectedIndex = 0;
  } catch(ex) {
    document.getElementById("errorShower").selectedIndex = 1;
  }

  window.setCursor("auto");

  treeView.QueryInterface(nsITreeView).selection.clearSelection();
  if (filePickerMode == nsIFilePicker.modeGetFolder) {
    textInput.value = "";
  }
  textInput.focus();
  sfile = directory;
}

function toggleShowHidden(event) {
  treeView.showHiddenFiles = !treeView.showHiddenFiles;
}

// from the current directory and whatever was entered
// in the entry field, try to make a new path. This
// uses "/" as the directory seperator, "~" as a shortcut
// for the home directory (but only when seen at the start
// of a path), and ".." to denote the parent directory.
// returns the path or false if an error occurred.
function processPath(path)
{
  var file;
  if (path[0] == '~') 
    path  = homeDir.path + path.substring(1);

  try{
    file = sfile.clone().QueryInterface(nsILocalFile);
  } catch(e) {
    dump("Couldn't clone\n"+e);
    return false;
  }

  if (path[0] == '/')   /* an absolute path was entered */
    file.initWithPath(path);
  else if ((path.indexOf("/../") > 0) ||
           (path.substr(-3) == "/..") ||
           (path.substr(0,3) == "../") ||
           (path == "..")) {
    /* appendRelativePath doesn't allow .. */
    try{
      file.initWithPath(file.path + "/" + path);
    } catch (e) {
      dump("Couldn't init path\n"+e);
      return false;
    }
  }
  else {
    try {
      file.appendRelativePath(path);
    } catch (e) {
      dump("Couldn't append path\n"+e);
      return false;
    }
  }
  return file;
}
