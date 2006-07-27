/* -*- Mode: Java; tab-width: 4; insert-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

//////////////////////////////////////////////////////////////
// Global variables
//////////////////////////////////////////////////////////////

// Set to true for noise
const CUST_DEBUG = false;

// the rdf service
var RDF = '@mozilla.org/rdf/rdf-service;1'
RDF = Components.classes[RDF].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

var NC = "http://home.netscape.com/NC-rdf#";

var sidebarObj = new Object;
var allPanelsObj = new Object;
var original_panels = new Array();

//////////////////////////////////////////////////////////////
// Sidebar Init/Destroy
//////////////////////////////////////////////////////////////

function sidebar_customize_init()
{
  allPanelsObj.datasources = window.arguments[0];
  allPanelsObj.resource    = window.arguments[1];
  sidebarObj.datasource_uri     = window.arguments[2];
  sidebarObj.resource           = window.arguments[3];

  debug("Init: all panels datasources = " + allPanelsObj.datasources);
  debug("Init: all panels resource    = " + allPanelsObj.resource);
  debug("Init: sidebarObj.datasource_uri = " + sidebarObj.datasource_uri);
  debug("Init: sidebarObj.resource       = " + sidebarObj.resource);

  var all_panels   = document.getElementById('other-panels');
  var current_panels = document.getElementById('current-panels');

  debug("Adding observer to all panels database.");
  all_panels.database.AddObserver(panels_observer);

  allPanelsObj.datasources = allPanelsObj.datasources.replace(/^\s+/,'');
  allPanelsObj.datasources = allPanelsObj.datasources.replace(/\s+$/,'');
  allPanelsObj.datasources = allPanelsObj.datasources.split(/\s+/);
  for (var ii = 0; ii < allPanelsObj.datasources.length; ii++) {
    debug("Init: Adding "+allPanelsObj.datasources[ii]);

    // This will load the datasource, if it isn't already.
    var datasource = RDF.GetDataSource(allPanelsObj.datasources[ii]);
    all_panels.database.AddDataSource(datasource);
  }

  // Add the datasource for current list of panels. It selects panels out
  // of the other datasources.
  debug("Init: Adding current panels, "+sidebarObj.datasource_uri);
  sidebarObj.datasource = RDF.GetDataSource(sidebarObj.datasource_uri);

  // Root the customize dialog at the correct place.
  debug("Init: reset all panels ref, "+allPanelsObj.resource);
  all_panels.setAttribute('ref', allPanelsObj.resource);

  // Create a "container" wrapper around the current panels to
  // manipulate the RDF:Seq more easily.
  var panel_list = sidebarObj.datasource.GetTarget(RDF.GetResource(sidebarObj.resource), RDF.GetResource(NC + "panel-list"), true);
  sidebarObj.container = Components.classes["@mozilla.org/rdf/container;1"].createInstance(Components.interfaces.nsIRDFContainer);
  sidebarObj.container.Init(sidebarObj.datasource, panel_list);

  // Add all the current panels to the tree
  current_panels = sidebarObj.container.GetElements();
  while (current_panels.hasMoreElements()) {
    var panel = current_panels.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
    if (add_node_to_current_list(sidebarObj.datasource, panel) >= 0) {
      original_panels.push(panel.Value);
      original_panels[panel.Value] = true;
    }
  }

  var links =
    all_panels.database.GetSources(RDF.GetResource(NC + "haslink"),
                                   RDF.GetLiteral("true"), true);

  while (links.hasMoreElements()) {
    var folder = 
      links.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
    var folder_name = folder.Value;
    debug("+++ fixing up remote container " + folder_name + "\n");
    fixup_remote_container(folder_name);
  }

  sizeToContent();
}

function sidebar_customize_destruct()
{
  var all_panels = document.getElementById('other-panels');
  debug("Removing observer from all_panels database.");
  all_panels.database.RemoveObserver(panels_observer);
}


//////////////////////////////////////////////////////////////////
// Panels' RDF Datasource Observer
//////////////////////////////////////////////////////////////////
var panels_observer = {
  onAssert : function(ds,src,prop,target) {
    //debug ("observer: assert");
    // "refresh" is asserted by select menu and by customize.js.
    if (prop == RDF.GetResource(NC + "link")) {
      setTimeout("fixup_remote_container('"+src.Value+"')",100);
      //fixup_remote_container(src.Value);
    }
  },
  onUnassert : function(ds,src,prop,target) {
    //debug ("observer: unassert");
  },
  onChange : function(ds,src,prop,old_target,new_target) {
    //debug ("observer: change");
  },
  onMove : function(ds,old_src,new_src,prop,target) {
    //debug ("observer: move");
  },
  onBeginUpdateBatch : function(ds) {
    //debug ("observer: onBeginUpdateBatch");
  },
  onEndUpdateBatch : function(ds) {
    //debug ("observer: onEndUpdateBatch");
  }
};

function fixup_remote_container(id)
{
  debug('fixup_remote_container('+id+')');

  var container = document.getElementById(id);
  if (container) {
    container.setAttribute('container', 'true');
    container.removeAttribute('open');
  }
}

function fixup_children(id) {
  // Add container="true" on nodes with "link" attribute
  var treeitem = document.getElementById(id);

  var children = treeitem.childNodes.item(1).childNodes;
  for (var ii=0; ii < children.length; ii++) {
    var child = children.item(ii);
    if (child.getAttribute('link') != '' &&
        child.getAttribute('container') != 'true') {
      child.setAttribute('container', 'true');
      child.removeAttribute('open');
    }
  }
}

function get_attr(registry,service,attr_name)
{
  var attr = registry.GetTarget(service,
                                RDF.GetResource(NC + attr_name),
                                true);
  if (attr)
    attr = attr.QueryInterface(Components.interfaces.nsIRDFLiteral);
  if (attr)
    attr = attr.Value;
  return attr;
}

function SelectChangeForOtherPanels(event, target)
{
  enable_buttons_for_other_panels();
}

function ClickOnOtherPanels(event)
{
  var tree = document.getElementById("other-panels");
  
  var rowIndex = -1;
  if (event.type == "click" && event.button == 0) {
    var row = {}, col = {}, obj = {};
    var b = tree.treeBoxObject;
    b.getCellAt(event.clientX, event.clientY, row, col, obj);

    if (obj.value == "twisty" || event.detail == 2) {
      rowIndex = row.value;
    }
  }

  if (rowIndex < 0) return;
  
  var treeitem = tree.contentView.getItemAtIndex(rowIndex);
  var res = RDF.GetResource(treeitem.id);
  
  if (treeitem.getAttribute('container') == 'true') {
    if (treeitem.getAttribute('open') == 'true') {
      var link = treeitem.getAttribute('link');
      var loaded_link = treeitem.getAttribute('loaded_link');
      if (link != '' && !loaded_link) {
        debug("Has remote datasource: "+link);
        add_datasource_to_other_panels(link);
        treeitem.setAttribute('loaded_link', 'true');
      } else {
        setTimeout('fixup_children("'+ treeitem.getAttribute('id') +'")', 100);
      }
    }
  }

  // Remove the selection in the "current" panels list
  var current_panels = document.getElementById('current-panels');
  current_panels.view.selection.clearSelection();
  enable_buttons_for_current_panels();
}

function add_datasource_to_other_panels(link) {
  // Convert the |link| attribute into a URL
  var url = document.location;
  debug("Current URL:  " +url);
  debug("Current link: " +link);

  var uri = Components.classes['@mozilla.org/network/standard-url;1'].createInstance();
  uri = uri.QueryInterface(Components.interfaces.nsIURI);
  uri.spec = url;
  uri = uri.resolve(link);

  debug("New URL:      " +uri);

  // Add the datasource to the tree
  var all_panels = document.getElementById('other-panels');
  all_panels.database.AddDataSource(RDF.GetDataSource(uri));

  // XXX This is a hack to force re-display
  //all_panels.setAttribute('ref', allPanelsObj.resource);
}

// Handle a selection change in the current panels.
function SelectChangeForCurrentPanels() {
  // Remove the selection in the available panels list
  var all_panels = document.getElementById('other-panels');
  all_panels.view.selection.clearSelection();

  enable_buttons_for_current_panels();
}

// Move the selected item up the the current panels list.
function MoveUp() {
  var tree = document.getElementById('current-panels');
  if (tree.view.selection.count == 1) {
    var index = tree.currentIndex;
    var selected = tree.contentView.getItemAtIndex(index);
    var before = selected.previousSibling;
    if (before) {
      before.parentNode.removeChild(selected);
      before.parentNode.insertBefore(selected, before);
      tree.view.selection.select(index-1);
      tree.treeBoxObject.ensureRowIsVisible(index-1);
    }
  }
}

// Move the selected item down the the current panels list.
function MoveDown() {
  var tree = document.getElementById('current-panels');
  if (tree.view.selection.count == 1) {
    var index = tree.currentIndex;
    var selected = tree.contentView.getItemAtIndex(index);
    if (selected.nextSibling) {
      if (selected.nextSibling.nextSibling)
        selected.parentNode.insertBefore(selected, selected.nextSibling.nextSibling);
      else
        selected.parentNode.appendChild(selected);
      tree.view.selection.select(index+1);
      tree.treeBoxObject.ensureRowIsVisible(index+1);
    }
  }
}

function PreviewPanel()
{
  var tree = document.getElementById('other-panels');
  var database = tree.database;
  var sel = tree.view.selection;
  var rangeCount = sel.getRangeCount();
  for (var range = 0; range < rangeCount; ++range) {
    var min = {}, max = {};
    sel.getRangeAt(range, min, max);
    for (var index = min.value; index <= max.value; ++index) {
      var item = tree.contentView.getItemAtIndex(index);
      var res = RDF.GetResource(item.id);

      var preview_name = get_attr(database, res, 'title');
      var preview_URL  = get_attr(database, res, 'content');
      if (!preview_URL || !preview_name) continue;

      window.openDialog("chrome://communicator/content/sidebar/preview.xul",
                        "_blank", "chrome,resizable,close,dialog=no",
                        preview_name, preview_URL);
    }
  }
}

// Add the selected panel(s).
function AddPanel()
{
  var added = -1;
  
  var tree = document.getElementById('other-panels');
  var database = tree.database;
  var sel = tree.view.selection;
  var ranges = sel.getRangeCount();
  for (var range = 0; range < ranges; ++range) {
    var min = {}, max = {};
    sel.getRangeAt(range, min, max);
    for (var index = min.value; index <= max.value; ++index) {
      var item = tree.contentView.getItemAtIndex(index);
      if (item.getAttribute("container") != "true") {
        var res = RDF.GetResource(item.id);
        // Add the panel to the current list.
        added = add_node_to_current_list(database, res);
      }
    }
  }

  if (added >= 0) {
    // Remove the selection in the other list.
    // Selection will move to "current" list.
    tree.view.selection.clearSelection();

    var current_panels = document.getElementById('current-panels');
    current_panels.view.selection.select(added);
    current_panels.treeBoxObject.ensureRowIsVisible(added);
  }
}

// Copy a panel node into a database such as the current panel list.
function add_node_to_current_list(registry, service)
{
  debug("Adding "+service.Value);

  // Copy out the attributes we want
  var option_title     = get_attr(registry, service, 'title');
  var option_customize = get_attr(registry, service, 'customize');
  var option_content   = get_attr(registry, service, 'content');
  if (!option_title || !option_content)
    return -1;

  var tree = document.getElementById('current-panels');
  var tree_root = tree.lastChild;
  
  // Check to see if the panel already exists...
  var i = 0;
  for (var treeitem = tree_root.firstChild; treeitem; treeitem = treeitem.nextSibling) {
    if (treeitem.id == service.Value)
      // The panel is already in the current panel list.
      // Avoid adding it twice.
      return i;
    ++i;
  }

  // Create a treerow for the new panel
  var item = document.createElement('treeitem');
  var row  = document.createElement('treerow');
  var cell = document.createElement('treecell');

  // Copy over the attributes
  item.setAttribute('id', service.Value);
  cell.setAttribute('label', option_title);

  // Add it to the current panels tree
  item.appendChild(row);
  row.appendChild(cell);
  tree_root.appendChild(item);
  return i;
}

// Remove the selected panel(s) from the current list tree.
function RemovePanel()
{
  var tree = document.getElementById('current-panels');
  var sel = tree.view.selection;
  
  var nextNode = -1;
  var rangeCount = sel.getRangeCount();
  for (var range = rangeCount-1; range >= 0; --range) {
    var min = {}, max = {};
    sel.getRangeAt(range, min, max);
    for (var index = max.value; index >= min.value; --index) {
      var item = tree.contentView.getItemAtIndex(index);
      nextNode = item.nextSibling ? index : -1;
      item.parentNode.removeChild(item);
    }
  }

  if (nextNode >= 0)
    sel.select(nextNode);
}

// Bring up a new window with the customize url
// for an individual panel.
function CustomizePanel()
{
  var tree  = document.getElementById('current-panels');
  var numSelected = tree.view.selection.count;

  if (numSelected == 1) {
    var index = tree.currentIndex;
    var selectedNode = tree.contentView.getItemAtIndex(index);
    var panel_id = selectedNode.getAttribute('id');
    var customize_url = selectedNode.getAttribute('customize');

    debug("url   = " + customize_url);

    if (!customize_url) return;

    window.openDialog('chrome://communicator/content/sidebar/customize-panel.xul',
                      '_blank',
                      'chrome,resizable,width=690,height=600,dialog=no,close',
                      panel_id,
                      customize_url,
                      sidebarObj.datasource_uri,
                      sidebarObj.resource);
  }
}

function BrowseMorePanels()
{
  var url = '';
  var browser_url = "chrome://navigator/content/navigator.xul";
  var locale;
  try {
    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefBranch);
    url = prefs.getCharPref("sidebar.customize.more_panels.url");
    var temp = prefs.getCharPref("browser.chromeURL");
    if (temp) browser_url = temp;
  } catch(ex) {
    debug("Unable to get prefs: "+ex);
  }
  window.openDialog(browser_url, "_blank", "chrome,all,dialog=no", url);
}

function customize_getBrowserURL()
{
  return url;
}

// Serialize the new list of panels.
function Save()
{
  persist_dialog_dimensions();

  var all_panels = document.getElementById('other-panels');
  var current_panels = document.getElementById('current-panels');

  // See if list membership has changed
  var panels = [];
  var tree_root = current_panels.lastChild.childNodes;
  var list_unchanged = (tree_root.length == original_panels.length);
  for (var i = 0; i < tree_root.length; i++) {
    var panel = tree_root[i].id;
    panels.push(panel);
    panels[panel] = true;
    if (list_unchanged && original_panels[i] != panel)
      list_unchanged = false;
  }
  if (list_unchanged)
    return;

  // Remove all the current panels from the datasource.
  current_panels = sidebarObj.container.GetElements();
  while (current_panels.hasMoreElements()) {
    panel = current_panels.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
    if (panel.Value in panels) {
      // This panel will remain in the sidebar.
      // Remove the resource, but keep all the other attributes.
      // Removing it will allow it to be added in the correct order.
      // Saving the attributes will preserve things such as the exclude state.
      sidebarObj.container.RemoveElement(panel, false);
    } else {
      // Kiss it goodbye.
      delete_resource_deeply(sidebarObj.container, panel);
    }
  }

  // Add the new list of panels
  for (var ii = 0; ii < panels.length; ++ii) {
    var id = panels[ii];
    var resource = RDF.GetResource(id);
    if (id in original_panels) {
      sidebarObj.container.AppendElement(resource);
    } else {
      copy_resource_deeply(all_panels.database, resource, sidebarObj.container);
    }
  }
  refresh_all_sidebars();

  // Write the modified panels out.
  sidebarObj.datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush();
}

// Search for an element in an array
function has_element(array, element) {
  for (var ii=0; ii < array.length; ii++) {
    if (array[ii] == element) {
      return true;
    }
  }
  return false;
}

// Search for targets from resource in datasource
function has_targets(datasource, resource) {
  var arcs = datasource.ArcLabelsOut(resource);
  return arcs.hasMoreElements();
}

// Use an assertion to pass a "refresh" event to all the sidebars.
// They use observers to watch for this assertion (in sidebarOverlay.js).
function refresh_all_sidebars() {
  sidebarObj.datasource.Assert(RDF.GetResource(sidebarObj.resource),
                               RDF.GetResource(NC + "refresh"),
                               RDF.GetLiteral("true"),
                               true);
  sidebarObj.datasource.Unassert(RDF.GetResource(sidebarObj.resource),
                                 RDF.GetResource(NC + "refresh"),
                                 RDF.GetLiteral("true"));
}

// Remove a resource and all the arcs out from it.
function delete_resource_deeply(container, resource) {
  var arcs = container.DataSource.ArcLabelsOut(resource);
  while (arcs.hasMoreElements()) {
    var arc = arcs.getNext();
    var targets = container.DataSource.GetTargets(resource, arc, true);
    while (targets.hasMoreElements()) {
      var target = targets.getNext();
      container.DataSource.Unassert(resource, arc, target, true);
    }
  }
  container.RemoveElement(resource, false);
}

// Copy a resource and all its arcs out to a new container.
function copy_resource_deeply(source_datasource, resource, dest_container) {
  var arcs = source_datasource.ArcLabelsOut(resource);
  while (arcs.hasMoreElements()) {
    var arc = arcs.getNext();
    var targets = source_datasource.GetTargets(resource, arc, true);
    while (targets.hasMoreElements()) {
      var target = targets.getNext();
      dest_container.DataSource.Assert(resource, arc, target, true);
    }
  }
  dest_container.AppendElement(resource);
}

function enable_buttons_for_other_panels()
{
  var add_button = document.getElementById('add_button');
  var preview_button = document.getElementById('preview_button');
  var all_panels = document.getElementById('other-panels');

  var sel = all_panels.view.selection;
  var num_selected = sel ? sel.count : 0;
  if (sel) {
    var ranges = sel.getRangeCount();
    for (var range = 0; range < ranges; ++range) {
      var min = {}, max = {};
      sel.getRangeAt(range, min, max);
      for (var index = min; index <= max; ++index) {
        var node = all_panels.contentView.getItemAtIndex(index);
        if (node.getAttribute('container') != 'true') {
          ++num_selected;
        }
      }
    }
  }

  if (num_selected > 0) {
    add_button.removeAttribute('disabled');
    preview_button.removeAttribute('disabled');
  } else {
    add_button.setAttribute('disabled','true');
    preview_button.setAttribute('disabled','true');
  }
}

function enable_buttons_for_current_panels() {
  var up        = document.getElementById('up');
  var down      = document.getElementById('down');
  var tree      = document.getElementById('current-panels');
  var customize = document.getElementById('customize-button');
  var remove    = document.getElementById('remove-button');

  var numSelected = tree.view.selection.count;
  var canMoveUp = false, canMoveDown = false, customizeURL = '';

  if (numSelected == 1 && tree.view.selection.isSelected(tree.currentIndex)) {
    var selectedNode = tree.view.getItemAtIndex(tree.currentIndex);
    customizeURL = selectedNode.getAttribute('customize');
    canMoveUp = selectedNode != selectedNode.parentNode.firstChild;
    canMoveDown = selectedNode != selectedNode.parentNode.lastChild;
  }

  up.disabled = !canMoveUp;
  down.disabled = !canMoveDown;
  customize.disabled = !customizeURL;
  remove.disabled = !numSelected;
}

function persist_dialog_dimensions() {
  // Stole this code from navigator.js to
  // insure the windows dimensions are saved.

  // Get the current window position/size.
  var x = window.screenX;
  var y = window.screenY;
  var h = window.outerHeight;
  var w = window.outerWidth;

  // Store these into the window attributes (for persistence).
  var win = document.getElementById( "main-window" );
  win.setAttribute( "x", x );
  win.setAttribute( "y", y );
  win.setAttribute( "height", h );
  win.setAttribute( "width", w );
}

///////////////////////////////////////////////////////////////
// Handy Debug Tools
//////////////////////////////////////////////////////////////
var debug = null;
var dump_attributes = null;
var dump_tree = null;
var _dump_tree_recur = null;

if (!CUST_DEBUG) {
  debug = function (s) {};
  dump_attributes = function (node, depth) {};
  dump_tree = function (node) {};
  _dump_tree_recur = function (node, depth, index) {};
} else {
  debug = function (s) { dump("-*- sb customize: " + s + "\n"); };

  dump_attributes = function (node, depth) {
    var attributes = node.attributes;
    var indent = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | . ";

    if (!attributes || attributes.length == 0) {
      debug(indent.substr(indent.length - depth*2) + "no attributes");
    }
    for (var ii=0; ii < attributes.length; ii++) {
      var attr = attributes.item(ii);
      debug(indent.substr(indent.length - depth*2) + attr.name +
            "=" + attr.value);
    }
  }
  dump_tree = function (node) {
    _dump_tree_recur(node, 0, 0);
  }
  _dump_tree_recur = function (node, depth, index) {
    if (!node) {
      debug("dump_tree: node is null");
    }
    var indent = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | + ";
    debug(indent.substr(indent.length - depth*2) + index +
          " " + node.nodeName);
    if (node.nodeType != Node.TEXT_NODE) {
      dump_attributes(node, depth);
    }
    var kids = node.childNodes;
    for (var ii=0; ii < kids.length; ii++) {
      _dump_tree_recur(kids[ii], depth + 1, ii);
    }
  }
}

//////////////////////////////////////////////////////////////
// Install the load/unload handlers
//////////////////////////////////////////////////////////////
addEventListener("load", sidebar_customize_init, false);
addEventListener("unload", sidebar_customize_destruct, false);
