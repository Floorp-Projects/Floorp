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



const RDFSERVICE_PROGID     = "component://netscape/rdf/rdf-service";
const DRAGSERVICE_PROGID    = "component://netscape/widget/dragservice";
const TRANSFERABLE_PROGID   = "component://netscape/widget/transferable";
const XULSORTSERVICE_PROGID = "component://netscape/rdf/xul-sort-service";
const ARRAY_PROGID          = "component://netscape/supports-array";
const WSTRING_PROGID        = "component://netscape/supports-wstring";

const NC_NS                 = "http://home.netscape.com/NC-rdf#";
const NC_NAME               = NC_NS + "Name";
const NC_LOADING            = NC_NS + "loading";

const nsIHTTPIndex          = Components.interfaces.nsIHTTPIndex;
const nsIDragService        = Components.interfaces.nsIDragService;
const nsITransferable       = Components.interfaces.nsITransferable;
const nsIXULSortService     = Components.interfaces.nsIXULSortService;
const nsIRDFService         = Components.interfaces.nsIRDFService;
const nsIRDFLiteral         = Components.interfaces.nsIRDFLiteral;
const nsISupportsArray      = Components.interfaces.nsISupportsArray;
const nsISupportsWString    = Components.interfaces.nsISupportsWString;



// By the time this runs, The 'HTTPIndex' variable will have been
// magically set on the global object by the native code.



function debug(msg)
{
    // Uncomment to print out debug info.
    // dump(msg);
}



// get handle to the BrowserAppCore in the content area.
var appCore = window._content.appCore;
var	loadingArc = null;
var loadingLevel = 0;

var	RDF_observer = new Object;

RDF_observer =
{
	onAssert   : function(ds, src, prop, target)
		{
		    if (prop == loadingArc)
		    {
		        if (loadingLevel++ == 0)
		        {
                    SetBusyCursor(window, true); 
		        }
                debug("Directory: assert: loading level is " + loadingLevel + " for " + src.Value + "\n");
		    }
		},
	onUnassert : function(ds, src, prop, target)
		{
		    if (prop == loadingArc)
		    {
		        if (loadingLevel > 0)
		        {
    		        if (--loadingLevel == 0)
    		        {
                        SetBusyCursor(window, false); 
    		        }
		        }
                debug("Directory: unassert: loading level is " + loadingLevel + " for " + src.Value + "\n");
		    }
		},
	onChange   : function(ds, src, prop, old_target, new_target)
		{
		},
	onMove     : function(ds, old_src, new_src, prop, target)
		{
		},
	beginUpdateBatch : function(ds)
		{
		},
	endUpdateBatch   : function(ds)
		{
		}
}



function
SetBusyCursor(window, enable)
{
    if(enable == true)
    {
        window.setCursor("wait");
        debug("Directory: cursor=busy\n");
    }
    else
    {
        window.setCursor("auto");
        debug("Directory: cursor=notbusy\n");
    }

    var numFrames = window.frames.length;
    for(var i = 0; i < numFrames; i++)
    {
        SetBusyCursor(window.frames[i], enable);
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
    var RDF = Components.classes[RDFSERVICE_PROGID].getService();
    if (RDF)    RDF = RDF.QueryInterface(nsIRDFService);
    if (RDF)
    {
        loadingArc = RDF.GetResource(NC_LOADING, true);

        var httpDS = HTTPIndex.DataSource;
        if (httpDS) httpDS = httpDS.QueryInterface(nsIHTTPIndex);
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

    tree.database.AddObserver(RDF_observer);
    debug("Directory: added observer\n");

    // root the tree (do this last)
    tree.setAttribute("ref", baseURI);
}



function DoUnload()
{
	var tree = document.getElementById("tree");
	if (tree)
	{
		tree.database.RemoveObserver(RDF_observer);
	    debug("Directory: removed observer\n");
	}
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
		var isupports = Components.classes[XULSORTSERVICE_PROGID].getService();
		if (!isupports)    return(false);
		var xulSortService = isupports.QueryInterface(nsIXULSortService);
		if (!xulSortService)    return(false);
		xulSortService.Sort(node, sortResource, sortDirection);
	}
	catch(ex)
	{
	}
	return(false);
}



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
    Components.classes[RDFSERVICE_PROGID].getService(nsIRDFService);
  if (!RDF) return(false);

  var dragStarted = false;

  var trans = 
    Components.classes[TRANSFERABLE_PROGID].createInstance(nsITransferable);
  if ( !trans ) return(false);

  var genData = 
    Components.classes[WSTRING_PROGID].createInstance(nsISupportsWString);
  if (!genData) return(false);

  var genDataURL = 
    Components.classes[WSTRING_PROGID].createInstance(nsISupportsWString);
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
  var prop = RDF.GetResource(NC_NAME, true);
  var target = database.GetTarget(src, prop, true);
  if (target) target = target.QueryInterface(nsIRDFLiteral);
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
    Components.classes[ARRAY_PROGID].createInstance(nsISupportsArray);
  if ( !transArray )  return(false);

  // put it into the transferable as an |nsISupports|
  var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
  transArray.AppendElement(genTrans);
  
  var dragService = 
    Components.classes[DRAGSERVICE_PROGID].getService(nsIDragService);
  if ( !dragService ) return(false);

  dragService.invokeDragSession ( event.target, transArray, null, nsIDragService.DRAGDROP_ACTION_COPY + 
                                     nsIDragService.DRAGDROP_ACTION_MOVE );
  dragStarted = true;

  return(!dragStarted);
}
