# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
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
#   Ben Goodger <ben@mozilla.org>
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
const nsIUpdateItem = Components.interfaces.nsIUpdateItem;

var gExtensionManager = null;
var gExtensionsView   = null;
var gWindowState      = "";
var gGetMoreURL       = "";
var gCurrentTheme     = "classic/1.0";
var gDefaultTheme     = "classic/1.0";
var gDownloadManager  = null;
var gObserverIndex    = -1;
var gItemType         = -1;
var gApp              = null;

const PREF_EXTENSIONS_GETMORETHEMESURL      = "extensions.getMoreThemesURL";
const PREF_EXTENSIONS_GETMOREEXTENSIONSURL  = "extensions.getMoreExtensionsURL";
const PREF_EXTENSIONS_DSS_ENABLED           = "extensions.dss.enabled";
const PREF_EXTENSIONS_DSS_SWITCHPENDING     = "extensions.dss.switchPending";
const PREF_EM_LAST_SELECTED_SKIN            = "extensions.lastSelectedSkin";
const PREF_GENERAL_SKINS_SELECTEDSKIN       = "general.skins.selectedSkin";

const RDFURI_ITEM_ROOT  = "urn:mozilla:item:root";
const PREFIX_ITEM_URI   = "urn:mozilla:item:";

const OP_NONE                         = "";
const OP_NEEDS_INSTALL                = "needs-install";
const OP_NEEDS_UPGRADE                = "needs-upgrade";
const OP_NEEDS_UNINSTALL              = "needs-uninstall";
const OP_NEEDS_ENABLE                 = "needs-enable";
const OP_NEEDS_DISABLE                = "needs-disable";

///////////////////////////////////////////////////////////////////////////////
// Utility Functions 

function LOG(msg) {
  dump("*** " + msg + "\n");
}

function getIDFromResourceURI(aURI) 
{
  if (aURI.substring(0, PREFIX_ITEM_URI.length) == PREFIX_ITEM_URI) 
    return aURI.substring(PREFIX_ITEM_URI.length);
  return aURI;
}

function openURL(aURL)
{
# If we're not a browser, use the external protocol service to load the URI.
#ifndef MOZ_PHOENIX
  var uri = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService)
                      .newURI(aURL, null, null);

  var protocolSvc = Components.classes["@mozilla.org/uriloader/external-protocol-service;1"]
                              .getService(Components.interfaces.nsIExternalProtocolService);
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

function setRestartMessage(aItem)
{
  var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                      .getService(Components.interfaces.nsIStringBundleService);
  var extensionStrings = sbs.createBundle("chrome://mozapps/locale/extensions/extensions.properties");
  var brandStrings = sbs.createBundle("chrome://branding/locale/brand.properties");
  var brandShortName = brandStrings.GetStringFromName("brandShortName");
  var themeName = aItem.getAttribute("name");
  var restartMessage = extensionStrings.formatStringFromName("dssSwitchAfterRestart", 
                                                             [brandShortName], 1);
  for (var i = 0; i < gExtensionsView.childNodes.length; ++i) {
    var item = gExtensionsView.childNodes[i];
    if (item.hasAttribute("oldDescription")) {
      item.setAttribute("description", item.getAttribute("oldDescription"));
      item.removeAttribute("oldDescription");
    }
  }
  aItem.setAttribute("oldDescription", aItem.getAttribute("description"));
  aItem.setAttribute("description", restartMessage);
}

///////////////////////////////////////////////////////////////////////////////
// Event Handlers
function onExtensionSelect(aEvent)
{
  if (aEvent.target.selectedItem)
    aEvent.target.setAttribute("last-selected", aEvent.target.selectedItem.id);
  else
    aEvent.target.removeAttribute("last-selected");
}

function onExtensionUpdateNow(aEvent)
{
  var item = gExtensionManager.getItemForID(getIDFromResourceURI(
    aEvent.target.id));
  gExtensionManager.addDownloads([item], 1, true);
}

///////////////////////////////////////////////////////////////////////////////
// Startup, Shutdown
function Startup() 
{
  gWindowState = window.location.search.substr("?type=".length, window.location.search.length);
  
  var isExtensions = gWindowState == "extensions";
  gItemType   = isExtensions ? nsIUpdateItem.TYPE_EXTENSION : nsIUpdateItem.TYPE_THEME;
  var typeCondition = document.getElementById("typeCondition");
  typeCondition.setAttribute("object", gItemType);
  
  document.documentElement.setAttribute("windowtype", document.documentElement.getAttribute("windowtype") + "-" + gWindowState);

  gExtensionsView = document.getElementById("extensionsView");
  gExtensionsView.setAttribute("state", gWindowState);
  gExtensionManager = Components.classes["@mozilla.org/extensions/manager;1"]
                                .getService(Components.interfaces.nsIExtensionManager);
  gApp = Components.classes["@mozilla.org/xre/app-info;1"].getService(Components.interfaces.nsIXULAppInfo)
                   .QueryInterface(Components.interfaces.nsIXULRuntime);

  // Extension Command Updating is handled by a command controller.
  gExtensionsView.controllers.appendController(gExtensionsViewController);

  // This persists the last-selected extension
  gExtensionsView.addEventListener("select", onExtensionSelect, false);
  gExtensionsView.addEventListener("extension-updatenow", onExtensionUpdateNow, false);

  // Finally, update the UI. 
  gExtensionsView.database.AddDataSource(gExtensionManager.datasource);
  gExtensionsView.setAttribute("ref", RDFURI_ITEM_ROOT);
  gExtensionsView.focus();
  
  var pref = Components.classes["@mozilla.org/preferences-service;1"]
                       .getService(Components.interfaces.nsIPrefBranch);
  var defaultPref = pref.QueryInterface(Components.interfaces.nsIPrefService)
                        .getDefaultBranch(null);
  if (!isExtensions) {
    gExtensionsView.addEventListener("select", onThemeSelect, false);

    try {
        gCurrentTheme = pref.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN);
        gDefaultTheme = defaultPref.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN);
    }
    catch (e) { }

    var useThemeButton = document.getElementById("useThemeButton");
    useThemeButton.hidden = false;

    var optionsButton = document.getElementById("optionsButton");
    optionsButton.hidden = true;
  }
  
  // Initialize the richlistbox.  This will select the last selected extension
  // or default to the first listed one.
  gExtensionsView.init();

  var extensionsStrings = document.getElementById("extensionsStrings");
  document.title = extensionsStrings.getString(gWindowState + "Title");
  
  gExtensionsViewController.onCommandUpdate(); 
  
  gGetMoreURL = pref.getComplexValue(isExtensions ? PREF_EXTENSIONS_GETMOREEXTENSIONSURL 
                                                  : PREF_EXTENSIONS_GETMORETHEMESURL, 
                                     Components.interfaces.nsIPrefLocalizedString).data;
  var app = Components.classes["@mozilla.org/xre/app-info;1"]
                      .getService(Components.interfaces.nsIXULAppInfo);
  gGetMoreURL = gGetMoreURL.replace(/%APPID%/g, app.ID);
  // Update various pieces of state-dependant UI
  var getMore = document.getElementById("getMore");
  getMore.setAttribute("value", getMore.getAttribute(isExtensions ? "valueextensions" : "valuethemes"));
  getMore.setAttribute("tooltiptext", getMore.getAttribute(isExtensions ? "tooltiptextextensions" : "tooltiptextthemes"));
  getMore.setAttribute('href', gGetMoreURL);
  
  if (!isExtensions) {
    var themePreviewArea = document.getElementById("themePreviewArea");
    themePreviewArea.hidden = false;
    gExtensionsView.removeAttribute("flex");
    gExtensionsView.style.width = extensionsStrings.getString("themesManagerLeftColumn");
  }
  
  // Set Initial Size
  var win = document.documentElement;
  if (!win.hasAttribute("width") || !win.hasAttribute("height")) {
    win.setAttribute("width", isExtensions ? (extensionsStrings.getString("extensionsManagerWidth")) : (extensionsStrings.getString("themesManagerWidth")));
    win.setAttribute("height", isExtensions ? (extensionsStrings.getString("extensionsManagerHeight")) : (extensionsStrings.getString("themesManagerHeight")));
  }

  // Now look and see if we're being opened by XPInstall
  gDownloadManager = new XPInstallDownloadManager();
  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Components.interfaces.nsIObserverService);
  os.addObserver(gDownloadManager, "xpinstall-download-started", false);

  gObserverIndex = gExtensionManager.addUpdateListener(gDownloadManager);
  
  if ("arguments" in window) {
    try {
      var params = window.arguments[0].QueryInterface(Components.interfaces.nsIDialogParamBlock);
      gDownloadManager.addDownloads(params);
    }
    catch (e) {
      if (window.arguments[0] == "updatecheck")
        performUpdate();
    }
  }
  
  // Set the tooltips
  if (!isExtensions) {
#ifndef MOZ_PHOENIX
    document.getElementById("installButton").setAttribute("tooltiptext", extensionsStrings.getString("cmdInstallTooltipTheme"));
#endif
    document.getElementById("uninstallButton").setAttribute("tooltiptext", extensionsStrings.getString("cmdUninstallTooltipTheme"));
    document.getElementById("updateButton").setAttribute("tooltiptext", extensionsStrings.getString("cmdUpdateTooltipTheme"));
  }
}

function Shutdown() 
{
  if (gWindowState != "extensions")
    gExtensionsView.removeEventListener("select", onThemeSelect, false);
  
  gExtensionsView.removeEventListener("select", onExtensionSelect, false);
  gExtensionsView.removeEventListener("extension-updatenow", onExtensionUpdateNow, false);

  gExtensionsView.database.RemoveDataSource(gExtensionManager.datasource);

  gExtensionManager.removeUpdateListenerAt(gObserverIndex);

  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Components.interfaces.nsIObserverService);
  if (gDownloadManager) 
    os.removeObserver(gDownloadManager, "xpinstall-download-started");
}

///////////////////////////////////////////////////////////////////////////////
//
// XPInstall
//

function getURLSpecFromFile(aFile)
{
  var ioServ = Components.classes["@mozilla.org/network/io-service;1"]
                          .getService(Components.interfaces.nsIIOService);
  var fph = ioServ.getProtocolHandler("file").QueryInterface(Components.interfaces.nsIFileProtocolHandler);
  return fph.getURLSpecFromFile(aFile);
}

function XPInstallDownloadManager()
{
  var extensionsStrings = document.getElementById("extensionsStrings");
  this._statusFormatKBMB  = extensionsStrings.getString("statusFormatKBMB");
  this._statusFormatKBKB  = extensionsStrings.getString("statusFormatKBKB");
  this._statusFormatMBMB  = extensionsStrings.getString("statusFormatMBMB");
}

XPInstallDownloadManager.prototype = {
  _statusFormat     : null,
  _statusFormatKBMB : null,
  _statusFormatKBKB : null,
  _statusFormatMBMB : null,
  
  observe: function (aSubject, aTopic, aData) 
  {
    switch (aTopic) {
      case "xpinstall-download-started":
        var params = aSubject.QueryInterface(Components.interfaces.nsISupportsArray);
        var paramBlock = params.GetElementAt(0).QueryInterface(Components.interfaces.nsISupportsInterfacePointer);
        paramBlock = paramBlock.data.QueryInterface(Components.interfaces.nsIDialogParamBlock);
        this.addDownloads(paramBlock);
        break;
    }
  },
  
  addDownloads: function (aParams)
  {
    var numXPInstallItems = aParams.GetInt(1);
    var isExtensions = gWindowState == "extensions";
    
    var items = [];
    for (var i = 0; i < numXPInstallItems;) {
      var displayName = aParams.GetString(i++);
      var url = aParams.GetString(i++);
      var iconURL = aParams.GetString(i++);
      if (!iconURL) { 
        iconURL = isExtensions ? "chrome://mozapps/skin/xpinstall/xpinstallItemGeneric.png" : 
                                 "chrome://mozapps/skin/extensions/themeGeneric.png";
      }
      
      var type = isExtensions ? nsIUpdateItem.TYPE_EXTENSION : nsIUpdateItem.TYPE_THEME;
      // gExtensionManager.addDownload(displayName, url, iconURL, type);
      var item = Components.classes["@mozilla.org/updates/item;1"]
                           .createInstance(Components.interfaces.nsIUpdateItem);
      item.init(url, " ", "app-profile", "", "", displayName, url, iconURL, "", type);
      items.push(item);

      // Advance the enumerator
      var certName = aParams.GetString(i++);
    }
    
    gExtensionManager.addDownloads(items, items.length, false);
    gExtensionsView.scrollBoxObject.ensureElementIsVisible(gExtensionsView.lastChild);
  },
  
  getElementForAddon: function(aAddon) 
  {
    var element = document.getElementById(PREFIX_ITEM_URI + aAddon.id);
    if (aAddon.id == aAddon.xpiURL)
      element = document.getElementById(aAddon.xpiURL);
    return element;
  },

  /////////////////////////////////////////////////////////////////////////////  
  // nsIAddonUpdateListener
  onStateChange: function (aAddon, aState, aValue)
  {
    const nsIXPIProgressDialog = Components.interfaces.nsIXPIProgressDialog;
    var element = this.getElementForAddon(aAddon);
    if (!element) return;
    switch (aState) {
      case nsIXPIProgressDialog.DOWNLOAD_START:
        element.setAttribute("state", "waiting");

        var extensionsStrings = document.getElementById("extensionsStrings");
        element.setAttribute("description",
                             extensionsStrings.getString("installWaiting"));

        element.setAttribute("progress", "0");
        break;
      case nsIXPIProgressDialog.DOWNLOAD_DONE:
        element.setAttribute("progress", "100");
        break;
      case nsIXPIProgressDialog.INSTALL_START:
        element.setAttribute("state", "installing");

        extensionsStrings = document.getElementById("extensionsStrings");
        element.setAttribute("description",
                             extensionsStrings.getString("installInstalling"));
        break;
      case nsIXPIProgressDialog.INSTALL_DONE:
        dump("*** state change = " + aAddon.xpiURL + ", state = " + aState + ", value = " + aValue + "\n");
        element.setAttribute("state", "done");
        extensionsStrings = document.getElementById("extensionsStrings");
        element.setAttribute("description",
                             extensionsStrings.getString("installSuccess"));
        var msg;
        if (aValue != 0 && aValue != 999) {
          var xpinstallStrings = document.getElementById("xpinstallStrings");
          try {
            msg = xpinstallStrings.getString("error" + aValue);
          }
          catch (e) {
            msg = xpinstallStrings.getFormattedString("unknown.error", [aValue]);
          }
          var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                              .getService(Components.interfaces.nsIStringBundleService);
          var extensionStrings = sbs.createBundle("chrome://mozapps/locale/extensions/extensions.properties");
          var title = extensionStrings.GetStringFromName("errorInstallTitle");

          var brandStrings = sbs.createBundle("chrome://branding/locale/brand.properties");
          var brandShortName = brandStrings.GetStringFromName("brandShortName");
          var params = [brandShortName, aAddon.xpiURL, msg];
          var message = extensionStrings.formatStringFromName("errorInstallMsg", params, params.length);

          var ps = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                             .getService(Components.interfaces.nsIPromptService);
          ps.alert(window, title, message);
          element.setAttribute("status", msg);
        }
        // Remove the dummy, since we installed successfully (only if we're a URL, 
        // not a download operation on an existing item.
        var type = gWindowState == "extensions" ? nsIUpdateItem.TYPE_EXTENSION
                                                : nsIUpdateItem.TYPE_THEME;
        gExtensionManager.removeDownload(aAddon.xpiURL);
        break;
      case nsIXPIProgressDialog.DIALOG_CLOSE:
        break;
    }
  },
  
  _urls: { },
  onProgress: function (aAddon, aValue, aMaxValue)
  {
    var element = this.getElementForAddon(aAddon);
    if (!element) return;
    var percent = Math.round((aValue / aMaxValue) * 100);
    if (percent > 1 && !(aAddon.xpiURL in this._urls)) {
      this._urls[aAddon.xpiURL] = true;
      element.setAttribute("state", "downloading");
    }
    element.setAttribute("progress", percent);
    
    var KBProgress = parseInt(aValue/1024 + .5);
    var KBTotal = parseInt(aMaxValue/1024 + .5);
    element.setAttribute("status", this._formatKBytes(KBProgress, KBTotal));
  },
  
  _replaceInsert: function ( text, index, value ) 
  {
    var result = text;
    var regExp = new RegExp( "#"+index );
    result = result.replace( regExp, value );
    return result;
  },
  
  // aBytes     aTotalKBytes    returns:
  // x, < 1MB   y < 1MB         x of y KB
  // x, < 1MB   y >= 1MB        x KB of y MB
  // x, >= 1MB  y >= 1MB        x of y MB
  _formatKBytes: function (aKBytes, aTotalKBytes)
  {
    var progressHasMB = parseInt(aKBytes/1000) > 0;
    var totalHasMB = parseInt(aTotalKBytes/1000) > 0;
    
    var format = "";
    if (!progressHasMB && !totalHasMB) {
      format = this._statusFormatKBKB;
      format = this._replaceInsert(format, 1, aKBytes);
      format = this._replaceInsert(format, 2, aTotalKBytes);
    }
    else if (progressHasMB && totalHasMB) {
      format = this._statusFormatMBMB;
      format = this._replaceInsert(format, 1, (aKBytes / 1000).toFixed(1));
      format = this._replaceInsert(format, 2, (aTotalKBytes / 1000).toFixed(1));
    }
    else if (totalHasMB && !progressHasMB) {
      format = this._statusFormatKBMB;
      format = this._replaceInsert(format, 1, aKBytes);
      format = this._replaceInsert(format, 2, (aTotalKBytes / 1000).toFixed(1));
    }
    else {
      // This is an undefined state!
      dump("*** huh?!\n");
    }
    
    return format;  
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsISupports
  QueryInterface: function (aIID) 
  {
    if (!aIID.equals(Components.interfaces.nsIAddonUpdateListener) &&
        !aIID.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

///////////////////////////////////////////////////////////////////////////////
//
// Update Listener
//
function UpdateCheckListener() {
  this._addons = [];
}
UpdateCheckListener.prototype = {
  /**
   * A list of addons we've found updates to in this update check transaction.
   */
  _addons: [],
  
  /**
   * See nsIExtensionManager.idl
   */
  onUpdateStarted: function() {
    gExtensionsView.setAttribute("update-check", "true");
    var command = document.getElementById("cmd_update");
    command.setAttribute("disabled", "true");
    command = document.getElementById("cmd_update_all");
    command.setAttribute("disabled", "true");
  },
  
  /**
   * See nsIExtensionManager.idl
   */
  onUpdateEnded: function() {
    gExtensionsView.removeAttribute("update-check");
    var command = document.getElementById("cmd_update");
    gExtensionsViewController.updateCommand(command);
    command = document.getElementById("cmd_update_all");
    command.removeAttribute("disabled");
    
    // Show message to user listing extensions for which updates are available
    // and prompt to install now.
    if (this._addons.length == 1) {
      var element = document.getElementById(PREFIX_ITEM_URI + this._addons[0].id);
      gExtensionsView.scrollBoxObject.scrollToElement(element);
    }
    else if (this._addons.length > 1) {
      var strings = document.getElementById("extensionsStrings");
      var brandName = document.getElementById("brandStrings").getString("brandShortName");
      var params = {
        message1: strings.getFormattedString("updatesAvailableMessage1",
                                             [brandName]),
        message2: strings.getString("updatesAvailableMessage2"),
        title: strings.getString("updatesAvailableTitle"),
        buttons: {
          accept: { label: strings.getString("updatesAvailableAccept"),
                    focused: true },
          cancel: { label: strings.getString("updatesAvailableCancel") }
        }
      }
      var names = [];
      for (var i = 0; i < this._addons.length; ++i) {
        var addon = this._addons[i];
        var existingItem = gExtensionManager.getItemForID(addon.id);
        var name = strings.getFormattedString("itemFormat", 
                                              [this._addons[i].name, 
                                               existingItem.version,
                                               this._addons[i].version]);
        names.push(name);
      }
      openDialog("chrome://mozapps/content/extensions/list.xul", "", 
                 "titlebar,modal", names, params);
      if (params.result == "accept") 
        gExtensionManager.addDownloads(this._addons, this._addons.length, true);
    }
  },
  
  /**
   * See nsIExtensionManager.idl
   */
  onAddonUpdateStarted: function(addon) {
    var element = document.getElementById(PREFIX_ITEM_URI + addon.id);
    element.setAttribute("loading", "true");
  },
  
  /**
   * See nsIExtensionManager.idl
   */
  onAddonUpdateEnded: function(addon, status) {
    var element = document.getElementById(PREFIX_ITEM_URI + addon.id);
    element.removeAttribute("loading");
    const nsIAUCL = Components.interfaces.nsIAddonUpdateCheckListener;
    var strings = document.getElementById("extensionsStrings");
    switch (status) {
    case nsIAUCL.STATUS_UPDATE:
      this._addons.push(addon);
      break;
    case nsIAUCL.STATUS_FAILURE:
      element.setAttribute("description", strings.getString("updateFailedMsg"));
      break;
    case nsIAUCL.STATUS_DISABLED:
      element.setAttribute("description", strings.getString("updateDisabledMsg")); 
      break;
    }
  },
  
  /**
   * See nsISupports.idl
   */
  QueryInterface: function(iid) {
    if (!iid.equals(Components.interfaces.nsIAddonUpdateCheckListener) &&
        !iid.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};


///////////////////////////////////////////////////////////////////////////////
//
// View Event Handlers
//
function onViewDoubleClick(aEvent)
{
  if (aEvent.button != 0)
    return;

  switch (gWindowState) {
    case "extensions":
      gExtensionsViewController.doCommand('cmd_options');
      break;
    case "themes":
      gExtensionsViewController.doCommand('cmd_useTheme');
      break;
  }
}

function onThemeSelect(aEvent)
{
  if (gWindowState != "themes")
    return;

  var previewImageDeck = document.getElementById("previewImageDeck");
  if (!gExtensionsView.selectedItem) {
    previewImageDeck.setAttribute("selectedIndex", "0");
    return;
  }
  var url = gExtensionsView.selectedItem.getAttribute("previewImage");
  if (url) {
    previewImageDeck.setAttribute("selectedIndex", "2");
    var previewImage = document.getElementById("previewImage");
    previewImage.setAttribute("src", url);
  }
  else
    previewImageDeck.setAttribute("selectedIndex", "1");
}

///////////////////////////////////////////////////////////////////////////////
// View Context Menus
var gExtensionContextMenus = ["menuitem_options", "menuitem_homepage", "menuitem_about", 
                              "menuseparator_1", "menuitem_uninstall", "menuitem_update",
                              "menuitem_enable", "menuitem_disable", "menuseparator_2", 
                              "menuitem_moveTop", "menuitem_moveUp", "menuitem_moveDn"];
var gThemeContextMenus = ["menuitem_useTheme", "menuitem_homepage", "menuitem_about", 
                          "menuseparator_1", "menuitem_uninstall", "menuitem_update",
                          "menuitem_enable"];

function buildContextMenu(aEvent)
{
  if (aEvent.target.id != "extensionContextMenu")
    return false;
    
  var popup = document.getElementById("extensionContextMenu");
  while (popup.hasChildNodes())
    popup.removeChild(popup.firstChild);

  var isExtensions = gWindowState == "extensions";

  var menus = isExtensions ? gExtensionContextMenus : gThemeContextMenus;  
  for (var i = 0; i < menus.length; ++i) {
    var clonedMenu = document.getElementById(menus[i]).cloneNode(true);
    clonedMenu.id = clonedMenu.id + "_clone";
    popup.appendChild(clonedMenu);
  }

  var extensionsStrings = document.getElementById("extensionsStrings");
  var menuitem_about = document.getElementById("menuitem_about_clone");
  var selectedItem = gExtensionsView.selectedItem;
  var name = selectedItem ? selectedItem.getAttribute("name") : "";
  menuitem_about.setAttribute("label", extensionsStrings.getFormattedString("aboutExtension", [name]));
  
  if (isExtensions) {
    var canEnable = gExtensionsViewController.isCommandEnabled("cmd_reallyEnable");
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
  }
  else {
    var enableMenu = document.getElementById("menuitem_enable_clone");
    if (gExtensionsView.selectedItem &&
       (gExtensionsView.selectedItem.getAttribute("compatible") == "false" ||
        gExtensionsView.selectedItem.disabled))
      // don't let the user activate incompatible themes, but show a (disabled) Enable
      // menuitem to give visual feedback; it's disabled because cmd_enable returns false
      enableMenu.hidden = false;
    else {
      enableMenu.hidden = true;
    }
  }
    
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Drag and Drop

var gExtensionsDNDObserver =
{
  _ioServ: null,
  _canDrop: false,
  
  _ensureServices: function ()
  {
    if (!this._ioServ)
      this._ioServ = Components.classes["@mozilla.org/network/io-service;1"]
                               .getService(Components.interfaces.nsIIOService);
  },

  // returns a JS object whose properties are used by xpinstall
  _getDataFromDragSession: function (aDragSession, aPosition)
  {
    var fileData = { };
    // if this fails we do not have valid data to drop
    try {
      var xfer = Components.classes["@mozilla.org/widget/transferable;1"]
                           .createInstance(Components.interfaces.nsITransferable);
      xfer.addDataFlavor("text/x-moz-url");
      xfer.addDataFlavor("application/x-moz-file", "nsIFile");
      aDragSession.getData(xfer, aPosition);

      var flavour = { }, data = { }, length = { };
      xfer.getAnyTransferData(flavour, data, length);
      var selectedFlavour = this.getSupportedFlavours().flavourTable[flavour.value];
      var xferData = new FlavourData(data.value, length.value, selectedFlavour);

      var fileURL = transferUtils.retrieveURLFromData(xferData.data,
                                                      xferData.flavour.contentType);
      fileData.fileURL = fileURL;

      var uri = this._ioServ.newURI(fileURL, null, null);
      var url = uri.QueryInterface(Components.interfaces.nsIURL);
      fileData.fileName = url.fileName;

      switch (url.fileExtension) {
        case "xpi":
          fileData.type = nsIUpdateItem.TYPE_EXTENSION;
          break;
        case "jar":
          fileData.type = nsIUpdateItem.TYPE_THEME;
          break;
        default:
          return null;
      }
    }
    catch (e) {
      return null;
    }

    return fileData;
  },

  canDrop: function (aEvent, aDragSession) { return this._canDrop; },

  onDragEnter: function (aEvent, aDragSession)
  {
    this._ensureServices();

    var count = aDragSession.numDropItems;
    for (var i = 0; i < count; ++i) {
      var fileData = this._getDataFromDragSession(aDragSession, i);
      if (!fileData) {
        this._canDrop = false;
        return;
      }
    }
    this._canDrop = true;
  },

  onDragOver: function (aEvent, aFlavor, aDragSession) { },

  onDrop: function(aEvent, aXferData, aDragSession)
  {
    this._ensureServices();
    
    var xpinstallObj = { };
    var themes = { };
    var xpiCount = 0;
    var themeCount = 0;
    
    var count = aDragSession.numDropItems;
    for (var i = 0; i < count; ++i) {
      var fileData = this._getDataFromDragSession(aDragSession, i);
      if (!fileData)
        return;

      if (fileData.type == nsIUpdateItem.TYPE_EXTENSION) {
        xpinstallObj[fileData.fileName] = fileData.fileURL;
        ++xpiCount;
      }
      else if (fileData.type == nsIUpdateItem.TYPE_THEME) {
        themes[fileData.fileName] = fileData.fileURL;
        ++themeCount;
      }
    }

    if (xpiCount > 0) 
      InstallTrigger.install(xpinstallObj);
    if (themeCount > 0) {
      for (var fileName in themes)
        InstallTrigger.installChrome(InstallTrigger.SKIN, themes[fileName], fileName);
    }
  },
  _flavourSet: null,  
  getSupportedFlavours: function ()
  {
    if (!this._flavourSet) {
      this._flavourSet = new FlavourSet();
      this._flavourSet.appendFlavour("text/x-moz-url");
      this._flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
    }
    return this._flavourSet;
  }
};

///////////////////////////////////////////////////////////////////////////////
// Command Updating and Command Handlers

function canWriteToLocation(element)
{
  var installLocation = null;
  if (element) {
    var id = getIDFromResourceURI(element.id)
    installLocation = gExtensionManager.getInstallLocation(id);
  }
  return installLocation ? installLocation.canAccess : false;
}

function performUpdate()
{
  gExtensionsViewController.commands.cmd_update(null);
}

var gExtensionsViewController = {
  supportsCommand: function (aCommand)
  {
    var commandNode = document.getElementById(aCommand);
    return commandNode && (commandNode.parentNode == document.getElementById("extensionsCommands"));
  },

  isCommandEnabled: function (aCommand)
  {
    var selectedItem = gExtensionsView.selectedItem;
    if (selectedItem) {
      if (selectedItem.getAttribute("downloadURL") != "")
        return false;
      var opType = selectedItem.getAttribute("opType");
    }

    switch (aCommand) {
    case "cmd_close":
      return true;
    case "cmd_useTheme":
      return selectedItem &&
             !selectedItem.disabled &&
             opType != OP_NEEDS_UNINSTALL &&
             gCurrentTheme != selectedItem.getAttribute("internalName");
    case "cmd_options":
      return selectedItem &&
             !selectedItem.disabled &&
             !gApp.inSafeMode &&
             opType == OP_NONE &&
             selectedItem.getAttribute("optionsURL") != "";
    case "cmd_about":
      return selectedItem &&
             opType != OP_NEEDS_INSTALL;
    case "cmd_homepage":
      return selectedItem && selectedItem.getAttribute("homepageURL") != "";
    case "cmd_uninstall":
      if (gWindowState != "extensions") {
        // uninstall is never available for the default theme
        return (selectedItem && 
                selectedItem.getAttribute("internalName") != gDefaultTheme &&
                opType != OP_NEEDS_UNINSTALL &&
                selectedItem.getAttribute("locked") != "true" &&
                canWriteToLocation(selectedItem));
      }
      return selectedItem &&
             opType != OP_NEEDS_UNINSTALL &&
             selectedItem.getAttribute("locked") != "true" &&
             canWriteToLocation(selectedItem);
    case "cmd_update":
      return selectedItem &&
             selectedItem.getAttribute("updateable") != "false" &&
             !gExtensionsView.hasAttribute("update-check");
    case "cmd_update_all":
      return gExtensionsView.children.length > 0 &&
             !gExtensionsView.hasAttribute("update-check");
    case "cmd_reallyEnable":
    // controls whether to show Enable or Disable in extensions' context menu
      return selectedItem && 
            (selectedItem.disabled &&
             opType != OP_NEEDS_ENABLE ||
             opType == OP_NEEDS_DISABLE ||
             opType == OP_NEEDS_UNINSTALL);
    case "cmd_enable":
    //controls wheter the Enable/Disable menuitem is enabled
      return selectedItem && 
             opType != OP_NEEDS_ENABLE &&
             opType != OP_NEEDS_INSTALL &&
             opType != OP_NEEDS_UPGRADE &&
             selectedItem.getAttribute("compatible") != "false";
    case "cmd_disable":
      return selectedItem &&
            (opType == OP_NONE ||
             opType == OP_NEEDS_ENABLE);
    case "cmd_movetop":
      return selectedItem && (gExtensionsView.children[0] != selectedItem);
    case "cmd_moveup":
      return selectedItem && (gExtensionsView.children[0] != selectedItem);
    case "cmd_movedn":
      var children = gExtensionsView.children;
      return selectedItem && (children[children.length-1] != selectedItem);
#ifndef MOZ_PHOENIX
    case "cmd_install":
      return true;   
#endif
    }
    return false;
  },

  doCommand: function (aCommand)
  {
    if (this.isCommandEnabled(aCommand))
      this.commands[aCommand](gExtensionsView.selectedItem);
  },  
  
  onCommandUpdate: function ()
  {
    var extensionsCommands = document.getElementById("extensionsCommands");
    for (var i = 0; i < extensionsCommands.childNodes.length; ++i)
      this.updateCommand(extensionsCommands.childNodes[i]);
  },
  
  updateCommand: function (command) 
  {
    if (this.isCommandEnabled(command.id))
      command.removeAttribute("disabled");
    else
      command.setAttribute("disabled", "true");
  },
  
  commands: { 
    cmd_close: function (aSelectedItem)
    {
      closeWindow(true);
    },  
  
    cmd_useTheme: function (aSelectedItem)
    {
      var pref = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefBranch);
      gCurrentTheme = aSelectedItem.getAttribute("internalName");
      // Set this pref so the user can reset the theme in safe mode
      pref.setCharPref(PREF_EM_LAST_SELECTED_SKIN, gCurrentTheme);

      if (pref.getBoolPref(PREF_EXTENSIONS_DSS_ENABLED)) {
        pref.setCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN, gCurrentTheme);
      }
      else {
        // Theme change will happen on next startup, this flag tells
        // the Theme Manager that it needs to show "This theme will
        // be selected after a restart" text in the selected theme
        // item.
        pref.setBoolPref(PREF_EXTENSIONS_DSS_SWITCHPENDING, true);
        // Update the view
        setRestartMessage(aSelectedItem);
      }
      
      // disable the useThemeButton
      gExtensionsViewController.onCommandUpdate();
    },
      
    cmd_options: function (aSelectedItem)
    {
      if (!aSelectedItem) return;
      var optionsURL = aSelectedItem.getAttribute("optionsURL");
      if (optionsURL != "") {
        var features;
        try {
          var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                                 .getService(Components.interfaces.nsIPrefBranch);
          instantApply = prefs.getBoolPref("browser.preferences.instantApply");
          features = "chrome,titlebar,toolbar,centerscreen" + (instantApply ? ",dialog=no" : ",modal");
        }
        catch (e) {
          features = "chrome,titlebar,toolbar,centerscreen,modal";
        }
        openDialog(optionsURL, "", features);
      }
    },
    
    cmd_homepage: function (aSelectedItem)
    {
      if (!aSelectedItem) return;
      var homepageURL = aSelectedItem.getAttribute("homepageURL");
      if (homepageURL != "")
        openURL(homepageURL);
    },
    
    cmd_about: function (aSelectedItem)
    {
      if (!aSelectedItem) return;
      var aboutURL = aSelectedItem.getAttribute("aboutURL");
      if (aboutURL != "")
        openDialog(aboutURL, "", "chrome,modal");
      else
        openDialog("chrome://mozapps/content/extensions/about.xul", "", "chrome,modal", aSelectedItem.id, gExtensionsView.database);
    },  
    
    cmd_movetop: function (aSelectedItem)
    {
      var movingID = aSelectedItem.id;
      var moveTopID = gExtensionsView.children[0].id;
      gExtensionManager.moveToIndexOf(movingID, moveTopID);
      gExtensionsView.selectedItem = document.getElementById(movingID);
    },
    
    cmd_moveup: function (aSelectedItem)
    {
      var movingID = aSelectedItem.id;
      var moveAboveID = aSelectedItem.previousSibling.id;
      gExtensionManager.moveToIndexOf(movingID, moveAboveID);
      gExtensionsView.selectedItem = document.getElementById(movingID);
    },
    
    cmd_movedn: function (aSelectedItem)
    {
      var movingID = aSelectedItem.id;
      var moveBelowID = aSelectedItem.nextSibling.id;
      gExtensionManager.moveToIndexOf(movingID, moveBelowID);
      gExtensionsView.selectedItem = document.getElementById(movingID);
    },
    
    cmd_update: function (aSelectedItem)
    { 
      var items = [];
      if (!aSelectedItem)
        items = gExtensionManager.getItemList(gItemType, { });
      else {
        var id = getIDFromResourceURI(aSelectedItem.id);
        items = [gExtensionManager.getItemForID(id)];
      }
      var listener = new UpdateCheckListener();
      gExtensionManager.update(items, items.length, false, listener);
    },

    cmd_update_all: function (aSelectedItem)
    {
      var items = gExtensionManager.getItemList(gItemType, { });
      var listener = new UpdateCheckListener();
      gExtensionManager.update(items, items.length, false, listener);
    },

    cmd_uninstall: function (aSelectedItem)
    {
      // Confirm the uninstall
      var extensionsStrings = document.getElementById("extensionsStrings");
      var brandStrings = document.getElementById("brandStrings");

      var name = aSelectedItem.getAttribute("name");
      var title = extensionsStrings.getFormattedString("queryUninstallTitle", [name]);
      if (gWindowState == "extensions")
        var message = extensionsStrings.getFormattedString("queryUninstallExtensionMessage", [name, name]);
      else if (gWindowState == "themes")
        message = extensionsStrings.getFormattedString("queryUninstallThemeMessage", [name]);

      // XXXben - improve the wording on the buttons here!
      var promptSvc = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                .getService(Components.interfaces.nsIPromptService);
      if (!promptSvc.confirm(window, title, message))
        return;

      var selectedID = aSelectedItem.id;
      // if no next item, go to the previous one
      if (!gExtensionsView.goDown())
        gExtensionsView.goUp();

      if (gWindowState == "themes") {
        // If the theme being uninstalled is the current theme, we need to reselect
        // the default. 
        var pref = Components.classes["@mozilla.org/preferences-service;1"]
                             .getService(Components.interfaces.nsIPrefBranch);
        var currentTheme = pref.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN);
        if (aSelectedItem.getAttribute("internalName") == currentTheme)
          this.cmd_useTheme(document.getElementById(PREFIX_ITEM_URI + "{972ce4c6-7e08-4474-a285-3208198ce6fd}"));
      }
      gExtensionManager.uninstallItem(getIDFromResourceURI(selectedID));

    },

    cmd_disable: function (aSelectedItem)
    {
      gExtensionManager.disableItem(getIDFromResourceURI(aSelectedItem.id));
      gExtensionsView.selectedItem = document.getElementById(aSelectedItem.id);
    },
    
    cmd_enable: function (aSelectedItem)
    {
      gExtensionManager.enableItem(getIDFromResourceURI(aSelectedItem.id));
      gExtensionsView.selectedItem = document.getElementById(aSelectedItem.id);
#ifdef MOZ_PHOENIX
    }
  }
};
#else
    },

    cmd_install: function(aSelectedItem)
    {
      if (gWindowState == "extensions") 
        installExtension();
      else
        installSkin();
    }
  }
};

//////////////////////////////////////////////////////////////////////////
// functions to support installing of themes in apps other than firefox //
//////////////////////////////////////////////////////////////////////////
const nsIFilePicker = Components.interfaces.nsIFilePicker;
const nsIIOService = Components.interfaces.nsIIOService;
const nsIFileProtocolHandler = Components.interfaces.nsIFileProtocolHandler;
const nsIURL = Components.interfaces.nsIURL;

function installSkin()
{
  // 1) Prompt the user for the location of the theme to install. 
  var extensionsStrings = document.getElementById("extensionsStrings");

  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  fp.init(window, extensionsStrings.getString("installThemePickerTitle"), nsIFilePicker.modeOpen);


  fp.appendFilter(extensionsStrings.getString("themesFilter"), "*.jar");
  fp.appendFilters(nsIFilePicker.filterAll);

  var ret = fp.show();
  if (ret == nsIFilePicker.returnOK) 
  {
    var ioService = Components.classes['@mozilla.org/network/io-service;1'].getService(nsIIOService);
    var fileProtocolHandler =
    ioService.getProtocolHandler("file").QueryInterface(nsIFileProtocolHandler);
    var url = fileProtocolHandler.newFileURI(fp.file).QueryInterface(nsIURL);
    InstallTrigger.installChrome(InstallTrigger.SKIN, url.spec, decodeURIComponent(url.fileBaseName));
  }
}

function installExtension()
{
  var extensionsStrings = document.getElementById("extensionsStrings");

  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  fp.init(window, extensionsStrings.getString("installExtensionPickerTitle"), nsIFilePicker.modeOpen);
  
  fp.appendFilter(extensionsStrings.getString("extensionFilter"), "*.xpi");
  
  fp.appendFilters(nsIFilePicker.filterAll);

  var ret = fp.show();
  if (ret == nsIFilePicker.returnOK) 
  {
    var ioService = Components.classes['@mozilla.org/network/io-service;1'].getService(nsIIOService);
    var fileProtocolHandler =
    ioService.getProtocolHandler("file").QueryInterface(nsIFileProtocolHandler);
    var url = fileProtocolHandler.newFileURI(fp.file).QueryInterface(nsIURL);
    var xpi = {};
    xpi[decodeURIComponent(url.fileBaseName)] = url.spec;
    InstallTrigger.install(xpi);
  }
}
#endif
