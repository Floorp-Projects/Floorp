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

var NC = "http://home.netscape.com/NC-rdf#";

// the magic number to find panels.rdf
var PANELS_RDF_FILE = 66626;

// the default sidebar:
var sidebar = new Object;

function debug(msg) {
  // uncomment for noise
  //dump(msg+"\n");
}


var panel_observer = new Object;

panel_observer = {
  OnAssert   : function(src,prop,target)
               { debug("panel_observer: onAssert"); },
  OnUnassert : function(src,prop,target)
    {
      debug("panel_observer: onUnassert"); 

      // Wait for unassert that marks the end of the customize changes.
      // See customize.js for where this is unasserted.
      if (prop == RDF.GetResource(NC + "inbatch")) {
        sidebarOpenDefaultPanel(100, 0);
      }
    },
  OnChange   : function(src,prop,old_target,new_target)
               { debug("panel_observer: onChange"); },
  OnMove     : function(old_src,new_src,prop,target)
               { debug("panel_observer: onMove"); }
}


function getSidebarDatasourceURI(panels_file_id) {
  try {
    var fileLocatorInterface = Components.interfaces.nsIFileLocator;
    var fileLocatorProgID = 'component://netscape/filelocator';
    var fileLocatorService  = Components.classes[fileLocatorProgID].getService();
    // use the fileLocator to look in the profile directory
    // to find 'panels.rdf', which is the
    // database of the user's currently selected panels.
    fileLocatorService = fileLocatorService.QueryInterface(fileLocatorInterface);

    // if <profile>/panels.rdf doesn't exist, GetFileLocation() will copy
    // bin/defaults/profile/panels.rdf to <profile>/panels.rdf
    var sidebar_file = fileLocatorService.GetFileLocation(panels_file_id);

    if (!sidebar_file.exists()) {
      // this should not happen, as GetFileLocation() should copy
      // defaults/panels.rdf to the users profile directory
      return null;
    }

    debug("sidebar uri is " + sidebar_file.URLString);
    return sidebar_file.URLString;
  }
  catch (ex) {
    // this should not happen
    return null;
  }
}

function sidebarOverlayInit() {
  sidebar.datasource_uri     = getSidebarDatasourceURI(PANELS_RDF_FILE);
  sidebar.resource           = 'urn:sidebar:current-panel-list';
  sidebar.master_datasources = 'chrome://sidebar/content/local-panels.rdf chrome://sidebar/content/remote-panels.rdf';
  sidebar.master_resource    = 'urn:sidebar:master-panel-list';

  // Initialize the display
  var sidebar_element  = document.getElementById('sidebar-box')
  var sidebar_menuitem = document.getElementById('menu_sidebar')
  if (sidebar_element.getAttribute('hidden') == 'true') {
    if (sidebar_menuitem) {
      sidebar_menuitem.setAttribute('checked', 'false')
    }
  } else {
    if (sidebar_menuitem) {
      sidebar_menuitem.setAttribute('checked', 'true');
    }

    debug("sidebar = "                + sidebar);
    debug("sidebar.resource = "       + sidebar.resource);
    debug("sidebar.datasource_uri = " + sidebar.datasource_uri);

    // Add the user's current panel choices to the template builder,
    // which will aggregate it with the other datasources that describe
    // the individual panel's title, customize URL, and content URL.
    var panels = document.getElementById('sidebar-panels');
    panels.database.AddDataSource(RDF.GetDataSource(sidebar.datasource_uri));

    // The stuff on the bottom
    var panels_bottom = document.getElementById('sidebar-panels-bottom');
    panels_bottom.database.AddDataSource(RDF.GetDataSource(sidebar.datasource_uri));

    debug("Adding observer to database.");
    panels.database.AddObserver(panel_observer);

    // XXX This is a hack to force re-display
    panels.setAttribute('ref', sidebar.resource);

    // XXX This is a hack to force re-display
    panels_bottom.setAttribute('ref', sidebar.resource);

    sidebarOpenDefaultPanel(100, 0);
  }
}

function sidebarOpenDefaultPanel(wait, tries) {
  var panels_top  = document.getElementById('sidebar-panels');
  var target      = panels_top.getAttribute('open-panel-src');
  var top_headers = panels_top.childNodes;

  debug("\nsidebarOpenDefaultPanel("+wait+","+tries+")\n");

  if (top_headers.length <= 1) {
    if (tries < 5) {
      // No children yet, try again later
      setTimeout('sidebarOpenDefaultPanel('+(wait*2)+','+(tries+1)+')',wait);
    } else {
      // No panels.
      // XXX This should load some help page instead of about:blank.
      var iframe = document.getElementById('sidebar-content');
      iframe.setAttribute('src', 'about:blank');
    }
    return;
  }

  selectPanel(target);
}

function selectPanel(target) {
  var panels_top   = document.getElementById('sidebar-panels');
  var iframe       = document.getElementById('sidebar-content');
  var top_headers  = panels_top.childNodes;

  debug("selectPanel("+target+")");

  var select_index = findPanel(top_headers, target);

  if (!select_index) {
    // Target not found. Pick the first panel by default.
    // It is at index 1 because the template is at index 0.
    select_index = 1;
    target = top_headers.item(1).getAttribute('iframe-src');
  }

  // Update the content area if necessary.
  if (iframe.getAttribute('src') != target) {
    iframe.setAttribute('src', target);
  }
  if (panels_top.getAttribute('open-panel-src') != target) {
    panels_top.setAttribute('open-panel-src', target);
  }

  updateHeaders(select_index);
}

function findPanel(panels, target) {
  if (target && target != '') {
    // Find the index of the selected panel
    for (var ii=1; ii < panels.length; ii++) {
      var item = panels.item(ii);
      
      if (item.getAttribute('iframe-src') == target) {

        // Found it!
        return ii;
      }
    }
  }
  return 0;
}

function updateHeaders(index) {
  var top_headers    = document.getElementById('sidebar-panels').childNodes;
  var bottom_headers = document.getElementById('sidebar-panels-bottom').childNodes;

  for (var ii=1; ii < top_headers.length; ii++) {
    var top_item    = top_headers.item(ii);
    var bottom_item = bottom_headers.item(ii);

    if (ii < index) { 
      top_item.removeAttribute('selected');
      top_item.removeAttribute('style');
      bottom_item.setAttribute('style','display:none');
    } else if (ii == index) {
      top_item.setAttribute('selected','true');
      top_item.removeAttribute('style');
      bottom_item.setAttribute('style','display:none');
    } else {
      top_item.setAttribute('style','display:none');
      bottom_item.removeAttribute('style');
      bottom_item.removeAttribute('selected');
    }
  }
}


// Change the sidebar content to the selected panel.
// Called when a panel title is clicked.
function SidebarSelectPanel(titledbutton) {
  var target = titledbutton.getAttribute('iframe-src');
  var last_src = titledbutton.parentNode.getAttribute('open-panel-src');

  if (target == last_src) {
    // XXX Maybe this should reload the content?
    return;
  }

  selectPanel(target);
}

// No one is calling this right now.
function sidebarReload() {
  sidebarOverlayInit();
}

// Set up a lame hack to avoid opening two customize
// windows on a double click.
var gDisableCustomize = false;
function enableCustomize() {
  gDisableCustomize = false;
}

// Bring up the Sidebar customize dialog.
function SidebarCustomize() {
  // Use a single sidebar customize dialog
  var cwindowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
  var iwindowManager = Components.interfaces.nsIWindowMediator;
  var windowManager  = cwindowManager.QueryInterface(iwindowManager);

  var customizeWindow = windowManager.getMostRecentWindow('sidebar:customize');

  if (customizeWindow) {
    debug("Reuse existing customize dialog");
    customizeWindow.focus();
  } else {
    debug("Open a new customize dialog");

    if (false == gDisableCustomize) {
      gDisableCustomize = true;

      var panels = document.getElementById('sidebar-panels');
      
      customizeWindow = window.openDialog(
                          'chrome://sidebar/content/customize.xul',
                          '_blank','chrome,resizable',
                          sidebar.master_datasources,
                          sidebar.master_resource,
                          sidebar.datasource_uri,
                          sidebar.resource);
      setTimeout(enableCustomize, 2000);
    }
  }
}

// Show/Hide the entire sidebar.
// Envoked by the "View / Sidebar" menu option.
function SidebarShowHide() {
  var sidebar          = document.getElementById('sidebar-box');
  var sidebar_splitter = document.getElementById('sidebar-splitter');
  var is_hidden        = sidebar.getAttribute('hidden');

  if (is_hidden && is_hidden == "true") {
    debug("Showing the sidebar");
    sidebar.removeAttribute('hidden');
    sidebar_splitter.removeAttribute('hidden');
    sidebarOverlayInit()
  } else {
    debug("Hiding the sidebar");
    sidebar.setAttribute('hidden','true');
    sidebar_splitter.setAttribute('hidden','true');
  }
  // Immediately save persistent values
  document.persist('sidebar-box', 'hidden');
  persistWidth();
}

// SidebarExpandCollapse() - Respond to grippy click.
// XXX This whole function is a hack to work around
// bugs #20546, and #22214.
function SidebarExpandCollapse() {
  var sidebar          = document.getElementById('sidebar-box');
  var sidebar_splitter = document.getElementById('sidebar-splitter');

  sidebar.setAttribute('hackforbug20546-applied','true');

  // Get the current open/collapsed state
  // The value of state is the "preclick" state
  var state = sidebar_splitter.getAttribute('state');

  if (state && state == 'collapsed') {
    // Going from collapsed to open.

    sidebar.removeAttribute('hackforbug20546');

    // Reset the iframe's src to get the content to display.
    // This might be bug #22214.
    var panels = document.getElementById('sidebar-panels');
    var target = panels.getAttribute('open-panel-src');
    var iframe = document.getElementById('sidebar-content');
    iframe.setAttribute('src', target);

    // XXX Partial hack workaround for bug #22214.
    bumpWidth(+1);
    setTimeout("bumpWidth(-1)",10);

  } else {
    // Going from open to collapsed.

    sidebar.setAttribute('hackforbug20546','true');
  }
}

// XXX Partial hack workaround for bug #22214.
function bumpWidth(delta) {
  var sidebar = document.getElementById('sidebar-box');
  var width = sidebar.getAttribute('width');
  sidebar.setAttribute('width', parseInt(width) + delta);
}

function persistWidth() {
  // XXX Partial workaround for bug #19488.
  var sidebar = document.getElementById('sidebar-box');
  var sidebar_splitter = document.getElementById('sidebar-splitter');
  var state = sidebar_splitter.getAttribute('state');

  var width = sidebar.getAttribute('width');

  if (!state || state == '' || state == 'open') {
    sidebar.removeAttribute('hackforbug20546');
    sidebar.setAttribute('hackforbug20546-applied','true');
  }

  if (width && (width > 410 || width < 15)) {
    sidebar.setAttribute('width',168);
  }

  width = sidebar.getAttribute('width');
  document.persist('sidebar-box', 'width');
}

function SidebarFinishDrag() {

  // XXX Semi-hack for bug #16516.
  // If we had the proper drag event listener, we would not need this
  // timeout. The timeout makes sure the width is written to disk after
  // the sidebar-box gets the newly dragged width.
  setTimeout("persistWidth()",100);
}

/*==================================================
// Handy debug routines
//==================================================
function dumpAttributes(node,depth) {
  var attributes = node.attributes;
  var indent = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | | . ";

  if (!attributes || attributes.length == 0) {
    debug(indent.substr(indent.length - depth*2) + "no attributes");
  }
  for (var ii=0; ii < attributes.length; ii++) {
    var attr = attributes.item(ii);
    debug(indent.substr(indent.length - depth*2) + attr.name +"="+attr.value);
  }
}

function dumpTree(node, depth) {
  if (!node) {
    debug("dumpTree: node is null");
  }
  var indent = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | | + ";
  debug(indent.substr(indent.length - depth*2) + node.nodeName);
  if (node.nodeName != "#text") {
    //debug(" id="+node.getAttribute('id'));
    dumpAttributes(node, depth);
  }
  var kids = node.childNodes;
  for (var ii=0; ii < kids.length; ii++) {
    dumpTree(kids[ii], depth + 1);
  }
}
//==================================================
// end of handy debug routines
//==================================================*/


// Install our load handler
addEventListener("load", sidebarOverlayInit, false);
