/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

const nsIFile             = Components.interfaces.nsIFile;
const nsILocalFile        = Components.interfaces.nsILocalFile;
const nsILocalFile_PROGID = "component://mozilla/file/local";

var sfile = Components.classes[nsILocalFile_PROGID].createInstance(nsILocalFile);
var retvals;

function onLoad() {

  if (window.arguments) {
    var o = window.arguments[0];
    retvals = o.retvals; /* set this to a global var so we can set return values */
    var title = o.title;
    var mode = o.mode;
    var numFilters = o.filters.length;
    var filterTitles = o.filters.titles;
    var filterTypes = o.filters.types;

    window.title = title;

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

  sfile.initWithPath("/");
  getDirectoryContents(document.getElementById("directoryList"), sfile.directoryEntries);
}

function onOK()
{
  textInput = document.getElementById("textInput");

  var file = Components.classes[nsILocalFile_PROGID].createInstance(nsILocalFile);
  file.initWithPath(textInput.value);

  if (file.isFile() && !file.isDirectory()) {
    retvals.file = file.QueryInterface(nsIFile);
    return true;
  }

  return false;
}

function onCancel()
{
  // Close the window.
  return true;
}



function onClick(e) {

  sfile.initWithPath(e.target.parentNode.getAttribute("path"));

  textInput = document.getElementById("textInput");
  textInput.value = sfile.path;

  if (e.clickCount == 2) {

    if (sfile.isDirectory()) {
      loadDirectory();
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
    array[i] = dirContents.GetNext().QueryInterface(nsIFile);
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
  loadDirectory();
}



function textEntered(name) {
  var file = Components.classes[nsILocalFile_PROGID].createInstance(nsILocalFile);
  file.initWithPath(name);
  dump("*** " + file + "\n*** " + file.path + "\n");
  if (file.exists()) {
    if (file.isDirectory()) {
      if (!sfile.equals(file)) {
        sfile.initWithPath(name);
        sfile.normalize();
        loadDirectory();
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
      sfile.initWithPath(nfile.path);
      sfile.normalize();
      loadDirectory();
    } else {
      dump("can't find file \"" + nfile.path + "\"");
    }
  }
}
