# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is The Extension Manager.
# 
# The Initial Developer of the Original Code is Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@bengoodger.com>
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

 
///////////////////////////////////////////////////////////////////////////////
// Globals

const kObserverServiceProgID = "@mozilla.org/observer-service;1";

var gExtensionManager = null;
var gDownloadListener = null;
var gExtensionssView  = null;
var gWindowState      = "";

///////////////////////////////////////////////////////////////////////////////
// Utility Functions 
function setRDFProperty(aID, aProperty, aValue)
{
  var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

  var db = gDownloadManager.datasource;
  var propertyArc = rdf.GetResource(NC_NS + aProperty);
  
  var res = rdf.GetResource(aID);
  var node = db.GetTarget(res, propertyArc, true);
  if (node)
    db.Change(res, propertyArc, node, rdf.GetLiteral(aValue));
  else
    db.Assert(res, propertyArc, rdf.GetLiteral(aValue), true);
}

function getRDFProperty(aID, aProperty)
{
  var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

  var db = gDownloadManager.datasource;
  var propertyArc = rdf.GetResource(NC_NS + aProperty);
  
  var res = rdf.GetResource(aID);
  var node = db.GetTarget(res, propertyArc, true);
  if (!node) return "";
  try {
    node = node.QueryInterface(Components.interfaces.nsIRDFLiteral);
    return node.Value;
  }
  catch (e) {
    try {
      node = node.QueryInterface(Components.interfaces.nsIRDFInt);
      return node.Value;
    }
    catch (e) {
      node = node.QueryInterface(Components.interfaces.nsIRDFResource);
      return node.Value;
    }
  }
  return "";
}

function fireEventForElement(aElement, aEventType)
{
  var e = document.createEvent("Events");
  e.initEvent("extension-" + aEventType, false, true);
  
  aElement.dispatchEvent(e);
}

///////////////////////////////////////////////////////////////////////////////
// Download Event Handlers

function onExtensionViewOptions(aEvent)
{
  var optionsURL = gExtensionsView.selected.getAttribute("optionsURL");
  if (optionsURL != "")
    openDialog(optionsURL, "", "chrome,modal");
}

function onExtensionVisitHomepage(aEvent)
{
  var homepageURL = gExtensionsView.selected.getAttribute("homepageURL");
  if (homepageURL != "")
    openURL(homepageURL);
}

function openURL(aURL)
{
# If we're not a browser, use the external protocol service to load the URI.
#ifndef MOZ_PHOENIX
  var uri = Components.classes["@mozilla.org/network/standard-url;1"]
                      .createInstance(Components.interfaces.nsIURI);
  uri.spec = aURL;

  var protocolSvc = Components.classes["@mozilla.org/uriloader/external-protocol-service;1"]
                              .getService(Components.interfaces.nsIExternalProtocolService);
  if (protocolSvc.isExposedProtocol(uri.scheme))
    protocolSvc.loadUrl(uri);
# If we're a browser, open a new browser window instead.    
#else
  openDialog("chrome://browser/content/browser.xul", "_blank", "chrome,all,dialog=no", aURL, null, null);
#endif
}

function onExtensionViewAbout(aEvent)
{
  var aboutURL = gExtensionsView.selected.getAttribute("aboutURL");
  if (aboutURL != "")
    openDialog(aboutURL, "", "chrome,modal");
  else
    openDialog("chrome://mozapps/content/extensions/about.xul", "", "chrome,modal", gExtensionsView.selected.id, gExtensionsView.database);
}

function onExtensionMoveTop(aEvent)
{
  var rdfs = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

  var extensions = rdfs.GetResource("urn:mozilla:extension:root");
  var container = Components.classes["@mozilla.org/rdf/container;1"].createInstance(Components.interfaces.nsIRDFContainer);
  container.Init(gExtensionManager.datasource, extensions);
  
  var extension = rdfs.GetResource(aEvent.target.id);
  var index = container.IndexOf(extension);
  if (index > 1) {
    container.RemoveElement(extension, false);
    container.InsertElementAt(extension, 1, true);
  }
  
  flushDataSource();
}

function onExtensionMoveUp(aEvent)
{
  var rdfs = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

  var extensions = rdfs.GetResource("urn:mozilla:extension:root");
  var container = Components.classes["@mozilla.org/rdf/container;1"].createInstance(Components.interfaces.nsIRDFContainer);
  container.Init(gExtensionManager.datasource, extensions);
  
  var extension = rdfs.GetResource(aEvent.target.id);
  var index = container.IndexOf(extension);
  if (index > 1) {
    container.RemoveElement(extension, false);
    container.InsertElementAt(extension, index - 1, true);
  }
  
  flushDataSource();
}

function onExtensionMoveDown(aEvent)
{
  var rdfs = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

  var extensions = rdfs.GetResource("urn:mozilla:extension:root");
  var container = Components.classes["@mozilla.org/rdf/container;1"].createInstance(Components.interfaces.nsIRDFContainer);
  container.Init(gExtensionManager.datasource, extensions);
  
  var extension = rdfs.GetResource(aEvent.target.id);
  var index = container.IndexOf(extension);
  var count = container.GetCount();
  if (index < count) {
    container.RemoveElement(extension, false);
    container.InsertElementAt(extension, index + 1, true);
  }
  
  flushDataSource();
}

function onExtensionUpdate(aEvent)
{

}

function onExtensionEnableDisable(aEvent)
{

}

function onExtensionUninstall(aEvent)
{

}

function flushDataSource()
{
#if 0
  var rds = gExtensionManager.datasource.QueryInterface(Components.interfaces.nsIRDFDataSource);
  if (rds)
    rds.Flush();
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Startup, Shutdown
function Startup() 
{
  gExtensionsView = document.getElementById("extensionsView");

  gExtensionManager = Components.classes["@mozilla.org/extension-manager;1"]
                                .getService(Components.interfaces.nsIExtensionManager);
  
  // Handlers for events generated by the Download Manager (download events)
  var observerService = Components.classes[kObserverServiceProgID]
                                  .getService(Components.interfaces.nsIObserverService);

  gWindowState = window.arguments[0];

  // This is for the "Clean Up" button, which requires there to be
  // non-active downloads before it can be enabled. 
  gExtensionsView.controllers.appendController(gExtensionsViewController);
  gExtensionsView.addEventListener("extension-open", onExtensionViewOptions, false);
  gExtensionsView.addEventListener("extension-show-options", onExtensionViewOptions, false);
  gExtensionsView.addEventListener("extension-show-homepage", onExtensionVisitHomepage, false);
  gExtensionsView.addEventListener("extension-show-info", onExtensionViewAbout, false);
  gExtensionsView.addEventListener("extension-uninstall", onExtensionUninstall, false);
  gExtensionsView.addEventListener("extension-update", onExtensionUpdate, false);
  gExtensionsView.addEventListener("extension-disable", onExtensionEnableDisable, false);
  gExtensionsView.addEventListener("extension-move-top", onExtensionMoveTop, false);
  gExtensionsView.addEventListener("extension-move-up", onExtensionMoveUp, false);
  gExtensionsView.addEventListener("extension-move-dn", onExtensionMoveDown, false);

  // Finally, update the UI. 
  gExtensionsView.database.AddDataSource(gExtensionManager.datasource);
  gExtensionsView.setAttribute("ref", "urn:mozilla:extension:root");
  gExtensionsView.focus();
}

function Shutdown() 
{
  var observerService = Components.classes[kObserverServiceProgID]
                                  .getService(Components.interfaces.nsIObserverService);
}

///////////////////////////////////////////////////////////////////////////////
// View Context Menus
var gExtensionContextMenus = ["menuitem_options", "menuitem_homepage", "menuitem_about", 
                              "menuseparator_1", "menuitem_uninstall", "menuitem_update",
                              "menuitem_disable", "menuseparator_2", "menuitem_moveTop",
                              "menuitem_moveUp", "menuitem_moveDn"];
var gThemeContextMenus = ["menuitem_homepage", "menuitem_about", "menuseparator_1", 
                          "menuitem_uninstall", "menuitem_update"];

function buildContextMenu(aEvent)
{
  if (aEvent.target.id != "extensionContextMenu")
    return false;
    
  var popup = document.getElementById("extensionContextMenu");
  while (popup.hasChildNodes())
    popup.removeChild(popup.firstChild);

  var menus = gWindowState == "extensions" ? gExtensionContextMenus : gThemeContextMenus;  
  for (var i = 0; i < menus.length; ++i) {
    var clonedMenu = document.getElementById(menus[i]).cloneNode(true);
    clonedMenu.id = clonedMenu.id + "_clone";
    popup.appendChild(clonedMenu);
  }

  var extensionsStrings = document.getElementById("extensionsStrings");
  var menuitem_about = document.getElementById("menuitem_about_clone");
  var name = document.popupNode.getAttribute("name");
  menuitem_about.setAttribute("label", extensionsStrings.getFormattedString("aboutExtension", [name]));
  
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Drag and Drop

var gExtensionsDNDObserver =
{
  onDragOver: function (aEvent, aFlavour, aDragSession)
  {
    aDragSession.canDrop = true;
  },
  
  onDrop: function(aEvent, aXferData, aDragSession)
  {
    var split = aXferData.data.split("\n");
    var url = split[0];
    if (url != aXferData.data) {  //do nothing, not a valid URL
      var name = split[1];
      saveURL(url, name, null, true, true);
    }
  },
  _flavourSet: null,  
  getSupportedFlavours: function ()
  {
    if (!this._flavourSet) {
      this._flavourSet = new FlavourSet();
      this._flavourSet.appendFlavour("text/x-moz-url");
      this._flavourSet.appendFlavour("text/unicode");
    }
    return this._flavourSet;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Command Updating and Command Handlers

var gExtensionsViewController = {
  supportsCommand: function (aCommand)
  {
    return aCommand == "cmd_cleanUp";
  },
  
  isCommandEnabled: function (aCommand)
  {
    if (aCommand == "cmd_cleanUp") 
      return gDownloadManager.canCleanUp;
    return false;
  },
  
  doCommand: function (aCommand)
  {
    if (aCommand == "cmd_cleanUp" && this.isCommandEnabled(aCommand)) {
      gDownloadManager.cleanUp();
      this.onCommandUpdate();
    }
  },  
  
  onCommandUpdate: function ()
  {
    var command = "cmd_cleanUp";
    var enabled = this.isCommandEnabled(command);
    goSetCommandEnabled(command, enabled);
  }
};

