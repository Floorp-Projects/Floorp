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

/*
  This is the old bookmarks code, included here for the sake of the bookmarks sidebar panel,
  which will be fixed to use my new code in .9. In the mean time, this file provides a 
  life line to various functionality. 
 */ 
 
 
var NC_NS  = "http://home.netscape.com/NC-rdf#";
var RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";



function TopLevelDrag ( event )
{
  return(true);
}


function BeginDragTree ( event )
{
  //XXX we rely on a capturer to already have determined which item the mouse was over
  //XXX and have set an attribute.
    
  // if the click is on the tree proper, ignore it. We only care about clicks on items.

  var tree = document.getElementById("bookmarksTree");
  if ( event.target == tree || event.target.localName == "treechildren" )
    return(true);         // continue propagating the event
    
  var childWithDatabase = tree;
  if ( ! childWithDatabase )
    return(false);
    
  var dragStarted = false;

  var trans = 
    Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
  if ( !trans ) return(false);

  var genData = 
    Components.classes["@mozilla.org/supports-wstring;1"].createInstance(Components.interfaces.nsISupportsWString);
  if (!genData) return(false);

  var genDataURL = 
    Components.classes["@mozilla.org/supports-wstring;1"].createInstance(Components.interfaces.nsISupportsWString);
  if (!genDataURL) return(false);

  trans.addDataFlavor("text/unicode");
  trans.addDataFlavor("moz/rdfitem");
        
  // ref/id (url) is on the <treeitem> which is two levels above the <treecell> which is
  // the target of the event.
  var id = event.target.parentNode.parentNode.getAttribute("ref");
  if (!id || id=="")
  {
	id = event.target.parentNode.parentNode.getAttribute("id");
  }

  var parentID = event.target.parentNode.parentNode.parentNode.parentNode.getAttribute("ref");
  if (!parentID || parentID == "")
  {
	parentID = event.target.parentNode.parentNode.parentNode.parentNode.getAttribute("id");
  }

  var trueID = id;
  if (parentID != null)
  {
  	trueID += "\n" + parentID;
  }
  genData.data = trueID;
  genDataURL.data = id;

  var database = childWithDatabase.database;
  var rdf = 
    Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
  if ((!rdf) || (!database))  { dump("CAN'T GET DATABASE\n"); return(false); }

  // make sure its a bookmark, bookmark separator, or bookmark folder
  var src = rdf.GetResource(id, true);
  var prop = rdf.GetResource(RDF_NS + "type", true);
  var target = database.GetTarget(src, prop, true);

  if (target) target = target.QueryInterface(Components.interfaces.nsIRDFResource);
  if (target) target = target.Value;
  if ((!target) || (target == "")) {dump("BAD\n"); return(false);}

  if ((target != NC_NS + "BookmarkSeparator") &&
    (target != NC_NS + "Bookmark") &&
    (target != NC_NS + "Folder")) return(false);

  trans.setTransferData ( "moz/rdfitem", genData, genData.data.length * 2);  // double byte data
  trans.setTransferData ( "text/unicode", genDataURL, genDataURL.data.length * 2);  // double byte data

  var transArray = 
    Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
  if ( !transArray )  return(false);

  // put it into the transferable as an |nsISupports|
  var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
  transArray.AppendElement(genTrans);
  
  var dragService = 
    Components.classes["@mozilla.org/widget/dragservice;1"].getService(Components.interfaces.nsIDragService);
  if ( !dragService ) return(false);

  var nsIDragService = Components.interfaces.nsIDragService;
  dragService.invokeDragSession ( event.target, transArray, null, nsIDragService.DRAGDROP_ACTION_COPY + 
                                     nsIDragService.DRAGDROP_ACTION_MOVE );
  dragStarted = true;

  return(!dragStarted);
}



function DragOverTree ( event )
{
  var validFlavor = false;
  var dragSession = null;
  var retVal = true;

  var dragService = 
    Components.classes["@mozilla.org/widget/dragservice;1"].getService(Components.interfaces.nsIDragService);
  if ( !dragService ) return(false);

  dragSession = dragService.getCurrentSession();
  if ( !dragSession ) return(false);

  if ( dragSession.isDataFlavorSupported("moz/rdfitem") ) validFlavor = true;
  else if ( dragSession.isDataFlavorSupported("text/unicode") ) validFlavor = true;
  //XXX other flavors here...

  // touch the attribute on the rowgroup to trigger the repaint with the drop feedback.
  if ( validFlavor )
  {
    var treeRoot = document.getElementById("bookmarksTree");
    if (!treeRoot)  return(false);
    var treeDatabase = treeRoot.database;
    if (!treeDatabase)  return(false);

    //XXX this is really slow and likes to refresh N times per second.
    var rowGroup = event.target.parentNode.parentNode;
    var sortActive = treeRoot.getAttribute("sortActive");
    if (sortActive == "true")
      rowGroup.setAttribute ( "dd-triggerrepaintsorted", 0 );
    else
      rowGroup.setAttribute ( "dd-triggerrepaint", 0 );

    dragSession.canDrop = true;
    // necessary??
    retVal = false;
  }
  return(retVal);
}



function DropOnTree ( event )
{
  var treeRoot = document.getElementById("bookmarksTree");
  if (!treeRoot)  return(false);
  var treeDatabase = treeRoot.database;
  if (!treeDatabase)  return(false);

  // for beta1, don't allow D&D if sorting is active
  var sortActive = treeRoot.getAttribute("sortActive");
  if (sortActive == "true")
  {
  	dump("Sorry, drag&drop is currently disabled when sorting is active.\n");
  	return(false);
  }

  var RDF = 
    Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
  if (!RDF) return(false);
  var RDFC =
    Components.classes["@mozilla.org/rdf/container;1"].getService(Components.interfaces.nsIRDFContainer);
  if (!RDFC)  return(false);

  var Bookmarks = RDF.GetDataSource("rdf:bookmarks");
  if (!Bookmarks) return(false);

  // target is the <treecell>, and "ref/id" is on the <treeitem> two levels above
  var treeItem = event.target.parentNode.parentNode;
  if (!treeItem)  return(false);

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
  {
    containerItem = treeItem.parentNode.parentNode;
  }

	// magical fix for bug # 33546: handle dropping after open container
	if (treeItem.getAttribute("container") == "true")
	{
		if (treeItem.getAttribute("open") == "true")
		{
			if (dropAction == "after")
			{
				dropAction = "before";
				containerItem = treeItem;

				// find <treechildren>, drop before first child
				var treeChildren = treeItem;
				treeItem = null;
				for (var x = 0; x < treeChildren.childNodes.length; x++)
				{
					if (treeChildren.childNodes[x].tagName == "treechildren")
					{
						treeItem = treeChildren.childNodes[x].childNodes[0];
						break;
					}
				}

				if (!treeItem)
				{
					dropAction = "on";
					containerItem = treeItem.parentNode.parentNode;
				}
			}
		}
	}

  var targetID = getAbsoluteID("bookmarksTree", treeItem);
  if (!targetID)  return(false);
  var targetNode = RDF.GetResource(targetID, true);
  if (!targetNode)  return(false);

  var containerID = getAbsoluteID("bookmarksTree", containerItem);
  if (!containerID) return(false);
  var containerNode = RDF.GetResource(containerID);
  if (!containerNode) return(false);

  var dragService = 
    Components.classes["@mozilla.org/widget/dragservice;1"].getService(Components.interfaces.nsIDragService);
  if ( !dragService ) return(false);
  
  var dragSession = dragService.getCurrentSession();
  if ( !dragSession ) return(false);

  var trans = 
    Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
  if ( !trans )   return(false);
  trans.addDataFlavor("moz/rdfitem");
  trans.addDataFlavor("text/x-moz-url");
  trans.addDataFlavor("text/unicode");

    var typeRes = RDF.GetResource(RDF_NS + "type");
    if (!typeRes) return false;
    var bmTypeRes = RDF.GetResource(NC_NS + "Bookmark");
    if (!bmTypeRes) return false;

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

    var sourceID = null;
    var parentID = null;
    var checkNameHack = false;
    var name=null;

    if (bestFlavor.value == "moz/rdfitem")
    {
	    // pull the URL out of the data object
	    var data = dataObj.data.substring(0, len.value / 2);

        // moz/rdfitem allows parent ID specified on next line; check for it
	    var cr = data.indexOf("\n");
	    if (cr >= 0)
	    {
	    	sourceID = data.substr(0, cr);
	    	parentID = data.substr(cr+1);
	    }
	    else
	    {
	        sourceID = data;
	    }
    }
    else if (bestFlavor.value == "text/x-moz-url")
    {
	    // pull the URL out of the data object
	    data = dataObj.data.substring(0, len.value / 2);
	    sourceID = data;

    	// we may need to synthesize a name (just use the URL)
    	checkNameHack = true;
    }
    else if (bestFlavor.value == "text/unicode")
    {
    	sourceID = dataObj.data;

    	// we may need to synthesize a name (just use the URL)
    	checkNameHack = true;
    }
    else
    {
        // unknown flavor, skip
        continue;
    }

    // pull the (optional) name out of the URL
    var separator = sourceID.indexOf("\n");
    if (separator >= 0)
    {
        name = sourceID.substr(separator+1);
        sourceID = sourceID.substr(0, separator);
    }

    var sourceNode = RDF.GetResource(sourceID, true);
    if (!sourceNode)  continue;

    var parentNode = null;
    if (parentID != null)
    {
    	parentNode = RDF.GetResource(parentID, true);
    }
    
    // Prevent dropping of a node before, after, or on itself
    if (sourceNode == targetNode) continue;
    // Prevent dropping of a node onto its parent container
    if ((dropAction == "on") && (containerID) && (containerID == parentID))	continue;

    RDFC.Init(Bookmarks, containerNode);

    // make sure appropriate bookmark type is set
    var bmTypeNode = Bookmarks.GetTarget( sourceNode, typeRes, true );
    if (!bmTypeNode)
    {
        // set default bookmark type
        Bookmarks.Assert(sourceNode, typeRes, bmTypeRes, true);
    }

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

	// select the newly added node
	if (parentID)
	{
	      selectDroppedItems(treeRoot, containerID, sourceID);
	}	     

      dirty = true;
    }
    else
    {
      // drop on
      RDFC.AppendElement(sourceNode);

	// select the newly added node
	if (parentID)
	{
	      selectDroppedItems(treeRoot, containerID, sourceID);
	}	     

      dirty = true;
    }

    if ((checkNameHack == true) || (name != null))
    {
    	var srcArc = RDF.GetResource(sourceID, true);
    	var propArc = RDF.GetResource(NC_NS + "Name", true);
    	if (srcArc && propArc && Bookmarks)
    	{
    		var targetArc = Bookmarks.GetTarget(srcArc, propArc, true);
    		if (!targetArc)
    		{
    		    // if no name, fallback to using the URL as the name
    			var defaultNameArc = RDF.GetLiteral((name != null && name != "") ? name : sourceID);
    			if (defaultNameArc)
    			{
    				Bookmarks.Assert(srcArc, propArc, defaultNameArc, true); 
    			}
    		}
    	}
    }
  }

	// should we move the node? (i.e. take it out of the source container?)
	if ((parentNode != null) && (containerNode != parentNode))
	{
	    try
	    {
    		RDFC.Init(Bookmarks, parentNode);
    		nodeIndex = RDFC.IndexOf(sourceNode);

    		if (nodeIndex >= 1)
    		{
    			RDFC.RemoveElementAt(nodeIndex, true, sourceNode);
    		}
        }
        catch(ex)
        {
        }
	}

  if (dirty == true)
  {
    var remote = Bookmarks.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
    if (remote)
    {
      remote.Flush();
    }
  }

  return(false);
}



function selectDroppedItems(treeRoot, containerID, targetID)
{
	var select_list = treeRoot.getElementsByAttribute("id", targetID);
	for (var x=0; x<select_list.length; x++)
	{
		var node = select_list[x];
		if (!node)	continue;

		var parent = node.parentNode.parentNode;
		if (!parent)	continue;

		var id = parent.getAttribute("ref");
		if (!id || id=="")
		{
			id = parent.getAttribute("id");
		}
		if (!id || id=="")	continue;

		if (id == containerID)
		{
			treeRoot.selectItem(node);
			break;
		}
	}
}
