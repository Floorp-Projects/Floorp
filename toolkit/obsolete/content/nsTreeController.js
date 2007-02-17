/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blake Ross <blaker@netscape.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// helper routines, for doing rdf-based cut/copy/paste/etc
// this needs to be more generic!

const nsTransferable_contractid = "@mozilla.org/widget/transferable;1";
const clipboard_contractid = "@mozilla.org/widget/clipboard;1";
const rdf_contractid = "@mozilla.org/rdf/rdf-service;1";
const supportswstring_contractid = "@mozilla.org/supports-string;1";
const rdfc_contractid = "@mozilla.org/rdf/container;1";

const nsISupportsString = Components.interfaces.nsISupportsString;
const nsIClipboard = Components.interfaces.nsIClipboard;
const nsITransferable = Components.interfaces.nsITransferable;
const nsIRDFLiteral = Components.interfaces.nsIRDFLiteral;
const nsIRDFContainer = Components.interfaces.nsIRDFContainer;

var gRDF;
var gClipboard;

function isContainer(tree, index)
{
  return tree.treeBoxObject.view.isContainer(index);
}

function isContainerOpen(tree, index)
{
  return tree.treeBoxObject.view.isContainerOpen(index);
}

function nsTreeController_SetTransferData(transferable, flavor, text)
{
  if (!text)
    return;
  var textData = Components.classes[supportswstring_contractid].createInstance(nsISupportsString);
  textData.data = text;

  transferable.addDataFlavor(flavor);
  transferable.setTransferData(flavor, textData, text.length*2);
}

function nsTreeController_copy()
{
  var rangeCount = this.treeSelection.getRangeCount();
  if (rangeCount < 1)
    return false;
   
  // Build a url that encodes all the select nodes 
  // as well as their parent nodes
  var url = "";
  var text = "";
  var html = "";
  var min = new Object();
  var max = new Object();

  for (var i = rangeCount - 1; i >= 0; --i) {
    this.treeSelection.getRangeAt(i, min, max);
    for (var k = max.value; k >= min.value; --k) {
      // If one of the selected items is
      // a container, ignore it.
      if (isContainer(this.tree, k))
        continue;
      var col = this.tree.columns["URL"];
      var pageUrl  = this.treeView.getCellText(k, col);
      col = this.tree.columns["Name"];
      var pageName = this.treeView.getCellText(k, col);

      url += "ID:{" + pageUrl + "};";
      url += "NAME:{" + pageName + "};";

      text += pageUrl + "\r";
      html += "<a href='" + pageUrl + "'>";
      if (pageName) html += pageName;
        html += "</a><p>";
    } 
  }

  if (!url)
    return false;

  // get some useful components
  var trans = Components.classes[nsTransferable_contractid].createInstance(nsITransferable);
  
  if (!gClipboard)
    gClipboard = Components.classes[clipboard_contractid].getService(Components.interfaces.nsIClipboard);

  gClipboard.emptyClipboard(nsIClipboard.kGlobalClipboard);

  this.SetTransferData(trans, "text/unicode", text);
  this.SetTransferData(trans, "moz/bookmarkclipboarditem", url);
  this.SetTransferData(trans, "text/html", html);

  gClipboard.setData(trans, null, nsIClipboard.kGlobalClipboard);
  return true;
}

function nsTreeController_cut()
{
  if (this.copy()) {
    this.doDelete();
    return true;            // copy succeeded, don't care if delete failed
  }
  return false;             // copy failed, so did cut
}

function nsTreeController_selectAll()
{
  this.treeSelection.selectAll();
}

function nsTreeController_delete()
{  
  var rangeCount = this.treeSelection.getRangeCount();
  if (rangeCount < 1)
    return false;      
  
  var datasource = this.tree.database;
  var dsEnum = datasource.GetDataSources();
  dsEnum.getNext();
  var ds = dsEnum.getNext()
                 .QueryInterface(Components.interfaces.nsIRDFDataSource);

  var count = this.treeSelection.count;
  
  // XXX 9 is a random number, just looking for a sweetspot
  // don't want to rebuild tree content for just a few items
  if (count > 9) {
    ds.beginUpdateBatch();
  }

  var min = new Object(); 
  var max = new Object();
  var dirty = false;
  for (var i = rangeCount - 1; i >= 0; --i) {
    this.treeSelection.getRangeAt(i, min, max);
    for (var k = max.value; k >= min.value; --k) {
      if (!gRDF)
        gRDF = Components.classes[rdf_contractid].getService(Components.interfaces.nsIRDFService);

      var IDRes = this.treeBuilder.getResourceAtIndex(k);
      if (!IDRes)
        continue;

      var root = this.tree.getAttribute('ref');
      var parentIDRes;
      try {
        parentIDRes = this.treeBuilder.getResourceAtIndex(this.treeView.getParentIndex(k));
      }
      catch(ex) {
        parentIDRes = gRDF.GetResource(root);
      }
      if (!parentIDRes)
        continue;
      
      // otherwise remove the parent/child assertion then
      var containment = gRDF.GetResource("http://home.netscape.com/NC-rdf#child");
      ds.Unassert(parentIDRes, containment, IDRes);
      dirty = true;
    }
  }

  if (count > 9) {
    ds.endUpdateBatch();
  }

  if (dirty) {    
    try {
      var remote = datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
      remote.Flush();
    } catch (ex) {
    }
  }
  if (max.value != -1) {
    var newIndex = min.value;
    if (newIndex >= this.treeView.rowCount)
      --newIndex;
    if (newIndex >= 0)
      this.treeSelection.select(newIndex);
  }
  return true;
}

function nsTreeController(tree)
{
  this._tree = tree;
  tree.controllers.appendController(this);
}

nsTreeController.prototype = 
{
  _treeSelection: null,
  _treeView: null,
  _treeBuilder: null,
  _treeBoxObject: null,
  _tree: null,
  get tree()
  {
    return this._tree;
  },
  get treeBoxObject()
  {
    if (this._treeBoxObject)
      return this._treeBoxObject;
    return this._treeBoxObject = this.tree.treeBoxObject;
  },
  get treeView()
  {
    if (this._treeView)
      return this._treeView;
    return this._treeView = this.tree.treeBoxObject.view;
  },
  get treeSelection()
  {
    if (this._treeSelection)
      return this._treeSelection;
    return this._treeSelection = this.tree.treeBoxObject.view.selection;
  },
  get treeBuilder()
  {
    if (this._treeBuilder)
      return this._treeBuilder;
    return this._treeBuilder = this.tree.builder.
                                   QueryInterface(Components.interfaces.nsIXULTreeBuilder);
  },
  SetTransferData : nsTreeController_SetTransferData,

  supportsCommand: function(command)
  {
    switch(command)
    {
      case "cmd_cut":
      case "cmd_copy":
      case "cmd_delete":
      case "cmd_selectAll":
        return true;
      default:
        return false;
    }
  },

  isCommandEnabled: function(command)
  {
    var haveCommand;
    switch (command)
    {
      // commands which do not require selection              
      case "cmd_selectAll":
        var treeView = this.treeView;
        return (treeView.rowCount !=  treeView.selection.count);
                
      // these commands require selection
      case "cmd_cut":
        haveCommand = (this.cut != undefined);
        break;
      case "cmd_copy":
        haveCommand = (this.copy != undefined);
        break;
      case "cmd_delete":
        haveCommand = (this.doDelete != undefined);
        break;
    }
        
    // if we get here, then we have a command that requires selection
    var haveSelection = (this.treeSelection.count);
    return (haveCommand && haveSelection);
  },

  doCommand: function(command)
  {
    switch(command)
    {
      case "cmd_cut":
        return this.cut();
      case "cmd_copy":
        return this.copy();
      case "cmd_delete":
        return this.doDelete();        
      case "cmd_selectAll":
        return this.selectAll();
    }
    return false;
  },
  copy: nsTreeController_copy,
  cut: nsTreeController_cut,
  doDelete: nsTreeController_delete,
  selectAll: nsTreeController_selectAll
}

