/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
// Class which defines methods for a bookmarks UI implementation based around
// a toolbar. Subclasses BookmarksBase in bookmarksOverlay.js. Some methods
// are required by the base class, others are for event handling. Window specific
// glue code should go into the BookmarksWindow class in bookmarks.js
function BookmarksToolbar (aID)
{
  this.id = aID;
}

BookmarksToolbar.prototype = {
  __proto__: BookmarksUIElement.prototype,

  /////////////////////////////////////////////////////////////////////////////
  // Personal Toolbar Specific Stuff
  
  openNewWindow: false,


  get db ()
  {
    return this.element.database;
  },

  get element ()
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
    var cmd = "cmd_" + aCommandName.substring(NC_NS_CMD.length)
    xulElement.setAttribute("command", cmd);
    
    switch (aCommandName) {
    case NC_NS_CMD + "open":
      xulElement.setAttribute("label", aDisplayName);
      xulElement.setAttribute("default", "true");
      break;
    case NC_NS_CMD + "openfolder":
      xulElement.setAttribute("default", "true");
      if (aItemNode.localName == "hbox") 
        // Don't show an "Open Folder" item for clicks on the toolbar itself.
        return null;
    default:
      xulElement.setAttribute("label", aDisplayName);
      break;
    }
    return xulElement;
  },

  // Command implementation
  commands: {
    openFolder: function (aSelectedItem)
    {
      var mbo = aSelectedItem.boxObject.QueryInterface(Components.interfaces.nsIMenuBoxObject);
      mbo.openMenu(true);
    },

    editCell: function (aSelectedItem, aXXXLameAssIndex)
    {
      goDoCommand("cmd_properties");
      return; // Disable Inline Edit for now. See bug 77125 for why this is being disabled
              // on the personal toolbar for the moment. 

      if (aSelectedItem.getAttribute("editable") != "true")
        return;
      var property = "http://home.netscape.com/NC-rdf#Name";
      aSelectedItem.setMode("edit");
      aSelectedItem.addObserver(this.postModifyCallback, "accept", 
                                [gBookmarksShell, aSelectedItem, property]);
    },

    ///////////////////////////////////////////////////////////////////////////
    // Called after an inline-edit cell has left inline-edit mode, and data
    // needs to be modified in the datasource.
    postModifyCallback: function (aParams)
    {
      var aShell = aParams[0];
      var selItemURI = NODE_ID(aParams[1]);
      aShell.propertySet(selItemURI, aParams[2], aParams[3]);
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
        var relativeNode = aSelectedItem || gBookmarksShell.element;
        var parentNode = relativeNode ? gBookmarksShell.findRDFNode(relativeNode, false) : gBookmarksShell.element;

        var args = [{ property: NC_NS + "parent",
                      resource: NODE_ID(parentNode) },
                    { property: NC_NS + "Name",
                      literal:  stringValue.value }];
        
        const kBMDS = gBookmarksShell.RDF.GetDataSource("rdf:bookmarks");
        var relId = relativeNode ? NODE_ID(relativeNode) : "NC:PersonalToolbarFolder";
        BookmarksUtils.doBookmarksCommand(relId, NC_NS_CMD + "newfolder", args);
      }
      
      return; 
      
      // HACK HACK HACK HACK HACK         
      /////////////////////////////////////////////////////////////////////////
      
      const kXULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
      var dummyButton = document.createElementNS(kXULNS, "menubutton");
      dummyButton = gBookmarksShell.createBookmarkFolderDecorations(dummyButton);
      dummyButton.setAttribute("class", "button-toolbar bookmark-item");

      dummyButton.setAttribute("label", gBookmarksShell.getLocaleString("ile_newfolder") + "  ");
      // By default, create adjacent to the selected button. If there is no button after
      // the selected button, or the target is the toolbar itself, just append. 
      var bIsButton = aSelectedItem.localName == "button" || aSelectedItem.localName == "menubutton";
      if (aSelectedItem.nextSibling && bIsButton)
        aSelectedItem.parentNode.insertBefore(dummyButton, aSelectedItem.nextSibling);
      else
        (bIsButton ? aSelectedItem.parentNode : aSelectedItem).appendChild(dummyButton);

      gBookmarksShell._focusElt = document.commandDispatcher.focusedElement;
      dummyButton.setMode("edit");
      // |aSelectedItem| will be the node we create the new folder relative to. 
      dummyButton.addObserver(this.onEditFolderName, "accept", 
                              [dummyButton, aSelectedItem, dummyButton]);
      dummyButton.addObserver(this.onEditFolderName, "reject", 
                              [dummyButton, aSelectedItem, dummyButton]);
    },

    ///////////////////////////////////////////////////////////////////////////
    // Edit folder name & update the datasource if name is valid
    onEditFolderName: function (aParams, aTopic)
    {
      // Because the toolbar has no concept of selection, this function
      // is much simpler than the one in bookmarksTree.js. However it may
      // become more complex if pink ever lets me put context menus on menus ;) 
      var name = aParams[3];
      var dummyButton = aParams[2];
      var relativeNode = aParams[1];
      var parentNode = gBookmarksShell.findRDFNode(relativeNode, false);

      dummyButton.parentNode.removeChild(dummyButton);

      if (!gBookmarksShell.commands.validateNameAndTopic(name, aTopic, relativeNode, dummyButton))
        return;

      parentNode = relativeNode.parentNode;
      if (relativeNode.localName == "hbox") {
        parentNode = relativeNode;
        relativeNode = (gBookmarksShell.commands.nodeIsValidType(relativeNode) && 
                        relativeNode.lastChild) || relativeNode;
      }

      var args = [{ property: NC_NS + "parent",
                    resource: NODE_ID(parentNode) },
                  { property: NC_NS + "Name",
                    literal:  name }];

      BookmarksUtils.doBookmarksCommand(NODE_ID(relativeNode),
                                        NC_NS_CMD + "newfolder", args);
      // We need to do this because somehow focus shifts and no commands 
      // operate any more. 
      //gBookmarksShell._focusElt.focus();
    },

    nodeIsValidType: function (aNode)
    {
      switch (aNode.localName) {
      case "button":
      case "menubutton":
      // case "menu":
      // case "menuitem":
        return true;
      }
      return false;
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
      return !(aTopic == "reject");
    }
  },

  _focusElt: null,

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
      return aElement.parentNode;
  },

  selectElement: function (aElement)
  {
  },

  //////////////////////////////////////////////////////////////////////////////
  // Add the treeitem element specified by aURI to the tree's current selection.
  addItemToSelection: function (aURI)
  {
  },

  /////////////////////////////////////////////////////////////////////////////
  // Return a set of DOM nodes that represents the current item in the Bookmarks
  // Toolbar. This is always |document.popupNode|.
  getSelection: function ()
  {
    return [document.popupNode];
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
    return [aItemNode];
  },

  getSelectedFolder: function ()
  {
    return "NC:PersonalToolbarFolder";
  },

  /////////////////////////////////////////////////////////////////////////////
  // For a given start DOM element, find the enclosing DOM element that contains
  // the template builder RDF resource decorations (id, ref, etc). In the 
  // Toolbar case, this is always the popup node (until we're proven wrong ;)
  findRDFNode: function (aStartNode, aIncludeStartNodeFlag)
  {
    var temp = aStartNode;
    while (temp && temp.localName != (aIncludeStartNodeFlag ? "button"     : "hbox") 
                && temp.localName != (aIncludeStartNodeFlag ? "menubutton" : "hbox"))
      temp = temp.parentNode;
    return temp || this.element;
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
      switch(aCommand) {
      case "cmd_undo":
      case "cmd_redo":
        return false;
      case "cmd_bm_paste":
        var cp = gBookmarksShell.canPaste();
        return cp;
      case "cmd_bm_cut":
      case "cmd_bm_copy":
      case "cmd_bm_delete":
        return (document.popupNode != null) && (NODE_ID(document.popupNode) != "NC:PersonalToolbarFolder");
      case "cmd_bm_selectAll":
        return false;
      case "cmd_open":
        var seln = gBookmarksShell.getSelection();
        return document.popupNode != null && seln[0].getAttribute("type") == NC_NS + "Bookmark";
      case "cmd_openfolder":
      case "cmd_openfolderinnewwindow":
        seln = gBookmarksShell.getSelection();
        return document.popupNode != null && seln[0].getAttribute("type") == NC_NS + "Folder";
      case "cmd_find":
      case "cmd_newbookmark":
      case "cmd_newfolder":
      case "cmd_newseparator":
      case "cmd_import":
      case "cmd_export":
        return true;
      case "cmd_properties":
      case "cmd_rename":
        return document.popupNode != null;
      case "cmd_setnewbookmarkfolder":
        seln = gBookmarksShell.getSelection();
        if (!seln.length) return false;
        var folderType = seln[0].getAttribute("type") == (NC_NS + "Folder");
        return document.popupNode != null && !(NODE_ID(seln[0]) == "NC:NewBookmarkFolder") && folderType;
      case "cmd_setpersonaltoolbarfolder":
        seln = gBookmarksShell.getSelection();
        if (!seln.length) return false;
        folderType = seln[0].getAttribute("type") == (NC_NS + "Folder");
        return document.popupNode != null && !(NODE_ID(seln[0]) == "NC:PersonalToolbarFolder") && folderType;
      case "cmd_setnewsearchfolder":
        seln = gBookmarksShell.getSelection();
        if (!seln.length) return false;
        folderType = seln[0].getAttribute("type") == (NC_NS + "Folder");
        return document.popupNode != null && !(NODE_ID(seln[0]) == "NC:NewSearchFolder") && folderType;
      case "cmd_bm_fileBookmark":
        seln = gBookmarksShell.getSelection();
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
        break;
      }
    },

    onEvent: function (aEvent)
    {
    },

    onCommandUpdate: function ()
    {
    }
  }
};

function BM_navigatorLoad(aEvent)
{
  if (!gBookmarksShell) {
    gBookmarksShell = new BookmarksToolbar("innermostBox");
    controllers.appendController(gBookmarksShell.controller);
    removeEventListener("load", BM_navigatorLoad, false);
  }
}

addEventListener("load", BM_navigatorLoad, false);

