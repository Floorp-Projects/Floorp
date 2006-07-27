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

var sidebar = new Object;
var original_panels = new Array();

function debug(msg)
{
  //dump(msg+"\n");
}

function Init()
{
  var all_panels_datasources = window.arguments[0];
  var all_panels_resource    = window.arguments[1];
  sidebar.datasource_uri     = window.arguments[2];
  sidebar.resource           = window.arguments[3];

  debug("all panels datasources = " + all_panels_datasources);
  debug("all panels resource    = " + all_panels_resource);
  debug("sidebar.datasource_uri = " + sidebar.datasource_uri);
  debug("sidebar.resource       = " + sidebar.resource);

  var all_panels   = document.getElementById('other-panels');
  var current_panels = document.getElementById('current-panels');

  all_panels_datasources = all_panels_datasources.split(/\s+/);
  for (var ii = 0; ii < all_panels_datasources.length; ii++) {
    debug("Adding "+all_panels_datasources[ii]);

    // This will load the datasource, if it isn't already.
    var datasource = RDF.GetDataSource(all_panels_datasources[ii]);
    all_panels.database.AddDataSource(datasource);
    current_panels.database.AddDataSource(datasource);
  }

  // Add the datasource for current list of panels. It selects panels out
  // of the other datasources.
  sidebar.datasource = RDF.GetDataSource(sidebar.datasource_uri);
  current_panels.database.AddDataSource(sidebar.datasource);

  // Root the customize dialog at the correct place.
  all_panels.setAttribute('ref', all_panels_resource);
  current_panels.setAttribute('ref', sidebar.resource);

  saveInitialPanels();
  enableButtonsForCurrentPanels();
}

// Remember the original list of panels so that
// the save button can be enabled when something changes.
function saveInitialPanels()
{
  var root = document.getElementById('current-panels-root');
  for (var node = root.firstChild; node != null; node = node.nextSibling) {
    original_panels[original_panels.length] = node.getAttribute('id');
  }
}

function getAttr(registry,service,attr_name) {
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
 enableButtonsForCurrentPanels();

 if (target.getAttribute('container') == 'true') {
   if (target.getAttribute('open') == 'true') {
     target.removeAttribute('open');
   } else {
     target.setAttribute('open','true');
   }
   return;
 }
 enableButtonsForOtherPanels();
}

// Handle a selection change in the current panels.
function SelectChangeForCurrentPanels() {
  // Remove the selection in the available panels list
  var all_panels = document.getElementById('other-panels');
  all_panels.clearItemSelection();

  enableButtonsForCurrentPanels();
  enableButtonsForOtherPanels();
}

// Move the selected item up the the current panels list.
function MoveUp() {
  var tree = document.getElementById('current-panels'); 
  if (tree.selectedItems.length == 1) {
    var selected = tree.selectedItems[0];
    if (selected.previousSibling) {
      selected.parentNode.insertBefore(selected, selected.previousSibling);
      tree.selectItem(selected);
    }
  }
  enableButtonsForCurrentPanels();
  enableSave();
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
  enableButtonsForCurrentPanels();
  enableSave();
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
    if (node.getAttribute('folder') == 'true') {
      continue;
    }
    var id = node.getAttribute("id");
    if (!id)      break;
    var rdfNode = RDF.GetResource(id);
    if (!rdfNode) break;

    var preview_name = getAttr(database, rdfNode, 'title');
    var preview_URL  = getAttr(database, rdfNode, 'content');
    if (!preview_URL || !preview_name) break;

    var preview = window.open("chrome://sidebar/content/preview.xul",
                              "_blank", "chrome");
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
  var isFirstAddition = true;
  for (var nodeIndex=0; nodeIndex<select_list.length; nodeIndex++) {
    var node = select_list[nodeIndex];
    if (!node)    break;
    // Skip folders.
    if (node.getAttribute('folder') == 'true') {
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
    // Pass "isFirstAddition" because only the first panel in a 
    // group of additions will get selected.
    addNodeToCurrentList(database, rdfNode, isFirstAddition);
    isFirstAddition = false;
  }

  // Remove the selection in the other list.
  // Selection will move to "current" list.
  var all_panels = document.getElementById('other-panels');
  all_panels.clearItemSelection();

  enableButtonsForCurrentPanels();
  enableButtonsForOtherPanels();
  enableSave();
}

// Copy a panel node into a database such as the current panel list.
function addNodeToCurrentList(registry, service, selectIt)
{
  debug("Adding "+service.Value);

  // Copy out the attributes we want
  var option_title     = getAttr(registry, service, 'title');
  var option_customize = getAttr(registry, service, 'customize');
  var option_content   = getAttr(registry, service, 'content');

  var tree     = document.getElementById('current-panels'); 
  var treeroot = document.getElementById('current-panels-root'); 

  // Check to see if the panel already exists...
  for (var ii = treeroot.firstChild; ii != null; ii = ii.nextSibling) {
    if (ii.getAttribute('id') == service.Value) {
      // The panel is already in the current panel list.
      // Avoid adding it twice.
      tree.selectItem(ii);
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
  if (selectIt) {
    debug("Selecting new item");
    tree.selectItem(item)
  }
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
  enableButtonsForCurrentPanels();
  enableSave();
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

    var customize = window.open(customize_url, "_blank", "resizable");
  }
  enableSave();
}

// Serialize the new list of panels.
function Save()
{
  // Iterate through the 'current-panels' tree to collect the panels
  // that the user has chosen. We need to do this _before_ we remove
  // the panels from the datasource, because the act of removing them
  // from the datasource will change the tree!
  var panels = new Array();
  var root = document.getElementById('current-panels-root');
  for (var node = root.firstChild; node != null; node = node.nextSibling) {
    panels[panels.length] = node.getAttribute('id');
  }

  start_batch(sidebar.datasource, sidebar.resource);

  // Now remove all the current panels from the datasource.

  // Create a "container" wrapper around the "urn:sidebar:current-panel-list"
  // object. This makes it easier to manipulate the RDF:Seq correctly.
  var container = Components.classes["component://netscape/rdf/container"].createInstance();
  container = container.QueryInterface(Components.interfaces.nsIRDFContainer);
  container.Init(sidebar.datasource, RDF.GetResource(sidebar.resource));

  for (var ii = container.GetCount(); ii >= 1; --ii) {
    debug('removing panel ' + ii);
    container.RemoveElementAt(ii, true);
  }

  // Now iterate through the panels, and re-add them to the datasource
  for (var ii = 0; ii < panels.length; ++ii) {
    debug('adding ' + panels[ii]);
    container.AppendElement(RDF.GetResource(panels[ii]));
  }

  end_batch(sidebar.datasource, sidebar.resource);

  // Write the modified panels out.
  sidebar.datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush();

  window.close();
}

// Mark the beginning of a batch by
// asserting inbatch="true" into the datasource.
// The observer in sidebarOverlay.js uses this to
// remember the panel selection.
function start_batch(datasource, resource) {
  datasource.Assert(RDF.GetResource(resource),
                    RDF.GetResource(NC + "inbatch"),
                    RDF.GetLiteral("true"),
                    true);
}

// Mark the end of a batch by
// unasserting 'inbatch' on the datasource.
function end_batch(datasource, resource) {
  datasource.Unassert(RDF.GetResource(resource),
                      RDF.GetResource(NC + "inbatch"),
                      RDF.GetLiteral("true"));
}

function enableButtonsForOtherPanels()
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
    add_button.setAttribute('disabled','');
    preview_button.setAttribute('disabled','');
  }
  else {
    add_button.setAttribute('disabled','true');
    preview_button.setAttribute('disabled','true');
  }
}

function enableButtonsForCurrentPanels() {
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
    up.setAttribute('disabled', '');
  }
  // down \/ button
  if (numSelected != 1 || isLast) {
    down.setAttribute('disabled', 'true');
  } else {
    down.setAttribute('disabled', '');
  }
  // "Customize..." button
  var customizeURL = null;
  if (numSelected == 1) {
    customizeURL = selectedNode.getAttribute('customize');
  }
  if (customizeURL == null || customizeURL == '') {
    customize.setAttribute('disabled','true');
  } else {
    customize.setAttribute('disabled','');
  }
  // "Remove" button
  if (numSelected == 0) {
    remove.setAttribute('disabled','true');
  } else {
    remove.setAttribute('disabled','');
  }
}

function enableSave() {
  debug("in enableSave()");
  var root = document.getElementById('current-panels-root');
  var panels = root.childNodes;  
  var list_unchanged = (panels.length == original_panels.length);

  debug ("panels.length="+panels.length);
  debug ("orig.length="+original_panels.length);

  for (var ii = 0; ii < panels.length && list_unchanged; ii++) {
    //node = root.firstChild; node != null; node = node.nextSibling) {
    debug("orig="+original_panels[ii]);
    debug(" new="+panels.item(ii).getAttribute('id'));
    debug("orig.length="+original_panels.length+" ii="+ii);
    if (original_panels[ii] != panels.item(ii).getAttribute('id')) {
      list_unchanged = false;
    }
  }
  var save_button = document.getElementById('save_button');
  if (list_unchanged) {
    save_button.setAttribute('disabled','true');  
  } else {
    save_button.setAttribute('disabled','');  
  }
}

function dumpAttributes(node) {
  var attributes = node.attributes

  if (!attributes || attributes.length == 0) {
    debug("no attributes\n")
  }
  for (var ii=0; ii < attributes.length; ii++) {
    var attr = attributes.item(ii)
    debug("attr "+ii+": "+ attr.name +"="+attr.value)
  }
}

