///////////////////////////////////////////////////////////////////////////////
// Globals

const kObserverServiceProgID = "@mozilla.org/observer-service;1";
const nsIUpdateItem = Components.interfaces.nsIUpdateItem;

var gExtensionManager = null;
var gExtensionsView   = null;
var gWindowState      = "";
var gURIPrefix        = ""; // extension or theme prefix
var gDSRoot           = ""; // extension or theme root
var gGetMoreURL       = "";
var gCurrentTheme     = "";
var gDownloadManager  = null;
var gObserverIndex    = -1;

const PREF_APP_ID                           = "app.id";
const PREF_EXTENSIONS_GETMORETHEMESURL      = "extensions.getMoreThemesURL";
const PREF_EXTENSIONS_GETMOREEXTENSIONSURL  = "extensions.getMoreExtensionsURL";
const PREF_EM_LAST_SELECTED_SKIN            = "extensions.lastSelectedSkin";
const PREF_GENERAL_SKINS_SELECTEDSKIN       = "general.skins.selectedSkin";

///////////////////////////////////////////////////////////////////////////////
// Utility Functions 
function stripPrefix(aResourceURI)
{
  return aResourceURI.substr(gURIPrefix.length, aResourceURI.length);
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
  gWindowState = window.location.search.substr("?type=".length, window.location.search.length);
  
  var isExtensions = gWindowState == "extensions";
  gURIPrefix  = isExtensions ? "urn:mozilla:extension:" : "urn:mozilla:theme:";
  gDSRoot     = isExtensions ? "urn:mozilla:extension:root" : "urn:mozilla:theme:root";
  
  document.documentElement.setAttribute("windowtype", document.documentElement.getAttribute("windowtype") + "-" + gWindowState);

  gExtensionsView = document.getElementById("extensionsView");
  gExtensionsView.setAttribute("state", gWindowState);
  gExtensionManager = Components.classes["@mozilla.org/extensions/manager;1"]
                                .getService(Components.interfaces.nsIExtensionManager);
  
  // Extension Command Updating is handled by a command controller.
  gExtensionsView.controllers.appendController(gExtensionsViewController);

  // This persists the last-selected extension
  gExtensionsView.addEventListener("richview-select", onExtensionSelect, false);

  // Finally, update the UI. 
  gExtensionsView.database.AddDataSource(gExtensionManager.datasource);
  gExtensionsView.setAttribute("ref", gDSRoot);
  gExtensionsView.focus();
  
  var pref = Components.classes["@mozilla.org/preferences-service;1"]
                       .getService(Components.interfaces.nsIPrefBranch);
  if (!isExtensions) {
    gExtensionsView.addEventListener("richview-select", onThemeSelect, false);
    var cr = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                       .getService(Components.interfaces.nsIXULChromeRegistry);
    gCurrentTheme = cr.getSelectedSkin("global");
    
    var useThemeButton = document.getElementById("useThemeButton");
    useThemeButton.hidden = false;
  }
  
  // Restore the last-selected extension
  var lastSelected = gExtensionsView.getAttribute("last-selected");
  if (lastSelected != "")
    lastSelected = document.getElementById(lastSelected);
  if (!lastSelected) 
    gExtensionsView.selectionForward();
  else
    gExtensionsView.selected = lastSelected;

  var extensionsStrings = document.getElementById("extensionsStrings");
  document.documentElement.setAttribute("title", extensionsStrings.getString(gWindowState + "Title"));
  
  gExtensionsViewController.onCommandUpdate(); 
  
  gGetMoreURL = pref.getComplexValue(isExtensions ? PREF_EXTENSIONS_GETMOREEXTENSIONSURL 
                                                  : PREF_EXTENSIONS_GETMORETHEMESURL, 
                                     Components.interfaces.nsIPrefLocalizedString).data;
  gGetMoreURL = gGetMoreURL.replace(/%APPID%/g, pref.getCharPref(PREF_APP_ID));
  // Update various pieces of state-dependant UI
  var getMore = document.getElementById("getMore");
  getMore.setAttribute("value", getMore.getAttribute(isExtensions ? "valueextensions" : "valuethemes"));
  getMore.setAttribute("tooltiptext", getMore.getAttribute(isExtensions ? "tooltiptextextensions" : "tooltiptextthemes"));
  
  if (!isExtensions) {
    var themePreviewArea = document.getElementById("themePreviewArea");
    themePreviewArea.hidden = false;
    gExtensionsView.removeAttribute("flex");
  }
  
  // Set Initial Size
  var win = document.documentElement;
  if (!win.hasAttribute("width") || !win.hasAttribute("height")) {
#ifdef MOZ_THUNDERBIRD
    win.setAttribute("width", isExtensions ? 460 : 560);
#else
    win.setAttribute("width", isExtensions ? 400 : 500);
#endif
    win.setAttribute("height", isExtensions ? 300 : 380);
  }

  // Now look and see if we're being opened by XPInstall
  var gDownloadManager = new XPInstallDownloadManager();
  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Components.interfaces.nsIObserverService);
  os.addObserver(gDownloadManager, "xpinstall-download-started", false);

  gObserverIndex = gExtensionManager.addDownloadObserver(gDownloadManager);
  
  if ("arguments" in window) {
    try {
      var params = window.arguments[0].QueryInterface(Components.interfaces.nsIDialogParamBlock);
      gDownloadManager.addDownloads(params);
    }
    catch (e) { }
  }
  
  // Set the tooltips
  if (!isExtensions) {
    var extensionsStrings = document.getElementById("extensionsStrings");
#ifdef MOZ_THUNDERBIRD
    document.getElementById("installButton").setAttribute("tooltiptext", extensionsStrings.getString("cmdInstallTooltipTheme"));
#endif
    document.getElementById("uninstallButton").setAttribute("tooltiptext", extensionsStrings.getString("cmdUninstallTooltipTheme"));
    document.getElementById("updateButton").setAttribute("tooltiptext", extensionsStrings.getString("cmdUpdateTooltipTheme"));
  }
}

function Shutdown() 
{
  if (gWindowState != "extensions")
    gExtensionsView.removeEventListener("richview-select", onThemeSelect, false);
  
  gExtensionManager.removeDownloadObserverAt(gObserverIndex);

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
      item.init(url, " ", displayName, -1, url, iconURL, "", type);
      items.push(item);

      // Advance the enumerator
      var certName = aParams.GetString(i++);
    }
    
    gExtensionManager.addDownloads(items, items.length);
  },

  removeDownload: function (aEvent)
  {
  
  },
  
  /////////////////////////////////////////////////////////////////////////////  
  // nsIExtensionDownloadProgressListener
  onStateChange: function (aURL, aState, aValue)
  {
    const nsIXPIProgressDialog = Components.interfaces.nsIXPIProgressDialog;
    var element = document.getElementById(aURL);
    if (!element) return;
    switch (aState) {
    case nsIXPIProgressDialog.DOWNLOAD_START:
      element.setAttribute("state", "waiting");
      element.setAttribute("progress", "0");
      break;
    case nsIXPIProgressDialog.DOWNLOAD_DONE:
      element.setAttribute("progress", "100");
      break;
    case nsIXPIProgressDialog.INSTALL_START:
      element.setAttribute("state", "installing");
      break;
    case nsIXPIProgressDialog.INSTALL_DONE:
      dump("*** state change = " + aURL + ", state = " + aState + ", value = " + aValue + "\n");
      element.setAttribute("state", "done");
      var msg;
      if (aValue != 0) {
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

        var brandStrings = sbs.createBundle("chrome://global/locale/brand.properties");
        var brandShortName = brandStrings.GetStringFromName("brandShortName");
        var params = [brandShortName, aURL, msg];
        var message = extensionStrings.formatStringFromName("errorInstallMessage", params, params.length);
        
        var ps = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                           .getService(Components.interfaces.nsIPromptService);
        ps.alert(window, title, message);
        element.setAttribute("status", msg);

      }
      // Remove the dummy, since we installed successfully
      var type = gWindowState == "extensions" ? nsIUpdateItem.TYPE_EXTENSION 
                                              : nsIUpdateItem.TYPE_THEME;
      gExtensionManager.removeDownload(aURL, type);
      break;
    case nsIXPIProgressDialog.DIALOG_CLOSE:
      break;
    }
  },
  
  _urls: { },
  onProgress: function (aURL, aValue, aMaxValue)
  {
    var element = document.getElementById(aURL);
    if (!element) return;
    var percent = Math.round((aValue / aMaxValue) * 100);
    if (percent > 1 && !(aURL in this._urls)) {
      this._urls[aURL] = true;
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
    if (!aIID.equals(Components.interfaces.nsIExtensionDownloadProgressListener) &&
        !aIID.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

///////////////////////////////////////////////////////////////////////////////
//
// View Event Handlers
//
function onViewDoubleClick()
{
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
  if (!gExtensionsView.selected) {
    previewImageDeck.setAttribute("selectedIndex", "0");
    return;
  }
  var url = gExtensionsView.selected.getAttribute("previewImage");
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
                          "menuseparator_1", "menuitem_uninstall", "menuitem_update"];

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
  var name = document.popupNode.getAttribute("name");
  menuitem_about.setAttribute("label", extensionsStrings.getFormattedString("aboutExtension", [name]));
  
  if (isExtensions) {
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
  }
    
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Drag and Drop

var gExtensionsDNDObserver =
{
  _ioServ: null,
  _filePH: null,
  
  _ensureServices: function ()
  {
    if (!this._ioServ) {
      this._ioServ = Components.classes["@mozilla.org/network/io-service;1"]
                               .getService(Components.interfaces.nsIIOService);
      this._filePH = this._ioServ.getProtocolHandler("file")
                         .QueryInterface(Components.interfaces.nsIFileProtocolHandler);
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

      var fileURI = this._ioServ.newURI(fileURL, null, null);
      var url = fileURI.QueryInterface(Components.interfaces.nsIURL);
      if (url.fileExtension != "jar" && url.fileExtension != "xpi") {
        aDragSession.canDrop = false;
        break;
      }
    }
  },
  
  onDrop: function(aEvent, aXferData, aDragSession)
  {
    this._ensureServices();
    
    var xpinstallObj = {};
    var themes = {};
    var xpiCount = 0;
    var themeCount = 0;
    
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
      if (url.fileExtension == "xpi") {
        xpinstallObj[url.fileName] = fileURL;
        ++xpiCount;
      }
      else if (url.fileExtension == "jar") {
        themes[url.fileName] = fileURL;
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
    case "cmd_useTheme":
      return selectedItem && gCurrentTheme != selectedItem.getAttribute("internalName");
    case "cmd_options":
      return selectedItem && !selectedItem.disabled && selectedItem.getAttribute("optionsURL") != "";
    case "cmd_about":
      return !selectedItem || (selectedItem.disabled ? selectedItem.getAttribute("aboutURL") == "" : true);
    case "cmd_homepage":
      return (selectedItem && selectedItem.getAttribute("homepageURL") != "");
    case "cmd_uninstall":
      return selectedItem && selectedItem.getAttribute("locked") != "true";
    case "cmd_update":
      return true;
    case "cmd_enable":
      return selectedItem && selectedItem.disabled && !gExtensionManager.inSafeMode;
    case "cmd_disable":
      return selectedItem && selectedItem.getAttribute("locked") != "true" && !selectedItem.disabled;
    case "cmd_movetop":
      return (gExtensionsView.children[0] != selectedItem);
    case "cmd_moveup":
      return (gExtensionsView.children[0] != selectedItem);
    case "cmd_movedn":
      var children = gExtensionsView.children;
      return (children[children.length-1] != selectedItem);
#ifdef MOZ_THUNDERBIRD
    case "cmd_install":
      return true;   
#endif
    }
    return false;
  },

  doCommand: function (aCommand)
  {
    if (this.isCommandEnabled(aCommand))
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
  
    cmd_useTheme: function ()
    {
      var cr = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                         .getService(Components.interfaces.nsIXULChromeRegistry);
      var pref = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefBranch);
      gCurrentTheme = gExtensionsView.selected.getAttribute("internalName");
      var inUse = cr.isSkinSelected(gCurrentTheme , true);
      if (inUse == Components.interfaces.nsIChromeRegistry.FULL)
        return;
        
      pref.setCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN, gCurrentTheme);
      
      // Set this pref so the user can reset the theme in safe mode
      pref.setCharPref(PREF_EM_LAST_SELECTED_SKIN, gCurrentTheme);
      cr.selectSkin(gCurrentTheme, true);
      cr.refreshSkins();


      // disable the useThemeButton
      gExtensionsViewController.onCommandUpdate();
    },
      
    cmd_options: function ()
    {
      if (!gExtensionsView.selected) return;
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
      var movingID = gExtensionsView.selected.id;
      gExtensionManager.moveTop(stripPrefix(movingID));
      gExtensionsView.selected = document.getElementById(movingID);
    },
    
    cmd_moveup: function ()
    {
      var movingID = gExtensionsView.selected.id;
      gExtensionManager.moveUp(stripPrefix(movingID));
      gExtensionsView.selected = document.getElementById(movingID);
    },
    
    cmd_movedn: function ()
    {
      var movingID = gExtensionsView.selected.id;
      gExtensionManager.moveDown(stripPrefix(movingID));
      gExtensionsView.selected = document.getElementById(movingID);
    },
    
    cmd_update: function ()
    { 
      var id = gExtensionsView.selected ? stripPrefix(gExtensionsView.selected.id) : null;
      var itemType = gWindowState == "extensions" ? nsIUpdateItem.TYPE_EXTENSION : nsIUpdateItem.TYPE_THEME;
      var items = gExtensionManager.getItemList(id, itemType, { });
      var updates = Components.classes["@mozilla.org/updates/update-service;1"]
                              .getService(Components.interfaces.nsIUpdateService);
      updates.checkForUpdates(items, items.length, itemType, 
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
    },
    
    cmd_disable: function ()
    {
      gExtensionManager.disableExtension(stripPrefix(gExtensionsView.selected.id));
    },
    
    cmd_enable: function ()
    {
      gExtensionManager.enableExtension(stripPrefix(gExtensionsView.selected.id));
    },
#ifdef MOZ_THUNDERBIRD
    cmd_install: function()
    {
      if (gWindowState == "extensions") 
        installExtension();
      else
        installSkin();
    },
#endif
  }
};

#ifdef MOZ_THUNDERBIRD
///////////////////////////////////////////////////////////////
// functions to support installing of themes in thunderbird
///////////////////////////////////////////////////////////////
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
