/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Extension", "ExtensionData"];

/* globals Extension ExtensionData */

/*
 * This file is the main entry point for extensions. When an extension
 * loads, its bootstrap.js file creates a Extension instance
 * and calls .startup() on it. It calls .shutdown() when the extension
 * unloads. Extension manages any extension-specific state in
 * the chrome process.
 */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.importGlobalProperties(["TextEncoder"]);

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionAPIs",
                                  "resource://gre/modules/ExtensionAPI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionStorage",
                                  "resource://gre/modules/ExtensionStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Locale",
                                  "resource://gre/modules/Locale.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Log",
                                  "resource://gre/modules/Log.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MatchGlobs",
                                  "resource://gre/modules/MatchPattern.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MatchPattern",
                                  "resource://gre/modules/MatchPattern.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MessageChannel",
                                  "resource://gre/modules/MessageChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyGetter(this, "require", () => {
  let obj = {};
  Cu.import("resource://devtools/shared/Loader.jsm", obj);
  return obj.require;
});

Cu.import("resource://gre/modules/ExtensionContent.jsm");
Cu.import("resource://gre/modules/ExtensionManagement.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "uuidGen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

const BASE_SCHEMA = "chrome://extensions/content/schemas/manifest.json";
const CATEGORY_EXTENSION_SCHEMAS = "webextension-schemas";
const CATEGORY_EXTENSION_SCRIPTS = "webextension-scripts";

let schemaURLs = new Set();

if (!AppConstants.RELEASE_BUILD) {
  schemaURLs.add("chrome://extensions/content/schemas/experiments.json");
}

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  BaseContext,
  EventEmitter,
  LocaleData,
  Messenger,
  injectAPI,
  instanceOf,
  flushJarCache,
} = ExtensionUtils;

XPCOMUtils.defineLazyGetter(this, "console", ExtensionUtils.getConsole);

const LOGGER_ID_BASE = "addons.webextension.";
const UUID_MAP_PREF = "extensions.webextensions.uuids";
const LEAVE_STORAGE_PREF = "extensions.webextensions.keepStorageOnUninstall";
const LEAVE_UUID_PREF = "extensions.webextensions.keepUuidOnUninstall";

const COMMENT_REGEXP = new RegExp(String.raw`
    ^
    (
      (?:
        [^"] |
        " (?:[^"\\] | \\.)* "
      )*?
    )

    //.*
  `.replace(/\s+/g, ""), "gm");

var scriptScope = this;

var ExtensionContext, GlobalManager;

// This object loads the ext-*.js scripts that define the extension API.
var Management = {
  initialized: null,
  scopes: [],
  apis: [],
  schemaApis: [],
  emitter: new EventEmitter(),

  // Loads all the ext-*.js scripts currently registered.
  lazyInit() {
    if (this.initialized) {
      return this.initialized;
    }

    // Load order matters here. The base manifest defines types which are
    // extended by other schemas, so needs to be loaded first.
    let promise = Schemas.load(BASE_SCHEMA).then(() => {
      let promises = [];
      for (let [/* name */, url] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCHEMAS)) {
        promises.push(Schemas.load(url));
      }
      for (let url of schemaURLs) {
        promises.push(Schemas.load(url));
      }
      return Promise.all(promises);
    });

    for (let [/* name */, value] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCRIPTS)) {
      let scope = {
        get console() { return console; },
        ExtensionContext,
        extensions: this,
        global: scriptScope,
        GlobalManager,
        require,
      };
      Services.scriptloader.loadSubScript(value, scope, "UTF-8");

      // Save the scope to avoid it being garbage collected.
      this.scopes.push(scope);
    }

    this.initialized = promise;
    return this.initialized;
  },

  // Called by an ext-*.js script to register an API. The |api|
  // parameter should be an object of the form:
  // {
  //   tabs: {
  //     create: ...,
  //     onCreated: ...
  //   }
  // }
  // This registers tabs.create and tabs.onCreated as part of the API.
  registerAPI(api) {
    this.apis.push({api});
  },

  // Same as above, but only register the API is the add-on has the
  // given permission.
  registerPrivilegedAPI(permission, api) {
    this.apis.push({api, permission});
  },

  registerSchemaAPI(namespace, api) {
    this.schemaApis.push({namespace, api});
  },

  // Mash together into a single object all the APIs registered by the
  // functions above. Return the merged object.
  generateAPIs(extension, context, apis, namespaces = null) {
    let obj = {};

    // Recursively copy properties from source to dest.
    function copy(dest, source) {
      for (let prop in source) {
        let desc = Object.getOwnPropertyDescriptor(source, prop);
        if (typeof(desc.value) == "object") {
          if (!(prop in dest)) {
            dest[prop] = {};
          }
          copy(dest[prop], source[prop]);
        } else {
          Object.defineProperty(dest, prop, desc);
        }
      }
    }

    for (let api of apis) {
      if (namespaces && !namespaces.includes(api.namespace)) {
        continue;
      }
      if (api.permission) {
        if (!extension.hasPermission(api.permission)) {
          continue;
        }
      }

      api = api.api(extension, context);
      copy(obj, api);
    }

    for (let api of extension.apis) {
      copy(obj, api.getAPI(context));
    }

    return obj;
  },

  // The ext-*.js scripts can ask to be notified for certain hooks.
  on(hook, callback) {
    this.emitter.on(hook, callback);
  },

  // Ask to run all the callbacks that are registered for a given hook.
  emit(hook, ...args) {
    return this.emitter.emit(hook, ...args);
  },

  off(hook, callback) {
    this.emitter.off(hook, callback);
  },
};

// An extension page is an execution context for any extension content
// that runs in the chrome process. It's used for background pages
// (type="background"), popups (type="popup"), and any extension
// content loaded into browser tabs (type="tab").
//
// |params| is an object with the following properties:
// |type| is one of "background", "popup", or "tab".
// |contentWindow| is the DOM window the content runs in.
// |uri| is the URI of the content (optional).
// |docShell| is the docshell the content runs in (optional).
// |incognito| is the content running in a private context (default: false).
ExtensionContext = class extends BaseContext {
  constructor(extension, params) {
    super(extension.id);

    let {type, uri} = params;
    this.extension = extension;
    this.type = type;
    this.uri = uri || extension.baseURI;
    this.incognito = params.incognito || false;

    if (params.contentWindow) {
      this.setContentWindow(params.contentWindow);
    }

    // This is the MessageSender property passed to extension.
    // It can be augmented by the "page-open" hook.
    let sender = {id: extension.uuid};
    if (uri) {
      sender.url = uri.spec;
    }
    let delegate = {
      getSender() {},
    };
    Management.emit("page-load", this, params, sender, delegate);

    // Properties in |filter| must match those in the |recipient|
    // parameter of sendMessage.
    let filter = {extensionId: extension.id};
    this.messenger = new Messenger(this, [Services.mm, Services.ppmm], sender, filter, delegate);

    if (this.externallyVisible) {
      this.extension.views.add(this);
    }
  }

  get cloneScope() {
    return this.contentWindow;
  }

  get principal() {
    return this.contentWindow.document.nodePrincipal;
  }

  get externallyVisible() {
    return true;
  }

  // Called when the extension shuts down.
  shutdown() {
    Management.emit("page-shutdown", this);
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

    Management.emit("page-unload", this);

    if (this.externallyVisible) {
      this.extension.views.delete(this);
    }
  }
};

class ProxyContext extends ExtensionContext {
  constructor(extension, params, messageManager, principal) {
    params.contentWindow = null;
    params.uri = NetUtil.newURI(params.url);

    super(extension, params);
    this.messageManager = messageManager;
    this.principal_ = principal;

    this.apiObj = {};
    GlobalManager.injectInObject(extension, this, null, this.apiObj, ["storage", "test"]);

    this.listenerProxies = new Map();

    this.sandbox = Cu.Sandbox(principal, {});
  }

  get principal() {
    return this.principal_;
  }

  get cloneScope() {
    return this.sandbox;
  }

  get externallyVisible() {
    return false;
  }
}

function findPathInObject(obj, path) {
  // Split any nested namespace (e.g devtools.inspectedWindow) element
  // and concatenate them into a flatten array.
  path = path.reduce((acc, el) => {
    return acc.concat(el.split("."));
  }, []);

  for (let elt of path) {
    // If we get a null object before reaching the requested path
    // (e.g. the API object is returned only on particular kind of contexts instead
    // of based on WebExtensions permissions, like it happens for the devtools APIs),
    // stop searching and return undefined.
    if (!obj || !(elt in obj)) {
      return undefined;
    }

    obj = obj[elt];
  }

  return obj;
}

let ParentAPIManager = {
  proxyContexts: new Map(),

  init() {
    Services.obs.addObserver(this, "message-manager-close", false);

    Services.mm.addMessageListener("API:CreateProxyContext", this);
    Services.mm.addMessageListener("API:CloseProxyContext", this, true);
    Services.mm.addMessageListener("API:Call", this);
    Services.mm.addMessageListener("API:AddListener", this);
    Services.mm.addMessageListener("API:RemoveListener", this);
  },

  // "message-manager-close" observer.
  observe(subject, topic, data) {
    let mm = subject;
    for (let [childId, context] of this.proxyContexts) {
      if (context.messageManager == mm) {
        this.closeProxyContext(childId);
      }
    }
  },

  receiveMessage({name, data, target}) {
    switch (name) {
      case "API:CreateProxyContext":
        this.createProxyContext(data, target);
        break;

      case "API:CloseProxyContext":
        this.closeProxyContext(data.childId);
        break;

      case "API:Call":
        this.call(data, target);
        break;

      case "API:AddListener":
        this.addListener(data, target);
        break;

      case "API:RemoveListener":
        this.removeListener(data);
        break;
    }
  },

  createProxyContext(data, target) {
    let {extensionId, childId, principal} = data;
    let extension = GlobalManager.getExtension(extensionId);

    let context = new ProxyContext(extension, data, target.messageManager, principal);
    this.proxyContexts.set(childId, context);
  },

  closeProxyContext(childId) {
    if (!this.proxyContexts.has(childId)) {
      return;
    }
    let context = this.proxyContexts.get(childId);
    context.unload();
    this.proxyContexts.delete(childId);
  },

  call(data, target) {
    let context = this.proxyContexts.get(data.childId);
    function callback(...cbArgs) {
      let lastError = context.lastError;

      target.messageManager.sendAsyncMessage("API:CallResult", {
        childId: data.childId,
        callId: data.callId,
        args: cbArgs,
        lastError: lastError ? lastError.message : null,
      });
    }

    let args = data.args;
    args = Cu.cloneInto(args, context.sandbox);
    if (data.callId) {
      args = args.concat(callback);
    }
    try {
      findPathInObject(context.apiObj, data.path)[data.name](...args);
    } catch (e) {
      let msg = e.message || "API failed";
      target.messageManager.sendAsyncMessage("API:CallResult", {
        childId: data.childId,
        callId: data.callId,
        lastError: msg,
      });
    }
  },

  addListener(data, target) {
    let context = this.proxyContexts.get(data.childId);

    function listener(...listenerArgs) {
      target.messageManager.sendAsyncMessage("API:RunListener", {
        childId: data.childId,
        path: data.path,
        name: data.name,
        args: listenerArgs,
      });
    }

    let ref = data.path.concat(data.name).join(".");
    context.listenerProxies.set(ref, listener);

    let args = Cu.cloneInto(data.args, context.sandbox);
    findPathInObject(context.apiObj, data.path)[data.name].addListener(listener, ...args);
  },

  removeListener(data) {
    let context = this.proxyContexts.get(data.childId);
    let ref = data.path.concat(data.name).join(".");
    let listener = context.listenerProxies.get(ref);
    findPathInObject(context.apiObj, data.path)[data.name].removeListener(listener);
  },
};

ParentAPIManager.init();

// All moz-extension URIs use a machine-specific UUID rather than the
// extension's own ID in the host component. This makes it more
// difficult for web pages to detect whether a user has a given add-on
// installed (by trying to load a moz-extension URI referring to a
// web_accessible_resource from the extension). UUIDMap.get()
// returns the UUID for a given add-on ID.
var UUIDMap = {
  _read() {
    let pref = Preferences.get(UUID_MAP_PREF, "{}");
    try {
      return JSON.parse(pref);
    } catch (e) {
      Cu.reportError(`Error parsing ${UUID_MAP_PREF}.`);
      return {};
    }
  },

  _write(map) {
    Preferences.set(UUID_MAP_PREF, JSON.stringify(map));
  },

  get(id, create = true) {
    let map = this._read();

    if (id in map) {
      return map[id];
    }

    let uuid = null;
    if (create) {
      uuid = uuidGen.generateUUID().number;
      uuid = uuid.slice(1, -1); // Strip { and } off the UUID.

      map[id] = uuid;
      this._write(map);
    }
    return uuid;
  },

  remove(id) {
    let map = this._read();
    delete map[id];
    this._write(map);
  },
};

// This is the old interface that UUIDMap replaced, to be removed when
// the references listed in bug 1291399 are updated.
/* exported getExtensionUUID */
function getExtensionUUID(id) {
  return UUIDMap.get(id, true);
}

// For extensions that have called setUninstallURL(), send an event
// so the browser can display the URL.
var UninstallObserver = {
  initialized: false,

  init() {
    if (!this.initialized) {
      AddonManager.addAddonListener(this);
      XPCOMUtils.defineLazyPreferenceGetter(this, "leaveStorage", LEAVE_STORAGE_PREF, false);
      XPCOMUtils.defineLazyPreferenceGetter(this, "leaveUuid", LEAVE_UUID_PREF, false);
      this.initialized = true;
    }
  },

  onUninstalling(addon) {
    let extension = GlobalManager.extensionMap.get(addon.id);
    if (extension) {
      // Let any other interested listeners respond
      // (e.g., display the uninstall URL)
      Management.emit("uninstall", extension);
    }
  },

  onUninstalled(addon) {
    let uuid = UUIDMap.get(addon.id, false);
    if (!uuid) {
      return;
    }

    if (!this.leaveStorage) {
      // Clear browser.local.storage
      ExtensionStorage.clear(addon.id);

      // Clear any IndexedDB storage created by the extension
      let baseURI = NetUtil.newURI(`moz-extension://${uuid}/`);
      let principal = Services.scriptSecurityManager.createCodebasePrincipal(
        baseURI, {addonId: addon.id}
      );
      Services.qms.clearStoragesForPrincipal(principal);

      // Clear localStorage created by the extension
      let attrs = JSON.stringify({addonId: addon.id});
      Services.obs.notifyObservers(null, "clear-origin-data", attrs);
    }

    if (!this.leaveUuid) {
      // Clear the entry in the UUID map
      UUIDMap.remove(addon.id);
    }
  },
};

// Responsible for loading extension APIs into the right globals.
GlobalManager = {
  // Map[extension ID -> Extension]. Determines which extension is
  // responsible for content under a particular extension ID.
  extensionMap: new Map(),
  initialized: false,

  init(extension) {
    if (this.extensionMap.size == 0) {
      Services.obs.addObserver(this, "document-element-inserted", false);
      UninstallObserver.init();
      this.initialized = true;
    }

    this.extensionMap.set(extension.id, extension);
  },

  uninit(extension) {
    this.extensionMap.delete(extension.id);

    if (this.extensionMap.size == 0 && this.initialized) {
      Services.obs.removeObserver(this, "document-element-inserted");
      this.initialized = false;
    }
  },

  getExtension(extensionId) {
    return this.extensionMap.get(extensionId);
  },

  injectInObject(extension, context, defaultCallback, dest, namespaces = null) {
    let api = Management.generateAPIs(extension, context, Management.apis, namespaces);
    injectAPI(api, dest);

    let schemaApi = Management.generateAPIs(extension, context, Management.schemaApis, namespaces);

    // Add in any extra API namespaces which do not have implementations
    // outside of their schema file.
    schemaApi.extensionTypes = {};

    let schemaWrapper = {
      get principal() {
        return context.principal;
      },

      get cloneScope() {
        return context.cloneScope;
      },

      hasPermission(permission) {
        return extension.hasPermission(permission);
      },

      callFunction(path, name, args) {
        return findPathInObject(schemaApi, path)[name](...args);
      },

      callFunctionNoReturn(path, name, args) {
        return findPathInObject(schemaApi, path)[name](...args);
      },

      callAsyncFunction(path, name, args, callback) {
        // We pass an empty stub function as a default callback for
        // the `chrome` API, so promise objects are not returned,
        // and lastError values are reported immediately.
        if (callback === null) {
          callback = defaultCallback;
        }

        let promise;
        try {
          promise = findPathInObject(schemaApi, path)[name](...args);
        } catch (e) {
          promise = Promise.reject(e);
        }

        return context.wrapPromise(promise || Promise.resolve(), callback);
      },

      shouldInject(namespace, name) {
        if (namespaces && !namespaces.includes(namespace)) {
          return false;
        }
        return findPathInObject(schemaApi, [namespace]) != null;
      },

      getProperty(path, name) {
        return findPathInObject(schemaApi, path)[name];
      },

      setProperty(path, name, value) {
        findPathInObject(schemaApi, path)[name] = value;
      },

      addListener(path, name, listener, args) {
        return findPathInObject(schemaApi, path)[name].addListener.call(null, listener, ...args);
      },
      removeListener(path, name, listener) {
        return findPathInObject(schemaApi, path)[name].removeListener.call(null, listener);
      },
      hasListener(path, name, listener) {
        return findPathInObject(schemaApi, path)[name].hasListener.call(null, listener);
      },
    };
    Schemas.inject(dest, schemaWrapper);
  },

  observe(document, topic, data) {
    let contentWindow = document.defaultView;
    if (!contentWindow) {
      return;
    }

    let inject = (extension, context) => {
      // We create two separate sets of bindings, one for the `chrome`
      // global, and one for the `browser` global. The latter returns
      // Promise objects if a callback is not passed, while the former
      // does not.
      let injectObject = (name, defaultCallback) => {
        let browserObj = Cu.createObjectIn(contentWindow, {defineAs: name});
        this.injectInObject(extension, context, defaultCallback, browserObj);
      };

      injectObject("browser", null);
      injectObject("chrome", () => {});
    };

    let id = ExtensionManagement.getAddonIdForWindow(contentWindow);

    // We don't inject privileged APIs into sub-frames of a UI page.
    const {FULL_PRIVILEGES} = ExtensionManagement.API_LEVELS;
    if (ExtensionManagement.getAPILevelForWindow(contentWindow, id) !== FULL_PRIVILEGES) {
      return;
    }

    // We don't inject privileged APIs if the addonId is null
    // or doesn't exist.
    if (!this.extensionMap.has(id)) {
      return;
    }

    let docShell = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDocShell);

    let parentDocument = docShell.parent.QueryInterface(Ci.nsIDocShell)
                                 .contentViewer.DOMDocument;

    let browser = docShell.chromeEventHandler;
    // If this is a sub-frame of the add-on manager, use that <browser>
    // element rather than the top-level chrome event handler.
    if (contentWindow.frameElement && parentDocument.documentURI == "about:addons") {
      browser = contentWindow.frameElement;
    }

    let type = "tab";
    if (browser.hasAttribute("webextension-view-type")) {
      type = browser.getAttribute("webextension-view-type");
    } else if (browser.classList.contains("inline-options-browser")) {
      // Options pages are currently displayed inline, but in Chrome
      // and in our UI mock-ups for a later milestone, they're
      // pop-ups.
      type = "popup";
    }

    let extension = this.extensionMap.get(id);
    let uri = document.documentURIObject;
    let incognito = PrivateBrowsingUtils.isContentWindowPrivate(contentWindow);

    let context = new ExtensionContext(extension, {type, contentWindow, uri, docShell, incognito});
    inject(extension, context);
    if (type == "background") {
      this._initializeBackgroundPage(contentWindow);
    }

    let innerWindowID = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;

    let onUnload = subject => {
      let windowId = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
      if (windowId == innerWindowID) {
        Services.obs.removeObserver(onUnload, "inner-window-destroyed");
        context.unload();
      }
    };
    Services.obs.addObserver(onUnload, "inner-window-destroyed", false);
  },

  _initializeBackgroundPage(contentWindow) {
    // Override the `alert()` method inside background windows;
    // we alias it to console.log().
    // See: https://bugzilla.mozilla.org/show_bug.cgi?id=1203394
    let alertDisplayedWarning = false;
    let alertOverwrite = text => {
      if (!alertDisplayedWarning) {
        require("devtools/client/framework/devtools-browser");

        let hudservice = require("devtools/client/webconsole/hudservice");
        hudservice.openBrowserConsoleOrFocus();

        contentWindow.console.warn("alert() is not supported in background windows; please use console.log instead.");

        alertDisplayedWarning = true;
      }

      contentWindow.console.log(text);
    };
    Cu.exportFunction(alertOverwrite, contentWindow, {defineAs: "alert"});
  },
};

// Represents the data contained in an extension, contained either
// in a directory or a zip file, which may or may not be installed.
// This class implements the functionality of the Extension class,
// primarily related to manifest parsing and localization, which is
// useful prior to extension installation or initialization.
//
// No functionality of this class is guaranteed to work before
// |readManifest| has been called, and completed.
this.ExtensionData = class {
  constructor(rootURI) {
    this.rootURI = rootURI;

    this.manifest = null;
    this.id = null;
    this.uuid = null;
    this.localeData = null;
    this._promiseLocales = null;

    this.apiNames = new Set();
    this.dependencies = new Set();
    this.permissions = new Set();

    this.errors = [];
  }

  get builtinMessages() {
    return null;
  }

  get logger() {
    let id = this.id || "<unknown>";
    return Log.repository.getLogger(LOGGER_ID_BASE + id);
  }

  // Report an error about the extension's manifest file.
  manifestError(message) {
    this.packagingError(`Reading manifest: ${message}`);
  }

  // Report an error about the extension's general packaging.
  packagingError(message) {
    this.errors.push(message);
    this.logger.error(`Loading extension '${this.id}': ${message}`);
  }

  /**
   * Returns the moz-extension: URL for the given path within this
   * extension.
   *
   * Must not be called unless either the `id` or `uuid` property has
   * already been set.
   *
   * @param {string} path The path portion of the URL.
   * @returns {string}
   */
  getURL(path = "") {
    if (!(this.id || this.uuid)) {
      throw new Error("getURL may not be called before an `id` or `uuid` has been set");
    }
    if (!this.uuid) {
      this.uuid = UUIDMap.get(this.id);
    }
    return `moz-extension://${this.uuid}/${path}`;
  }

  readDirectory(path) {
    return Task.spawn(function* () {
      if (this.rootURI instanceof Ci.nsIFileURL) {
        let uri = NetUtil.newURI(this.rootURI.resolve("./" + path));
        let fullPath = uri.QueryInterface(Ci.nsIFileURL).file.path;

        let iter = new OS.File.DirectoryIterator(fullPath);
        let results = [];

        try {
          yield iter.forEach(entry => {
            results.push(entry);
          });
        } catch (e) {
          // Always return a list, even if the directory does not exist (or is
          // not a directory) for symmetry with the ZipReader behavior.
        }
        iter.close();

        return results;
      }

      if (!(this.rootURI instanceof Ci.nsIJARURI &&
            this.rootURI.JARFile instanceof Ci.nsIFileURL)) {
        // This currently happens for app:// URLs passed to us by
        // UserCustomizations.jsm
        return [];
      }

      // FIXME: We need a way to do this without main thread IO.

      let file = this.rootURI.JARFile.file;
      let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(Ci.nsIZipReader);
      try {
        zipReader.open(file);

        let results = [];

        // Normalize the directory path.
        path = path.replace(/\/\/+/g, "/").replace(/^\/|\/$/g, "") + "/";

        // Escape pattern metacharacters.
        let pattern = path.replace(/[[\]()?*~|$\\]/g, "\\$&");

        let enumerator = zipReader.findEntries(pattern + "*");
        while (enumerator.hasMore()) {
          let name = enumerator.getNext();
          if (!name.startsWith(path)) {
            throw new Error("Unexpected ZipReader entry");
          }

          // The enumerator returns the full path of all entries.
          // Trim off the leading path, and filter out entries from
          // subdirectories.
          name = name.slice(path.length);
          if (name && !/\/./.test(name)) {
            results.push({
              name: name.replace("/", ""),
              isDir: name.endsWith("/"),
            });
          }
        }

        return results;
      } finally {
        zipReader.close();
      }
    }.bind(this));
  }

  readJSON(path) {
    return new Promise((resolve, reject) => {
      let uri = this.rootURI.resolve(`./${path}`);

      NetUtil.asyncFetch({uri, loadUsingSystemPrincipal: true}, (inputStream, status) => {
        if (!Components.isSuccessCode(status)) {
          reject(new Error(status));
          return;
        }
        try {
          let text = NetUtil.readInputStreamToString(inputStream, inputStream.available(),
                                                     {charset: "utf-8"});

          text = text.replace(COMMENT_REGEXP, "$1");

          resolve(JSON.parse(text));
        } catch (e) {
          reject(e);
        }
      });
    });
  }

  // Reads the extension's |manifest.json| file, and stores its
  // parsed contents in |this.manifest|.
  readManifest() {
    return Promise.all([
      this.readJSON("manifest.json"),
      Management.lazyInit(),
    ]).then(([manifest]) => {
      this.manifest = manifest;
      this.rawManifest = manifest;

      if (manifest && manifest.default_locale) {
        return this.initLocale();
      }
    }).then(() => {
      let context = {
        url: this.baseURI && this.baseURI.spec,

        principal: this.principal,

        logError: error => {
          this.logger.warn(`Loading extension '${this.id}': Reading manifest: ${error}`);
        },

        preprocessors: {},
      };

      if (this.localeData) {
        context.preprocessors.localize = this.localize.bind(this);
      }

      let normalized = Schemas.normalize(this.manifest, "manifest.WebExtensionManifest", context);
      if (normalized.error) {
        this.manifestError(normalized.error);
      } else {
        this.manifest = normalized.value;
      }

      try {
        // Do not override the add-on id that has been already assigned.
        if (!this.id && this.manifest.applications.gecko.id) {
          this.id = this.manifest.applications.gecko.id;
        }
      } catch (e) {
        // Errors are handled by the type checks above.
      }

      let permissions = this.manifest.permissions || [];

      let whitelist = [];
      for (let perm of permissions) {
        this.permissions.add(perm);

        let match = /^(\w+)(?:\.(\w+)(?:\.\w+)*)?$/.exec(perm);
        if (!match) {
          whitelist.push(perm);
        } else if (match[1] == "experiments" && match[2]) {
          this.apiNames.add(match[2]);
        }
      }
      this.whiteListedHosts = new MatchPattern(whitelist);

      for (let api of this.apiNames) {
        this.dependencies.add(`${api}@experiments.addons.mozilla.org`);
      }

      return this.manifest;
    });
  }

  localizeMessage(...args) {
    return this.localeData.localizeMessage(...args);
  }

  localize(...args) {
    return this.localeData.localize(...args);
  }

  // If a "default_locale" is specified in that manifest, returns it
  // as a Gecko-compatible locale string. Otherwise, returns null.
  get defaultLocale() {
    if (this.manifest.default_locale != null) {
      return this.normalizeLocaleCode(this.manifest.default_locale);
    }

    return null;
  }

  // Normalizes a Chrome-compatible locale code to the appropriate
  // Gecko-compatible variant. Currently, this means simply
  // replacing underscores with hyphens.
  normalizeLocaleCode(locale) {
    return String.replace(locale, /_/g, "-");
  }

  // Reads the locale file for the given Gecko-compatible locale code, and
  // stores its parsed contents in |this.localeMessages.get(locale)|.
  readLocaleFile(locale) {
    return Task.spawn(function* () {
      let locales = yield this.promiseLocales();
      let dir = locales.get(locale) || locale;
      let file = `_locales/${dir}/messages.json`;

      try {
        let messages = yield this.readJSON(file);
        return this.localeData.addLocale(locale, messages, this);
      } catch (e) {
        this.packagingError(`Loading locale file ${file}: ${e}`);
        return new Map();
      }
    }.bind(this));
  }

  // Reads the list of locales available in the extension, and returns a
  // Promise which resolves to a Map upon completion.
  // Each map key is a Gecko-compatible locale code, and each value is the
  // "_locales" subdirectory containing that locale:
  //
  // Map(gecko-locale-code -> locale-directory-name)
  promiseLocales() {
    if (!this._promiseLocales) {
      this._promiseLocales = Task.spawn(function* () {
        let locales = new Map();

        let entries = yield this.readDirectory("_locales");
        for (let file of entries) {
          if (file.isDir) {
            let locale = this.normalizeLocaleCode(file.name);
            locales.set(locale, file.name);
          }
        }

        this.localeData = new LocaleData({
          defaultLocale: this.defaultLocale,
          locales,
          builtinMessages: this.builtinMessages,
        });

        return locales;
      }.bind(this));
    }

    return this._promiseLocales;
  }

  // Reads the locale messages for all locales, and returns a promise which
  // resolves to a Map of locale messages upon completion. Each key in the map
  // is a Gecko-compatible locale code, and each value is a locale data object
  // as returned by |readLocaleFile|.
  initAllLocales() {
    return Task.spawn(function* () {
      let locales = yield this.promiseLocales();

      yield Promise.all(Array.from(locales.keys(),
                                   locale => this.readLocaleFile(locale)));

      let defaultLocale = this.defaultLocale;
      if (defaultLocale) {
        if (!locales.has(defaultLocale)) {
          this.manifestError('Value for "default_locale" property must correspond to ' +
                             'a directory in "_locales/". Not found: ' +
                             JSON.stringify(`_locales/${this.manifest.default_locale}/`));
        }
      } else if (locales.size) {
        this.manifestError('The "default_locale" property is required when a ' +
                           '"_locales/" directory is present.');
      }

      return this.localeData.messages;
    }.bind(this));
  }

  // Reads the locale file for the given Gecko-compatible locale code, or the
  // default locale if no locale code is given, and sets it as the currently
  // selected locale on success.
  //
  // Pre-loads the default locale for fallback message processing, regardless
  // of the locale specified.
  //
  // If no locales are unavailable, resolves to |null|.
  initLocale(locale = this.defaultLocale) {
    return Task.spawn(function* () {
      if (locale == null) {
        return null;
      }

      let promises = [this.readLocaleFile(locale)];

      let {defaultLocale} = this;
      if (locale != defaultLocale && !this.localeData.has(defaultLocale)) {
        promises.push(this.readLocaleFile(defaultLocale));
      }

      let results = yield Promise.all(promises);

      this.localeData.selectedLocale = locale;
      return results[0];
    }.bind(this));
  }
};


/**
 * A skeleton Extension-like object, used for testing, which installs an
 * add-on via the add-on manager when startup() is called, and
 * uninstalles it on shutdown().
 *
 * @param {string} id
 * @param {nsIFile} file
 * @param {nsIURI} rootURI
 * @param {string} installType
 */
class MockExtension {
  constructor(file, rootURI, installType) {
    this.id = null;
    this.file = file;
    this.rootURI = rootURI;
    this.installType = installType;
    this.addon = null;

    let promiseEvent = eventName => new Promise(resolve => {
      let onstartup = (msg, extension) => {
        if (this.addon && extension.id == this.addon.id) {
          Management.off(eventName, onstartup);

          this.id = extension.id;
          this._extension = extension;
          resolve(extension);
        }
      };
      Management.on(eventName, onstartup);
    });

    this._extension = null;
    this._extensionPromise = promiseEvent("startup");
    this._readyPromise = promiseEvent("ready");
  }

  testMessage(...args) {
    return this._extension.testMessage(...args);
  }

  on(...args) {
    this._extensionPromise.then(extension => {
      extension.on(...args);
    });
  }

  off(...args) {
    this._extensionPromise.then(extension => {
      extension.off(...args);
    });
  }

  startup() {
    if (this.installType == "temporary") {
      return AddonManager.installTemporaryAddon(this.file).then(addon => {
        this.addon = addon;
        return this._readyPromise;
      });
    } else if (this.installType == "permanent") {
      return new Promise((resolve, reject) => {
        AddonManager.getInstallForFile(this.file, install => {
          let listener = {
            onInstallFailed: reject,
            onInstallEnded: (install, newAddon) => {
              this.addon = newAddon;
              resolve(this._readyPromise);
            },
          };

          install.addListener(listener);
          install.install();
        });
      });
    }
    throw new Error("installType must be one of: temporary, permanent");
  }

  shutdown() {
    this.addon.uninstall();
    return this.cleanupGeneratedFile();
  }

  cleanupGeneratedFile() {
    flushJarCache(this.file);
    return OS.File.remove(this.file.path);
  }
}

// We create one instance of this class per extension. |addonData|
// comes directly from bootstrap.js when initializing.
this.Extension = class extends ExtensionData {
  constructor(addonData) {
    super(addonData.resourceURI);

    this.uuid = UUIDMap.get(addonData.id);

    if (addonData.cleanupFile) {
      Services.obs.addObserver(this, "xpcom-shutdown", false);
      this.cleanupFile = addonData.cleanupFile || null;
      delete addonData.cleanupFile;
    }

    this.addonData = addonData;
    this.id = addonData.id;
    this.baseURI = NetUtil.newURI(this.getURL("")).QueryInterface(Ci.nsIURL);
    this.principal = this.createPrincipal();

    this.views = new Set();

    this.onStartup = null;

    this.hasShutdown = false;
    this.onShutdown = new Set();

    this.uninstallURL = null;

    this.apis = [];
    this.whiteListedHosts = null;
    this.webAccessibleResources = null;

    this.emitter = new EventEmitter();
  }

  /**
   * This code is designed to make it easy to test a WebExtension
   * without creating a bunch of files. Everything is contained in a
   * single JSON blob.
   *
   * Properties:
   *   "background": "<JS code>"
   *     A script to be loaded as the background script.
   *     The "background" section of the "manifest" property is overwritten
   *     if this is provided.
   *   "manifest": {...}
   *     Contents of manifest.json
   *   "files": {"filename1": "contents1", ...}
   *     Data to be included as files. Can be referenced from the manifest.
   *     If a manifest file is provided here, it takes precedence over
   *     a generated one. Always use "/" as a directory separator.
   *     Directories should appear here only implicitly (as a prefix
   *     to file names)
   *
   * To make things easier, the value of "background" and "files"[] can
   * be a function, which is converted to source that is run.
   *
   * The generated extension is stored in the system temporary directory,
   * and an nsIFile object pointing to it is returned.
   *
   * @param {object} data
   * @returns {nsIFile}
   */
  static generateXPI(data) {
    let manifest = data.manifest;
    if (!manifest) {
      manifest = {};
    }

    let files = data.files;
    if (!files) {
      files = {};
    }

    function provide(obj, keys, value, override = false) {
      if (keys.length == 1) {
        if (!(keys[0] in obj) || override) {
          obj[keys[0]] = value;
        }
      } else {
        if (!(keys[0] in obj)) {
          obj[keys[0]] = {};
        }
        provide(obj[keys[0]], keys.slice(1), value, override);
      }
    }

    provide(manifest, ["name"], "Generated extension");
    provide(manifest, ["manifest_version"], 2);
    provide(manifest, ["version"], "1.0");

    if (data.background) {
      let bgScript = uuidGen.generateUUID().number + ".js";

      provide(manifest, ["background", "scripts"], [bgScript], true);
      files[bgScript] = data.background;
    }

    provide(files, ["manifest.json"], manifest);

    return this.generateZipFile(files);
  }

  static generateZipFile(files, baseName = "generated-extension.xpi") {
    let ZipWriter = Components.Constructor("@mozilla.org/zipwriter;1", "nsIZipWriter");
    let zipW = new ZipWriter();

    let file = FileUtils.getFile("TmpD", [baseName]);
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

    const MODE_WRONLY = 0x02;
    const MODE_TRUNCATE = 0x20;
    zipW.open(file, MODE_WRONLY | MODE_TRUNCATE);

    // Needs to be in microseconds for some reason.
    let time = Date.now() * 1000;

    function generateFile(filename) {
      let components = filename.split("/");
      let path = "";
      for (let component of components.slice(0, -1)) {
        path += component + "/";
        if (!zipW.hasEntry(path)) {
          zipW.addEntryDirectory(path, time, false);
        }
      }
    }

    for (let filename in files) {
      let script = files[filename];
      if (typeof(script) == "function") {
        script = "(" + script.toString() + ")()";
      } else if (instanceOf(script, "Object") || instanceOf(script, "Array")) {
        script = JSON.stringify(script);
      }

      if (!instanceOf(script, "ArrayBuffer")) {
        script = new TextEncoder("utf-8").encode(script).buffer;
      }

      let stream = Cc["@mozilla.org/io/arraybuffer-input-stream;1"].createInstance(Ci.nsIArrayBufferInputStream);
      stream.setData(script, 0, script.byteLength);

      generateFile(filename);
      zipW.addEntryStream(filename, time, 0, stream, false);
    }

    zipW.close();

    return file;
  }

  /**
   * Generates a new extension using |Extension.generateXPI|, and initializes a
   * new |Extension| instance which will execute it.
   *
   * @param {object} data
   * @returns {Extension}
   */
  static generate(data) {
    let file = this.generateXPI(data);

    flushJarCache(file);
    Services.ppmm.broadcastAsyncMessage("Extension:FlushJarCache", {path: file.path});

    let fileURI = Services.io.newFileURI(file);
    let jarURI = Services.io.newURI("jar:" + fileURI.spec + "!/", null, null);

    // This may be "temporary" or "permanent".
    if (data.useAddonManager) {
      return new MockExtension(file, jarURI, data.useAddonManager);
    }

    let id;
    if (data.manifest) {
      if (data.manifest.applications && data.manifest.applications.gecko) {
        id = data.manifest.applications.gecko.id;
      } else if (data.manifest.browser_specific_settings && data.manifest.browser_specific_settings.gecko) {
        id = data.manifest.browser_specific_settings.gecko.id;
      }
    }
    if (!id) {
      id = uuidGen.generateUUID().number;
    }

    return new Extension({
      id,
      resourceURI: jarURI,
      cleanupFile: file,
    });
  }

  on(hook, f) {
    return this.emitter.on(hook, f);
  }

  off(hook, f) {
    return this.emitter.off(hook, f);
  }

  emit(...args) {
    return this.emitter.emit(...args);
  }

  testMessage(...args) {
    Management.emit("test-message", this, ...args);
  }

  createPrincipal(uri = this.baseURI) {
    return Services.scriptSecurityManager.createCodebasePrincipal(
      uri, {addonId: this.id});
  }

  // Checks that the given URL is a child of our baseURI.
  isExtensionURL(url) {
    let uri = Services.io.newURI(url, null, null);

    let common = this.baseURI.getCommonBaseSpec(uri);
    return common == this.baseURI.spec;
  }

  readManifest() {
    return super.readManifest().then(manifest => {
      if (AppConstants.RELEASE_BUILD) {
        return manifest;
      }

      // Load Experiments APIs that this extension depends on.
      return Promise.all(
        Array.from(this.apiNames, api => ExtensionAPIs.load(api))
      ).then(apis => {
        for (let API of apis) {
          this.apis.push(new API(this));
        }

        return manifest;
      });
    });
  }

  // Representation of the extension to send to content
  // processes. This should include anything the content process might
  // need.
  serialize() {
    return {
      id: this.id,
      uuid: this.uuid,
      manifest: this.manifest,
      resourceURL: this.addonData.resourceURI.spec,
      baseURL: this.baseURI.spec,
      content_scripts: this.manifest.content_scripts || [],  // eslint-disable-line camelcase
      webAccessibleResources: this.webAccessibleResources.serialize(),
      whiteListedHosts: this.whiteListedHosts.serialize(),
      localeData: this.localeData.serialize(),
      permissions: this.permissions,
    };
  }

  broadcast(msg, data) {
    return new Promise(resolve => {
      let count = Services.ppmm.childCount;
      Services.ppmm.addMessageListener(msg + "Complete", function listener() {
        count--;
        if (count == 0) {
          Services.ppmm.removeMessageListener(msg + "Complete", listener);
          resolve();
        }
      });
      Services.ppmm.broadcastAsyncMessage(msg, data);
    });
  }

  runManifest(manifest) {
    // Strip leading slashes from web_accessible_resources.
    let strippedWebAccessibleResources = [];
    if (manifest.web_accessible_resources) {
      strippedWebAccessibleResources = manifest.web_accessible_resources.map(path => path.replace(/^\/+/, ""));
    }

    this.webAccessibleResources = new MatchGlobs(strippedWebAccessibleResources);

    let promises = [];
    for (let directive in manifest) {
      if (manifest[directive] !== null) {
        promises.push(Management.emit(`manifest_${directive}`, directive, this, manifest));
      }
    }

    let data = Services.ppmm.initialProcessData;
    if (!data["Extension:Extensions"]) {
      data["Extension:Extensions"] = [];
    }
    let serial = this.serialize();
    data["Extension:Extensions"].push(serial);

    return this.broadcast("Extension:Startup", serial).then(() => {
      return Promise.all(promises);
    });
  }

  callOnClose(obj) {
    this.onShutdown.add(obj);
  }

  forgetOnClose(obj) {
    this.onShutdown.delete(obj);
  }

  get builtinMessages() {
    return new Map([
      ["@@extension_id", this.uuid],
    ]);
  }

  // Reads the locale file for the given Gecko-compatible locale code, or if
  // no locale is given, the available locale closest to the UI locale.
  // Sets the currently selected locale on success.
  initLocale(locale = undefined) {
    // Ugh.
    let super_ = super.initLocale.bind(this);

    return Task.spawn(function* () {
      if (locale === undefined) {
        let locales = yield this.promiseLocales();

        let localeList = Array.from(locales.keys(), locale => {
          return {name: locale, locales: [locale]};
        });

        let match = Locale.findClosestLocale(localeList);
        locale = match ? match.name : this.defaultLocale;
      }

      return super_(locale);
    }.bind(this));
  }

  startup() {
    let started = false;
    return this.readManifest().then(() => {
      ExtensionManagement.startupExtension(this.uuid, this.addonData.resourceURI, this);
      started = true;

      if (!this.hasShutdown) {
        return this.initLocale();
      }
    }).then(() => {
      if (this.errors.length) {
        // b2g add-ons generate manifest errors that we've silently
        // ignoring prior to adding this check.
        if (!this.rootURI.schemeIs("app")) {
          return Promise.reject({errors: this.errors});
        }
      }

      if (this.hasShutdown) {
        return;
      }

      GlobalManager.init(this);

      Management.emit("startup", this);

      return this.runManifest(this.manifest);
    }).then(() => {
      Management.emit("ready", this);
    }).catch(e => {
      dump(`Extension error: ${e.message} ${e.filename || e.fileName}:${e.lineNumber} :: ${e.stack || new Error().stack}\n`);
      Cu.reportError(e);

      if (started) {
        ExtensionManagement.shutdownExtension(this.uuid);
      }

      this.cleanupGeneratedFile();

      throw e;
    });
  }

  cleanupGeneratedFile() {
    if (!this.cleanupFile) {
      return;
    }

    let file = this.cleanupFile;
    this.cleanupFile = null;

    Services.obs.removeObserver(this, "xpcom-shutdown");

    this.broadcast("Extension:FlushJarCache", {path: file.path}).then(() => {
      // We can't delete this file until everyone using it has
      // closed it (because Windows is dumb). So we wait for all the
      // child processes (including the parent) to flush their JAR
      // caches. These caches may keep the file open.
      file.remove(false);
    });
  }

  shutdown() {
    this.hasShutdown = true;
    if (!this.manifest) {
      ExtensionManagement.shutdownExtension(this.uuid);

      this.cleanupGeneratedFile();
      return;
    }

    GlobalManager.uninit(this);

    for (let view of this.views) {
      view.shutdown();
    }

    for (let obj of this.onShutdown) {
      obj.close();
    }

    for (let api of this.apis) {
      api.destroy();
    }

    Management.emit("shutdown", this);

    Services.ppmm.broadcastAsyncMessage("Extension:Shutdown", {id: this.id});

    MessageChannel.abortResponses({extensionId: this.id});

    ExtensionManagement.shutdownExtension(this.uuid);

    this.cleanupGeneratedFile();
  }

  observe(subject, topic, data) {
    if (topic == "xpcom-shutdown") {
      this.cleanupGeneratedFile();
    }
  }

  hasPermission(perm) {
    let match = /^manifest:(.*)/.exec(perm);
    if (match) {
      return this.manifest[match[1]] != null;
    }

    return this.permissions.has(perm);
  }

  get name() {
    return this.localize(this.manifest.name);
  }
};
