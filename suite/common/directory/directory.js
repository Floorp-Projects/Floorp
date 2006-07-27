/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Bradley Baetz <bbaetz@student.usyd.edu.au>
 *   Joe Hewitt <hewitt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
   Script for the directory window
*/

const RDFSERVICE_CONTRACTID     = "@mozilla.org/rdf/rdf-service;1";
const DRAGSERVICE_CONTRACTID    = "@mozilla.org/widget/dragservice;1";
const TRANSFERABLE_CONTRACTID   = "@mozilla.org/widget/transferable;1";
const XULSORTSERVICE_CONTRACTID = "@mozilla.org/xul/xul-sort-service;1";
const ARRAY_CONTRACTID          = "@mozilla.org/supports-array;1";
const WSTRING_CONTRACTID        = "@mozilla.org/supports-string;1";

const NC_NS                 = "http://home.netscape.com/NC-rdf#";
const NC_NAME               = NC_NS + "Name";
const NC_URL                = NC_NS + "URL";
const NC_LOADING            = NC_NS + "loading";

const nsIHTTPIndex          = Components.interfaces.nsIHTTPIndex;
const nsIDragService        = Components.interfaces.nsIDragService;
const nsITransferable       = Components.interfaces.nsITransferable;
const nsIXULSortService     = Components.interfaces.nsIXULSortService;
const nsIRDFService         = Components.interfaces.nsIRDFService;
const nsIRDFLiteral         = Components.interfaces.nsIRDFLiteral;
const nsISupportsArray      = Components.interfaces.nsISupportsArray;
const nsISupportsString    = Components.interfaces.nsISupportsString;

// By the time this runs, The 'HTTPIndex' variable will have been
// magically set on the global object by the native code.

function debug(msg)
{
  // Uncomment to print out debug info.
  //dump(msg);
}

var loadingArc = null;
var loadingLevel = 0;

var	RDF_observer =
{
	onAssert: function(ds, src, prop, target)
	{
		if (prop == loadingArc) {
		  if (loadingLevel++ == 0)
        SetBusyCursor(window, true); 
      debug("Directory: assert: loading level is " + loadingLevel + " for " + src.Value + "\n");
		}
	},
		
	onUnassert: function(ds, src, prop, target)
	{
		if (prop == loadingArc) {
		  if (loadingLevel > 0)
    	  if (--loadingLevel == 0)
          SetBusyCursor(window, false); 
      debug("Directory: unassert: loading level is " + loadingLevel + " for " + src.Value + "\n");
		}
	},

	onChange: function(ds, src, prop, old_target, new_target) { },
	onMove: function(ds, old_src, new_src, prop, target) { },
	onBeginUpdateBatch: function(ds) { },
	onEndUpdateBatch: function(ds) { }
};

function
SetBusyCursor(window, enable)
{
  // Defensive check: setCursor() is only available for
  // chrome windows. Since one of our frame might be a
  // non-chrome window, make sure the window we treat has
  // a setCursor method.
  if("setCursor" in window) {
    if(enable == true) {
      window.setCursor("wait");
      debug("Directory: cursor=busy\n");
    } else {
      window.setCursor("auto");
      debug("Directory: cursor=notbusy\n");
    }
  }

  var numFrames = window.frames.length;
  for (var i = 0; i < numFrames; i++)
    SetBusyCursor(window.frames[i], enable);
}

// We need this hack because we've completely circumvented the onload() logic.
function Boot()
{
  if (document.getElementById('tree'))
    Init();
  else
    setTimeout("Boot()", 500);
}

setTimeout("Boot()", 0);

function Init()
{
  debug("directory.js: Init()\n");

  var tree = document.getElementById('tree');

  // Initialize the tree's base URL to whatever the HTTPIndex is rooted at
  var baseURI = HTTPIndex.BaseURL;

  if (baseURI && (baseURI.indexOf("ftp://") == 0)) {
    // fix bug # 37102: if its a FTP directory
    // ensure it ends with a trailing slash
    if (baseURI.substr(baseURI.length - 1) != "/") {
      debug("append traiing slash to FTP directory URL\n");
      baseURI += "/";
    }

	  // Lets also enable the loggin window.

	  var node = document.getElementById("main-splitter");
	  node.setAttribute("hidden", false);

	  node = document.getElementById("logbox");
	  node.setAttribute("hidden", false);
  }

  if (baseURI && (baseURI.indexOf("file://") != 0)) {
    // Note: DON'T add the HTTPIndex datasource into the tree
    // for file URLs, only do it for FTP/Gopher/etc URLs; the "rdf:files"
    // datasources handles file URLs
    tree.database.AddDataSource(HTTPIndex);
  }

  // Note: set encoding BEFORE setting "ref" (important!)
  var RDF = Components.classes[RDFSERVICE_CONTRACTID].getService();
  if (RDF) RDF = RDF.QueryInterface(nsIRDFService);
  if (RDF) {
    loadingArc = RDF.GetResource(NC_LOADING, true);

    var httpDS = HTTPIndex.DataSource;
    if (httpDS) httpDS = httpDS.QueryInterface(nsIHTTPIndex);
    if (httpDS) {
      httpDS.encoding = "ISO-8859-1";

      // Use a default character set.
      if (window.content.defaultCharacterset)
        httpDS.encoding = window.content.defaultCharacterset;
    }
  }

  // set window title
  document.title = baseURI;

  tree.database.AddObserver(RDF_observer);
  debug("Directory: added observer\n");

  // root the tree (do this last)
  tree.setAttribute("ref", baseURI);
}

function DoUnload()
{
	var tree = document.getElementById("tree");
	if (tree) {
		tree.database.RemoveDataSource(HTTPIndex);
		tree.database.RemoveObserver(RDF_observer);
    debug("Directory: removed observer\n");
	}
}

function OnClick(event)
{
  if (event.target.localName != "treechildren")
    return false;
  if( event.type == "click" && (event.button != 0 || event.detail != 2))
    return false;
  if( event.type == "keypress" && event.keyCode != 13)
    return false;

  var tree = document.getElementById("tree");
  if (tree.currentIndex >= 0) {
    var item = tree.contentView.getItemAtIndex(tree.currentIndex);
    window.content.location.href = item.getAttributeNS(NC_NS, "url");
  }
}

function doSort(aTarget)
{
  if (aTarget.localName != "treecol")
    return;
    
	// determine column resource to sort on
	var sortResource = aTarget.getAttribute('resource');

	// switch between ascending & descending sort (no natural order support)
  var sortDirection = aTarget.getAttribute("sortDirection") == "ascending" ? "descending" : "ascending";

	try {
	  var sortService = Components.classes[XULSORTSERVICE_CONTRACTID].getService(nsIXULSortService);
		sortService.sort(aTarget, sortResource, sortDirection);
	} catch(ex) { }
}

function BeginDragTree (event)
{
  if (event.target.localName != "treechildren")
    return true;
    
  var dragStarted = false;

  try {
    // determine which treeitem was dragged
    var tree = document.getElementById("tree");
    var row = tree.treeBoxObject.getRowAt(event.clientX, event.clientY);
    var item = tree.contentView.getItemAtIndex(row);

    // get information from treeitem for drag
    var url = item.getAttributeNS(NC_NS, "url");
    var desc = item.getAttributeNS(NC_NS, "desc");
    
    var RDF = Components.classes[RDFSERVICE_CONTRACTID].getService(nsIRDFService);
    var transferable = 
      Components.classes[TRANSFERABLE_CONTRACTID].createInstance(nsITransferable);
    var genDataURL = 
      Components.classes[WSTRING_CONTRACTID].createInstance(nsISupportsString);
    var genDataHTML = 
      Components.classes[WSTRING_CONTRACTID].createInstance(nsISupportsString);
    var genData = 
      Components.classes[WSTRING_CONTRACTID].createInstance(nsISupportsString);
    var genDataURL = 
      Components.classes[WSTRING_CONTRACTID].createInstance(nsISupportsString);

    transferable.addDataFlavor("text/x-moz-url");
    transferable.addDataFlavor("text/html");
    transferable.addDataFlavor("text/unicode");
    
    genDataURL.data = url + "\n" + desc;
    genDataHTML.data = "<a href=\"" + url + "\">" + desc + "</a>";
    genData.data = url;

    transferable.setTransferData("text/x-moz-url", genDataURL, genDataURL.data.length * 2);
    transferable.setTransferData("text/html", genDataHTML, genDataHTML.data.length * 2);
    transferable.setTransferData("text/unicode", genData, genData.data.length * 2);

    var transArray = 
      Components.classes[ARRAY_CONTRACTID].createInstance(nsISupportsArray);

    // put it into the transferable as an |nsISupports|
    var genTrans = transferable.QueryInterface(Components.interfaces.nsISupports);
    transArray.AppendElement(genTrans);
    
    var dragService = 
      Components.classes[DRAGSERVICE_CONTRACTID].getService(nsIDragService);

    dragService.invokeDragSession(event.target, transArray, null, nsIDragService.DRAGDROP_ACTION_COPY + 
                                  nsIDragService.DRAGDROP_ACTION_MOVE);
  
    dragStarted = true;
  } catch (ex) { }
  
  return !dragStarted;
}

function scrollDown()
{
  window.frames[0].scrollTo(0, window.frames[0].document.height);
}

function OnFTPControlLog( server, msg )
{
	var logdoc = frames[0].document;
	var logdocDiv = logdoc.getElementById("logboxDiv");
	var div = document.createElementNS("http://www.w3.org/1999/xhtml", 
                                       "html:div");

	if (server)
		div.setAttribute("class", "server");
	else
		div.setAttribute("class", "client");

	div.appendChild (document.createTextNode(msg));
	
	logdocDiv.appendChild(div);

	scrollDown();
}

