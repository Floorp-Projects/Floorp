/* -*- Mode: Java; tab-width: 4; insert-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and
 * limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

// the rdf service
var rdf_uri = 'component://netscape/rdf/rdf-service'
var RDF = Components.classes[rdf_uri].getService()
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService)

// the default sidebar:
var sidebar = new Object;

function debug(msg)
{
  // uncomment for noise
  //dump(msg);
}

function sidebarOverlayInit()
{
  // Look in the profile directory to find 'panels.rdf', which is the
  // database of the user's currently selected panels.
  var profileInterface = Components.interfaces.nsIProfile;
  var profileURI = 'component://netscape/profile/manager';
  var profileService  = Components.classes[profileURI].getService();
  profileService = profileService.QueryInterface(profileInterface);
  var sidebar_url = profileService.getCurrentProfileDirFromJS();
  sidebar_url.URLString += "panels.rdf";

  if (sidebar_url.exists()) {
    debug("sidebar url is " + sidebar_url.URLString + "\n");
    sidebar.db = sidebar_url.URLString;
  }
  else {
    // XXX What we should _really_ do here is copy the default panels
    // into the profile directory and then try again.


    sidebar.db = 'chrome://sidebar/content/default-panels.rdf'
    debug("using " + sidebar.db + " because " + sidebar_url.URLString + " does not exist\n");
  }

  sidebar.resource = 'urn:sidebar:current-panel-list';

  // Initialize the display
  var sidebar_element = document.getElementById('sidebar-box')
  var sidebar_menuitem = document.getElementById('menu_sidebar')
  if (sidebar_element.getAttribute('hidden') == 'true') {
    sidebar_element.setAttribute('style', 'display:none')
    sidebar_menuitem.setAttribute('checked', 'false')
    return
  }

  debug("sidebar = " + sidebar + "\n");
  debug("sidebar.resource = " + sidebar.resource + "\n");
  debug("sidebar.db = " + sidebar.db + "\n");

  // Add the user's current panel choices to the template builder,
  // which will aggregate it with the other datasources that describe
  // the individual panel's title, customize URL, and content URL.
  var panels = document.getElementById('sidebar-panels');
  panels.database.AddDataSource(RDF.GetDataSource(sidebar.db));

  // XXX This is a hack to force re-display
  panels.setAttribute('ref', 'urn:sidebar:current-panel-list');

  sidebarOpenDefaultPanel(1, 0);
}

function sidebarOpenDefaultPanel(wait, tries) {
  var parent = document.getElementById('sidebar-panels');
  var target = parent.getAttribute('open-panel-src');
  var children = parent.childNodes;

  debug("~~~~~~~~~~~opening default panel\n");
  if (children.length < 3) {
  debug("~~~~~~~~~~~not enough kids yet\n");
	if (tries < 5) {
      // No children yet, try again later
      setTimeout('sidebarOpenDefaultPanel('+(wait*2+1)+','+(tries+1)+')',wait);
	}
    return;
  }
    for (var ii=0; ii < children.length; ii++) {
      debug("~~ child " + ii + "\n");
	  dumpAttributes(children.item(ii));
	}
  if (target && target != '') {
    for (var ii=0; ii < children.length; ii++) {
	  if (children.item(ii).getAttribute('src') == target) {
		children.item(ii).removeAttribute('collapsed');
		return;
	  }
	}
  }
  // Pick the first one
  debug("~~~~~~~~~~~picking first one\n");
  var first_iframe = children.item(2);
  if (first_iframe) {
    first_iframe.removeAttribute('collapsed');
    parent.setAttribute('open-panel-src',first_iframe.getAttribute('src'));
  }
}

function sidebarOpenClosePanel(titledbutton) {
  var target = titledbutton.getAttribute('iframe-src');
  var last_src = titledbutton.parentNode.getAttribute('open-panel-src');
  var children = titledbutton.parentNode.childNodes;

  if (target == last_src) {
	return;
  }

  for (var ii=0; ii < children.length; ii++) {
    var src = children.item(ii).getAttribute('src')

	if (src == target) {
	  children.item(ii).removeAttribute('collapsed');
	  titledbutton.parentNode.setAttribute('open-panel-src',target);
	}
	if (src == last_src) {
	  children.item(ii).setAttribute('collapsed','true');
	}
  }
}

function sidebarReload() {
  sidebarOverlayInit(sidebar) 
}

function sidebarCustomize() {
  var newWin = window.openDialog('chrome://sidebar/content/customize.xul',
                                 '_blank','chrome,modal',
                                 sidebar.db, sidebar.resource)
  return newWin
}

function sidebarShowHide() {
  var sidebar = document.getElementById('sidebar-box')
  var sidebar_splitter = document.getElementById('sidebar-splitter')
  var is_hidden = sidebar.getAttribute('hidden')

  if (is_hidden && is_hidden == "true") {
    debug("Showing the sidebar\n")
    sidebar.setAttribute('hidden','')
    sidebar_splitter.setAttribute('hidden','')
	  //sidebarOverlayInit()
  } else {
    debug("Hiding the sidebar\n")
    sidebar.setAttribute('hidden','true')
    sidebar_splitter.setAttribute('hidden','true')
  }
}

function dumpAttributes(node) {
  var attributes = node.attributes

  if (!attributes || attributes.length == 0) {
    debug("no attributes\n")
  }
  for (var ii=0; ii < attributes.length; ii++) {
    var attr = attributes.item(ii)
    debug("attr "+ii+": "+ attr.name +"="+attr.value+"\n")
  }
}

function dumpStats() {
  var box = document.getElementById('sidebar-box');
  var splitter = document.getElementById('sidebar-splitter');
  var style = box.getAttribute('style')

  var visibility = style.match('visibility:([^;]*)')
  if (visibility) {
    visibility = visibility[1]
  }
  debug("sidebar-box.style="+style+"\n")
  debug("sidebar-box.visibility="+visibility+"\n")
  debug('sidebar-box.width='+box.getAttribute('width')+'\n')
  debug('sidebar-box attrs\n---------------------\n')
  dumpAttributes(box)
  debug('sidebar-splitter attrs\n--------------------------\n')
  dumpAttributes(splitter)
}

function dumpTree(node, depth) {
  var indent = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | | + "
  var kids = node.childNodes
  debug(indent.substr(indent.length - depth*2))

  // Print your favorite attributes here
  debug(node.nodeName)
  debug(" "+node.getAttribute('id'))
  debug("\n")

  for (var ii=0; ii < kids.length; ii++) {
    dumpTree(kids[ii], depth + 1)
  }
}

// To get around "window.onload" not working in viewer.
function sidebarOverlayBoot()
{
    var panels = document.getElementById('sidebar-panels');
    if (panels == null) {
        setTimeout(sidebarOverlayBoot, 1);
    }
    else {
        sidebarOverlayInit();
    }
}

setTimeout('sidebarOverlayBoot()', 0);
