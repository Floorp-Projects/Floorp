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
  OnAssert : function(src,prop,target) { 
    debug ("*** sidebar observer: assert");
    // "refresh" is asserted by select menu and by customize.js.
    if (prop == RDF.GetResource(NC + "refresh")) {
      sidebar_refresh();
    }
  },
  OnUnassert : function(src,prop,target) {
    debug ("*** sidebar observer: unassert");
  },
  OnChange : function(src,prop,old_target,new_target) {
    debug ("*** sidebar observer: change");
  },
  OnMove : function(old_src,new_src,prop,target) {
    debug ("*** sidebar observer: move");
  }
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
  sidebarObj.master_datasources += " chrome://communicator/content/sidebar/local-panels.rdf";
  sidebarObj.master_resource = 'urn:sidebar:master-panel-list';
  sidebarObj.component = document.firstChild.getAttribute('windowtype');
  sidebar.loading_iframe = null;
  debug("sidebarObj.component is " + sidebarObj.component);

  // Initialize the display
  var sidebar_element = document.getElementById('sidebar-box');
  var sidebar_menuitem = document.getElementById('sidebar-menu');
  if (sidebar_is_hidden()) {
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

      // Show the header for the panels area. Use a splitter if there
      // is stuff over the panels area.
      var sidebar_title = document.getElementById('sidebar-title-box');
      if (sidebar_element.firstChild == sidebar_title) {
        sidebar_title.setAttribute('type','box');
      } else {
        sidebar_title.setAttribute('type','splitter');
      }
      
      // Add the user's current panel choices to the template builder,
      // which will aggregate it with the other datasources that describe
      // the individual panel's title, customize URL, and content URL.
      var panels = document.getElementById('sidebar-panels');
      panels.database.AddDataSource(RDF.GetDataSource(sidebarObj.datasource_uri));
      
      debug("Adding observer to database.");
      panels.database.AddObserver(panel_observer);
      // XXX This is a hack to force re-display
      panels.setAttribute('ref', sidebarObj.resource);
    }
    sidebar_open_default_panel(100, 0);
  }
}

function sidebar_overlay_destruct() {
    var panels = document.getElementById('sidebar-panels');
    debug("Removing observer from database.");
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
    var locale;
    try {
      url = prefs.CopyCharPref("sidebar.customize.all_panels.url");
      url = url.replace(/%SIDEBAR_VERSION%/g, SIDEBAR_VERSION);
      locale = prefs.CopyCharPref("intl.content.langcode");
    } catch(ex) {
      debug("Unable to get remote url pref. What now? "+ex);
    }

    if (!locale) {
      // activation part not ready yet!

      try {
        var locale_progid = 'component://netscape/intl/nslocaleservice';
        var syslocale = Components.classes[locale_progid].getService();
        syslocale = syslocale.QueryInterface(Components.interfaces.nsILocaleService);
        locale = syslocale.GetLocaleComponentForUserAgent();
	  } catch(ex) {
        debug("Unable to get system locale. What now? "+ex);
      }
    }

    if (locale) {
      locale = locale.toLowerCase();
      url = url.replace(/%LOCALE%/g, locale);
    }
    debug("Remote url is " + url);
  }
  return url;
}

function sidebar_open_default_panel(wait, tries) {
  var panels = document.getElementById('sidebar-panels');

  debug("sidebar_open_default_panel("+wait+","+tries+")");

  // Make sure the sidebar exists before trying to refresh it.
  if (panels.childNodes.length <= 1) {
    if (tries < 5) {
      // No children yet, try again later
      setTimeout('sidebar_open_default_panel('+(wait*2)+','+(tries+1)+')',wait);
    } else {
      // No panels.
      // XXX This should load some help page
    }
    return;
  }
  sidebar_refresh();
}

function sidebar_refresh() {
  var panels = document.getElementById('sidebar-panels');
  var last_selected_panel = panels.getAttribute('last-selected-panel');
  if (is_selected(last_selected_panel)) {
    // A panel is already selected
    update_panels();
  } else {
    // This is either the first refresh after creating the sidebar,
    // or a panel has been added or removed.

    var sidebar_container = document.getElementById('sidebar-box');
    var default_panel = sidebar_container.getAttribute('defaultpanel');
    if (default_panel != '') {
      // Use value of "defaultpanel" which was set in the content.
      select_panel(default_panel);
    } else {
      // Select the most recently selected panel.
      select_panel(last_selected_panel);

    }
  }
}

function is_selected(panel_id) {
  var panels = document.getElementById('sidebar-panels');
  var panel_index = find_panel(panels, panel_id);
  if (panel_index == 0) return false;
  var header = panels.childNodes.item(panel_index);
  if (!header) return false;
  return 'true' == header.getAttribute('selected') ;
}

function select_panel(target) {
  var panels = document.getElementById('sidebar-panels');
  var iframe = document.getElementById('sidebar-content');

  debug("select_panel("+target+")");
  
  var select_index = find_panel(panels, target);

  if (!select_index) {
    // Target not found. Pick the last panel by default.
    // It is at index 1 because the template is at index 0.
    select_index = pick_last_panel(panels);
    debug("select_panel: target not found, choosing last panel, index "+select_index+"\n");
    target = panels.childNodes.item(select_index).getAttribute('id');
  }

  if (panels.getAttribute('last-selected-panel') != target) {
    panels.setAttribute('last-selected-panel', target);
  }
  update_panels();
}

function find_panel(panels, target) {
  if (target && target != '') {
    // Find the index of the selected panel
    for (var ii=1; ii < panels.childNodes.length; ii += 2) {
      var item = panels.childNodes.item(ii);
      
      if (item.getAttribute('id') == target) {
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

function pick_last_panel(panels) {
  last_non_excluded_index = null;
  debug("pick_default_panel: length="+panels.childNodes.length);
  for (var ii=1; ii < panels.childNodes.length; ii += 2) {
    if (!is_excluded(panels.childNodes.item(ii))) {
      last_non_excluded_index = ii;
    }
  }
  return last_non_excluded_index;
}

function is_excluded(item) {
  var exclude = item.getAttribute('exclude');
  return ( exclude && exclude != '' && 
           exclude.indexOf(sidebarObj.component) != -1 );
}

function panel_loader() {
  debug("---------- panel_loader");
  dump_tree(this);
  sidebar.loading_iframe = null;
  this.removeEventListener("load", panel_loader, true);
  this.removeAttribute('collapsed');
  this.parentNode.firstChild.setAttribute('hidden','true');
}

function get_iframe(content) {
  debug("---------- get_iframe");
  dump_tree(content);
  var unsandboxed_iframe = content.childNodes.item(1);
  var sandboxed_iframe = content.childNodes.item(2);
  if (unsandboxed_iframe.getAttribute('src').match(/^chrome:/)) {
    return unsandboxed_iframe;
  } else {
    return sandboxed_iframe;
  }
}

function update_panels() {
  // This function requires that the attributre 'last-selected-panel'
  // holds the id of a non-excluded panel. If it doesn't, no panel will
  // be selected.
  var panels = document.getElementById('sidebar-panels');
  var selected_id = panels.getAttribute('last-selected-panel');
  var have_set_top = 0;
  var have_set_after_selected = 0;
  var is_after_selected = 0;
  var last_header = 0;
  for (var ii=1; ii < panels.childNodes.length; ii += 2) {
    var header = panels.childNodes.item(ii);
    var content = panels.childNodes.item(ii+1);
    var id = header.getAttribute('id');
    if (is_excluded(header)) {
      debug("item("+ii+") excluded");
      header.setAttribute('hidden','true');
      content.setAttribute('hidden','true');
    } else {
      last_header = header;
      header.removeAttribute('last-panel');
      if (!have_set_top) {
        header.setAttribute('top-panel','true');
        have_set_top = 1
      } else {
        header.removeAttribute('top-panel');
      }
      if (!have_set_after_selected && is_after_selected) {
        header.setAttribute('first-panel-after-selected','true');
        have_set_after_selected = 1
      } else {
        header.removeAttribute('first-panel-after-selected');
      }
      header.removeAttribute('hidden');

      if (selected_id == id) {
        is_after_selected = 1
        debug("item("+ii+") selected");
        header.setAttribute('selected', 'true');
        content.removeAttribute('hidden');
        content.removeAttribute('collapsed');
        var previously_shown = content.getAttribute('have-shown');
        content.setAttribute('have-shown','true');
        if (!previously_shown) {
          // Pick sandboxed, or unsandboxed iframe
          var iframe = get_iframe(content);
          iframe.removeAttribute('hidden');

          // Use a timeout to set the onload handler.
          // Otherwise, this fires too early and javascript dies. Bug 37428.
          sidebar.loading_iframe = iframe;
          setTimeout("sidebar.loading_iframe.addEventListener('load', panel_loader, true);", 1);
        }
      } else { 
        debug("item("+ii+") unselected");
        header.removeAttribute('selected');
        content.setAttribute('collapsed','true');
      }
    }
  }
  if (last_header) {
    last_header.setAttribute('last-panel','true');
  }
}

// Change the sidebar content to the selected panel.
// Called when a panel title is clicked.
function SidebarSelectPanel(tab) {
  var target = tab.getAttribute('id');
  var last_panel = tab.parentNode.getAttribute('last-selected-panel');

  if (target == last_panel) {
    // XXX Maybe this should reload the content?
    return;
  }

  select_panel(target);
}

// No one is calling this right now.
function SidebarReload() {
  sidebar_refresh();
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
      debug("First time creating customize dialog");
      gDisableCustomize = true;

      var panels = document.getElementById('sidebar-panels');
      
      customizeWindow = window.openDialog(
                         'chrome://communicator/content/sidebar/customize.xul',
                         '_blank','centerscreen,chrome,resizable',
                         sidebarObj.master_datasources,
                         sidebarObj.master_resource,
                         sidebarObj.datasource_uri,
                         sidebarObj.resource);
      setTimeout(enable_customize, 2000);
    }
  }
}

function sidebar_is_hidden() {
  var sidebar_title = document.getElementById('sidebar-title-box');
  var sidebar_box = document.getElementById('sidebar-box');
  return sidebar_box.getAttribute('hidden') == 'true'
         || sidebar_title.getAttribute('hidden') == 'true';
}

// Show/Hide the entire sidebar.
// Envoked by the "View / Sidebar" menu option.
function SidebarShowHide() {
  var sidebar_box = document.getElementById('sidebar-box');
  var sidebar_title = document.getElementById('sidebar-title-box');
  var sidebar_panels = document.getElementById('sidebar-panels');
  var sidebar_splitter = document.getElementById('sidebar-splitter');
  var hide_everything = sidebar_title.getAttribute('type') == 'box';

  var elem1, elem2, elem1_name;
  if (sidebar_is_hidden()) {
    debug("Showing the sidebar");
    sidebar_box.removeAttribute('hidden');
    sidebar_title.removeAttribute('hidden');
    sidebar_panels.removeAttribute('hidden');
    sidebar_splitter.removeAttribute('hidden');
    sidebar_overlay_init();
  } else {
    debug("Hiding the sidebar");
    if (hide_everything) {
      sidebar_box.setAttribute('hidden', 'true');
      sidebar_splitter.setAttribute('hidden', 'true');
    }
    sidebar_title.setAttribute('hidden', 'true');
    sidebar_panels.setAttribute('hidden', 'true');
  }
  // Immediately save persistent values
  document.persist('sidebar-title-box', 'hidden');
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
  var sidebar_title = document.getElementById('sidebar-title-box');
  if (sidebar_title && sidebar_title.getAttribute('type') == "splitter") {
    setTimeout("document.persist('sidebar-panels','height');",100);
  }
}

function persist_width() {
  // XXX Partial workaround for bug #19488.
  var sidebar_box = document.getElementById('sidebar-box');

  var width = sidebar_box.getAttribute('width');
  if (width && (width > 410 || width < 100)) {
    sidebar_box.setAttribute('width',168);
    debug("Forcing sidebar width to 168. It was too narror or too wide)");
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

function SidebarBuildPickerPopup() {
  var menu = document.getElementById('sidebar-panel-picker-popup');
  menu.database.AddDataSource(RDF.GetDataSource(sidebarObj.datasource_uri));
  menu.setAttribute('ref', sidebarObj.resource);

  for (var ii=3; ii < menu.childNodes.length; ii++) {
    var panel_menuitem = menu.childNodes.item(ii);
    if (is_excluded(panel_menuitem)) {
      debug(ii+": "+panel_menuitem.getAttribute('value')+ ": excluded; uncheck.");
      panel_menuitem.removeAttribute('checked');
    } else {
      debug(ii+": "+panel_menuitem.getAttribute('value')+ ": included; check.");
      panel_menuitem.setAttribute('checked', 'true');
    }
  }
}

function SidebarTogglePanel(panel_menuitem) {
  // Create a "container" wrapper around the current panels to
  // manipulate the RDF:Seq more easily.
  sidebarObj.datasource = RDF.GetDataSource(sidebarObj.datasource_uri);

  var panel_id = panel_menuitem.getAttribute('id')
  var panel_exclude = panel_menuitem.getAttribute('exclude')
  if (panel_exclude == '') {
    // Nothing excluded for this panel yet, so add this component to the list.
    debug("Excluding " + panel_id + " from " + sidebarObj.component);
    sidebarObj.datasource.Assert(RDF.GetResource(panel_id),
                                RDF.GetResource(NC + "exclude"),
                                RDF.GetLiteral(sidebarObj.component),
                                true);
  } else {
    // Panel has an exclude string, but it may or may not have the
    // current component listed in the string.
    debug("Current exclude string: " + panel_exclude);

    var new_exclude = panel_exclude;
    if (is_excluded(panel_menuitem)) {
      debug("Plucking this component out of the exclude list");
      replace_pat = new RegExp(sidebarObj.component + "\s*");
      new_exclude = new_exclude.replace(replace_pat,'');
      new_exclude = new_exclude.replace(/^\s+/,'');
      select_panel(panel_id);
    } else {
      debug("Adding this component to the exclude list");
      new_exclude = new_exclude + " " + sidebarObj.component;
    }
    if (new_exclude == '') {
      debug("Removing exclude list");
      sidebarObj.datasource.Unassert(RDF.GetResource(panel_id),
                                    RDF.GetResource(NC + "exclude"),
                                    RDF.GetLiteral(sidebarObj.component));
      select_panel(panel_id);
    } else {
      debug("New exclude string: " + new_exclude);
      exclude_target = 
        sidebarObj.datasource.GetTarget(RDF.GetResource(panel_id),
                                       RDF.GetResource(NC + "exclude"),
                                       true);
      sidebarObj.datasource.Change(RDF.GetResource(panel_id),
                                  RDF.GetResource(NC + "exclude"),
                                  exclude_target,
                                  RDF.GetLiteral(new_exclude));
    }
  }

  // force all the sidebars to update
  refresh_all_sidebars();

  // Write the modified panels out.
  sidebarObj.datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush();
}

// Use an assertion to pass a "refresh" event to all the sidebars.
// They use observers to watch for this assertion (see above).
function refresh_all_sidebars() {
  sidebarObj.datasource.Assert(RDF.GetResource(sidebarObj.resource),
                               RDF.GetResource(NC + "refresh"),
                               RDF.GetLiteral("true"),
                               true);
  sidebarObj.datasource.Unassert(RDF.GetResource(sidebarObj.resource),
                                 RDF.GetResource(NC + "refresh"),
                                 RDF.GetLiteral("true"));
}

//*==================================================
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

function dump_tree(node) {
  dump_tree_recur(node, 0, 0);
}

function dump_tree_recur(node, depth, index) {
  if (!node) {
    debug("dump_tree: node is null");
  }
  var indent = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | | + ";
  debug(indent.substr(indent.length - depth*2) + index + " " + node.nodeName);
  if (node.nodeName != "#text") {
    //debug(" id="+node.getAttribute('id'));
    dump_attributes(node, depth);
  }
  var kids = node.childNodes;
  for (var ii=0; ii < kids.length; ii++) {
    dump_tree_recur(kids[ii], depth + 1, ii);
  }
}
//==================================================
// end of handy debug routines
//==================================================*/


// Install our load handler
addEventListener("load", sidebar_overlay_init, false);
addEventListener("unload", sidebar_overlay_destruct, false);
