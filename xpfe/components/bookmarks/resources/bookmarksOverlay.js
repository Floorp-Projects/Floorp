/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

var NC_NS  = "http://home.netscape.com/NC-rdf#";
var RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const NC_NS_CMD = NC_NS + "command?cmd=";

/** 
 * XXX - 02/03/2001 - Work In Progress
 *  - some of the methods here and in the derived classes in bookmarksTree.js
 *    and bookmarks.js can probably be broken out into a shared library for RDF
 *    utilities. So far the focus has been on getting this stuff to work, once 
 *    I'm done with that I'll look at what can be reorganized. 
 */

function NODE_ID (aElement)
{
  return aElement.getAttribute("ref") || aElement.id;
}

function LITERAL (aDB, aElement, aPropertyID)
{
  var RDF = BookmarksUIElement.prototype.RDF;
  var rSource = RDF.GetResource(NODE_ID(aElement));
  var rProperty = RDF.GetResource(aPropertyID);
  var node = aDB.GetTarget(rSource, rProperty, true);
  return node ? node.QueryInterface(Components.interfaces.nsIRDFLiteral).Value : "";
}

function BookmarksUIElement () { }
BookmarksUIElement.prototype = {
  _rdf: null,
  get RDF ()
  {
    if (!this._rdf) {
      const kRDFContractID = "@mozilla.org/rdf/rdf-service;1";
      const kRDFIID = Components.interfaces.nsIRDFService;
      this._rdf = Components.classes[kRDFContractID].getService(kRDFIID);
    }
    return this._rdf;
  },

  propertySet: function (sourceID, propertyID, newValue)
  {
    if (!newValue) return;
    const kRDFContractID = "@mozilla.org/rdf/rdf-service;1";
    const kRDFIID = Components.interfaces.nsIRDFService;
    const kRDF = Components.classes[kRDFContractID].getService(kRDFIID);
    // need to shuffle this into an API. 
    const kBMDS = kRDF.GetDataSource("rdf:bookmarks");
    const krProperty = kRDF.GetResource(propertyID);
    const krItem = kRDF.GetResource(sourceID);
    var rCurrValue = kBMDS.GetTarget(krItem, krProperty, true);
    const krNewValue = kRDF.GetLiteral(newValue);
    if (!rCurrValue)
      kBMDS.Assert(krItem, krProperty, krNewValue, true);
    else {
      rCurrValue = rCurrValue.QueryInterface(Components.interfaces.nsIRDFLiteral);
      if (rCurrValue.Value != newValue) 
        kBMDS.Change(krItem, krProperty, rCurrValue, krNewValue);
    }
  },

  listener: null,  
  exDB: ["rdf:files", "rdf:httpindex"],
  setUpCompositeDataSource: function ()
  {
    if (!this.listener) 
      this.listener = new AggregationPrefListener ();
      
    const kPrefSvcContractID = "@mozilla.org/preferences;1";
    const kPrefSvcIID = Components.interfaces.nsIPref;
    const kPrefSvc = Components.classes[kPrefSvcContractID].getService(kPrefSvcIID);
    var bUseExtendedDataViews = false;
    try {
      bUseExtendedDataViews = kPrefSvc.GetBoolPref("browser.bookmarks.show_extended_data");
    }
    catch (e) {
    }
    var element = document.getElementById(this.id);
    var db = element.database;

    for (var i = 0; i < this.exDB.length; ++i) {
      dump("** txxsdflks = " + this.exDB[i] + "\n");
      db[bUseExtendedDataViews ? "AddDataSource" : "RemoveDataSource"](this.RDF.GetDataSource(this.exDB[i]));
    }
    element.builder.rebuild();
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // Fill a context menu popup with menuitems that are appropriate for the current
  // selection.
  createContextMenu: function (aEvent)
  {
    var popup = aEvent.target;
    
    // clear out the old context menu contents (if any)
    while (popup.hasChildNodes()) 
      popup.removeChild(popup.firstChild);
    
    var popupNode = document.popupNode;
    
    if (!("findRDFNode" in this))
      throw "Clients must implement findRDFNode!";
    var itemNode = this.findRDFNode(popupNode, true);
    if (!itemNode) return false;

    if (!("getContextSelection" in this)) 
      throw "Clients must implement getContextSelection!";
    var selection = this.getContextSelection (itemNode);
    var commonCommands = [];
    for (var i = 0; i < selection.length; ++i) {
      var nodeURI = NODE_ID(selection[i]);
      var commands = this.getAllCmds(nodeURI);
      commands = this.flattenEnumerator(commands);
      if (!commonCommands.length) commonCommands = commands;
      commonCommands = this.findCommonNodes(commands, commonCommands);
    }

    if (!commonCommands.length) return false;
    
    // Now that we should have generated a list of commands that is valid
    // for the entire selection, build a context menu.
    for (i = 0; i < commonCommands.length; ++i) {
      const kXULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
      var currCommand = commonCommands[i].QueryInterface(Components.interfaces.nsIRDFResource).Value;
      var element = null;
      if (currCommand != NC_NS_CMD + "separator") {
        var commandName = this.getCommandName(currCommand);
        element = this.createMenuItem(commandName, currCommand, itemNode);
      }
      else if (i != 0 && i < commonCommands.length-1) {
        // Never append a separator as the first or last element in a context
        // menu.
        element = document.createElementNS(kXULNS, "menuseparator");
      }
      
      if (element) 
        popup.appendChild(element);
    }
    return true;
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
  getAllCmds: function (aNodeID)
  {
    var type = this.resolveType(aNodeID);
    var commands = [];
    switch (type) {
    case "http://home.netscape.com/NC-rdf#BookmarkSeparator":
      commands = ["find", "separator", "cut", "copy", "paste", 
                  "delete"];
      break;
    case "http://home.netscape.com/NC-rdf#Bookmark":
      commands = ["open", "find", "separator", "cut", "copy", "paste", 
                  "delete", "separator", "rename", "separator", 
                  "properties"];
      break;
    case "http://home.netscape.com/NC-rdf#Folder":
      commands = ["openfolder", "openfolderinnewwindow", "find", "separator", 
                  "cut", "copy", "paste", "delete", "separator", "rename", 
                  "separator", "newfolder", "separator", "properties"];
      break;
    case "http://home.netscape.com/NC-rdf#IEFavoriteFolder":
      commands = ["open", "find", "separator", "copy", "separator", "rename", 
                  "separator", "properties"];
      break;
    case "http://home.netscape.com/NC-rdf#IEFavorite":
      commands = ["open", "find", "separator", "copy"];
      break;
    case "http://home.netscape.com/NC-rdf#FileSystemObject":
      commands = ["open", "find", "separator", "copy"];
      break;
    default: 
      var source = this.RDF.GetResource(aNodeID);
      return this.db.GetAllCmds(source);
    }
    return new CommandArrayEnumerator(commands);
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // Retrieve the human-readable name for a particular command. Used when 
  // manufacturing a UI to invoke commands.
  getCommandName: function (aCommand) 
  {
    var cmdName = aCommand.substring(NC_NS_CMD.length);
    try {
      // Note: this will succeed only if there's a string in the bookmarks
      //       string bundle for this command name. Otherwise, <xul:stringbundle/>
      //       will throw, we'll catch & stifle the error, and look up the command
      //       name in the datasource. 
      return this.getLocaleString ("cmd_" + cmdName);
    }
    catch (e) {
    }      
    const rName = this.RDF.GetResource(NC_NS + "Name");
    const rSource = this.RDF.GetResource(aNodeID);
    return this.db.GetTarget(rSource, rName, true).Value;
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // Perform a command based on a UI event. XXX - work to do here. 
  preExecCommand: function (aEvent)
  {
    var commandID = aEvent.target.getAttribute("command");
    if (!commandID) return;
    goDoCommand("cmd_" + commandID.substring(NC_NS_CMD.length));
  },
  
  execCommand: function (aCommandID) 
  {
    var selection = this.getSelection ();
    if (selection.length >= 1) 
      var selectedItem = selection[0];
    switch (aCommandID) {
    case "open":
      this.openRDFNode(selectedItem);
      break;
    case "openfolder":
      this.commands.openFolder(selectedItem);
      break;
    case "openfolderinnewwindow":
      this.openFolderInNewWindow(selectedItem);
      break;
    case "rename":
      // XXX - this is SO going to break if we ever do column re-ordering.
      this.commands.editCell(selectedItem, 0);
      break;
    case "editurl":
      this.commands.editCell(selectedItem, 1);
      break;
    case "setnewbookmarkfolder":
    case "setpersonaltoolbarfolder":
    case "setnewsearchfolder":
      var args = [];
      gBookmarksShell.doBookmarksCommand(NODE_ID(selectedItem), NC_NS_CMD + aCommandID, []);
      // XXX - The containing node seems to be closed here and the 
      //       focus/selection is destroyed.
      this.selectElement(selectedItem);
      break;
    case "properties":
      this.showPropertiesForNode(selectedItem);
      break;
    case "find":
      this.findInBookmarks();
      break;
    case "cut":
      this.copySelection (selection);
      this.deleteSelection (selection);
      break;
    case "copy":
      this.copySelection (selection);
      break;
    case "paste":
      this.paste (selection);
      break;
    case "delete":
      this.deleteSelection (selection);
      break;
    case "newfolder":
      var nfseln = this.getBestItem ();
      this.commands.createBookmarkItem("folder", nfseln);
      break;
    case "newbookmark":
      var folder = this.getSelectedFolder();
      openDialog("chrome://communicator/content/bookmarks/addBookmark.xul", "", 
                 "centerscreen,chrome,dialog=no,resizable=no", null, null, folder, null);
      break;
    case "newseparator":
      nfseln = this.getBestItem ();
      this.commands.createSeparator(nfseln);
      break;
    case "import":
    case "export":
      const isImport = aCommandID == "import";
      try {
        const kFilePickerContractID = "@mozilla.org/filepicker;1";
        const kFilePickerIID = Components.interfaces.nsIFilePicker;
        const kFilePicker = Components.classes[kFilePickerContractID].createInstance(kFilePickerIID);
        
        const kTitle = this.getLocaleString(isImport ? "SelectImport": "EnterExport");
        kFilePicker.init(window, kTitle, kFilePickerIID[isImport ? "modeOpen" : "modeSave"]);
        kFilePicker.appendFilters(kFilePickerIID.filterHTML | kFilePickerIID.filterAll);
        if (!isImport) kFilePicker.defaultString = "bookmarks.html";
        if (kFilePicker.show() != kFilePickerIID.returnCancel) {
          var fileName = kFilePicker.fileURL.spec;
          if (!fileName) break;
        }
        else break;
      }
      catch (e) {
        break;
      }
      var seln = this.getBestItem();
      var args = [{ property: NC_NS + "URL", literal: fileName}];
      gBookmarksShell.doBookmarksCommand(NODE_ID(seln), NC_NS_CMD + aCommandID, args);
      break;
    }
  },
  
  ///////////////////////////////////////////////////////////////////////////
  // Execute a command with the given source and arguments
  doBookmarksCommand: function (aSourceURI, aCommand, aArgumentsArray)
  {
    var rCommand = this.RDF.GetResource(aCommand);
  
    var kSuppArrayContractID = "@mozilla.org/supports-array;1";
    var kSuppArrayIID = Components.interfaces.nsISupportsArray;
    var sourcesArray = Components.classes[kSuppArrayContractID].createInstance(kSuppArrayIID);
    if (aSourceURI) {
      var rSource = this.RDF.GetResource(aSourceURI);
      sourcesArray.AppendElement (rSource);
    }
  
    var argsArray = Components.classes[kSuppArrayContractID].createInstance(kSuppArrayIID);
    for (var i = 0; i < aArgumentsArray.length; ++i) {
      var rArc = this.RDF.GetResource(aArgumentsArray[i].property);
      argsArray.AppendElement(rArc);
      var rValue = null;
      if ("resource" in aArgumentsArray[i]) 
        rValue = this.RDF.GetResource(aArgumentsArray[i].resource);
      else
        rValue = this.RDF.GetLiteral(aArgumentsArray[i].literal);
      argsArray.AppendElement(rValue);
    }

    // Exec the command in the Bookmarks datasource. 
    const kBMDS = this.RDF.GetDataSource("rdf:bookmarks");
    kBMDS.DoCommand(sourcesArray, rCommand, argsArray);
  },

  openFolderInNewWindow: function (aSelectedItem)
  {
    openDialog("chrome://communicator/content/bookmarks/bookmarks.xul", 
               "", "chrome,all,dialog=no", NODE_ID(aSelectedItem));
  },
  
  copySelection: function (aSelection)
  {
    const kSuppArrayContractID = "@mozilla.org/supports-array;1";
    const kSuppArrayIID = Components.interfaces.nsISupportsArray;
    var itemArray = Components.classes[kSuppArrayContractID].createInstance(kSuppArrayIID);

    const kSuppWStringContractID = "@mozilla.org/supports-wstring;1";
    const kSuppWStringIID = Components.interfaces.nsISupportsWString;
    var bmstring = Components.classes[kSuppWStringContractID].createInstance(kSuppWStringIID);
    var unicodestring = Components.classes[kSuppWStringContractID].createInstance(kSuppWStringIID);
    var htmlstring = Components.classes[kSuppWStringContractID].createInstance(kSuppWStringIID);
  
    var sBookmarkItem = ""; var sTextUnicode = ""; var sTextHTML = "";
    for (var i = 0; i < aSelection.length; ++i) {
      var url = LITERAL(this.db, aSelection[i], NC_NS + "URL");
      var name = LITERAL(this.db, aSelection[i], NC_NS + "Name");
      sBookmarkItem += NODE_ID(aSelection[i]) + "\n";
      sTextUnicode += url + "\n";
      sTextHTML += "<A HREF=\"" + url + "\">" + name + "</A>";
    }    
    
    const kXferableContractID = "@mozilla.org/widget/transferable;1";
    const kXferableIID = Components.interfaces.nsITransferable;
    var xferable = Components.classes[kXferableContractID].createInstance(kXferableIID);

    xferable.addDataFlavor("moz/bookmarkclipboarditem");
    bmstring.data = sBookmarkItem;
    xferable.setTransferData("moz/bookmarkclipboarditem", bmstring, sBookmarkItem.length*2)
    
    xferable.addDataFlavor("text/html");
    htmlstring.data = sTextHTML;
    xferable.setTransferData("text/html", htmlstring, sTextHTML.length*2)
    
    xferable.addDataFlavor("text/unicode");
    unicodestring.data = sTextHTML;
    xferable.setTransferData("text/unicode", unicodestring, sTextUnicode.length*2)
    
    const kClipboardContractID = "@mozilla.org/widget/clipboard;1";
    const kClipboardIID = Components.interfaces.nsIClipboard;
    var clipboard = Components.classes[kClipboardContractID].getService(kClipboardIID);
    clipboard.setData(xferable, null, kClipboardIID.kGlobalClipboard);
  },

  paste: function (aSelection)
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
    var data = { };
    var length = { };
    xferable.getAnyTransferData(flavour, data, length);
    var nodes = []; var names = [];
    data = data.value.QueryInterface(Components.interfaces.nsISupportsWString).data;
    switch (flavour.value) {
    case "moz/bookmarkclipboarditem":
      nodes = data.split("\n");
      break;
    case "text/x-moz-url":
      var ix = data.value.indexOf("\n");
      nodes.push(data.substring(0, ix != -1 ? ix : data.length));
      names.push(data.substring(ix));
      break;
    default: 
      return;
    }
    
    const lastSelected = aSelection[aSelection.length-1];  
    const kParentNode = this.resolvePasteFolder(aSelection);
    const krParent = this.RDF.GetResource(NODE_ID(kParentNode));
    const krSource = this.RDF.GetResource(NODE_ID(lastSelected));
    
    const kRDFCContractID = "@mozilla.org/rdf/container;1";
    const kRDFCIID = Components.interfaces.nsIRDFContainer;
    const ksRDFC = Components.classes[kRDFCContractID].getService(kRDFCIID);
    const kBMDS = this.RDF.GetDataSource("rdf:bookmarks");

    if ("beginBatch" in this && nodes.length > 1)
      this.beginBatch();
    var additiveFlag = false;
    for (var i = 0; i < nodes.length; ++i) {
      if (!nodes[i]) continue;
      var rCurrent = this.RDF.GetResource(nodes[i]);
      const krTypeProperty = this.RDF.GetResource(RDF_NS + "type");
      var rType = this.db.GetTarget(rCurrent, krTypeProperty, true);
      try {
        rType = rType.QueryInterface(Components.interfaces.nsIRDFResource);
      }
      catch (e) {
        rType = rType.QueryInterface(Components.interfaces.nsIRDFLiteral);
      }

      // If the node is a folder, then we need to create a new anonymous 
      // resource and copy all the arcs over.
      if (rType.Value == NC_NS + "Folder")
        rCurrent = this.cloneFolder(rCurrent, krParent, krSource);

      // If we are given names, this implies that the nodes do not already 
      // exist in the graph, and we need to create some additional information
      // for them. 
      if (names.length) {
        const krName = this.RDF.GetResource(names[i]);
        const krNameProperty = this.RDF.GetResource(NC_NS + "Name");
        const krBookmark = this.RDF.GetResource(NC_NS + "Bookmark");
        this.db.Assert(rCurrent, krNameProperty, krName, true);
        this.db.Assert(rCurrent, krTypeProperty, krBookmark, true);
      }
      ksRDFC.Init(kBMDS, krParent);
      ix = ksRDFC.IndexOf(krSource);
      if (ix != -1) 
        ksRDFC.InsertElementAt(rCurrent, ix+1, true);
      else
        ksRDFC.AppendElement(rCurrent);
      this.selectFolderItem(krSource.Value, rCurrent.Value, additiveFlag);
      if (!additiveFlag) additiveFlag = true;
    }
    if ("endBatch" in this && nodes.length > 1)
      this.endBatch();
  },
  
  cloneFolder: function (aFolder, aParent, aRelativeItem) 
  {
    const kBMDS = this.RDF.GetDataSource("rdf:bookmarks");
    
    const krNameArc = this.RDF.GetResource(NC_NS + "Name");
    var rName = kBMDS.GetTarget(aFolder, krNameArc, true);
    rName = rName.QueryInterface(Components.interfaces.nsIRDFLiteral);
    
    const kBMSvcContractID = "@mozilla.org/browser/bookmarks-service;1";
    const kBMSvcIID = Components.interfaces.nsIBookmarksService;
    const kBMSvc = Components.classes[kBMSvcContractID].getService(kBMSvcIID);
    const krNewFolder = kBMSvc.CreateFolder(rName.Value, aRelativeItem, aParent);
    
    // Now need to append kiddies. 
    try {
      const kRDFCContractID = "@mozilla.org/rdf/container;1";
      const kRDFCIID = Components.interfaces.nsIRDFContainer;
      const ksRDFC = Components.classes[kRDFCContractID].getService(kRDFCIID);
      ksRDFC.Init(kBMDS, aFolder);

      const kElts = ksRDFC.GetElements();
      ksRDFC.Init(kBMDS, krNewFolder);
      while (kElts.hasMoreElements()) {
        var curr = kElts.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
        ksRDFC.AppendElement(curr);
      }
    }
    catch (e) {
    }
    return krNewFolder;
  },
  
  canPaste: function ()
  {
    const kClipboardContractID = "@mozilla.org/widget/clipboard;1";
    const kClipboardIID = Components.interfaces.nsIClipboard;
    var clipboard = Components.classes[kClipboardContractID].getService(kClipboardIID);
    const kSuppArrayContractID = "@mozilla.org/supports-array;1";
    const kSuppArrayIID = Components.interfaces.nsISupportsArray;
    var flavourArray = Components.classes[kSuppArrayContractID].createInstance(kSuppArrayIID);
    const kSuppStringContractID = "@mozilla.org/supports-string;1";
    const kSuppStringIID = Components.interfaces.nsISupportsString;
    
    var flavours = ["moz/bookmarkclipboarditem", "text/x-moz-url"];
    for (var i = 0; i < flavours.length; ++i) {
      const kSuppString = Components.classes[kSuppStringContractID].createInstance(kSuppStringIID);
      kSuppString.data = flavours[i];
      flavourArray.AppendElement(kSuppString);
    }
    var hasFlavors = clipboard.hasDataMatchingFlavors(flavourArray, kClipboardIID.kGlobalClipboard);
    return hasFlavors;
  },
  
  deleteSelection: function (aSelection)
  {
    const kRDFCContractID = "@mozilla.org/rdf/container;1";
    const kRDFCIID = Components.interfaces.nsIRDFContainer;
    const ksRDFC = Components.classes[kRDFCContractID].getService(kRDFCIID);

    var nextElement;
    var count = 0;

    var selectionLength = aSelection.length;
    if ("beginBatch" in this && selectionLength > 1)
      this.beginBatch();
    while (aSelection.length && aSelection[count]) {
      const currParent = this.findRDFNode(aSelection[count], false);
      const kSelectionURI = NODE_ID(aSelection[count]);

      // Disallow the removal of certain 'special' nodes
      if (kSelectionURI == "NC:BookmarksRoot" || 
          kSelectionURI == "NC:IEFavoritesRoot") {
        ++count;
        continue;
      }
        
      const krParent = this.RDF.GetResource(NODE_ID(currParent));
      const krNode = this.RDF.GetResource(kSelectionURI);
      const kBMDS = this.RDF.GetDataSource("rdf:bookmarks");

      ksRDFC.Init(kBMDS, krParent);
      nextElement = this.getNextElement(aSelection[count]);
      ksRDFC.RemoveElement(krNode, true);
    }
    if ("beginBatch" in this && selectionLength > 1)
      this.endBatch();
      
    this.selectElement(nextElement);
  },

  moveBookmark: function (aBookmarkURI, aFromFolderURI, aToFolderURI)
  {
    const kBMDS = this.RDF.GetDataSource("rdf:bookmarks");
    const kRDFCContractID = "@mozilla.org/rdf/container;1";
    const kRDFCIID = Components.interfaces.nsIRDFContainer;
    const kRDFC = Components.classes[kRDFCContractID].getService(kRDFCIID);
    const krSrc = this.RDF.GetResource(aBookmarkURI);
    const krOldParent = this.RDF.GetResource(aFromFolderURI);
    const krNewParent = this.RDF.GetResource(aToFolderURI);
    kRDFC.Init(kBMDS, krNewParent);
    kRDFC.AppendElement(krSrc);
    kRDFC.Init(kBMDS, krOldParent);
    kRDFC.RemoveElement(krSrc, true);
  },
  
  openRDFNode: function (aRDFNode)
  {
    var urlValue = LITERAL(this.db, aRDFNode, NC_NS + "URL");
    // Ignore "NC:" and empty urls.
    if (urlValue.substring(0,3) == "NC:" || !urlValue) return;
    openDialog (getBrowserURL(), "_blank", "chrome,all,dialog=no", urlValue);
  },

  open: function (aEvent) 
  { 
    if (!this.isValidOpenEvent(aEvent))   
      return;

    var rdfNode = this.findRDFNode(aEvent.target, true);
    if (rdfNode.getAttribute("container") == "true") 
      return;
  
    var urlValue = LITERAL(this.db, rdfNode, NC_NS + "URL");
    
    // Ignore "NC:" and empty urls.
    if (urlValue.substring(0,3) == "NC:" || !urlValue) return;
    
    if (aEvent.altKey)   
      this.showPropertiesForNode (rdfNode);
    else
      openDialog (getBrowserURL(), "_blank", "chrome,all,dialog=no", urlValue);
      
    aEvent.preventBubble();
  },

  showPropertiesForNode: function (aBookmarkItem) 
  {
    if (aBookmarkItem.getAttribute("type") != NC_NS + "BookmarkSeparator") 
      openDialog("chrome://communicator/content/bookmarks/bm-props.xul",
                 "", "centerscreen,chrome,dialog=no,resizable=no", 
                 NODE_ID(aBookmarkItem));
  },

  findInBookmarks: function ()
  {
    openDialog("chrome://communicator/content/bookmarks/bm-find.xul",
               "FindBookmarksWindow",
               "centerscreen,chrome,resizable");
  },

  getLocaleString: function (aStringKey)
  {
    var bundle = document.getElementById("bookmarksbundle");
    return bundle.getString (aStringKey);
  },
  
  flushDataSource: function ()
  {
    const kBMDS = this.RDF.GetDataSource("rdf:bookmarks");
    var remoteDS = kBMDS.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
    remoteDS.Flush();
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // Determine the rdf:type property for the given resource.
  resolveType: function (aID)
  {
    const krType = this.RDF.GetResource(RDF_NS + "type");
    const krElement = this.RDF.GetResource(aID);
    const type = gBookmarksShell.db.GetTarget(krElement, krType, true);
    try {
      return type.QueryInterface(Components.interfaces.nsIRDFResource).Value;
    }
    catch (e) {
      return type.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    }    
  }
};

function CommandArrayEnumerator (aCommandArray)
{
  this._inner = [];
  const kRDFContractID = "@mozilla.org/rdf/rdf-service;1";
  const kRDFIID = Components.interfaces.nsIRDFService;
  const RDF = Components.classes[kRDFContractID].getService(kRDFIID);
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
  addBookmarkToWindow: function (aWindow)
  {
    var url = aWindow.location.href;
    var title = aWindow.document.title || url;
    var docCharset = aWindow.document.characterSet;

    const kPrefContractID = "@mozilla.org/preferences;1";
    const kPrefIID = Components.interfaces.nsIPref;
    const kPrefSvc = Components.classes[kPrefContractID].getService(kPrefIID);
    var showDialog = true;
    try {
      showDialog = !kPrefSvc.GetBoolPref("browser.bookmarks.add_without_dialog");
    }
    catch (e) {
    }
    
    if (showDialog)
      openDialog("chrome://communicator/content/bookmarks/addBookmark.xul", "", 
                 "centerscreen,chrome,dialog=no,resizable=no", title, url, null, docCharset);
    else {
      // User has elected to override the file dialog and always file bookmarks
      // into the default bookmark folder. 
      const kBMSvcContractID = "@mozilla.org/browser/bookmarks-service;1";
      const kBMSvcIID = Components.interfaces.nsIBookmarksService;
      const kBMSvc = Components.classes[kBMSvcContractID].getService(kBMSvcIID);
      kBMSvc.AddBookmark(url, title, kBMSvcIID.BOOKMARK_DEFAULT_TYPE, docCharset);
    }
  }

};

function AggregationPrefListener () 
{ 
  const kPrefSvcContractID = "@mozilla.org/preferences;1";
  const kPrefSvcIID = Components.interfaces.nsIPref;
  const kPrefSvc = Components.classes[kPrefSvcContractID].getService(kPrefSvcIID);
  kPrefSvc.addObserver(this.pref, this);
}
AggregationPrefListener.prototype = {
  pref: "browser.bookmarks.show_extended_data",
  Observe: function (subject, topic, prefName) 
  {
    if (topic != "nsPref:changed") return;
    if (prefName != this.pref) return;
    gBookmarksShell.setUpCompositeDataSource();
  }
};
  
