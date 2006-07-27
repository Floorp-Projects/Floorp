/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

  Code for the Search Sidebar Panel

 */



var	rootNode = null;
var	textArc = null;
var	RDF_observer = new Object;

RDF_observer =
{
	OnAssert   : function(src, prop, target)
		{
//			debug("RDF_observer: onAssert\n");
			if ((src == rootNode) && (prop == textArc))
			{
				rememberSearchText(target);
			}
		},
	OnUnassert : function(src, prop, target)
		{
//			debug("RDF_observer: onUnassert\n"); 
		},
	OnChange   : function(src, prop, old_target, new_target)
		{
//			debug("RDF_observer: onChange\n");
			if ((src == rootNode) && (prop == textArc))
			{
				rememberSearchText(new_target);
			}
		},
	OnMove     : function(old_src, new_src, prop, target)
		{
//			debug("RDF_observer: onMove\n");
		}
}



function rememberSearchText(targetNode)
{
	if (targetNode)	targetNode = targetNode.QueryInterface(Components.interfaces.nsIRDFLiteral);
	if (targetNode)	targetNode = targetNode.Value;
	if (targetNode && (targetNode != ""))
	{
		var textNode = document.getElementById("sidebar-search-text");
		if (!textNode)	return(false);
		textNode.value = unescape(targetNode);
	}
}



function debug(msg)
{
	// uncomment for noise
	// dump(msg);
}



function Init()
{
	// Initialize the Search panel
	var tree = document.getElementById("Tree");
	if (tree)
	{
		var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
		if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
		if (rdf)
		{
			rootNode = rdf.GetResource("NC:LastSearchRoot", true);
			textArc = rdf.GetResource("http://home.netscape.com/NC-rdf#LastText", true);

			tree.database.AddObserver(RDF_observer);
		}
	}
}



function doSearch()
{
	var textNode = document.getElementById("sidebar-search-text");
	if (!textNode)	return(false);
	var text = textNode.value;
	top.OpenSearch("internet", false, text);
	return(true);
}



function openURL(event, treeitem, root)
{
	if ((event.button != 1) || (event.clickCount != 2))
		return(false);

	if (treeitem.getAttribute("container") == "true")
		return(false);

	if (treeitem.getAttribute("type") == "http://home.netscape.com/NC-rdf#BookmarkSeparator")
		return(false);

	var id = treeitem.getAttribute('id');
	if (!id)
		return(false);

	// rjc: add support for anonymous resources; if the node has
	// a "#URL" property, use it, otherwise default to using the id
	try
	{
		var rootNode = document.getElementById(root);
		var ds = null;
		if (rootNode)
		{
			ds = rootNode.database;
		}
		var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
		if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
		if (rdf)
		{
			if (ds)
			{
				var src = rdf.GetResource(id, true);
				var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
				var target = ds.GetTarget(src, prop, true);
				if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
				if (target)	target = target.Value;
				if (target)	id = target;
			}
		}
	}
	catch(ex)
	{
	}

	window.content.location = id;
}
