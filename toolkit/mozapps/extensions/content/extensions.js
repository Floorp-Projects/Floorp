///////////////////////////////////////////////////////////////////////////////
// Globals

const kObserverServiceProgID = "@mozilla.org/observer-service;1";
const nsIUpdateItem = Components.interfaces.nsIUpdateItem;

var gExtensionManager = null;
var gDownloadListener = null;
var gExtensionssView  = null;
var gWindowState      = "";

///////////////////////////////////////////////////////////////////////////////
// Utility Functions 
function stripPrefix(aResourceURI)
{
  return aResourceURI.substr("urn:mozilla:extension:".length, aResourceURI.length);
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

function flushDataSource()
{
  var rds = gExtensionManager.datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
  if (rds)
    rds.Flush();
}

///////////////////////////////////////////////////////////////////////////////
// Event Handlers
function onExtensionSelect(aEvent)
{
  if (aEvent.target.selected)
    aEvent.target.setAttribute("last-selected", aEvent.target.selected.id);
  else
    aEvent.target.removeAttribute("last-selected");
}

///////////////////////////////////////////////////////////////////////////////
// Startup, Shutdown
function Startup() 
{
  gWindowState = window.arguments[0];

  gExtensionsView = document.getElementById("extensionsView");
  gExtensionManager = Components.classes["@mozilla.org/extensions/manager;1"]
                                .getService(Components.interfaces.nsIExtensionManager);
  
  // Extension Command Updating is handled by a command controller.
  gExtensionsView.controllers.appendController(gExtensionsViewController);

  // This persists the last-selected extension
  gExtensionsView.addEventListener("richview-select", onExtensionSelect, false);

  // Finally, update the UI. 
  gExtensionsView.database.AddDataSource(gExtensionManager.datasource);
  gExtensionsView.setAttribute("ref", "urn:mozilla:extension:root");
  gExtensionsView.focus();
  
  // Restore the last-selected extension
  var lastSelected = gExtensionsView.getAttribute("last-selected");
  if (lastSelected != "")
    lastSelected = document.getElementById(lastSelected);
  if (!lastSelected) 
    gExtensionsView.selectionForward();
  else
    gExtensionsView.selected = lastSelected;
}

function Shutdown() 
{
}

///////////////////////////////////////////////////////////////////////////////
// View Context Menus
var gExtensionContextMenus = ["menuitem_options", "menuitem_homepage", "menuitem_about", 
                              "menuseparator_1", "menuitem_uninstall", "menuitem_update",
                              "menuitem_enable", "menuitem_disable", "menuseparator_2", 
                              "menuitem_moveTop", "menuitem_moveUp", "menuitem_moveDn"];
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
  
  var canEnable = gExtensionsViewController.isCommandEnabled("cmd_enable");
  var menuitemToShow, menuitemToHide;
  if (canEnable) {
    menuitemToShow = document.getElementById("menuitem_enable_clone");
    menuitemToHide = document.getElementById("menuitem_disable_clone");
  }
  else {
    menuitemToShow = document.getElementById("menuitem_disable_clone");
    menuitemToHide = document.getElementById("menuitem_enable_clone");
  }
  menuitemToShow.hidden = false;
  menuitemToHide.hidden = true;
  
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Drag and Drop

var gExtensionsDNDObserver =
{
  _ioServ: null,
  _filePH: null,
  _mimeSvc: null,
  
  _ensureServices: function ()
  {
    if (!this._ioServ) {
      this._ioServ = Components.classes["@mozilla.org/network/io-service;1"]
                               .getService(Components.interfaces.nsIIOService);
      this._filePH = this._ioServ.getProtocolHandler("file")
                         .QueryInterface(Components.interfaces.nsIFileProtocolHandler);
      this._mimeSvc = Components.classes["@mozilla.org/uriloader/external-helper-app-service;1"]
                                .getService(Components.interfaces.nsIMIMEService);
    }
  },
  
  onDragOver: function (aEvent, aFlavor, aDragSession)
  {
    this._ensureServices();
  
    aDragSession.canDrop = true;
    var count = aDragSession.numDropItems;
    for (var i = 0; i < count; ++i) {
      var xfer = Components.classes["@mozilla.org/widget/transferable;1"]
                          .createInstance(Components.interfaces.nsITransferable);
      xfer.addDataFlavor("text/x-moz-url");
      aDragSession.getData(xfer, i);
      
      var data = { }, length = { };
      xfer.getTransferData("text/x-moz-url", data, length);
      var fileURL = data.value.QueryInterface(Components.interfaces.nsISupportsString).data;

      var file = this._filePH.getFileFromURLSpec(fileURL);
      var mimeType = this._mimeSvc.getTypeFromFile(file);
      if (mimeType != "application/x-xpinstall") {
        aDragSession.canDrop = false;
        break;
      }
    }
  },
  
  onDrop: function(aEvent, aXferData, aDragSession)
  {
    this._ensureServices();
    
    var xpinstallObj = {};
  
    var count = aDragSession.numDropItems;
    for (var i = 0; i < count; ++i) {
      var xfer = Components.classes["@mozilla.org/widget/transferable;1"]
                          .createInstance(Components.interfaces.nsITransferable);
      xfer.addDataFlavor("text/x-moz-url");
      aDragSession.getData(xfer, i);
      
      var data = { }, length = { };
      xfer.getTransferData("text/x-moz-url", data, length);
      var fileURL = data.value.QueryInterface(Components.interfaces.nsISupportsString).data;
      var uri = Components.classes["@mozilla.org/network/standard-url;1"]
                          .createInstance(Components.interfaces.nsIURI);
      uri.spec = fileURL;
      var url = uri.QueryInterface(Components.interfaces.nsIURL);
      xpinstallObj[url.fileName] = fileURL;
    }
    InstallTrigger.install(xpinstallObj);
  },
  _flavourSet: null,  
  getSupportedFlavours: function ()
  {
    if (!this._flavourSet) {
      this._flavourSet = new FlavourSet();
      this._flavourSet.appendFlavour("text/x-moz-url");
    }
    return this._flavourSet;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Command Updating and Command Handlers

var gExtensionsViewController = {
  supportsCommand: function (aCommand)
  {
    var commandNode = document.getElementById(aCommand);
    return commandNode && (commandNode.parentNode == document.getElementById("extensionsCommands"));
  },
  
  isCommandEnabled: function (aCommand)
  {
    var selectedItem = gExtensionsView.selected;
    switch (aCommand) {
    case "cmd_close":
      return true;
    case "cmd_options":
      return selectedItem && !selectedItem.disabled && selectedItem.getAttribute("optionsURL") != "";
    case "cmd_about":
      return !selectedItem || (selectedItem.disabled ? selectedItem.getAttribute("aboutURL") == "" : true);
    case "cmd_homepage":
      return (selectedItem && selectedItem.getAttribute("homepageURL") != "");
    case "cmd_uninstall":
      return selectedItem && selectedItem.getAttribute("blockUninstall") != "true";
    case "cmd_update":
      return true;
    case "cmd_enable":
      return selectedItem && selectedItem.disabled;
    case "cmd_disable":
      return selectedItem && selectedItem.getAttribute("blockDisable") != "true" && !selectedItem.disabled;
    case "cmd_movetop":
      return (gExtensionsView.children[0] != selectedItem);
    case "cmd_moveup":
      return (gExtensionsView.children[0] != selectedItem);
    case "cmd_movedn":
      var children = gExtensionsView.children;
      return (children[children.length-1] != selectedItem);
    }
    return false;
  },

  doCommand: function (aCommand)
  {
    this.commands[aCommand]();
  },  
  
  onCommandUpdate: function ()
  {
    var extensionsCommands = document.getElementById("extensionsCommands");
    for (var i = 0; i < extensionsCommands.childNodes.length; ++i) {
      var command = extensionsCommands.childNodes[i];
      if (this.isCommandEnabled(command.id))
        command.removeAttribute("disabled");
      else
        command.setAttribute("disabled", "true");
    }
  },
  
  commands: { 
    cmd_close: function ()
    {
      closeWindow(true);
    },  
      
    cmd_options: function ()
    {
      var optionsURL = gExtensionsView.selected.getAttribute("optionsURL");
      if (optionsURL != "")
        openDialog(optionsURL, "", "chrome,modal");
    },
    
    cmd_homepage: function ()
    {
      var homepageURL = gExtensionsView.selected.getAttribute("homepageURL");
      if (homepageURL != "")
        openURL(homepageURL);
    },
    
    cmd_about: function ()
    {
      var aboutURL = gExtensionsView.selected.getAttribute("aboutURL");
      if (aboutURL != "")
        openDialog(aboutURL, "", "chrome,modal");
      else
        openDialog("chrome://mozapps/content/extensions/about.xul", "", "chrome,modal", gExtensionsView.selected.id, gExtensionsView.database);
    },  
    
    cmd_movetop: function ()
    {
      var rdfs = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

      var extensions = rdfs.GetResource("urn:mozilla:extension:root");
      var container = Components.classes["@mozilla.org/rdf/container;1"].createInstance(Components.interfaces.nsIRDFContainer);
      container.Init(gExtensionManager.datasource, extensions);
      
      var movingID = gExtensionsView.selected.id;
      var extension = rdfs.GetResource(movingID);
      var index = container.IndexOf(extension);
      if (index > 1) {
        container.RemoveElement(extension, false);
        container.InsertElementAt(extension, 1, true);
      }
      
      flushDataSource();
      
      gExtensionsView.selected = document.getElementById(movingID);
    },
    
    cmd_moveup: function ()
    {
      var rdfs = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

      var extensions = rdfs.GetResource("urn:mozilla:extension:root");
      var container = Components.classes["@mozilla.org/rdf/container;1"].createInstance(Components.interfaces.nsIRDFContainer);
      container.Init(gExtensionManager.datasource, extensions);
      
      var movingID = gExtensionsView.selected.id;
      var extension = rdfs.GetResource(movingID);
      var index = container.IndexOf(extension);
      if (index > 1) {
        container.RemoveElement(extension, false);
        container.InsertElementAt(extension, index - 1, true);
      }
      
      flushDataSource();
      
      gExtensionsView.selected = document.getElementById(movingID);
    },
    
    cmd_movedn: function ()
    {
      var rdfs = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

      var extensions = rdfs.GetResource("urn:mozilla:extension:root");
      var container = Components.classes["@mozilla.org/rdf/container;1"].createInstance(Components.interfaces.nsIRDFContainer);
      container.Init(gExtensionManager.datasource, extensions);
      
      var movingID = gExtensionsView.selected.id;
      var extension = rdfs.GetResource(movingID);
      var index = container.IndexOf(extension);
      var count = container.GetCount();
      if (index < count) {
        container.RemoveElement(extension, true);
        container.InsertElementAt(extension, index + 1, true);
      }
      
      flushDataSource();
      
      gExtensionsView.selected = document.getElementById(movingID);
    },
    
    cmd_update: function ()
    { 
      var id = stripPrefix(gExtensionsView.selected.id);
      var items = gExtensionManager.getItemList(id, nsIUpdateItem.TYPE_EXTENSION, { });
      var updates = Components.classes["@mozilla.org/updates/update-service;1"]
                              .getService(Components.interfaces.nsIUpdateService);
      updates.checkForUpdates(items, items.length, nsIUpdateItem.TYPE_EXTENSION, 
                              Components.interfaces.nsIUpdateService.SOURCE_EVENT_USER,
                              window);
    },

    cmd_uninstall: function ()
    {
      // Confirm the uninstall
      var extensionsStrings = document.getElementById("extensionsStrings");
      var brandStrings = document.getElementById("brandStrings");

      var name = gExtensionsView.selected.getAttribute("name");
      var title = extensionsStrings.getFormattedString("queryUninstallTitle", [name]);
      if (gWindowState == "extensions")
        message = extensionsStrings.getFormattedString("queryUninstallExtensionMessage", [name, name]);
      else if (gWindowState == "themes")
        message = extensionsStrings.getFormattedString("queryUninstallThemeMessage", [name]);

      // XXXben - improve the wording on the buttons here!
      var promptSvc = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                .getService(Components.interfaces.nsIPromptService);
      if (!promptSvc.confirm(window, title, message))
        return;

      var selectedID = gExtensionsView.selected.id;
      var selectedElement = document.getElementById(selectedID);
      var nextElement = selectedElement.nextSibling;
      if (!nextElement)
        nextElement = selectedElement.previousSibling;
      nextElement = nextElement.id;
      
      if (gWindowState == "extensions")
        gExtensionManager.uninstallExtension(stripPrefix(selectedID));
      else if (gWindowState == "themes")
        gExtensionManager.uninstallTheme(stripPrefix(selectedID));
      
      gExtensionsView.selected = document.getElementById(nextElement);

#if ANNOYING_ASS_ALERTS
      // Show an alert message telling the user they need to restart for the
      // action to take effect.
      message = extensionsStrings.getFormattedString("restartBeforeUninstallMessage", 
                                                     [gExtensionsView.selected.getAttribute("name"),
                                                      brandStrings.getString("brandShortName")]);

      promptSvc.alert(window, extensionsStrings.getString("restartBeforeUninstallTitle"), message);
#endif
    },
    
    cmd_disable: function ()
    {
      gExtensionManager.disableExtension(stripPrefix(gExtensionsView.selected.id));

#if ANNOYING_ASS_ALERTS
      // Show an alert message telling the user they need to restart for the
      // action to take effect.
      var extensionsStrings = document.getElementById("extensionsStrings");
      var brandStrings = document.getElementById("brandStrings");

      var message = extensionsStrings.getFormattedString("restartBeforeDisableMessage", 
                                                         [gExtensionsView.selected.getAttribute("name"),
                                                         brandStrings.getString("brandShortName")]);

      var promptSvc = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                .getService(Components.interfaces.nsIPromptService);
      promptSvc.alert(window, extensionsStrings.getString("restartBeforeDisableTitle"), message);
#endif
    },
    
    cmd_enable: function ()
    {
      gExtensionManager.enableExtension(stripPrefix(gExtensionsView.selected.id));

#if ANNOYING_ASS_ALERTS
      // Show an alert message telling the user they need to restart for the
      // action to take effect.
      var extensionsStrings = document.getElementById("extensionsStrings");
      var brandStrings = document.getElementById("brandStrings");

      var message = extensionsStrings.getFormattedString("restartBeforeEnableMessage", 
                                                         [gExtensionsView.selected.getAttribute("name"),
                                                         brandStrings.getString("brandShortName")]);

      var promptSvc = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                .getService(Components.interfaces.nsIPromptService);
      promptSvc.alert(window, extensionsStrings.getString("restartBeforeEnableTitle"), message);
#endif
    },
  }
};


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
