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
 *   Ben Goodger <ben@netscape.com> (Original Author, v2.0)
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

var NC_NS  = "http://home.netscape.com/NC-rdf#";
var RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";

var gSpringLoadTracker = {
  timeout: 0,
  element: null,
  
  open: function (aRDFNode) 
  {
    if (this.element)
      this.element.setAttribute("open", "true");
    clearTimeout(this.timeout);
  }
};

var bookmarksDNDObserver = {
  _RDF: null,
  get RDF ()
  {
    if (!this._RDF) {
      const kRDFContractID = "@mozilla.org/rdf/rdf-service;1";
      const kRDFIID = Components.interfaces.nsIRDFService;
      this._RDF = Components.classes[kRDFContractID].getService(kRDFIID);
    }
    return this._RDF;
  },

  // XXX I belong somewhere shared. 
  getResource: function(aString)
  {
    return this.RDF.GetResource(aString, true);
  },
  
  getTarget: function(aDS, aSourceID, aPropertyID)
  {
    var source = this.getResource(aSourceID);
    var property = this.getResource(aPropertyID);
    return aDS.GetTarget(source, property, true);
  },
  
  onDragStart: function (aEvent, aXferData, aDragAction)
  {
    var bookmarksTree = document.getElementById("bookmarksTree");
    if (aEvent.target == bookmarksTree || aEvent.target.localName == "treechildren" || 
        aEvent.target.localName == "splitter" || aEvent.target.localName == "menu")
      throw Components.results.NS_OK; // not a draggable item.
    if (aEvent.target.parentNode && aEvent.target.parentNode.parentNode &&
        aEvent.target.parentNode.parentNode.localName == "treehead")
      throw Components.results.NS_OK; // don't drag treehead cells.
    if (bookmarksTree.getAttribute("sortActive") == "true")
      throw Components.results.NS_OK; 
          
    var selItems = null;
    if (bookmarksTree.selectedItems.length <= 0)
      selItems = [aEvent.target.parentNode.parentNode];
    else 
      selItems = bookmarksTree.selectedItems;
    aXferData.data = new TransferDataSet();
    for (var i = 0; i < selItems.length; ++i) {
      var currItem = selItems[i];
      var currURI = NODE_ID(currItem);
    
      var parentItem = currItem.parentNode.parentNode;
      var parentURI = NODE_ID(parentItem);
    
      var type = this.getTarget(bookmarksTree.database, currURI, RDF_NS + "type");
      type = type.QueryInterface(Components.interfaces.nsIRDFResource).Value;
      if (!type || (type != (NC_NS + "BookmarkSeparator") && 
                    type != (NC_NS + "Bookmark") && 
                    type != (NC_NS + "Folder"))) 
        throw Components.results.NS_OK;
      var name = this.getTarget(bookmarksTree.database, currURI, NC_NS + "Name");        
      var data = new TransferData();
      if (name) {
        name = name.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
        data.addDataForFlavour("text/x-moz-url", currURI + "\n" + name);
      }
      else {
        data.addDataForFlavour("text/x-moz-url", currURI);
      }
      data.addDataForFlavour("moz/rdfitem", currURI + "\n" + parentURI);

      data.addDataForFlavour("text/unicode", currURI);
      aXferData.data.push(data);
    }

    if (aEvent.ctrlKey) {
      const kDSIID = Components.interfaces.nsIDragService;
      aDragAction.action = kDSIID.DRAGDROP_ACTION_COPY + kDSIID.DRAGDROP_ACTION_LINK;
    }
  },
  
  onDragOver: function (aEvent, aFlavour, aDragSession)
  {
    var bookmarksTree = document.getElementById("bookmarksTree");
    var rowGroup = aEvent.target.parentNode.parentNode;
    if (rowGroup)
      rowGroup.setAttribute("dd-triggerrepaint" + 
                            (bookmarksTree.getAttribute("sortActive") == "true" ? "sorted" : ""), 0);
    
    var rdfNode = gBookmarksShell.findRDFNode(aEvent.target, true);
    var rdfParent = rdfNode.parentNode.parentNode;
    if (rdfParent && rdfParent.getAttribute("container") == "true") {
      var rDragOverContainer = this.RDF.GetResource(NODE_ID(rdfParent));

      const kBMDS = this.RDF.GetDataSource("rdf:bookmarks");
      const kRDFCUtilsContractID = "@mozilla.org/rdf/container-utils;1";
      const kRDFCUtilsIID = Components.interfaces.nsIRDFContainerUtils;
      const kRDFCUtils = Components.classes[kRDFCUtilsContractID].getService(kRDFCUtilsIID);
      var isContainer = kRDFCUtils.IsContainer(kBMDS, rDragOverContainer);
      if (!isContainer) {
        // This ain't a container. Don't allow drops, and bail before doing anything
        // else.
        aDragSession.canDrop = false;
        return;
      }
    }      
    
    // Springloaded folders.
    /* XXX - not yet. 
    if (rdfNode && rdfNode.getAttribute("container") == "true" &&
        rdfNode.getAttribute("open") != "true") {
      if (!gSpringLoadTracker.element || gSpringLoadTracker.element.id != rdfNode.id) {
        // XXX - this is not good enough. We need to keep track of nesting and close up
        // folders after the user has dragged out of them otherwise we end up with 
        // everything open and a big mess!
        if (gSpringLoadTracker.timeout)
          clearTimeout(gSpringLoadTracker.timeout);
        gSpringLoadTracker.element = rdfNode;
        gSpringLoadTracker.timeout = setTimeout("gSpringLoadTracker.open()", 100);
      }
    }
    */
  },  
  
  _flavourSet: null,
  getSupportedFlavours: function ()
  {
    if (!this._flavourSet) {
      this._flavourSet = new FlavourSet();
      this._flavourSet.appendFlavour("moz/rdfitem");
      this._flavourSet.appendFlavour("text/x-moz-url");
      this._flavourSet.appendFlavour("text/unicode");
    }
    return this._flavourSet;
  },
  
  canHandleMultipleItems: true,
  
  onDrop: function (aEvent, aXferData, aDragSession)
  {
    var bookmarksTree = document.getElementById("bookmarksTree");
    // XXX lame
    if (bookmarksTree.getAttribute("sortActive") == "true") return;

    const kRDFCContractID = "@mozilla.org/rdf/container;1";
    const kRDFIID = Components.interfaces.nsIRDFContainer;
    var RDFC = Components.classes[kRDFCContractID].getService(kRDFIID);
    
    const kBMDS = this.RDF.GetDataSource("rdf:bookmarks");
    
    var dropItem = aEvent.target.parentNode.parentNode;
    if (aEvent.target.localName == "treechildren") 
      dropItem = aEvent.target.parentNode; // handle drop on blank space. 

    // In the default view, the root node is the NC root, and we don't want to append
    // to that. Adjust accordingly...
    if (NODE_ID(dropItem) == "NC:NavCenter")
      dropItem = document.getElementById("treechildren-bookmarks").firstChild;
    
    if (!dropItem) return;
      
    // XXX we could probably compute this ourselves, but let the tree do this 
    //     automagically for now.
    var dropBefore = dropItem.getAttribute("dd-droplocation");
    var dropOn = dropItem.getAttribute("dd-dropon");

    var dropAction = dropBefore == "true" ? "before" : dropOn == "true" ? "on" : "after";
    if (aEvent.target.localName == "treechildren")  
      dropAction = "on"; // handle drop on blank space. 
    var containerItem = dropAction == "on" ? dropItem : dropItem.parentNode.parentNode;

  	// XXX magical fix for bug # 33546: handle dropping after open container
    if (dropItem.getAttribute("container") && dropItem.getAttribute("open") &&
        dropAction == "after") {    
      dropAction = "before";
      containerItem = dropItem;

      dropItem = null;
      for (var i = 0; i < containerItem.childNodes.length; ++i) {
        if (containerItem.childNodes[i].localName == "treechildren") {
          dropItem = containerItem.childNodes[i].firstChild;
          break;
        }
      }
      if (!dropItem) {
        dropAction = "on";
        dropItem = containerItem.parentNode.parentNode;
      }
    }
    
    var rTarget = this.getResource(NODE_ID(dropItem));
    var rContainer = this.getResource(NODE_ID(containerItem));    

    const kRDFCUtilsContractID = "@mozilla.org/rdf/container-utils;1";
    const kRDFCUtilsIID = Components.interfaces.nsIRDFContainerUtils;
    const kRDFCUtils = Components.classes[kRDFCUtilsContractID].getService(kRDFCUtilsIID);
    var isContainer = kRDFCUtils.IsContainer(kBMDS, rContainer);

    // XXX
    var rType = this.getResource(RDF_NS + "type");
    var rBookmark = this.getResource(NC_NS + "Bookmark");
    
    var dirty = false;
    var additiveFlag = false;
    var numObjects = aXferData.dataList.length;
    /*
    if (numObjects > 1) {
      var bo = bookmarksTree.boxObject.QueryInterface(Components.interfaces.nsITreeBoxObject);
      bo.beginBatch();
    }
    */

    var sourceID = [], parentID = [], nameRequired = [], name = [];
    var flavourData;
    for (i = 0; i < numObjects; ++i) {
      flavourData = aXferData.dataList[i].first;
      nameRequired[i] = false;
      name[i] = null;
      var data = flavourData.data;
      switch (flavourData.flavour.contentType) {
      case "moz/rdfitem":
        var ix = data.indexOf("\n");
        sourceID[i] = ix >= 0 ? (parentID[i] = data.substr(ix+1), data.substr(0, ix)) : data;
        break;
      case "text/x-moz-url":
        ix = data.indexOf("\n");
        sourceID[i] = ix >= 0 ? (name[i] = data.substr(ix+1), data.substr(0, ix)) : data;
        break;
      case "text/unicode":
        sourceID[i] = data;
        nameRequired[i] = true;
        break;
      default: 
        continue;
      }
      
      var rSource = this.getResource(sourceID[i]);
      var rParent = parentID[i] ? this.getResource(parentID[i]) : null;

      // Prevent dropping node on itself, before or after itself, on its parent 
      // container, or a weird situation when an open container is dropped into
      // itself (which results in data loss!).
      // Also prevent dropping into a folder that isn't actually a container 
      // (and is thus probably a pseudo-container from an aggregated datasource,
      //  see bug 68656 fir details).
      if (rSource == rTarget || (dropAction == "on" && rContainer == rParent) ||
          rContainer == rSource || !isContainer)
        return;

      // Prevent dropping node into one of its own subfolders
      var dropItem2 = dropItem;
      do {
        var targetAncestor = NODE_ID(dropItem2);
        dropItem2 = dropItem2.parentNode;
      } while (targetAncestor != "NC:BookmarksRoot" && targetAncestor != sourceID[i]);
      if (targetAncestor == sourceID[i]) {
        return;
      }
    }

    for (i = 0; i < numObjects; ++i) {
      flavourData = aXferData.dataList[i].first;
      rSource = this.getResource(sourceID[i]);
      rParent = parentID[i] ? this.getResource(parentID[i]) : null;

      // XXX if any of the following fails, the nodes are gone for good!
      const kDSIID = Components.interfaces.nsIDragService;
      const kCopyAction = kDSIID.DRAGDROP_ACTION_COPY + kDSIID.DRAGDROP_ACTION_LINK;
      if (rParent) {
        if (!(aDragSession.dragAction & kCopyAction)) {
          try {
            RDFC.Init(kBMDS, rParent);
            ix = RDFC.IndexOf(rSource);
            if (ix >= 1) 
              RDFC.RemoveElementAt(ix, true);
          }
          catch (ex) { }
        }
      }

      RDFC.Init(kBMDS, rContainer);
      
      var bmType = this.getTarget(bookmarksTree.database, sourceID[i], RDF_NS + "type");
      if (!bmType) 
        kBMDS.Assert(rSource, rType, rBookmark, true);
      if (bmType == NC_NS + "Folder") {
        // If we're going to copy a folder type, we need to clone the folder 
        // rather than just asserting the new node as a child of the drop folder.
        if (aDragSession.dragAction & kCopyAction)
          rSource = BookmarksUtils.cloneFolder(rSource, rContainer, rTarget);
      }
      
      if (dropAction == "before" || dropAction == "after") {
        var dropIx = RDFC.IndexOf(rTarget);
        RDFC.InsertElementAt(rSource, dropAction == "after" ? ++dropIx : dropIx, true);
      }
      else
        RDFC.AppendElement(rSource); // drop on

      dirty = true;

      if (rParent) {
        gBookmarksShell.selectFolderItem(rContainer.Value, sourceID[i], additiveFlag);
        if (!additiveFlag) additiveFlag = true;
      }

      // If a name is supplied, we want to assert this information into the 
      // graph. E.g. user drags an internet shortcut to the app, we want to 
      // preserve not only the URL but the name of the shortcut. The other case
      // where we need to assert a name is when the node does not already exist
      // in the graph, in this case we'll just use the URL as the name.
      if (name[i] || nameRequired[i]) {
        var currentName = this.getTarget(bookmarksTree.database, sourceID[i], NC_NS + "Name");
        if (!currentName) {
          var rDefaultName = this.RDF.GetLiteral(name[i] || sourceID[i]);
          if (rDefaultName) {
            var rName = this.RDF.GetResource(NC_NS + "Name");
            kBMDS.Assert(rSource, rName, rDefaultName, true);
          }
        }
      }
    }

    /*
    if (numObjects > 1) {
      var bo = bookmarksTree.boxObject.QueryInterface(Components.interfaces.nsITreeBoxObject);
      bo.endBatch();
    }
    */
    
    if (dirty) {
      var remoteDS = kBMDS.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
      remoteDS.Flush();
    }    
  }
}

