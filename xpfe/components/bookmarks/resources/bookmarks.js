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


function debug(msg)
{
  // uncomment for noise
  //dump(msg+"\n");
}


function copySelectionToClipboard()
{
	var treeNode = document.getElementById("bookmarksTree");
	if (!treeNode)    return(false);
	var select_list = treeNode.selectedItems;
	if (!select_list)	return(false);
	if (select_list.length < 1)    return(false);
	debug("# of Nodes selected: " + select_list.length + "\n");

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

		debug("Node " + nodeIndex + ": " + ID + "    name: " + theName);
		url += "ID:{" + ID + "};";
		url += "NAME:{" + theName + "};";
	}
	if (url == "")	return(false);
	debug("Copy URL: " + url);

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
	debug("Paste onto " + pasteNodeID);
	var isContainerFlag = select_list[0].getAttribute("container") == "true" ? true:false;
	debug("Container status: " + ((isContainerFlag) ? "true" : "false"));

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

	debug("Inited RDFC");

	if (isContainerFlag == false)
	{
		pasteNodeIndex = RDFC.IndexOf(pasteNodeRes);
		if (pasteNodeIndex < 0)	return(false);			// how did that happen?
	}

	debug("Loop over strings");

	var dirty = false;

	for (var x=0; x<strings.length; x=x+2)
	{
		var theID = strings[x];
		var theName = strings[x+1];
		if ((theID.indexOf("ID:{") == 0) && (theName.indexOf("NAME:{") == 0))
		{
			theID = theID.substr(4, theID.length-5);
			theName = theName.substr(6, theName.length-7);
			debug("Paste  ID: " + theID + "    NAME: " + theName);

			var IDRes = RDF.GetResource(theID);
			if (!IDRes)	continue;

			if (RDFC.IndexOf(IDRes) > 0)
			{
				debug("Unable to add ID:'" + theID + "' as its already in this folder.\n");
				continue;
			}

			if (theName != "")
			{
				var NameLiteral = RDF.GetLiteral(theName);
				if (NameLiteral)
				{
					Bookmarks.Assert(IDRes, nameRes, NameLiteral, true);
					dirty = true;
				}
			}

			if (isContainerFlag == true)
			{
				RDFC.AppendElement(IDRes);
				debug("Appended node onto end of container");
			}
			else
			{
				RDFC.InsertElementAt(IDRes, pasteNodeIndex++, true);
				debug("Pasted at index # " + pasteNodeIndex);
			}
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

	return(true);
}



function doDelete(promptFlag)
{
	var treeNode = document.getElementById("bookmarksTree");
	if (!treeNode)    return(false);
	var select_list = treeNode.selectedItems;
	if (!select_list)	return(false);
	if (select_list.length < 1)    return(false);

	debug("# of Nodes selected: " + select_list.length);

	if (promptFlag == true)
	{
		var bundle = srGetStrBundle("chrome://bookmarks/locale/bookmark.properties");
		var deleteStr = bundle.GetStringFromName("DeleteItems");
		var ok = confirm(deleteStr);
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

	var dirty = false;

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

		debug("Node " + nodeIndex + ": " + ID);
		debug("Parent Node " + nodeIndex + ": " + parentID);

		var IDRes = RDF.GetResource(ID);
		if (!IDRes)	continue;
		var parentIDRes = RDF.GetResource(parentID);
		if (!parentIDRes)	continue;

		RDFC.Init(Bookmarks, parentIDRes);
		RDFC.RemoveElement(IDRes, true);
		dirty = true;
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

	return(true);
}



function doSelectAll()
{
	var treeNode = document.getElementById("bookmarksTree");
	if (!treeNode)    return(false);
	treeNode.selectAll();
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
          window.openDialog("chrome://bookmarks/content/bm-props.xul",
                            "_blank", "chrome,menubar",
                            select_list[0].getAttribute("id"));
        }
    }
  else
    {
      debug("nothing selected!\n"); 
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
	if ((event.button != 1) || (event.clickCount != 2) || (node.nodeName != "treeitem"))
		return(false);

	if (node.getAttribute("container") == "true")
		return(false);

	var url = getAbsoluteID(root, node);

	// Ignore "NC:" urls.
	if (url.substring(0, 3) == "NC:")
		return(false);

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
		debug("Exception calling xulSortService.Sort()");
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
	while (popupNode.childNodes.length)
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

    var target_item = document.popupNode.parentNode.parentNode;
    if (target_item && target_item.nodeName == "treeitem")
    {
	    if (target_item.getAttribute('selected') != 'true') {
	      treeNode.selectItem(target_item);
	    }
    }

    var select_list = treeNode.selectedItems;

    debug("# of Nodes selected: " + treeNode.selectedItems.length + "\n");

    // perform intersection of commands over selected nodes
    var cmdArray = new Array();
    var selectLength = select_list.length;
    if (selectLength == 0)	selectLength = 1;
    for (var nodeIndex=0; nodeIndex < selectLength; nodeIndex++)
    {
    	var id = null;

	// if nothing is selected, get commands for the "ref" of the bookmarks root
    	if (select_list.length == 0)
    	{
		id = treeNode.getAttribute("ref");
	        if (!id)    break;
    	}
    	else
    	{
	        var node = select_list[nodeIndex];
	        if (!node)    break;
	        id = node.getAttribute("id");
	        if (!id)    break;
	}
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

/*
    // build up menu items
    if (cmdArray.length < 1)    return(false);
*/

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

        debug("Command #" + cmdIndex + ": id='" + cmdResource.Value + "'  name='" + cmdName + "'");

        var menuItem = document.createElement("menuitem");
        menuItem.setAttribute("value", cmdName);        
        popupNode.appendChild(menuItem);
        // work around bug # 26402 by setting "oncommand" attribute AFTER appending menuitem
        menuItem.setAttribute("oncommand", "return doContextCmd('" + cmdResource.Value + "');");
    }

	// if one and only one node is selected
	if (treeNode.selectedItems.length == 1)
	{
		// and its a bookmark or a bookmark folder (there can be other types,
		// not just separators, so check explicitly for what we allow)
		var type = select_list[0].getAttribute("type");
		if ((type == "http://home.netscape.com/NC-rdf#Bookmark") ||
			(type == "http://home.netscape.com/NC-rdf#Folder"))
		{
			// then add a menu separator (if necessary)
			if (popupNode.childNodes.length > 0)
			{
				var menuSep = document.createElement("menuseparator");
				popupNode.appendChild(menuSep);
			}

			// and then add a "Properties" menu items
			var bundle = srGetStrBundle("chrome://bookmarks/locale/bookmark.properties");
			var propMenuName = bundle.GetStringFromName("BookmarkProperties");
			var menuItem = document.createElement("menuitem");
			menuItem.setAttribute("value", propMenuName);
			popupNode.appendChild(menuItem);
		        // work around bug # 26402 by setting "oncommand" attribute AFTER appending menuitem
			menuItem.setAttribute("oncommand", "return BookmarkProperties();");
		}
	}

    return(true);
}



function doContextCmd(cmdName)
{
	debug("doContextCmd start: cmd='" + cmdName + "'");

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

	// set up selection nsISupportsArray
	var selectionInstance = Components.classes["component://netscape/supports-array"].createInstance();
	var selectionArray = selectionInstance.QueryInterface(Components.interfaces.nsISupportsArray);

	// set up arguments nsISupportsArray
	var argumentsInstance = Components.classes["component://netscape/supports-array"].createInstance();
	var argumentsArray = argumentsInstance.QueryInterface(Components.interfaces.nsISupportsArray);

	// get argument (parent)
	var parentArc = rdf.GetResource("http://home.netscape.com/NC-rdf#parent");
	if (!parentArc)        return(false);

	var select_list = treeNode.selectedItems;
	debug("# of Nodes selected: " + select_list.length);

	if (select_list.length < 1)
	{
		// if nothing is selected, default to using the "ref" on the root of the tree
		var	uri = treeNode.getAttribute("ref");
		if (!uri || uri=="")    return(false);
		var rdfNode = rdf.GetResource(uri);
		// add node into selection array
		if (rdfNode)
		{
			selectionArray.AppendElement(rdfNode);
		}
	}
	else for (var nodeIndex=0; nodeIndex<select_list.length; nodeIndex++)
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

	debug("doContextCmd ends.");
	return(true);
}

function dumpAttributes(node) {
  debug("Attributes for " + node.nodeName);
  debug("type=" + node.nodeType);
  debug("value=" + node.nodeValue);

  var attributes = node.attributes
  if (!attributes || attributes.length == 0) {
    debug("no attributes")
  }
  for (var ii=0; ii < attributes.length; ii++) {
    var attr = attributes.item(ii)
    debug("att "+ii+": "+ attr.name +"="+attr.value)
  }
}
