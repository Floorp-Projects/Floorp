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
#   Robert Strong <robert.bugzilla@gmail.com>
#   Dão Gottwald <dao@design-noir.de>
#   Dave Townsend <dtownsend@oxymoronical.com>
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
const nsIExtensionManager    = Components.interfaces.nsIExtensionManager;
const nsIUpdateItem          = Components.interfaces.nsIUpdateItem;
const nsIFilePicker          = Components.interfaces.nsIFilePicker;
const nsIIOService           = Components.interfaces.nsIIOService;
const nsIFileProtocolHandler = Components.interfaces.nsIFileProtocolHandler;
const nsIURL                 = Components.interfaces.nsIURL;
const nsIAppStartup          = Components.interfaces.nsIAppStartup;

var gView             = null;
var gExtensionManager = null;
var gExtensionsView   = null;
var gExtensionStrings = null;
var gCurrentTheme     = "classic/1.0";
var gDefaultTheme     = "classic/1.0";
var gDownloadManager  = null;
var gObserverIndex    = -1;
var gInSafeMode       = false;
var gCheckCompat      = true;
var gCheckUpdateSecurity = true;
var gUpdatesOnly      = false;
var gAppID            = "";
var gPref             = null;
var gPriorityCount    = 0;
var gInstallCount     = 0;
var gPlugins          = null;
var gPluginsDS        = null;

const PREF_EM_CHECK_COMPATIBILITY           = "extensions.checkCompatibility";
const PREF_EM_CHECK_UPDATE_SECURITY         = "extensions.checkUpdateSecurity";
const PREF_EXTENSIONS_GETMORETHEMESURL      = "extensions.getMoreThemesURL";
const PREF_EXTENSIONS_GETMOREEXTENSIONSURL  = "extensions.getMoreExtensionsURL";
const PREF_EXTENSIONS_GETMOREPLUGINSURL     = "extensions.getMorePluginsURL";
const PREF_EXTENSIONS_DSS_ENABLED           = "extensions.dss.enabled";
const PREF_EXTENSIONS_DSS_SWITCHPENDING     = "extensions.dss.switchPending";
const PREF_EXTENSIONS_HIDE_INSTALL_BTN      = "extensions.hideInstallButton";
const PREF_EM_LAST_SELECTED_SKIN            = "extensions.lastSelectedSkin";
const PREF_GENERAL_SKINS_SELECTEDSKIN       = "general.skins.selectedSkin";
const PREF_UPDATE_NOTIFYUSER                = "extensions.update.notifyUser";

const RDFURI_ITEM_ROOT    = "urn:mozilla:item:root";
const PREFIX_ITEM_URI     = "urn:mozilla:item:";
const PREFIX_NS_EM        = "http://www.mozilla.org/2004/em-rdf#";
const kXULNSURI           = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const XMLURI_PARSE_ERROR  = "http://www.mozilla.org/newlayout/xml/parsererror.xml"

const OP_NONE                         = "";
const OP_NEEDS_INSTALL                = "needs-install";
const OP_NEEDS_UPGRADE                = "needs-upgrade";
const OP_NEEDS_UNINSTALL              = "needs-uninstall";
const OP_NEEDS_ENABLE                 = "needs-enable";
const OP_NEEDS_DISABLE                = "needs-disable";

///////////////////////////////////////////////////////////////////////////////
// Utility Functions
function setElementDisabledByID(aID, aDoDisable) {
  var element = document.getElementById(aID);
  if (element) {
    if (aDoDisable)
      element.setAttribute("disabled", "true");
    else
      element.removeAttribute("disabled");
  }
}

function getBrandShortName() {
  var brandStrings = document.getElementById("brandStrings");
  return brandStrings.getString("brandShortName");
}

function getExtensionString(key, strings) {
  if (strings)
    return gExtensionStrings.getFormattedString(key, strings);
  return gExtensionStrings.getString(key);
}

function MessageButton(aLabel, aAccesskey, aData) {
  this.label = aLabel;
  this.accessKey = aAccesskey;
  this.data = aData || "addons-message-dismiss";
}
MessageButton.prototype = {
  label: null,
  accessKey: null,
  data: null,

  callback: function (aNotification, aButton) {
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.notifyObservers(null, "addons-message-notification", aButton.data);
    aNotification.close();
    return true;
  }
};

function showMessage(aIconURL, aMessage, aButtonLabel, aButtonAccesskey,
                     aShowCloseButton, aNotifyData) {
  var addonsMsg = document.getElementById("addonsMsg");
  var buttons = null;
  if (aButtonLabel)
    buttons = [new MessageButton(aButtonLabel, aButtonAccesskey, aNotifyData)];
  var oldMessage = addonsMsg.getNotificationWithValue(aMessage);
  if (oldMessage && oldMessage.parentNode)
    addonsMsg.removeNotification(oldMessage);
  if (addonsMsg.currentNotification)
    gPriorityCount += 0.0001;
  else
    gPriorityCount = 0;
  addonsMsg.appendNotification(aMessage, aMessage, aIconURL,
                               addonsMsg.PRIORITY_WARNING_LOW + gPriorityCount,
                               buttons).hideclose = !aShowCloseButton;
}

// dynamically creates a template
var AddonsViewBuilder = {
  _bindingList: null,
  _actionList: null,

  // if aActionList is null aBindingList will be used to generate actions
  updateView: function(aRulesList, aURI, aBindingList, aActionList)
  {
    this._bindingList = aBindingList;
    this._actionList = aActionList ? aActionList : aBindingList;

    this.clearChildren(gExtensionsView);
    var template = document.createElementNS(kXULNSURI, "template");
    gExtensionsView.appendChild(template);
    for (var i = 0; i < aRulesList.length; ++i)
      template.appendChild(this.createRule(aRulesList[i], aURI));

    this._bindingList = null;
    this._actionList = null;
    gExtensionsView.builder.rebuild();
  },

  clearChildren: function(aEl)
  {
    while (aEl.hasChildNodes())
      aEl.removeChild(aEl.lastChild);
  },

  createRule: function(aTriplesList, aURI)
  {
    var rule = document.createElementNS(kXULNSURI, "rule");
    var conditions = document.createElementNS(kXULNSURI, "conditions");
    rule.appendChild(conditions);

    var content = document.createElementNS(kXULNSURI, "content");
    content.setAttribute("uri", "?uri");
    conditions.appendChild(content);

    var member = this.createMember("?uri", "?" + aURI);
    conditions.appendChild(member);

    for (var i = 0; i < aTriplesList.length; ++i)
      conditions.appendChild(this.createTriple("?" + aURI, PREFIX_NS_EM + aTriplesList[i][0], aTriplesList[i][1], aTriplesList[i][2]));

    var bindings = document.createElementNS(kXULNSURI, "bindings");
    rule.appendChild(bindings);
    for (i = 0; i < this._bindingList.length; ++i)
      bindings.appendChild(this.createBinding(this._bindingList[i], aURI));

    var action = document.createElementNS(kXULNSURI, "action");
    rule.appendChild(action);
    var extension = document.createElementNS(kXULNSURI, aURI);
    action.appendChild(extension);
    extension.setAttribute("uri", "?" + aURI);
    for (i = 0; i < this._actionList.length; ++i)
      extension.setAttribute(this._actionList[i][0], this._actionList[i][1]);

    return rule;
  },

  createTriple: function(aSubject, aPredicate, aObject, aParseType)
  {
    var triple = document.createElementNS(kXULNSURI, "triple");
    triple.setAttribute("subject", aSubject);
    triple.setAttribute("predicate", aPredicate);
    triple.setAttribute("object", aObject);
    if (aParseType)
      triple.setAttribute("parsetype", aParseType);
    return triple;
  },

  createBinding: function(aPredicateObjectList, aURI)
  {
    var binding = document.createElementNS(kXULNSURI, "binding");
    binding.setAttribute("subject", "?" + aURI);
    binding.setAttribute("predicate", PREFIX_NS_EM + aPredicateObjectList[0]);
    binding.setAttribute("object", aPredicateObjectList[1]);
    return binding;
  },

  createMember: function(aContainer, aChild)
  {
    var member = document.createElementNS(kXULNSURI, "member");
    member.setAttribute("container", aContainer);
    member.setAttribute("child", aChild);

    return member;
  }
};

function showView(aView) {
  if (gView == aView)
    return;

  updateLastSelected(aView);
  gView = aView;

  // Using disabled to represent add-on state in regards to the EM causes evil
  // focus behavior when used as an element attribute when the element isn't
  // really disabled.
  var bindingList = [ ["aboutURL", "?aboutURL"],
                      ["addonID", "?addonID"],
                      ["availableUpdateURL", "?availableUpdateURL"],
                      ["availableUpdateVersion", "?availableUpdateVersion"],
                      ["blocklisted", "?blocklisted"],
                      ["compatible", "?compatible"],
                      ["description", "?description"],
                      ["downloadURL", "?downloadURL"],
                      ["isDisabled", "?isDisabled"],
                      ["hidden", "?hidden"],
                      ["homepageURL", "?homepageURL"],
                      ["iconURL", "?iconURL"],
                      ["internalName", "?internalName"],
                      ["locked", "?locked"],
                      ["name", "?name"],
                      ["optionsURL", "?optionsURL"],
                      ["opType", "?opType"],
                      ["plugin", "?plugin"],
                      ["previewImage", "?previewImage"],
                      ["satisfiesDependencies", "?satisfiesDependencies"],
                      ["providesUpdatesSecurely", "?providesUpdatesSecurely"],
                      ["type", "?type"],
                      ["updateable", "?updateable"],
                      ["updateURL", "?updateURL"],
                      ["version", "?version"] ];

  var prefURL;
  var showInstallFile = true;
  try {
    showInstallFile = !gPref.getBoolPref(PREF_EXTENSIONS_HIDE_INSTALL_BTN);
  }
  catch (e) { }
  var showCheckUpdatesAll = true;
  var showInstallUpdatesAll = false;
  var showRestartApp = true;
  var showSkip = false;
  var showContinue = false;
  switch (aView) {
    case "extensions":
      prefURL = PREF_EXTENSIONS_GETMOREEXTENSIONSURL;
      var types = [ [ ["type", nsIUpdateItem.TYPE_EXTENSION, "Integer"] ] ];
      break;
    case "themes":
      prefURL = PREF_EXTENSIONS_GETMORETHEMESURL;
      types = [ [ ["type", nsIUpdateItem.TYPE_THEME, "Integer"] ] ];
      break;
    case "locales":
      types = [ [ ["type", nsIUpdateItem.TYPE_LOCALE, "Integer"] ] ];
      break;
    case "plugins":
      prefURL = PREF_EXTENSIONS_GETMOREPLUGINSURL;
      types = [ [ ["plugin", "true", null] ] ];
      break;
    case "updates":
      document.getElementById("updates-view").hidden = false;
      showInstallFile = false;
      showCheckUpdatesAll = false;
      showInstallUpdatesAll = true;
      if (gUpdatesOnly) {
        showSkip = true;
        showRestartApp = false;
      }
      bindingList = [ ["aboutURL", "?aboutURL"],
                      ["availableUpdateURL", "?availableUpdateURL"],
                      ["availableUpdateVersion", "?availableUpdateVersion"],
                      ["availableUpdateInfo", "?availableUpdateInfo"],
                      ["blocklisted", "?blocklisted"],
                      ["homepageURL", "?homepageURL"],
                      ["iconURL", "?iconURL"],
                      ["internalName", "?internalName"],
                      ["locked", "?locked"],
                      ["name", "?name"],
                      ["opType", "?opType"],
                      ["previewImage", "?previewImage"],
                      ["satisfiesDependencies", "?satisfiesDependencies"],
                      ["providesUpdatesSecurely", "?providesUpdatesSecurely"],
                      ["type", "?type"],
                      ["updateURL", "?updateURL"],
                      ["version", "?version"],
                      ["typeName", "update"] ];
      types = [ [ ["availableUpdateVersion", "?availableUpdateVersion", null],
                  ["updateable", "true", null] ] ];
      break;
    case "installs":
      document.getElementById("installs-view").hidden = false;
      showInstallFile = false;
      showCheckUpdatesAll = false;
      showInstallUpdatesAll = false;
      if (gUpdatesOnly) {
        showContinue = true;
        showRestartApp = false;
      }
      bindingList = [ ["aboutURL", "?aboutURL"],
                      ["addonID", "?addonID"],
                      ["availableUpdateURL", "?availableUpdateURL"],
                      ["availableUpdateVersion", "?availableUpdateVersion"],
                      ["blocklisted", "?blocklisted"],
                      ["compatible", "?compatible"],
                      ["description", "?description"],
                      ["downloadURL", "?downloadURL"],
                      ["incompatibleUpdate", "?incompatibleUpdate"],
                      ["isDisabled", "?isDisabled"],
                      ["hidden", "?hidden"],
                      ["homepageURL", "?homepageURL"],
                      ["iconURL", "?iconURL"],
                      ["internalName", "?internalName"],
                      ["locked", "?locked"],
                      ["name", "?name"],
                      ["optionsURL", "?optionsURL"],
                      ["opType", "?opType"],
                      ["previewImage", "?previewImage"],
                      ["progress", "?progress"],
                      ["state", "?state"],
                      ["type", "?type"],
                      ["updateable", "?updateable"],
                      ["updateURL", "?updateURL"],
                      ["version", "?version"],
                      ["newVersion", "?newVersion"],
                      ["typeName", "install"] ];
      types = [ [ ["state", "?state", null] ] ];
      break;
  }

  var showGetMore = false;
  var getMore = document.getElementById("getMore");
  if (prefURL) {
    try {
      getMore.setAttribute("value", getMore.getAttribute("value" + aView));
      var getMoreURL = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"]
                                 .getService(Components.interfaces.nsIURLFormatter)
                                 .formatURLPref(prefURL);
      getMore.setAttribute("getMoreURL", getMoreURL);
      showGetMore = getMoreURL == "about:blank" ? false : true;
    }
    catch (e) { }
  }
  getMore.hidden = !showGetMore;

  var isThemes = aView == "themes";

  if (aView == "themes" || aView == "extensions") {
    var el = document.getElementById("installFileButton");
    el.setAttribute("tooltiptext", el.getAttribute(isThemes ? "tooltiptextthemes" :
                                                              "tooltiptextaddons"));
    el = document.getElementById("checkUpdatesAllButton");
    el.setAttribute("tooltiptext", el.getAttribute(isThemes ? "tooltiptextthemes" :
                                                              "tooltiptextaddons"));
  }

  document.getElementById("installFileButton").hidden = !showInstallFile;
  document.getElementById("checkUpdatesAllButton").hidden = !showCheckUpdatesAll;
  document.getElementById("installUpdatesAllButton").hidden = !showInstallUpdatesAll;
  document.getElementById("restartAppButton").hidden = !showRestartApp;
  document.getElementById("skipDialogButton").hidden = !showSkip;
  document.getElementById("continueDialogButton").hidden = !showContinue;
  document.getElementById("themePreviewArea").hidden = !isThemes;
  document.getElementById("themeSplitter").hidden = !isThemes;
  document.getElementById("showUpdateInfoButton").hidden = aView != "updates";
  document.getElementById("hideUpdateInfoButton").hidden = true;

  AddonsViewBuilder.updateView(types, "richlistitem", bindingList, null);

  if (aView == "updates" || aView == "installs")
    gExtensionsView.selectedItem = gExtensionsView.children[0];

  if (showSkip) {
    var button = document.getElementById("installUpdatesAllButton");
    button.setAttribute("default", "true");
    window.setTimeout(function () { button.focus(); }, 0);
  } else
    document.getElementById("installUpdatesAllButton").removeAttribute("default");

  if (showContinue)
    document.getElementById("continueDialogButton").setAttribute("default", "true");
  else
    document.getElementById("continueDialogButton").removeAttribute("default");

  if (isThemes)
    onAddonSelect();
  updateGlobalCommands();
}

// manages the last-selected attribute for the view buttons and richlistbox
function updateLastSelected(aView) {
  var viewGroup = document.getElementById("viewGroup");
  if (viewGroup.hasAttribute("last-selected"))
    var lastSelectedView = viewGroup.getAttribute("last-selected");

  if (lastSelectedView && lastSelectedView == gView) {
    var lastViewButton = document.getElementById(lastSelectedView + "-view");
    if (lastViewButton.hasAttribute("persist")) {
      if (gExtensionsView.selectedItem)
        lastViewButton.setAttribute("last-selected", gExtensionsView.selectedItem.id);
      else if (lastViewButton.hasAttribute("last-selected"))
        lastViewButton.removeAttribute("last-selected");
    }
  }
  var viewButton = document.getElementById(aView + "-view");
  if (viewButton.hasAttribute("last-selected")) {
    gExtensionsView.setAttribute("last-selected", viewButton.getAttribute("last-selected"));
  }
  else if (gExtensionsView.hasAttribute("last-selected")) {
    gExtensionsView.clearSelection();
    gExtensionsView.removeAttribute("last-selected");
  }
  viewGroup.selectedItem = viewButton;
  // Only set last-selected for view buttons that have a persist attribute
  // (e.g. they themselves persist the last selected add-on in the view).
  // This prevents opening updates and installs views when they were the last
  // view selected and instead this will open the previously selected
  // extensions, themes, locales, plug-ins, etc. view.
  if (viewButton.hasAttribute("persist"))
    viewGroup.setAttribute("last-selected", aView);
}

function LOG(msg) {
  dump("*** " + msg + "\n");
}

function getIDFromResourceURI(aURI)
{
  if (aURI.substring(0, PREFIX_ITEM_URI.length) == PREFIX_ITEM_URI)
    return aURI.substring(PREFIX_ITEM_URI.length);
  return aURI;
}

function showProgressBar() {
  var progressBox = document.getElementById("progressBox");
  var height = document.defaultView.getComputedStyle(progressBox.parentNode, "")
                       .getPropertyValue("height");
  progressBox.parentNode.style.height = height;
  document.getElementById("viewGroup").hidden = true;
  progressBox.hidden = false;
}

function flushDataSource()
{
  var rds = gExtensionManager.datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
  if (rds)
    rds.Flush();
}

function noUpdatesDismiss(aEvent)
{
  window.removeEventListener("select", noUpdatesDismiss, true);

  var children = gExtensionsView.children;
  for (var i = 0; i < children.length; ++i) {
    var child = children[i];
    if (child.hasAttribute("updateStatus"))
      child.removeAttribute("updateStatus");
  }

  if (aEvent.target.localName != "notification")
    document.getElementById("addonsMsg").removeCurrentNotification();
}

function setRestartMessage(aItem)
{
  var themeName = aItem.getAttribute("name");
  var restartMessage = getExtensionString("dssSwitchAfterRestart",
                                          [getBrandShortName()]);
  var children = gExtensionsView.children;
  for (var i = 0; i < children.length; ++i) {
    var item = children[i];
    if (item.hasAttribute("oldDescription")) {
      item.setAttribute("description", item.getAttribute("oldDescription"));
      item.removeAttribute("oldDescription");
    }
  }
  aItem.setAttribute("oldDescription", aItem.getAttribute("description"));
  aItem.setAttribute("description", restartMessage);
}

function initPluginsDS()
{
  var phs = Components.classes["@mozilla.org/plugin/host;1"]
                      .getService(Components.interfaces.nsIPluginHost);
  var plugins = phs.getPluginTags({ });
  var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                      .getService(Components.interfaces.nsIRDFService);
  var rdfCU = Components.classes["@mozilla.org/rdf/container-utils;1"]
                        .getService(Components.interfaces.nsIRDFContainerUtils);
  gPluginsDS = Components.classes["@mozilla.org/rdf/datasource;1?name=in-memory-datasource"]
                         .createInstance(Components.interfaces.nsIRDFDataSource);
  var rootctr = rdfCU.MakeSeq(gPluginsDS, rdf.GetResource(RDFURI_ITEM_ROOT));
  gPlugins = { };

  // Case insensitive sort
  function compare(a, b) {
    if (a.name.toLowerCase() < b.name.toLowerCase())
      return -1;
    if (a.name.toLowerCase() > b.name.toLowerCase())
      return 1;
    return 0;
  }
  plugins.sort(compare);

  for (var i = 0; i < plugins.length; i++) {
    var plugin = plugins[i];
    var name = plugin.name;
    if (!(name in gPlugins))
      gPlugins[name] = { };

    // Removes all html markup in a plugin's description
    var desc = plugin.description.replace(/<\/?[a-z][^>]*>/gi, " ");
    if (!(desc in gPlugins[name])) {
      var homepageURL = null;
      // Some plugins (e.g. QuickTime) add an anchor to their description to
      // provide a link to the plugin's homepage in about:plugins. This can be
      // used to provide access to a plugins homepage in the add-ons mgr.
      if (/<A\s+HREF=[^>]*>/i.test(plugin.description))
        homepageURL = /<A\s+HREF=["']?([^>"'\s]*)/i.exec(plugin.description)[1];

      gPlugins[name][desc] = { filename    : plugin.filename,
                               homepageURL : homepageURL,
                               disabled    : plugin.disabled,
                               blocklisted : plugin.blocklisted,
                               plugins     : [] };
    }
    gPlugins[name][desc].plugins.push(plugin);
  }

  for (var pluginName in gPlugins) {
    for (var pluginDesc in gPlugins[pluginName]) {
      plugin = gPlugins[pluginName][pluginDesc];
      var pluginNode = rdf.GetResource(PREFIX_ITEM_URI + plugin.filename);
      rootctr.AppendElement(pluginNode);
      gPluginsDS.Assert(pluginNode,
                        rdf.GetResource(PREFIX_NS_EM + "name"),
                        rdf.GetLiteral(pluginName),
                        true);
      gPluginsDS.Assert(pluginNode,
                        rdf.GetResource(PREFIX_NS_EM + "addonID"),
                        rdf.GetLiteral(plugin.filename),
                        true);
      gPluginsDS.Assert(pluginNode,
                        rdf.GetResource(PREFIX_NS_EM + "description"),
                        rdf.GetLiteral(pluginDesc),
                        true);
      if (plugin.homepageURL)
        gPluginsDS.Assert(pluginNode,
                          rdf.GetResource(PREFIX_NS_EM + "homepageURL"),
                          rdf.GetLiteral(plugin.homepageURL),
                          true);
      gPluginsDS.Assert(pluginNode,
                        rdf.GetResource(PREFIX_NS_EM + "isDisabled"),
                        rdf.GetLiteral(plugin.disabled ? "true" : "false"),
                        true);
      gPluginsDS.Assert(pluginNode,
                        rdf.GetResource(PREFIX_NS_EM + "blocklisted"),
                        rdf.GetLiteral(plugin.blocklisted ? "true" : "false"),
                        true);
      gPluginsDS.Assert(pluginNode,
                        rdf.GetResource(PREFIX_NS_EM + "compatible"),
                        rdf.GetLiteral("true"),
                        true);
      gPluginsDS.Assert(pluginNode,
                        rdf.GetResource(PREFIX_NS_EM + "plugin"),
                        rdf.GetLiteral("true"),
                        true);
    }
  }
}

function togglePluginDisabled(aName, aDesc)
{
  var plugin = gPlugins[aName][aDesc];
  plugin.disabled = !plugin.disabled;
  for (var i = 0; i < plugin.plugins.length; ++i)
    plugin.plugins[i].disabled = plugin.disabled;
  var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                      .getService(Components.interfaces.nsIRDFService);
  gPluginsDS.Change(rdf.GetResource(PREFIX_ITEM_URI + plugin.filename),
                    rdf.GetResource(PREFIX_NS_EM + "isDisabled"),
                    rdf.GetLiteral(plugin.disabled ? "false" : "true"),
                    rdf.GetLiteral(plugin.disabled ? "true" : "false"));
  gExtensionsViewController.onCommandUpdate();
  gExtensionsView.selectedItem.focus();
}

///////////////////////////////////////////////////////////////////////////////
// Startup, Shutdown
function Startup()
{
  gExtensionStrings = document.getElementById("extensionsStrings");
  gPref = Components.classes["@mozilla.org/preferences-service;1"]
                    .getService(Components.interfaces.nsIPrefBranch);
  var defaultPref = gPref.QueryInterface(Components.interfaces.nsIPrefService)
                         .getDefaultBranch(null);
  try {
    gCurrentTheme = gPref.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN);
    gDefaultTheme = defaultPref.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN);
  }
  catch (e) { }

  gExtensionsView = document.getElementById("extensionsView");
  gExtensionManager = Components.classes["@mozilla.org/extensions/manager;1"]
                                .getService(nsIExtensionManager);
  var appInfo = Components.classes["@mozilla.org/xre/app-info;1"]
                          .getService(Components.interfaces.nsIXULAppInfo)
                          .QueryInterface(Components.interfaces.nsIXULRuntime);
  gInSafeMode = appInfo.inSafeMode;
  gAppID = appInfo.ID;

  try {
    gCheckCompat = gPref.getBoolPref(PREF_EM_CHECK_COMPATIBILITY);
  } catch(e) { }

  try {
    gCheckUpdateSecurity = gPref.getBoolPref(PREF_EM_CHECK_UPDATE_SECURITY);
  } catch(e) { }

  // Sort on startup and anytime an add-on is installed or upgraded.
  gExtensionManager.sortTypeByProperty(nsIUpdateItem.TYPE_ADDON, "name", true);
  // Extension Command Updating is handled by a command controller.
  gExtensionsView.controllers.appendController(gExtensionsViewController);
  gExtensionsView.addEventListener("select", onAddonSelect, false);

  initPluginsDS();
  gExtensionsView.database.AddDataSource(gPluginsDS);
  gExtensionsView.database.AddDataSource(gExtensionManager.datasource);
  gExtensionsView.setAttribute("ref", RDFURI_ITEM_ROOT);
  updateOptionalViews();

  var viewGroup = document.getElementById("viewGroup");

  gExtensionsView.focus();
  gExtensionsViewController.onCommandUpdate();

  // Now look and see if we're being opened by XPInstall
  gDownloadManager = new XPInstallDownloadManager();
  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Components.interfaces.nsIObserverService);
  os.addObserver(gDownloadManager, "xpinstall-download-started", false);
  os.addObserver(gAddonsMsgObserver, "addons-message-notification", false);

  gObserverIndex = gExtensionManager.addUpdateListener(gDownloadManager);

  if (!gCheckCompat) {
    var msgText = getExtensionString("disabledCompatMsg");
    var buttonLabel = getExtensionString("enableButtonLabel");
    var buttonAccesskey = getExtensionString("enableButtonAccesskey");
    var notifyData = "addons-enable-compatibility";
    showMessage("chrome://mozapps/skin/extensions/question.png",
                msgText, buttonLabel, buttonAccesskey,
                true, notifyData);
  }
  if (!gCheckUpdateSecurity) {
    var defaultCheckSecurity = true;
    try {
      defaultCheckSecurity = defaultPref.getBoolPref(PREF_EM_CHECK_UPDATE_SECURITY);
    } catch (e) { }

    // App has update security checking enabled by default so show warning
    if (defaultCheckSecurity) {
      var msgText = getExtensionString("disabledUpdateSecurityMsg");
      var buttonLabel = getExtensionString("enableButtonLabel");
      var buttonAccesskey = getExtensionString("enableButtonAccesskey");
      var notifyData = "addons-enable-updatesecurity";
      showMessage("chrome://mozapps/skin/extensions/question.png",
                  msgText, buttonLabel, buttonAccesskey,
                  true, notifyData);
    }
  }
  if (gInSafeMode) {
    showMessage("chrome://mozapps/skin/extensions/question.png",
                getExtensionString("safeModeMsg"),
                null, null, true, null);
  }

  if ("arguments" in window) {
    try {
      var params = window.arguments[0].QueryInterface(Components.interfaces.nsIDialogParamBlock);
      showView("installs");
      gDownloadManager.addDownloads(params);
    }
    catch (e) {
      if (window.arguments[0] == "updates-only") {
        gUpdatesOnly = true;
#ifdef MOZ_PHOENIX
        // If we are Firefox when updating on startup don't display context
        // menuitems that can open a browser window.
        gUpdateContextMenus = gUpdateContextMenusNoBrowser;
#endif
        document.getElementById("viewGroup").hidden = true;
        document.getElementById("extensionsView").setAttribute("norestart", "");
        showView("updates");
        showMessage("chrome://mozapps/skin/extensions/question.png",
                    getExtensionString("newUpdatesAvailableMsg"),
                    null, null, true, null);
        document.title = getExtensionString("newUpdateWindowTitle", [getBrandShortName()]);
      }
      else
        showView(window.arguments[0]);
    }
  }
  else if (viewGroup.hasAttribute("last-selected") &&
           !document.getElementById(viewGroup.getAttribute("last-selected") + "-view").hidden)
    showView(viewGroup.getAttribute("last-selected"));
  else
    showView("extensions");

  if (gExtensionsView.selectedItem)
    gExtensionsView.scrollBoxObject.scrollToElement(gExtensionsView.selectedItem);

  gPref.setBoolPref(PREF_UPDATE_NOTIFYUSER, false);

  if (gUpdatesOnly && gExtensionsView.children.length == 0)
    window.close();
}

function Shutdown()
{
  gPref = null;
  gExtensionsView.removeEventListener("select", onAddonSelect, false);
  gExtensionsView.database.RemoveDataSource(gExtensionManager.datasource);

  gExtensionManager.removeUpdateListenerAt(gObserverIndex);

  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Components.interfaces.nsIObserverService);
  os.removeObserver(gAddonsMsgObserver, "addons-message-notification");
  os.removeObserver(gDownloadManager, "xpinstall-download-started");
  var currentNotification = document.getElementById("addonsMsg").currentNotification;
  if (currentNotification && currentNotification.value == "addons-no-updates")
    window.removeEventListener("select", noUpdatesDismiss, true);
}

///////////////////////////////////////////////////////////////////////////////
//
// XPInstall
//

function getURLSpecFromFile(aFile)
{
  var ioServ = Components.classes["@mozilla.org/network/io-service;1"]
                         .getService(nsIIOService);
  var fph = ioServ.getProtocolHandler("file").QueryInterface(Components.interfaces.nsIFileProtocolHandler);
  return fph.getURLSpecFromFile(aFile);
}

function XPInstallDownloadManager()
{
  this._statusFormatKBMB  = getExtensionString("statusFormatKBMB");
  this._statusFormatKBKB  = getExtensionString("statusFormatKBKB");
  this._statusFormatMBMB  = getExtensionString("statusFormatMBMB");
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
        showView("installs");
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
    var items = [];
    for (var i = 0; i < numXPInstallItems;) {
      var displayName = aParams.GetString(i++);
      var url = aParams.GetString(i++);
      var iconURL = aParams.GetString(i++);
      var uri = Components.classes["@mozilla.org/network/io-service;1"]
                          .getService(nsIIOService).newURI(url, null, null);
      var isTheme = uri.QueryInterface(nsIURL).fileExtension.toLowerCase() == "jar";
      if (!iconURL) {
        iconURL = isTheme ? "chrome://mozapps/skin/extensions/themeGeneric.png" :
                            "chrome://mozapps/skin/xpinstall/xpinstallItemGeneric.png";
      }

      var type = isTheme ? nsIUpdateItem.TYPE_THEME : nsIUpdateItem.TYPE_EXTENSION;
      var item = Components.classes["@mozilla.org/updates/item;1"]
                           .createInstance(Components.interfaces.nsIUpdateItem);
      item.init(url, " ", "app-profile", "", "", displayName, url, "", iconURL, "", "", type, "");
      items.push(item);

      // Advance the enumerator
      var certName = aParams.GetString(i++);
    }

    gExtensionManager.addDownloads(items, items.length, false);
    updateOptionalViews();
    updateGlobalCommands();
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
    switch (aState) {
      case nsIXPIProgressDialog.DOWNLOAD_START:
      case nsIXPIProgressDialog.DOWNLOAD_DONE:
      case nsIXPIProgressDialog.INSTALL_START:
        break;
      case nsIXPIProgressDialog.INSTALL_DONE:
        gInstallCount--;
        if (gInstallCount == 0)
          updateGlobalCommands();

        // From nsInstall.h
        // SUCCESS        = 0
        // REBOOT_NEEDED  = 999
        // USER_CANCELLED = -210
        if (aValue == 0 || aValue == 999 || aValue == -210)
          break;

        var xpinstallStrings = document.getElementById("xpinstallStrings");
        try {
          var msg = xpinstallStrings.getString("error" + aValue);
        }
        catch (e) {
          msg = xpinstallStrings.getFormattedString("unknown.error", [aValue]);
        }
        var title = getExtensionString("errorInstallTitle");
        var message = getExtensionString("errorInstallMsg", [getBrandShortName(),
                                                             aAddon.xpiURL, msg]);
        var ps = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                           .getService(Components.interfaces.nsIPromptService);
        ps.alert(window, title, message + "\n" + aValue);
        break;
      case nsIXPIProgressDialog.DIALOG_CLOSE:
        // Sort on startup and anytime an add-on is installed or upgraded.
        gExtensionManager.sortTypeByProperty(nsIUpdateItem.TYPE_ADDON, "name", true);
        // XXXrstrong - installs may be made compatible after this notification
        // see bug 351819
        // For updates on startup always enable the continue button after the
        // update has completed.
        if (gUpdatesOnly) {
          setElementDisabledByID("cmd_continue", false);
          document.getElementById("continueDialogButton").focus();
        }
        else
          updateGlobalCommands();
        break;
    }
  },

  _urls: { },
  onProgress: function (aAddon, aValue, aMaxValue)
  {
    var element = this.getElementForAddon(aAddon);
    if (!element)
      return;
    var percent = Math.round((aValue / aMaxValue) * 100);
    if (percent > 1 && !(aAddon.xpiURL in this._urls))
      this._urls[aAddon.xpiURL] = true;

    var KBProgress = parseInt(aValue/1024 + .5);
    var KBTotal = parseInt(aMaxValue/1024 + .5);
    var statusPrevious = element.getAttribute("status");
    var statusCurrent = this._formatKBytes(KBProgress, KBTotal);
    if (statusCurrent != statusPrevious)
      element.setAttribute("status", statusCurrent);
  },

  _replaceInsert: function ( text, index, value )
  {
    return text.replace("#"+index, value);
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
}
UpdateCheckListener.prototype = {
  _updateFound: false,
  _totalCount: 0,
  _completedCount: 0,

  /**
   * See nsIExtensionManager.idl
   */
  onUpdateStarted: function() {
    gExtensionsView.setAttribute("update-operation", "checking");
    gExtensionsViewController.onCommandUpdate();
    updateGlobalCommands();
    this._totalCount = gExtensionsView.children.length;
  },

  /**
   * See nsIExtensionManager.idl
   */
  onUpdateEnded: function() {
    if (!document)
      return;
    document.getElementById("progressBox").hidden = true;
    var viewGroup = document.getElementById("viewGroup");
    viewGroup.hidden = false;
    gExtensionsView.removeAttribute("update-operation");
    gExtensionsViewController.onCommandUpdate();
    updateGlobalCommands();
    if (this._updateFound)
      showView("updates");
    else {
      showMessage("chrome://mozapps/skin/extensions/question.png",
                  getExtensionString("noUpdatesMsg"),
                  null, null, true, "addons-no-updates");
      window.addEventListener("select", noUpdatesDismiss, true);
    }
  },

  /**
   * See nsIExtensionManager.idl
   */
  onAddonUpdateStarted: function(addon) {
    if (!document)
      return;
    var element = document.getElementById(PREFIX_ITEM_URI + addon.id);
    element.setAttribute("loading", "true");
    element.setAttribute("updateStatus", getExtensionString("updatingMsg"));
  },

  /**
   * See nsIExtensionManager.idl
   */
  onAddonUpdateEnded: function(addon, status) {
    if (!document)
      return;
    var element = document.getElementById(PREFIX_ITEM_URI + addon.id);
    element.removeAttribute("loading");
    const nsIAUCL = Components.interfaces.nsIAddonUpdateCheckListener;
    switch (status) {
    case nsIAUCL.STATUS_UPDATE:
      var statusMsg = getExtensionString("updateAvailableMsg", [addon.version]);
      this._updateFound = true;
      break;
    case nsIAUCL.STATUS_VERSIONINFO:
      statusMsg = getExtensionString("updateCompatibilityMsg");
      break;
    case nsIAUCL.STATUS_FAILURE:
      var name = element.getAttribute("name");
      statusMsg = getExtensionString("updateErrorMessage", [name]);
      break;
    case nsIAUCL.STATUS_DISABLED:
      name = element.getAttribute("name");
      statusMsg = getExtensionString("updateDisabledMessage", [name]);
      break;
    case nsIAUCL.STATUS_APP_MANAGED:
    case nsIAUCL.STATUS_NO_UPDATE:
      statusMsg = getExtensionString("updateNoUpdateMsg");
      break;
    case nsIAUCL.STATUS_NOT_MANAGED:
      statusMsg = getExtensionString("updateNotManagedMessage", [getBrandShortName()]);
      break;
    case nsIAUCL.STATUS_READ_ONLY:
      statusMsg = getExtensionString("updateReadOnlyMessage");
      break;
    default:
      statusMsg = getExtensionString("updateNoUpdateMsg");
    }
    element.setAttribute("updateStatus", statusMsg);
    ++this._completedCount;
    document.getElementById("progressStatus").value = getExtensionString("finishedUpdateCheck", [addon.name]);
    document.getElementById("addonsProgress").value = Math.ceil((this._completedCount / this._totalCount) * 100);
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
  if (aEvent.button != 0 || !gExtensionsView.selectedItem)
    return;

  switch (gView) {
    case "extensions":
      gExtensionsViewController.doCommand('cmd_options');
      break;
    case "themes":
      gExtensionsViewController.doCommand('cmd_useTheme');
      break;
    case "updates":
      gExtensionsViewController.doCommand('cmd_includeUpdate');
      break;
  }
}

function onAddonSelect(aEvent)
{
  var viewButton = document.getElementById("viewGroup").selectedItem;
  if (viewButton.hasAttribute("persist") && gExtensionsView.selectedItem)
    viewButton.setAttribute("last-selected", gExtensionsView.selectedItem.id);

  if (!document.getElementById("themePreviewArea").hidden) {
    var previewImageDeck = document.getElementById("previewImageDeck");
    if (gView == "themes") {
      var previewImage = document.getElementById("previewImage");
      if (!gExtensionsView.selectedItem) {
        previewImageDeck.selectedIndex = 0;
        if (previewImage.hasAttribute("src"))
          previewImage.removeAttribute("src");
      }
      else {
        var url = gExtensionsView.selectedItem.getAttribute("previewImage");
        if (url) {
          previewImageDeck.selectedIndex = 2;
          previewImage.setAttribute("src", url);
        }
        else {
          previewImageDeck.selectedIndex = 1;
          if (previewImage.hasAttribute("src"))
            previewImage.removeAttribute("src");
        }
      }
    }
    else if (gView == "updates") {
      UpdateInfoLoader.cancelLoad();
      if (!gExtensionsView.selectedItem)
        previewImageDeck.selectedIndex = 3;
      else if (!gExtensionsView.selectedItem.hasAttribute("availableUpdateInfo"))
        previewImageDeck.selectedIndex = 4;
      else
        UpdateInfoLoader.loadInfo(gExtensionsView.selectedItem.getAttribute("availableUpdateInfo"));
    }
  }
}

/**
 * Manages the retrieval of update information and the xsl stylesheet
 * used to format the inforation into chrome.
 */
var UpdateInfoLoader = {
  _stylesheet: null,
  _styleRequest: null,
  _infoDocument: null,
  _infoRequest: null,
  
  // Called once both stylesheet and info requests have completed
  displayInfo: function()
  {
    var processor = Components.classes["@mozilla.org/document-transformer;1?type=xslt"]
                              .createInstance(Components.interfaces.nsIXSLTProcessor);
    processor.flags |= Components.interfaces.nsIXSLTProcessorPrivate.DISABLE_ALL_LOADS;
    
    processor.importStylesheet(this._stylesheet);
    var fragment = processor.transformToFragment(this._infoDocument, document);
    document.getElementById("infoDisplay").appendChild(fragment);
    document.getElementById("previewImageDeck").selectedIndex = 7;
  },
  
  onStylesheetLoaded: function(event)
  {
    var request = event.target;
    this._styleRequest = null;
    this._stylesheet = request.responseXML;

    if (!this._stylesheet ||
        this._stylesheet.documentElement.namespaceURI == XMLURI_PARSE_ERROR ||
        (request.status != 200 && request.status != 0)) {
      // The stylesheet load failing is a bad sign
      document.getElementById("previewImageDeck").selectedIndex = 6;
      return;
    }

    if (this._infoDocument)
      this.displayInfo();
  },
  
  onInfoLoaded: function(event)
  {
    var request = event.target;
    this._infoRequest = null;
    this._infoDocument = request.responseXML;
    
    if (!this._infoDocument ||
        this._infoDocument.documentElement.namespaceURI == XMLURI_PARSE_ERROR ||
        (request.status != 200 && request.status != 0)) {
      // Should attempt to parse request.responseText with the html parser
      document.getElementById("previewImageDeck").selectedIndex = 6;
      return;
    }

    if (this._stylesheet)
      this.displayInfo();
  },
  
  onError: function(event)
  {
    if (event.request == this._infoRequest)
      this._infoRequest = null;
    else // Means the stylesheet load has failed which is pretty bad news
      this.cancelRequest();

    document.getElementById("previewImageDeck").selectedIndex = 6;
  },
  
  loadInfo: function(url)
  {
    this.cancelLoad();
    this._infoDocument = null;
    document.getElementById("previewImageDeck").selectedIndex = 5;
    var display = document.getElementById("infoDisplay");
    while (display.lastChild)
      display.removeChild(display.lastChild);

    this._infoRequest = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                                  .createInstance(Components.interfaces.nsIXMLHttpRequest);
    this._infoRequest.open("GET", url, true);
    
    var self = this;
    this._infoRequest.onerror = function(event) { self.onError(event); };
    this._infoRequest.onload = function(event) { self.onInfoLoaded(event); };
    this._infoRequest.send(null);

    // We may have the stylesheet cached from a previous load, or may still be
    // loading it.
    if (this._stylesheet || this._styleRequest)
      return;

    this._styleRequest = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                                   .createInstance(Components.interfaces.nsIXMLHttpRequest);
    this._styleRequest.open("GET", "chrome://mozapps/content/extensions/updateinfo.xsl", true);
    this._styleRequest.overrideMimeType("text/xml");
    
    this._styleRequest.onerror = function(event) { self.onError(event); };
    this._styleRequest.onload = function(event) { self.onStylesheetLoaded(event); };
    this._styleRequest.send(null);
  },
  
  cancelLoad: function()
  {
    // Leave the stylesheet loader running, there's a good chance we'll need it
    if (this._infoRequest)
      this._infoRequest.abort();
    this._infoRequest = null;
  }
}

///////////////////////////////////////////////////////////////////////////////
// View Context Menus
var gAddonContextMenus = ["menuitem_useTheme", "menuitem_options", "menuitem_homepage",
                          "menuitem_about",  "menuseparator_1", "menuitem_uninstall",
                          "menuitem_cancelUninstall", "menuitem_cancelInstall",
                          "menuitem_cancelUpgrade", "menuitem_checkUpdate",
                          "menuitem_enable", "menuitem_disable"];
var gUpdateContextMenus = ["menuitem_homepage", "menuitem_about", "menuseparator_1",
                           "menuitem_installUpdate", "menuitem_includeUpdate"];
// For Firefox don't display context menuitems that can open a browser window.
var gUpdateContextMenusNoBrowser = ["menuitem_installUpdate", "menuitem_includeUpdate"];
var gInstallContextMenus = ["menuitem_homepage", "menuitem_about"];

function buildContextMenu(aEvent)
{
  var popup = document.getElementById("addonContextMenu");
  var selectedItem = gExtensionsView.selectedItem;
  if (aEvent.target !== popup || !selectedItem)
    return false;

  while (popup.hasChildNodes())
    popup.removeChild(popup.firstChild);

  switch (gView) {
  case "extensions":
  case "themes":
  case "locales":
  case "plugins":
    var menus = gAddonContextMenus;
    break;
  case "updates":
    menus = gUpdateContextMenus;
    break;
  case "installs":
    menus = gInstallContextMenus;
    break;
  }

  for (var i = 0; i < menus.length; ++i) {
    var clonedMenu = document.getElementById(menus[i]).cloneNode(true);
    clonedMenu.id = clonedMenu.id + "_clone";
    popup.appendChild(clonedMenu);
  }

  // All views support about
  var menuitem_about = document.getElementById("menuitem_about_clone");
  var name = selectedItem ? selectedItem.getAttribute("name") : "";
  menuitem_about.setAttribute("label", getExtensionString("aboutAddon", [name]));

  /* When an update or install is pending allow canceling the update or install
     and don't allow uninstall. When an uninstall is pending allow canceling the
     uninstall.*/
  if (gView != "updates" && gView != "installs") {
    var canEnable = gExtensionsViewController.isCommandEnabled("cmd_cancelUninstall");
    document.getElementById("menuitem_cancelUninstall_clone").hidden = !canEnable;
    var canCancelInstall = gExtensionsViewController.isCommandEnabled("cmd_cancelInstall");
    document.getElementById("menuitem_cancelInstall_clone").hidden = !canCancelInstall;
    var canCancelUpgrade = gExtensionsViewController.isCommandEnabled("cmd_cancelUpgrade");
    document.getElementById("menuitem_cancelUpgrade_clone").hidden = !canCancelUpgrade;
    document.getElementById("menuitem_uninstall_clone").hidden = canEnable || canCancelInstall || canCancelUpgrade;
  }

  switch (gView) {
  case "extensions":
    canEnable = gExtensionsViewController.isCommandEnabled("cmd_reallyEnable");
    document.getElementById("menuitem_enable_clone").hidden = !canEnable;
    document.getElementById("menuitem_disable_clone").hidden = canEnable;
    document.getElementById("menuitem_useTheme_clone").hidden = true;
    break;
  case "themes":
    var enableMenu = document.getElementById("menuitem_enable_clone");
    if (!selectedItem.isCompatible || selectedItem.isBlocklisted ||
        !selectedItem.satisfiesDependencies || selectedItem.isDisabled)
      // don't let the user activate incompatible themes, but show a (disabled) Enable
      // menuitem to give visual feedback; it's disabled because cmd_enable returns false
      enableMenu.hidden = false;
    else
      enableMenu.hidden = true;
    document.getElementById("menuitem_options_clone").hidden = true;
    document.getElementById("menuitem_disable_clone").hidden = true;
    break;
  case "plugins":
    document.getElementById("menuitem_about_clone").hidden = true;
    document.getElementById("menuitem_uninstall_clone").hidden = true;
    document.getElementById("menuitem_checkUpdate_clone").hidden = true;
  case "locales":
    canEnable = gExtensionsViewController.isCommandEnabled("cmd_reallyEnable");
    document.getElementById("menuitem_enable_clone").hidden = !canEnable;
    document.getElementById("menuitem_disable_clone").hidden = canEnable;
    document.getElementById("menuitem_useTheme_clone").hidden = true;
    document.getElementById("menuitem_options_clone").hidden = true;
    break;
  case "updates":
    var includeUpdate = document.getAnonymousElementByAttribute(selectedItem, "anonid", "includeUpdate");
    var menuitem_includeUpdate = document.getElementById("menuitem_includeUpdate_clone");
    menuitem_includeUpdate.setAttribute("checked", includeUpdate.checked ? "true" : "false");
    break;
  case "installs":
    break;
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
                               .getService(nsIIOService);
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
      var url = uri.QueryInterface(nsIURL);
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
    // XXXrstrong - bug 269568, GTK2 drag and drop is returning invalid data for
    // dragenter and dragover. To workaround this we always set canDrop to true
    // and just use the xfer data returned in ondrop which is valid.
#ifndef MOZ_WIDGET_GTK2
    this._ensureServices();

    var count = aDragSession.numDropItems;
    for (var i = 0; i < count; ++i) {
      var fileData = this._getDataFromDragSession(aDragSession, i);
      if (!fileData) {
        this._canDrop = false;
        return;
      }
    }
#endif
    this._canDrop = true;
  },

  onDragOver: function (aEvent, aFlavor, aDragSession) { },

  onDrop: function(aEvent, aXferData, aDragSession)
  {
    if (!isXPInstallEnabled())
      return;

    this._ensureServices();

    var xpinstallObj = { };
    var themes = { };
    var xpiCount = 0;
    var themeCount = 0;

    var count = aDragSession.numDropItems;
    for (var i = 0; i < count; ++i) {
      var fileData = this._getDataFromDragSession(aDragSession, i);
      if (!fileData)
        continue;

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
      // XXXrstrong Only allow the install of one theme due to bug 257992
      for (var fileName in themes) {
        InstallTrigger.installChrome(InstallTrigger.SKIN, themes[fileName], fileName);
        break;
      }
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
// Notification Messages
const gAddonsMsgObserver = {
  observe: function (aSubject, aTopic, aData)
  {
    switch (aData) {
    case "addons-enable-xpinstall":
      gPref.setBoolPref("xpinstall.enabled", true);
      break;
    case "addons-enable-compatibility":
      gPref.clearUserPref(PREF_EM_CHECK_COMPATIBILITY);
      gCheckCompat = true;
      break;
    case "addons-enable-updatesecurity":
      gPref.clearUserPref(PREF_EM_CHECK_UPDATE_SECURITY);
      gCheckUpdateSecurity = true;
      break;
    case "addons-no-updates":
      var children = gExtensionsView.children;
      for (var i = 0; i < children.length; ++i) {
        var child = children[i];
        if (child.hasAttribute("updateStatus"))
          child.removeAttribute("updateStatus");
      }
      break;
    case "addons-go-online":
      var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                                .getService(nsIIOService);
      ioService.offline = false;
      break;
    case "addons-message-dismiss":
      break;
    }
    if (gExtensionsView.selectedItem)
      gExtensionsView.selectedItem.focus();
  }
};

function isXPInstallEnabled() {
  var enabled = false;
  var locked = false;
  try {
    enabled = gPref.getBoolPref("xpinstall.enabled");
    if (enabled)
      return true;
    locked = gPref.prefIsLocked("xpinstall.enabled");
  }
  catch (e) { }

  var msgText = getExtensionString(locked ? "xpinstallDisabledMsgLocked" :
                                            "xpinstallDisabledMsg");
  var buttonLabel = locked ? null : getExtensionString("enableButtonLabel");
  var buttonAccesskey = locked ? null : getExtensionString("enableButtonAccesskey");
  var notifyData = locked ? null : "addons-enable-xpinstall";
  showMessage("chrome://mozapps/skin/extensions/question.png",
              msgText, buttonLabel, buttonAccesskey,
              !locked, notifyData);
  return false;
}

function isOffline(messageKey) {
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(nsIIOService);
  if (ioService.offline) {
    showMessage("chrome://mozapps/skin/extensions/question.png",
                getExtensionString(messageKey, [getBrandShortName()]),
                getExtensionString("goOnlineButtonLabel"),
                getExtensionString("goOnlineButtonAccesskey"),
                true, "addons-go-online");
  }
  return ioService.offline;
}

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

function updateOptionalViews() {
  var ds = gExtensionsView.database;
  var rdfs = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                   .getService(Components.interfaces.nsIRDFService);
  var ctr = Components.classes["@mozilla.org/rdf/container;1"]
                      .createInstance(Components.interfaces.nsIRDFContainer);
  ctr.Init(ds, rdfs.GetResource(RDFURI_ITEM_ROOT));
  var elements = ctr.GetElements();
  var showLocales = false;
  var showUpdates = false;
  var showInstalls = false;
  gInstallCount = 0;
  while (elements.hasMoreElements()) {
    var e = elements.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
    if (!showLocales) {
      var typeArc = rdfs.GetResource(PREFIX_NS_EM + "type");
      var type = ds.GetTarget(e, typeArc, true);
      if (type && type instanceof Components.interfaces.nsIRDFInt) {
        if (type.Value & nsIUpdateItem.TYPE_LOCALE)
          showLocales = true;
      }
    }

    var stateArc = rdfs.GetResource(PREFIX_NS_EM + "state");
    var state = ds.GetTarget(e, stateArc, true);
    if (state) {
      showInstalls = true;
      if (state instanceof Components.interfaces.nsIRDFLiteral &&
          state.Value != "success" && state.Value != "failure")
        gInstallCount++;
    }

    if (!showUpdates) {
      var updateURLArc = rdfs.GetResource(PREFIX_NS_EM + "availableUpdateURL");
      var updateURL = ds.GetTarget(e, updateURLArc, true);
      if (updateURL) {
        var updateableArc = rdfs.GetResource(PREFIX_NS_EM + "updateable");
        var updateable = ds.GetTarget(e, updateableArc, true);
        updateable = updateable.QueryInterface(Components.interfaces.nsIRDFLiteral);
        if (updateable.Value == "true")
          showUpdates = true;
      }
    }
  }
  document.getElementById("locales-view").hidden = !showLocales;
  document.getElementById("updates-view").hidden = !showUpdates;
  document.getElementById("installs-view").hidden = !showInstalls;
}

function updateGlobalCommands() {
  var disableInstallFile = false;
  var disableUpdateCheck = true;
  var disableInstallUpdate = true;
  var disableAppRestart = (gInstallCount > 0);
  if (gExtensionsView.hasAttribute("update-operation")) {
    disableInstallFile = true;
    disableAppRestart = true;
  }
  else if (gView == "updates") {
    disableInstallUpdate = false;
    disableAppRestart = true;
  }
  else {
    var children = gExtensionsView.children;
    for (var i = 0; i < children.length; ++i) {
      if (children[i].getAttribute("updateable") == "true") {
        disableUpdateCheck = false;
        break;
      }
    }
  }
  setElementDisabledByID("cmd_checkUpdatesAll", disableUpdateCheck);
  setElementDisabledByID("cmd_installUpdatesAll", disableInstallUpdate);
  setElementDisabledByID("cmd_restartApp", disableAppRestart);
  setElementDisabledByID("cmd_installFile", disableInstallFile);
}

function showUpdateInfo()
{
  document.getElementById("themePreviewArea").hidden = false;
  document.getElementById("themeSplitter").hidden = false;
  document.getElementById("showUpdateInfoButton").hidden = true;
  document.getElementById("hideUpdateInfoButton").hidden = false;
  onAddonSelect();
}

function hideUpdateInfo()
{
  UpdateInfoLoader.cancelLoad();
  document.getElementById("themePreviewArea").hidden = true;
  document.getElementById("themeSplitter").hidden = true;
  document.getElementById("showUpdateInfoButton").hidden = false;
  document.getElementById("hideUpdateInfoButton").hidden = true;
}

function checkUpdatesAll() {
  if (isOffline("offlineUpdateMsg"))
    return;

  if (!isXPInstallEnabled())
    return;

  // To support custom views we check the add-ons displayed in the list
  var items = [];
  var children = gExtensionsView.children;
  for (var i = 0; i < children.length; ++i) {
    if (children[i].getAttribute("updateable") != "false")
      items.push(gExtensionManager.getItemForID(getIDFromResourceURI(children[i].id)));
  }

  if (items.length > 0) {
    showProgressBar();
    var listener = new UpdateCheckListener();
    gExtensionManager.update(items, items.length,
                             nsIExtensionManager.UPDATE_CHECK_NEWVERSION,
                             listener);
  }
  if (gExtensionsView.selectedItem)
    gExtensionsView.selectedItem.focus();

  gPref.setBoolPref(PREF_UPDATE_NOTIFYUSER, false);
}

function installUpdatesAll() {
  if (isOffline("offlineUpdateMsg"))
    return;

  if (!isXPInstallEnabled())
    return;

  if (gUpdatesOnly)
    document.getElementById("addonsMsg").removeCurrentNotification();

  var items = [];
  var children = gExtensionsView.children;
  for (var i = 0; i < children.length; ++i) {
    var includeUpdate = document.getAnonymousElementByAttribute(children[i], "anonid", "includeUpdate");
    if (includeUpdate && includeUpdate.checked)
      items.push(gExtensionManager.getItemForID(getIDFromResourceURI(children[i].id)));
  }
  if (items.length > 0) {
    gExtensionManager.addDownloads(items, items.length, true);
    showView("installs");
    // Remove the updates view if there are no add-ons left to update
    updateOptionalViews();
    updateGlobalCommands();
  }
}

function restartApp() {

  // Notify all windows that an application quit has been requested.
  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Components.interfaces.nsIObserverService);
  var cancelQuit = Components.classes["@mozilla.org/supports-PRBool;1"]
                             .createInstance(Components.interfaces.nsISupportsPRBool);
  os.notifyObservers(cancelQuit, "quit-application-requested", "restart");

  // Something aborted the quit process.
  if (cancelQuit.data)
    return;

  Components.classes["@mozilla.org/toolkit/app-startup;1"].getService(nsIAppStartup)
            .quit(nsIAppStartup.eRestart | nsIAppStartup.eAttemptQuit);
}

function installWithFilePicker() {
  if (!isXPInstallEnabled())
    return;

  if (gView == "themes")
    installSkin();
  else
    installExtension();
}

function closeEM() {
  closeWindow(true);
}

function confirmOperation(aName, aTitle, aQueryMsg, aAcceptBtn, aCancelBtn,
                          aWarnMsg, aDependantItems) {
  var params = {
    message2: getExtensionString(aQueryMsg, [aName]),
    title: getExtensionString(aTitle, [aName]),
    buttons: {
      accept: { label: getExtensionString(aAcceptBtn) },
      cancel: { label: getExtensionString(aCancelBtn), focused: true }
    }
  }
  if (aDependantItems.length > 0)
    params.message1 = getExtensionString(aWarnMsg, [aName]);
  var names = [];
  for (var i = 0; i < aDependantItems.length; ++i)
    names.push(aDependantItems[i].name + " " + aDependantItems[i].version);

  window.openDialog("chrome://mozapps/content/extensions/list.xul", "",
                    "titlebar,modal,centerscreen", names, params);
  return params.result == "accept";
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
    if (!selectedItem)
      return false;

    if (selectedItem.hasAttribute("downloadURL") &&
        selectedItem.getAttribute("downloadURL") != "") {
      if (aCommand == "cmd_uninstall")
        return true;
      return false;
    }
    switch (aCommand) {
    case "cmd_useTheme":
      return selectedItem.type == nsIUpdateItem.TYPE_THEME &&
             !selectedItem.isDisabled &&
             selectedItem.opType != OP_NEEDS_UNINSTALL &&
             gCurrentTheme != selectedItem.getAttribute("internalName");
    case "cmd_options":
      return selectedItem.type == nsIUpdateItem.TYPE_EXTENSION &&
             !selectedItem.isDisabled &&
             !gInSafeMode &&
             !selectedItem.opType &&
             selectedItem.getAttribute("optionsURL") != "";
    case "cmd_about":
      return selectedItem.opType != OP_NEEDS_INSTALL &&
             selectedItem.getAttribute("plugin") != "true";
    case "cmd_homepage":
      return selectedItem.getAttribute("homepageURL") != "";
    case "cmd_uninstall":
      return (selectedItem.type != nsIUpdateItem.TYPE_THEME ||
             selectedItem.type == nsIUpdateItem.TYPE_THEME &&
             selectedItem.getAttribute("internalName") != gDefaultTheme) &&
             selectedItem.opType != OP_NEEDS_UNINSTALL &&
             selectedItem.getAttribute("locked") != "true" &&
             canWriteToLocation(selectedItem) &&
             !gExtensionsView.hasAttribute("update-operation");
    case "cmd_cancelUninstall":
      return selectedItem.opType == OP_NEEDS_UNINSTALL;
    case "cmd_cancelInstall":
      return selectedItem.opType == OP_NEEDS_INSTALL;
    case "cmd_cancelUpgrade":
      return selectedItem.opType == OP_NEEDS_UPGRADE;
    case "cmd_checkUpdate":
      return selectedItem.getAttribute("updateable") != "false" &&
             !gExtensionsView.hasAttribute("update-operation");
    case "cmd_installUpdate":
      return selectedItem.hasAttribute("availableUpdateURL") &&
             !gExtensionsView.hasAttribute("update-operation");
    case "cmd_includeUpdate":
      return selectedItem.hasAttribute("availableUpdateURL") &&
             !gExtensionsView.hasAttribute("update-operation");
    case "cmd_reallyEnable":
    // controls whether to show Enable or Disable in extensions' context menu
      return selectedItem.isDisabled &&
             selectedItem.opType != OP_NEEDS_ENABLE ||
             selectedItem.opType == OP_NEEDS_DISABLE;
    case "cmd_enable":
      return selectedItem.type != nsIUpdateItem.TYPE_THEME &&
             (selectedItem.isDisabled ||
             (!selectedItem.opType ||
             selectedItem.opType == "needs-disable")) &&
             !selectedItem.isBlocklisted &&
             (!gCheckUpdateSecurity || selectedItem.providesUpdatesSecurely) &&
             (!gCheckCompat || selectedItem.isCompatible) &&
             selectedItem.satisfiesDependencies &&
             !gExtensionsView.hasAttribute("update-operation");
    case "cmd_disable":
      return selectedItem.type != nsIUpdateItem.TYPE_THEME &&
             (!selectedItem.isDisabled &&
             !selectedItem.opType ||
             selectedItem.opType == OP_NEEDS_ENABLE) &&
             !selectedItem.isBlocklisted &&
             selectedItem.satisfiesDependencies &&
             !gExtensionsView.hasAttribute("update-operation");
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
      gCurrentTheme = aSelectedItem.getAttribute("internalName");
      // Set this pref so the user can reset the theme in safe mode
      gPref.setCharPref(PREF_EM_LAST_SELECTED_SKIN, gCurrentTheme);

      if (gPref.getBoolPref(PREF_EXTENSIONS_DSS_ENABLED)) {
        gPref.setCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN, gCurrentTheme);
      }
      else {
        // Theme change will happen on next startup, this flag tells
        // the Theme Manager that it needs to show "This theme will
        // be selected after a restart" text in the selected theme
        // item.
        gPref.setBoolPref(PREF_EXTENSIONS_DSS_SWITCHPENDING, true);
        // Update the view
        setRestartMessage(aSelectedItem);
      }

      // Flush preference change to disk
      gPref.QueryInterface(Components.interfaces.nsIPrefService)
           .savePrefFile(null);

      // disable the useThemeButton
      gExtensionsViewController.onCommandUpdate();
    },

    cmd_options: function (aSelectedItem)
    {
      if (!aSelectedItem) return;
      var optionsURL = aSelectedItem.getAttribute("optionsURL");
      if (optionsURL == "")
        return;

      var windows = Components.classes['@mozilla.org/appshell/window-mediator;1']
                              .getService(Components.interfaces.nsIWindowMediator)
                              .getEnumerator(null);
      while (windows.hasMoreElements()) {
        var win = windows.getNext();
        if (win.document.documentURI == optionsURL) {
          win.focus();
          return;
        }
      }

      var features;
      try {
        var instantApply = gPref.getBoolPref("browser.preferences.instantApply");
        features = "chrome,titlebar,toolbar,centerscreen" + (instantApply ? ",dialog=no" : ",modal");
      }
      catch (e) {
        features = "chrome,titlebar,toolbar,centerscreen,modal";
      }
      openDialog(optionsURL, "", features);
    },

    cmd_homepage: function (aSelectedItem)
    {
      if (!aSelectedItem) return;
      var homepageURL = aSelectedItem.getAttribute("homepageURL");
      // only allow http(s) homepages
      var scheme = "";
      var uri = null;
      try {
        uri = makeURI(homepageURL);
        scheme = uri.scheme;
      } catch (ex) {}
      if (uri && (scheme == "http" || scheme == "https"))
        openURL(uri.spec);
    },

    cmd_about: function (aSelectedItem)
    {
      if (!aSelectedItem) return;
      var aboutURL = aSelectedItem.getAttribute("aboutURL");
      if (aboutURL != "")
        openDialog(aboutURL, "", "chrome,centerscreen,modal");
      else
        openDialog("chrome://mozapps/content/extensions/about.xul", "", "chrome,centerscreen,modal", aSelectedItem.id, gExtensionsView.database);
    },

    cmd_checkUpdate: function (aSelectedItem)
    {
      if (isOffline("offlineUpdateMsg"))
        return;

      if (!isXPInstallEnabled())
        return;

      var id = getIDFromResourceURI(aSelectedItem.id);
      var items = [gExtensionManager.getItemForID(id)];
      var listener = new UpdateCheckListener();
      gExtensionManager.update(items, items.length,
                               nsIExtensionManager.UPDATE_CHECK_NEWVERSION,
                               listener);
    },

    cmd_installUpdate: function (aSelectedItem)
    {
      if (isOffline("offlineUpdateMsg"))
        return;

      if (!isXPInstallEnabled())
        return;

      showView("installs");
      var item = gExtensionManager.getItemForID(getIDFromResourceURI(aSelectedItem.id));
      gExtensionManager.addDownloads([item], 1, true);
      // Remove the updates view if there are no add-ons left to update
      updateOptionalViews();
      updateGlobalCommands();
    },

    cmd_includeUpdate: function (aSelectedItem)
    {
      var includeUpdate = document.getAnonymousElementByAttribute(aSelectedItem, "anonid", "includeUpdate");
      includeUpdate.checked = !includeUpdate.checked;
    },

    cmd_uninstall: function (aSelectedItem)
    {
      // Confirm the uninstall
      var name = aSelectedItem.getAttribute("name");
      var id = getIDFromResourceURI(aSelectedItem.id);
      var dependentItems = gExtensionManager.getDependentItemListForID(id, true, { });
      var result = confirmOperation(name, "uninstallTitle", "uninstallQueryMessage",
                                    "uninstallButton", "cancelButton",
                                    "uninstallWarnDependMsg", dependentItems);
      if (!result)
        return;

      if (aSelectedItem.type == nsIUpdateItem.TYPE_THEME) {
        // If the theme being uninstalled is the current theme, we need to reselect
        // the default. If it isn't change the selection before uninstall so the
        // theme preview is updated since the theme will be removed immediately.
        var currentTheme = gPref.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN);
        if (aSelectedItem.getAttribute("internalName") == currentTheme)
          this.cmd_useTheme(document.getElementById(PREFIX_ITEM_URI + "{972ce4c6-7e08-4474-a285-3208198ce6fd}"));
        else if (!gExtensionsView.goDown())
          gExtensionsView.goUp();
      }
      gExtensionManager.uninstallItem(getIDFromResourceURI(aSelectedItem.id));
      gExtensionsViewController.onCommandUpdate();
      updateGlobalCommands();
      if (gExtensionsView.selectedItem)
        gExtensionsView.selectedItem.focus();
      updateOptionalViews();
    },

    cmd_cancelUninstall: function (aSelectedItem)
    {
      gExtensionManager.cancelUninstallItem(getIDFromResourceURI(aSelectedItem.id));
      gExtensionsViewController.onCommandUpdate();
      updateGlobalCommands();
      gExtensionsView.selectedItem.focus();
      updateOptionalViews();
    },

    cmd_cancelInstall: function (aSelectedItem)
    {
      var name = aSelectedItem.getAttribute("name");
      var result = false;
      // Confirm cancelling the operation
      switch (aSelectedItem.opType)
      {
        case OP_NEEDS_INSTALL:
          result = confirmOperation(name, "cancelInstallTitle", "cancelInstallQueryMessage",
                                    "cancelInstallButton", "cancelCancelInstallButton",
                                    null, []);
          break;
        case OP_NEEDS_UPGRADE:
          result = confirmOperation(name, "cancelUpgradeTitle", "cancelUpgradeQueryMessage",
                                    "cancelUpgradeButton", "cancelCancelUpgradeButton",
                                    null, []);
          break;
      }
      if (!result)
        return;

      gExtensionManager.cancelInstallItem(getIDFromResourceURI(aSelectedItem.id));
      gExtensionsViewController.onCommandUpdate();
      updateGlobalCommands();
      gExtensionsView.selectedItem.focus();
      updateOptionalViews();
    },

    cmd_cancelUpgrade: function (aSelectedItem)
    {
      this.cmd_cancelInstall(aSelectedItem);
    },

    cmd_disable: function (aSelectedItem)
    {
      if (aSelectedItem.getAttribute("plugin") == "true") {
        var name = aSelectedItem.getAttribute("name");
        var desc = aSelectedItem.getAttribute("description");
        togglePluginDisabled(name, desc);
        return;
      }

      var id = getIDFromResourceURI(aSelectedItem.id);
      var dependentItems = gExtensionManager.getDependentItemListForID(id, false, { });

      if (dependentItems.length > 0) {
        name = aSelectedItem.getAttribute("name");
        var result = confirmOperation(name, "disableTitle", "disableQueryMessage",
                                      "disableButton", "cancelButton",
                                      "disableWarningDependMessage", dependentItems);
        if (!result)
          return;
      }
      gExtensionManager.disableItem(id);
      gExtensionsViewController.onCommandUpdate();
      gExtensionsView.selectedItem.focus();
      updateOptionalViews();
    },

    cmd_enable: function (aSelectedItem)
    {
      if (aSelectedItem.getAttribute("plugin") == "true") {
        var name = aSelectedItem.getAttribute("name");
        var desc = aSelectedItem.getAttribute("description");
        togglePluginDisabled(name, desc);
        return;
      }

      gExtensionManager.enableItem(getIDFromResourceURI(aSelectedItem.id));
      gExtensionsViewController.onCommandUpdate();
      gExtensionsView.selectedItem.focus();
      updateOptionalViews();
    }
  }
};

//////////////////////////////////////////////////////////////////////////
// functions to support installing of themes in apps other than firefox //
//////////////////////////////////////////////////////////////////////////
function installSkin()
{
  // 1) Prompt the user for the location of the theme to install.
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  fp.init(window, getExtensionString("installThemePickerTitle"), nsIFilePicker.modeOpen);
  fp.appendFilter(getExtensionString("themesFilter"), "*.jar");
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
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  fp.init(window, getExtensionString("installExtensionPickerTitle"), nsIFilePicker.modeOpen);
  fp.appendFilter(getExtensionString("extensionFilter"), "*.xpi");
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
