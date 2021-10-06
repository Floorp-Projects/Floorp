/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* exported ExtensionPageChild */

var EXPORTED_SYMBOLS = ["ExtensionPageChild"];

/**
 * This file handles privileged extension page logic that runs in the
 * child process.
 */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionChildDevToolsUtils",
  "resource://gre/modules/ExtensionChildDevToolsUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ExtensionProcessScript",
  "resource://gre/modules/ExtensionProcessScript.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Schemas",
  "resource://gre/modules/Schemas.jsm"
);

const CATEGORY_EXTENSION_SCRIPTS_ADDON = "webextension-scripts-addon";
const CATEGORY_EXTENSION_SCRIPTS_DEVTOOLS = "webextension-scripts-devtools";

const { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);
const { ExtensionChild, ExtensionActivityLogChild } = ChromeUtils.import(
  "resource://gre/modules/ExtensionChild.jsm"
);
const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

const { getInnerWindowID, getUniqueId, promiseEvent } = ExtensionUtils;

const {
  BaseContext,
  CanOfAPIs,
  SchemaAPIManager,
  defineLazyGetter,
} = ExtensionCommon;

const {
  ChildAPIManager,
  Messenger,
  WebIDLChildAPIManager,
  WorkerMessenger,
} = ExtensionChild;

var ExtensionPageChild;

const initializeBackgroundPage = context => {
  // Override the `alert()` method inside background windows;
  // we alias it to console.log().
  // See: https://bugzilla.mozilla.org/show_bug.cgi?id=1203394
  let alertDisplayedWarning = false;
  const innerWindowID = getInnerWindowID(context.contentWindow);

  function logWarningMessage({ text, filename, lineNumber, columnNumber }) {
    let consoleMsg = Cc["@mozilla.org/scripterror;1"].createInstance(
      Ci.nsIScriptError
    );
    consoleMsg.initWithWindowID(
      text,
      filename,
      null,
      lineNumber,
      columnNumber,
      Ci.nsIScriptError.warningFlag,
      "webextension",
      innerWindowID
    );
    Services.console.logMessage(consoleMsg);
  }

  let alertOverwrite = text => {
    const { filename, columnNumber, lineNumber } = Components.stack.caller;

    if (!alertDisplayedWarning) {
      context.childManager.callParentAsyncFunction(
        "runtime.openBrowserConsole",
        []
      );

      logWarningMessage({
        text:
          "alert() is not supported in background windows; please use console.log instead.",
        filename,
        lineNumber,
        columnNumber,
      });

      alertDisplayedWarning = true;
    }

    logWarningMessage({ text, filename, lineNumber, columnNumber });
  };
  Cu.exportFunction(alertOverwrite, context.contentWindow, {
    defineAs: "alert",
  });
};

function getFrameData(global) {
  return ExtensionProcessScript.getFrameData(global, true);
}

var apiManager = new (class extends SchemaAPIManager {
  constructor() {
    super("addon", Schemas);
    this.initialized = false;
  }

  lazyInit() {
    if (!this.initialized) {
      this.initialized = true;
      this.initGlobal();
      for (let { value } of Services.catMan.enumerateCategory(
        CATEGORY_EXTENSION_SCRIPTS_ADDON
      )) {
        this.loadScript(value);
      }
    }
  }
})();

var devtoolsAPIManager = new (class extends SchemaAPIManager {
  constructor() {
    super("devtools", Schemas);
    this.initialized = false;
  }

  lazyInit() {
    if (!this.initialized) {
      this.initialized = true;
      this.initGlobal();
      for (let { value } of Services.catMan.enumerateCategory(
        CATEGORY_EXTENSION_SCRIPTS_DEVTOOLS
      )) {
        this.loadScript(value);
      }
    }
  }
})();

function getContextChildManagerGetter({ envType }) {
  return function() {
    let apiManager =
      envType === "devtools_parent"
        ? devtoolsAPIManager
        : this.extension.apiManager;

    apiManager.lazyInit();

    let localApis = {};
    let can = new CanOfAPIs(this, apiManager, localApis);

    const ChildAPIManagerClass = this.useWebIDLBindings
      ? WebIDLChildAPIManager
      : ChildAPIManager;

    let childManager = new ChildAPIManagerClass(
      this,
      this.messageManager,
      can,
      {
        envType,
        viewType: this.viewType,
        url: this.uri.spec,
        incognito: this.incognito,
      }
    );

    this.callOnClose(childManager);

    return childManager;
  };
}

class WorkerContextChild extends BaseContext {
  /**
   * This WorkerContextChild represents an addon execution environment
   * that is running on the worker thread in an extension child process.
   *
   * @param {BrowserExtensionContent} extension This context's owner.
   * @param {object}                         params
   * @param {mozIExtensionServiceWorkerInfo} params.serviceWorkerInfo
   */
  constructor(extension, { serviceWorkerInfo }) {
    if (
      !serviceWorkerInfo?.scriptURL ||
      !serviceWorkerInfo?.clientInfoId ||
      !serviceWorkerInfo?.principal
    ) {
      throw new Error("Missing or invalid serviceWorkerInfo");
    }

    super("addon_child", extension);
    this.viewType = "background_worker";
    this.uri = Services.io.newURI(serviceWorkerInfo.scriptURL);
    this.workerClientInfoId = serviceWorkerInfo.clientInfoId;
    this.workerPrincipal = serviceWorkerInfo.principal;
    this.incognito = serviceWorkerInfo.principal.privateBrowsingId > 0;

    // A mozIExtensionAPIRequest being processed (set by the withAPIRequest
    // method while executing a given callable, can be optionally used by
    // the API implementation methods to access the mozIExtensionAPIRequest
    // being processed and customize their result if necessary to handle
    // requests originated by the webidl bindings).
    this.webidlAPIRequest = null;

    // This context uses a plain object as a cloneScope (anyway the values
    // moved across thread are going to be automatically serialized/deserialized
    // as structure clone data, we may remove this if we are changing the
    // internals to not use the context.cloneScope).
    this.workerCloneScope = {
      Promise,
      // The instances of this Error constructor will be recognized by the
      // ExtensionAPIRequestHandler as errors that should be propagated to
      // the worker thread and received by extension code that originated
      // the API request.
      Error: ExtensionUtils.WorkerExtensionError,
    };
  }

  openConduit(subject, address) {
    let proc = ChromeUtils.domProcessChild;
    let conduit = proc.getActor("ProcessConduits").openConduit(subject, {
      id: subject.id || getUniqueId(),
      extensionId: this.extension.id,
      envType: this.envType,
      workerScriptURL: this.uri.spec,
      ...address,
    });
    this.callOnClose(conduit);
    conduit.setCloseCallback(() => {
      this.forgetOnClose(conduit);
    });
    return conduit;
  }

  withAPIRequest(request, callable) {
    this.webidlAPIRequest = request;
    try {
      return callable();
    } finally {
      this.webidlAPIRequest = null;
    }
  }

  getAPIRequest() {
    return this.webidlAPIRequest;
  }

  /**
   * Captures the most recent stack frame from the WebIDL API request being
   * processed.
   *
   * @returns {SavedFrame?}
   */
  getCaller() {
    return this.webidlAPIRequest?.callerSavedFrame;
  }

  logActivity(type, name, data) {
    ExtensionActivityLogChild.log(this, type, name, data);
  }

  get cloneScope() {
    return this.workerCloneScope;
  }

  get principal() {
    return this.workerPrincipal;
  }

  get tabId() {
    return -1;
  }

  get useWebIDLBindings() {
    return true;
  }

  shutdown() {
    this.unload();
  }

  unload() {
    if (this.unloaded) {
      return;
    }

    super.unload();
  }
}

defineLazyGetter(WorkerContextChild.prototype, "messenger", function() {
  return new WorkerMessenger(this);
});

defineLazyGetter(
  WorkerContextChild.prototype,
  "childManager",
  getContextChildManagerGetter({ envType: "addon_parent" })
);

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
    let { viewType = "tab", uri, contentWindow, tabId } = params;
    this.viewType = viewType;
    this.uri = uri || extension.baseURI;

    this.setContentWindow(contentWindow);
    this.browsingContextId = contentWindow.docShell.browsingContext.id;

    if (viewType == "tab") {
      Object.defineProperty(this, "tabId", {
        value: tabId,
        enumerable: true,
        configurable: true,
      });
    }

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

  logActivity(type, name, data) {
    ExtensionActivityLogChild.log(this, type, name, data);
  }

  get cloneScope() {
    return this.contentWindow;
  }

  get principal() {
    return this.contentWindow.document.nodePrincipal;
  }

  get tabId() {
    // Will be overwritten in the constructor if necessary.
    return -1;
  }

  // Called when the extension shuts down.
  shutdown() {
    if (this.contentWindow) {
      this.contentWindow.close();
    }

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

    super.unload();
  }
}

defineLazyGetter(ExtensionBaseContextChild.prototype, "messenger", function() {
  return new Messenger(this);
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
    super(extension, Object.assign(params, { envType: "addon_child" }));

    if (this.viewType == "background") {
      initializeBackgroundPage(this);
    }

    this.extension.views.add(this);
  }

  unload() {
    super.unload();
    this.extension.views.delete(this);
  }
}

defineLazyGetter(
  ExtensionPageContextChild.prototype,
  "childManager",
  getContextChildManagerGetter({ envType: "addon_parent" })
);

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
    super(extension, Object.assign(params, { envType: "devtools_child" }));

    this.devtoolsToolboxInfo = params.devtoolsToolboxInfo;
    ExtensionChildDevToolsUtils.initThemeChangeObserver(
      params.devtoolsToolboxInfo.themeName,
      this
    );

    this.extension.devtoolsViews.add(this);
  }

  unload() {
    super.unload();
    this.extension.devtoolsViews.delete(this);
  }
}

defineLazyGetter(
  DevToolsContextChild.prototype,
  "childManager",
  getContextChildManagerGetter({ envType: "devtools_parent" })
);

ExtensionPageChild = {
  initialized: false,

  // Map<innerWindowId, ExtensionPageContextChild>
  extensionContexts: new Map(),

  // Map<workerClientInfoId, ExtensionWorkerContextChild>
  extensionWorkerContexts: new Map(),

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

    promiseEvent(
      global,
      "DOMContentLoaded",
      true,
      event => event.target.location != "about:blank"
    ).then(() => {
      let windowId = getInnerWindowID(global.content);
      let context = this.extensionContexts.get(windowId);

      global.sendAsyncMessage("Extension:ExtensionViewLoaded", {
        childId: context && context.childManager.id,
      });
    });
  },

  getContextForWorker(extension, serviceWorkerInfo) {
    this._init();

    if (!serviceWorkerInfo) {
      return null;
    }

    let context = this.extensionWorkerContexts.get(
      serviceWorkerInfo.clientInfoId
    );
    if (context && context.extension === extension) {
      return context;
    }

    // Lazily create the context.
    if (!context) {
      context = new WorkerContextChild(extension, { serviceWorkerInfo });

      this.extensionWorkerContexts.set(serviceWorkerInfo.clientInfoId, context);
    }

    return context;
  },

  /**
   * Create a privileged context at initial-document-element-inserted.
   *
   * @param {BrowserExtensionContent} extension
   *     The extension for which the context should be created.
   * @param {nsIDOMWindow} contentWindow The global of the page.
   */
  initExtensionContext(extension, contentWindow) {
    this._init();

    if (!WebExtensionPolicy.isExtensionProcess) {
      throw new Error(
        "Cannot create an extension page context in current process"
      );
    }

    let windowId = getInnerWindowID(contentWindow);
    let context = this.extensionContexts.get(windowId);
    if (context) {
      if (context.extension !== extension) {
        throw new Error(
          "A different extension context already exists for this frame"
        );
      }
      throw new Error(
        "An extension context was already initialized for this frame"
      );
    }

    let mm = contentWindow.docShell.messageManager;

    let { viewType, tabId, devtoolsToolboxInfo } = getFrameData(mm) || {};

    let uri = contentWindow.document.documentURIObject;

    if (devtoolsToolboxInfo) {
      context = new DevToolsContextChild(extension, {
        viewType,
        contentWindow,
        uri,
        tabId,
        devtoolsToolboxInfo,
      });
    } else {
      context = new ExtensionPageContextChild(extension, {
        viewType,
        contentWindow,
        uri,
        tabId,
      });
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

    for (let [workerClientInfoId, context] of this.extensionWorkerContexts) {
      if (context.extension.id == extensionId) {
        context.shutdown();
        this.extensionWorkerContexts.delete(workerClientInfoId);
      }
    }
  },
};
