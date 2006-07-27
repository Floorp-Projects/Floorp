/* -*- Mode: Java; tab-width: 4; c-basic-offset: 4; -*-
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
   Script for the directory window
*/



// By the time this runs, The 'HTTPIndex' variable will have been
// magically set on the global object by the native code.



function debug(msg)
{
    // Uncomment to print out debug info.
    dump(msg);
}



// get handle to the BrowserAppCore in the content area.
var appCore = window._content.appCore;



function Init()
{
    debug("directory.js: Init()\n");

    var tree = document.getElementById('tree');

    // Initialize the tree's base URL to whatever the HTTPIndex is rooted at
    var baseURI = HTTPIndex.BaseURL;

    if (baseURI && (baseURI.indexOf("ftp://") == 0))
    {
        // fix bug # 37102: if its a FTP directory
        // ensure it ends with a trailing slash
    	if (baseURI.substr(baseURI.length - 1) != "/")
    	{
        	debug("append traiing slash to FTP directory URL\n");
        	baseURI += "/";
        }

        // Note: DON'T add the HTTPIndex datasource into the tree
        // for file URLs, only do it for FTP URLs; the "rdf:files"
        // datasources handles file URLs
        tree.database.AddDataSource(HTTPIndex.DataSource);
    }

    // Note: set encoding BEFORE setting "ref" (important!)
    var RDF = Components.classes["component://netscape/rdf/rdf-service"].getService();
    if (RDF)    RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
    if (RDF)
    {
        var httpDS = HTTPIndex.DataSource;
        if (httpDS) httpDS = httpDS.QueryInterface(Components.interfaces.nsIHTTPIndex);
        if (httpDS)
        {
            httpDS.encoding = "ISO-8859-1";

            // Use a default character set.
            if (window._content.defaultCharacterset)
            {
                httpDS.encoding = window._content.defaultCharacterset;
            }
        }
    }

    // set window title
    var theWindow = window._content.parentWindow;
    if (theWindow)
    {
        theWindow.title = baseURI;
    }

    // root the tree (do this last)
    tree.setAttribute("ref", baseURI);
}



function OnClick(event, node)
{
    if( event.type == "click" &&
        ( event.button != 1 || event.detail != 2 || node.nodeName != "treeitem") )
      return(false);
    if( event.type == "keypress" && event.keyCode != 13 )
      return(false);

    var tree = document.getElementById("tree");
    if( tree.selectedItems.length == 1 ) 
      {
        var selectedItem = tree.selectedItems[0];
        var theID = selectedItem.getAttribute("id");

        //if( selectedItem.getAttribute( "type" ) == "FILE" )
		if(appCore)
		{
		    // support session history (if appCore is available)
            appCore.loadUrl(theID);
		}
		else
		{
		    // fallback case (if appCore isn't available)
            window._content.location.href = theID;
		}

        // set window title
        var theWindow = window._content.parentWindow;
        if (theWindow)
        {
            theWindow.title = theID;
        }
      }
}


// We need this hack because we've completely circumvented the onload() logic.
function Boot()
{
    if (document.getElementById('tree')) {
        Init();
    }
    else {
        setTimeout("Boot()", 500);
    }
}

setTimeout("Boot()", 0);


function doSort(sortColName)
{
	var node = document.getElementById(sortColName);
	if (!node) return(false);

	// determine column resource to sort on
	var sortResource = node.getAttribute('resource');

	// switch between ascending & descending sort (no natural order support)
	var sortDirection="ascending";
	var isSortActive = node.getAttribute('sortActive');
	if (isSortActive == "true")
	{
		var currentDirection = node.getAttribute('sortDirection');
		if (currentDirection == "ascending")
		{
			sortDirection = "descending";
		}
	}

	try
	{
		var isupports = Components.classes["component://netscape/rdf/xul-sort-service"].getService();
		if (!isupports)    return(false);
		var xulSortService = isupports.QueryInterface(Components.interfaces.nsIXULSortService);
		if (!xulSortService)    return(false);
		xulSortService.Sort(node, sortResource, sortDirection);
	}
	catch(ex)
	{
	}
	return(false);
}



var NC_NS  = "http://home.netscape.com/NC-rdf#";



function BeginDragTree ( event )
{
  var tree = document.getElementById("tree");
  if ( event.target == tree )
    return(true);         // continue propagating the event

  // only <treeitem>s can be dragged out
  if ( event.target.parentNode.parentNode.tagName != "treeitem")
    return(false);

  var database = tree.database;
  if (!database)    return(false);

  var RDF = 
    Components.classes["component://netscape/rdf/rdf-service"].getService(Components.interfaces.nsIRDFService);
  if (!RDF) return(false);

  var dragStarted = false;

  var trans = 
    Components.classes["component://netscape/widget/transferable"].createInstance(Components.interfaces.nsITransferable);
  if ( !trans ) return(false);

  var genData = 
    Components.classes["component://netscape/supports-wstring"].createInstance(Components.interfaces.nsISupportsWString);
  if (!genData) return(false);

  var genDataURL = 
    Components.classes["component://netscape/supports-wstring"].createInstance(Components.interfaces.nsISupportsWString);
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

  // if we can get node's name, append (space) name to url
  var src = RDF.GetResource(id, true);
  var prop = RDF.GetResource(NC_NS + "Name", true);
  var target = database.GetTarget(src, prop, true);
  if (target) target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
  if (target) target = target.Value;
  if (target && (target != ""))
  {
      id = id + " " + target;
  }

  var trueID = id;
  if (parentID != null)
  {
  	trueID += "\n" + parentID;
  }
  genData.data = trueID;
  genDataURL.data = id;

  trans.setTransferData ( "moz/rdfitem", genData, genData.data.length * 2);  // double byte data
  trans.setTransferData ( "text/unicode", genDataURL, genDataURL.data.length * 2);  // double byte data

  var transArray = 
    Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
  if ( !transArray )  return(false);

  // put it into the transferable as an |nsISupports|
  var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
  transArray.AppendElement(genTrans);
  
  var dragService = 
    Components.classes["component://netscape/widget/dragservice"].getService(Components.interfaces.nsIDragService);
  if ( !dragService ) return(false);

  var nsIDragService = Components.interfaces.nsIDragService;
  dragService.invokeDragSession ( event.target, transArray, null, nsIDragService.DRAGDROP_ACTION_COPY + 
                                     nsIDragService.DRAGDROP_ACTION_MOVE );
  dragStarted = true;

  return(!dragStarted);
}
