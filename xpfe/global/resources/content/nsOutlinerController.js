/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// helper routines, for doing rdf-based cut/copy/paste/etc
// this needs to be more generic!

const nsTransferable_contractid = "@mozilla.org/widget/transferable;1";
const clipboard_contractid = "@mozilla.org/widget/clipboard;1";
const rdf_contractid = "@mozilla.org/rdf/rdf-service;1";
const supportswstring_contractid = "@mozilla.org/supports-wstring;1";
const rdfc_contractid = "@mozilla.org/rdf/container;1";

const nsISupportsWString = Components.interfaces.nsISupportsWString;
const nsIClipboard = Components.interfaces.nsIClipboard;
const nsITransferable = Components.interfaces.nsITransferable;
const nsIRDFLiteral = Components.interfaces.nsIRDFLiteral;
const nsIRDFContainer = Components.interfaces.nsIRDFContainer;

var gRDF;
var gClipboard;

function isContainer(outliner, index)
{
  return outliner.outlinerBoxObject.view.isContainer(index);
}

function isContainerOpen(outliner, index)
{
  return outliner.outlinerBoxObject.view.isContainerOpen(index);
}

function nsOutlinerController_SetTransferData(transferable, flavor, text)
{
  if (!text)
    return;
  var textData = Components.classes[supportswstring_contractid].createInstance(nsISupportsWString);
  textData.data = text;

  transferable.addDataFlavor(flavor);
  transferable.setTransferData(flavor, textData, text.length*2);
}

function nsOutlinerController_copy()
{
  var rangeCount = this.outlinerSelection.getRangeCount();
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
    this.outlinerSelection.getRangeAt(i, min, max);
    for (var k = max.value; k >= min.value; --k) {
      // If one of the selected items is
      // a container, ignore it.
      if (isContainer(this.outliner, k))
        continue;
      var pageUrl  = this.outlinerView.getCellText(k, "URL");        
      var pageName = this.outlinerView.getCellText(k, "Name");

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

function nsOutlinerController_cut()
{
  if (this.copy()) {
    this.doDelete();
    return true;            // copy succeeded, don't care if delete failed
  }
  return false;             // copy failed, so did cut
}

function nsOutlinerController_selectAll()
{
  this.outlinerSelection.selectAll();
}

function nsOutlinerController_delete()
{  
  var rangeCount = this.outlinerSelection.getRangeCount();
  if (rangeCount < 1)
    return false;      
  
  var datasource = this.outliner.database;
  var dsEnum = datasource.GetDataSources(); 
  dsEnum.getNext();
  var ds = dsEnum.getNext();

  var count = this.outlinerSelection.count;
  
  // XXX 9 is a random number, just looking for a sweetspot
  // don't want to rebuild outliner content for just a few items
  if (count > 9)
    ds.QueryInterface(Components.interfaces.nsIBrowserHistory).startBatchUpdate();

  var min = new Object(); 
  var max = new Object();
  var dirty = false;
  for (var i = rangeCount - 1; i >= 0; --i) {
    this.outlinerSelection.getRangeAt(i, min, max);
    for (var k = max.value; k >= min.value; --k) {
      if (!gRDF)
        gRDF = Components.classes[rdf_contractid].getService(Components.interfaces.nsIRDFService);

      var IDRes = this.outlinerBuilder.getResourceAtIndex(k);
      if (!IDRes)
        continue;

      var root = this.outliner.getAttribute('ref');
      var parentIDRes;
      try {
        parentIDRes = this.outlinerBuilder.getResourceAtIndex(this.outlinerView.getParentIndex(k));
      }
      catch(ex) {
        parentIDRes = gRDF.GetResource(root);
      }
      if (!parentIDRes)
        continue;
      
      // otherwise remove the parent/child assertion then
      var containment = gRDF.GetResource("http://home.netscape.com/NC-rdf#child");
      ds.QueryInterface(Components.interfaces.nsIRDFDataSource).Unassert(parentIDRes, containment, IDRes);
      dirty = true;
    }
  }

  if (dirty) {    
    if (count > 9) {
      ds.QueryInterface(Components.interfaces.nsIBrowserHistory).endBatchUpdate();
      this.outliner.builder.rebuild();
    }

    try {
      var remote = datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
      remote.Flush();
    } catch (ex) {
    }
  }
  if (max.value) {
    var newIndex = max.value - (max.value - min.value);
    if (newIndex >= this.outlinerView.rowCount)
      --newIndex;
    this.outlinerSelection.select(newIndex);
  }
  return true;
}

function nsOutlinerController(outliner)
{
  this._outliner = outliner;
  outliner.controllers.appendController(this);
}

nsOutlinerController.prototype = 
{
  _outlinerSelection: null,
  _outlinerView: null,
  _outlinerBuilder: null,
  _outlinerBoxObject: null,
  _outliner: null,
  get outliner()
  {
    return this._outliner;
  },
  get outlinerBoxObject()
  {
    if (this._outlinerBoxObject)
      return this._outlinerBoxObject;
    return this._outlinerBoxObject = this.outliner.outlinerBoxObject;
  },
  get outlinerView()
  {
    if (this._outlinerView)
      return this._outlinerView;
    return this._outlinerView = this.outliner.outlinerBoxObject.view;
  },
  get outlinerSelection()
  {
    if (this._outlinerSelection)
      return this._outlinerSelection;
    return this._outlinerSelection = this.outliner.outlinerBoxObject.view.selection;
  },
  get outlinerBuilder()
  {
    if (this._outlinerBuilder)
      return this._outlinerBuilder;
    return this._outlinerBuilder = this.outliner.builder.
                                   QueryInterface(Components.interfaces.nsIXULOutlinerBuilder);
  },
  SetTransferData : nsOutlinerController_SetTransferData,

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
        var outlinerView = this.outlinerView;
        return (outlinerView.rowCount !=  outlinerView.selection.count);
                
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
    var haveSelection = (this.outlinerSelection.count);
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
  copy: nsOutlinerController_copy,
  cut: nsOutlinerController_cut,
  doDelete: nsOutlinerController_delete,
  selectAll: nsOutlinerController_selectAll
}

