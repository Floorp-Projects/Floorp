/* -*- Mode: Java; tab-width: 2; c-basic-offset: 2 -*-
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

// the rdf service
var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

var sidebar = new Object;

function Init(sidebar_db, sidebar_resource)
{
  // Initialize the Sidebar
	sidebar.db       = sidebar_db;
  sidebar.resource = sidebar_resource;

  var registry;
  try {
    // First try to construct a new one and load it
    // synchronously. nsIRDFService::GetDataSource() loads RDF/XML
    // asynchronously by default.
    registry = Components.classes['component://netscape/rdf/datasource?name=xml-datasource'].createInstance();
    registry = registry.QueryInterface(Components.interfaces.nsIRDFDataSource);

    var remote = registry.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
    remote.Init(sidebar.db); // this will throw if it's already been opened and registered.

    // read it in synchronously.
    remote.Refresh(true);
  }
  catch (ex) {
    // if we get here, then the RDF/XML has been opened and read
    // once. We just need to grab the datasource.
    registry = RDF.GetDataSource(sidebar.db);
  }

  // Create a 'container' wrapper around the sidebar.resources
  // resource so we can use some utility routines that make access a
  // bit easier.
  var sb_datasource = Components.classes['component://netscape/rdf/container'].createInstance();
  sb_datasource = sb_datasource.QueryInterface(Components.interfaces.nsIRDFContainer);

  sb_datasource.Init(registry, RDF.GetResource(sidebar.resource));
  
  var sidebox = document.getElementById('sidebox');

  // Now enumerate all of the flash datasources.
  var enumerator = sb_datasource.GetElements();

  while (enumerator.HasMoreElements()) {
    var service = enumerator.GetNext();
    service = service.QueryInterface(Components.interfaces.nsIRDFResource);

    var new_panel = createPanel(registry, service);
    if (new_panel) {
      sidebox.appendChild(new_panel);
    }
  }
}

function createPanel(registry, service) {
  var panel_title     = getAttr(registry, service, 'title');
  var panel_content   = getAttr(registry, service, 'content');
  var panel_height    = getAttr(registry, service, 'height');

  var box      = document.createElement('box');
  var iframe   = document.createElement('html:iframe');

  var iframeId = iframe.getAttribute('id');
  var panelbar = createPanelTitle(panel_title, iframeId);

  box.setAttribute('align', 'vertical');
  iframe.setAttribute('src', panel_content);
  if (panel_height) {
    var height_style = 'height:' + panel_height + ';';
	  iframe.setAttribute('style',       height_style);
		iframe.setAttribute('save_height', height_style);
	}
  box.appendChild(panelbar);
  box.appendChild(iframe);

  return box;
}

function createPanelTitle(titletext, id)
{
  var panelbar  = document.createElement('box');
  var title     = document.createElement('titledbutton');
  var spring    = document.createElement('spring');
  var corner   = document.createElement('html:img');

  title.setAttribute('value', titletext);
  title.setAttribute('class', 'borderless paneltitle');
  title.setAttribute('onclick', 'resize("'+id+'")');
  spring.setAttribute('flex', '100%');
  panelbar.setAttribute('class', 'panelbar');
  corner.setAttribute('src', 'chrome://sidebar/skin/corner.gif');

  panelbar.appendChild(corner);
  panelbar.appendChild(title);
  panelbar.appendChild(spring);

  return panelbar;
}

function getAttr(registry,service,attr_name) {
  var attr = registry.GetTarget(service,
           RDF.GetResource('http://home.netscape.com/NC-rdf#' + attr_name),
           true);
  if (attr)
    attr = attr.QueryInterface(
          Components.interfaces.nsIRDFLiteral);
  if (attr)
      attr = attr.Value;
  return attr;
}

function resize(id) {
  var box = document.getElementById('sidebox');
  var iframe = document.getElementById(id);

  if (iframe.getAttribute('display') == 'none') {
		var height_style = iframe.getAttribute('save_height');
    iframe.setAttribute('style', height_style + 'visibility:visible');
    iframe.setAttribute('display','block');
  } else {
    iframe.setAttribute('style', 'height:0px; visibility:hidden');
    iframe.setAttribute('display','none');
  }
}

function customize() {
	var newWin = window.openDialog('chrome://sidebar/content/customize.xul','New','chrome', sidebar.db, sidebar.resource);
	return newWin;
}

function dumpTree(node, depth) {
  var indent = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | | + ";
  var kids = node.childNodes;
  dump(indent.substr(indent.length - depth*2));

  // Print your favorite attributes here
  dump(node.nodeName)
  dump(" "+node.getAttribute('id'));
  dump("\n");

  for (var ii=0; ii < kids.length; ii++) {
    dumpTree(kids[ii], depth + 1);
  }
}

