/* -*- Mode: Java; tab-width: 4; insert-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

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
    current_panels.database.AddDataSource(datasource);
  }

  // Add the datasource for current list of panels. It selects panels out
  // of the other datasources.
  debug("Init: Adding current panels, "+sidebarObj.datasource_uri);
  sidebarObj.datasource = RDF.GetDataSource(sidebarObj.datasource_uri);
  current_panels.database.AddDataSource(sidebarObj.datasource);

  // Root the customize dialog at the correct place.
  debug("Init: reset all panels ref, "+allPanelsObj.resource);
  all_panels.setAttribute('ref', allPanelsObj.resource);
  debug("Init: reset current panels ref, "+sidebarObj.resource);
  current_panels.setAttribute('ref', sidebarObj.resource);
  save_initial_panels();
  enable_buttons_for_current_panels();

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

// Remember the original list of panels so that
// the save button can be enabled when something changes.
function save_initial_panels()
{
  var tree = document.getElementById('current-panels');
  if (!tree.view) {
    setTimeout(save_initial_panels, 0);
  } else {
    for (var i = 0; i < tree.view.rowCount; ++i) {
      var item = tree.contentView.getItemAtIndex(i);
      original_panels.push(item.id);
    }
  }
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
  beginUpdateBatch : function(ds) {
    //debug ("observer: beginUpdateBatch");
  },
  endUpdateBatch : function(ds) {
    //debug ("observer: endUpdateBatch");
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
  current_panels.treeBoxObject.selection.clearSelection();
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
  all_panels.treeBoxObject.selection.clearSelection();

  enable_buttons_for_current_panels();
  enable_buttons_for_other_panels();
}

// Move the selected item up the the current panels list.
function MoveUp() {
  var tree = document.getElementById('current-panels');
  if (tree.treeBoxObject.selection.count == 1) {
    var index = tree.currentIndex;
    var selected = tree.contentView.getItemAtIndex(index);
    var before = selected.previousSibling;
    if (before) {
      before.parentNode.removeChild(selected);
      before.parentNode.insertBefore(selected, before);
      tree.treeBoxObject.selection.select(index-1);
      tree.treeBoxObject.ensureRowIsVisible(index-1);
    }
  }
  enable_buttons_for_current_panels();
}

// Move the selected item down the the current panels list.
function MoveDown() {
  var tree = document.getElementById('current-panels');
  if (tree.treeBoxObject.selection.count == 1) {
    var index = tree.currentIndex;
    var selected = tree.contentView.getItemAtIndex(index);
    if (selected.nextSibling) {
      if (selected.nextSibling.nextSibling)
        selected.parentNode.insertBefore(selected, selected.nextSibling.nextSibling);
      else
        selected.parentNode.appendChild(selected);
      tree.treeBoxObject.selection.select(index+1);
      tree.treeBoxObject.ensureRowIsVisible(index+1);
    }
  }
  enable_buttons_for_current_panels();
}

function PreviewPanel()
{
  var tree = document.getElementById('other-panels');
  var database = tree.database;
  var sel = tree.treeBoxObject.selection;
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
  var added = 0;
  
  var tree = document.getElementById('other-panels');
  var database = tree.database;
  var sel = tree.treeBoxObject.selection;
  var ranges = sel.getRangeCount();
  for (var range = 0; range < ranges; ++range) {
    var min = {}, max = {};
    sel.getRangeAt(range, min, max);
    for (var index = min.value; index <= max.value; ++index) {
      var item = tree.contentView.getItemAtIndex(index);
      if (item.getAttribute("container") != "true") {
        var res = RDF.GetResource(item.id);
        // Add the panel to the current list.
        add_node_to_current_list(database, res);
        ++added;
      }
    }
  }

  if (added) {
    // Remove the selection in the other list.
    // Selection will move to "current" list.
    var all_panels = document.getElementById('other-panels');
    all_panels.treeBoxObject.selection.clearSelection();

    enable_buttons_for_current_panels();
    enable_buttons_for_other_panels();
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

  var tree = document.getElementById('current-panels');
  var tree_root = tree.getElementsByTagName("treechildren")[1];
  
  // Check to see if the panel already exists...
  var i = 0;
  for (var treeitem = tree_root.firstChild; treeitem; treeitem = treeitem.nextSibling) {
    if (treeitem.id == service.Value) {
      // The panel is already in the current panel list.
      // Avoid adding it twice.
      tree.treeBoxObject.selection.select(i);
      tree.treeBoxObject.ensureRowIsVisible(i);
      return;
    }
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
  
  // Select is only if the caller wants to.
  var newIndex = tree_root.getElementsByTagName("treeitem").length - 1;
  tree.treeBoxObject.selection.select(newIndex);
  tree.treeBoxObject.ensureRowIsVisible(newIndex);
}

// Remove the selected panel(s) from the current list tree.
function RemovePanel()
{
  var tree = document.getElementById('current-panels');
  var sel = tree.treeBoxObject.selection;
  
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
    
  enable_buttons_for_current_panels();
}

// Bring up a new window with the customize url
// for an individual panel.
function CustomizePanel()
{
  var tree  = document.getElementById('current-panels');
  var numSelected = tree.treeBoxObject.selection ? tree.treeBoxObject.selection.count : 0;

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

var panel;

// Serialize the new list of panels.
function Save()
{
  persist_dialog_dimensions();

  var all_panels = document.getElementById('other-panels');
  var current_panels = document.getElementById('current-panels');

  // See if list membership has changed
  var root = current_panels.getElementsByTagName("treechildren")[1];
  var panels = root.childNodes;
  var list_unchanged = (panels.length == original_panels.length);
  for (var i = 0; i < panels.length && list_unchanged; i++) {
    if (original_panels[i] != panels[i].id)
      list_unchanged = false;
  }
  if (list_unchanged)
    return;

  // Iterate through the 'current-panels' tree to collect the panels
  // that the user has chosen. We need to do this _before_ we remove
  // the panels from the datasource, because the act of removing them
  // from the datasource will change the tree!
  panels = [];
  for (var node = root.firstChild; node != null; node = node.nextSibling)
    panels.push(node.id);

  // Cut off the connection between the dialog and the datasource.
  // The dialog is closed at the end of this function.
  // Without this, the dialog tries to update as it is being destroyed.
  current_panels.setAttribute('ref', 'rdf:null');

  // Create a "container" wrapper around the current panels to
  // manipulate the RDF:Seq more easily.
  var panel_list = sidebarObj.datasource.GetTarget(RDF.GetResource(sidebarObj.resource), RDF.GetResource(NC+"panel-list"), true);
  if (panel_list) {
    panel_list.QueryInterface(Components.interfaces.nsIRDFResource);
  } else {
    // Datasource is busted. Start over.
    debug("Sidebar datasource is busted\n");
  }

  var container = Components.classes["@mozilla.org/rdf/container;1"].createInstance();
  container = container.QueryInterface(Components.interfaces.nsIRDFContainer);
  container.Init(sidebarObj.datasource, panel_list);

  // Remove all the current panels from the datasource.
  var have_panel_attributes = [];
  current_panels = container.GetElements();
  while (current_panels.hasMoreElements()) {
    panel = current_panels.getNext();
    id = panel.QueryInterface(Components.interfaces.nsIRDFResource).Value;
    if (has_element(panels, id)) {
      // This panel will remain in the sidebar.
      // Remove the resource, but keep all the other attributes.
      // Removing it will allow it to be added in the correct order.
      // Saving the attributes will preserve things such as the exclude state.
      have_panel_attributes[id] = true;
      container.RemoveElement(panel, false);
    } else {
      // Kiss it goodbye.
      delete_resource_deeply(container, panel);
    }
  }

  // Add the new list of panels
  for (var ii = 0; ii < panels.length; ++ii) {
    var id = panels[ii];
    var resource = RDF.GetResource(id);
    if (id in have_panel_attributes && have_panel_attributes[id]) {
      container.AppendElement(resource);
    } else {
      copy_resource_deeply(all_panels.database, resource, container);
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
  container.RemoveElement(panel, false);
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

  var sel = all_panels.treeBoxObject.selection;
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

  var numSelected = tree.treeBoxObject.selection ? tree.treeBoxObject.selection.count : 0;

  var noneSelected, isFirst, isLast, selectedNode

  if (numSelected > 0) {
    selectedNode = tree.contentView.getItemAtIndex(tree.currentIndex);
    isFirst = selectedNode == selectedNode.parentNode.firstChild
    isLast  = selectedNode == selectedNode.parentNode.lastChild
  }

  // up /\ button
  if (numSelected != 1 || isFirst) {
    up.setAttribute('disabled', 'true');
  } else {
    up.removeAttribute('disabled');
  }
  // down \/ button
  if (numSelected != 1 || isLast) {
    down.setAttribute('disabled', 'true');
  } else {
    down.removeAttribute('disabled');
  }
  // "Customize..." button
  var customizeURL = null;
  if (numSelected == 1) {
    customizeURL = selectedNode.getAttribute('customize');
  }
  if (customizeURL == null || customizeURL == '') {
    customize.setAttribute('disabled','true');
  } else {
    customize.removeAttribute('disabled');
  }
  // "Remove" button
  if (numSelected == 0) {
    remove.setAttribute('disabled','true');
  } else {
    remove.removeAttribute('disabled');
  }
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
    if (node.nodeName != "#text") {
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
