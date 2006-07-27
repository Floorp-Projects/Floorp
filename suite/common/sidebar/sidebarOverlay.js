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
var SIDEBAR_VERSION = "0.0";

// the default sidebar:
var sidebarObj = new Object;
sidebarObj.never_built = true;

function debug(msg) {
  // uncomment for noise
  //dump(msg+"\n");
}

var panel_observer = new Object;

panel_observer = {
  OnAssert : function(src,prop,target) { debug ("*** assert");},
  OnUnassert : function(src,prop,target) {
    // Wait for unassert that marks the end of the customize changes.
    // See customize.js for where this is unasserted.
    if (prop == RDF.GetResource(NC + "inbatch")) {
      sidebar_open_default_panel(100, 0);
    }
  },
  OnChange : function(src,prop,old_target,new_target) {},
  OnMove : function(old_src,new_src,prop,target) {}
}


function get_sidebar_datasource_uri(panels_file_id) {
  try {
    var locator_interface = Components.interfaces.nsIFileLocator;
    var locator_prog_id = 'component://netscape/filelocator';
    var locator_service = Components.classes[locator_prog_id].getService();
    // use the fileLocator to look in the profile directory
    // to find 'panels.rdf', which is the
    // database of the user's currently selected panels.
    locator_service = locator_service.QueryInterface(locator_interface);

    // if <profile>/panels.rdf doesn't exist, GetFileLocation() will copy
    // bin/defaults/profile/panels.rdf to <profile>/panels.rdf
    var sidebar_file = locator_service.GetFileLocation(panels_file_id);

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
    debug("Error: Unable to load panels file.\n");
    return null;
  }
}

function sidebar_overlay_init() {
  sidebarObj.datasource_uri = get_sidebar_datasource_uri(PANELS_RDF_FILE);
  sidebarObj.resource = 'urn:sidebar:current-panel-list';
  
  sidebarObj.master_datasources = get_remote_datasource_url();
  sidebarObj.master_datasources += " chrome://sidebar/content/local-panels.rdf";
  sidebarObj.master_resource = 'urn:sidebar:master-panel-list';
  sidebarObj.component = document.location.href;

  // Initialize the display
  var sidebar_element  = document.getElementById('sidebar-box');
  var sidebar_menuitem = document.getElementById('menu_sidebar');
  if (sidebar_element.getAttribute('hidden') == 'true') {
    if (sidebar_menuitem) {
      sidebar_menuitem.setAttribute('checked', 'false');
    }
  } else {
    if (sidebar_menuitem) {
      sidebar_menuitem.setAttribute('checked', 'true');
    }
    
    if (sidebarObj.never_built) {
      sidebarObj.never_built = false;

      debug("sidebar = " + sidebarObj);
      debug("sidebarObj.resource = " + sidebarObj.resource);
      debug("sidebarObj.datasource_uri = " + sidebarObj.datasource_uri);

      // Add the user's current panel choices to the template builder,
      // which will aggregate it with the other datasources that describe
      // the individual panel's title, customize URL, and content URL.
      var panels = document.getElementById('sidebar-panels');
      panels.database.AddDataSource(RDF.GetDataSource(sidebarObj.datasource_uri));
      
      // The stuff on the bottom
      var panels_bottom = document.getElementById('sidebar-panels-bottom');
      panels_bottom.database.AddDataSource(RDF.GetDataSource(sidebarObj.datasource_uri));
    
      debug("Adding observer to database.");
      panels.database.AddObserver(panel_observer);

      // XXX This is a hack to force re-display
      panels.setAttribute('ref', sidebarObj.resource);

      // XXX This is a hack to force re-display
      panels_bottom.setAttribute('ref', sidebarObj.resource);
    }
    sidebar_open_default_panel(100, 0);
  }
}

function sidebar_overlay_destruct() {
    var panels = document.getElementById('sidebar-panels');
    debug("Removeing observer from database.");
    panels.database.RemoveObserver(panel_observer);
}

// Get the template for the available panels url from preferences.
// Replace variables in the url:
//     %LOCALE%  -->  Application locale (e.g. en-us).
//     %VERSION% --> Sidebar file format version (e.g. 0.0).
function get_remote_datasource_url() {
  var url = '';
  var prefs = Components.classes['component://netscape/preferences'];
  if (prefs) {
    prefs = prefs.getService();
  }
  if (prefs) {
    prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
  }
  if (prefs) {
    try {
      url = prefs.CopyCharPref("sidebar.customize.all_panels.url");
      url = url.replace(/%SIDEBAR_VERSION%/g, SIDEBAR_VERSION);

      var locale_progid = 'component://netscape/intl/nslocaleservice';
      var locale = Components.classes[locale_progid].getService();
      locale = locale.QueryInterface(Components.interfaces.nsILocaleService);
      locale = locale.GetLocaleComponentForUserAgent();
      locale = locale.toLowerCase();
      url = url.replace(/%LOCALE%/g, locale);

      debug("Remote url is " + url);
    } catch(ex) {
      debug("Unable to get remote url pref. What now? "+ex);
    }
  }
  return url;
}

function sidebar_open_default_panel(wait, tries) {
  var panels_top  = document.getElementById('sidebar-panels');
  var target = panels_top.getAttribute('open-panel-src');
  var top_headers = panels_top.childNodes;

  debug("sidebar_open_default_panel("+wait+","+tries+")");

  if (top_headers.length <= 1) {
    if (tries < 5) {
      // No children yet, try again later
      setTimeout('sidebar_open_default_panel('+(wait*2)+','+(tries+1)+')',wait);
    } else {
      // No panels.
      // XXX This should load some help page instead of about:blank.
      var iframe = document.getElementById('sidebar-content');
      iframe.setAttribute('src', 'about:blank');
    }
    return;
  }

  select_panel(target);
}

function select_panel(target) {
  var panels_top = document.getElementById('sidebar-panels');
  var iframe = document.getElementById('sidebar-content');
  var top_headers = panels_top.childNodes;

  debug("select_panel("+target+")");

  iframe.setAttribute('src', 'about:blank');

  var select_index = find_panel(top_headers, target);

  if (!select_index) {
    // Target not found. Pick the first panel by default.
    // It is at index 1 because the template is at index 0.
    debug("select_panel: target not found, choosing last panel, index "+top_headers.length+"\n");
    select_index = pick_default_panel(top_headers);
    target = top_headers.item(select_index).getAttribute('iframe-src');
  }

  // Update the content area if necessary.
  if (iframe.getAttribute('src') != target) {
    iframe.setAttribute('src', target);
  }
  if (panels_top.getAttribute('open-panel-src') != target) {
    panels_top.setAttribute('open-panel-src', target);
  }

  update_headers(select_index);
}

function find_panel(panels, target) {
  if (target && target != '') {
    // Find the index of the selected panel
    for (var ii=1; ii < panels.length; ii++) {
      var item = panels.item(ii);
      
      if (item.getAttribute('iframe-src') == target) {
        if (is_excluded(item)) {
          debug("find_panel: Found panel at index, "+ii+", but it is excluded");
          return 0;
        } else {
          // Found it!
          debug("find_panel: Found panel at index, "+ii);
          return ii;
        }
      }
    }
  }
  return 0;
}

function pick_default_panel(panels) {
  last_non_excluded_index = null;
  for (var ii=1; ii < panels.length; ii++) {
    if (!is_excluded(panels.item(ii))) {
      last_non_excluded_index = ii;
    }
  }
  return last_non_excluded_index;
}

function is_excluded(item) {
  var exclude = item.getAttribute('exclude');
  var src = item.getAttribute('iframe-src');
  debug("src="+src);
  debug("exclude="+exclude);
  return exclude && exclude != '' && exclude.indexOf(sidebarObj.component) != -1;
}

function update_headers(index) {
  var top_headers = document.getElementById('sidebar-panels').childNodes;
  var bottom_headers = document.getElementById('sidebar-panels-bottom').childNodes;

  for (var ii=1; ii < top_headers.length; ii++) {
    var top_item = top_headers.item(ii);
    var bottom_item = bottom_headers.item(ii);

    if (is_excluded(top_item)) {
      top_item.setAttribute('hidden','true');
      bottom_item.setAttribute('hidden','true');
    } else if (ii < index) { 
      top_item.removeAttribute('selected');
      top_item.removeAttribute('hidden');
      bottom_item.setAttribute('hidden','true');
    } else if (ii == index) {
      top_item.setAttribute('selected','true');
      top_item.removeAttribute('hidden');
      bottom_item.setAttribute('hidden','true');
    } else {
      top_item.setAttribute('hidden','true');
      bottom_item.removeAttribute('hidden');
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

  select_panel(target);
}

// No one is calling this right now.
function SidebarReload() {
  sidebar_open_default_panel(100, 0);
}

// Set up a lame hack to avoid opening two customize
// windows on a double click.
var gDisableCustomize = false;
function enable_customize() {
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
                          sidebarObj.master_datasources,
                          sidebarObj.master_resource,
                          sidebarObj.datasource_uri,
                          sidebarObj.resource);
      setTimeout(enable_customize, 2000);
    }
  }
}

// Show/Hide the entire sidebar.
// Envoked by the "View / Sidebar" menu option.
function SidebarShowHide() {
  var sidebar_box = document.getElementById('sidebar-box');
  var sidebar_splitter = document.getElementById('sidebar-splitter');
  var is_hidden = sidebar_box.getAttribute('hidden');

  if (is_hidden && is_hidden == "true") {
    debug("Showing the sidebar");
    sidebar_box.removeAttribute('hidden');
    sidebar_splitter.removeAttribute('hidden');
    sidebar_overlay_init();
  } else {
    debug("Hiding the sidebar");
    sidebar_box.setAttribute('hidden','true');
    sidebar_splitter.setAttribute('hidden','true');
  }
  // Immediately save persistent values
  document.persist('sidebar-box', 'hidden');
  persist_width();
}

// SidebarExpandCollapse() - Respond to grippy click.
function SidebarExpandCollapse() {
  // XXX Mini hack. Persist isn't working too well. Force the persist,
  // but wait until the change has commited.
  setTimeout("document.persist('sidebar-box', 'collapsed');",100);
}

function PersistHeight() {
  // XXX Mini hack. Persist isn't working too well. Force the persist,
  // but wait until the last drag has been committed.
  // May want to do something smarter here like only force it if the 
  // width has really changed.
  setTimeout("document.persist('sidebar-panels-box','height');",100);
}

function persist_width() {
  // XXX Partial workaround for bug #19488.
  var sidebar_box = document.getElementById('sidebar-box');

  var width = sidebar_box.getAttribute('width');
  if (width && (width > 410 || width < 15)) {
    sidebar_box.setAttribute('width',168);
  }

  // XXX Mini hack. Persist isn't working too well. Force the persist,
  // but wait until the width change has commited.
  setTimeout("document.persist('sidebar-box', 'width');",100);
}

function SidebarFinishDrag() {

  // XXX Semi-hack for bug #16516.
  // If we had the proper drag event listener, we would not need this
  // timeout. The timeout makes sure the width is written to disk after
  // the sidebar-box gets the newly dragged width.
  setTimeout("persist_width()",100);
}

/*==================================================
// Handy debug routines
//==================================================
function dump_attributes(node,depth) {
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

function dump_tree(node, depth) {
  if (!node) {
    debug("dump_tree: node is null");
  }
  var indent = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | | + ";
  debug(indent.substr(indent.length - depth*2) + node.nodeName);
  if (node.nodeName != "#text") {
    //debug(" id="+node.getAttribute('id'));
    dump_attributes(node, depth);
  }
  var kids = node.childNodes;
  for (var ii=0; ii < kids.length; ii++) {
    dump_tree(kids[ii], depth + 1);
  }
}
//==================================================
// end of handy debug routines
//==================================================*/


// Install our load handler
addEventListener("load", sidebar_overlay_init, false);
addEventListener("unload", sidebar_overlay_destruct, false);
