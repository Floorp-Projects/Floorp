/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
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
 * The Original Code is DevTools (HeadsUpDisplay) Console Code
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Dahl <ddahl@mozilla.com> (original author)
 *   Rob Campbell <rcampbell@mozilla.com>
 *   Johnathan Nightingale <jnightingale@mozilla.com>
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["HUDService"];

XPCOMUtils.defineLazyServiceGetter(this, "scriptError",
                                   "@mozilla.org/scripterror;1",
                                   "nsIScriptError");

XPCOMUtils.defineLazyServiceGetter(this, "activityDistributor",
                                   "@mozilla.org/network/http-activity-distributor;1",
                                   "nsIHttpActivityDistributor");

XPCOMUtils.defineLazyServiceGetter(this, "sss",
                                   "@mozilla.org/content/style-sheet-service;1",
                                   "nsIStyleSheetService");

function LogFactory(aMessagePrefix)
{
  function log(aMessage) {
    var _msg = aMessagePrefix + " " + aMessage + "\n";
    dump(_msg);
  }
  return log;
}

let log = LogFactory("*** HUDService:");

const ELEMENT_NS_URI = "http://www.w3.org/1999/xhtml";
const ELEMENT_NS = "html:";
const HUD_STYLESHEET_URI = "chrome://global/skin/headsUpDisplay.css";
const HUD_STRINGS_URI = "chrome://global/locale/headsUpDisplay.properties";

XPCOMUtils.defineLazyGetter(this, "stringBundle", function () {
  return Services.strings.createBundle(HUD_STRINGS_URI);
});

const ERRORS = { LOG_MESSAGE_MISSING_ARGS:
                 "Missing arguments: aMessage, aConsoleNode and aMessageNode are required.",
                 CANNOT_GET_HUD: "Cannot getHeads Up Display with provided ID",
                 MISSING_ARGS: "Missing arguments",
                 LOG_OUTPUT_FAILED: "Log Failure: Could not append messageNode to outputNode",
};

function HUD_SERVICE()
{
  // TODO: provide mixins for FENNEC: bug 568621
  if (appName() == "FIREFOX") {
    var mixins = new FirefoxApplicationHooks();
  }
  else {
    throw new Error("Unsupported Application");
  }

  this.mixins = mixins;
  this.storage = new ConsoleStorage();
  this.defaultFilterPrefs = this.storage.defaultDisplayPrefs;
  this.defaultGlobalConsolePrefs = this.storage.defaultGlobalConsolePrefs;

  // load stylesheet with StyleSheetService
  var uri = Services.io.newURI(HUD_STYLESHEET_URI, null, null);
  sss.loadAndRegisterSheet(uri, sss.AGENT_SHEET);

  // begin observing HTTP traffic
  this.startHTTPObservation();
};

HUD_SERVICE.prototype =
{
  /**
   * L10N shortcut function
   *
   * @param string aName
   * @returns string
   */
  getStr: function HS_getStr(aName)
  {
    return stringBundle.GetStringFromName(aName);
  },

  /**
   * L10N shortcut function
   *
   * @param string aName
   * @returns (format) string
   */
  getFormatStr: function HS_getFormatStr(aName, aArray)
  {
    return stringBundle.formatStringFromName(aName, aArray, aArray.length);
  },

  /**
   * getter for UI commands to be used by the frontend
   *
   * @returns object
   */
  get consoleUI() {
    return HeadsUpDisplayUICommands;
  },

  /**
   * Collection of HUDIds that map to the tabs/windows/contexts
   * that a HeadsUpDisplay can be activated for.
   */
  activatedContexts: [],

  /**
   * Registry of HeadsUpDisplay DOM node ids
   */
  _headsUpDisplays: {},

  /**
   * Mapping of HUDIds to URIspecs
   */
  displayRegistry: {},

  /**
   * Mapping of URISpecs to HUDIds
   */
  uriRegistry: {},

  /**
   * The nsILoadGroups being tracked
   */
  loadGroups: {},

  /**
   * The sequencer is a generator (after initialization) that returns unique
   * integers
   */
  sequencer: null,

  /**
   * Each HeadsUpDisplay has a set of filter preferences
   */
  filterPrefs: {},

  /**
   * Event handler to get window errors
   * TODO: a bit of a hack but is able to associate
   * errors thrown in a window's scope we do not know
   * about because of the nsIConsoleMessages not having a
   * window reference.
   * see bug 567165
   *
   * @param nsIDOMWindow aWindow
   * @returns boolean
   */
  setOnErrorHandler: function HS_setOnErrorHandler(aWindow) {
    var self = this;
    var window = aWindow.wrappedJSObject;
    var console = window.console;
    var origOnerrorFunc = window.onerror;
    window.onerror = function windowOnError(aErrorMsg, aURL, aLineNumber)
    {
      var lineNum = "";
      if (aLineNumber) {
        lineNum = self.getFormatStr("errLine", [aLineNumber]);
      }
      console.error(aErrorMsg + " @ " + aURL + " " + lineNum);
      if (origOnerrorFunc) {
        origOnerrorFunc(aErrorMsg, aURL, aLineNumber);
      }
      return false;
    };
  },

  /**
   * Tell the HUDService that a HeadsUpDisplay can be activated
   * for the window or context that has 'aContextDOMId' node id
   *
   * @param string aContextDOMId
   * @return void
   */
  registerActiveContext: function HS_registerActiveContext(aContextDOMId)
  {
    this.activatedContexts.push(aContextDOMId);
  },

  /**
   * Firefox-specific current tab getter
   *
   * @returns nsIDOMWindow
   */
  currentContext: function HS_currentContext() {
    return this.mixins.getCurrentContext();
  },

  /**
   * Tell the HUDService that a HeadsUpDisplay should be deactivated
   *
   * @param string aContextDOMId
   * @return void
   */
  unregisterActiveContext: function HS_deregisterActiveContext(aContextDOMId)
  {
    var domId = aContextDOMId.split("_")[1];
    var idx = this.activatedContexts.indexOf(domId);
    if (idx > -1) {
      this.activatedContexts.splice(idx, 1);
    }
  },

  /**
   * Tells callers that a HeadsUpDisplay can be activated for the context
   *
   * @param string aContextDOMId
   * @return boolean
   */
  canActivateContext: function HS_canActivateContext(aContextDOMId)
  {
    var domId = aContextDOMId.split("_")[1];
    for (var idx in this.activatedContexts) {
      if (this.activatedContexts[idx] == domId){
        return true;
      }
    }
    return false;
  },

  /**
   * Activate a HeadsUpDisplay for the current window
   *
   * @param nsIDOMWindow aContext
   * @returns void
   */
  activateHUDForContext: function HS_activateHUDForContext(aContext)
  {
    var window = aContext.linkedBrowser.contentWindow;
    var id = aContext.linkedBrowser.parentNode.getAttribute("id");
    this.registerActiveContext(id);
    HUDService.windowInitializer(window);
  },

  /**
   * Deactivate a HeadsUpDisplay for the current window
   *
   * @param nsIDOMWindow aContext
   * @returns void
   */
  deactivateHUDForContext: function HS_deactivateHUDForContext(aContext)
  {
    var gBrowser = HUDService.currentContext().gBrowser;
    var window = aContext.linkedBrowser.contentWindow;
    var browser = gBrowser.getBrowserForDocument(window.top.document);
    var tabId = gBrowser.getNotificationBox(browser).getAttribute("id");
    var hudId = "hud_" + tabId;
    var displayNode = this.getHeadsUpDisplay(hudId);

    this.unregisterActiveContext(hudId);
    this.unregisterDisplay(hudId);
    window.wrappedJSObject.console = null;

  },

  /**
   * Clear the specified HeadsUpDisplay
   *
   * @param string aId
   * @returns void
   */
  clearDisplay: function HS_clearDisplay(aId)
  {
    var displayNode = this.getOutputNodeById(aId);
    var outputNode = displayNode.querySelectorAll(".hud-output-node")[0];

    while (outputNode.firstChild) {
      outputNode.removeChild(outputNode.firstChild);
    }
  },

  /**
   * get a unique ID from the sequence generator
   *
   * @returns integer
   */
  sequenceId: function HS_sequencerId()
  {
    if (!this.sequencer) {
      this.sequencer = this.createSequencer(-1);
    }
    return this.sequencer.next();
  },

  /**
   * get the default filter prefs
   *
   * @param string aHUDId
   * @returns JS Object
   */
  getDefaultFilterPrefs: function HS_getDefaultFilterPrefs(aHUDId) {
    return this.filterPrefs[aHUDId];
  },

  /**
   * get the current filter prefs
   *
   * @param string aHUDId
   * @returns JS Object
   */
  getFilterPrefs: function HS_getFilterPrefs(aHUDId) {
    return this.filterPrefs[aHUDId];
  },

  /**
   * get the filter state for a specific toggle button on a heads up display
   *
   * @param string aHUDId
   * @param string aToggleType
   * @returns boolean
   */
  getFilterState: function HS_getFilterState(aHUDId, aToggleType)
  {
    if (!aHUDId) {
      return false;
    }
    try {
      var bool = this.filterPrefs[aHUDId][aToggleType];
      return bool;
    }
    catch (ex) {
      return false;
    }
  },

  /**
   * set the filter state for a specific toggle button on a heads up display
   *
   * @param string aHUDId
   * @param string aToggleType
   * @param boolean aState
   * @returns void
   */
  setFilterState: function HS_setFilterState(aHUDId, aToggleType, aState)
  {
    this.filterPrefs[aHUDId][aToggleType] = aState;
  },

  /**
   * Keeps a weak reference for each HeadsUpDisplay that is created
   *
   */
  hudWeakReferences: {},

  /**
   * Register a weak reference of each HeadsUpDisplay that is created
   *
   * @param object aHUDRef
   * @param string aHUDId
   * @returns void
   */
  registerHUDWeakReference:
  function HS_registerHUDWeakReference(aHUDRef, aHUDId)
  {
    this.hudWeakReferences[aHUDId] = aHUDRef;
  },

  /**
   * Deletes a HeadsUpDisplay object from memory
   *
   * @param string aHUDId
   * @returns void
   */
  deleteHeadsUpDisplay: function HS_deleteHeadsUpDisplay(aHUDId)
  {
    delete this.hudWeakReferences[aHUDId].get();
  },

  /**
   * Register a new Heads Up Display
   *
   * @param string aHUDId
   * @param string aURISpec
   * @returns void
   */
  registerDisplay: function HS_registerDisplay(aHUDId, aURISpec)
  {
    // register a display DOM node Id and HUD uriSpec with the service

    if (!aHUDId || !aURISpec){
      throw new Error(ERRORS.MISSING_ARGS);
    }
    this.filterPrefs[aHUDId] = this.defaultFilterPrefs;
    this.displayRegistry[aHUDId] = aURISpec;
    this._headsUpDisplays[aHUDId] = { id: aHUDId, };
    this.registerActiveContext(aHUDId);
    // init storage objects:
    this.storage.createDisplay(aHUDId);

    var huds = this.uriRegistry[aURISpec];
    var foundHUDId = false;

    if (huds) {
      var len = huds.length;
      for (var i = 0; i < len; i++) {
        if (huds[i] == aHUDId) {
          foundHUDId = true;
          break;
        }
      }
      if (!foundHUDId) {
        this.uriRegistry[aURISpec].push(aHUDId);
      }
    }
    else {
      this.uriRegistry[aURISpec] = [aHUDId];
    }
  },

  /**
   * When a display is being destroyed, unregister it first
   *
   * @param string aId
   * @returns void
   */
  unregisterDisplay: function HS_unregisterDisplay(aId)
  {
    // remove HUD DOM node and
    // remove display references from local registries get the outputNode
    var outputNode = this.mixins.getOutputNodeById(aId);
    var parent = outputNode.parentNode;
    var splitters = parent.querySelectorAll("splitter");
    var len = splitters.length;
    for (var i = 0; i < len; i++) {
      if (splitters[i].getAttribute("class") == "hud-splitter") {
        splitters[i].parentNode.removeChild(splitters[i]);
        break;
      }
    }
    // remove the DOM Nodes
    parent.removeChild(outputNode);
    // remove our record of the DOM Nodes from the registry
    delete this._headsUpDisplays[aId];
    // remove the HeadsUpDisplay object from memory
    this.deleteHeadsUpDisplay(aId);
    // remove the related storage object
    this.storage.removeDisplay(aId);
    let displays = this.displays();

    var uri  = this.displayRegistry[aId];
    var specHudArr = this.uriRegistry[uri];

    for (var i = 0; i < specHudArr.length; i++) {
      if (specHudArr[i] == aId) {
        specHudArr.splice(i, 1);
      }
    }
    delete displays[aId];
    delete this.displayRegistry[aId];
  },

  /**
   * Shutdown all HeadsUpDisplays on xpcom-shutdown
   *
   * @returns void
   */
  shutdown: function HS_shutdown()
  {
    for (var displayId in this._headsUpDisplays) {
      this.unregisterDisplay(displayId);
    }
    // delete the storage as it holds onto channels
    delete this.storage;

     var xulWindow = aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
       .getInterface(Ci.nsIWebNavigation)
       .QueryInterface(Ci.nsIDocShellTreeItem)
       .rootTreeItem
       .QueryInterface(Ci.nsIInterfaceRequestor)
       .getInterface(Ci.nsIDOMWindow);

    xulWindow = XPCNativeWrapper.unwrap(xulWindow);
    var gBrowser = xulWindow.gBrowser;
    gBrowser.tabContainer.removeEventListener("TabClose", this.onTabClose, false);
  },

  /**
   * get the nsIDOMNode outputNode via a nsIURI.spec
   *
   * @param string aURISpec
   * @returns nsIDOMNode
   */
  getDisplayByURISpec: function HS_getDisplayByURISpec(aURISpec)
  {
    // TODO: what about data:uris? see bug 568626
    var hudIds = this.uriRegistry[aURISpec];
    if (hudIds.length == 1) {
      // only one HUD connected to this URISpec
      return this.getHeadsUpDisplay(hudIds[0]);
    }
    else {
      // TODO: how to determine more fully the origination of this activity?
      // see bug 567165
      return this.getHeadsUpDisplay(hudIds[0]);
    }
  },

  /**
   * Gets HUD DOM Node
   * @param string id
   *        The Heads Up Display DOM Id
   * @returns nsIDOMNode
   */
  getHeadsUpDisplay: function HS_getHeadsUpDisplay(aId)
  {
    return this.mixins.getOutputNodeById(aId);
  },

  /**
   * gets the nsIDOMNode outputNode by ID via the gecko app mixins
   *
   * @param string aId
   * @returns nsIDOMNode
   */
  getOutputNodeById: function HS_getOutputNodeById(aId)
  {
    return this.mixins.getOutputNodeById(aId);
  },

  /**
   * Gets an object that contains active DOM Node Ids for all Heads Up Displays
   *
   * @returns object
   */
  displays: function HS_displays() {
    return this._headsUpDisplays;
  },

  /**
   * Get an array of HUDIds that match a uri.spec
   *
   * @param string aURISpec
   * @returns array
   */
  getHUDIdsForURISpec: function HS_getHUDIdsForURISpec(aURISpec)
  {
    if (this.uriRegistry[aURISpec]) {
      return this.uriRegistry[aURISpec];
    }
    return [];
  },

  /**
   * Gets an array that contains active DOM Node Ids for all HUDs
   * @returns array
   */
  displaysIndex: function HS_displaysIndex()
  {
    var props = [];
    for (var prop in this._headsUpDisplays) {
      props.push(prop);
    }
    return props;
  },

  /**
   * get the current filter string for the HeadsUpDisplay
   *
   * @param string aHUDId
   * @returns string
   */
  getFilterStringByHUDId: function HS_getFilterStringbyHUDId(aHUDId) {
    var hud = this.getHeadsUpDisplay(aHUDId);
    var filterStr = hud.querySelectorAll(".hud-filter-box")[0].value;
    return filterStr || null;
  },

  /**
   * The filter strings per HeadsUpDisplay
   *
   */
  hudFilterStrings: {},

  /**
   * Update the filter text in the internal tracking object for all
   * filter strings
   *
   * @param nsIDOMNode aTextBoxNode
   * @returns void
   */
  updateFilterText: function HS_updateFiltertext(aTextBoxNode)
  {
    var hudId = aTextBoxNode.getAttribute(hudId);
    this.hudFilterStrings[hudId] = aTextBoxNode.value || null;
  },

  /**
   * Filter each message being logged into the console
   *
   * @param string aFilterString
   * @param nsIDOMNode aMessageNode
   * @returns JS Object
   */
  filterLogMessage:
  function HS_filterLogMessage(aFilterString, aMessageNode)
  {
    aFilterString = aFilterString.toLowerCase();
    var messageText = aMessageNode.innerHTML.toLowerCase();
    var idx = messageText.indexOf(aFilterString);
    if (idx > -1) {
      return { strLength: aFilterString.length, strIndex: idx };
    }
    else {
      return null;
    }
  },

  /**
   * Get the filter textbox from a HeadsUpDisplay
   *
   * @param string aHUDId
   * @returns nsIDOMNode
   */
  getFilterTextBox: function HS_getFilterTextBox(aHUDId)
  {
    var hud = this.getHeadsUpDisplay(aHUDId);
    return hud.querySelectorAll(".hud-filter-box")[0];
  },

  /**
   * Logs a HUD-generated console message
   * @param object aMessage
   *        The message to log, which is a JS object, this is the
   *        "raw" log message
   * @param nsIDOMNode aConsoleNode
   *        The output DOM node to log the messageNode to
   * @param nsIDOMNode aMessageNode
   *        The message DOM Node that will be appended to aConsoleNode
   * @returns void
   */
  logHUDMessage: function HS_logHUDMessage(aMessage,
                                           aConsoleNode,
                                           aMessageNode,
                                           aFilterState,
                                           aFilterString)
  {
    if (!aFilterState) {
      // do not log anything
      return;
    }

    if (!aMessage) {
      throw new Error(ERRORS.MISSING_ARGS);
    }

    if (aFilterString) {
      var filtered = this.filterLogMessage(aFilterString, aMessageNode);
      if (filtered) {
        // we have successfully filtered a message, we need to log it
        aConsoleNode.appendChild(aMessageNode);
        aMessageNode.scrollIntoView(false);
      }
      else {
        // we need to ignore this message by changing its css class - we are
        // still logging this, it is just hidden
        var hiddenMessage = ConsoleUtils.hideLogMessage(aMessageNode);
        aConsoleNode.appendChild(hiddenMessage);
      }
    }
    else {
      // log everything
      aConsoleNode.appendChild(aMessageNode);
      aMessageNode.scrollIntoView(false);
    }
    // store this message in the storage module:
    this.storage.recordEntry(aMessage.hudId, aMessage);
  },

  /**
   * logs a message to the Heads Up Display that originates
   * in the nsIConsoleService
   *
   * @param nsIConsoleMessage aMessage
   * @param nsIDOMNode aConsoleNode
   * @param nsIDOMNode aMessageNode
   * @returns void
   */
  logConsoleMessage: function HS_logConsoleMessage(aMessage,
                                                   aConsoleNode,
                                                   aMessageNode,
                                                   aFilterState,
                                                   aFilterString)
  {
    if (aFilterState){
      aConsoleNode.appendChild(aMessageNode);
      aMessageNode.scrollIntoView(false);
    }
    // store this message in the storage module:
    this.storage.recordEntry(aMessage.hudId, aMessage);
  },

  /**
   * Logs a Message.
   * @param aMessage
   *        The message to log, which is a JS object, this is the
   *        "raw" log message
   * @param aConsoleNode
   *        The output DOM node to log the messageNode to
   * @param The message DOM Node that will be appended to aConsoleNode
   * @returns void
   */
  logMessage: function HS_logMessage(aMessage, aConsoleNode, aMessageNode)
  {
    if (!aMessage) {
      throw new Error(ERRORS.MISSING_ARGS);
    }

    var hud = this.getHeadsUpDisplay(aMessage.hudId);
    // check filter before logging to the outputNode
    var filterState = this.getFilterState(aMessage.hudId, aMessage.logLevel);
    var filterString = this.getFilterStringByHUDId(aMessage.hudId);

    switch (aMessage.origin) {
      case "network":
      case "HUDConsole":
      case "console-listener":
        this.logHUDMessage(aMessage, aConsoleNode, aMessageNode, filterState, filterString);
        break;
      default:
        // noop
        break;
    }
  },

  /**
   * report consoleMessages recieved via the HUDConsoleObserver service
   * @param nsIConsoleMessage aConsoleMessage
   * @returns void
   */
  reportConsoleServiceMessage:
  function HS_reportConsoleServiceMessage(aConsoleMessage)
  {
    this.logActivity("console-listener", null, aConsoleMessage);
  },

  /**
   * report scriptErrors recieved via the HUDConsoleObserver service
   * @param nsIScriptError aScriptError
   * @returns void
   */
  reportConsoleServiceContentScriptError:
  function HS_reportConsoleServiceContentScriptError(aScriptError)
  {
    try {
      var uri = Services.io.newURI(aScriptError.sourceName, null, null);
    }
    catch(ex) {
      var uri = { spec: "" };
    }
    this.logActivity("console-listener", uri, aScriptError);
  },

  /**
   * generates an nsIScriptError
   *
   * @param object aMessage
   * @param integer flag
   * @returns nsIScriptError
   */
  generateConsoleMessage:
  function HS_generateConsoleMessage(aMessage, flag)
  {
    let message = scriptError; // nsIScriptError
    message.init(aMessage.message, null, null, 0, 0, flag,
                 "HUDConsole");
    return message;
  },

  /**
   * Register a Gecko app's specialized ApplicationHooks object
   *
   * @returns void or throws "UNSUPPORTED APPLICATION" error
   */
  registerApplicationHooks:
  function HS_registerApplications(aAppName, aHooksObject)
  {
    switch(aAppName) {
      case "FIREFOX":
        this.applicationHooks = aHooksObject;
        return;
      default:
        throw new Error("MOZ APPLICATION UNSUPPORTED");
    }
  },

  /**
   * Registry of ApplicationHooks used by specified Gecko Apps
   *
   * @returns Specific Gecko 'ApplicationHooks' Object/Mixin
   */
  applicationHooks: null,

  /**
   * Given an nsIChannel, return the corresponding nsILoadContext
   *
   * @param nsIChannel aChannel
   * @returns nsILoadContext
   */
  getLoadContext: function HS_getLoadContext(aChannel)
  {
    if (!aChannel) {
      return null;
    }
    var loadContext;
    var callbacks = aChannel.notificationCallbacks;

    loadContext =
      aChannel.notificationCallbacks.getInterface(Ci.nsILoadContext);
    if (!loadContext) {
      loadContext =
        aChannel.QueryInterface(Ci.nsIRequest).loadGroup.notificationCallbacks.getInterface(Ci.nsILoadContext);
    }
    return loadContext;
  },

  /**
   * Given an nsILoadContext, return the corresponding nsIDOMWindow
   *
   * @param nsILoadContext aLoadContext
   * @returns nsIDOMWindow
   */
  getWindowFromContext: function HS_getWindowFromContext(aLoadContext)
  {
    if (!aLoadContext) {
      throw new Error("loadContext is null");
    }
    if (aLoadContext.isContent) {
      if (aLoadContext.associatedWindow) {
        return aLoadContext.associatedWindow;
      }
      else if (aLoadContext.topWindow) {
        return aLoadContext.topWindow;
      }
    }
    throw new Error("Cannot get window from " + aLoadContext);
  },

  getChromeWindowFromContentWindow:
  function HS_getChromeWindowFromContentWindow(aContentWindow)
  {
    if (!aContentWindow) {
      throw new Error("Cannot get contentWindow via nsILoadContext");
    }
    var win = aContentWindow.QueryInterface(Ci.nsIDOMWindow)
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation)
      .QueryInterface(Ci.nsIDocShellTreeItem)
      .rootTreeItem
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindow)
      .QueryInterface(Ci.nsIDOMChromeWindow);
    return win;
  },

  /**
   * get the outputNode from the window object
   *
   * @param nsIDOMWindow aWindow
   * @returns nsIDOMNode
   */
  getOutputNodeFromWindow:
  function HS_getOutputNodeFromWindow(aWindow)
  {
    var browser = gBrowser.getBrowserForDocument(aWindow.top.document);
    var tabId = gBrowser.getNotificationBox(browser).getAttribute("id");
    var hudId = "hud_" + tabId;
    var displayNode = this.getHeadsUpDisplay(hudId);
    return displayNode.querySelectorAll(".hud-output-node")[0];
  },

  /**
   * Try to get the outputNode via the nsIRequest
   * TODO: get node via request, see bug 552140
   * @param nsIRequest aRequest
   * @returns nsIDOMNode
   */
  getOutputNodeFromRequest: function HS_getOutputNodeFromRequest(aRequest)
  {
    var context = this.getLoadContext(aRequest);
    var window = this.getWindowFromContext(context);
    return this.getOutputNodeFromWindow(window);
  },

  getLoadContextFromChannel: function HS_getLoadContextFromChannel(aChannel)
  {
    try {
      return aChannel.QueryInterface(Ci.nsIChannel).notificationCallbacks.getInterface(Ci.nsILoadContext);
    }
    catch (ex) {
      // noop, keep this output quiet. see bug 552140
    }
    try {
      return aChannel.QueryInterface(Ci.nsIChannel).loadGroup.notificationCallbacks.getInterface(Ci.nsILoadContext);
    }
    catch (ex) {
      // noop, keep this output quiet. see bug 552140
    }
    return null;
  },

  getWindowFromLoadContext:
  function HS_getWindowFromLoadContext(aLoadContext)
  {
    if (aLoadContext.topWindow) {
      return aLoadContext.topWindow;
    }
    else {
      return aLoadContext.associatedWindow;
    }
  },

  /**
   * Begin observing HTTP traffic that we care about,
   * namely traffic that originates inside any context that a Heads Up Display
   * is active for.
   */
  startHTTPObservation: function HS_httpObserverFactory()
  {
    // creates an observer for http traffic
    var self = this;
    var httpObserver = {
      observeActivity :
      function (aChannel, aActivityType, aActivitySubtype,
                aTimestamp, aExtraSizeData, aExtraStringData)
      {
        var loadGroup;
        if (aActivityType ==
            activityDistributor.ACTIVITY_TYPE_HTTP_TRANSACTION) {
          try {
            var loadContext = self.getLoadContextFromChannel(aChannel);
            // TODO: get image request data from the channel
            // see bug 552140
            var window = self.getWindowFromLoadContext(loadContext);
            window = XPCNativeWrapper.unwrap(window);
            var chromeWin = self.getChromeWindowFromContentWindow(window);
            var vboxes =
              chromeWin.document.getElementsByTagName("vbox");
            var hudId;
            for (var i = 0; i < vboxes.length; i++) {
              if (vboxes[i].getAttribute("class") == "hud-box") {
                hudId = vboxes[i].getAttribute("id");
              }
            }
            loadGroup = self.getLoadGroup(hudId);
          }
          catch (ex) {
            loadGroup = aChannel.QueryInterface(Ci.nsIChannel)
                        .QueryInterface(Ci.nsIRequest).loadGroup;
          }

          if (!loadGroup) {
              return;
          }

          aChannel = aChannel.QueryInterface(Ci.nsIHttpChannel);

          var transCodes = this.httpTransactionCodes;

          var httpActivity = {
            channel: aChannel,
            loadGroup: loadGroup,
            type: aActivityType,
            subType: aActivitySubtype,
            timestamp: aTimestamp,
            extraSizeData: aExtraSizeData,
            extraStringData: aExtraStringData,
            stage: transCodes[aActivitySubtype],
          };
          if (aActivitySubtype ==
              activityDistributor.ACTIVITY_SUBTYPE_REQUEST_HEADER ) {
                // create a unique ID to track this transaction and be able to
                // update the logged node with subsequent http transactions
                httpActivity.httpId = self.sequenceId();
                let loggedNode =
                  self.logActivity("network", aChannel.URI, httpActivity);
                self.httpTransactions[aChannel] =
                  new Number(httpActivity.httpId);
          }
        }
      },

      httpTransactionCodes: {
        0x5001: "REQUEST_HEADER",
        0x5002: "REQUEST_BODY_SENT",
        0x5003: "RESPONSE_START",
        0x5004: "RESPONSE_HEADER",
        0x5005: "RESPONSE_COMPLETE",
        0x5006: "TRANSACTION_CLOSE",
      }
    };

    activityDistributor.addObserver(httpObserver);
  },

  // keep tracked of trasactions where the request header was logged
  // update logged transactions thereafter.
  httpTransactions: {},

  /**
   * Logs network activity
   *
   * @param nsIURI aURI
   * @param object aActivityObject
   * @returns void
   */
  logNetActivity: function HS_logNetActivity(aType, aURI, aActivityObject)
  {
    var displayNode, outputNode, hudId;
    try {
      displayNode =
      this.getDisplayByLoadGroup(aActivityObject.loadGroup,
                                 {URI: aURI}, aActivityObject);
      if (!displayNode) {
        return;
      }
      outputNode = displayNode.querySelectorAll(".hud-output-node")[0];
      hudId = displayNode.getAttribute("id");

      if (!outputNode) {
        outputNode = this.getOutputNodeFromRequest(aActivityObject.request);
        hudId = outputNode.ownerDocument.querySelectorAll(".hud-box")[0].
                getAttribute("id");
      }

      // check if network activity logging is "on":
      if (!this.getFilterState(hudId, "network")) {
        return;
      }

      // get an id to attach to the dom node for lookup of node
      // when updating the log entry with additional http transactions
      var domId = "hud-log-node-" + this.sequenceId();

      var message = { logLevel: aType,
                      activityObj: aActivityObject,
                      hudId: hudId,
                      origin: "network",
                      domId: domId,
                    };
      var msgType = this.getStr("typeNetwork");
      var msg = msgType + " " +
        aActivityObject.channel.requestMethod +
        " " +
        aURI.spec;
      message.message = msg;
      var messageObject =
      this.messageFactory(message, aType, outputNode, aActivityObject);
      this.logMessage(messageObject.messageObject, outputNode, messageObject.messageNode);
    }
    catch (ex) {
      Cu.reportError(ex);
    }
  },

  /**
   * Logs console listener activity
   *
   * @param nsIURI aURI
   * @param object aActivityObject
   * @returns void
   */
  logConsoleActivity: function HS_logConsoleActivity(aURI, aActivityObject)
  {
    var displayNode, outputNode, hudId;
    try {
        var hudIds = this.uriRegistry[aURI.spec];
        hudId = hudIds[0];
    }
    catch (ex) {
      // TODO: uri spec is not tracked becasue the net request is
      // using a different loadGroup
      // see bug 568034
      if (!displayNode) {
        return;
      }
    }

    var _msgLogLevel = this.scriptMsgLogLevel[aActivityObject.flags];
    var msgLogLevel = this.getStr(_msgLogLevel);

    var logLevel = "warn";

    if (aActivityObject.flags in this.scriptErrorFlags) {
      logLevel = this.scriptErrorFlags[aActivityObject.flags];
    }

    // check if we should be logging this message:
    var filterState = this.getFilterState(hudId, logLevel);

    if (!filterState) {
      // Ignore log message
      return;
    }

    // in this case, the "activity object" is the
    // nsIScriptError or nsIConsoleMessage
    var message = {
      activity: aActivityObject,
      origin: "console-listener",
      hudId: hudId,
    };

    var lineColSubs = [aActivityObject.columnNumber,
                       aActivityObject.lineNumber];
    var lineCol = this.getFormatStr("errLineCol", lineColSubs);

    var errFileSubs = [aActivityObject.sourceName];
    var errFile = this.getFormatStr("errFile", errFileSubs);

    var msgCategory = this.getStr("msgCategory");

    message.logLevel = logLevel;
    message.level = logLevel;

    message.message = msgLogLevel + " " +
                      aActivityObject.errorMessage + " " +
                      errFile + " " +
                      lineCol + " " +
                      msgCategory + " " + aActivityObject.category;

    displayNode = this.getHeadsUpDisplay(hudId);
    outputNode = displayNode.querySelectorAll(".hud-output-node")[0];

    var messageObject =
    this.messageFactory(message, message.level, outputNode, aActivityObject);

    this.logMessage(messageObject.messageObject, outputNode, messageObject.messageNode);
  },

  /**
   * Parse log messages for origin or listener type
   * Get the correct outputNode if it exists
   * Finally, call logMessage to write this message to
   * storage and optionally, a DOM output node
   *
   * @param string aType
   * @param nsIURI aURI
   * @param object (or nsIScriptError) aActivityObj
   * @returns void
   */
  logActivity: function HS_logActivity(aType, aURI, aActivityObject)
  {
    var displayNode, outputNode, hudId;

    if (aType == "network") {
      var result = this.logNetActivity(aType, aURI, aActivityObject);
    }
    else if (aType == "console-listener") {
      this.logConsoleActivity(aURI, aActivityObject);
    }
  },

  /**
   * update loadgroup when the window object is re-created
   *
   * @param string aId
   * @param nsILoadGroup aLoadGroup
   * @returns void
   */
  updateLoadGroup: function HS_updateLoadGroup(aId, aLoadGroup)
  {
    if (this.loadGroups[aId] == undefined) {
      this.loadGroups[aId] = { id: aId,
                               loadGroup: Cu.getWeakReference(aLoadGroup) };
    }
    else {
      this.loadGroups[aId].loadGroup = Cu.getWeakReference(aLoadGroup);
    }
  },

  /**
   * gets the load group that corresponds to a HUDId
   *
   * @param string aId
   * @returns nsILoadGroup
   */
  getLoadGroup: function HS_getLoadGroup(aId)
  {
    try {
      return this.loadGroups[aId].loadGroup.get();
    }
    catch (ex) {
      return null;
    }
  },

  /**
   * gets outputNode for a specific heads up display by loadGroup
   *
   * @param nsILoadGroup aLoadGroup
   * @returns nsIDOMNode
   */
  getDisplayByLoadGroup:
  function HS_getDisplayByLoadGroup(aLoadGroup, aChannel, aActivityObject)
  {
    if (!aLoadGroup) {
      return null;
    }
    var trackedLoadGroups = this.getAllLoadGroups();
    var len = trackedLoadGroups.length;
    for (var i = 0; i < len; i++) {
      try {
        var unwrappedLoadGroup =
        XPCNativeWrapper.unwrap(trackedLoadGroups[i].loadGroup);
        if (aLoadGroup == unwrappedLoadGroup) {
          return this.getOutputNodeById(trackedLoadGroups[i].hudId);
        }
      }
      catch (ex) {
        // noop
      }
    }
    // TODO: also need to check parent loadGroup(s) incase of iframe activity?;
    // see bug 568643
    return null;
  },

  /**
   * gets all nsILoadGroups that are being tracked by this service
   * the loadgroups are matched to HUDIds in an object and an array is returned
   * @returns array
   */
  getAllLoadGroups: function HS_getAllLoadGroups()
  {
    var loadGroups = [];
    for (var hudId in this.loadGroups) {
      let loadGroupObj = { loadGroup: this.loadGroups[hudId].loadGroup.get(),
                           hudId: this.loadGroups[hudId].id,
                         };
      loadGroups.push(loadGroupObj);
    }
    return loadGroups;
  },

  /**
   * gets the DOM Node that maps back to what context/tab that
   * activity originated via the URI
   *
   * @param nsIURI aURI
   * @returns nsIDOMNode
   */
  getActivityOutputNode: function HS_getActivityOutputNode(aURI)
  {
    // determine which outputNode activity tied to aURI should be logged to.
    var display = this.getDisplayByURISpec(aURI.spec);
    if (display) {
      return this.getOutputNodeById(display);
    }
    else {
      throw new Error("Cannot get outputNode by hudId");
    }
  },

  /**
   * Wrapper method that generates a LogMessage object
   *
   * @param object aMessage
   * @param string aLevel
   * @param nsIDOMNode aOutputNode
   * @param object aActivityObject
   * @returns
   */
  messageFactory:
  function messageFactory(aMessage, aLevel, aOutputNode, aActivityObject)
  {
    // generate a LogMessage object
    return new LogMessage(aMessage, aLevel, aOutputNode,  aActivityObject);
  },

  /**
   * Initialize the JSTerm object to create a JS Workspace
   *
   * @param nsIDOMWindow aContext
   * @param nsIDOMNode aParentNode
   * @returns void
   */
  initializeJSTerm: function HS_initializeJSTerm(aContext, aParentNode)
  {
    // create Initial JS Workspace:
    var context = Cu.getWeakReference(aContext);
    var firefoxMixin = new JSTermFirefoxMixin(context, aParentNode);
    var jsTerm = new JSTerm(context, aParentNode, firefoxMixin);
    // TODO: injection of additional functionality needs re-thinking/api
    // see bug 559748
  },

  /**
   * Passed a HUDId, the corresponding window is returned
   *
   * @param string aHUDId
   * @returns nsIDOMWindow
   */
  getContentWindowFromHUDId: function HS_getContentWindowFromHUDId(aHUDId)
  {
    var hud = this.getHeadsUpDisplay(aHUDId);
    var nodes = hud.parentNode.childNodes;

    for (var i = 0; i < nodes.length; i++) {
      if (nodes[i].contentWindow) {
        return nodes[i].contentWindow;
      }
    }
    throw new Error("HS_getContentWindowFromHUD: Cannot get contentWindow");
  },

  /**
   * Creates a generator that always returns a unique number for use in the
   * indexes
   *
   * @returns Generator
   */
  createSequencer: function HS_createSequencer(aInt)
  {
    function sequencer(aInt)
    {
      while(1) {
        aInt++;
        yield aInt;
      }
    }
    return sequencer(aInt);
  },

  scriptErrorFlags: {
    0: "error",
    1: "warn",
    2: "exception",
    4: "strict"
  },

  /**
   * replacement strings (L10N)
   */
  scriptMsgLogLevel: {
    0: "typeError",
    1: "typeWarning",
    2: "typeException",
    4: "typeStrict",
  },

  /**
   * onTabClose event handler function
   *
   * @param aEvent
   * @returns void
   */
  onTabClose: function HS_onTabClose(aEvent)
  {
    var browser = aEvent.target;
    var tabId = gBrowser.getNotificationBox(browser).getAttribute("id");
    var hudId = "hud_" + tabId;
    this.unregisterDisplay(hudId);
  },

  /**
   * windowInitializer - checks what Gecko app is running and inits the HUD
   *
   * @param nsIDOMWindow aContentWindow
   * @returns void
   */
  windowInitializer: function HS_WindowInitalizer(aContentWindow)
  {
    var xulWindow = aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation)
      .QueryInterface(Ci.nsIDocShellTreeItem)
      .rootTreeItem
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindow);

    if (aContentWindow.document.location.href == "about:blank" &&
        HUDWindowObserver.initialConsoleCreated == false) {
      // TODO: need to make this work with about:blank in the future
      // see bug 568661
      return;
    }

    let xulWindow = XPCNativeWrapper.unwrap(xulWindow);
    let gBrowser = xulWindow.gBrowser;


    var container = gBrowser.tabContainer;
    container.addEventListener("TabClose", this.onTabClose, false);

    if (gBrowser && !HUDWindowObserver.initialConsoleCreated) {
      HUDWindowObserver.initialConsoleCreated = true;
    }

    let _browser =
      gBrowser.getBrowserForDocument(aContentWindow.document.wrappedJSObject);
    let nBox = gBrowser.getNotificationBox(_browser);
    let nBoxId = nBox.getAttribute("id");
    let hudId = "hud_" + nBoxId;

    if (!this.canActivateContext(hudId)) {
      return;
    }

    this.registerDisplay(hudId, aContentWindow.document.location.href);

    // check if aContentWindow has a console Object
    let _console = aContentWindow.wrappedJSObject.console;
    if (!_console) {
      // no console exists. does the HUD exist?
      let hudNode;
      let childNodes = nBox.childNodes;

      for (var i = 0; i < childNodes.length; i++) {
        let id = childNodes[i].getAttribute("id");
        if (id.split("_")[0] == "hud") {
          hudNode = childNodes[i];
          break;
        }
      }

      if (!hudNode) {
        // get nBox object and call new HUD
        let config = { parentNode: nBox,
                       contentWindow: aContentWindow
                     };

        let _hud = new HeadsUpDisplay(config);

        let hudWeakRef = Cu.getWeakReference(_hud);
        HUDService.registerHUDWeakReference(hudWeakRef, hudId);
      }
      else {
        // only need to attach a console object to the window object
        let config = { hudNode: hudNode,
                       consoleOnly: true,
                       contentWindow: aContentWindow
                     };

        let _hud = new HeadsUpDisplay(config);

        let hudWeakRef = Cu.getWeakReference(_hud);
        HUDService.registerHUDWeakReference(hudWeakRef, hudId);

        aContentWindow.wrappedJSObject.console = _hud.console;
      }
    }
    // capture JS Errors
    this.setOnErrorHandler(aContentWindow);
  }
};

//////////////////////////////////////////////////////////////////////////
// HeadsUpDisplay
//////////////////////////////////////////////////////////////////////////

/*
 * HeadsUpDisplay is an interactive console initialized *per tab*  that
 * displays console log data as well as provides an interactive terminal to
 * manipulate the current tab's document content.
 * */
function HeadsUpDisplay(aConfig)
{
  // sample config: { parentNode: aDOMNode,
  //                  // or
  //                  parentNodeId: "myHUDParent123",
  //
  //                  placement: "appendChild"
  //                  // or
  //                  placement: "insertBefore",
  //                  placementChildNodeIndex: 0,
  //                }
  //
  // or, just create a new console - as there is already a HUD in place
  // config: { hudNode: existingHUDDOMNode,
  //           consoleOnly: true,
  //           contentWindow: aWindow
  //         }

  if (aConfig.consoleOnly) {
    this.HUDBox = aConfig.hudNode;
    this.parentNode = aConfig.hudNode.parentNode;
    this.notificationBox = this.parentNode;
    this.contentWindow = aConfig.contentWindow;
    this.uriSpec = aConfig.contentWindow.location.href;
    this.reattachConsole();
    return;
  }

  this.HUDBox = null;

  if (aConfig.parentNode) {
    // TODO: need to replace these DOM calls with internal functions
    // that operate on each application's node structure
    // better yet, we keep these functions in a "bridgeModule" or the HUDService
    // to keep a registry of nodeGetters for each application
    // see bug 568647
    this.parentNode = aConfig.parentNode;
    this.notificationBox = aConfig.parentNode;
    this.chromeDocument = aConfig.parentNode.ownerDocument;
    this.contentWindow = aConfig.contentWindow;
    this.uriSpec = aConfig.contentWindow.location.href;
    this.hudId = "hud_" + aConfig.parentNode.getAttribute("id");
  }
  else {
    // parentNodeId is the node's id where we attach the HUD
    // TODO: is the "navigator:browser" below used in all Gecko Apps?
    // see bug 568647
    let windowEnum = Services.wm.getEnumerator("navigator:browser");
    let parentNode;
    let contentDocument;
    let contentWindow;
    let chromeDocument;

    // TODO: the following  part is still very Firefox specific
    // see bug 568647

    while (windowEnum.hasMoreElements()) {
      let window = windowEnum.getNext();
      try {
        let gBrowser = window.gBrowser;
        let _browsers = gBrowser.browsers;
        let browserLen = _browsers.length;

        for (var i = 0; i < browserLen; i++) {
          var _notificationBox = gBrowser.getNotificationBox(_browsers[i]);
          this.notificationBox = _notificationBox;

          if (_notificationBox.getAttribute("id") == aConfig.parentNodeId) {
            this.parentNodeId = _notificationBox.getAttribute("id");
            this.hudId = "hud_" + this.parentNodeId;

            parentNode = _notificationBox;

            this.contentDocument =
              _notificationBox.childNodes[0].contentDocument;
            this.contentWindow =
              _notificationBox.childNodes[0].contentWindow;
            this.uriSpec = aConfig.contentWindow.location.href;

            this.chromeDocument =
              _notificationBox.ownerDocument;

            break;
          }
        }
      }
      catch (ex) {
        Cu.reportError(ex);
      }

      if (parentNode) {
        break;
      }
    }
    if (!parentNode) {
      throw new Error(this.ERRORS.PARENTNODE_NOT_FOUND);
    }
    this.parentNode = parentNode;
  }
  // create XUL, HTML and textNode Factories:
  try  {
    this.HTMLFactory = NodeFactory("html", "html", this.chromeDocument);
  }
  catch(ex) {
    Cu.reportError(ex);
  }

  this.XULFactory = NodeFactory("xul", "xul", this.chromeDocument);
  this.textFactory = NodeFactory("text", "xul", this.chromeDocument);

  // create a panel dynamically and attach to the parentNode
  let hudBox = this.createHUD();

  let splitter = this.chromeDocument.createElement("splitter");
  splitter.setAttribute("collapse", "before");
  splitter.setAttribute("resizeafter", "flex");
  splitter.setAttribute("class", "hud-splitter");

  let grippy = this.chromeDocument.createElement("grippy");
  this.notificationBox.insertBefore(splitter,
                                    this.notificationBox.childNodes[1]);
  splitter.appendChild(grippy);

  let console = this.createConsole();

  this.contentWindow.wrappedJSObject.console = console;

  // create the JSTerm input element
  try {
    this.createConsoleInput(this.contentWindow, this.consoleWrap, this.outputNode);
  }
  catch (ex) {
    Cu.reportError(ex);
  }
}

HeadsUpDisplay.prototype = {
  /**
   * L10N shortcut function
   *
   * @param string aName
   * @returns string
   */
  getStr: function HUD_getStr(aName)
  {
    return stringBundle.GetStringFromName(aName);
  },

  /**
   * L10N shortcut function
   *
   * @param string aName
   * @param array aArray
   * @returns string
   */
  getFormatStr: function HUD_getFormatStr(aName, aArray)
  {
    return stringBundle.formatStringFromName(aName, aArray, aArray.length);
  },

  /**
   * The JSTerm object that contains the console's inputNode
   *
   */
  jsterm: null,

  /**
   * creates and attaches the console input node
   *
   * @param nsIDOMWindow aWindow
   * @returns void
   */
  createConsoleInput:
  function HUD_createConsoleInput(aWindow, aParentNode, aExistingConsole)
  {
    var context = Cu.getWeakReference(aWindow);

    if (appName() == "FIREFOX") {
      let outputCSSClassOverride = "hud-msg-node hud-console";
      let mixin = new JSTermFirefoxMixin(context, aParentNode, aExistingConsole, outputCSSClassOverride);
      this.jsterm = new JSTerm(context, aParentNode, mixin);
    }
    else {
      throw new Error("Unsupported Gecko Application");
    }
  },

  /**
   * Re-attaches a console when the contentWindow is recreated
   *
   * @returns void
   */
  reattachConsole: function HUD_reattachConsole()
  {
    this.hudId = this.HUDBox.getAttribute("id");

    // set outputNode
    this.outputNode = this.HUDBox.querySelectorAll(".hud-output-node")[0];

    this.chromeDocument = this.HUDBox.ownerDocument;

    if (this.outputNode) {
      // createConsole
      this.createConsole();
    }
    else {
      throw new Error("Cannot get output node");
    }
  },

  /**
   * Gets the loadGroup for the contentWindow
   *
   * @returns nsILoadGroup
   */
  get loadGroup()
  {
    var loadGroup = this.contentWindow
                    .QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIWebNavigation)
                    .QueryInterface(Ci.nsIDocumentLoader).loadGroup;
    return loadGroup;
  },

  /**
   * Shortcut to make HTML nodes
   *
   * @param string aTag
   * @returns nsIDOMNode
   */
  makeHTMLNode:
  function HUD_makeHTMLNode(aTag)
  {
    try {
      return this.HTMLFactory(aTag);
    }
    catch (ex) {
      var ns = ELEMENT_NS;
      var nsUri = ELEMENT_NS_URI;
      var tag = ns + aTag;
      return this.chromeDocument.createElementNS(nsUri, tag);
    }
  },

  /**
   * Shortcut to make XUL nodes
   *
   * @param string aTag
   * @returns nsIDOMNode
   */
  makeXULNode:
  function HUD_makeXULNode(aTag)
  {
    return this.XULFactory(aTag);
  },

  /**
   * Clears the HeadsUpDisplay output node of any log messages
   *
   * @returns void
   */
  clearConsoleOutput: function HUD_clearConsoleOutput()
  {
    for each (var node in this.outputNode.childNodes) {
      this.outputNode.removeChild(node);
    }
  },

  /**
   * Build the UI of each HeadsUpDisplay
   *
   * @returns nsIDOMNode
   */
  makeHUDNodes: function HUD_makeHUDNodes()
  {
    let self = this;
    this.HUDBox = this.makeXULNode("vbox");
    this.HUDBox.setAttribute("id", this.hudId);
    this.HUDBox.setAttribute("class", "hud-box");

    var height = Math.ceil((this.contentWindow.innerHeight * .33)) + "px";
    var style = "height: " + height + ";";
    this.HUDBox.setAttribute("style", style);

    let outerWrap = this.makeXULNode("vbox");
    outerWrap.setAttribute("class", "hud-outer-wrapper");
    outerWrap.setAttribute("flex", "1");

    let consoleCommandSet = this.makeXULNode("commandset");
    outerWrap.appendChild(consoleCommandSet);

    let consoleWrap = this.makeXULNode("vbox");
    this.consoleWrap = consoleWrap;
    consoleWrap.setAttribute("class", "hud-console-wrapper");
    consoleWrap.setAttribute("flex", "1");

    this.outputNode = this.makeXULNode("vbox");
    this.outputNode.setAttribute("class", "hud-output-node");
    this.outputNode.setAttribute("flex", "1");

    this.filterBox = this.makeXULNode("textbox");
    this.filterBox.setAttribute("class", "hud-filter-box");
    this.filterBox.setAttribute("hudId", this.hudId);

    this.filterClearButton = this.makeXULNode("button");
    this.filterClearButton.setAttribute("class", "hud-filter-clear");
    this.filterClearButton.setAttribute("label", this.getStr("stringFilterClear"));
    this.filterClearButton.setAttribute("hudId", this.hudId);

    this.setFilterTextBoxEvents();

    this.consoleClearButton = this.makeXULNode("button");
    this.consoleClearButton.setAttribute("class", "hud-console-clear");
    this.consoleClearButton.setAttribute("label", this.getStr("btnClear"));
    this.consoleClearButton.setAttribute("buttonType", "clear");
    this.consoleClearButton.setAttribute("hudId", this.hudId);
    var command = "HUDConsoleUI.command(this)";
    this.consoleClearButton.setAttribute("oncommand", command);

    this.filterPrefs = HUDService.getDefaultFilterPrefs(this.hudId);

    let consoleFilterToolbar = this.makeFilterToolbar();
    consoleFilterToolbar.setAttribute("id", "viewGroup");
    this.consoleFilterToolbar = consoleFilterToolbar;
    consoleWrap.appendChild(consoleFilterToolbar);

    consoleWrap.appendChild(this.outputNode);
    outerWrap.appendChild(consoleWrap);

    this.jsTermParentNode = outerWrap;
    this.HUDBox.appendChild(outerWrap);
    return this.HUDBox;
  },


  /**
   * sets the click events for all binary toggle filter buttons
   *
   * @returns void
   */
  setFilterTextBoxEvents: function HUD_setFilterTextBoxEvents()
  {
    var self = this;
    function keyPress(aEvent)
    {
      HUDService.updateFilterText(aEvent.target);
    }
    this.filterBox.addEventListener("keydown", keyPress, false);

    function filterClick(aEvent) {
      self.filterBox.value = "";
    }
    this.filterClearButton.addEventListener("click", filterClick, false);
  },

  /**
   * Make the filter toolbar where we can toggle logging filters
   *
   * @returns nsIDOMNode
   */
  makeFilterToolbar: function HUD_makeFilterToolbar()
  {
    let buttons = ["Network", "CSSParser", "Exception", "Error",
                   "Info", "Warn", "Log",];

    let toolbar = this.makeXULNode("toolbar");
    toolbar.setAttribute("class", "hud-console-filter-toolbar");
    toolbar.setAttribute("mode", "text");

    toolbar.appendChild(this.consoleClearButton);
    let btn;
    for (var i = 0; i < buttons.length; i++) {
      if (buttons[i] == "Clear") {
        btn = this.makeButton(buttons[i], "plain");
      }
      else {
        btn = this.makeButton(buttons[i], "checkbox");
      }
      toolbar.appendChild(btn);
    }
    toolbar.appendChild(this.filterBox);
    toolbar.appendChild(this.filterClearButton);
    return toolbar;
  },

  makeButton: function HUD_makeButton(aName, aType)
  {
    var self = this;
    let prefKey = aName.toLowerCase();
    let btn = this.makeXULNode("toolbarbutton");

    if (aType == "checkbox") {
      btn.setAttribute("type", aType);
    }
    btn.setAttribute("hudId", this.hudId);
    btn.setAttribute("buttonType", prefKey);
    btn.setAttribute("class", "hud-filter-btn");
    let key = "btn" + aName;
    btn.setAttribute("label", this.getStr(key));
    key = "tip" + aName;
    btn.setAttribute("tooltip", this.getStr(key));

    if (aType == "checkbox") {
      btn.setAttribute("checked", this.filterPrefs[prefKey]);
      function toggle(btn) {
        self.consoleFilterCommands.toggle(btn);
      };

      btn.setAttribute("oncommand", "HUDConsoleUI.toggleFilter(this);");
    }
    else {
      var command = "HUDConsoleUI.command(this)";
      btn.setAttribute("oncommand", command);
    }
    return btn;
  },

  createHUD: function HUD_createHUD()
  {
    let self = this;
    if (this.HUDBox) {
      return this.HUDBox;
    }
    else  {
      this.makeHUDNodes();

      let nodes = this.notificationBox.insertBefore(this.HUDBox,
        this.notificationBox.childNodes[0]);

      return this.HUDBox;
    }
  },

  get console() { return this._console || this.createConsole(); },

  getLogCount: function HUD_getLogCount()
  {
    return this.outputNode.childNodes.length;
  },

  getLogNodes: function HUD_getLogNodes()
  {
    return this.outputNode.childNodes;
  },

  /**
   * This console will accept a message, get the tab's meta-data and send
   * properly-formatted message object to the service identifying
   * where it came from, etc...
   *
   * @returns console
   */
  createConsole: function HUD_createConsole()
  {
    return new HUDConsole(this);
  },

  ERRORS: {
    HUD_BOX_DOES_NOT_EXIST: "Heads Up Display does not exist",
    TAB_ID_REQUIRED: "Tab DOM ID is required",
    PARENTNODE_NOT_FOUND: "parentNode element not found"
  }
};


//////////////////////////////////////////////////////////////////////////////
// HUDConsole factory function
//////////////////////////////////////////////////////////////////////////////

/**
 * The console object that is attached to each contentWindow
 *
 * @param object aHeadsUpDisplay
 * @returns object
 */
function HUDConsole(aHeadsUpDisplay)
{
  let hud = aHeadsUpDisplay;
  let hudId = hud.hudId;
  let outputNode = hud.outputNode;
  let chromeDocument = hud.chromeDocument;
  let makeHTMLNode = hud.makeHTMLNode;

  aHeadsUpDisplay._console = this;

  HUDService.updateLoadGroup(hudId, hud.loadGroup);

  let sendToHUDService = function console_send(aLevel, aArguments)
  {
    // check to see if logging is on for this level before logging!
    var filterState = HUDService.getFilterState(hudId, aLevel);

    if (!filterState) {
      // Ignoring log message
      return;
    }

    let ts = ConsoleUtils.timestamp();
    let messageNode = hud.makeHTMLNode("div");

    let klass = "hud-msg-node hud-" + aLevel;

    messageNode.setAttribute("class", klass);

    let argumentArray = [];
    for (var i = 0; i < aArguments.length; i++) {
      argumentArray.push(aArguments[i]);
    }

    let message = argumentArray.join(' ');
    let timestampedMessage = ConsoleUtils.timestampString(ts) + ": " +
      message;

    messageNode.appendChild(chromeDocument.createTextNode(timestampedMessage));

    // need a constructor here to properly set all attrs
    let messageObject = {
      logLevel: aLevel,
      hudId: hud.hudId,
      message: message,
      timestamp: ts,
      origin: "HUDConsole",
    };

    HUDService.logMessage(messageObject, hud.outputNode, messageNode);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Console API.
  this.log = function console_log()
  {
    sendToHUDService("log", arguments);
  },

  this.info = function console_info()
  {
    sendToHUDService("info", arguments);
  },

  this.warn = function console_warn()
  {
    sendToHUDService("warn", arguments);
  },

  this.error = function console_error()
  {
    sendToHUDService("error", arguments);
  },

  this.exception = function console_exception()
  {
    sendToHUDService("exception", arguments);
  }
};

/**
 * Creates a DOM Node factory for either XUL nodes or HTML nodes - as
 * well as textNodes
 * @param   aFactoryType
 *          "xul" or "html"
 * @returns DOM Node Factory function
 */
function NodeFactory(aFactoryType, aNameSpace, aDocument)
{
  // aDocument is presumed to be a XULDocument
  const ELEMENT_NS_URI = "http://www.w3.org/1999/xhtml";

  if (aFactoryType == "text") {
    function factory(aText) {
      return aDocument.createTextNode(aText);
    }
    return factory;
  }
  else {
    if (aNameSpace == "xul") {
      function factory(aTag)
      {
        return aDocument.createElement(aTag);
      }
      return factory;
    }
    else {
      function factory(aTag)
      {
        var tag = "html:" + aTag;
        return aDocument.createElementNS(ELEMENT_NS_URI, tag);
      }
      return factory;
    }
  }
}

//////////////////////////////////////////////////////////////////////////
// JS Completer
//////////////////////////////////////////////////////////////////////////

const STATE_NORMAL = 0;
const STATE_QUOTE = 2;
const STATE_DQUOTE = 3;

const OPEN_BODY = '{[('.split('');
const CLOSE_BODY = '}])'.split('');
const OPEN_CLOSE_BODY = {
  '{': '}',
  '[': ']',
  '(': ')'
};

/**
 * Analyses a given string to find the last statement that is interesting for
 * later completion.
 *
 * @param   string aStr
 *          A string to analyse.
 *
 * @returns object
 *          If there was an error in the string detected, then a object like
 *
 *            { err: "ErrorMesssage" }
 *
 *          is returned, otherwise a object like
 *
 *            {
 *              state: STATE_NORMAL|STATE_QUOTE|STATE_DQUOTE,
 *              startPos: index of where the last statement begins
 *            }
 */
function findCompletionBeginning(aStr)
{
  let bodyStack = [];

  let state = STATE_NORMAL;
  let start = 0;
  let c;
  for (let i = 0; i < aStr.length; i++) {
    c = aStr[i];

    switch (state) {
      // Normal JS state.
      case STATE_NORMAL:
        if (c == '"') {
          state = STATE_DQUOTE;
        }
        else if (c == '\'') {
          state = STATE_QUOTE;
        }
        else if (c == ';') {
          start = i + 1;
        }
        else if (c == ' ') {
          start = i + 1;
        }
        else if (OPEN_BODY.indexOf(c) != -1) {
          bodyStack.push({
            token: c,
            start: start
          });
          start = i + 1;
        }
        else if (CLOSE_BODY.indexOf(c) != -1) {
          var last = bodyStack.pop();
          if (OPEN_CLOSE_BODY[last.token] != c) {
            return {
              err: "syntax error"
            };
          }
          if (c == '}') {
            start = i + 1;
          }
          else {
            start = last.start;
          }
        }
        break;

      // Double quote state > " <
      case STATE_DQUOTE:
        if (c == '\\') {
          i ++;
        }
        else if (c == '\n') {
          return {
            err: "unterminated string literal"
          };
        }
        else if (c == '"') {
          state = STATE_NORMAL;
        }
        break;

      // Single quoate state > ' <
      case STATE_QUOTE:
        if (c == '\\') {
          i ++;
        }
        else if (c == '\n') {
          return {
            err: "unterminated string literal"
          };
          return;
        }
        else if (c == '\'') {
          state = STATE_NORMAL;
        }
        break;
    }
  }

  return {
    state: state,
    startPos: start
  };
}

/**
 * Provides a list of properties, that are possible matches based on the passed
 * scope and inputValue.
 *
 * @param object aScope
 *        Scope to use for the completion.
 *
 * @param string aInputValue
 *        Value that should be completed.
 *
 * @returns null or object
 *          If no completion valued could be computed, null is returned,
 *          otherwise a object with the following form is returned:
 *            {
 *              matches: [ string, string, string ],
 *              matchProp: Last part of the inputValue that was used to find
 *                         the matches-strings.
 *            }
 */
function JSPropertyProvider(aScope, aInputValue)
{
  let obj = aScope;

  // Analyse the aInputValue and find the beginning of the last part that
  // should be completed.
  let beginning = findCompletionBeginning(aInputValue);

  // There was an error analysing the string.
  if (beginning.err) {
    return null;
  }

  // If the current state is not STATE_NORMAL, then we are inside of an string
  // which means that no completion is possible.
  if (beginning.state != STATE_NORMAL) {
    return null;
  }

  let completionPart = aInputValue.substring(beginning.startPos);

  // Don't complete on just an empty string.
  if (completionPart.trim() == "") {
    return null;
  }

  let properties = completionPart.split('.');
  let matchProp;
  if (properties.length > 1) {
      matchProp = properties[properties.length - 1].trimLeft();
      properties.pop();
      for each (var prop in properties) {
        prop = prop.trim();

        // If obj is undefined or null, then there is no change to run
        // completion on it. Exit here.
        if (typeof obj === "undefined" || obj === null) {
          return null;
        }

        // Check if prop is a getter function on obj. Functions can change other
        // stuff so we can't execute them to get the next object. Stop here.
        if (obj.__lookupGetter__(prop)) {
          return null;
        }
        obj = obj[prop];
      }
  }
  else {
    matchProp = properties[0].trimLeft();
  }

  // If obj is undefined or null, then there is no change to run
  // completion on it. Exit here.
  if (typeof obj === "undefined" || obj === null) {
    return null;
  }

  let matches = [];
  for (var prop in obj) {
    matches.push(prop);
  }

  matches = matches.filter(function(item) {
    return item.indexOf(matchProp) == 0;
  }).sort();

  return {
    matchProp: matchProp,
    matches: matches
  };
}

//////////////////////////////////////////////////////////////////////////
// JSTerm
//////////////////////////////////////////////////////////////////////////

/**
 * JSTerm
 *
 * JavaScript Terminal: creates input nodes for console code interpretation
 * and 'JS Workspaces'
 */

/**
 * Create a JSTerminal or attach a JSTerm input node to an existing output node
 *
 *
 *
 * @param object aContext
 *        Usually nsIDOMWindow, but doesn't have to be
 * @param nsIDOMNode aParentNode
 * @param object aMixin
 *        Gecko-app (or Jetpack) specific utility object
 * @returns void
 */
function JSTerm(aContext, aParentNode, aMixin)
{
  // set the context, attach the UI by appending to aParentNode

  this.application = appName();
  this.context = aContext;
  this.parentNode = aParentNode;
  this.mixins = aMixin;

  this.elementFactory =
    NodeFactory("html", "html", aParentNode.ownerDocument);

  this.xulElementFactory =
    NodeFactory("xul", "xul", aParentNode.ownerDocument);

  this.textFactory = NodeFactory("text", "xul", aParentNode.ownerDocument);

  this.setTimeout = aParentNode.ownerDocument.defaultView.setTimeout;

  this.historyIndex = 0;
  this.historyPlaceHolder = 0;  // this.history.length;
  this.log = LogFactory("*** JSTerm:");
  this.init();
}

JSTerm.prototype = {

  propertyProvider: JSPropertyProvider,

  COMPLETE_FORWARD: 0,
  COMPLETE_BACKWARD: 1,
  COMPLETE_HINT_ONLY: 2,

  init: function JST_init()
  {
    this.createSandbox();
    this.inputNode = this.mixins.inputNode;
    this.scrollToNode = this.mixins.scrollToNode;
    let eventHandler = this.keyDown();
    this.inputNode.addEventListener('keypress', eventHandler, false);
    this.outputNode = this.mixins.outputNode;
    if (this.mixins.cssClassOverride) {
      this.cssClassOverride = this.mixins.cssClassOverride;
    }
  },

  get codeInputString()
  {
    // TODO: filter the input for windows line breaks, conver to unix
    // see bug 572812
    return this.inputNode.value;
  },

  generateUI: function JST_generateUI()
  {
    this.mixins.generateUI();
  },

  attachUI: function JST_attachUI()
  {
    this.mixins.attachUI();
  },

  createSandbox: function JST_setupSandbox()
  {
    // create a JS Sandbox out of this.context
    this._window.wrappedJSObject.jsterm = {};
    this.console = this._window.wrappedJSObject.console;
    this.sandbox = new Cu.Sandbox(this._window);
    this.sandbox.window = this._window;
    this.sandbox.console = this.console;
    this.sandbox.__proto__ = this._window.wrappedJSObject;
  },

  get _window()
  {
    return this.context.get().QueryInterface(Ci.nsIDOMWindowInternal);
  },

  execute: function JST_execute(aExecuteString)
  {
    // attempt to execute the content of the inputNode
    var str = aExecuteString || this.inputNode.value;
    if (!str) {
      this.console.log("no value to execute");
      return;
    }

    this.writeOutput(str);

    try {
      var execStr = "with(window) {" + str + "}";
      var result =
        Cu.evalInSandbox(execStr,  this.sandbox, "default", "HUD Console", 1);

      if (result || result === false || result === " ") {
        this.writeOutput(result);
      }
      else if (result === undefined) {
        this.writeOutput("undefined");
      }
      else if (result === null) {
        this.writeOutput("null");
      }
    }
    catch (ex) {
      if (ex) {
        this.console.error(ex);
      }
    }

    this.history.push(str);
    this.historyIndex++;
    this.historyPlaceHolder = this.history.length;
    this.inputNode.value = "";
  },

  writeOutput: function JST_writeOutput(aOutputMessage)
  {
    var node = this.elementFactory("div");
    if (this.cssClassOverride) {
      node.setAttribute("class", this.cssClassOverride);
    }
    else {
      node.setAttribute("class", "jsterm-output-line");
    }
    var textNode = this.textFactory(aOutputMessage);
    node.appendChild(textNode);
    this.outputNode.appendChild(node);
    node.scrollIntoView(false);
  },

  clearOutput: function JST_clearOutput()
  {
    let outputNode = this.outputNode;

    while (outputNode.firstChild) {
      outputNode.removeChild(outputNode.firstChild);
    }
  },

  keyDown: function JSTF_keyDown(aEvent)
  {
    var self = this;
    function handleKeyDown(aEvent) {
      // ctrl-a
      var setTimeout = aEvent.target.ownerDocument.defaultView.setTimeout;
      var target = aEvent.target;
      var tmp;

      if (aEvent.ctrlKey) {
        switch (aEvent.charCode) {
          case 97:
            // control-a
            tmp = self.codeInputString;
            setTimeout(function() {
              self.inputNode.value = tmp;
              self.inputNode.setSelectionRange(0, 0);
            }, 0);
            break;
          case 101:
            // control-e
            tmp = self.codeInputString;
            self.inputNode.value = "";
            setTimeout(function(){
              var endPos = tmp.length + 1;
              self.inputNode.value = tmp;
            }, 0);
            break;
          default:
            return;
        }
        return;
      }
      else if (aEvent.shiftKey && aEvent.keyCode == 13) {
        // shift return
        // TODO: expand the inputNode height by one line
        return;
      }
      else {
        switch(aEvent.keyCode) {
          case 13:
            // return
            self.execute();
            break;
          case 38:
            // up arrow: history previous
            if (self.caretInFirstLine()){
              self.historyPeruse(true);
              if (aEvent.cancelable) {
                let inputEnd = self.inputNode.value.length;
                self.inputNode.setSelectionRange(inputEnd, inputEnd);
                aEvent.preventDefault();
              }
            }
            break;
          case 40:
            // down arrow: history next
            if (self.caretInLastLine()){
              self.historyPeruse(false);
              if (aEvent.cancelable) {
                let inputEnd = self.inputNode.value.length;
                self.inputNode.setSelectionRange(inputEnd, inputEnd);
                aEvent.preventDefault();
              }
            }
            break;
          case 9:
            // tab key
            // If there are more than one possible completion, pressing tab
            // means taking the next completion, shift_tab means taking
            // the previous completion.
            if (aEvent.shiftKey) {
              self.complete(self.COMPLETE_BACKWARD);
            }
            else {
              self.complete(self.COMPLETE_FORWARD);
            }
            var bool = aEvent.cancelable;
            if (bool) {
              aEvent.preventDefault();
            }
            else {
              // noop
            }
            aEvent.target.focus();
            break;
          case 8:
            // backspace key
          case 46:
            // delete key
            // necessary so that default is not reached.
            break;
          default:
            // all not handled keys
            // Store the current inputNode value. If the value is the same
            // after keyDown event was handled (after 0ms) then the user
            // moved the cursor. If the value changed, then call the complete
            // function to show completion on new value.
            var value = self.inputNode.value;
            setTimeout(function() {
              if (self.inputNode.value !== value) {
                self.complete(self.COMPLETE_HINT_ONLY);
              }
            }, 0);
            break;
        }
        return;
      }
    }
    return handleKeyDown;
  },

  historyPeruse: function JST_historyPeruse(aFlag) {
    if (!this.history.length) {
      return;
    }

    // Up Arrow key
    if (aFlag) {
      if (this.historyPlaceHolder <= 0) {
        return;
      }

      let inputVal = this.history[--this.historyPlaceHolder];
      if (inputVal){
        this.inputNode.value = inputVal;
      }
    }
    // Down Arrow key
    else {
      if (this.historyPlaceHolder == this.history.length - 1) {
        this.historyPlaceHolder ++;
        this.inputNode.value = "";
        return;
      }
      else if (this.historyPlaceHolder >= (this.history.length)) {
        return;
      }
      else {
        let inputVal = this.history[++this.historyPlaceHolder];
        if (inputVal){
          this.inputNode.value = inputVal;
        }
      }
    }
  },

  refocus: function JSTF_refocus()
  {
    this.inputNode.blur();
    this.inputNode.focus();
  },

  caretInFirstLine: function JSTF_caretInFirstLine()
  {
    var firstLineBreak = this.codeInputString.indexOf("\n");
    return ((firstLineBreak == -1) ||
            (this.codeInputString.selectionStart <= firstLineBreak));
  },

  caretInLastLine: function JSTF_caretInLastLine()
  {
    var lastLineBreak = this.codeInputString.lastIndexOf("\n");
    return (this.inputNode.selectionEnd > lastLineBreak);
  },

  history: [],

  // Stores the data for the last completion.
  lastCompletion: null,

  /**
   * Completes the current typed text in the inputNode. Completion is performed
   * only if the selection/cursor is at the end of the string. If no completion
   * is found, the current inputNode value and cursor/selection stay.
   *
   * @param int type possible values are
   *    - this.COMPLETE_FORWARD: If there is more than one possible completion
   *          and the input value stayed the same compared to the last time this
   *          function was called, then the next completion of all possible
   *          completions is used. If the value changed, then the first possible
   *          completion is used and the selection is set from the current
   *          cursor position to the end of the completed text.
   *          If there is only one possible completion, then this completion
   *          value is used and the cursor is put at the end of the completion.
   *    - this.COMPLETE_BACKWARD: Same as this.COMPLETE_FORWARD but if the
   *          value stayed the same as the last time the function was called,
   *          then the previous completion of all possible completions is used.
   *    - this.COMPLETE_HINT_ONLY: If there is more than one possible
   *          completion and the input value stayed the same compared to the
   *          last time this function was called, then the same completion is
   *          used again. If there is only one possible completion, then
   *          the inputNode.value is set to this value and the selection is set
   *          from the current cursor position to the end of the completed text.
   *
   * @returns void
   */
  complete: function JSTF_complete(type)
  {
    let inputNode = this.inputNode;
    let inputValue = inputNode.value;
    let selStart = inputNode.selectionStart, selEnd = inputNode.selectionEnd;

    // 'Normalize' the selection so that end is always after start.
    if (selStart > selEnd) {
      let newSelEnd = selStart;
      selStart = selEnd;
      selEnd = newSelEnd;
    }

    // Only complete if the selection is at the end of the input.
    if (selEnd != inputValue.length) {
      this.lastCompletion = null;
      return;
    }

    // Remove the selected text from the inputValue.
    inputValue = inputValue.substring(0, selStart);

    let matches;
    let matchIndexToUse;
    let matchOffset;
    let completionStr;

    // If there is a saved completion from last time and the used value for
    // completion stayed the same, then use the stored completion.
    if (this.lastCompletion && inputValue == this.lastCompletion.value) {
      matches = this.lastCompletion.matches;
      matchOffset = this.lastCompletion.matchOffset;
      if (type === this.COMPLETE_BACKWARD) {
        this.lastCompletion.index --;
      }
      else if (type === this.COMPLETE_FORWARD) {
        this.lastCompletion.index ++;
      }
      matchIndexToUse = this.lastCompletion.index;
    }
    else {
      // Look up possible completion values.
      let completion = this.propertyProvider(this.sandbox.window, inputValue);
      if (!completion) {
        return;
      }
      matches = completion.matches;
      matchIndexToUse = 0;
      matchOffset = completion.matchProp.length
      // Store this match;
      this.lastCompletion = {
        index: 0,
        value: inputValue,
        matches: matches,
        matchOffset: matchOffset
      };
    }

    if (matches.length != 0) {
      // Ensure that the matchIndexToUse is always a valid array index.
      if (matchIndexToUse < 0) {
        matchIndexToUse = matches.length + (matchIndexToUse % matches.length);
        if (matchIndexToUse == matches.length) {
          matchIndexToUse = 0;
        }
      }
      else {
        matchIndexToUse = matchIndexToUse % matches.length;
      }

      completionStr = matches[matchIndexToUse].substring(matchOffset);
      this.inputNode.value = inputValue +  completionStr;

      selEnd = inputValue.length + completionStr.length;

      // If there is more than one possible completion or the completed part
      // should get displayed only without moving the cursor at the end of the
      // completion.
      if (matches.length > 1 || type === this.COMPLETE_HINT_ONLY) {
        inputNode.setSelectionRange(selStart, selEnd);
      }
      else {
        inputNode.setSelectionRange(selEnd, selEnd);
      }
    }
  }
};

/**
 * JSTermFirefoxMixin
 *
 * JavaScript Terminal Firefox Mixin
 *
 */
function
JSTermFirefoxMixin(aContext,
                   aParentNode,
                   aExistingConsole,
                   aCSSClassOverride)
{
  // aExisting Console is the existing outputNode to use in favor of
  // creating a new outputNode - this is so we can just attach the inputNode to
  // a normal HeadsUpDisplay console output, and re-use code.
  this.cssClassOverride = aCSSClassOverride;
  this.context = aContext;
  this.parentNode = aParentNode;
  this.existingConsoleNode = aExistingConsole;
  this.setTimeout = aParentNode.ownerDocument.defaultView.setTimeout;

  if (aParentNode.ownerDocument) {
    this.elementFactory =
      NodeFactory("html", "html", aParentNode.ownerDocument);

    this.xulElementFactory =
      NodeFactory("xul", "xul", aParentNode.ownerDocument);

    this.textFactory = NodeFactory("text", "xul", aParentNode.ownerDocument);
    this.generateUI();
    this.attachUI();
  }
  else {
    throw new Error("aParentNode should be a DOM node with an ownerDocument property ");
  }
}

JSTermFirefoxMixin.prototype = {
  /**
   * Generates and attaches the UI for an entire JS Workspace or
   * just the input node used under the console output
   *
   * @returns void
   */
  generateUI: function JSTF_generateUI()
  {
    let inputNode = this.xulElementFactory("textbox");
    inputNode.setAttribute("class", "jsterm-input-node");

    if (this.existingConsoleNode == undefined) {
      // create elements
      let term = this.elementFactory("div");
      term.setAttribute("class", "jsterm-wrapper-node");
      term.setAttribute("flex", "1");

      let outputNode = this.elementFactory("div");
      outputNode.setAttribute("class", "jsterm-output-node");

      let scrollToNode = this.elementFactory("div");
      scrollToNode.setAttribute("class", "jsterm-scroll-to-node");

      // construction
      outputNode.appendChild(scrollToNode);
      term.appendChild(outputNode);
      term.appendChild(inputNode);

      this.scrollToNode = scrollToNode;
      this.outputNode = outputNode;
      this.inputNode = inputNode;
      this.term = term;
    }
    else {
      this.inputNode = inputNode;
      this.term = inputNode;
      this.outputNode = this.existingConsoleNode;
    }
  },

  get inputValue()
  {
    return this.inputNode.value;
  },

  attachUI: function JSTF_attachUI()
  {
    this.parentNode.appendChild(this.term);
  }
};

/**
 * LogMessage represents a single message logged to the "outputNode" console
 */
function LogMessage(aMessage, aLevel, aOutputNode, aActivityObject)
{
  if (!aOutputNode || !aOutputNode.ownerDocument) {
    throw new Error("aOutputNode is required and should be type nsIDOMNode");
  }
  if (!aMessage.origin) {
    throw new Error("Cannot create and log a message without an origin");
  }
  this.message = aMessage;
  if (aMessage.domId) {
    // domId is optional - we only need it if the logmessage is
    // being asynchronously updated
    this.domId = aMessage.domId;
  }
  this.activityObject = aActivityObject;
  this.outputNode = aOutputNode;
  this.level = aLevel;
  this.origin = aMessage.origin;

  this.elementFactory =
  NodeFactory("html", "html", aOutputNode.ownerDocument);

  this.xulElementFactory =
  NodeFactory("xul", "xul", aOutputNode.ownerDocument);

  this.textFactory = NodeFactory("text", "xul", aOutputNode.ownerDocument);

  this.createLogNode();
}

LogMessage.prototype = {

  /**
   * create a console log div node
   *
   * @returns nsIDOMNode
   */
  createLogNode: function LM_createLogNode()
  {
    this.messageNode = this.elementFactory("div");

    var ts = ConsoleUtils.timestamp();
    var timestampedMessage = ConsoleUtils.timestampString(ts) + ": " +
      this.message.message;
    var messageTxtNode = this.textFactory(timestampedMessage);

    this.messageNode.appendChild(messageTxtNode);

    var klass = "hud-msg-node hud-" + this.level;
    this.messageNode.setAttribute("class", klass);

    var self = this;

    var messageObject = {
      logLevel: self.level,
      message: self.message,
      timestamp: ts,
      activity: self.activityObject,
      origin: self.origin,
      hudId: self.message.hudId,
    };

    this.messageObject = messageObject;
  }
};


/**
 * Firefox-specific Application Hooks.
 * Each Gecko-based application will need an object like this in
 * order to use the Heads Up Display
 */
function FirefoxApplicationHooks()
{ }

FirefoxApplicationHooks.prototype = {

  /**
   * Firefox-specific method for getting an array of chrome Window objects
   */
  get chromeWindows()
  {
    var windows = [];
    var enumerator = Services.ww.getWindowEnumerator(null);
    while (enumerator.hasMoreElements()) {
      windows.push(enumerator.getNext());
    }
    return windows;
  },

  /**
   * Firefox-specific method for getting the DOM node (per tab) that message
   * nodes are appended to.
   * @param aId
   *        The DOM node's id.
   */
  getOutputNodeById: function FAH_getOutputNodeById(aId)
  {
    if (!aId) {
      throw new Error("FAH_getOutputNodeById: id is null!!");
    }
    var enumerator = Services.ww.getWindowEnumerator(null);
    while (enumerator.hasMoreElements()) {
      let window = enumerator.getNext();
      let node = window.document.getElementById(aId);
      if (node) {
        return node;
      }
    }
    throw new Error("Cannot get outputNode by id");
  },

  /**
   * gets the current contentWindow (Firefox-specific)
   *
   * @returns nsIDOMWindow
   */
  getCurrentContext: function FAH_getCurrentContext()
  {
    return Services.wm.getMostRecentWindow("navigator:browser");
  }
};

//////////////////////////////////////////////////////////////////////////////
// Utility functions used by multiple callers
//////////////////////////////////////////////////////////////////////////////

/**
 * ConsoleUtils: a collection of globally used functions
 *
 */

ConsoleUtils = {

  /**
   * Generates a millisecond resolution timestamp.
   *
   * @returns integer
   */
  timestamp: function ConsoleUtils_timestamp()
  {
    return Date.now();
  },

  /**
   * Generates a formatted timestamp string for displaying in console messages.
   *
   * @param integer [ms] Optional, allows you to specify the timestamp in 
   * milliseconds since the UNIX epoch.
   * @returns string The timestamp formatted for display.
   */
  timestampString: function ConsoleUtils_timestampString(ms)
  {
    // TODO: L10N see bug 568656
    var d = new Date(ms ? ms : null);

    function pad(n, mil)
    {
      if (mil) {
        return n < 100 ? "0" + n : n;
      }
      else {
        return n < 10 ? "0" + n : n;
      }
    }

    return pad(d.getHours()) + ":"
      + pad(d.getMinutes()) + ":"
      + pad(d.getSeconds()) + ":"
      + pad(d.getMilliseconds(), true);
  },

  /**
   * Hides a log message by changing its class
   *
   * @param nsIDOMNode aMessageNode
   * @returns nsIDOMNode
   */
  hideLogMessage: function ConsoleUtils_hideLogMessage(aMessageNode) {
    var klass = aMessageNode.getAttribute("class");
    klass += " hud-hidden";
    aMessageNode.setAttribute("class", klass);
    return aMessageNode;
  }
};

/**
 * Creates a DOM Node factory for either XUL nodes or HTML nodes - as
 * well as textNodes
 * @param   aFactoryType
 *          "xul", "html" or "text"
 * @returns DOM Node Factory function
 */
function NodeFactory(aFactoryType, aNameSpace, aDocument)
{
  // aDocument is presumed to be a XULDocument
  const ELEMENT_NS_URI = "http://www.w3.org/1999/xhtml";

  if (aFactoryType == "text") {
    function factory(aText) {
      return aDocument.createTextNode(aText);
    }
    return factory;
  }
  else {
    if (aNameSpace == "xul") {
      function factory(aTag) {
        return aDocument.createElement(aTag);
      }
      return factory;
    }
    else {
      function factory(aTag) {
        var tag = "html:" + aTag;
        return aDocument.createElementNS(ELEMENT_NS_URI, tag);
      }
      return factory;
    }
  }
}


//////////////////////////////////////////////////////////////////////////
// HeadsUpDisplayUICommands
//////////////////////////////////////////////////////////////////////////

HeadsUpDisplayUICommands = {
  toggleHUD: function UIC_toggleHUD() {
    var window = HUDService.currentContext();
    var gBrowser = window.gBrowser;
    var linkedBrowser = gBrowser.selectedTab.linkedBrowser;
    var tabId = gBrowser.getNotificationBox(linkedBrowser).getAttribute("id");
    var hudId = "hud_" + tabId;
    var hud = gBrowser.selectedTab.ownerDocument.getElementById(hudId);
    if (hud) {
      HUDService.deactivateHUDForContext(gBrowser.selectedTab);
    }
    else {
      HUDService.activateHUDForContext(gBrowser.selectedTab);
    }
  },

  toggleFilter: function UIC_toggleFilter(aButton) {
    var filter = aButton.getAttribute("buttonType");
    var hudId = aButton.getAttribute("hudId");
    var state = HUDService.getFilterState(hudId, filter);
    if (state) {
      HUDService.setFilterState(hudId, filter, false);
      aButton.setAttribute("checked", false);
    }
    else {
      HUDService.setFilterState(hudId, filter, true);
      aButton.setAttribute("checked", true);
    }
  },

  command: function UIC_command(aButton) {
    var filter = aButton.getAttribute("buttonType");
    var hudId = aButton.getAttribute("hudId");
    if (filter == "clear") {
      HUDService.clearDisplay(hudId);
    }
  },

};

//////////////////////////////////////////////////////////////////////////
// ConsoleStorage
//////////////////////////////////////////////////////////////////////////

var prefs = Services.prefs;

const GLOBAL_STORAGE_INDEX_ID = "GLOBAL_CONSOLE";
const PREFS_BRANCH_PREF = "devtools.hud.display.filter";
const PREFS_PREFIX = "devtools.hud.display.filter.";
const PREFS = { network: PREFS_PREFIX + "network",
                cssparser: PREFS_PREFIX + "cssparser",
                exception: PREFS_PREFIX + "exception",
                error: PREFS_PREFIX + "error",
                info: PREFS_PREFIX + "info",
                warn: PREFS_PREFIX + "warn",
                log: PREFS_PREFIX + "log",
                global: PREFS_PREFIX + "global",
              };

function ConsoleStorage()
{
  this.sequencer = null;
  this.consoleDisplays = {};
  // each display will have an index that tracks each ConsoleEntry
  this.displayIndexes = {};
  this.globalStorageIndex = [];
  this.globalDisplay = {};
  this.createDisplay(GLOBAL_STORAGE_INDEX_ID);
  // TODO: need to create a method that truncates the message
  // see bug 570543

  // store an index of display prefs
  this.displayPrefs = {};

  // check prefs for existence, create & load if absent, load them if present
  let filterPrefs;
  let defaultDisplayPrefs;

  try {
    filterPrefs = prefs.getBoolPref(PREFS_BRANCH_PREF);
  }
  catch (ex) {
    filterPrefs = false;
  }

  // TODO: for FINAL release,
  // use the sitePreferencesService to save specific site prefs
  // see bug 570545

  if (filterPrefs) {
    defaultDisplayPrefs = {
      network: (prefs.getBoolPref(PREFS.network) ? true: false),
      cssparser: (prefs.getBoolPref(PREFS.cssparser) ? true: false),
      exception: (prefs.getBoolPref(PREFS.exception) ? true: false),
      error: (prefs.getBoolPref(PREFS.error) ? true: false),
      info: (prefs.getBoolPref(PREFS.info) ? true: false),
      warn: (prefs.getBoolPref(PREFS.warn) ? true: false),
      log: (prefs.getBoolPref(PREFS.log) ? true: false),
      global: (prefs.getBoolPref(PREFS.global) ? true: false),
    };
  }
  else {
    prefs.setBoolPref(PREFS_BRANCH_PREF, false);
    // default prefs for each HeadsUpDisplay
    prefs.setBoolPref(PREFS.network, true);
    prefs.setBoolPref(PREFS.cssparser, true);
    prefs.setBoolPref(PREFS.exception, true);
    prefs.setBoolPref(PREFS.error, true);
    prefs.setBoolPref(PREFS.info, true);
    prefs.setBoolPref(PREFS.warn, true);
    prefs.setBoolPref(PREFS.log, true);
    prefs.setBoolPref(PREFS.global, false);

    defaultDisplayPrefs = {
      network: prefs.getBoolPref(PREFS.network),
      cssparser: prefs.getBoolPref(PREFS.cssparser),
      exception: prefs.getBoolPref(PREFS.exception),
      error: prefs.getBoolPref(PREFS.error),
      info: prefs.getBoolPref(PREFS.info),
      warn: prefs.getBoolPref(PREFS.warn),
      log: prefs.getBoolPref(PREFS.log),
      global: prefs.getBoolPref(PREFS.global),
    };
  }
  this.defaultDisplayPrefs = defaultDisplayPrefs;
}

ConsoleStorage.prototype = {

  updateDefaultDisplayPrefs:
  function CS_updateDefaultDisplayPrefs(aPrefsObject) {
    prefs.setBoolPref(PREFS.network, (aPrefsObject.network ? true : false));
    prefs.setBoolPref(PREFS.cssparser, (aPrefsObject.cssparser ? true : false));
    prefs.setBoolPref(PREFS.exception, (aPrefsObject.exception ? true : false));
    prefs.setBoolPref(PREFS.error, (aPrefsObject.error ? true : false));
    prefs.setBoolPref(PREFS.info, (aPrefsObject.info ? true : false));
    prefs.setBoolPref(PREFS.warn, (aPrefsObject.warn ? true : false));
    prefs.setBoolPref(PREFS.log, (aPrefsObject.log ? true : false));
    prefs.setBoolPref(PREFS.global, (aPrefsObject.global ? true : false));
  },

  sequenceId: function CS_sequencerId()
  {
    if (!this.sequencer) {
      this.sequencer = this.createSequencer();
    }
    return this.sequencer.next();
  },

  createSequencer: function CS_createSequencer()
  {
    function sequencer(aInt) {
      while(1) {
        aInt++;
        yield aInt;
      }
    }
    return sequencer(-1);
  },

  globalStore: function CS_globalStore(aIndex)
  {
    return this.displayStore(GLOBAL_CONSOLE_DOM_NODE_ID);
  },

  displayStore: function CS_displayStore(aId)
  {
    var self = this;
    var idx = -1;
    var id = aId;
    var aLength = self.displayIndexes[id].length;

    function displayStoreGenerator(aInt, aLength)
    {
      // create a generator object to iterate through any of the display stores
      // from any index-starting-point
      while(1) {
        // throw if we exceed the length of displayIndexes?
        aInt++;
        var indexIt = self.displayIndexes[id];
        var index = indexIt[aInt];
        if (aLength < aInt) {
          // try to see if we have more entries:
          var newLength = self.displayIndexes[id].length;
          if (newLength > aLength) {
            aLength = newLength;
          }
          else {
            throw new StopIteration();
          }
        }
        var entry = self.consoleDisplays[id][index];
        yield entry;
      }
    }

    return displayStoreGenerator(-1, aLength);
  },

  recordEntries: function CS_recordEntries(aHUDId, aConfigArray)
  {
    var len = aConfigArray.length;
    for (var i = 0; i < len; i++){
      this.recordEntry(aHUDId, aConfigArray[i]);
    }
  },


  recordEntry: function CS_recordEntry(aHUDId, aConfig)
  {
    var id = this.sequenceId();

    this.globalStorageIndex[id] = { hudId: aHUDId };

    var displayStorage = this.consoleDisplays[aHUDId];

    var displayIndex = this.displayIndexes[aHUDId];

    if (displayStorage && displayIndex) {
      var entry = new ConsoleEntry(aConfig, id);
      displayIndex.push(entry.id);
      displayStorage[entry.id] = entry;
      return entry;
    }
    else {
      throw new Error("Cannot get displayStorage or index object for id " + aHUDId);
    }
  },

  getEntry: function CS_getEntry(aId)
  {
    var display = this.globalStorageIndex[aId];
    var storName = display.hudId;
    return this.consoleDisplays[storName][aId];
  },

  updateEntry: function CS_updateEntry(aUUID)
  {
    // update an individual entry
    // TODO: see bug 568634
  },

  createDisplay: function CS_createdisplay(aId)
  {
    if (!this.consoleDisplays[aId]) {
      this.consoleDisplays[aId] = {};
      this.displayIndexes[aId] = [];
    }
  },

  removeDisplay: function CS_removeDisplay(aId)
  {
    try {
      delete this.consoleDisplays[aId];
      delete this.displayIndexes[aId];
    }
    catch (ex) {
      Cu.reportError("Could not remove console display for id " + aId);
    }
  }
};

/**
 * A Console log entry
 *
 * @param JSObject aConfig, object literal with ConsolEntry properties
 * @param integer aId
 * @returns void
 */

function ConsoleEntry(aConfig, id)
{
  if (!aConfig.logLevel && aConfig.message) {
    throw new Error("Missing Arguments when creating a console entry");
  }

  this.config = aConfig;
  this.id = id;
  for (var prop in aConfig) {
    if (!(typeof aConfig[prop] == "function")){
      this[prop] = aConfig[prop];
    }
  }

  if (aConfig.logLevel == "network") {
    this.transactions = { };
    if (aConfig.activity) {
      this.transactions[aConfig.activity.stage] = aConfig.activity;
    }
  }

}

ConsoleEntry.prototype = {

  updateTransaction: function CE_updateTransaction(aActivity) {
    this.transactions[aActivity.stage] = aActivity;
  }
};

//////////////////////////////////////////////////////////////////////////
// HUDWindowObserver
//////////////////////////////////////////////////////////////////////////

HUDWindowObserver = {
  QueryInterface: XPCOMUtils.generateQI(
    [Ci.nsIObserver,]
  ),

  init: function HWO_init()
  {
    Services.obs.addObserver(this, "xpcom-shutdown", false);
    Services.obs.addObserver(this, "content-document-global-created", false);
  },

  observe: function HWO_observe(aSubject, aTopic, aData)
  {
    if (aTopic == "content-document-global-created") {
      HUDService.windowInitializer(aSubject);
    }
    else if (aTopic == "xpcom-shutdown") {
      this.uninit();
    }
  },

  uninit: function HWO_uninit()
  {
    Services.obs.removeObserver(this, "content-document-global-created");
    HUDService.shutdown();
  },

  /**
   * once an initial console is created set this to true so we don't
   * over initialize
   */
  initialConsoleCreated: false,
};

///////////////////////////////////////////////////////////////////////////////
// HUDConsoleObserver
///////////////////////////////////////////////////////////////////////////////

/**
 * HUDConsoleObserver: Observes nsIConsoleService for global consoleMessages,
 * if a message originates inside a contentWindow we are tracking,
 * then route that message to the HUDService for logging.
 */

HUDConsoleObserver = {
  QueryInterface: XPCOMUtils.generateQI(
    [Ci.nsIObserver]
  ),

  init: function HCO_init()
  {
    Services.console.registerListener(this);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  observe: function HCO_observe(aSubject, aTopic, aData)
  {
    if (aTopic == "xpcom-shutdown") {
      Services.console.unregisterListener(this);
    }

    if (aSubject instanceof Ci.nsIScriptError) {
      switch (aSubject.category) {
        case "XPConnect JavaScript":
        case "component javascript":
        case "chrome javascript":
          // we ignore these CHROME-originating errors as we only
          // care about content
          return;
        case "HUDConsole":
        case "CSS Parser":
        case "content javascript":
          HUDService.reportConsoleServiceContentScriptError(aSubject);
          return;
        default:
          HUDService.reportConsoleServiceMessage(aSubject);
          return;
      }
    }
  }
};

///////////////////////////////////////////////////////////////////////////
// appName
///////////////////////////////////////////////////////////////////////////

/**
 * Get the app's name so we can properly dispatch app-specific
 * methods per API call
 * @returns Gecko application name
 */
function appName()
{
  let APP_ID = Services.appinfo.QueryInterface(Ci.nsIXULRuntime).ID;

  let APP_ID_TABLE = {
    "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}": "FIREFOX" ,
    "{3550f703-e582-4d05-9a08-453d09bdfdc6}": "THUNDERBIRD",
    "{a23983c0-fd0e-11dc-95ff-0800200c9a66}": "FENNEC" ,
    "{92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a}": "SEAMONKEY",
  };

  let name = APP_ID_TABLE[APP_ID];

  if (name){
    return name;
  }
  throw new Error("appName: UNSUPPORTED APPLICATION UUID");
}

///////////////////////////////////////////////////////////////////////////
// HUDService (exported symbol)
///////////////////////////////////////////////////////////////////////////

try {
  // start the HUDService
  // This is in a try block because we want to kill everything if
  // *any* of this fails
  var HUDService = new HUD_SERVICE();
  HUDWindowObserver.init();
  HUDConsoleObserver.init();
}
catch (ex) {
  Cu.reportError("HUDService failed initialization.\n" + ex);
  // TODO: kill anything that may have started up
  // see bug 568665
}
