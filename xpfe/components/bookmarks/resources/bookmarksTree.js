/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@netscape.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  // XXX - change this to .element and move into base. 
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
    xulElement.setAttribute("cmd", aCommandName);
    xulElement.setAttribute("command", "cmd_" + aCommandName.substring(NC_NS_CMD.length));

    switch (aCommandName) {
    case NC_NS_CMD + "open":
      xulElement.setAttribute("label", aDisplayName);
      xulElement.setAttribute("default", "true");
      break;
    case NC_NS_CMD + "openfolder":
      aDisplayName = aItemNode.getAttribute("open") == "true" ? this.getLocaleString("cmd_openfolder2") : aDisplayName;
      xulElement.setAttribute("label", aDisplayName);
      xulElement.setAttribute("default", "true");
      break;
    case NC_NS_CMD + "renamebookmark":
      if (!document.popupNode.hasAttribute("type")) {
        xulElement.setAttribute("label", this.getLocaleString("cmd_renamebookmark2"));
        xulElement.setAttribute("cmd", (NC_NS_CMD + "editurl"));
      }
      else
        xulElement.setAttribute("label", aDisplayName);
      break;
    default:
      xulElement.setAttribute("label", aDisplayName);
      break;
    }
    return xulElement;
  },

  // XXX - ideally this would be in the base. this.tree needs to change to 
  // this.element and then we can do just that. 
  setRoot: function (aRoot)
  {
    this.tree.setAttribute("ref", aRoot);
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
      // XXX throw up properties dialog with name selected so user can rename
      //     that way, until Outliner conversion allows us to use IL again. 
      goDoCommand("cmd_properties");
      return; // Disable inline edit for now.

      var editCell = aSelectedItem.firstChild.childNodes[aCell];
      if (editCell.getAttribute("editable") != "true")
        return;
      
      // Cause the inline edit cell binding to be used.
      editCell.setAttribute("class", "treecell-indent treecell-editable");
      var editColGroup = document.getElementById("theColumns");
      var count = 0;
      var property = "";
      for (var i = 0; i < editColGroup.childNodes.length; ++i) {
        var currCol = editColGroup.childNodes[i];
        if (currCol.getAttribute("hidden") == "true")
          return;
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
                             [editCell, aSelectedItem, property]);
      }
    },

    ///////////////////////////////////////////////////////////////////////////
    // Called after an inline-edit cell has left inline-edit mode, and data
    // needs to be modified in the datasource.
    postModifyCallback: function (aParams)
    {
      var selItemURI = NODE_ID(aParams[1]);
      gBookmarksShell.propertySet(selItemURI, aParams[2], aParams[3]);
      gBookmarksShell.selectFolderItem(NODE_ID(gBookmarksShell.findRDFNode(aParams[1], false)),
                                       selItemURI, false);
      gBookmarksShell.tree.focus();
      gSelectionTracker.clickCount = 0;
      
      // Set the cell back to use the standard treecell binding.
      var editCell = aParams[0];
      editCell.setAttribute("class", "treecell-indent");
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
      var parentNode = relativeNode ? gBookmarksShell.findRDFNode(relativeNode, false) : gBookmarksShell.tree;

      dummyItem.parentNode.removeChild(dummyItem);

      if (!shell.validateNameAndTopic(name, aTopic, relativeNode, dummyItem)) {
        gBookmarksShell.tree.selectItem(relativeNode);
        gBookmarksShell.tree.focus();
        return;
      }

      if (relativeNode) {
        // If we're attempting to create a folder as a subfolder of an open folder,
        // we need to set the parentFolder to be relativeNode, which will be the
        // parent of the new folder, rather than the parent of the relativeNode,
        // which will result in the folder being created in an incorrect position
        // (adjacent to the relativeNode).
        var selKids = ContentUtils.childByLocalName(relativeNode, "treechildren");
        if (selKids && selKids.hasChildNodes() && selKids.lastChild == dummyItem)
          parentNode = relativeNode;
      }

      var args = [{ property: NC_NS + "parent",
                    resource: NODE_ID(parentNode) },
                  { property: NC_NS + "Name",
                    literal:  name }];

      const kBMDS = gBookmarksShell.RDF.GetDataSource("rdf:bookmarks");
      kBMDS.AddObserver(newFolderRDFObserver);
      var relId = relativeNode ? NODE_ID(relativeNode) : "NC:BookmarksRoot";
      BookmarksUtils.doBookmarksCommand(relId, NC_NS_CMD + "newfolder", args);
      kBMDS.RemoveObserver(newFolderRDFObserver);
      var newFolderItem = document.getElementById(newFolderRDFObserver._newFolderURI);
      gBookmarksShell.tree.focus();
      gBookmarksShell.tree.selectItem(newFolderItem);
      // Can't use newFolderItem because it may not have been created yet. Hack, huh?
      var index = gBookmarksShell.tree.getIndexOfItem(relativeNode);
      gBookmarksShell.tree.ensureIndexIsVisible(index+1);
      gSelectionTracker.clickCount = 0;
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
        return false;
      }
      return true;
    },

    ///////////////////////////////////////////////////////////////////////////
    // Creates a dummy item that can be placed in edit mode to retrieve data
    // to create new bookmarks/folders.
    createBookmarkItem: function (aMode, aSelectedItem)
    {
      /////////////////////////////////////////////////////////////////////////
      // HACK HACK HACK HACK HACK         
      // Disable Inline-Edit for now and just use a dialog. 
      
      // XXX - most of this is just copy-pasted from the other two folder
      //       creation functions. Yes it's ugly, but it'll do the trick for 
      //       now as this is in no way intended to be a long-term solution.

      const kPromptSvcContractID = "@mozilla.org/embedcomp/prompt-service;1";
      const kPromptSvcIID = Components.interfaces.nsIPromptService;
      const kPromptSvc = Components.classes[kPromptSvcContractID].getService(kPromptSvcIID);
      
      var defaultValue  = gBookmarksShell.getLocaleString("ile_newfolder");
      var dialogTitle   = gBookmarksShell.getLocaleString("newfolder_dialog_title");
      var dialogMsg     = gBookmarksShell.getLocaleString("newfolder_dialog_msg");
      var stringValue   = { value: defaultValue };
      if (kPromptSvc.prompt(window, dialogTitle, dialogMsg, stringValue, null, { value: 0 })) {
        var relativeNode = gBookmarksShell.tree;
        if (aSelectedItem && aSelectedItem.localName != "tree") {
          // By default, create adjacent to the selected item
          relativeNode = aSelectedItem;
          if (relativeNode.getAttribute("container") == "true" && 
              relativeNode.getAttribute("open") == "true") {
            // But if it's an open container, the relative node should be the last child. 
            var treechildren = ContentUtils.childByLocalName(relativeNode, "treechildren");
            if (treechildren && treechildren.hasChildNodes()) 
              relativeNode = treechildren.lastChild;
          }
        }

        var parentNode = relativeNode ? gBookmarksShell.findRDFNode(relativeNode, false) : gBookmarksShell.tree;
        if (relativeNode) {
          // If we're attempting to create a folder as a subfolder of an open folder,
          // we need to set the parentFolder to be relativeNode, which will be the
          // parent of the new folder, rather than the parent of the relativeNode,
          // which will result in the folder being created in an incorrect position
          // (adjacent to the relativeNode).
          var selKids = ContentUtils.childByLocalName(relativeNode, "treechildren");
          if (selKids && selKids.hasChildNodes() && selKids.lastChild == dummyItem)
            parentNode = relativeNode;
        }

        var args = [{ property: NC_NS + "parent",
                      resource: NODE_ID(parentNode) },
                    { property: NC_NS + "Name",
                      literal:  stringValue.value }];
        
        const kBMDS = gBookmarksShell.RDF.GetDataSource("rdf:bookmarks");
        kBMDS.AddObserver(newFolderRDFObserver);
        var relId = relativeNode ? NODE_ID(relativeNode) : "NC:BookmarksRoot";
        BookmarksUtils.doBookmarksCommand(relId, NC_NS_CMD + "newfolder", args);
        kBMDS.RemoveObserver(newFolderRDFObserver);
        var newFolderItem = document.getElementById(newFolderRDFObserver._newFolderURI);
        gBookmarksShell.tree.focus();
        gBookmarksShell.tree.selectItem(newFolderItem);
        // Can't use newFolderItem because it may not have been created yet. Hack, huh?
        var index = gBookmarksShell.tree.getIndexOfItem(relativeNode);
        gBookmarksShell.tree.ensureIndexIsVisible(index+1);
      }
      
      return; 
      
      // HACK HACK HACK HACK HACK         
      /////////////////////////////////////////////////////////////////////////

      /* Disable inline edit for now
      const kXULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
      var dummyItem = document.createElementNS(kXULNS, "treeitem");
      dummyItem = gBookmarksShell.createBookmarkFolderDecorations(dummyItem);
      dummyItem.setAttribute("class", "bookmark-item");

      var dummyRow    = document.createElementNS(kXULNS, "treerow");
      var dummyCell   = document.createElementNS(kXULNS, "treecell");
      var dummyCell2  = document.createElementNS(kXULNS, "treecell");
      dummyCell.setAttribute("label", gBookmarksShell.getLocaleString("ile_newfolder") + "  ");
      dummyCell.setAttribute("type", NC_NS + "Folder");
      dummyCell.setAttribute("editable", "true");
      dummyCell.setAttribute("class", "treecell-indent treecell-editable");
      dummyRow.appendChild(dummyCell);
      dummyItem.appendChild(dummyRow);
      
      var relativeNode = null;

      // If there are selected items, try to create the dummy item relative to the 
      // best item, and position the bookmark there when created. Otherwise just
      // append to the root. 
      if (aSelectedItem && aSelectedItem.localName != "tree") {
        // By default, create adjacent to the selected item
        relativeNode = aSelectedItem;
        if (relativeNode.getAttribute("container") == "true" && 
            relativeNode.getAttribute("open") == "true") {
          // But if it's an open container, the relative node should be the last child. 
          var treechildren = ContentUtils.childByLocalName(relativeNode, "treechildren");
          if (treechildren && treechildren.hasChildNodes()) 
            relativeNode = treechildren.lastChild;
        }

        if (aSelectedItem.getAttribute("container") == "true") {
          if (aSelectedItem.getAttribute("open") == "true") {
            var treechildren = ContentUtils.childByLocalName(aSelectedItem, "treechildren");
            if (!treechildren) {
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
          var index = gBookmarksShell.tree.getIndexOfItem(dummyItem);
          gBookmarksShell.tree.ensureIndexIsVisible(index);
        }
        else {
          if (aSelectedItem.nextSibling)
            aSelectedItem.parentNode.insertBefore(dummyItem, aSelectedItem.nextSibling);
          else
            aSelectedItem.parentNode.appendChild(dummyItem);
        }
      }
      else {
        // No items in the tree. Append to the root. 
        var rootKids = document.getElementById("treechildren-bookmarks");
        rootKids.appendChild(dummyItem);
      }

      dummyCell.setMode("edit");
      dummyCell.addObserver(this.onEditFolderName, "accept", [dummyCell, relativeNode, dummyItem]);
      dummyCell.addObserver(this.onEditFolderName, "reject", [dummyCell, relativeNode, dummyItem]);
      */
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
             (aEvent.button != 0 || aEvent.detail != this.openClickCount))
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
    // Note that we don't just the selectedItems NodeList here because that
    // is a reference to a LIVE DOM NODE LIST. We want to maintain control
    // over what is in the selection array ourselves.
    return [].concat(this.tree.selectedItems);
  },

  getBestItem: function ()
  {
    var seln = this.getSelection ();
    if (seln.length < 1) {
      var kids = ContentUtils.childByLocalName(this.tree, "treechildren");
      return kids.lastChild || this.tree;
    }
    else
      return seln[0];
    return this.tree;
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
    if (aItemNode.localName == "treeitem")
      this.tree.selectItem(aItemNode);
    return this.tree.selectedItems.length ? this.tree.selectedItems : [this.tree];
  },

  getSelectedFolder: function ()
  {
    var selectedItem = this.getBestItem();
    if (!selectedItem) return "NC:BookmarksRoot";
    while (selectedItem && selectedItem.nodeType == Node.ELEMENT_NODE) {
      if (selectedItem.getAttribute("container") == "true" && 
          selectedItem.getAttribute("open") == "true")
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

  /////////////////////////////////////////////////////////////////////////////
  // Tree click events. This handles when to go into inline-edit mode for
  // editable cells.
  treeClicked: function (aEvent)
  {
    // We are disabling Inline Edit for now. It's too buggy in the old XUL tree widget.
    // A more solid implementation will follow the conversion to outliner. 
/*
    if (this.tree.selectedItems.length > 1 || aEvent.detail > 1 || aEvent.button != 0) {
      gSelectionTracker.clickCount = 0;
      return;
    }
    if (gSelectionTracker.currentItem == this.tree.currentItem &&
        gSelectionTracker.currentCell == aEvent.target)
      ++gSelectionTracker.clickCount;
    else
      gSelectionTracker.clickCount = 0;

    if (!this.tree.currentItem)
      return;

    gSelectionTracker.currentItem = this.tree.currentItem;
    gSelectionTracker.currentCell = aEvent.target;

    if (gSelectionTracker.currentItem.getAttribute("type") != NC_NS + "Bookmark" &&
        gSelectionTracker.currentItem.getAttribute("type") != NC_NS + "Folder")
      return;

    var row = gSelectionTracker.currentItem.firstChild;
    if (row) {
      for (var i = 0; i < row.childNodes.length; ++i) {
        if (row.childNodes[i] == gSelectionTracker.currentCell) {
          // Don't allow inline-edit of cells other than name for folders.
          // XXX - so so skeezy. Change this to look for NC:Name or some such.
          if (gSelectionTracker.currentItem.getAttribute("type") != NC_NS + "Bookmark" && i)
            return;
          // Don't allow editing of the root folder name
          if (gSelectionTracker.currentItem.id == "NC:BookmarksRoot")
            return;
          if (gSelectionTracker.clickCount == 1 && this.openClickCount > 1)
            gBookmarksShell.commands.editCell(this.tree.currentItem, i);
          break;
        }
      }
    }
*/
  },

  treeOpen: function (aEvent)
  {
    if (this.isValidOpenEvent(aEvent)) {
      var rdfNode = this.findRDFNode(aEvent.target, true);
      if (rdfNode.getAttribute("container") != "true")
        this.open(aEvent, rdfNode);
    }
  },

  /////////////////////////////////////////////////////////////////////////////
  // Tree key events. This handles when to go into inline-edit mode for editable
  // cells, when to load a URL, etc.
  treeKeyPress: function (aEvent)
  {
    if (this.tree.selectedItems.length > 1) return;

 /* Disabling Inline Edit
    if (aEvent.keyCode == 113 && aEvent.shiftKey) {
      const kNodeId = NODE_ID(this.tree.currentItem);
      if (this.resolveType(kNodeId) == NC_NS + "Bookmark")
        gBookmarksShell.commands.editCell (this.tree.currentItem, 1);
    }
    else */
    if (aEvent.keyCode == 113)
      goDoCommand("cmd_rename");
    else if (aEvent.keyCode == 13) // && this.tree.currentItem.firstChild.getAttribute("inline-edit") != "true")
      goDoCommand(aEvent.altKey ? "cmd_properties" : "cmd_open");
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
      case "cmd_bm_cut":
      case "cmd_bm_copy":
      case "cmd_bm_paste":
      case "cmd_bm_delete":
      case "cmd_bm_selectAll":
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
      case "cmd_setnewbookmarkfolder":
      case "cmd_setpersonaltoolbarfolder":
      case "cmd_setnewsearchfolder":
      case "cmd_import":
      case "cmd_export":
      case "cmd_bm_fileBookmark":
        return true;
      default:
        return false;
      }
    },

    isCommandEnabled: function (aCommand)
    {
      var numSelectedItems = gBookmarksShell.tree.selectedItems.length;
      var seln, firstSelected, folderType, bItemCountCorrect;
      switch(aCommand) {
      case "cmd_undo":
      case "cmd_redo":
        return false;
      case "cmd_bm_paste":
        return gBookmarksShell.canPaste();
      case "cmd_bm_cut":
      case "cmd_bm_copy":
      case "cmd_bm_delete":
        return numSelectedItems >= 1;
      case "cmd_bm_selectAll":
        return true;
      case "cmd_open":
         seln = gBookmarksShell.tree.selectedItems;
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
        seln = gBookmarksShell.tree.selectedItems;
        return numSelectedItems == 1 && seln[0].getAttribute("type") != NC_NS + "BookmarkSeparator";
      case "cmd_setnewbookmarkfolder":
        seln = gBookmarksShell.tree.selectedItems;
        firstSelected = seln.length ? seln[0] : gBookmarksShell.tree;
        folderType = firstSelected.getAttribute("type") == (NC_NS + "Folder");
        bItemCountCorrect = seln.length ? numSelectedItems == 1 : true;
        return bItemCountCorrect && !(NODE_ID(firstSelected) == "NC:NewBookmarkFolder") && folderType;
      case "cmd_setpersonaltoolbarfolder":
        seln = gBookmarksShell.tree.selectedItems;
        firstSelected = seln.length ? seln[0] : gBookmarksShell.tree;
        folderType = firstSelected.getAttribute("type") == (NC_NS + "Folder");
        bItemCountCorrect = seln.length ? numSelectedItems == 1 : true;
        return bItemCountCorrect && !(NODE_ID(firstSelected) == "NC:PersonalToolbarFolder") && folderType;
      case "cmd_setnewsearchfolder":
        seln = gBookmarksShell.tree.selectedItems;
        firstSelected = seln.length ? seln[0] : gBookmarksShell.tree;
        folderType = firstSelected.getAttribute("type") == (NC_NS + "Folder");
        bItemCountCorrect = seln.length ? numSelectedItems == 1 : true;
        return bItemCountCorrect == 1 && !(NODE_ID(firstSelected) == "NC:NewSearchFolder") && folderType;
      case "cmd_bm_fileBookmark":
        seln = gBookmarksShell.tree.selectedItems;
        return seln.length > 0;
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
      case "cmd_bm_paste":
      case "cmd_bm_copy":
      case "cmd_bm_cut":
      case "cmd_bm_delete":
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
      case "cmd_bm_fileBookmark":
        gBookmarksShell.execCommand(aCommand.substring("cmd_".length));
        break;
      case "cmd_bm_selectAll":
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
      var commands = ["cmd_properties", "cmd_rename", "cmd_bm_copy",
                      "cmd_bm_paste", "cmd_bm_cut", "cmd_bm_delete",
                      "cmd_setpersonaltoolbarfolder", 
                      "cmd_setnewbookmarkfolder",
                      "cmd_setnewsearchfolder", "cmd_bm_fileBookmark", 
                      "cmd_openfolderinnewwindow", "cmd_openfolder"];
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

