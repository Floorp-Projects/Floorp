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

var gBookmarksShell = null; 
 
///////////////////////////////////////////////////////////////////////////////
// Tracks the selected item, the cell last clicked on, and the number of clicks
// given to it. Used to activate inline edit mode. 
var gSelectionTracker = { currentItem: null, currentCell: null, clickCount: 0 };

///////////////////////////////////////////////////////////////////////////////
// Class which defines methods for a bookmarks UI implementation based around
// a treeview. Subclasses BookmarksBase in bookmarksOverlay.js. Some methods 
// are required by the base class, others are for event handling. Window specific
// glue code should go into the BookmarksWindow class in bookmarks.js
function BookmarksTree (aID) 
{
  this.id = aID;
}

BookmarksTree.prototype = {
  __proto__: BookmarksUIElement.prototype,

  get tree ()
  {
    return document.getElementById(this.id);
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // This method constructs a menuitem for a context menu for the given command. 
  // This is implemented by the client so that it can intercept menuitem naming
  // as appropriate. 
  createMenuItem: function (aDisplayName, aCommandName, aItemNode)
  {
    const kXULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    var xulElement = document.createElementNS(kXULNS, "menuitem");
    xulElement.setAttribute("command", aCommandName);
    xulElement.setAttribute("observes", "cmd_" + aCommandName.substring(NC_NS_CMD.length));

    switch (aCommandName) {
    case NC_NS_CMD + "open":
      xulElement.setAttribute("value", aDisplayName);
      xulElement.setAttribute("default", "true");
      break;
    case NC_NS_CMD + "openfolder":
      aDisplayName = aItemNode.getAttribute("open") == "true" ? this.getLocaleString("cmd_openfolder2") : aDisplayName;
      xulElement.setAttribute("value", aDisplayName);
      xulElement.setAttribute("default", "true");
      break;
    case NC_NS_CMD + "renamebookmark":
      if (!document.popupNode.hasAttribute("type")) {
        xulElement.setAttribute("value", this.getLocaleString("cmd_renamebookmark2"));
        xulElement.setAttribute("command", (NC_NS_CMD + "editurl"));
      }
      else
        xulElement.setAttribute("value", aDisplayName);
      break;
    default:
      xulElement.setAttribute("value", aDisplayName);
      break;
    }    
    return xulElement;  
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // For the given selection, determines the element that should form the 
  // container to paste items into.
  resolvePasteFolder: function (aSelection)
  {
    const lastSelected = aSelection[aSelection.length-1];  
    if (lastSelected.getAttribute("container") == "true" &&
        lastSelected.getAttribute("open") == "true" && 
        aSelection.length == 1)
      return lastSelected;
    return this.findRDFNode(lastSelected, false);
  },
  
  // Command implementation
  commands: {
    openFolder: function (aSelectedItem)
    {
      if (aSelectedItem.getAttribute("open") == "true") 
        aSelectedItem.removeAttribute("open");
      else
        aSelectedItem.setAttribute("open", "true");
    },
    
    // Things Needed to Satisfy Mac Weenies:
    // 1) need to implement timed single click edit. This could be Hard. 
    // 2) need to implement some other method of key access apart from F2. 
    //    mpt claims that 'Cmd+U' is the excel equivalent. 
    editCell: function (aSelectedItem, aCell)
    {
      var editCell = aSelectedItem.firstChild.childNodes[aCell];
      if (editCell.getAttribute("editable") != "true") 
        return;
      var editColGroup = document.getElementById("theColumns");
      var count = 0;
      var property = "";
      for (var i = 0; i < editColGroup.childNodes.length; ++i) {
        var currCol = editColGroup.childNodes[i];
        if (count == aCell) {
          property = currCol.getAttribute("resource");
          break;
        }
        ++count;
        
        // Deal with interleaved column resizer splitters
        if (currCol.nextSibling.localName == "splitter") ++i;
      }
      
      if (property) {
        editCell.setMode("edit");
        editCell.addObserver(this.postModifyCallback, "accept",
                             [gBookmarksShell, NODE_ID(aSelectedItem), property]);
      }
    },

    ///////////////////////////////////////////////////////////////////////////
    // Called after an inline-edit cell has left inline-edit mode, and data
    // needs to be modified in the datasource.     
    postModifyCallback: function (aParams) 
    {
      var aShell = aParams[0];
      aShell.propertySet(aParams[1], aParams[2], aParams[3]); 
      gBookmarksShell.tree.focus();
      gBookmarksShell.tree.selectItem(document.getElementById(aParams[2]));
      gSelectionTracker.clickCount = 0;
    },

    ///////////////////////////////////////////////////////////////////////////
    // New Folder Creation
    // Strategy: create a dummy row with edit fields to harvest information
    //           from the user, then destroy these rows and create an item
    //           in its place.    
    
    ///////////////////////////////////////////////////////////////////////////
    // Edit folder name & update the datasource if name is valid
    onEditFolderName: function (aParams, aTopic)
    {
      var name = aParams[3];
      var shell = gBookmarksShell.commands; // suck
      var dummyItem = aParams[2];
      var relativeNode = aParams[1];
      var parentNode = gBookmarksShell.findRDFNode(relativeNode, false);

      if (!shell.validateNameAndTopic(name, aTopic, relativeNode, dummyItem))
        return;
      
      // If we're attempting to create a folder as a subfolder of an open folder,
      // we need to set the parentFolder to be relativeNode, which will be the 
      // parent of the new folder, rather than the parent of the relativeNode,
      // which will result in the folder being created in an incorrect position
      // (adjacent to the relativeNode). 
      var selKids = ContentUtils.childByLocalName(relativeNode, "treechildren");
      if (selKids && selKids.hasChildNodes() && selKids.lastChild == dummyItem)
        parentNode = relativeNode;

      var args = [{ property: NC_NS + "parent", 
                    resource: NODE_ID(parentNode) },
                  { property: NC_NS + "Name",
                    literal:  name }];

      const kBMDS = gBookmarksShell.RDF.GetDataSource("rdf:bookmarks");
      kBMDS.AddObserver(newFolderRDFObserver);
      gBookmarksShell.doBookmarksCommand(NODE_ID(relativeNode), 
                                         NC_NS_CMD + "newfolder", args);
      kBMDS.RemoveObserver(newFolderRDFObserver);
      var newFolderItem = document.getElementById(newFolderRDFObserver._newFolderURI);
      gBookmarksShell.tree.focus();
      gBookmarksShell.tree.selectItem(newFolderItem);
    },
    
    createSeparator: function (aSelectedItem)
    {
      var parentNode = gBookmarksShell.findRDFNode(aSelectedItem, false);
      var args = [{ property: NC_NS + "parent", 
                    resource: NODE_ID(parentNode) }];
      gBookmarksShell.doBookmarksCommand(NODE_ID(aSelectedItem), 
                                         NC_NS_CMD + "newseparator", args);
    },
    
    ///////////////////////////////////////////////////////////////////////////
    // Performs simple validation on what the user has entered:
    //  1) prevents entering an empty string
    //  2) in the case of a canceled operation, remove the dummy item and 
    //     restore selection.
    validateNameAndTopic: function (aName, aTopic, aOldSelectedItem, aDummyItem)
    {
      // Don't allow user to enter an empty string "";
      if (!aName) return false;
            
      // If the user hit escape, go no further. 
      if (aTopic == "reject") {
        if (aOldSelectedItem)
          gBookmarksShell.tree.selectItem(aOldSelectedItem);
        aDummyItem.parentNode.removeChild(aDummyItem);
        return false;
      }
      return true;
    },
    
    ///////////////////////////////////////////////////////////////////////////
    // Creates a dummy item that can be placed in edit mode to retrieve data 
    // to create new bookmarks/folders.
    createBookmarkItem: function (aMode, aSelectedItem) 
    {
      aSelectedItem.removeAttribute("selected");
      var dummyItem = aSelectedItem.cloneNode(true);
      dummyItem.removeAttribute("id");
      dummyItem.removeAttribute("selected");
      if (aMode == "folder")
        dummyItem.setAttribute("container", "true");
      for (var i = 0; i < dummyItem.firstChild.childNodes.length; ++i) 
        dummyItem.firstChild.childNodes[i].removeAttribute("value");
      var editCell = dummyItem.firstChild.firstChild;
      editCell.setAttribute("value", 
                            gBookmarksShell.getLocaleString(aMode == "folder" ? "ile_newfolder" : 
                                                                                "ile_newbookmark"));
      editCell.setAttribute("type", NC_NS + (aMode == "folder" ? "Folder" : "Bookmark"));
      var relativeNode = aSelectedItem;
      if (dummyItem.getAttribute("container") == "true") {
        for (i = 0; i < dummyItem.childNodes.length; ++i) {
          if (dummyItem.childNodes[i].localName == "treechildren")
            dummyItem.removeChild(dummyItem.childNodes[i]);
        }
        
        if (dummyItem.getAttribute("open") == "true") {
          var treechildren = ContentUtils.childByLocalName(aSelectedItem, "treechildren");
          if (!treechildren) {
            const kXULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
            treechildren = document.createElementNS(kXULNS, "treechildren");
            aSelectedItem.appendChild(treechildren);
          }
          // Insert new item after last item. 
          treechildren.appendChild(dummyItem);
        }
        else {
          if (aSelectedItem.nextSibling)
            aSelectedItem.parentNode.insertBefore(dummyItem, aSelectedItem.nextSibling);
          else 
            aSelectedItem.parentNode.appendChild(dummyItem);
        }
      }
      else {
        if (aSelectedItem.nextSibling)
          aSelectedItem.parentNode.insertBefore(dummyItem, aSelectedItem.nextSibling);
        else 
          aSelectedItem.parentNode.appendChild(dummyItem);
      }
      editCell.setMode("edit");
      var fn = aMode == "folder" ? this.onEditFolderName : this.onEditBookmarkName;
      editCell.addObserver(fn, "accept", [editCell, relativeNode, dummyItem]);
      editCell.addObserver(fn, "reject", [editCell, relativeNode, dummyItem]);
    }
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // Evaluates an event to determine whether or not it affords opening a tree
  // item. Typically, this is when the left mouse button is used, and provided
  // the click-rate matches that specified by our owning tree class. For example,
  // some trees open an item when double clicked (bookmarks/history windows) and
  // others on a single click (sidebar panels).
  isValidOpenEvent: function (aEvent)
  {  
    return !(aEvent.type == "click" && 
             (aEvent.button != 1 || aEvent.detail != this.openClickCount))
  },

  /////////////////////////////////////////////////////////////////////////////
  // For the given selection, selects the best adjacent element. This method is
  // useful when an action such as a cut or a deletion is performed on a 
  // selection, and focus/selection needs to be restored after the operation
  // is performed. 
  getNextElement: function (aElement)
  {
    if (aElement.nextSibling)
      return aElement.nextSibling;
    else if (aElement.previousSibling)
      return aElement.previousSibling;
    else
      return aElement.parentNode.parentNode;
  },
  
  selectElement: function (aElement)
  {
    this.tree.selectItem(aElement);
  },  
  
  //////////////////////////////////////////////////////////////////////////////
  // Add the treeitem element specified by aURI to the tree's current selection.
  addItemToSelection: function (aURI)
  {
    var item = document.getElementById(aURI) // XXX flawed for multiple ids
    this.tree.addItemToSelection(item);
  },

  /////////////////////////////////////////////////////////////////////////////
  // Return a set of DOM nodes that represent the selection in the tree widget.
  // This method is takes a node parameter which is the popupNode for the 
  // document. If the popupNode is not contained by the selection, the
  // popupNode is selected and the new selection returned. 
  getSelection: function ()
  {
    return this.tree.selectedItems;
  },
  
  getBestItem: function ()
  {
    var seln = this.getSelection ();
    if (seln.length < 1) {
      var kids = ContentUtils.childByLocalName(this.tree, "treechildren");
      if (kids) return kids.lastChild;
    }
    else 
      return seln[0];
    return null;
  },

  /////////////////////////////////////////////////////////////////////////////
  // Return a set of DOM nodes that represent the selection in the tree widget.
  // This method is takes a node parameter which is the popupNode for the 
  // document. If the popupNode is not contained by the selection, the
  // popupNode is selected and the new selection returned. 
  getContextSelection: function (aItemNode)
  {
    // How a context-click works:
    // if the popup node is contained by the selection, the context menu is 
    // built for that selection. However, if the popup node is invoked on a 
    // non-selected node, unless modifiers are pressed**, the previous
    // selection is discarded and that node selected. 
    var selectedItems = this.tree.selectedItems;
    for (var i = 0; i < selectedItems.length; ++i) {
      if (selectedItems[i] == aItemNode)
        return selectedItems;
    }
    this.tree.selectItem(aItemNode);
    return this.tree.selectedItems;
  },

  getSelectedFolder: function ()
  {
    var selectedItem = this.getBestItem();
    if (!selectedItem) return "NC:BookmarksRoot";
    while (selectedItem) {
      if (selectedItem.getAttribute("container") == "true") 
        return NODE_ID(selectedItem);
      selectedItem = selectedItem.parentNode.parentNode;
    }
    return "NC:BookmarksRoot";
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // For a given start DOM element, find the enclosing DOM element that contains
  // the template builder RDF resource decorations (id, ref, etc).
  findRDFNode: function (aStartNode, aIncludeStartNodeFlag)
  {
    var temp = aIncludeStartNodeFlag ? aStartNode : aStartNode.parentNode;
    while (temp && temp.localName != "treeitem") 
      temp = temp.parentNode;
    return temp || this.tree;
  },

  beginBatch: function ()
  {
    /*
    var bo = this.tree.boxObject.QueryInterface(Components.interfaces.nsITreeBoxObject);
    bo.beginBatch();
    */
  },
  
  endBatch: function ()
  {
    /*
    var bo = this.tree.boxObject.QueryInterface(Components.interfaces.nsITreeBoxObject);
    bo.endBatch();
    */
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // Tree click events. This handles when to go into inline-edit mode for 
  // editable cells. 
  treeClicked: function (aEvent)
  {
    if (this.tree.selectedItems.length > 1 || aEvent.detail > 1 || aEvent.button != 1)
      return;
    if (gSelectionTracker.currentItem == this.tree.currentItem && 
        gSelectionTracker.currentCell == aEvent.target) 
      ++gSelectionTracker.clickCount;
    else 
      gSelectionTracker.clickCount = 0;
    gSelectionTracker.currentItem = this.tree.currentItem;
    gSelectionTracker.currentCell = aEvent.target;

    var row = gSelectionTracker.currentItem.firstChild;
    for (var i = 0; i < row.childNodes.length; ++i) {
      if (row.childNodes[i] == gSelectionTracker.currentCell) {
        // Don't allow inline-edit of cells other than name for folders. 
        if (gSelectionTracker.currentItem.getAttribute("type") != NC_NS + "Bookmark" &&
            i > 1)
          return;
        if (gSelectionTracker.clickCount == 1) 
          gBookmarksShell.commands.editCell(this.tree.currentItem, i);
        break;
      }
    }
  },

  /////////////////////////////////////////////////////////////////////////////
  // Tree key events. This handles when to go into inline-edit mode for editable
  // cells, when to load a URL, etc. 
  treeKeyPress: function (aEvent)
  {
    if (this.tree.selectedItems.length > 1) return;
    if (aEvent.keyCode == 113 && aEvent.shiftKey) {
      if (this.resolveType(NODE_ID(this.tree.currentItem)) == NC_NS + "Bookmark")
        gBookmarksShell.commands.editCell (this.tree.currentItem, 1);
    }
    else if (aEvent.keyCode == 113)
      goDoCommand("cmd_rename");
    else if (aEvent.keyCode == 13 && 
             this.tree.currentItem.firstChild.getAttribute("inline-edit") != "true")
      goDoCommand("cmd_open");
  },
  
  selectFolderItem: function (aFolderURI, aItemURI, aAdditiveFlag)
  {
    var folder = document.getElementById(aFolderURI);
    var kids = ContentUtils.childByLocalName(folder, "treechildren");
    if (!kids) return;
    
    var item = kids.firstChild;
    while (item) {
      if (item.id == aItemURI) break;
      item = item.nextSibling;
    }
    if (!item) return;
    
    this.tree[aAdditiveFlag ? "addItemToSelection" : "selectItem"](item);
  },

  /////////////////////////////////////////////////////////////////////////////
  // Command handling & Updating. 
  controller: {
    supportsCommand: function (aCommand)
    {
      switch(aCommand) {
      case "cmd_undo":
      case "cmd_redo":
        return false;
      case "cmd_cut":
      case "cmd_copy":
      case "cmd_paste":
      case "cmd_delete":
      case "cmd_selectAll":
        return true;
      case "cmd_open":
      case "cmd_openfolder":
      case "cmd_openfolderinnewwindow":
      case "cmd_newbookmark":
      case "cmd_newfolder":
      case "cmd_newseparator":
      case "cmd_find":
      case "cmd_properties":
      case "cmd_rename":
      case "cmd_delete":
      case "cmd_setnewbookmarkfolder":
      case "cmd_setpersonaltoolbarfolder":
      case "cmd_setnewsearchfolder":    
      case "cmd_import":
      case "cmd_export":
        return true;
      default:
        return false;
      }
    },
  
    isCommandEnabled: function (aCommand)
    {
      var numSelectedItems = gBookmarksShell.tree.selectedItems.length;
      switch(aCommand) {
      case "cmd_undo":
      case "cmd_redo":
        return false;
      case "cmd_paste":
        return gBookmarksShell.canPaste();
      case "cmd_cut":
      case "cmd_copy":
      case "cmd_delete":
        return numSelectedItems >= 1;
      case "cmd_selectAll":
        return true;
      case "cmd_open":
        var seln = gBookmarksShell.tree.selectedItems;
        return numSelectedItems == 1 && seln[0].getAttribute("type") == NC_NS + "Bookmark";
      case "cmd_openfolder":
      case "cmd_openfolderinnewwindow":
        seln = gBookmarksShell.tree.selectedItems;
        return numSelectedItems == 1 && seln[0].getAttribute("type") == NC_NS + "Folder";
      case "cmd_find":
      case "cmd_newbookmark":
      case "cmd_newfolder":
      case "cmd_newseparator":
      case "cmd_import":
      case "cmd_export":
        return true;
      case "cmd_properties":
      case "cmd_rename": 
        return numSelectedItems == 1;
      case "cmd_setnewbookmarkfolder":
        seln = gBookmarksShell.tree.selectedItems;
        if (!seln.length) return false;
        var folderType = seln[0].getAttribute("type") == (NC_NS + "Folder");
        return numSelectedItems == 1 && !(NODE_ID(seln[0]) == "NC:NewBookmarkFolder") && folderType;
      case "cmd_setpersonaltoolbarfolder":
        seln = gBookmarksShell.tree.selectedItems;
        if (!seln.length) return false;
        folderType = seln[0].getAttribute("type") == (NC_NS + "Folder");
        return numSelectedItems == 1 && !(NODE_ID(seln[0]) == "NC:PersonalToolbarFolder") && folderType;
      case "cmd_setnewsearchfolder":
        seln = gBookmarksShell.tree.selectedItems;
        if (!seln.length) return false;
        folderType = seln[0].getAttribute("type") == (NC_NS + "Folder");
        return numSelectedItems == 1 && !(NODE_ID(seln[0]) == "NC:NewSearchFolder") && folderType;
      default:
        return false;
      }
    },
  
    doCommand: function (aCommand)
    {
      switch(aCommand) {
      case "cmd_undo":
      case "cmd_redo":
        break;
      case "cmd_paste":
      case "cmd_copy":
      case "cmd_cut":
      case "cmd_delete":
      case "cmd_newbookmark":
      case "cmd_newfolder":
      case "cmd_newseparator":
      case "cmd_properties":
      case "cmd_rename": 
      case "cmd_open":
      case "cmd_openfolder":
      case "cmd_openfolderinnewwindow":
      case "cmd_setnewbookmarkfolder":
      case "cmd_setpersonaltoolbarfolder":
      case "cmd_setnewsearchfolder":
      case "cmd_find":
      case "cmd_import":
      case "cmd_export":
        gBookmarksShell.execCommand(aCommand.substring("cmd_".length));
        break;
      case "cmd_selectAll":
        gBookmarksShell.tree.selectAll();
        break;
      }
    },
  
    onEvent: function (aEvent)
    {
      switch (aEvent) {
      case "tree-select":
        this.onCommandUpdate();
        break;
      }
    },

    onCommandUpdate: function ()
    {
      var commands = ["cmd_properties", "cmd_rename", "cmd_copy",
                      "cmd_paste", "cmd_cut", "cmd_delete",
                      "cmd_setpersonaltoolbarfolder", "cmd_setnewbookmarkfolder", 
                      "cmd_setnewsearchfolder"];
      for (var i = 0; i < commands.length; ++i)
        goUpdateCommand(commands[i]);
    }
  }
};

var newFolderRDFObserver = {
  _newFolderURI: null,
  onAssert: function (aDS, aSource, aProperty, aValue) 
  {
    try {
      var value = aValue.QueryInterface(Components.interfaces.nsIRDFResource);
      if (aDS.URI == "rdf:bookmarks" && aProperty.Value == RDF_NS + "type" && 
          value.Value == NC_NS + "Folder")
        this._newFolderURI = aSource.Value;
    }
    catch (e) {
      // Failures are OK, the value could be a literal instead of a resource.
    }
  },
    
  onUnassert: function (aDS, aSource, aProperty, aTarget) { },
  onChange: function (aDS, aSource, aProperty, aOldTarget, aNewTarget) { },
  onMove: function (aDS, aOldSource, aNewSource, aProperty, aTarget) { },
  beginUpdateBatch: function (aDS) { },
  endUpdateBatch: function (aDS) { }    
};

var ContentUtils = {
  childByLocalName: function (aSelectedItem, aLocalName)
  {
    var temp = aSelectedItem.firstChild;
    while (temp) {
      if (temp.localName == aLocalName) 
        return temp;
      temp = temp.nextSibling;
    }
    return null;
  }
};


