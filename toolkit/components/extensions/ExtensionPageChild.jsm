/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* exported ExtensionPageChild */

this.EXPORTED_SYMBOLS = ["ExtensionPageChild"];

/**
 * This file handles privileged extension page logic that runs in the
 * child process.
 */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionChildDevToolsUtils",
                                  "resource://gre/modules/ExtensionChildDevToolsUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebNavigationFrames",
                                  "resource://gre/modules/WebNavigationFrames.jsm");

XPCOMUtils.defineLazyGetter(
  this, "processScript",
  () => Cc["@mozilla.org/webextensions/extension-process-script;1"]
          .getService().wrappedJSObject);

const CATEGORY_EXTENSION_SCRIPTS_ADDON = "webextension-scripts-addon";
const CATEGORY_EXTENSION_SCRIPTS_DEVTOOLS = "webextension-scripts-devtools";

Cu.import("resource://gre/modules/ExtensionCommon.jsm");
Cu.import("resource://gre/modules/ExtensionChild.jsm");
Cu.import("resource://gre/modules/ExtensionUtils.jsm");

const {
  defineLazyGetter,
  getInnerWindowID,
  promiseEvent,
} = ExtensionUtils;

const {
  BaseContext,
  CanOfAPIs,
  SchemaAPIManager,
} = ExtensionCommon;

const {
  ChildAPIManager,
  Messenger,
} = ExtensionChild;

var ExtensionPageChild;

const initializeBackgroundPage = (context) => {
  // Override the `alert()` method inside background windows;
  // we alias it to console.log().
  // See: https://bugzilla.mozilla.org/show_bug.cgi?id=1203394
  let alertDisplayedWarning = false;
  const innerWindowID = getInnerWindowID(context.contentWindow);

  function logWarningMessage({text, filename, lineNumber, columnNumber}) {
    let consoleMsg = Cc["@mozilla.org/scripterror;1"].createInstance(Ci.nsIScriptError);
    consoleMsg.initWithWindowID(text, filename, null, lineNumber, columnNumber,
                                Ci.nsIScriptError.warningFlag, "webextension",
                                innerWindowID);
    Services.console.logMessage(consoleMsg);
  }

  let alertOverwrite = text => {
    const {filename, columnNumber, lineNumber} = Components.stack.caller;

    if (!alertDisplayedWarning) {
      context.childManager.callParentAsyncFunction("runtime.openBrowserConsole", []);

      logWarningMessage({
        text: "alert() is not supported in background windows; please use console.log instead.",
        filename, lineNumber, columnNumber,
      });

      alertDisplayedWarning = true;
    }

    logWarningMessage({text, filename, lineNumber, columnNumber});
  };
  Cu.exportFunction(alertOverwrite, context.contentWindow, {defineAs: "alert"});
};

function getFrameData(global) {
  return processScript.getFrameData(global, true);
}

var apiManager = new class extends SchemaAPIManager {
  constructor() {
    super("addon", Schemas);
    this.initialized = false;
  }

  lazyInit() {
    if (!this.initialized) {
      this.initialized = true;
      this.initGlobal();
      for (let [/* name */, value] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCRIPTS_ADDON)) {
        this.loadScript(value);
      }
    }
  }
}();

var devtoolsAPIManager = new class extends SchemaAPIManager {
  constructor() {
    super("devtools", Schemas);
    this.initialized = false;
  }

  lazyInit() {
    if (!this.initialized) {
      this.initialized = true;
      this.initGlobal();
      for (let [/* name */, value] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCRIPTS_DEVTOOLS)) {
        this.loadScript(value);
      }
    }
  }
}();

class ExtensionBaseContextChild extends BaseContext {
  /**
   * This ExtensionBaseContextChild represents an addon execution environment
   * that is running in an addon or devtools child process.
   *
   * @param {BrowserExtensionContent} extension This context's owner.
   * @param {object} params
   * @param {string} params.envType One of "addon_child" or "devtools_child".
   * @param {nsIDOMWindow} params.contentWindow The window where the addon runs.
   * @param {string} params.viewType One of "background", "popup", "tab",
   *   "sidebar", "devtools_page" or "devtools_panel".
   * @param {number} [params.tabId] This tab's ID, used if viewType is "tab".
   */
  constructor(extension, params) {
    if (!params.envType) {
      throw new Error("Missing envType");
    }

    super(params.envType, extension);
    let {viewType = "tab", uri, contentWindow, tabId} = params;
    this.viewType = viewType;
    this.uri = uri || extension.baseURI;

    this.setContentWindow(contentWindow);

    // This is the MessageSender property passed to extension.
    let sender = {id: extension.id};
    if (viewType == "tab") {
      sender.frameId = WebNavigationFrames.getFrameId(contentWindow);
      sender.tabId = tabId;
      Object.defineProperty(this, "tabId",
                            {value: tabId,
                             enumerable: true,
                             configurable: true});
    }
    if (uri) {
      sender.url = uri.spec;
    }
    this.sender = sender;

    Schemas.exportLazyGetter(contentWindow, "browser", () => {
      let browserObj = Cu.createObjectIn(contentWindow);
      this.childManager.inject(browserObj);
      return browserObj;
    });

    Schemas.exportLazyGetter(contentWindow, "chrome", () => {
      let chromeApiWrapper = Object.create(this.childManager);
      chromeApiWrapper.isChromeCompat = true;

      let chromeObj = Cu.createObjectIn(contentWindow);
      chromeApiWrapper.inject(chromeObj);
      return chromeObj;
    });
  }

  get cloneScope() {
    return this.contentWindow;
  }

  get principal() {
    return this.contentWindow.document.nodePrincipal;
  }

  get windowId() {
    if (["tab", "popup", "sidebar"].includes(this.viewType)) {
      let frameData = getFrameData(this.messageManager);
      return frameData ? frameData.windowId : -1;
    }
    return -1;
  }

  get tabId() {
    // Will be overwritten in the constructor if necessary.
    return -1;
  }

  // Called when the extension shuts down.
  shutdown() {
    this.unload();
  }

  // This method is called when an extension page navigates away or
  // its tab is closed.
  unload() {
    // Note that without this guard, we end up running unload code
    // multiple times for tab pages closed by the "page-unload" handlers
    // triggered below.
    if (this.unloaded) {
      return;
    }

    if (this.contentWindow) {
      this.contentWindow.close();
    }

    super.unload();
  }
}

defineLazyGetter(ExtensionBaseContextChild.prototype, "messenger", function() {
  let filter = {extensionId: this.extension.id};
  let optionalFilter = {};
  // Addon-generated messages (not necessarily from the same process as the
  // addon itself) are sent to the main process, which forwards them via the
  // parent process message manager. Specific replies can be sent to the frame
  // message manager.
  return new Messenger(this, [Services.cpmm, this.messageManager], this.sender,
                       filter, optionalFilter);
});

class ExtensionPageContextChild extends ExtensionBaseContextChild {
  /**
   * This ExtensionPageContextChild represents a privileged addon
   * execution environment that has full access to the WebExtensions
   * APIs (provided that the correct permissions have been requested).
   *
   * This is the child side of the ExtensionPageContextParent class
   * defined in ExtensionParent.jsm.
   *
   * @param {BrowserExtensionContent} extension This context's owner.
   * @param {object} params
   * @param {nsIDOMWindow} params.contentWindow The window where the addon runs.
   * @param {string} params.viewType One of "background", "popup", "sidebar" or "tab".
   *     "background", "sidebar" and "tab" are used by `browser.extension.getViews`.
   *     "popup" is only used internally to identify page action and browser
   *     action popups and options_ui pages.
   * @param {number} [params.tabId] This tab's ID, used if viewType is "tab".
   */
  constructor(extension, params) {
    super(extension, Object.assign(params, {envType: "addon_child"}));

    this.extension.views.add(this);
  }

  unload() {
    super.unload();
    this.extension.views.delete(this);
  }
}

defineLazyGetter(ExtensionPageContextChild.prototype, "childManager", function() {
  this.extension.apiManager.lazyInit();

  let localApis = {};
  let can = new CanOfAPIs(this, this.extension.apiManager, localApis);

  let childManager = new ChildAPIManager(this, this.messageManager, can, {
    envType: "addon_parent",
    viewType: this.viewType,
    url: this.uri.spec,
    incognito: this.incognito,
  });

  this.callOnClose(childManager);

  if (this.viewType == "background") {
    initializeBackgroundPage(this);
  }

  return childManager;
});

class DevToolsContextChild extends ExtensionBaseContextChild {
  /**
   * This DevToolsContextChild represents a devtools-related addon execution
   * environment that has access to the devtools API namespace and to the same subset
   * of APIs available in a content script execution environment.
   *
   * @param {BrowserExtensionContent} extension This context's owner.
   * @param {object} params
   * @param {nsIDOMWindow} params.contentWindow The window where the addon runs.
   * @param {string} params.viewType One of "devtools_page" or "devtools_panel".
   * @param {object} [params.devtoolsToolboxInfo] This devtools toolbox's information,
   *   used if viewType is "devtools_page" or "devtools_panel".
   */
  constructor(extension, params) {
    super(extension, Object.assign(params, {envType: "devtools_child"}));

    this.devtoolsToolboxInfo = params.devtoolsToolboxInfo;
    ExtensionChildDevToolsUtils.initThemeChangeObserver(
      params.devtoolsToolboxInfo.themeName, this);

    this.extension.devtoolsViews.add(this);
  }

  unload() {
    super.unload();
    this.extension.devtoolsViews.delete(this);
  }
}

defineLazyGetter(DevToolsContextChild.prototype, "childManager", function() {
  devtoolsAPIManager.lazyInit();

  let localApis = {};
  let can = new CanOfAPIs(this, devtoolsAPIManager, localApis);

  let childManager = new ChildAPIManager(this, this.messageManager, can, {
    envType: "devtools_parent",
    viewType: this.viewType,
    url: this.uri.spec,
    incognito: this.incognito,
  });

  this.callOnClose(childManager);

  return childManager;
});

ExtensionPageChild = {
  // Map<innerWindowId, ExtensionPageContextChild>
  extensionContexts: new Map(),

  initialized: false,

  apiManager,

  _init() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    Services.obs.addObserver(this, "inner-window-destroyed"); // eslint-ignore-line mozilla/balanced-listeners
  },

  observe(subject, topic, data) {
    if (topic === "inner-window-destroyed") {
      let windowId = subject.QueryInterface(Ci.nsISupportsPRUint64).data;

      this.destroyExtensionContext(windowId);
    }
  },

  expectViewLoad(global, viewType) {
    if (["popup", "sidebar"].includes(viewType)) {
      global.docShell.isAppTab = true;
    }

    promiseEvent(global, "DOMContentLoaded", true).then(() => {
      let windowId = getInnerWindowID(global.content);
      let context = this.extensionContexts.get(windowId);

      global.sendAsyncMessage("Extension:ExtensionViewLoaded",
                              {childId: context && context.childManager.id});
    });
  },

  /**
   * Create a privileged context at document-element-inserted.
   *
   * @param {BrowserExtensionContent} extension
   *     The extension for which the context should be created.
   * @param {nsIDOMWindow} contentWindow The global of the page.
   */
  initExtensionContext(extension, contentWindow) {
    this._init();

    if (!WebExtensionPolicy.isExtensionProcess) {
      throw new Error("Cannot create an extension page context in current process");
    }

    let windowId = getInnerWindowID(contentWindow);
    let context = this.extensionContexts.get(windowId);
    if (context) {
      if (context.extension !== extension) {
        throw new Error("A different extension context already exists for this frame");
      }
      throw new Error("An extension context was already initialized for this frame");
    }

    let mm = contentWindow.document.docShell
                          .QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIContentFrameMessageManager);

    let {viewType, tabId, devtoolsToolboxInfo} = getFrameData(mm) || {};

    let uri = contentWindow.document.documentURIObject;

    if (devtoolsToolboxInfo) {
      context = new DevToolsContextChild(extension, {
        viewType, contentWindow, uri, tabId, devtoolsToolboxInfo,
      });
    } else {
      context = new ExtensionPageContextChild(extension, {viewType, contentWindow, uri, tabId});
    }

    this.extensionContexts.set(windowId, context);
  },

  /**
   * Close the ExtensionPageContextChild belonging to the given window, if any.
   *
   * @param {number} windowId The inner window ID of the destroyed context.
   */
  destroyExtensionContext(windowId) {
    let context = this.extensionContexts.get(windowId);
    if (context) {
      context.unload();
      this.extensionContexts.delete(windowId);
    }
  },

  shutdownExtension(extensionId) {
    for (let [windowId, context] of this.extensionContexts) {
      if (context.extension.id == extensionId) {
        context.shutdown();
        this.extensionContexts.delete(windowId);
      }
    }
  },
};
