/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
  Script for the bookmarks properties window
*/



function BookmarkProperties()
{
  var tree = document.getElementById('bookmarksTree');
  var select_list = tree.getElementsByAttribute("selected", "true");

  if (select_list.length >= 1) {

	// don't bother showing properties on bookmark separators
	var type = select_list[0].getAttribute('type');
        if (type != "http://home.netscape.com/NC-rdf#BookmarkSeparator")
        {
		var props = window.open("chrome://bookmarks/content/bm-props.xul",
                                "BookmarkProperties", "chrome,menubar,resizable");
		props.BookmarkURL = select_list[0].getAttribute("id");
	}
  } else {
    dump("nothing selected!\n"); 
  }
}



function OpenSearch(tabName)
{
	window.openDialog("resource:/res/samples/search.xul", "SearchWindow", "dialog=no,close,chrome,resizable", tabName);
}



function OpenURL(event, node, root)
{
    if (node.getAttribute('container') == "true")
    {
        return(false);
    }

    var url = node.getAttribute('id');

    // Ignore "NC:" urls.
    if (url.substring(0, 3) == "NC:")
    {
        return(false);
    }

	try
	{
		var rootNode = document.getElementById(root);
		var ds = null;
		if (rootNode)
		{
			ds = rootNode.database;
		}
		// add support for IE favorites under Win32, and NetPositive URLs under BeOS
		var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
		if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
		if (rdf)
		{
			if (ds)
			{
				var src = rdf.GetResource(url, true);
				var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
				var target = ds.GetTarget(src, prop, true);
				if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
				if (target)	target = target.Value;
				if (target)	url = target;
				
			}
		}
	}
	catch(ex)
	{
	}

    window.open(url,'bookmarks');
    dump("OpenURL(" + url + ")\n");

    return(true);
}



function doSort(sortColName)
{
  var node = document.getElementById(sortColName);
  // determine column resource to sort on
  var sortResource = node.getAttribute('resource');
  if (!node) return(false);

  var sortDirection="ascending";
  var isSortActive = node.getAttribute('sortActive');
  if (isSortActive == "true") {
    var currentDirection = node.getAttribute('sortDirection');
    if (currentDirection == "ascending")
      sortDirection = "descending";
    else if (currentDirection == "descending")
      sortDirection = "natural";
    else
      sortDirection = "ascending";
  }

  // get RDF Core service
  var rdfCore = XPAppCoresManager.Find("RDFCore");
  if (!rdfCore) {
    rdfCore = new RDFCore();
    if (!rdfCore) {
      return(false);
    }
    rdfCore.Init("RDFCore");
  }
  // sort!!!
  rdfCore.doSort(node, sortResource, sortDirection);
  return(false);
}



function fillContextMenu(name)
{
    if (!name)    return(false);
    var popupNode = document.getElementById(name);
    if (!popupNode)    return(false);

    // remove the menu node (which tosses all of its kids);
    // do this in case any old command nodes are hanging around
    var menuNode = popupNode.childNodes[0];
    popupNode.removeChild(menuNode);

    // create a new menu node
    menuNode = document.createElement("menu");
    popupNode.appendChild(menuNode);

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

    var select_list = treeNode.getElementsByAttribute("selected", "true");
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
        
        menuNode.appendChild(menuItem);
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

	var select_list = treeNode.getElementsByAttribute("selected", "true");
	if (select_list.length < 1)    return(false);

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
