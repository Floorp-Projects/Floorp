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
 */

const nsIFilePicker       = Components.interfaces.nsIFilePicker;
const nsIDirectoryServiceProvider = Components.interfaces.nsIDirectoryServiceProvider;
const nsIDirectoryServiceProvider_CONTRACTID = "@mozilla.org/file/directory_service;1";
const nsIOutlinerBoxObject = Components.interfaces.nsIOutlinerBoxObject;

var sfile = Components.classes[nsLocalFile_CONTRACTID].createInstance(nsILocalFile);
var retvals;
var filePickerMode;
var homeDir;
var outlinerView;

var textInput;
var okButton;

var gFilePickerBundle;

function filepickerLoad() {
  gFilePickerBundle = document.getElementById("bundle_filepicker");

  textInput = document.getElementById("textInput");
  okButton = document.getElementById("ok");
  outlinerView = new nsFileView();
  outlinerView.selectionCallback = onSelect;

  if (window.arguments) {
    var o = window.arguments[0];
    retvals = o.retvals; /* set this to a global var so we can set return values */
    const title = o.title;
    filePickerMode = o.mode;
    if (o.displayDirectory) {
      const directory = o.displayDirectory.unicodePath;
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

  if ((filePickerMode == nsIFilePicker.modeOpen) ||
      (filePickerMode == nsIFilePicker.modeSave)) {

    outlinerView.setFilter(filterTypes[0]);

    /* build filter popup */
    var filterPopup = document.createElement("menupopup");

    for (var i = 0; i < numFilters; i++) {
      var menuItem = document.createElement("menuitem");
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
  } else if (filePickerMode == nsIFilePicker.modeGetFolder) {
    outlinerView.showOnlyDirectories = true;
  }

  // start out with a filename sort
  handleColumnClick("FilenameColumn");

  try {
    var buttonLabel = getOKAction();
    okButton.setAttribute("label", buttonLabel);
  } catch (exception) {
    // keep it set to "OK"
  }

  // setup the dialogOverlay.xul button handlers
  doSetOKCancel(selectOnOK, onCancel);
  retvals.buttonStatus = nsIFilePicker.returnCancel;

  var outliner = document.getElementById("directoryOutliner");
  outliner.outlinerBoxObject.view = outlinerView;

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
    sfile.initWithUnicodePath(directory);
  }
  if (!directory || !(sfile.exists() && sfile.isDirectory())) {
    // Start in the user's home directory
    sfile.initWithUnicodePath(homeDir.unicodePath);
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
  outlinerView.setFilter(filterTypes);
  window.setCursor("auto");
}

function openOnOK()
{
  var dir = outlinerView.getSelectedFile();
  if (dir)
    gotoDirectory(dir);
  retvals.file = dir;
  retvals.buttonStatus = nsIFilePicker.returnCancel;
  return false;
}

function selectOnOK()
{
  var errorTitle, errorMessage, promptService;
  var ret = nsIFilePicker.returnCancel;

  var isDir = false;
  var isFile = false;

  var input = textInput.value;
  if (input[0] == '~') // XXX XP?
    input  = homeDir.unicodePath + input.substring(1);

  var file = sfile.clone().QueryInterface(nsILocalFile);
  if (!file)
    return false;

  /* XXX we need an XP way to test for an absolute path! */
  if (input[0] == '/')   /* an absolute path was entered */
    file.initWithUnicodePath(input);
  else if ((input.indexOf("/../") > 0) ||
           (input.substr(-3) == "/..") ||
           (input.substr(0,3) == "../") ||
           (input == "..")) {
    /* appendRelativePath doesn't allow .. */
    file.initWithUnicodePath(file.unicodePath + "/" + input);
    file.normalize();
  }
  else {
    try {
      file.appendRelativeUnicodePath(input);
    } catch (e) {
      dump("Can't append relative path '"+input+"':\n");
      return false;
    }
  }

  if (!file.exists() && (filePickerMode != nsIFilePicker.modeSave)) {
    errorTitle = gFilePickerBundle.getFormattedString("errorOpenFileDoesntExistTitle",
                                                      [file.unicodePath]);
    errorMessage = gFilePickerBundle.getFormattedString("errorOpenFileDoesntExistMessage",
                                                        [file.unicodePath]);
    promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                              .getService(Components.interfaces.nsIPromptService);
    promptService.alert(window, errorTitle, errorMessage);
    return false;
  }

  if (file.exists()) {
    isDir = file.isDirectory();
    isFile = file.isFile();
  }

  switch(filePickerMode) {
  case nsIFilePicker.modeOpen:
    if (isFile) {
      retvals.directory = file.parent.unicodePath;
      ret = nsIFilePicker.returnOK;
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
      // we need to pop up a dialog asking if you want to save
      var message = gFilePickerBundle.getFormattedString("confirmFileReplacing",
                                                         [file.unicodePath]);
      var rv = window.confirm(message);
      if (rv) {
        ret = nsIFilePicker.returnReplace;
        retvals.directory = file.parent.unicodePath;
      } else {
        ret = nsIFilePicker.returnCancel;
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
      if (parent.exists() && parent.isDirectory()) {
        ret = nsIFilePicker.returnOK;
        retvals.directory = parent.unicodePath;
      } else {
        var oldParent = parent;
        while (!parent.exists()) {
          oldParent = parent;
          parent = parent.parent;
        }
        errorTitle = gFilePickerBundle.getFormattedString("errorSavingFileTitle",
                                                          [file.unicodePath]);
        if (parent.isFile()) {
          errorMessage = gFilePickerBundle.getFormattedString("saveParentIsFileMessage",
                                                              [parent.unicodePath, file.unicodePath]);
        } else {
          errorMessage = gFilePickerBundle.getFormattedString("saveParentDoesntExistMessage",
                                                              [oldParent.unicodePath, file.unicodePath]);
        }
        promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                  .getService(Components.interfaces.nsIPromptService);
        promptService.alert(window, errorTitle, errorMessage);
        ret = nsIFilePicker.returnCancel;
      }
    }
    break;
  case nsIFilePicker.modeGetFolder:
    if (isDir) {
      retvals.directory = file.parent.unicodePath;
    } else { // if nothing selected, the current directory will be fine
      retvals.directory = sfile.unicodePath;
    }
    ret = nsIFilePicker.returnOK;
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

function onDblClick(e) {
  var t = e.originalTarget;
  if (t.localName != "outlinerbody")
    return;

  openSelectedFile();
}

function openSelectedFile() {
  var file = outlinerView.getSelectedFile();
  if (!file)
    return;

  if (file.isDirectory())
    gotoDirectory(file);
  else if (file.isFile())
    doOKButton();
}

function onClick(e) {
  var t = e.originalTarget;
  if (t.localName == "outlinercol")
    handleColumnClick(t.id);
}

function convertColumnIDtoSortType(columnID) {
  var sortKey;
  
  switch (columnID) {
  case "FilenameColumn":
    sortKey = nsFileView.SORTTYPE_NAME;
    break;
  case "FileSizeColumn":
    sortKey = nsFileView.SORTTYPE_SIZE;
    break;
  case "LastModifiedColumn":
    sortKey = nsFileView.SORTTYPE_DATE;
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
  var sortOrder = (outlinerView.sortType == sortType) ? !outlinerView.reverseSort : false;
  outlinerView.sort(sortType, sortOrder, false);
  
  // set the sort indicator on the column we are sorted by
  var sortedColumn = document.getElementById(columnID);
  if (outlinerView.reverseSort) {
    sortedColumn.setAttribute("sortDirection", "descending");
  } else {
    sortedColumn.setAttribute("sortDirection", "ascending");
  }
  
  // remove the sort indicator from the rest of the columns
  var currCol = document.getElementById("directoryOutliner").firstChild;
  while (currCol) {
    while (currCol && currCol.localName != "outlinercol")
      currCol = currCol.nextSibling;
    if (currCol) {
      if (currCol != sortedColumn) {
        currCol.removeAttribute("sortDirection");
      }
      currCol = currCol.nextSibling;
    }
  }
}

function doSort(sortType) {
  outlinerView.sort(sortType, false);
}

function onKeypress(e) {
  if (e.keyCode == 8) /* backspace */
    goUp();
  else if (e.keyCode == 13) { /* enter */
    var file = outlinerView.getSelectedFile();
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

function onOutlinerFocus(event) {
  // Reset the button label and enabled/disabled state.
  onSelect(outlinerView.getSelectedFile());
}

function getOKAction(file) {
  var buttonLabel;

  if (file && file.isDirectory() && filePickerMode != nsIFilePicker.modeGetFolder) {
    doSetOKCancel(openOnOK, onCancel);
    buttonLabel = gFilePickerBundle.getString("openButtonLabel");
  }
  else {
    doSetOKCancel(selectOnOK, onCancel);
    switch(filePickerMode) {
    case nsIFilePicker.modeGetFolder:
      buttonLabel = gFilePickerBundle.getString("selectFolderButtonLabel");
      break;
    case nsIFilePicker.modeOpen:
      buttonLabel = gFilePickerBundle.getString("openButtonLabel");
      break;
    case nsIFilePicker.modeSave:
      buttonLabel = gFilePickerBundle.getString("saveButtonLabel");
      break;
    }
  }

  return buttonLabel;
}

function onSelect(file) {
  if (file) {
    var path = file.unicodeLeafName;
    
    if (path) {
      if ((filePickerMode == nsIFilePicker.modeGetFolder) || !file.isDirectory())
        textInput.value = path;
      
      var buttonLabel = getOKAction(file);
      okButton.setAttribute("label", buttonLabel);
      okButton.disabled = false;
      return;
    }
  }

  okButton.disabled = true;
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
  file.initWithUnicodePath(path);

  if (!sfile.equals(file)) {
    // Do this on a timeout callback so the directory list can roll up
    // and we don't keep the mouse grabbed while we are loading.

    setTimeout(gotoDirectory, 0, file);
  }
}

function addToHistory(directoryName) {
  var menu = document.getElementById("lookInMenu");
  var children = menu.childNodes;

  var i = 0;
  while (i < children.length) {
    if (children[i].getAttribute("label") == directoryName)
      break;

    ++i;
  }

  if (i < children.length) {
    if (i != 0) {
      var node = children[i];
      menu.removeChild(node);
      menu.insertBefore(node, children[0]);
    }
  } else {
    var menuItem = document.createElement("menuitem");
    menuItem.setAttribute("label", directoryName);
    menu.insertBefore(menuItem, children[0]);
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
  addToHistory(directory.unicodePath);

  window.setCursor("wait");
  outlinerView.setDirectory(directory.unicodePath);
  window.setCursor("auto");

  outlinerView.selection.clearSelection();
  textInput.focus();
  sfile = directory;
}

