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
 */


//
// Determine if d&d is on or not, off by default for beta but we want mozilla
// folks to be able to turn it on if they so desire.
//
var gDragDropEnabled = false;
var pref = null;
try {
  pref = Components.classes['component://netscape/preferences'];
  pref = pref.getService();
  pref = pref.QueryInterface(Components.interfaces.nsIPref);
}
catch (ex) {
  dump("failed to get prefs service!\n");
  pref = null;
}

try {
  gDragDropEnabled = pref.GetBoolPref("xpfe.dragdrop.enable");
}
catch (ex) {
  dump("assuming d&d is off for Bookmarks\n");
}  


function TopLevelDrag ( event )
{
  debug("TOP LEVEL bookmarks window got a drag");
  return(true);
}


function BeginDragTree ( event )
{
  if ( !gDragDropEnabled )
    return;
    
  //XXX we rely on a capturer to already have determined which item the mouse was over
  //XXX and have set an attribute.
    
  // if the click is on the tree proper, ignore it. We only care about clicks on items.

  var tree = document.getElementById("bookmarksTree");
  if ( event.target == tree )
    return(true);         // continue propagating the event
    
  var childWithDatabase = tree;
  if ( ! childWithDatabase )
    return(false);
    
  var dragStarted = false;

  var dragService = 
    Components.classes["component://netscape/widget/dragservice"].getService(Components.interfaces.nsIDragService);
  if ( !dragService ) return(false);

  var trans = 
    Components.classes["component://netscape/widget/transferable"].createInstance(Components.interfaces.nsITransferable);
  if ( !trans ) return(false);

  var genData = 
    Components.classes["component://netscape/supports-wstring"].createInstance(Components.interfaces.nsISupportsWString);
  if (!genData) return(false);

  trans.addDataFlavor("text/unicode");
        
  // id (url) is on the <treeitem> which is two levels above the <treecell> which is
  // the target of the event.
  var id = event.target.parentNode.parentNode.getAttribute("id");
  genData.data = id;
  genTextData.data = id;
  debug("ID: " + id);

  var database = childWithDatabase.database;
  var rdf = 
    Components.classes["component://netscape/rdf/rdf-service"].getService(Components.interfaces.nsIRDFService);
  if ((!rdf) || (!database))  { dump("CAN'T GET DATABASE\n"); return(false); }

  // make sure its a bookmark, bookmark separator, or bookmark folder
  var src = rdf.GetResource(id, true);
  var prop = rdf.GetResource("http://www.w3.org/1999/02/22-rdf-syntax-ns#type", true);
  var target = database.GetTarget(src, prop, true);

  if (target) target = target.QueryInterface(Components.interfaces.nsIRDFResource);
  if (target) target = target.Value;
  if ((!target) || (target == "")) {dump("BAD\n"); return(false);}
  debug("Type: '" + target + "'");

  if ((target != "http://home.netscape.com/NC-rdf#BookmarkSeparator") &&
    (target != "http://home.netscape.com/NC-rdf#Bookmark") &&
    (target != "http://home.netscape.com/NC-rdf#Folder")) return(false);


//  trans.setTransferData ( "moz/toolbaritem", genData, id.length*2 );  // double byte data (len*2)
  trans.setTransferData ( "text/unicode", genData, id.length * 2);  // double byte data

  var transArray = 
    Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
  if ( !transArray )  return(false);

  // put it into the transferable as an |nsISupports|
  var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
  transArray.AppendElement(genTrans);
  var nsIDragService = Components.interfaces.nsIDragService;
  dragService.invokeDragSession ( transArray, null, nsIDragService.DRAGDROP_ACTION_COPY + 
                                     nsIDragService.DRAGDROP_ACTION_MOVE );
  dragStarted = true;

  return(!dragStarted);  // don't propagate the event if a drag has begun
}



function DragOverTree ( event )
{
  if ( !gDragDropEnabled )
    return;
    
  var validFlavor = false;
  var dragSession = null;
  var retVal = true;

  var dragService = 
    Components.classes["component://netscape/widget/dragservice"].getService(Components.interfaces.nsIDragService);
  if ( !dragService ) return(false);

  dragSession = dragService.getCurrentSession();
  if ( !dragSession ) return(false);

  if ( dragSession.isDataFlavorSupported("moz/toolbaritem") ) validFlavor = true;
  else if ( dragSession.isDataFlavorSupported("text/unicode") ) validFlavor = true;
  //XXX other flavors here...

  // touch the attribute on the rowgroup to trigger the repaint with the drop feedback.
  if ( validFlavor )
  {
    //XXX this is really slow and likes to refresh N times per second.
    var rowGroup = event.target.parentNode.parentNode;
    rowGroup.setAttribute ( "dd-triggerrepaint", 0 );
    dragSession.canDrop = true;
    // necessary??
    retVal = false; // do not propagate message
  }
  return(retVal);
}



function DropOnTree ( event )
{
  if ( !gDragDropEnabled )
    return;
    
  var RDF = 
    Components.classes["component://netscape/rdf/rdf-service"].getService(Components.interfaces.nsIRDFService);
  if (!RDF) return(false);
  var RDFC =
    Components.classes["component://netscape/rdf/container"].getService(Components.interfaces.nsIRDFContainer);
  if (!RDFC)  return(false);

  var Bookmarks = RDF.GetDataSource("rdf:bookmarks");
  if (!Bookmarks) return(false);

  var treeRoot = document.getElementById("bookmarksTree");
  if (!treeRoot)  return(false);
  var treeDatabase = treeRoot.database;
  if (!treeDatabase)  return(false);

  // target is the <treecell>, and "id" is on the <treeitem> two levels above
  var treeItem = event.target.parentNode.parentNode;
  if (!treeItem)  return(false);
  var targetID = getAbsoluteID(treeRoot, treeItem);
  if (!targetID)  return(false);
  var targetNode = RDF.GetResource(targetID, true);
  if (!targetNode)  return(false);

  // get drop hint attributes
  var dropBefore = treeItem.getAttribute("dd-droplocation");
  var dropOn = treeItem.getAttribute("dd-dropon");

  // calculate drop action
  var dropAction;
  if (dropBefore == "true") dropAction = "before";
  else if (dropOn == "true")  dropAction = "on";
  else        dropAction = "after";

  // calculate parent container node
  var containerItem = treeItem;
  if (dropAction != "on")
    containerItem = treeItem.parentNode.parentNode;

  var containerID = getAbsoluteID(treeRoot, containerItem);
  if (!containerID) return(false);
  var containerNode = RDF.GetResource(containerID);
  if (!containerNode) return(false);

  var dragService = 
    Components.classes["component://netscape/widget/dragservice"].getService(Components.interfaces.nsIDragService);
  if ( !dragService ) return(false);
  
  var dragSession = dragService.getCurrentSession();
  if ( !dragSession ) return(false);

  var trans = 
    Components.classes["component://netscape/widget/transferable"].createInstance(Components.interfaces.nsITransferable);
  if ( !trans )   return(false);
  trans.addDataFlavor("text/unicode");

  var dirty = false;

  for ( var i = 0; i < dragSession.numDropItems; ++i )
  {
    dragSession.getData ( trans, i );
    var dataObj = new Object();
    var bestFlavor = new Object();
    var len = new Object();
    trans.getAnyTransferData ( bestFlavor, dataObj, len );
    if ( dataObj )  dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsWString);
    if ( !dataObj ) continue;

    // pull the URL out of the data object
    var sourceID = dataObj.data;
    if (!sourceID)  continue;

    debug("    Node #" + i + ": drop '" + sourceID + "' " + dropAction + " '" + targetID + "'");

    var sourceNode = RDF.GetResource(sourceID, true);
    if (!sourceNode)  continue;
    
    // Prevent dropping of a node before, after, or on itself
    if (sourceNode == targetNode) continue;

    RDFC.Init(Bookmarks, containerNode);

    if ((dropAction == "before") || (dropAction == "after"))
    {
      // drop before or after
      var nodeIndex;

      nodeIndex = RDFC.IndexOf(sourceNode);
      if (nodeIndex >= 1)
      {
        // moving a node around inside of the container
        // so remove, then re-add the node
        RDFC.RemoveElementAt(nodeIndex, true, sourceNode);
      }
      
      nodeIndex = RDFC.IndexOf(targetNode);
      if (nodeIndex < 1)  return(false);
      if (dropAction == "after")  ++nodeIndex;

      RDFC.InsertElementAt(sourceNode, nodeIndex, true);
      dirty = true;
    }
    else
    {
      // drop on
      RDFC.AppendElement(sourceNode);
      dirty = true;
    }
  }

  if (dirty == true)
  {
    var remote = Bookmarks.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
    if (remote)
    {
      remote.Flush();
      debug("Wrote out bookmark changes.");
    }
  }

  return(false);
}
