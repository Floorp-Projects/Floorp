/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

const nsILocalFile        = Components.interfaces.nsILocalFile;
const nsILocalFile_PROGID = "component://mozilla/file/local";
const nsIFilePicker       = Components.interfaces.nsIFilePicker;

var sfile = Components.classes[nsILocalFile_PROGID].createInstance(nsILocalFile);
var retvals;
var filePickerMode;

function onLoad() {

  if (window.arguments) {
    var o = window.arguments[0];
    retvals = o.retvals; /* set this to a global var so we can set return values */
    const title = o.title;
    filePickerMode = o.mode;
    const directory = o.displayDirectory;
    const initialText = o.defaultString;
    const filterTitles = o.filters.titles;
    const filterTypes = o.filters.types;
    const numFilters = filterTitles.length;

    window.title = title;

    if (initialText) {
      textInput = document.getElementById("textInput");
      textInput.value = initialText;
    }
    /* build filter popup */
    var filterPopup = document.createElement("menupopup");

    for (var i = 0; i < numFilters; i++) {
      var menuItem = document.createElement("menuitem");
      menuItem.setAttribute("value", filterTitles[i] + " (" + filterTypes[i] + ")");
      filterPopup.appendChild(menuItem);
    }

    var filterMenuList = document.getElementById("filterMenuList");
    filterMenuList.appendChild(filterPopup);

  }

  // setup the dialogOverlay.xul button handlers
  doSetOKCancel(onOK, onCancel);

  if (directory) {
    sfile.initWithPath(directory);
  } else {
    sfile.initWithPath("/");
  }

  addToHistory(sfile.path);

  getDirectoryContents(document.getElementById("directoryList"), sfile.directoryEntries);
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
    var nfile = sfile.clone();
    nfile.append(file.path);
    dump(nfile.path);
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
    } else if (isDirectory) {
      if (!sfile.equals(file)) {
        gotoDirectory(file.path);
      }
      ret = nsIFilePicker.returnCancel;
    }
    break;
  case nsIFilePicker.modeSave:
    if (isFile) {
      // we need to pop up a dialog asking if you want to save
      rv = window.confirm(file.path + "already exists.  Do you want to replace it?");
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
  retvals.retval = nsIFilePicker.returnCancel;
  return true;
}

function onClick(e) {

  var file = Components.classes[nsILocalFile_PROGID].createInstance(nsILocalFile);
  file.initWithPath(e.target.parentNode.getAttribute("path"));

  textInput = document.getElementById("textInput");
  textInput.value = file.path;

  if (e.clickCount == 2) {
    if (file.isDirectory()) {
      gotoDirectory(file.path);
    }
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

    /* treeCell -- name */
    var treeCell = document.createElement("treecell");
    /*    treeCell.setAttribute("indent", "true");*/
    treeCell.setAttribute("value", file.leafName);
    if (isDirectory)
      treeCell.setAttribute("class", "directory");
    else if (isFile)
      treeCell.setAttribute("class", "file");
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
      dump(p + " ");
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
        dump(perms + "\n");
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
  var i = 0;
  var array = new Array();
  while (dirContents.HasMoreElements()) {
    array[i] = dirContents.GetNext().QueryInterface(nsILocalFile);
    i++;
  }

  /* sort the array */
  array.sort(dirSort);

  createTree(parentElement, array);
}

function clearTree() {
  var tree = document.getElementById("directoryList");

  /* lets make an assumption that the tree children are at the end of the tree... */
  if (tree.lastChild)
    tree.removeChild(tree.lastChild);
}


function addToHistory(directoryName) {
  var menuList = document.getElementById("lookInMenuList");
  var menu = document.getElementById("lookInMenu");
  var menuItem = document.createElement("menuitem");
  menuItem.setAttribute("value", directoryName);
  menu.appendChild(menuItem);

  menuList.selectedItem = menuItem;
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
        getDirectoryContents(document.getElementById("directoryList"), sfile.directoryEntries);
      } catch(ex) { dump("getDirectoryContents() failed\n"); }
      addToHistory(sfile.path);
      document.getElementById("textInput").value = "";
    }
  } catch(ex) { dump("isDirectory failed\n"); }
}

function gotoDirectory(directoryName) {
  sfile.initWithPath(directoryName);
  sfile.normalize();
  loadDirectory();
}



function textEntered(name) {
  var file = Components.classes[nsILocalFile_PROGID].createInstance(nsILocalFile);
  file.initWithPath(name);
  dump("*** " + file + "\n*** " + file.path + "\n");
  if (file.exists()) {
    if (file.isDirectory()) {
      if (!sfile.equals(file)) {
        gotoDirectory(name);
      }
      return;
    } else if (file.isFile()) {
      retvals.file = file;
      window.close();
    }
  } else {
    /* look for something in our current directory */
    var nfile = sfile.clone();
    nfile.append(file.path);
    dump(nfile.path);
    if (nfile.isFile()) {
      retvals.file = nfile;
      window.close();
    } else if (nfile.isDirectory()) {
      gotoDirectory(nfile.path);
    } else {
      dump("can't find file \"" + nfile.path + "\"");
    }
  }
}
