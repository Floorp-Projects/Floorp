/* -*- Mode: Java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*
  Script for the bookmarks properties window
*/



function TopLevelDrag ( event )
{
	dump("TOP LEVEL bookmarks window got a drag\n");
	return(true);
}



function BeginDragTree ( event )
{
	//XXX we rely on a capturer to already have determined which item the mouse was over
	//XXX and have set an attribute.
    
	// if the click is on the tree proper, ignore it. We only care about clicks on items.

	var tree = document.getElementById("bookmarksTree");
	if ( event.target == tree )
		return(true);					// continue propagating the event
    
	var childWithDatabase = tree;
	if ( ! childWithDatabase )
		return(false);
    
	var dragStarted = false;

	var dragService = Components.classes["component://netscape/widget/dragservice"].getService();
	if ( dragService ) dragService = dragService.QueryInterface(Components.interfaces.nsIDragService);
	if ( !dragService )	return(false);

	var trans = Components.classes["component://netscape/widget/transferable"].createInstance();
	if ( trans ) trans = trans.QueryInterface(Components.interfaces.nsITransferable);
	if ( !trans )		return(false);

	var genData = Components.classes["component://netscape/supports-wstring"].createInstance();
	if ( genData ) genData = genData.QueryInterface(Components.interfaces.nsISupportsWString);
	if (!genData)		return(false);

	var genTextData = Components.classes["component://netscape/supports-string"].createInstance();
	if ( genTextData ) genTextData = genTextData.QueryInterface(Components.interfaces.nsISupportsString);
	if (!genTextData)	return(false);

        trans.addDataFlavor("text/plain");
        
	// id (url) is on the <treeitem> which is two levels above the <treecell> which is
	// the target of the event.
	var id = event.target.parentNode.parentNode.getAttribute("id");
	genData.data = id;
	genTextData.data = id;
	dump("ID: " + id + "\n");

	var database = childWithDatabase.database;
	var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
	if ((!rdf) || (!database))	{ dump("CAN'T GET DATABASE\n"); return(false); }

	// make sure its a bookmark, bookmark separator, or bookmark folder
	var src = rdf.GetResource(id, true);
	var prop = rdf.GetResource("http://www.w3.org/1999/02/22-rdf-syntax-ns#type", true);
	var target = database.GetTarget(src, prop, true);

	if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFResource);
	if (target)	target = target.Value;
	if ((!target) || (target == "")) {dump("BAD\n"); return(false);}
	dump("Type: '" + target + "'\n");

	if ((target != "http://home.netscape.com/NC-rdf#BookmarkSeparator") &&
		(target != "http://home.netscape.com/NC-rdf#Bookmark") &&
		(target != "http://home.netscape.com/NC-rdf#Folder"))	return(false);


//	trans.setTransferData ( "moz/toolbaritem", genData, id.length*2 );  // double byte data (len*2)
	trans.setTransferData ( "text/plain", genTextData, id.length );  // single byte data

	var transArray = Components.classes["component://netscape/supports-array"].createInstance();
	if ( transArray ) transArray = transArray.QueryInterface(Components.interfaces.nsISupportsArray);
	if ( !transArray )	return(false);

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
	var validFlavor = false;
	var dragSession = null;
	var retVal = true;

	var dragService = Components.classes["component://netscape/widget/dragservice"].getService();
	if ( dragService ) dragService = dragService.QueryInterface(Components.interfaces.nsIDragService);
	if ( !dragService )	return(false);

	dragSession = dragService.getCurrentSession();
	if ( !dragSession )	return(false);

	if ( dragSession.isDataFlavorSupported("moz/toolbaritem") )	validFlavor = true;
	else if ( dragSession.isDataFlavorSupported("text/plain") )	validFlavor = true;
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
	var RDF = Components.classes["component://netscape/rdf/rdf-service"].getService();
	if (RDF)	RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
	if (!RDF)	return(false);
	var RDFC = Components.classes["component://netscape/rdf/container"].getService();
	RDFC = RDFC.QueryInterface(Components.interfaces.nsIRDFContainer);
	if (!RDFC)	return(false);

	var Bookmarks = RDF.GetDataSource("rdf:bookmarks");
	if (!Bookmarks)	return(false);

	var treeRoot = document.getElementById("bookmarksTree");
	if (!treeRoot)	return(false);
	var treeDatabase = treeRoot.database;
	if (!treeDatabase)	return(false);

	// target is the <treecell>, and "id" is on the <treeitem> two levels above
	var treeItem = event.target.parentNode.parentNode;
	if (!treeItem)	return(false);
	var targetID = getAbsoluteID(treeRoot, treeItem);
	if (!targetID)	return(false);
	var targetNode = RDF.GetResource(targetID, true);
	if (!targetNode)	return(false);

	// get drop hint attributes
	var dropBefore = treeItem.getAttribute("dd-droplocation");
	var dropOn = treeItem.getAttribute("dd-dropon");

	// calculate drop action
	var dropAction;
	if (dropBefore == "true")	dropAction = "before";
	else if (dropOn == "true")	dropAction = "on";
	else				dropAction = "after";

	// calculate parent container node
	var containerItem = treeItem;
	if (dropAction != "on")
		containerItem = treeItem.parentNode.parentNode;

	var containerID = getAbsoluteID(treeRoot, containerItem);
	if (!containerID)	return(false);
	var containerNode = RDF.GetResource(containerID);
	if (!containerNode)	return(false);

	var dragService = Components.classes["component://netscape/widget/dragservice"].getService();
	if ( dragService ) dragService = dragService.QueryInterface(Components.interfaces.nsIDragService);
	if ( !dragService )	return(false);
	
	var dragSession = dragService.getCurrentSession();
	if ( !dragSession )	return(false);

	var trans = Components.classes["component://netscape/widget/transferable"].createInstance();
	if ( trans ) trans = trans.QueryInterface(Components.interfaces.nsITransferable);
	if ( !trans )		return(false);
	trans.addDataFlavor("text/plain");

	var bookmarksChangedFlag = false;
	for ( var i = 0; i < dragSession.numDropItems; ++i )
	{
		dragSession.getData ( trans, i );
		var dataObj = new Object();
		var bestFlavor = new Object();
		var len = new Object();
		trans.getAnyTransferData ( bestFlavor, dataObj, len );
		if ( dataObj )	dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);
		if ( !dataObj )	continue;

		// pull the URL out of the data object
		var sourceID = dataObj.data.substring(0, len.value);
		if (!sourceID)	continue;

		dump("    Node #" + i + ": drop '" + sourceID + "' " + dropAction + " '" + targetID + "'\n");

		var sourceNode = RDF.GetResource(sourceID, true);
		if (!sourceNode)	continue;
		
		// Prevent dropping of a node before, after, or on itself
		if (sourceNode == targetNode)	continue;

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
			if (nodeIndex < 1)	return(false);
			if (dropAction == "after")	++nodeIndex;

			RDFC.InsertElementAt(sourceNode, nodeIndex, true);
			bookmarksChangedFlag = true;
		}
		else
		{
			// drop on
			RDFC.AppendElement(sourceNode);
			bookmarksChangedFlag = true;
		}
	}

	if (bookmarksChangedFlag == true)
	{
		var remote = Bookmarks.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
		if (remote)
		{
			remote.Flush();
			dump("Wrote out bookmark changes.\n");
		}
	}

	return(false);
}



function copySelectionToClipboard()
{
	var treeNode = document.getElementById("bookmarksTree");
	if (!treeNode)    return(false);
	var select_list = treeNode.selectedItems;
	if (!select_list)	return(false);
	if (select_list.length < 1)    return(false);
	dump("# of Nodes selected: " + select_list.length + "\n\n");

	var RDF = Components.classes["component://netscape/rdf/rdf-service"].getService();
	RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
	if (!RDF)	return(false);

	var Bookmarks = RDF.GetDataSource("rdf:bookmarks");
	if (!Bookmarks)	return(false);

	var nameRes = RDF.GetResource("http://home.netscape.com/NC-rdf#Name");
	if (!nameRes)	return(false);

	// build a url that encodes all the select nodes as well as their parent nodes
	var url="";

	for (var nodeIndex=0; nodeIndex<select_list.length; nodeIndex++)
	{
		var node = select_list[nodeIndex];
		if (!node)    continue;
		var ID = node.getAttribute("id");
		if (!ID)    continue;
		
		var IDRes = RDF.GetResource(ID);
		if (!IDRes)	continue;
		var nameNode = Bookmarks.GetTarget(IDRes, nameRes, true);
		var theName = "";
		if (nameNode) nameNode = nameNode.QueryInterface(Components.interfaces.nsIRDFLiteral);
		if (nameNode)	theName = nameNode.Value;

		dump("Node " + nodeIndex + ": " + ID + "    name: " + theName + "\n");
		url += "ID:{" + ID + "};";
		url += "NAME:{" + theName + "};";
	}
	if (url == "")	return(false);
	dump("Copy URL: " + url + "\n\n");

	// get some useful components
	var trans = Components.classes["component://netscape/widget/transferable"].createInstance();
	if ( trans ) trans = trans.QueryInterface(Components.interfaces.nsITransferable);
	if ( !trans )	return(false);
	trans.addDataFlavor("text/unicode");

	var clip = Components.classes["component://netscape/widget/clipboard"].createInstance();
	if ( clip ) clip = clip.QueryInterface(Components.interfaces.nsIClipboard);
	if (!clip)	return(false);
	clip.emptyClipboard();

	// save bookmark's ID
	var data = Components.classes["component://netscape/supports-wstring"].createInstance();
	if ( data )	data = data.QueryInterface(Components.interfaces.nsISupportsWString);
	if (!data)	return(false);
	data.data = url;
	trans.setTransferData ( "text/unicode", data, url.length*2 );			// double byte data

	clip.setData(trans, null);
	return(true);
}



function doCut()
{
	if (copySelectionToClipboard() == true)
	{
		doDelete(false);
	}
	return(true);
}



function doCopy()
{
	copySelectionToClipboard();
	return(true);
}



function doPaste()
{
	var treeNode = document.getElementById("bookmarksTree");
	if (!treeNode)    return(false);
	var select_list = treeNode.selectedItems;
	if (!select_list)	return(false);
	if (select_list.length != 1)    return(false);
	
	var pasteNodeID = select_list[0].getAttribute("id");
	dump("Paste onto " + pasteNodeID + "\n");
	var isContainerFlag = select_list[0].getAttribute("container") == "true" ? true:false;
	dump("Container status: " + ((isContainerFlag) ? "true" : "false") + "\n\n");

	var clip = Components.classes["component://netscape/widget/clipboard"].createInstance();
	if ( clip ) clip = clip.QueryInterface(Components.interfaces.nsIClipboard);
	if (!clip)	return(false);

	var trans = Components.classes["component://netscape/widget/transferable"].createInstance();
	if ( trans ) trans = trans.QueryInterface(Components.interfaces.nsITransferable);
	if ( !trans )	return(false);
	trans.addDataFlavor("text/unicode");

	clip.getData(trans);
	var data = new Object();
	var dataLen = new Object();
	trans.getTransferData("text/unicode", data, dataLen);
	if (data)	data = data.value.QueryInterface(Components.interfaces.nsISupportsWString);
	var url=null;
	if (data)	url = data.data.substring(0, dataLen.value / 2);	// double byte data
	if (!url)	return(false);

	var strings = url.split(";");
	if (!strings)	return(false);

	var RDF = Components.classes["component://netscape/rdf/rdf-service"].getService();
	RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
	if (!RDF)	return(false);
	var RDFC = Components.classes["component://netscape/rdf/container"].getService();
	RDFC = RDFC.QueryInterface(Components.interfaces.nsIRDFContainer);
	if (!RDFC)	return(false);
	var Bookmarks = RDF.GetDataSource("rdf:bookmarks");
	if (!Bookmarks)	return(false);

	var nameRes = RDF.GetResource("http://home.netscape.com/NC-rdf#Name");
	if (!nameRes)	return(false);

	pasteNodeRes = RDF.GetResource(pasteNodeID);
	if (!pasteNodeRes)	return(false);
	var pasteContainerRes = null;
	var pasteNodeIndex = -1;
	if (isContainerFlag == true)
	{
		pasteContainerRes = pasteNodeRes;
	}
	else
	{
		var parID = select_list[0].parentNode.parentNode.getAttribute("ref");
		if (!parID)	parID = select_list[0].parentNode.parentNode.getAttribute("id");
		if (!parID)	return(false);
		pasteContainerRes = RDF.GetResource(parID);
		if (!pasteContainerRes)	return(false);
	}
	RDFC.Init(Bookmarks, pasteContainerRes);
	dump("Inited RDFC\n");

	if (isContainerFlag == false)
	{
		pasteNodeIndex = RDFC.IndexOf(pasteNodeRes);
		if (pasteNodeIndex < 0)	return(false);			// how did that happen?
	}

	dump("Loop over strings\n");

	for (var x=0; x<strings.length; x=x+2)
	{
		var theID = strings[x];
		var theName = strings[x+1];
		if ((theID.indexOf("ID:{") == 0) && (theName.indexOf("NAME:{") == 0))
		{
			theID = theID.substr(4, theID.length-5);
			theName = theName.substr(6, theName.length-7);
			dump("Paste  ID: " + theID + "    NAME: " + theName + "\n");

			var IDRes = RDF.GetResource(theID);
			if (!IDRes)	continue;

			if (theName != "")
			{
				var NameLiteral = RDF.GetLiteral(theName);
				if (NameLiteral)
				{
					Bookmarks.Assert(IDRes, nameRes, NameLiteral, true);
				}
			}

			if (isContainerFlag == true)
			{
				RDFC.AppendElement(IDRes);
				dump("Appended node onto end of container\n");
			}
			else
			{
				RDFC.InsertElementAt(IDRes, pasteNodeIndex++, false);	// XXX should probably be true
				dump("Pasted at index # " + pasteNodeIndex + "\n");
			}
		}
	}

	var remote = Bookmarks.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
	if (remote)
	{
		remote.Flush();
		dump("Wrote out bookmark changes.\n");
	}

	return(true);
}



function doDelete(promptFlag)
{
	var treeNode = document.getElementById("bookmarksTree");
	if (!treeNode)    return(false);
	var select_list = treeNode.selectedItems;
	if (!select_list)	return(false);
	if (select_list.length < 1)    return(false);

	dump("# of Nodes selected: " + select_list.length + "\n\n");

	if (promptFlag == true)
	{
		var ok = confirm("Delete the selected bookmark(s)?");
		if (!ok)	return(false);
	}

	var RDF = Components.classes["component://netscape/rdf/rdf-service"].getService();
	RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
	if (!RDF)	return(false);

	var RDFC = Components.classes["component://netscape/rdf/container"].getService();
	RDFC = RDFC.QueryInterface(Components.interfaces.nsIRDFContainer);
	if (!RDFC)	return(false);

	var Bookmarks = RDF.GetDataSource("rdf:bookmarks");
	if (!Bookmarks)	return(false);

	// note: backwards delete so that we handle odd deletion cases such as
	//       deleting a child of a folder as well as the folder itself
	for (var nodeIndex=select_list.length-1; nodeIndex>=0; nodeIndex--)
	{
		var node = select_list[nodeIndex];
		if (!node)    continue;
		var ID = node.getAttribute("id");
		if (!ID)    continue;
		var parentID = node.parentNode.parentNode.getAttribute("ref");
		if (!parentID)	parentID = node.parentNode.parentNode.getAttribute("id");
		if (!parentID)	continue;

		dump("Node " + nodeIndex + ": " + ID + "\n");
		dump("Parent Node " + nodeIndex + ": " + parentID + "\n");

		var IDRes = RDF.GetResource(ID);
		if (!IDRes)	continue;
		var parentIDRes = RDF.GetResource(parentID);
		if (!parentIDRes)	continue;

		RDFC.Init(Bookmarks, parentIDRes);
		RDFC.RemoveElement(IDRes, false);		// XXX should probably be true
	}

	var remote = Bookmarks.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
	if (remote)
	{
		remote.Flush();
		dump("Wrote out bookmark changes.\n");
	}
	return(true);
}



function doSelectAll()
{
	// XXX once selectAll() is implemented, use that
/*
	var treeNode = document.getElementById("bookmarksTree");
	if (!treeNode)    return(false);
	treeNode.selectAll();
*/
	return(true);
}



function doUnload()
{
    // Get the current window position/size.
    var x = window.screenX;
    var y = window.screenY;
    var h = window.outerHeight;
    var w = window.outerWidth;

    // Store these into the window attributes (for persistence).
    var win = document.getElementById( "bookmark-window" );
    win.setAttribute( "x", x );
    win.setAttribute( "y", y );
    win.setAttribute( "height", h );
    win.setAttribute( "width", w );
}



function BookmarkProperties()
{
	var treeNode = document.getElementById('bookmarksTree');
//	var select_list = treeNode.getElementsByAttribute("selected", "true");
	var select_list = treeNode.selectedItems;

	if (select_list.length >= 1)
	{
		// don't bother showing properties on bookmark separators
		var type = select_list[0].getAttribute('type');
		if (type != "http://home.netscape.com/NC-rdf#BookmarkSeparator")
		{
			var props = window.open("chrome://bookmarks/content/bm-props.xul",
						"BookmarkProperties", "chrome, menubar");
			props.BookmarkURL = select_list[0].getAttribute("id");
		}
	}
	else
	{
		dump("nothing selected!\n"); 
	}
	return(true);
}



function OpenSearch(tabName)
{
	window.openDialog("chrome://search/content/search.xul", "SearchWindow", "dialog=no,close,chrome,resizable", tabName, "");
	return(true);
}



function getAbsoluteID(root, node)
{
	var url = node.getAttribute("ref");
	if ((url == null) || (url == ""))
	{
		url = node.getAttribute("id");
	}
	try
	{
		var rootNode = document.getElementById(root);
		var ds = null;
		if (rootNode)
		{
			ds = rootNode.database;
		}

		// add support for anonymous resources such as Internet Search results,
		// IE favorites under Win32, and NetPositive URLs under BeOS
		var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
		if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
		if (rdf && ds)
		{
			var src = rdf.GetResource(url, true);
			var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
			var target = ds.GetTarget(src, prop, true);
			if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
			if (target)	target = target.Value;
			if (target)	url = target;
		}
	}
	catch(ex)
	{
	}
	return(url);
}



function OpenURL(event, node, root)
{
	if (node.getAttribute('container') == "true")	return(false);

	var url = getAbsoluteID(root, node);

	// Ignore "NC:" urls.
	if (url.substring(0, 3) == "NC:")	return(false);

 	// get right sized window
 	window.openDialog( "chrome://navigator/content/navigator.xul", "_blank", "chrome,all,dialog=no", url );
	return(true);
}



function doSort(sortColName)
{
	var node = document.getElementById(sortColName);
	// determine column resource to sort on
	var sortResource = node.getAttribute('resource');
	if (!node)	return(false);

	var sortDirection="ascending";
	var isSortActive = node.getAttribute('sortActive');
	if (isSortActive == "true")
	{
		var currentDirection = node.getAttribute('sortDirection');
		if (currentDirection == "ascending")
			sortDirection = "descending";
		else if (currentDirection == "descending")
			sortDirection = "natural";
		else	sortDirection = "ascending";
	}

	var isupports = Components.classes["component://netscape/rdf/xul-sort-service"].getService();
	if (!isupports)    return(false);
	var xulSortService = isupports.QueryInterface(Components.interfaces.nsIXULSortService);
	if (!xulSortService)    return(false);
	try
	{
		xulSortService.Sort(node, sortResource, sortDirection);
	}
	catch(ex)
	{
		dump("Exception calling xulSortService.Sort()\n");
	}
	return(false);
}



function fillContextMenu(name)
{
    if (!name)    return(false);
    var popupNode = document.getElementById(name);
    if (!popupNode)    return(false);

    // remove the menu node (which tosses all of its kids);
    // do this in case any old command nodes are hanging around
	for (var i = 0; i < popupNode.childNodes.length; i++) 
	{
	  popupNode.removeChild(popupNode.childNodes[0]);
	}

    var treeNode = document.getElementById("bookmarksTree");
    if (!treeNode)    return(false);
    var db = treeNode.database;
    if (!db)    return(false);
    
    var compositeDB = db.QueryInterface(Components.interfaces.nsIRDFDataSource);
    if (!compositeDB)    return(false);

    var isupports = Components.classes["component://netscape/rdf/rdf-service"].getService();
    if (!isupports)    return(false);
    var rdf = isupports.QueryInterface(Components.interfaces.nsIRDFService);
    if (!rdf)    return(false);

    var select_list = treeNode.selectedItems;
    if (!select_list)	return(false);
    if (select_list.length < 1)    return(false);
    
    dump("# of Nodes selected: " + select_list.length + "\n\n");

    // perform intersection of commands over selected nodes
    var cmdArray = new Array();

    for (var nodeIndex=0; nodeIndex<select_list.length; nodeIndex++)
    {
        var node = select_list[nodeIndex];
        if (!node)    break;
        var id = node.getAttribute("id");
        if (!id)    break;
        var rdfNode = rdf.GetResource(id);
        if (!rdfNode)    break;
        var cmdEnum = db.GetAllCmds(rdfNode);
        if (!cmdEnum)    break;

        var nextCmdArray = new Array();
        while (cmdEnum.HasMoreElements())
        {
            var cmd = cmdEnum.GetNext();
            if (!cmd)    break;
            if (nodeIndex == 0)
            {
                cmdArray[cmdArray.length] = cmd;
            }
            else
            {
                nextCmdArray[cmdArray.length] = cmd;
            }
        }
        if (nodeIndex > 0)
        {
            // perform command intersection calculation
            for (var cmdIndex = 0; cmdIndex < cmdArray.length; cmdIndex++)
            {
                var    cmdFound = false;
                for (var nextCmdIndex = 0; nextCmdIndex < nextCmdArray.length; nextCmdIndex++)
                {
                    if (nextCmdArray[nextCmdIndex] == cmdArray[cmdIndex])
                    {
                        cmdFound = true;
                        break;
                    }
                }
                if (cmdFound == false)
                {
                    cmdArray[cmdIndex] = null;
                }
            }
        }
    }

    // need a resource to ask RDF for each command's name
    var rdfNameResource = rdf.GetResource("http://home.netscape.com/NC-rdf#Name");
    if (!rdfNameResource)        return(false);

    // build up menu items
    if (cmdArray.length < 1)    return(false);

    for (var cmdIndex = 0; cmdIndex < cmdArray.length; cmdIndex++)
    {
        var cmd = cmdArray[cmdIndex];
        if (!cmd)        continue;
        var cmdResource = cmd.QueryInterface(Components.interfaces.nsIRDFResource);
        if (!cmdResource)    break;
        var cmdNameNode = compositeDB.GetTarget(cmdResource, rdfNameResource, true);
        if (!cmdNameNode)    break;
        cmdNameLiteral = cmdNameNode.QueryInterface(Components.interfaces.nsIRDFLiteral);
        if (!cmdNameLiteral)    break;
        cmdName = cmdNameLiteral.Value;
        if (!cmdName)        break;

        dump("Command #" + cmdIndex + ": id='" + cmdResource.Value + "'  name='" + cmdName + "'\n\n");

        var menuItem = document.createElement("menuitem");
        menuItem.setAttribute("value", cmdName);
        menuItem.setAttribute("oncommand", "return doContextCmd('" + cmdResource.Value + "');");
        
        popupNode.appendChild(menuItem);
    }

    return(true);
}



function doContextCmd(cmdName)
{
	dump("doContextCmd start: cmd='" + cmdName + "'\n");

	var treeNode = document.getElementById("bookmarksTree");
	if (!treeNode)    return(false);
	var db = treeNode.database;
	if (!db)    return(false);

	var compositeDB = db.QueryInterface(Components.interfaces.nsIRDFDataSource);
	if (!compositeDB)    return(false);

	var isupports = Components.classes["component://netscape/rdf/rdf-service"].getService();
	if (!isupports)    return(false);
	var rdf = isupports.QueryInterface(Components.interfaces.nsIRDFService);
	if (!rdf)    return(false);

	// need a resource for the command
	var cmdResource = rdf.GetResource(cmdName);
	if (!cmdResource)        return(false);
	cmdResource = cmdResource.QueryInterface(Components.interfaces.nsIRDFResource);
	if (!cmdResource)        return(false);

	var select_list = treeNode.selectedItems;
	if (!select_list)		return(false);
	if (select_list.length < 1)	return(false);

	dump("# of Nodes selected: " + select_list.length + "\n\n");

	// set up selection nsISupportsArray
	var selectionInstance = Components.classes["component://netscape/supports-array"].createInstance();
	var selectionArray = selectionInstance.QueryInterface(Components.interfaces.nsISupportsArray);

	// set up arguments nsISupportsArray
	var argumentsInstance = Components.classes["component://netscape/supports-array"].createInstance();
	var argumentsArray = argumentsInstance.QueryInterface(Components.interfaces.nsISupportsArray);

	// get argument (parent)
	var parentArc = rdf.GetResource("http://home.netscape.com/NC-rdf#parent");
	if (!parentArc)        return(false);

	for (var nodeIndex=0; nodeIndex<select_list.length; nodeIndex++)
	{
		var node = select_list[nodeIndex];
		if (!node)    break;
		var	uri = node.getAttribute("ref");
		if ((uri) || (uri == ""))
		{
			uri = node.getAttribute("id");
		}
		if (!uri)    return(false);

		var rdfNode = rdf.GetResource(uri);
		if (!rdfNode)    break;

		// add node into selection array
		selectionArray.AppendElement(rdfNode);

		// get the parent's URI
		var parentURI="";
		var	theParent = node;
		while (theParent)
		{
			theParent = theParent.parentNode;

			parentURI = theParent.getAttribute("ref");
			if ((!parentURI) || (parentURI == ""))
			{
				parentURI = theParent.getAttribute("id");
			}
			if (parentURI != "")	break;
		}
		if (parentURI == "")    return(false);

		var parentNode = rdf.GetResource(parentURI, true);
		if (!parentNode)	return(false);

		// add parent arc and node into arguments array
		argumentsArray.AppendElement(parentArc);
		argumentsArray.AppendElement(parentNode);
	}

	// do the command
	compositeDB.DoCommand( selectionArray, cmdResource, argumentsArray );

	dump("doContextCmd ends.\n\n");
	return(true);
}
