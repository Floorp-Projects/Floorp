/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Ben Goodger <ben@netscape.com> (Original Author)
 */

var gR_NC_child = null;

const kNC_NS = "http://home.netscape.com/NC-rdf#";

const kToolboxID = "navigator-toolbox";

var ToolbarShell = {
  
  RDFSvc : Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService),
  RDFC   : Components.classes["@mozilla.org/rdf/container;1"].getService(Components.interfaces.nsIRDFContainer),
  
  initToolbars: function ()
  {
    var toolbox = document.getElementById(kToolboxID);
  
    var rdfService = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
    rdfService = rdfService.QueryInterface(Components.interfaces.nsIRDFService);
    
    // Add Datasources to toolbox
    // NOTE: This is for navigator toolbar datasource hookup ONLY. Datasources
    //       from other components must add themselves separately. 
    
    var toolbars = rdfService.GetDataSource("chrome://navigator/content/navigator-toolbars.rdf");
    toolbox.database.AddDataSource(toolbars);
    
    var commands = rdfService.GetDataSource("chrome://navigator/content/navigator-commands.rdf");
    toolbox.database.AddDataSource(commands);
        
    // XXX - shift to applicable component overlays
    var bmds = rdfService.GetDataSource("rdf:bookmarks");
    toolbox.database.AddDataSource(bmds);
    
    var sidebarDS = rdfService.GetDataSource(get_sidebar_datasource_uri());
    toolbox.database.AddDataSource(sidebarDS);

    toolbox.builder.rebuild();

    // globals    
    gR_NC_child = rdfService.GetResource(kNC_NS + "child", true);
    
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // @param aNode           wstring Node URI being inserted
  // @param aRelativeNode   wstring Node URI being inserted into/adjacent to
  // @param aParentNode     wstring Node URI of parent
  // @param aPosition       boolean true = before, false = after
  insertNode: function (aNode, aRelativeNode, aParentNode, aPosition)
  {
    var toolbox = document.getElementById(kToolboxID);
    var db = toolbox.database;
    
    var rdfService = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
    rdfService = rdfService.QueryInterface(Components.interfaces.nsIRDFService);

    // Obtain RDF Resources for the node URIs
    var rNode = rdfService.GetResource(aNode, true);
    var rRelNode = rdfService.GetResource(aRelativeNode, true);
    
    const cUtilsContractID = "@mozilla.org/rdf/container-utils;1";
    const cUtilsIID = Components.interfaces.nsIRDFContainerUtils;
    var cUtils = Components.classes[cUtilsContractID].getService();
    cUtils = cUtils.QueryInterface(cUtilsIID);    
    
    const cContractID = "@mozilla.org/rdf/container;1";
    const cIID = Components.interfaces.nsIRDFContainer;
    var container = Components.classes[cContractID].getService();
    container = container.QueryInterface(cIID);
        
    // If the relative and parent nodes are the same, the node is a container
    // and we want to append to it. 
    if (aRelativeNode == aParentNode) {
      try {
        // Relative Node is a bona fide container, init container and append.
        container.Init(db, rRelNode);
        container.AppendElement(rNode);
      }
      catch (e) {
        // No container, just assert a child relationship
        this.conditionalAssert(rRelNode, gR_NC_child, rNode, true);
      }
    }
    else {
      // Insert relative to a given node and its parent. 
      try {
        var rParentNode = rdfService.GetResource(aParentNode, true);
        
        // See if our parent is a container, if it is, we can just append directly
        // to it. 
        if (cUtils.IsContainer(db, rParentNode)) {
          container.Init(db, rParentNode);
          var ix = container.IndexOf(rRelNode);
          container.InsertElementAt(rNode, ix + (aPosition ? 0 : 1), true);
        } 
        else {
          // Otherwise, we're higher up in the graph and need to navigate down
          // into our children looking for sequences...
          var childSeqs = db.GetTargets(rParentNode, gR_NC_child, true);
          while (childSeqs.hasMoreElements()) {
            var currSeq = childSeqs.getNext();
            currSeq = currSeq.QueryInterface(Components.interfaces.nsIRDFResource);

            if (cUtils.IsContainer(db, currSeq)) {
              container.Init(db, currSeq);
              var ix = container.IndexOf(rRelNode);
              dump("*** index of relative node is " + ix + "\n");
              DUMP_seq(rdfService, container, db, currSeq, rRelNode);
              if (ix >= 1) {
                // This is the right container. It has the node we care about. 
                // Append to it. 
                container.InsertElementAt(rNode, ix + (aPosition ? 0 : 1), true);
                DUMP_seq(rdfService, container, db, currSeq, rRelNode);
                return;
              }
              // Otherwise, continue to inspect child containers. 
            }
          }
        }
      }
      catch (e) { throw e }
    }
  },
  
  removeNode: function (aNode, aParent)
  {
    var toolbox = document.getElementById(kToolboxID);
    var db = toolbox.database;
    
    // If we want to append aNode to aRelativeNode, a shortcut is to assert a 
    // child property 
    const cUtilsContractID = "@mozilla.org/rdf/container-utils;1";
    const cUtilsIID = Components.interfaces.nsIRDFContainerUtils;
    var containerUtils = Components.classes[cUtilsContractID].getService();
    containerUtils = containerUtils.QueryInterface(cUtilsIID);
    
    const cContractID = "@mozilla.org/rdf/container;1";
    const cIID = Components.interfaces.nsIRDFContainer;
    var container = Components.classes[cContractID].getService();
    container = container.QueryInterface(cIID);
      
    var rdfService = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
    rdfService = rdfService.QueryInterface(Components.interfaces.nsIRDFService);

    var rParent = rdfService.GetResource(aParent, true);
    var rNode = rdfService.GetResource(aNode, true);
    
    if (containerUtils.IsContainer(db, rParent)) {
      // If the parent is a container, remove aNode from it. 
      container.Init(db, rParent);
      container.RemoveElement(rNode, true);
    }
    else {
      // Otherwise (the likely case) the parent is not itself a container, but
      // a resource with a series of child arcs out to containers. Iterate over
      // these child arcs, and compare their URIs with the aParentURI. When we
      // find that, we can remove aNodeURI from it safely. 
      var childSeqs = db.GetTargets(rParent, gR_NC_child, true);
      while (childSeqs.hasMoreElements()) {
        var currSeq = childSeqs.getNext();
        currSeq = currSeq.QueryInterface(Components.interfaces.nsIRDFResource);
        
        if (containerUtils.IsContainer(db, currSeq)) {
          container.Init(db, currSeq);
          var ix = container.IndexOf(rNode);
          if (ix >= 0) {
            container.RemoveElement(rNode, true);
            return;
          }
        }
      }
    }
  },

  copy: function (aNode, aIID)
  {
    this._copy(aNode, aIID);
  },
  
  conditionalAssert: function (aSource, aProperty, aTarget, aTruthValue)
  {
    var toolbox = document.getElementById(kToolboxID);
    var db = toolbox.database;
    if (!db) return;
    
    if (db.HasAssertion(aSource, aProperty, aTarget, aTruthValue)) {
      var currValue = db.GetTarget(aSource, aProperty, true);
      db.Change(aSource, aProperty, currValue, aTarget);
    }
    else
      db.Assert(aSource, aProperty, aTarget, aTruthValue);
  },
  
  getProperty: function (aSourceURI, aPropertyURI)
  {
    var rdfService = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
    rdfService = rdfService.QueryInterface(Components.interfaces.nsIRDFService);
    
    var toolbox = document.getElementById(kToolboxID);
    var db = toolbox.database;

    var rSource = rdfService.GetResource(aSourceURI, true);
    var rURL    = rdfService.GetResource(aPropertyURI, true);
    
    var URL = db.GetTarget(rSource, rURL, true);
    if (!URL) return null;
    try { 
      URL = URL.QueryInterface(Components.interfaces.nsIRDFResource);
    }
    catch (e) {
      URL = URL.QueryInterface(Components.interfaces.nsIRDFLiteral);
    }
    return URL.Value;
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // Drag And Drop Observer Implementation
  onDragStart: function (aEvent)
  {
    // targetURI is the unique identifier of this node in the RDF graph. 
    // targetURL is its URL property, which we will want for creation of HTML
    //           links, etc.
    // targetName is the short descriptive label.
    // targetDesc is the longer descriptive label
    var targetURI   = aEvent.target.id;
    var targetURL   = this.getProperty(targetURI, kNC_NS + "URL");
    var targetDesc  = this.getProperty(targetURI, kNC_NS + "Description");
    var targetName  = this.getProperty(targetURI, kNC_NS + "Name");

    var flavourList = { };

    if (targetURI) {
      // For moz/toolbaritem, transferring the URI is enough as we always remain 
      // in the context of the graph, and we can always figure out what we need.
      flavourList["moz/toolbaritem"] = { width: 2, data: targetURI };
      // For simple URLs, transport the Name and the Location
      flavourList["text/x-moz-url"] = { width: 2, data: targetURL + "\n" + targetName };
      // For HTML Links, take the Name, Location and descriptive text for 
      // the TITLE tooltip. 
      var htmlString = "";
      if (targetDesc) 
        htmlString = "<a href=\"" + targetURL + "\" title=\"" + targetDesc + "\">" + targetName + "</a>";
      else
        htmlString = "<a href=\"" + targetURL + "\">" + targetName + "</a>";
      flavourList["text/html"] = { width: 2, data: htmlString };
      // For the most basic case, simply take the URL. 
      flavourList["text/unicode"] = { width: 2, data: targetURL };
    }
    
    DUMP_flavours(flavourList);
  
    return flavourList;
  },
  
  onDragOver: function (aEvent, aFlavourList, aDragSession)
  {
    // Handle Drag & Drop Feedback
  },
  
  onDragExit: function (aEvent, aDragSession)
  {
    // Handle Drag & Drop Feedback
  },
  
  onDrop: function (aEvent, aDragData, aDragSession)
  {
    // Find the node dropped on
    var dropNode = aEvent.target;
    
    // Get Client Coords and Widget Bounds
    var cX = aEvent.clientX;
    var wX = dropNode.boxObject.x;
    var wW = dropNode.boxObject.width;
    
    var relNodeURI = dropNode.id;
    var parentNodeURI = aEvent.target.parentNode.id;
    
    // bounds check    
    if (cX > wX && cX < (wX + wW)) {
      // XXX - need to handle containers here, which will mean creating three
      //       dropzones rather than 2. 
      if (cX > wX && cX < (wX + (wW/2))) {
        // Drop to the left
        if (aDragData.flavour == "moz/toolbaritem") {
          try {
            this.insertNode(aDragData.data.data, relNodeURI, parentNodeURI, true);
            // if insertNode fails, it will throw, and we won't remove the node
            // from the destination
            this.removeNode(aDragData.data.data, parentNodeURI);
          }
          catch (e) {
            dump("*** ToolbarShell::insertNode failed for some reason\n");
            dump("*** Reason: " + e + "\n");
          }
        }
      }
      else if (cX > (wX + (wW/2)) && cX < (wX + wW)) {
        // Drop to the right
        if (aDragData.flavour == "moz/toolbaritem") {
          try {
            this.insertNode(aDragData.data.data, relNodeURI, parentNodeURI, false);
            // if insertNode fails, it will throw, and we won't remove the node
            // from the destination
            this.removeNode(aDragData.data.data, parentNodeURI);
          }
          catch (e) {
            dump("*** ToolbarShell::insertNode failed for some reason\n");
            dump("*** Reason: " + e + "\n");
          }
        }
      }
      else if (0) {
        // Drop On
      }
    }
  },

  getSupportedFlavours: function () 
  {
    var flavourList = { };
    flavourList["moz/toolbaritem"]  = { width: 2, iid: "nsISupportsWString" };
    flavourList["text/x-moz-url"]   = { width: 2, iid: "nsISupportsWString" };
    flavourList["text/html"]        = { width: 2, iid: "nsISupportsWString" };
    flavourList["text/unicode"]     = { width: 2, iid: "nsISupportsWString" };
    return flavourList;
  }
  
  
};

function DUMP_flavours(aFlavourList)
{
  for (var flavour in aFlavourList)
    dump("*** Flavour " + flavour + " has data : " + aFlavourList[flavour].data + "\n");
}

function DUMP_seq(aRDFS, aContainer, aDB, aSequence, aRelNode)
{
  var res = aContainer.GetElements();
  var count = 1;
  while (res.hasMoreElements()) {
    var currRes = res.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
    if (currRes.Value == aRelNode.Value)
      dump("[[ #" + count++ + " * " + currRes.Value + "\n");
    else
      dump("[[ #" + count++ + "   " + currRes.Value + "\n");
  }
}

addEventListener("load", ToolbarShell.initToolbars, false);

