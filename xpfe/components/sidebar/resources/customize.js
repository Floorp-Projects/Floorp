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

// the rdf service
var RDF = 'component://netscape/rdf/rdf-service'
RDF = Components.classes[RDF].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

var NC = "http://home.netscape.com/NC-rdf#";

var sidebarObj = new Object;
var original_panels = new Array();

function debug(msg)
{
  //dump(msg+"\n");
}

function Init()
{
  doSetOKCancel(Save);
  
  var all_panels_datasources = window.arguments[0];
  var all_panels_resource    = window.arguments[1];
  sidebarObj.datasource_uri     = window.arguments[2];
  sidebarObj.resource           = window.arguments[3];

  debug("Init: all panels datasources = " + all_panels_datasources);
  debug("Init: all panels resource    = " + all_panels_resource);
  debug("Init: sidebarObj.datasource_uri = " + sidebarObj.datasource_uri);
  debug("Init: sidebarObj.resource       = " + sidebarObj.resource);

  var all_panels   = document.getElementById('other-panels');
  var current_panels = document.getElementById('current-panels');

  all_panels_datasources = all_panels_datasources.replace(/^\s+/,'');
  all_panels_datasources = all_panels_datasources.replace(/\s+$/,'');
  all_panels_datasources = all_panels_datasources.split(/\s+/);
  for (var ii = 0; ii < all_panels_datasources.length; ii++) {
    debug("Init: Adding "+all_panels_datasources[ii]);

    // This will load the datasource, if it isn't already.
    var datasource = RDF.GetDataSource(all_panels_datasources[ii]);
    all_panels.database.AddDataSource(datasource);
    current_panels.database.AddDataSource(datasource);
  }
  
  // Add the datasource for current list of panels. It selects panels out
  // of the other datasources.
  debug("Init: Adding current panels, "+sidebarObj.datasource_uri);
  sidebarObj.datasource = RDF.GetDataSource(sidebarObj.datasource_uri);
  current_panels.database.AddDataSource(sidebarObj.datasource);

  // Root the customize dialog at the correct place.
  debug("Init: reset all panels ref, "+all_panels_resource);
  all_panels.setAttribute('ref', all_panels_resource);
  debug("Init: reset current panels ref, "+sidebarObj.resource);
  current_panels.setAttribute('ref', sidebarObj.resource);

  save_initial_panels();
  enable_buttons_for_current_panels();
}

// Remember the original list of panels so that
// the save button can be enabled when something changes.
function save_initial_panels()
{
  var root = document.getElementById('current-panels-root');
  for (var node = root.firstChild; node != null; node = node.nextSibling) {
    original_panels[original_panels.length] = node.getAttribute('id');
  }
}

function get_attr(registry,service,attr_name) {
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
  // Remove the selection in the "current" panels list
  var current_panels = document.getElementById('current-panels');
  current_panels.clearItemSelection();
  enable_buttons_for_current_panels();

  if (target.getAttribute('container') == 'true') {
    if (target.getAttribute('open') == 'true') {
      target.removeAttribute('open');
    } else {
      target.setAttribute('open','true');
    }
  } else {
    link = target.getAttribute('link');
    if (link && link != '') {
      debug("Has remote datasource: "+link);
      add_datasource_to_other_panels(link);
      target.setAttribute('container', 'true');
      target.setAttribute('open', 'true');
    }
  }
  enable_buttons_for_other_panels();
}

function add_datasource_to_other_panels(link) {
  // Convert the |link| attribute into a URL
  var url = document.location;
  debug("Current URL:  " +url);
  debug("Current link: " +link);

  uri = Components.classes['component://netscape/network/standard-url'].createInstance();
  uri = uri.QueryInterface(Components.interfaces.nsIURI);
  uri.spec = url;
  uri = uri.resolve(link);

  debug("New URL:      " +uri);
  
  // Add the datasource to the tree
  var all_panels = document.getElementById('other-panels');
  all_panels.database.AddDataSource(RDF.GetDataSource(uri));

  // XXX This is a hack to force re-display
  all_panels.setAttribute('ref', 'urn:blah:main-root');
}

// Handle a selection change in the current panels.
function SelectChangeForCurrentPanels() {
  // Remove the selection in the available panels list
  var all_panels = document.getElementById('other-panels');
  all_panels.clearItemSelection();

  enable_buttons_for_current_panels();
  enable_buttons_for_other_panels();
}

// Move the selected item up the the current panels list.
function MoveUp() {
  var tree = document.getElementById('current-panels'); 
  if (tree.selectedItems.length == 1) {
    var selected = tree.selectedItems[0];
    var before = selected.previousSibling
    if (before) {
      before.parentNode.insertBefore(selected, before);
      tree.selectItem(selected);
      tree.ensureElementIsVisible(selected);
    }
  }
  enable_buttons_for_current_panels();
}
   
// Move the selected item down the the current panels list.
function MoveDown() {
  var tree = document.getElementById('current-panels');
  if (tree.selectedItems.length == 1) {
    var selected = tree.selectedItems[0];
    if (selected.nextSibling) {
      if (selected.nextSibling.nextSibling) {
        selected.parentNode.insertBefore(selected, selected.nextSibling.nextSibling);
      }
      else {
        selected.parentNode.appendChild(selected);
      }
      tree.selectItem(selected);
    }
  }
  enable_buttons_for_current_panels();
}

function PreviewPanel() 
{
  var tree = document.getElementById('other-panels');
  var database = tree.database;
  var select_list = tree.selectedItems
  for (var nodeIndex=0; nodeIndex<select_list.length; nodeIndex++) {
    var node = select_list[nodeIndex];
    if (!node)    break;
    // Skip folders
    if (node.getAttribute('container') == 'true') {
      continue;
    }
    var id = node.getAttribute("id");
    if (!id)      break;
    var rdfNode = RDF.GetResource(id);
    if (!rdfNode) break;

    var preview_name = get_attr(database, rdfNode, 'title');
    var preview_URL  = get_attr(database, rdfNode, 'content');
    if (!preview_URL || !preview_name) break;

    var preview = window.open("chrome://communicator/content/sidebar/preview.xul",
                              "_blank", "chrome,resizable");
    preview.panel_name = preview_name;
    preview.panel_URL = preview_URL;
  }
}

// Add the selected panel(s).
function AddPanel() 
{
  var tree = document.getElementById('other-panels');
  var database = tree.database;
  var select_list = tree.selectedItems
  for (var nodeIndex=0; nodeIndex<select_list.length; nodeIndex++) {
    var node = select_list[nodeIndex];
    if (!node)    break;
    // Skip folders.
    if (node.getAttribute('container') == 'true') {
      continue;
    }
    var id = node.getAttribute("id");
    // No id? Sorry. Only nodes with id's can get
    // in the current panel list.
    if (!id)      break;
    var rdfNode = RDF.GetResource(id);
    // You need an rdf node too. Sorry, those are the rules.
    if (!rdfNode) break;

    // Add the panel to the current list.
    add_node_to_current_list(database, rdfNode);
  }

  // Remove the selection in the other list.
  // Selection will move to "current" list.
  var all_panels = document.getElementById('other-panels');
  all_panels.clearItemSelection();

  enable_buttons_for_current_panels();
  enable_buttons_for_other_panels();
}

// Copy a panel node into a database such as the current panel list.
function add_node_to_current_list(registry, service)
{
  debug("Adding "+service.Value);

  // Copy out the attributes we want
  var option_title     = get_attr(registry, service, 'title');
  var option_customize = get_attr(registry, service, 'customize');
  var option_content   = get_attr(registry, service, 'content');

  var tree     = document.getElementById('current-panels'); 
  var treeroot = document.getElementById('current-panels-root'); 

  // Check to see if the panel already exists...
  for (var ii = treeroot.firstChild; ii != null; ii = ii.nextSibling) {
    if (ii.getAttribute('id') == service.Value) {
      // The panel is already in the current panel list.
      // Avoid adding it twice.
      tree.selectItem(ii);
      tree.ensureElementIsVisible(ii);
      return;
    }
  }

  // Create a treerow for the new panel
  var item = document.createElement('treeitem');
  var row  = document.createElement('treerow');
  var cell = document.createElement('treecell');
  
  // Copy over the attributes
  item.setAttribute('id', service.Value);
  item.setAttribute('customize', option_customize);
  item.setAttribute('content', option_content);
  cell.setAttribute('value', option_title);
 
  // Add it to the current panels tree
  item.appendChild(row);
  row.appendChild(cell);
  treeroot.appendChild(item);

  // Select is only if the caller wants to.
  tree.selectItem(item);
  tree.ensureElementIsVisible(item);
}

// Remove the selected panel(s) from the current list tree.
function RemovePanel()
{
  var tree = document.getElementById('current-panels');

  var nextNode = null;
  var numSelected = tree.selectedItems.length
  while (tree.selectedItems.length > 0) {
    var selectedNode = tree.selectedItems[0]
    nextNode = selectedNode.nextSibling;
    if (!nextNode) {
      nextNode = selectedNode.previousSibling;
    }
    selectedNode.parentNode.removeChild(selectedNode)
  } 
  
  if (nextNode) {
    tree.selectItem(nextNode)
  }
  enable_buttons_for_current_panels();
}

// Bring up a new window with the customize url 
// for an individual panel.
function CustomizePanel() 
{
  var tree  = document.getElementById('current-panels');
  var numSelected = tree.selectedItems.length;

  if (numSelected == 1) {
    var selectedNode  = tree.selectedItems[0];
    var customize_url = selectedNode.getAttribute('customize');

    debug("url   = " + customize_url);

    if (!customize_url) return;

    window.openDialog('chrome://communicator/content/sidebar/customize-panel.xul',
                      '_blank','chrome,resizable,width=690,height=600',
                      customize_url);
  }
}

// Serialize the new list of panels.
function Save()
{
  var all_panels = document.getElementById('other-panels');
  var current_panels = document.getElementById('current-panels');

  // See if list membership has changed
  var root = document.getElementById('current-panels-root');
  var panels = root.childNodes;  
  var list_unchanged = (panels.length == original_panels.length);
  for (var ii = 0; ii < panels.length && list_unchanged; ii++) {
    if (original_panels[ii] != panels.item(ii).getAttribute('id')) {
      list_unchanged = false;
    }
  }
  if (list_unchanged) {
    window.close();
    return;
  }

  // Iterate through the 'current-panels' tree to collect the panels
  // that the user has chosen. We need to do this _before_ we remove
  // the panels from the datasource, because the act of removing them
  // from the datasource will change the tree!
  var panels = new Array();
  var root = document.getElementById('current-panels-root');
  for (var node = root.firstChild; node != null; node = node.nextSibling) {
    panels[panels.length] = node.getAttribute('id');
  }

  // Cut off the connection between the dialog and the datasource.
  // The dialog is closed at the end of this function.
  // Without this, the dialog tries to update as it is being destroyed.
  current_panels.setAttribute('ref', 'rdf:null');

  start_batch(sidebarObj.datasource, sidebarObj.resource);

  // Create a "container" wrapper around the current panels to
  // manipulate the RDF:Seq more easily.
  var container = Components.classes["component://netscape/rdf/container"].createInstance();
  container = container.QueryInterface(Components.interfaces.nsIRDFContainer);
  container.Init(sidebarObj.datasource, RDF.GetResource(sidebarObj.resource));

  // Remove all the current panels from the datasource.
  var current_panels = container.GetElements();
  while (current_panels.HasMoreElements()) {
    panel = current_panels.GetNext();
    id = panel.QueryInterface(Components.interfaces.nsIRDFResource).Value;

    // If this panel is not in the new list,
    // or, if description of this panel is in the all panels list,
    // then, remove all assertions for the panel to avoid leaving
    // unneeded assertions behind and to avoid multiply asserted
    // attributes, respectively.
    if (!has_element(panels, id) || 
        has_targets(all_panels.database, panel)) {
      delete_resource_deeply(container, panel);
    } else {
      container.RemoveElement(panel, false);
    }
  }

  // Add the new list of panels
  for (var ii = 0; ii < panels.length; ++ii) {
    copy_resource_deeply(all_panels.database, RDF.GetResource(panels[ii]),
                         container);
  }

  end_batch(sidebarObj.datasource, sidebarObj.resource);

  // Write the modified panels out.
  sidebarObj.datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush();

  window.close();
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
  return arcs.HasMoreElements();
}

// Mark the beginning of a batch by asserting inbatch="true"
// into the datasource. The observer in sidebarOverlay.js uses 
// this to remember the panel selection.
function start_batch(datasource, resource) {
  datasource.Assert(RDF.GetResource(resource),
                    RDF.GetResource(NC + "inbatch"),
                    RDF.GetLiteral("true"),
                    true);
}

// Mark the end of a batch by unasserting 'inbatch' on the datasource.
function end_batch(datasource, resource) {
  datasource.Unassert(RDF.GetResource(resource),
                      RDF.GetResource(NC + "inbatch"),
                      RDF.GetLiteral("true"));
}

// Remove a resource and all the arcs out from it.
function delete_resource_deeply(container, resource) {
  var arcs = container.DataSource.ArcLabelsOut(resource);
  while (arcs.HasMoreElements()) {
    var arc = arcs.GetNext();
    var targets = container.DataSource.GetTargets(resource, arc, true);
    while (targets.HasMoreElements()) {
      var target = targets.GetNext();
      container.DataSource.Unassert(resource, arc, target, true);
    }
  }
  container.RemoveElement(panel, false);
}

// Copy a resource and all its arcs out to a new container.
function copy_resource_deeply(source_datasource, resource, dest_container) {
  var arcs = source_datasource.ArcLabelsOut(resource);
  while (arcs.HasMoreElements()) {
    var arc = arcs.GetNext();
    var targets = source_datasource.GetTargets(resource, arc, true);
    while (targets.HasMoreElements()) {
      var target = targets.GetNext();
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

  var num_selected = 0;
  // Only count non-folders as selected for button enabling
  for (var ii=0; ii<all_panels.selectedItems.length; ii++) {
    var node = all_panels.selectedItems[ii];
    if (node.getAttribute('container') != 'true') {
      num_selected++;
    }
  }
  if (num_selected > 0) {
    add_button.removeAttribute('disabled');
    preview_button.removeAttribute('disabled');
  }
  else {
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

  var numSelected = tree.selectedItems.length;

  var noneSelected, isFirst, isLast, selectedNode

  if (numSelected > 0) {
    selectedNode = tree.selectedItems[0]
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

function dump_attributes(node) {
  var attributes = node.attributes

  if (!attributes || attributes.length == 0) {
    debug("no attributes\n")
  }
  for (var ii=0; ii < attributes.length; ii++) {
    var attr = attributes.item(ii)
    debug("attr "+ii+": "+ attr.name +"="+attr.value)
  }
}

