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

function debug(msg)
{
  //dump(msg);
}

function Init()
{
  sidebar.db       = window.arguments[0];
  sidebar.resource = window.arguments[1];
  debug("sidebar.db = " + sidebar.db + "\n");
  debug("sidebar.resource = " + sidebar.resource + "\n");

  // This will load the datasource, if it isn't already.
  sidebar.datasource = RDF.GetDataSource(sidebar.db);

  // Add the necessary datasources to the select list
  var select_list = document.getElementById('selected-panels');
  select_list.database.AddDataSource(sidebar.datasource);

  // Root the customize dialog at the correct place.
  select_list.setAttribute('ref', sidebar.resource);

  enableButtons();
}

function addOption(registry, service, selectIt)
{
  dump("Adding "+service.Value+"\n");
  var option_title     = getAttr(registry, service, 'title');
  var option_customize = getAttr(registry, service, 'customize');
  var option_content   = getAttr(registry, service, 'content');

  var tree = document.getElementById('selected-panels'); 
  var treeroot = document.getElementById('selected-panels-root'); 

  // Check to see if the panel already exists...
  for (var ii = treeroot.firstChild; ii != null; ii = ii.nextSibling) {
      if (ii.getAttribute('id') == service.Value) {
      // we already had the panel installed
      tree.selectItem(ii);
      return;
      }
  }


  var item = document.createElement('treeitem');
  var row  = document.createElement('treerow');
  var cell = document.createElement('treecell');
  
  item.setAttribute('id', service.Value);
  item.setAttribute('customize', option_customize);
  item.setAttribute('content', option_content);
  cell.setAttribute('value', option_title);
 
  item.appendChild(row);
  row.appendChild(cell);
  treeroot.appendChild(item);

  if (selectIt) {
    dump("Selecting new item\n");
    tree.selectItem(item)
  }
}

function createOptionTitle(titletext)
{
  var title = document.createElement('html:option');
  var textOption = document.createTextNode(titletext);
  title.appendChild(textOption);

  return textOption;
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

function selectChange() {
  enableButtons();
}

function moveUp() {
  var tree = document.getElementById('selected-panels'); 
  if (tree.selectedItems.length == 1) {
    var selected = tree.selectedItems[0];
    if (selected.previousSibling) {
      selected.parentNode.insertBefore(selected, selected.previousSibling);
      tree.selectItem(selected);
    }
  }
  enableButtons();
}
   
function moveDown() {
  var tree = document.getElementById('selected-panels');
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
  enableButtons();
}

function enableButtons() {
  var up        = document.getElementById('up');
  var down      = document.getElementById('down');
  var tree      = document.getElementById('selected-panels');
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
  if (selectedNode) {
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

function CustomizePanel() 
{
  var tree  = document.getElementById('selected-panels');
  var index = tree.selectedIndex;

  if (index != -1) {
    var title         = tree.childNodes.item(index).getAttribute('title');
    var customize_URL = tree.childNodes.item(index).getAttribute('customize');

    if (!title || !customize_URL) return;

    var customize = window.open("chrome://sidebar/content/customize-panel.xul",
                                "_blank", "chrome");

    customize.panel_name          = title;
    customize.panel_customize_URL = customize_URL;
  }
  enableSave();
}

function RemovePanel()
{
  var tree = document.getElementById('selected-panels');

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
  enableButtons();
  enableSave();
}

// Note that there is a bug with resource: URLs right now.
var FileURL = "file:////u/slamm/tt/sidebar-browser.rdf";

// var the "NC" namespace. Used to construct resources
function Save()
{
  // Iterate through the 'selected-panels' tree to collect the panels
  // that the user has chosen. We need to do this _before_ we remove
  // the panels from the datasource, because the act of removing them
  // from the datasource will change the tree!
  var panels = new Array();
  var root = document.getElementById('selected-panels-root');
  for (var node = root.firstChild; node != null; node = node.nextSibling) {
    panels[panels.length] = node.getAttribute('id');
  }

  // Now remove all the current panels from the datasource.

  // Create a "container" wrapper around the "NC:BrowserSidebarRoot"
  // object. This makes it easier to manipulate the RDF:Seq correctly.
  var container = Components.classes["component://netscape/rdf/container"].createInstance();
  container = container.QueryInterface(Components.interfaces.nsIRDFContainer);
  container.Init(sidebar.datasource, RDF.GetResource(sidebar.resource));

  for (var ii = container.GetCount(); ii >= 1; --ii) {
    dump('removing panel ' + ii + '\n');
    container.RemoveElementAt(ii, true);
  }

  // Now iterate through the panels, and re-add them to the datasource
  for (var ii = 0; ii < panels.length; ++ii) {
    debug('adding ' + panels[ii] + '\n');
    container.AppendElement(RDF.GetResource(panels[ii]));
  }

  // Write the modified panels out.
  sidebar.datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush();

  window.close();
}

function otherPanelSelected()
{ 
    var add_button = document.getElementById('add_button');
    var preview_button = document.getElementById('preview_button');
    var other_panels = document.getElementById('other-panels');

    if (other_panels.selectedItems.length > 0) {
        add_button.setAttribute('disabled','');
        preview_button.setAttribute('disabled','');
    }
    else {
        add_button.setAttribute('disabled','true');
        preview_button.setAttribute('disabled','true');
    }
}

function AddPanel() 
{
  var tree = document.getElementById('other-panels');
  var database = tree.database;
  var select_list = tree.selectedItems
  var isFirstAddition = true;
  for (var nodeIndex=0; nodeIndex<select_list.length; nodeIndex++) {
    var node = select_list[nodeIndex];
    if (!node)    break;
    var id = node.getAttribute("id");
    if (!id)      break;
    var rdfNode = RDF.GetResource(id);
    if (!rdfNode) break;
    addOption(database, rdfNode, isFirstAddition);
    isFirstAddition = false;
  }
  enableButtons();
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

function enableSave() {
  var save_button = document.getElementById('save_button');
  save_button.setAttribute('disabled','');  
}
