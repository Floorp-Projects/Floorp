/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Pierre Chanial <chanial@noos.fr>.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var NC_NS, WEB_NS, RDF_NS, XUL_NS, NC_NS_CMD;

// definition of the services frequently used for bookmarks
var kRDFContractID;
var kRDFSVCIID;
var kRDFRSCIID;
var kRDFLITIID;
var RDF;

var kRDFCContractID;
var kRDFCIID;
var RDFC;

var kRDFCUContractID;
var kRDFCUIID;
var RDFCU;

var BMDS;
var kBMSVCIID;
var BMSVC;

var kPREFContractID;
var kPREFIID;
var PREF;

var kSOUNDContractID;
var kSOUNDIID;
var SOUND;

var kWINDOWContractID;
var kWINDOWIID;
var WINDOWSVC;

var kDSContractID;
var kDSIID;
var DS;

// should be moved in a separate file
function initServices()
{
  NC_NS     = "http://home.netscape.com/NC-rdf#";
  WEB_NS    = "http://home.netscape.com/WEB-rdf#";
  RDF_NS    = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
  XUL_NS    = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  NC_NS_CMD = NC_NS + "command?cmd=";

  kRDFContractID   = "@mozilla.org/rdf/rdf-service;1";
  kRDFSVCIID       = Components.interfaces.nsIRDFService;
  kRDFRSCIID       = Components.interfaces.nsIRDFResource;
  kRDFLITIID       = Components.interfaces.nsIRDFLiteral;
  RDF              = Components.classes[kRDFContractID].getService(kRDFSVCIID);

  kRDFCContractID  = "@mozilla.org/rdf/container;1";
  kRDFCIID         = Components.interfaces.nsIRDFContainer;
  RDFC             = Components.classes[kRDFCContractID].createInstance(kRDFCIID);

  kRDFCUContractID = "@mozilla.org/rdf/container-utils;1";
  kRDFCUIID        = Components.interfaces.nsIRDFContainerUtils;
  RDFCU            = Components.classes[kRDFCUContractID].getService(kRDFCUIID);

  kPREFContractID  = "@mozilla.org/preferences-service;1";
  kPREFIID         = Components.interfaces.nsIPrefService;
  try {
    PREF             = Components.classes[kPREFContractID].getService(kPREFIID)
                                 .getBranch(null);
  } catch (e) {}

  kSOUNDContractID = "@mozilla.org/sound;1";
  kSOUNDIID        = Components.interfaces.nsISound;
  try {
    SOUND            = Components.classes[kSOUNDContractID].createInstance(kSOUNDIID);
  } catch (e) {}

  kWINDOWContractID = "@mozilla.org/appshell/window-mediator;1";
  kWINDOWIID        = Components.interfaces.nsIWindowMediator;
  try {
    WINDOWSVC         = Components.classes[kWINDOWContractID].getService(kWINDOWIID);
  } catch (e) {}

  kDSContractID     = "@mozilla.org/widget/dragservice;1";
  kDSIID            = Components.interfaces.nsIDragService;
  try {
    DS                = Components.classes[kDSContractID].getService(kDSIID);
  } catch (e) {}
}

function initBMService()
{
  kBMSVCIID = Components.interfaces.nsIBookmarksService;
  BMDS  = RDF.GetDataSource("rdf:bookmarks");
  BMSVC = BMDS.QueryInterface(kBMSVCIID);
  BookmarkTransaction.prototype.RDFC = RDFC;
  BookmarkTransaction.prototype.BMDS = BMDS;
}

/**
 * XXX - 04/16/01
 *  ACK! massive command name collision problems are causing big issues
 *  in getting this stuff to work in the Navigator window. For sanity's 
 *  sake, we need to rename all the commands to be of the form cmd_bm_*
 *  otherwise there'll continue to be problems. For now, we're just 
 *  renaming those that affect the personal toolbar (edit operations,
 *  which were clashing with the textfield controller)
 *
 * There are also several places that need to be updated if you need
 * to change a command name. 
 *   1) the controller...
 *      - in bookmarksTree.xml if the command is tree-specifc
 *      - in bookmarksMenu.js if the command is DOM-specific
 *      - in bookmarks.js otherwise
 *   2) the command nodes in the overlay or xul file
 *   3) the command human-readable name key in bookmarks.properties
 *   4) the function 'getCommands' in bookmarks.js
 */

var BookmarksCommand = {

  /////////////////////////////////////////////////////////////////////////////
  // This method constructs a menuitem for a context menu for the given command.
  // This is implemented by the client so that it can intercept menuitem naming
  // as appropriate.
  createMenuItem: function (aDisplayName, aCommandName, aSelection)
  {
    var xulElement = document.createElementNS(XUL_NS, "menuitem");
    xulElement.setAttribute("cmd", aCommandName);
    var cmd = "cmd_" + aCommandName.substring(NC_NS_CMD.length);
    xulElement.setAttribute("command", cmd);
    
    switch (aCommandName) {
    case NC_NS_CMD + "bm_expandfolder":
      var shouldCollapse = true;
      for (var i=0; i<aSelection.length; ++i)
        if (!aSelection.isExpanded[i])
          shouldCollapse = false;
      aDisplayName =  shouldCollapse? BookmarksUtils.getLocaleString("cmd_bm_collapsefolder") : aDisplayName;
    break;
    }
    xulElement.setAttribute("label", aDisplayName);
    return xulElement;
  },

  /////////////////////////////////////////////////////////////////////////////
  // Fill a context menu popup with menuitems that are appropriate for the current
  // selection.
  createContextMenu: function (aEvent, aSelection)
  {
    var popup = aEvent.target;
    // clear out the old context menu contents (if any)
    while (popup.hasChildNodes()) 
      popup.removeChild(popup.firstChild);
        
    var commonCommands = [];
    for (var i = 0; i < aSelection.length; ++i) {
      var commands = this.getCommands(aSelection.item[i]);
      if (!commands) {
        aEvent.preventDefault();
        return;
      }
      commands = this.flattenEnumerator(commands);
      if (!commonCommands.length) commonCommands = commands;
      commonCommands = this.findCommonNodes(commands, commonCommands);
    }

    if (!commonCommands.length) {
      aEvent.preventDefault();
      return;
    }
    
    // Now that we should have generated a list of commands that is valid
    // for the entire selection, build a context menu.
    for (i = 0; i < commonCommands.length; ++i) {
      var currCommand = commonCommands[i].QueryInterface(kRDFRSCIID).Value;
      var element = null;
      if (currCommand != NC_NS_CMD + "bm_separator") {
        var commandName = this.getCommandName(currCommand);
        element = this.createMenuItem(commandName, currCommand, aSelection);
      }
      else if (i != 0 && i < commonCommands.length-1) {
        // Never append a separator as the first or last element in a context
        // menu.
        element = document.createElementNS(XUL_NS, "menuseparator");
      }
      if (element) 
        popup.appendChild(element);
    }
    switch (popup.firstChild.getAttribute("command")) {
    case "cmd_bm_open":
    case "cmd_bm_expandfolder":
      popup.firstChild.setAttribute("default", "true");
    }
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // Given two unique arrays, return an array that contains only the elements
  // common to both. 
  findCommonNodes: function (aNewArray, aOldArray)
  {
    var common = [];
    for (var i = 0; i < aNewArray.length; ++i) {
      for (var j = 0; j < aOldArray.length; ++j) {
        if (common.length > 0 && common[common.length-1] == aNewArray[i])
          continue;
        if (aNewArray[i] == aOldArray[j])
          common.push(aNewArray[i]);
      }
    }
    return common;
  },

  flattenEnumerator: function (aEnumerator)
  {
    if ("_index" in aEnumerator)
      return aEnumerator._inner;
    
    var temp = [];
    while (aEnumerator.hasMoreElements()) 
      temp.push(aEnumerator.getNext());
    return temp;
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // For a given URI (a unique identifier of a resource in the graph) return 
  // an enumeration of applicable commands for that URI. 
  getCommands: function (aNodeID)
  {
    var type = BookmarksUtils.resolveType(aNodeID);
    if (!type)
      return null;
    var commands = [];
    // menu order:
    // 
    // bm_expandfolder
    // bm_open
    // bm_openinnewwindow
    // bm_openinnewtab
    // ---------------------
    // bm_newfolder
    // ---------------------
    // bm_cut
    // bm_copy
    // bm_paste
    // ---------------------
    // bm_delete
    // ---------------------
    // bm_properties
    switch (type) {
    case "BookmarkSeparator":
      commands = ["bm_newfolder", "bm_separator", 
                  "bm_cut", "bm_copy", "bm_paste", "bm_separator",
                  "bm_delete", "bm_separator",
                  "bm_properties"];
      break;
    case "Bookmark":
      commands = ["bm_open", "bm_openinnewwindow", "bm_openinnewtab", "bm_separator",
                  "bm_newfolder", "bm_sortfolder", "bm_sortfolderbyname", "bm_separator",
                  "bm_cut", "bm_copy", "bm_paste", "bm_movebookmark", "bm_separator",
                  "bm_rename", "bm_delete", "bm_separator",
                  "bm_properties"];
      break;
    case "Folder":
      commands = ["bm_expandfolder", "bm_managefolder", "bm_separator", 
                  "bm_newfolder", "bm_sortfolder", "bm_sortfolderbyname", "bm_separator",
                  "bm_cut", "bm_copy", "bm_paste", "bm_movebookmark", "bm_separator",
                  "bm_rename", "bm_delete", "bm_separator",
                  "bm_properties"];
      break;
    case "FolderGroup":
      commands = ["bm_open", "bm_openinnewwindow", "bm_expandfolder", "bm_separator",
                  "bm_newfolder", "bm_sortfolder", "bm_sortfolderbyname", "bm_separator",
                  "bm_cut", "bm_copy", "bm_paste", "bm_movebookmark", "bm_separator",
                  "bm_rename", "bm_delete", "bm_separator",
                  "bm_properties"];
      break;
    case "PersonalToolbarFolder":
      commands = ["bm_newfolder", "bm_sortfolder", "bm_sortfolderbyname", "bm_separator",
                  "bm_cut", "bm_copy", "bm_paste", "bm_movebookmark", "bm_separator",
                  "bm_rename", "bm_delete", "bm_separator",
                  "bm_properties"];
      break;
    case "IEFavoriteFolder":
      commands = ["bm_expandfolder", "bm_separator", "bm_delete"];
      break;
    case "IEFavorite":
      commands = ["bm_open", "bm_openinnewwindow", "bm_openinnewtab", "bm_separator",
                  "bm_copy"];
      break;
    case "FileSystemObject":
      commands = ["bm_open", "bm_openinnewwindow", "bm_openinnewtab", "bm_separator",
                  "bm_copy"];
      break;
    default: 
      commands = [];
    }

    return new CommandArrayEnumerator(commands);
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // Retrieve the human-readable name for a particular command. Used when 
  // manufacturing a UI to invoke commands.
  getCommandName: function (aCommand) 
  {
    var cmdName = aCommand.substring(NC_NS_CMD.length);
    return BookmarksUtils.getLocaleString ("cmd_" + cmdName);
    /*
    try {
      // Note: this will succeed only if there's a string in the bookmarks
      //       string bundle for this command name. Otherwise, <xul:stringbundle/>
      //       will throw, we'll catch & stifle the error, and look up the command
      //       name in the datasource. 
      return BookmarksUtils.getLocaleString ("cmd_" + cmdName);
    }
    catch (e) {
    }   
    // XXX - WORK TO DO HERE! (rjc will cry if we don't fix this) 
    // need to ask the ds for the commands for this node, however we don't
    // have the right params. This is kind of a problem. 
    dump("*** BAD! EVIL! WICKED! NO! ACK! ARGH! ORGH!"+aCommand+"\n");
    const rName = RDF.GetResource(NC_NS + "Name");
    const rSource = RDF.GetResource(aNodeID);
    return BMDS.GetTarget(rSource, rName, true).Value;
    */
  },
    
  ///////////////////////////////////////////////////////////////////////////
  // Execute a command with the given source and arguments
  doBookmarksCommand: function (aSource, aCommand, aArgumentsArray)
  {
    var rCommand = RDF.GetResource(aCommand);
  
    var kSuppArrayContractID = "@mozilla.org/supports-array;1";
    var kSuppArrayIID = Components.interfaces.nsISupportsArray;
    var sourcesArray = Components.classes[kSuppArrayContractID].createInstance(kSuppArrayIID);
    if (aSource) {
      sourcesArray.AppendElement(aSource);
    }
  
    var argsArray = Components.classes[kSuppArrayContractID].createInstance(kSuppArrayIID);
    var length = aArgumentsArray?aArgumentsArray.length:0;
    for (var i = 0; i < length; ++i) {
      var rArc = RDF.GetResource(aArgumentsArray[i].property);
      argsArray.AppendElement(rArc);
      var rValue = null;
      if ("resource" in aArgumentsArray[i]) { 
        rValue = RDF.GetResource(aArgumentsArray[i].resource);
      }
      else
        rValue = RDF.GetLiteral(aArgumentsArray[i].literal);
      argsArray.AppendElement(rValue);
    }

    // Exec the command in the Bookmarks datasource. 
    BMDS.DoCommand(sourcesArray, rCommand, argsArray);
  },

  undoBookmarkTransaction: function ()
  {
    BMSVC.transactionManager.undoTransaction();
    BookmarksUtils.flushDataSource();
  },

  redoBookmarkTransaction: function ()
  {
    BMSVC.transactionManager.redoTransaction();
    BookmarksUtils.flushDataSource();
  },

  manageFolder: function (aSelection)
  {
    openDialog("chrome://communicator/content/bookmarks/bookmarksManager.xul", 
               "", "chrome,all,dialog=no", aSelection.item[0].Value);
  },
  
  cutBookmark: function (aSelection)
  {
    this.copyBookmark(aSelection);
    BookmarksUtils.removeSelection("cut", aSelection);
  },

  copyBookmark: function (aSelection)
  {

    const kSuppArrayContractID = "@mozilla.org/supports-array;1";
    const kSuppArrayIID = Components.interfaces.nsISupportsArray;
    var itemArray = Components.classes[kSuppArrayContractID].createInstance(kSuppArrayIID);

    const kSuppWStringContractID = "@mozilla.org/supports-string;1";
    const kSuppWStringIID = Components.interfaces.nsISupportsString;
    var bmstring = Components.classes[kSuppWStringContractID].createInstance(kSuppWStringIID);
    var unicodestring = Components.classes[kSuppWStringContractID].createInstance(kSuppWStringIID);
    var htmlstring = Components.classes[kSuppWStringContractID].createInstance(kSuppWStringIID);
  
    var sBookmarkItem = ""; var sTextUnicode = ""; var sTextHTML = "";
    for (var i = 0; i < aSelection.length; ++i) {
      var url  = BookmarksUtils.getProperty(aSelection.item[i], NC_NS+"URL" );
      var name = BookmarksUtils.getProperty(aSelection.item[i], NC_NS+"Name");
      sBookmarkItem += aSelection.item[i].Value + "\n";
      sTextUnicode += url + "\n";
      sTextHTML += "<A HREF=\"" + url + "\">" + name + "</A>";
    }
    
    const kXferableContractID = "@mozilla.org/widget/transferable;1";
    const kXferableIID = Components.interfaces.nsITransferable;
    var xferable = Components.classes[kXferableContractID].createInstance(kXferableIID);

    xferable.addDataFlavor("moz/bookmarkclipboarditem");
    bmstring.data = sBookmarkItem;
    xferable.setTransferData("moz/bookmarkclipboarditem", bmstring, sBookmarkItem.length*2);
    
    xferable.addDataFlavor("text/html");
    htmlstring.data = sTextHTML;
    xferable.setTransferData("text/html", htmlstring, sTextHTML.length*2);
    
    xferable.addDataFlavor("text/unicode");
    unicodestring.data = sTextUnicode;
    xferable.setTransferData("text/unicode", unicodestring, sTextUnicode.length*2);
    
    const kClipboardContractID = "@mozilla.org/widget/clipboard;1";
    const kClipboardIID = Components.interfaces.nsIClipboard;
    var clipboard = Components.classes[kClipboardContractID].getService(kClipboardIID);
    clipboard.setData(xferable, null, kClipboardIID.kGlobalClipboard);
  },

  pasteBookmark: function (aTarget)
  {
    const kXferableContractID = "@mozilla.org/widget/transferable;1";
    const kXferableIID = Components.interfaces.nsITransferable;
    var xferable = Components.classes[kXferableContractID].createInstance(kXferableIID);
    xferable.addDataFlavor("moz/bookmarkclipboarditem");
    xferable.addDataFlavor("text/x-moz-url");
    xferable.addDataFlavor("text/unicode");

    const kClipboardContractID = "@mozilla.org/widget/clipboard;1";
    const kClipboardIID = Components.interfaces.nsIClipboard;
    var clipboard = Components.classes[kClipboardContractID].getService(kClipboardIID);
    clipboard.getData(xferable, kClipboardIID.kGlobalClipboard);
    
    var flavour = { };
    var data    = { };
    var length  = { };
    xferable.getAnyTransferData(flavour, data, length);
    var items, name, url;
    data = data.value.QueryInterface(Components.interfaces.nsISupportsString).data;
    switch (flavour.value) {
    case "moz/bookmarkclipboarditem":
      items = data.split("\n");
      // since data are ended by \n, remove the last empty node
      items.pop(); 
      for (var i=0; i<items.length; ++i) {
        items[i] = RDF.GetResource(items[i]);
      }
      break;
    case "text/x-moz-url":
      // there should be only one item in this case
      var ix = data.indexOf("\n");
      items = data.substring(0, ix != -1 ? ix : data.length);
      name  = data.substring(ix);
      // XXX: we should infer the best charset
      BookmarksUtils.createBookmark(null, items, null, name);
      items = [items];
      break;
    default: 
      return;
    }
   
    var selection = {item: items, parent:Array(items.length), length: items.length};
    BookmarksUtils.checkSelection(selection);
    BookmarksUtils.insertSelection("paste", selection, aTarget);
  },
  
  deleteBookmark: function (aSelection)
  {
    BookmarksUtils.removeSelection("delete", aSelection);
  },

  moveBookmark: function (aSelection)
  {
    var rv = { selectedFolder: null };      
    openDialog("chrome://communicator/content/bookmarks/addBookmark.xul", "", 
               "centerscreen,chrome,modal=yes,dialog=yes,resizable=yes", null, null, null, null, "selectFolder", rv);
    if (!rv.target)
      return;
    
    var target = rv.target;
    BookmarksUtils.moveSelection("move", aSelection, target);
  },

  openBookmark: function (aSelection, aTargetBrowser, aDS) 
  {
    if (!aTargetBrowser)
      return;
    for (var i=0; i<aSelection.length; ++i) {
      var type = aSelection.type[i];
      if (type == "Bookmark" || type == "")
        this.openOneBookmark(aSelection.item[i].Value, aTargetBrowser, aDS);
      else if (type == "FolderGroup" || type == "Folder" || type == "PersonalToolbarFolder")
        this.openGroupBookmark(aSelection.item[i].Value, aTargetBrowser);
    }
  },
  
  openBookmarkProperties: function (aSelection) 
  {
    // Bookmark Properties dialog is only ever opened with one selection 
    // (command is disabled otherwise)
    var bookmark = aSelection.item[0].Value;
    return openDialog("chrome://communicator/content/bookmarks/bm-props.xul", "", "centerscreen,chrome,dependent,resizable=no", bookmark);      
  },

  // requires utilityOverlay.js if opening in new window for getTopWin()
  openOneBookmark: function (aURI, aTargetBrowser, aDS)
  {
    var url = BookmarksUtils.getProperty(aURI, NC_NS+"URL", aDS);
    // Ignore "NC:" and empty urls.
    if (url == "")
      return;
    var w = aTargetBrowser == "window"? null:getTopWin();
    if (!w) {
      openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", url);
      return;
    }
    var browser = w.document.getElementById("content");
    switch (aTargetBrowser) {
    case "current":
      browser.loadURI(url);
      w.content.focus();
      break;
    case "tab":
      var tab = browser.addTab(url);
      browser.selectedTab = tab;
      break;
    case "tab_background":
      browser.addTab(url);
      break;
    }
  },

  openGroupBookmark: function (aURI, aTargetBrowser)
  {
    var w = getTopWin();
    if (!w) // no browser window open, so we have to open in new window
      aTargetBrowser="window";

    var resource = RDF.GetResource(aURI);
    var urlArc   = RDF.GetResource(NC_NS+"URL");
    RDFC.Init(BMDS, resource);
    var containerChildren = RDFC.GetElements();
    var URIs = [];
    while (containerChildren.hasMoreElements()) {
      var res = containerChildren.getNext().QueryInterface(kRDFRSCIID);
      var target = BMDS.GetTarget(res, urlArc, true);
      if (target) {
        if (aTargetBrowser == "window")
          URIs.push(target.QueryInterface(kRDFLITIID).Value);
        else
          URIs.push({ URI: target.QueryInterface(kRDFLITIID).Value });        
      }
    }
    if (aTargetBrowser == "window") {
      // This opens the URIs in separate tabs of a new window
      openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", URIs.join("\n"));
    } else {
      var browser = w.getBrowser();
      var tab = browser.loadGroup(URIs);
      if (aTargetBrowser != "tab_background")
        browser.selectedTab = tab;
    }
  },

  findBookmark: function ()
  {
    openDialog("chrome://communicator/content/bookmarks/findBookmark.xul",
               "FindBookmarksWindow",
               "centerscreen,resizable=no,chrome,dependent");
  },

  createNewFolder: function (aTarget)
  {
    var name      = BookmarksUtils.getLocaleString("ile_newfolder");
    var rFolder   = BMSVC.createFolder(name);
    
    var selection = BookmarksUtils.getSelectionFromResource(rFolder, aTarget.parent);
    var ok        = BookmarksUtils.insertSelection("newfolder", selection, aTarget);
    if (ok) {
      var propWin = this.openBookmarkProperties(selection);
      
      function canceledNewFolder()
      {
        BookmarksCommand.deleteBookmark(selection);
        propWin.document.documentElement.removeEventListener("dialogcancel", canceledNewFolder, false);
        propWin.removeEventListener("load", propertiesWindowLoad, false);
      }
      
      function propertiesWindowLoad()
      {
        propWin.document.documentElement.addEventListener("dialogcancel", canceledNewFolder, false);
      }
      propWin.addEventListener("load", propertiesWindowLoad, false);
    }
  },

  createNewSeparator: function (aTarget)
  {
    var rSeparator  = BMSVC.createSeparator();
    var selection   = BookmarksUtils.getSelectionFromResource(rSeparator);
    BookmarksUtils.insertSelection("newseparator", selection, aTarget);
  },

  importBookmarks: function ()
  {
    ///transaction...
    try {
      const kFilePickerContractID = "@mozilla.org/filepicker;1";
      const kFilePickerIID = Components.interfaces.nsIFilePicker;
      const kFilePicker = Components.classes[kFilePickerContractID].createInstance(kFilePickerIID);
    
      const kTitle = BookmarksUtils.getLocaleString("SelectImport");
      kFilePicker.init(window, kTitle, kFilePickerIID["modeOpen"]);
      kFilePicker.appendFilters(kFilePickerIID.filterHTML | kFilePickerIID.filterAll);
      var fileName;
      if (kFilePicker.show() != kFilePickerIID.returnCancel) {
        fileName = kFilePicker.file.path;
        if (!fileName) return;
      }
      else return;
    }
    catch (e) {
      return;
    }
    rTarget = RDF.GetResource("NC:BookmarksRoot");
    RDFC.Init(BMDS, rTarget);
    var countBefore = parseInt(BookmarksUtils.getProperty(rTarget, RDF_NS+"nextVal"));
    var args = [{ property: NC_NS+"URL", literal: fileName}];
    this.doBookmarksCommand(rTarget, NC_NS_CMD+"import", args);
    var countAfter = parseInt(BookmarksUtils.getProperty(rTarget, RDF_NS+"nextVal"));

    var transaction = new BookmarkImportTransaction("import");
    for (var index = countBefore; index < countAfter; index++) {
      var nChildArc = RDFCU.IndexToOrdinalResource(index);
      var rChild    = BMDS.GetTarget(rTarget, nChildArc, true);
      transaction.item   .push(rChild);
      transaction.parent .push(rTarget);
      transaction.index  .push(index);
      transaction.isValid.push(true);
    }
    var isCancelled = !BookmarksUtils.any(transaction.isValid);
    if (!isCancelled) {
      BMSVC.transactionManager.doTransaction(transaction);
      BookmarksUtils.flushDataSource();
    }    
  },

  exportBookmarks: function ()
  {
    try {
      const kFilePickerContractID = "@mozilla.org/filepicker;1";
      const kFilePickerIID = Components.interfaces.nsIFilePicker;
      const kFilePicker = Components.classes[kFilePickerContractID].createInstance(kFilePickerIID);
      
      const kTitle = BookmarksUtils.getLocaleString("EnterExport");
      kFilePicker.init(window, kTitle, kFilePickerIID["modeSave"]);
      kFilePicker.appendFilters(kFilePickerIID.filterHTML | kFilePickerIID.filterAll);
      kFilePicker.defaultString = "bookmarks.html";
      var fileName;
      if (kFilePicker.show() != kFilePickerIID.returnCancel) {
        fileName = kFilePicker.file.path;
        if (!fileName) return;
      }
      else return;

      var aFile = Components.classes["@mozilla.org/file/local;1"].createInstance(Components.interfaces.nsILocalFile);
      if (!aFile)
        return;
      aFile.initWithPath(fileName);
      if (!aFile.exists()) {
        aFile.create(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, 0644);
      }
    }
    catch (e) {
      return;
    }
    var selection = RDF.GetResource("NC:BookmarksRoot");
    var args = [{ property: NC_NS+"URL", literal: fileName}];
    this.doBookmarksCommand(selection, NC_NS_CMD+"export", args);
  },

  sortFolderByName: function(aSelection)
  {
    var folder;
    var type = aSelection.type[0];
    if (type == "Bookmark")
      folder = RDF.GetResource(aSelection.parent[0].Value);
    else {
      folder = RDF.GetResource(aSelection.item[0].Value);
    }
    var property = RDF.GetResource(NC_NS + "Name");
    BMDS.sortFolder(folder, property, kBMSVCIID.SORT_ASCENDING, true, false);
  },

  sortFolder: function(aSelection)
  {
    var folder;
    var type = aSelection.type[0];
    if (type == "Bookmark")
      folder = RDF.GetResource(aSelection.parent[0].Value);
    else {
      folder = RDF.GetResource(aSelection.item[0].Value);
    }

    var sortOptions = { accepted : false };
    openDialog("chrome://communicator/content/bookmarks/sortFolder.xul",
               "sortFolder",
               "centerscreen,chrome,dependent,resizable=no,modal=yes",
               sortOptions);

    if (sortOptions.accepted) {
      var property = BookmarksUtils.getResource(sortOptions.sortBy);
      var direction = sortOptions.sortOrder == "ascending"
                      ? kBMSVCIID.SORT_ASCENDING : kBMSVCIID.SORT_DESCENDING;
      var foldersFirst = sortOptions.sortFoldersFirst;
      var recurse = sortOptions.sortRecursively;
      BMDS.sortFolder(folder, property, direction, foldersFirst, recurse);
    }
  }
}

  /////////////////////////////////////////////////////////////////////////////
  // Command handling & Updating.
var BookmarksController = {

  supportsCommand: function (aCommand)
  {
    var isCommandSupported;
    switch(aCommand) {
    case "cmd_undo":
    case "cmd_redo":
    case "cmd_bm_undo":
    case "cmd_bm_redo":
    case "cmd_bm_cut":
    case "cmd_bm_copy":
    case "cmd_bm_paste":
    case "cmd_bm_delete":
    case "cmd_bm_selectAll":
    case "cmd_bm_open":
    case "cmd_bm_openinnewwindow":
    case "cmd_bm_openinnewtab":
    case "cmd_bm_expandfolder":
    case "cmd_bm_managefolder":
    case "cmd_bm_newbookmark":
    case "cmd_bm_newfolder":
    case "cmd_bm_newseparator":
    case "cmd_bm_find":
    case "cmd_bm_properties":
    case "cmd_bm_rename":
    case "cmd_bm_setnewbookmarkfolder":
    case "cmd_bm_setpersonaltoolbarfolder":
    case "cmd_bm_setnewsearchfolder":
    case "cmd_bm_import":
    case "cmd_bm_export":
    case "cmd_bm_movebookmark":
    case "cmd_bm_sortfolderbyname":
    case "cmd_bm_sortfolder":
      isCommandSupported = true;
      break;
    default:
      isCommandSupported = false;
    }
    //if (!isCommandSupported)
    //  dump("Bookmark command '"+aCommand+"' is not supported!\n");
    return isCommandSupported;
  },

  isCommandEnabled: function (aCommand, aSelection, aTarget)
  {
    var item0, type0;
    var length = aSelection.length;
    if (length != 0) {
      item0 = aSelection.item[0].Value;
      type0 = aSelection.type[0];
    }
    var i;

    switch(aCommand) {
    case "cmd_undo":
    case "cmd_bm_undo":
      return BMSVC.transactionManager.numberOfUndoItems > 0;
    case "cmd_redo":
    case "cmd_bm_redo":
      return BMSVC.transactionManager.numberOfRedoItems > 0;
    case "cmd_bm_paste":
      if (!(aTarget && BookmarksUtils.isValidTargetContainer(aTarget.parent)))
        return false;
      const kClipboardContractID = "@mozilla.org/widget/clipboard;1";
      const kClipboardIID = Components.interfaces.nsIClipboard;
      var clipboard = Components.classes[kClipboardContractID].getService(kClipboardIID);
      const kSuppArrayContractID = "@mozilla.org/supports-array;1";
      const kSuppArrayIID = Components.interfaces.nsISupportsArray;
      var flavourArray = Components.classes[kSuppArrayContractID].createInstance(kSuppArrayIID);
      const kSuppStringContractID = "@mozilla.org/supports-cstring;1";
      const kSuppStringIID = Components.interfaces.nsISupportsCString;
    
      var flavours = ["moz/bookmarkclipboarditem", "text/x-moz-url"];
      for (i = 0; i < flavours.length; ++i) {
        const kSuppString = Components.classes[kSuppStringContractID].createInstance(kSuppStringIID);
        kSuppString.data = flavours[i];
        flavourArray.AppendElement(kSuppString);
      }
      var hasFlavours = clipboard.hasDataMatchingFlavors(flavourArray, kClipboardIID.kGlobalClipboard);
      return hasFlavours;
    case "cmd_bm_copy":
      return length > 0;
    case "cmd_bm_cut":
    case "cmd_bm_delete":
      return length > 0 && aSelection.containsMutable && !aSelection.containsPTF;
    case "cmd_bm_selectAll":
      return true;
    case "cmd_bm_open":
    case "cmd_bm_expandfolder":
    case "cmd_bm_managefolder":
      return length == 1;
    case "cmd_bm_openinnewwindow":
    case "cmd_bm_openinnewtab":
    case "cmd_bm_find":
    case "cmd_bm_import":
    case "cmd_bm_export":
      return true;
    case "cmd_bm_newbookmark":
    case "cmd_bm_newfolder":
    case "cmd_bm_newseparator":
      return (aTarget &&
              BookmarksUtils.isValidTargetContainer(aTarget.parent));
    case "cmd_bm_properties":
    case "cmd_bm_rename":
    case "cmd_bm_sortfolderbyname":
    case "cmd_bm_sortfolder":
      return length == 1;
    case "cmd_bm_setnewbookmarkfolder":
      if (length != 1) 
        return false;
      return item0 != "NC:NewBookmarkFolder"     &&
             (type0 == "Folder" || type0 == "PersonalToolbarFolder");
    case "cmd_bm_setpersonaltoolbarfolder":
      if (length != 1)
        return false;
      return item0 != "NC:PersonalToolbarFolder" &&
             item0 != "NC:BookmarksRoot" && type0 == "Folder";
    case "cmd_bm_setnewsearchfolder":
      if (length != 1)
        return false;
      return item0 != "NC:NewSearchFolder"       && 
             (type0 == "Folder" || type0 == "PersonalToolbarFolder");
    case "cmd_bm_movebookmark":
      return length > 0 && !aSelection.containsRF;
    default:
      return false;
    }
  },

  doCommand: function (aCommand, aSelection, aTarget)
  {
    switch (aCommand) {
    case "cmd_undo":
    case "cmd_bm_undo":
      BookmarksCommand.undoBookmarkTransaction();
      break;
    case "cmd_redo":
    case "cmd_bm_redo":
      BookmarksCommand.redoBookmarkTransaction();
      break;
    case "cmd_bm_open":
      BookmarksCommand.openBookmark(aSelection, "current");
      break;
    case "cmd_bm_openinnewwindow":
      BookmarksCommand.openBookmark(aSelection, "window");
      break;
    case "cmd_bm_openinnewtab":
      BookmarksCommand.openBookmark(aSelection, "tab");
      break;
    case "cmd_bm_managefolder":
      BookmarksCommand.manageFolder(aSelection);
      break;
    case "cmd_bm_setnewbookmarkfolder":
    case "cmd_bm_setpersonaltoolbarfolder":
    case "cmd_bm_setnewsearchfolder":
      BookmarksCommand.doBookmarksCommand(aSelection.item[0], NC_NS_CMD+aCommand.substring("cmd_bm_".length), []);
      break;
    case "cmd_bm_rename":
    case "cmd_bm_properties":
      BookmarksCommand.openBookmarkProperties(aSelection);
      break;
    case "cmd_bm_find":
      BookmarksCommand.findBookmark();
      break;
    case "cmd_bm_cut":
      BookmarksCommand.cutBookmark(aSelection);
      break;
    case "cmd_bm_copy":
      BookmarksCommand.copyBookmark(aSelection);
      break;
    case "cmd_bm_paste":
      BookmarksCommand.pasteBookmark(aTarget);
      break;
    case "cmd_bm_delete":
      BookmarksCommand.deleteBookmark(aSelection);
      break;
    case "cmd_bm_movebookmark":
      BookmarksCommand.moveBookmark(aSelection);
      break;
    case "cmd_bm_newfolder":
      BookmarksCommand.createNewFolder(aTarget);
      break;
    case "cmd_bm_newbookmark":
      var folder = aTarget.parent.Value;
      var rv = { newBookmark: null };
      openDialog("chrome://communicator/content/bookmarks/addBookmark.xul", "", 
                 "centerscreen,chrome,modal=yes,dialog=yes,resizable=yes", null, null, folder, null, "newBookmark", rv);
      break;
    case "cmd_bm_newseparator":
      BookmarksCommand.createNewSeparator(aTarget);
      break;
    case "cmd_bm_import":
      BookmarksCommand.importBookmarks();
      break;
    case "cmd_bm_export":
      BookmarksCommand.exportBookmarks();
      break;
    case "cmd_bm_sortfolderbyname":
      BookmarksCommand.sortFolderByName(aSelection);
      break;
    case "cmd_bm_sortfolder":
      BookmarksCommand.sortFolder(aSelection);
      break;
    default: 
      dump("Bookmark command "+aCommand+" not handled!\n");
    }

  },

  onCommandUpdate: function (aSelection, aTarget)
  {
    var commands = ["cmd_bm_newbookmark", "cmd_bm_newfolder", "cmd_bm_newseparator", 
                    "cmd_bm_properties", "cmd_bm_rename", 
                    "cmd_bm_copy", "cmd_bm_paste", "cmd_bm_cut", "cmd_bm_delete",
                    "cmd_bm_setpersonaltoolbarfolder", 
                    "cmd_bm_setnewbookmarkfolder",
                    "cmd_bm_setnewsearchfolder", "cmd_bm_movebookmark", 
                    "cmd_bm_managefolder",
                    "cmd_bm_sortfolder", "cmd_bm_sortfolderbyname",
                    "cmd_undo", "cmd_redo", "cmd_bm_undo", "cmd_bm_redo"];
    for (var i = 0; i < commands.length; ++i) {
      var commandNode = document.getElementById(commands[i]);
      if (commandNode) {
        if (this.isCommandEnabled(commands[i], aSelection, aTarget))
          commandNode.removeAttribute("disabled");
        else 
          commandNode.setAttribute("disabled", "true");
      }
    }
  }
}

function CommandArrayEnumerator (aCommandArray)
{
  this._inner = [];
  for (var i = 0; i < aCommandArray.length; ++i)
    this._inner.push(RDF.GetResource(NC_NS_CMD + aCommandArray[i]));
    
  this._index = 0;
}

CommandArrayEnumerator.prototype = {
  getNext: function () 
  {
    return this._inner[this._index];
  },
  
  hasMoreElements: function ()
  {
    return this._index < this._inner.length;
  }
};

var BookmarksUtils = {

  DROP_BEFORE: Components.interfaces.nsITreeView.DROP_BEFORE,
  DROP_ON    : Components.interfaces.nsITreeView.DROP_ON,
  DROP_AFTER : Components.interfaces.nsITreeView.DROP_AFTER,

  any: function (aArray)
  {
    for (var i=0; i<aArray.length; ++i)
      if (aArray[i]) return true;
    return false;
  },

  all: function (aArray)
  {
    for (var i=0; i<aArray.length; ++i)
      if (!aArray[i]) return false;
    return true;
  },

  _bundle        : null,
  _brandShortName: null,

  /////////////////////////////////////////////////////////////////////////////////////
  // returns a property from chrome://communicator/locale/bookmarks/bookmarks.properties
  getLocaleString: function (aStringKey, aReplaceString)
  {
    if (!this._bundle) {
      // for those who would xblify Bookmarks.js, there is a need to create string bundle 
      // manually instead of using <xul:stringbundle/> see bug 63370 for details
      var LOCALESVC = Components.classes["@mozilla.org/intl/nslocaleservice;1"]
                                .getService(Components.interfaces.nsILocaleService);
      var BUNDLESVC = Components.classes["@mozilla.org/intl/stringbundle;1"]
                                .getService(Components.interfaces.nsIStringBundleService);
      var bookmarksBundle  = "chrome://communicator/locale/bookmarks/bookmarks.properties";
      this._bundle         = BUNDLESVC.createBundle(bookmarksBundle, LOCALESVC.getApplicationLocale());
      var brandBundle      = "chrome://global/locale/brand.properties";
      this._brandShortName = BUNDLESVC.createBundle(brandBundle,     LOCALESVC.getApplicationLocale())
                                      .GetStringFromName("brandShortName");
    }
   
    var bundle;
    try {
      if (!aReplaceString)
        bundle = this._bundle.GetStringFromName(aStringKey);
      else if (typeof(aReplaceString) == "string")
        bundle = this._bundle.formatStringFromName(aStringKey, [aReplaceString], 1);
      else
        bundle = this._bundle.formatStringFromName(aStringKey, aReplaceString, aReplaceString.length);
    } catch (e) {
      dump("Bookmark bundle "+aStringKey+" not found!\n");
      bundle = "";
    }

    bundle = bundle.replace(/%brandShortName%/, this._brandShortName);
    return bundle;
  },
    
  /////////////////////////////////////////////////////////////////////////////
  // returns the literal targeted by the URI aArcURI for a resource or uri
  getProperty: function (aInput, aArcURI, aDS)
  {
    var node;
    var arc  = RDF.GetResource(aArcURI);
    if (typeof(aInput) == "string") 
      aInput = RDF.GetResource(aInput);
    if (!aDS)
      node = BMDS.GetTarget(aInput, arc, true);
    else
      node = aDS .GetTarget(aInput, arc, true);
    return (node instanceof kRDFRSCIID) || (node instanceof kRDFLITIID) ? node.Value : "";
  },

  getResource: function (aName)
  {
    if (aName == "LastModifiedDate" ||
        aName == "LastVisitDate") {
      return RDF.GetResource(WEB_NS + aName);
    }
    else {
      return RDF.GetResource(NC_NS + aName);
    }
  },

  /////////////////////////////////////////////////////////////////////////////
  // Determine the rdf:type property for the given resource.
  resolveType: function (aResource)
  {
    var type = this.getProperty(aResource, RDF_NS+"type");
    if (type != "")
      type = type.split("#")[1];
    if (type == "Folder") {
      if (this.isPersonalToolbarFolder(aResource))
        type = "PersonalToolbarFolder";
      else if (this.isFolderGroup(aResource))
        type = "FolderGroup";
    }
    return type;
  },

  /////////////////////////////////////////////////////////////////////////////
  // Returns true if aResource is a folder group
  isFolderGroup: function (aResource) {
    return this.getProperty(aResource, NC_NS+"FolderGroup") == "true";
  },

  /////////////////////////////////////////////////////////////////////////////
  // Returns true if aResource is the Personal Toolbar Folder
  isPersonalToolbarFolder: function (aResource) {
    return this.getProperty(aResource, NC_NS+"FolderType") == "NC:PersonalToolbarFolder";
  },

  /////////////////////////////////////////////////////////////////////////////
  // Returns the folder which 'FolderType' is aProperty
  getSpecialFolder: function (aProperty)
  {
    var sources = BMDS.GetSources(RDF.GetResource(NC_NS+"FolderType"),
                                  RDF.GetResource(aProperty), true);
    var folder = null;
    if (sources.hasMoreElements())
      folder = sources.getNext();
    else 
      folder = RDF.GetResource("NC:BookmarksRoot");
    return folder;
  },

  /////////////////////////////////////////////////////////////////////////////
  // Returns the New Bookmark Folder
  getNewBookmarkFolder: function()
  {
    return this.getSpecialFolder("NC:NewBookmarkFolder");
  },

  /////////////////////////////////////////////////////////////////////////////
  // Returns the New Search Folder
  getNewSearchFolder: function()
  {
    return this.getSpecialFolder("NC:NewSearchFolder");
  },

  /////////////////////////////////////////////////////////////////////////////
  // Returns the container of a given container
  getParentOfContainer: function(aChild)
  {
    var arcsIn = BMDS.ArcLabelsIn(aChild);
    var containerArc;
    while (arcsIn.hasMoreElements()) {
      containerArc = arcsIn.getNext();
      if (RDFCU.IsOrdinalProperty(containerArc)) {
        return BMDS.GetSources(containerArc, aChild, true).getNext()
                   .QueryInterface(kRDFRSCIID);
      }
    }
    return null;
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // Caches frequently used informations about the selection
  checkSelection: function (aSelection)
  {
    if (aSelection.length == 0)
      return;

    aSelection.type            = new Array(aSelection.length);
    aSelection.protocol        = new Array(aSelection.length);
    aSelection.isContainer     = new Array(aSelection.length);
    aSelection.isImmutable     = new Array(aSelection.length);
    aSelection.isValid         = new Array(aSelection.length);
    aSelection.containsMutable = false;
    aSelection.containsPTF     = false;
    aSelection.containsRF      = false;
    var index, item, parent, type, protocol, isContainer, isImmutable, isValid;
    for (var i=0; i<aSelection.length; ++i) {
      item        = aSelection.item[i];
      parent      = aSelection.parent[i];
      type        = BookmarksUtils.resolveType(item);
      protocol    = item.Value.split(":")[0];
      isContainer = RDFCU.IsContainer(BMDS, item) ||
                    protocol == "find" || protocol == "file";
      isValid     = true;
      isImmutable = false;
      if (item.Value == "NC:BookmarksRoot") {
        isImmutable = true;
        aSelection.containsRF = true;
      }
      else if (type != "Bookmark" && type != "BookmarkSeparator" && 
               type != "Folder"   && type != "FolderGroup" && 
               type != "PersonalToolbarFolder")
        isImmutable = true;
      else if (parent) {
        var parentProtocol = parent.Value.split(":")[0];
        if (parentProtocol == "find" || parentProtocol == "file")
          aSelection.parent[i] = null;
      }
      if (!isImmutable && aSelection.parent[i])
        aSelection.containsMutable = true;

      aSelection.type       [i] = type;
      aSelection.protocol   [i] = protocol;
      aSelection.isContainer[i] = isContainer;
      aSelection.isImmutable[i] = isImmutable;
      aSelection.isValid    [i] = isValid;
    }
    if (this.isContainerChildOrSelf(RDF.GetResource("NC:PersonalToolbarFolder"), aSelection))
      aSelection.containsPTF = true;
  },

  isSelectionValidForInsertion: function (aSelection, aTarget, aAction)
  {
    if (!BookmarksUtils.isValidTargetContainer(aTarget.parent, aSelection)) {
      var isValid = new Array(aSelection.length);
      for (var i=0; i<aSelection.length; ++i)
        isValid[i] = false;
      return isValid;
    }
    return aSelection.isValid;
  },

  isSelectionValidForDeletion: function (aSelection)
  {
    var isValid = new Array(aSelection.length);
    for (i=0; i<aSelection.length; ++i) {
      if (!aSelection.isValid[i] || aSelection.isImmutable[i] || 
          !aSelection.parent [i])
        isValid[i] = false;
      else
        isValid[i] = true;
    }
    return isValid;
  },

  /////////////////////////////////////////////////////////////////////////////
  // Returns true is aContainer is a member or a child of the selection
  isContainerChildOrSelf: function (aContainer, aSelection)
  {
    var folder = aContainer;
    do {
      for (var i=0; i<aSelection.length; ++i) {
        if (aSelection.isContainer[i] && aSelection.item[i] == folder)
          return true;
      }
      folder = BookmarksUtils.getParentOfContainer(folder);
      if (!folder)
        return false; // sanity check
    } while (folder.Value != "NC:BookmarksRoot")
    return false;
  },

  /////////////////////////////////////////////////////////////////////////////
  // Returns true if aSelection can be inserted in aFolder
  isValidTargetContainer: function (aFolder, aSelection)
  {
    if (!aFolder)
      return false;
    if (aFolder.Value == "NC:BookmarksTopRoot")
      return false;
    if (aFolder.Value == "NC:BookmarksRoot")
      return true;

    // don't insert items in an invalid container
    // 'file:' and 'find:' items have a 'Bookmark' type
    var type = BookmarksUtils.resolveType(aFolder);
    if (type != "Folder" && type != "FolderGroup" && type != "PersonalToolbarFolder")
      return false;

    // bail if we just check the container
    if (!aSelection)
      return true;

    // don't insert folders in a group folder except for 'file:' pseudo folders
    if (type == "FolderGroup")
      for (var i=0; i<aSelection.length; ++i)
        if (aSelection.isContainer[i] && aSelection.protocol[i] != "file")
          return false;

    // check that the selected folder is not the selected item nor its child
    if (this.isContainerChildOrSelf(aFolder, aSelection))
      return false;

    return true;
  },

  /////////////////////////////////////////////////////////////////////////////
  // Returns true is aItem is a child of aContainer
  isChildOfContainer: function (aItem, aContainer)
  {
    RDFC.Init(BMDS, aContainer);
    var rChildren = RDFC.GetElements();
    while (rChildren.hasMoreElements()) {
      if (aItem == rChildren.getNext())
        return true;
    }
    return false;
  },

  /////////////////////////////////////////////////////////////////////////////
  removeSelection: function (aAction, aSelection)
  {
    var transaction     = new BookmarkRemoveTransaction(aAction);
    transaction.item    = new Array(aSelection.length);
    transaction.parent  = new Array(aSelection.length);
    transaction.index   = new Array(aSelection.length);
    transaction.isValid = BookmarksUtils.isSelectionValidForDeletion(aSelection);
    for (var i = 0; i < aSelection.length; ++i) {
      transaction.item  [i] = aSelection.item  [i];
      transaction.parent[i] = aSelection.parent[i];
      if (transaction.isValid[i]) {
        RDFC.Init(BMDS, aSelection.parent[i]);
        transaction.index[i]  = RDFC.IndexOf(aSelection.item[i]);
      }
    }
    if (SOUND && aAction != "move" && !BookmarksUtils.all(transaction.isValid))
      SOUND.beep();
    var isCancelled = !BookmarksUtils.any(transaction.isValid);
    if (!isCancelled) {
      BMSVC.transactionManager.doTransaction(transaction);
      if (aAction != "move")
        BookmarksUtils.flushDataSource();
    }
    return !isCancelled;
  },
      // If the current bookmark is the IE Favorites folder, we have a little
      // extra work to do - set the pref |browser.bookmarks.import_system_favorites|
      // to ensure that we don't re-import next time. 
      //if (aSelection[count].getAttribute("type") == (NC_NS + "IEFavoriteFolder")) {
      //  const kPrefSvcContractID = "@mozilla.org/preferences-service;1";
      //  const kPrefSvcIID = Components.interfaces.nsIPrefBranch;
      //  const kPrefSvc = Components.classes[kPrefSvcContractID].getService(kPrefSvcIID);
      //  kPrefSvc.setBoolPref("browser.bookmarks.import_system_favorites", false);
      //}
        
  insertSelection: function (aAction, aSelection, aTarget)
  {
    var transaction     = new BookmarkInsertTransaction(aAction);
    transaction.item    = new Array(aSelection.length);
    transaction.parent  = new Array(aSelection.length);
    transaction.index   = new Array(aSelection.length);
    if (aAction != "move")
      transaction.isValid = BookmarksUtils.isSelectionValidForInsertion(aSelection, aTarget, aAction);
    else // isValid has already been determined in moveSelection
      transaction.isValid = aSelection.isValid;
    var index = aTarget.index;
    for (var i=0; i<aSelection.length; ++i) {
      var rSource = aSelection.item[i];
      
      if (transaction.isValid[i]) {
        if (BMSVC.isBookmarkedResource(rSource))
          rSource = BMSVC.cloneResource(rSource);
        transaction.item  [i] = rSource;
        transaction.parent[i] = aTarget.parent;
        transaction.index [i] = index++;
      } else {
        transaction.item  [i] = rSource;
        transaction.parent[i] = aSelection.parent[i];
      } 
    }
    if (SOUND && !BookmarksUtils.all(transaction.isValid))
      SOUND.beep();
    var isCancelled = !BookmarksUtils.any(transaction.isValid);
    if (!isCancelled) {
      BMSVC.transactionManager.doTransaction(transaction);
      BookmarksUtils.flushDataSource();
    }
    return !isCancelled;
  },

  moveSelection: function (aAction, aSelection, aTarget)
  {
    aSelection.isValid = BookmarksUtils.isSelectionValidForInsertion(aSelection, aTarget, "move");
    var transaction = new BookmarkMoveTransaction(aAction, aSelection, aTarget);
    var isCancelled = !BookmarksUtils.any(transaction.isValid);
    if (!isCancelled) {
      BMSVC.transactionManager.doTransaction(transaction);
    } else if (SOUND)
      SOUND.beep();
    return !isCancelled;
  }, 

  getXferDataFromSelection: function (aSelection)
  {
    if (aSelection.length == 0)
      return null;
    var dataSet = new TransferDataSet();
    var data, item, itemUrl, itemName, parent, name;
    for (var i=0; i<aSelection.length; ++i) {
      data     = new TransferData();
      item     = aSelection.item[i].Value;
      itemUrl  = this.getProperty(item, NC_NS+"URL");
      itemName = this.getProperty(item, NC_NS+"Name");
      parent   = aSelection.parent[i].Value;
      data.addDataForFlavour("moz/rdfitem",    item+"\n"+(parent?parent:""));
      data.addDataForFlavour("text/x-moz-url", itemUrl+"\n"+itemName);
      data.addDataForFlavour("text/html",      "<A HREF='"+itemUrl+"'>"+itemName+"</A>");
      data.addDataForFlavour("text/unicode",   itemUrl);
      dataSet.push(data);
    }
    return dataSet;
  },

  getSelectionFromXferData: function (aDragSession)
  {
    var selection    = {};
    selection.item   = [];
    selection.parent = [];
    var trans = Components.classes["@mozilla.org/widget/transferable;1"]
                          .createInstance(Components.interfaces.nsITransferable);
    trans.addDataFlavor("moz/rdfitem");
    trans.addDataFlavor("text/x-moz-url");
    trans.addDataFlavor("text/unicode");
    var uri, extra, rSource, rParent, parent;
    for (var i = 0; i < aDragSession.numDropItems; ++i) {
      var bestFlavour = {}, dataObj = {}, len = {};
      aDragSession.getData(trans, i);
      trans.getAnyTransferData(bestFlavour, dataObj, len);
      dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);
      if (!dataObj)
        continue;
      dataObj = dataObj.data.substring(0, len.value).split("\n");
      uri     = dataObj[0];
      if (dataObj.length > 1 && dataObj[1] != "")
        extra = dataObj[1];
      else
        extra = null;
      switch (bestFlavour.value) {
      case "moz/rdfitem":
        rSource = RDF.GetResource(uri);
        parent  = extra;
        break;
      case "text/x-moz-url":
      case "text/unicode":
        rSource = BookmarksUtils.createBookmark(null, uri, null, extra);
        parent = null;
        break;
      }
      selection.item.push(rSource);
      if (parent)
        rParent = RDF.GetResource(parent);
      else
        rParent = null;
      selection.parent.push(rParent);
    }
    selection.length = selection.item.length;
    BookmarksUtils.checkSelection(selection);
    return selection;
  },

  getTargetFromFolder: function(aResource)
  {
    var index = parseInt(this.getProperty(aResource, RDF_NS+"nextVal"));
    if (isNaN(index))
      return {parent: null, index: -1};
    else
      return {parent: aResource, index: index};
  },

  getSelectionFromResource: function (aItem, aParent)
  {
    var selection    = {};
    selection.length = 1;
    selection.item   = [aItem  ];
    selection.parent = [aParent];
    this.checkSelection(selection);
    return selection;
  },

  createBookmark: function (aName, aURL, aCharSet, aDefaultName)
  {
    if (!aName) {
      // look up in the history ds to retrieve the name
      var rSource = RDF.GetResource(aURL);
      var HISTDS  = RDF.GetDataSource("rdf:history");
      var nameArc = RDF.GetResource(NC_NS+"Name");
      var rName   = HISTDS.GetTarget(rSource, nameArc, true);
      aName       = rName ? rName.QueryInterface(kRDFLITIID).Value : aDefaultName;
      if (!aName)
        aName = aURL;
    }
    if (!aCharSet) {
      var fw = document.commandDispatcher.focusedWindow;
      if (fw)
        aCharSet = fw.document.characterSet;
    }
    return BMSVC.createBookmark(aName, aURL, null, null, aCharSet);
  },

  flushDataSource: function ()
  {
    var remoteDS = BMDS.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
    setTimeout(function () {remoteDS.Flush()}, 100);
  },

  addBookmarkForTabBrowser: function( aTabBrowser, aSelect )
  {
    var tabsInfo = [];
    var currentTabInfo = { name: "", url: "", charset: null };

    const activeBrowser = aTabBrowser.selectedBrowser;
    const browsers = aTabBrowser.browsers;
    for (var i = 0; i < browsers.length; ++i) {
      var webNav = browsers[i].webNavigation;
      var url = webNav.currentURI.spec;
      var name = "";
      var charset;
      try {
        var doc = new XPCNativeWrapper(webNav.document, "title", "characterSet");
        name = doc.title || url;
        charset = doc.characterSet;
      } catch (e) {
        name = url;
      }

      tabsInfo[i] = { name: name, url: url, charset: charset };

      if (browsers[i] == activeBrowser)
        currentTabInfo = tabsInfo[i];
    }

    openDialog("chrome://communicator/content/bookmarks/addBookmark.xul", "",
               "centerscreen,chrome,dialog=yes,resizable=yes,dependent",
               currentTabInfo.name, currentTabInfo.url, null,
               currentTabInfo.charset, "addGroup" + (aSelect ? ",group" : ""), tabsInfo);
  },

  addBookmarkForBrowser: function (aDocShell, aShowDialog)
  {
    // Bug 52536: We obtain the URL and title from the nsIWebNavigation 
    //            associated with a <browser/> rather than from a DOMWindow.
    //            This is because when a full page plugin is loaded, there is
    //            no DOMWindow (?) but information about the loaded document
    //            may still be obtained from the webNavigation. 
    var url = aDocShell.currentURI.spec;
    var title, docCharset = null;
    try {
      var doc = new XPCNativeWrapper(aDocShell.document, "title", "characterSet");
      title = doc.title || url;
      docCharset = doc.characterSet;
    }
    catch (e) {
      title = url;
    }

    this.addBookmark(url, title, docCharset, aShowDialog);
  }, 

  // should update the caller, aShowDialog is no more necessary
  addBookmark: function (aURL, aTitle, aCharset, aShowDialog)
  {                                                                             
    if (aCharset === undefined) {
      var fw = document.commandDispatcher.focusedWindow;
      aCharset = fw.document.characterSet;
    }

    if (aShowDialog) {
      openDialog("chrome://communicator/content/bookmarks/addBookmark.xul", "",
                 "centerscreen,chrome,dialog=yes,resizable=yes,dependent", aTitle, aURL, null, aCharset);
    }
    else {
      // User has elected to override the file dialog and always file bookmarks
      // into the default bookmark folder.
      BMSVC.addBookmarkImmediately(aURL, aTitle, kBMSVCIID.BOOKMARK_DEFAULT_TYPE, aCharset);
    }
  },

  shouldLoadTabInBackground: function(aEvent)
  {
    var loadInBackground = PREF.getBoolPref("browser.tabs.loadInBackground");
    if (aEvent.shiftKey)
      loadInBackground = !loadInBackground;
    return loadInBackground;
  },

  getBrowserTargetFromEvent: function (aEvent)
  {
    if (!aEvent)
      return null;

    switch (aEvent.type) {
    case "click":
    case "dblclick":
      if (aEvent.button > 1)
        return null;

      if (aEvent.button != 1 && !aEvent.metaKey && !aEvent.ctrlKey)
        return "current";

      break;
    case "keypress":
    case "command":
      if (!aEvent.metaKey && !aEvent.ctrlKey)
        return "current";

      break;
    default:
      return null;
    }

    if (PREF && PREF.getBoolPref("browser.tabs.opentabfor.middleclick"))
      return this.shouldLoadTabInBackground(aEvent) ? "tab_background" : "tab";

    if (PREF && PREF.getBoolPref("middlemouse.openNewWindow"))
      return "window";

    return null;
  },

  loadBookmarkBrowser: function (aEvent, aDS)
  {
    var target = BookmarksUtils.getBrowserTargetFromEvent(aEvent);
    if (!target)
      return;

    var rSource   = RDF.GetResource(aEvent.target.id);
    var selection = BookmarksUtils.getSelectionFromResource(rSource);
    BookmarksCommand.openBookmark(selection, target, aDS)
  }
}


function BookmarkTransaction()
{
}

BookmarkTransaction.prototype = {
  BATCH_LIMIT : 8,
  RDFC        : null,
  BMDS        : null,

  beginUpdateBatch: function()
  {
    if (this.item.length > this.BATCH_LIMIT) {
      this.BMDS.beginUpdateBatch();
    }
  },

  endUpdateBatch: function()
  {
    if (this.item.length > this.BATCH_LIMIT) {
      this.BMDS.endUpdateBatch();
    }
  },

  // nsITransaction method stubs
  doTransaction: function() {},
  undoTransaction: function() {},
  redoTransaction: function() { this.doTransaction(); },
  get isTransient() { return false; },
  merge: function(aTransaction) { return false; },

  // debugging helper
  get wrappedJSObject() { return this; }
}

function BookmarkInsertTransaction (aAction)
{
  this.type    = "insert";
  this.action  = aAction;
  this.item    = null;
  this.parent  = null;
  this.index   = null;
  this.isValid = null;
}

BookmarkInsertTransaction.prototype =
{
  __proto__: BookmarkTransaction.prototype,

  doTransaction: function ()
  {
    this.beginUpdateBatch();
    for (var i=0; i<this.item.length; ++i) {
      if (this.isValid[i]) {
        this.RDFC.Init(this.BMDS, this.parent[i]);
        this.RDFC.InsertElementAt(this.item[i], this.index[i], true);
      }
    }
    this.endUpdateBatch();
  },

  undoTransaction: function ()
  {
    this.beginUpdateBatch();
    // XXXvarga Can't use |RDFC| here because it's being "reused" elsewhere.
    var container = Components.classes[kRDFCContractID].createInstance(kRDFCIID);
    for (var i=this.item.length-1; i>=0; i--) {
      if (this.isValid[i]) {
        container.Init(this.BMDS, this.parent[i]);
        container.RemoveElementAt(this.index[i], true);
      }
    }
    this.endUpdateBatch();
  }

}

function BookmarkRemoveTransaction (aAction)
{
  this.type    = "remove";
  this.action  = aAction;
  this.item    = null;
  this.parent  = null;
  this.index   = null;
  this.isValid = null;
}

BookmarkRemoveTransaction.prototype =
{
  __proto__: BookmarkTransaction.prototype,

  doTransaction: function ()
  {
    this.beginUpdateBatch();
    for (var i=0; i<this.item.length; ++i) {
      if (this.isValid[i]) {
        this.RDFC.Init(this.BMDS, this.parent[i]);
        this.RDFC.RemoveElementAt(this.index[i], false);
      }
    }
    this.endUpdateBatch();
  },

  undoTransaction: function ()
  {
    this.beginUpdateBatch();
    for (var i=this.item.length-1; i>=0; i--) {
      if (this.isValid[i]) {
        this.RDFC.Init(this.BMDS, this.parent[i]);
        this.RDFC.InsertElementAt(this.item[i], this.index[i], false);
      }
    }
    this.endUpdateBatch();
  }

}

function BookmarkMoveTransaction (aAction, aSelection, aTarget)
{
  this.type      = "move";
  this.action    = aAction;
  this.selection = aSelection;
  this.target    = aTarget;
  this.isValid   = aSelection.isValid;
}

BookmarkMoveTransaction.prototype =
{
  __proto__: BookmarkTransaction.prototype,

  beginUpdateBatch: function()
  {
    if (this.selection.length > this.BATCH_LIMIT) {
      this.BMDS.beginUpdateBatch();
    }
  },

  endUpdateBatch: function()
  {
    if (this.selection.length > this.BATCH_LIMIT) {
      this.BMDS.endUpdateBatch();
    }
  },

  doTransaction: function ()
  {
    this.beginUpdateBatch();
    BookmarksUtils.removeSelection("move", this.selection);
    BookmarksUtils.insertSelection("move", this.selection, this.target);
    this.endUpdateBatch();
  },

  redoTransaction     : function () {}

}

function BookmarkImportTransaction (aAction)
{
  this.type    = "import";
  this.action  = aAction;
  this.item    = [];
  this.parent  = [];
  this.index   = [];
  this.isValid = [];
}

BookmarkImportTransaction.prototype =
{
  __proto__: BookmarkTransaction.prototype,

  undoTransaction: function ()
  {
    this.beginUpdateBatch();
    for (var i=this.item.length-1; i>=0; i--) {
      if (this.isValid[i]) {
        this.RDFC.Init(this.BMDS, this.parent[i]);
        this.RDFC.RemoveElementAt(this.index[i], true);
      }
    }
    this.endUpdateBatch();
  },
   
  redoTransaction: function ()
  {
    this.beginUpdateBatch();
    for (var i=0; i<this.item.length; ++i) {
      if (this.isValid[i]) {
        this.RDFC.Init(this.BMDS, this.parent[i]);
        this.RDFC.InsertElementAt(this.item[i], this.index[i], true);
      }
    }
    this.endUpdateBatch();
  }

}
