/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionContent"];

/* globals ExtensionContent */

/*
 * This file handles the content process side of extensions. It mainly
 * takes care of content script injection, content script APIs, and
 * messaging.
 */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionManagement",
                                  "resource://gre/modules/ExtensionManagement.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LanguageDetector",
                                  "resource:///modules/translation/LanguageDetector.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MatchPattern",
                                  "resource://gre/modules/MatchPattern.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MatchGlobs",
                                  "resource://gre/modules/MatchPattern.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MessageChannel",
                                  "resource://gre/modules/MessageChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PromiseUtils",
                                  "resource://gre/modules/PromiseUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebNavigationFrames",
                                  "resource://gre/modules/WebNavigationFrames.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  runSafeSyncWithoutClone,
  BaseContext,
  LocaleData,
  Messenger,
  injectAPI,
  flushJarCache,
  detectLanguage,
  promiseDocumentReady,
  ChildAPIManager,
} = ExtensionUtils;

function isWhenBeforeOrSame(when1, when2) {
  let table = {"document_start": 0,
               "document_end": 1,
               "document_idle": 2};
  return table[when1] <= table[when2];
}

function getInnerWindowID(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindowUtils)
    .currentInnerWindowID;
}

// This is the fairly simple API that we inject into content
// scripts.
var api = context => {
  return {
    runtime: {
      connect: function(extensionId, connectInfo) {
        if (!connectInfo) {
          connectInfo = extensionId;
          extensionId = null;
        }
        let name = connectInfo && connectInfo.name || "";
        let recipient = extensionId ? {extensionId} : {extensionId: context.extensionId};
        return context.messenger.connect(context.messageManager, name, recipient);
      },

      get id() {
        return context.extensionId;
      },

      get lastError() {
        return context.lastError;
      },

      getManifest: function() {
        return Cu.cloneInto(context.extension.manifest, context.cloneScope);
      },

      getURL: function(url) {
        return context.extension.baseURI.resolve(url);
      },

      onConnect: context.messenger.onConnect("runtime.onConnect"),

      onMessage: context.messenger.onMessage("runtime.onMessage"),

      sendMessage: function(...args) {
        let options; // eslint-disable-line no-unused-vars
        let extensionId, message, responseCallback;
        if (args.length == 1) {
          message = args[0];
        } else if (args.length == 2) {
          [message, responseCallback] = args;
        } else {
          [extensionId, message, options, responseCallback] = args;
        }

        let recipient = extensionId ? {extensionId} : {extensionId: context.extensionId};
        return context.messenger.sendMessage(context.messageManager, message, recipient, responseCallback);
      },
    },

    extension: {
      getURL: function(url) {
        return context.extension.baseURI.resolve(url);
      },

      get lastError() {
        return context.lastError;
      },

      inIncognitoContext: PrivateBrowsingUtils.isContentWindowPrivate(context.contentWindow),
    },

    i18n: {
      getMessage: function(messageName, substitutions) {
        return context.extension.localizeMessage(messageName, substitutions, {cloneScope: context.cloneScope});
      },

      getAcceptLanguages: function(callback) {
        let result = context.extension.localeData.acceptLanguages;
        return context.wrapPromise(Promise.resolve(result), callback);
      },

      getUILanguage: function() {
        return context.extension.localeData.uiLocale;
      },

      detectLanguage: function(text, callback) {
        let result = detectLanguage(text);
        return context.wrapPromise(result, callback);
      },
    },
  };
};

// Represents a content script.
function Script(extension, options, deferred = PromiseUtils.defer()) {
  this.extension = extension;
  this.options = options;
  this.run_at = this.options.run_at;
  this.js = this.options.js || [];
  this.css = this.options.css || [];
  this.remove_css = this.options.remove_css;

  this.deferred = deferred;

  this.matches_ = new MatchPattern(this.options.matches);
  this.exclude_matches_ = new MatchPattern(this.options.exclude_matches || null);
  // TODO: MatchPattern should pre-mangle host-only patterns so that we
  // don't need to call a separate match function.
  this.matches_host_ = new MatchPattern(this.options.matchesHost || null);
  this.include_globs_ = new MatchGlobs(this.options.include_globs);
  this.exclude_globs_ = new MatchGlobs(this.options.exclude_globs);

  this.requiresCleanup = !this.remove_css && (this.css.length > 0 || options.cssCode);
}

Script.prototype = {
  get cssURLs() {
    // We can handle CSS urls (css) and CSS code (cssCode).
    let urls = [];
    for (let url of this.css) {
      urls.push(this.extension.baseURI.resolve(url));
    }

    if (this.options.cssCode) {
      let url = "data:text/css;charset=utf-8," + encodeURIComponent(this.options.cssCode);
      urls.push(url);
    }

    return urls;
  },

  matches(window) {
    let uri = window.document.documentURIObject;
    if (!(this.matches_.matches(uri) || this.matches_host_.matchesIgnoringPath(uri))) {
      return false;
    }

    if (this.exclude_matches_.matches(uri)) {
      return false;
    }

    if (this.options.include_globs != null) {
      if (!this.include_globs_.matches(uri.spec)) {
        return false;
      }
    }

    if (this.exclude_globs_.matches(uri.spec)) {
      return false;
    }

    if (this.options.frame_id != null) {
      if (WebNavigationFrames.getFrameId(window) != this.options.frame_id) {
        return false;
      }
    } else if (!this.options.all_frames && window.top != window) {
      return false;
    }

    // TODO: match_about_blank.

    return true;
  },

  cleanup(window) {
    if (!this.remove_css) {
      let winUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);

      for (let url of this.cssURLs) {
        runSafeSyncWithoutClone(winUtils.removeSheetUsingURIString, url, winUtils.AUTHOR_SHEET);
      }
    }
  },

  tryInject(window, sandbox, shouldRun) {
    if (!this.matches(window)) {
      this.deferred.reject({message: "No matching window"});
      return;
    }

    if (shouldRun("document_start")) {
      let winUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);

      let {cssURLs} = this;
      if (cssURLs.length > 0) {
        let method = this.remove_css ? winUtils.removeSheetUsingURIString : winUtils.loadSheetUsingURIString;
        for (let url of cssURLs) {
          runSafeSyncWithoutClone(method, url, winUtils.AUTHOR_SHEET);
        }

        this.deferred.resolve();
      }
    }

    let result;
    let scheduled = this.run_at || "document_idle";
    if (shouldRun(scheduled)) {
      for (let url of this.js) {
        // On gonk we need to load the resources asynchronously because the
        // app: channels only support asyncOpen. This is safe only in the
        // `document_idle` state.
        if (AppConstants.platform == "gonk" && scheduled != "document_idle") {
          Cu.reportError(`Script injection: ignoring ${url} at ${scheduled}`);
          continue;
        }
        url = this.extension.baseURI.resolve(url);

        let options = {
          target: sandbox,
          charset: "UTF-8",
          async: AppConstants.platform == "gonk",
        };
        try {
          result = Services.scriptloader.loadSubScriptWithOptions(url, options);
        } catch (e) {
          Cu.reportError(e);
          this.deferred.reject(e);
        }
      }

      if (this.options.jsCode) {
        try {
          result = Cu.evalInSandbox(this.options.jsCode, sandbox, "latest");
        } catch (e) {
          Cu.reportError(e);
          this.deferred.reject(e);
        }
      }

      this.deferred.resolve(result);
    }
  },
};

function getWindowMessageManager(contentWindow) {
  let ir = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDocShell)
                        .QueryInterface(Ci.nsIInterfaceRequestor);
  try {
    return ir.getInterface(Ci.nsIContentFrameMessageManager);
  } catch (e) {
    // Some windows don't support this interface (hidden window).
    return null;
  }
}

var DocumentManager;
var ExtensionManager;

// Scope in which extension content script code can run. It uses
// Cu.Sandbox to run the code. There is a separate scope for each
// frame.
class ExtensionContext extends BaseContext {
  constructor(extensionId, contentWindow, contextOptions = {}) {
    super(extensionId);

    let {isExtensionPage} = contextOptions;

    this.isExtensionPage = isExtensionPage;
    this.extension = ExtensionManager.get(extensionId);
    this.extensionId = extensionId;
    this.contentWindow = contentWindow;

    let frameId = WebNavigationFrames.getFrameId(contentWindow);
    this.frameId = frameId;

    this.scripts = [];

    let mm = getWindowMessageManager(contentWindow);
    this.messageManager = mm;

    let prin;
    let contentPrincipal = contentWindow.document.nodePrincipal;
    let ssm = Services.scriptSecurityManager;

    // copy origin attributes from the content window origin attributes to
    // preserve the user context id. overwrite the addonId.
    let attrs = contentPrincipal.originAttributes;
    attrs.addonId = extensionId;
    let extensionPrincipal = ssm.createCodebasePrincipal(this.extension.baseURI, attrs);
    Object.defineProperty(this, "principal",
                          {value: extensionPrincipal, enumerable: true, configurable: true});

    if (ssm.isSystemPrincipal(contentPrincipal)) {
      // Make sure we don't hand out the system principal by accident.
      // also make sure that the null principal has the right origin attributes
      prin = ssm.createNullPrincipal(attrs);
    } else {
      prin = [contentPrincipal, extensionPrincipal];
    }

    if (isExtensionPage) {
      if (ExtensionManagement.getAddonIdForWindow(this.contentWindow) != extensionId) {
        throw new Error("Invalid target window for this extension context");
      }
      // This is an iframe with content script API enabled and its principal should be the
      // contentWindow itself. (we create a sandbox with the contentWindow as principal and with X-rays disabled
      // because it enables us to create the APIs object in this sandbox object and then copying it
      // into the iframe's window, see Bug 1214658 for rationale)
      this.sandbox = Cu.Sandbox(contentWindow, {
        sandboxPrototype: contentWindow,
        wantXrays: false,
        isWebExtensionContentScript: true,
      });
    } else {
      // This metadata is required by the Developer Tools, in order for
      // the content script to be associated with both the extension and
      // the tab holding the content page.
      let metadata = {
        "inner-window-id": getInnerWindowID(contentWindow),
        addonId: attrs.addonId,
      };

      this.sandbox = Cu.Sandbox(prin, {
        metadata,
        sandboxPrototype: contentWindow,
        wantXrays: true,
        isWebExtensionContentScript: true,
        wantGlobalProperties: ["XMLHttpRequest", "fetch"],
      });
    }

    let delegate = {
      getSender(context, target, sender) {
        // Nothing to do here.
      },
    };

    let url = contentWindow.location.href;
    // The |sender| parameter is passed directly to the extension.
    let sender = {id: this.extension.uuid, frameId, url};
    // Properties in |filter| must match those in the |recipient|
    // parameter of sendMessage.
    let filter = {extensionId, frameId};
    this.messenger = new Messenger(this, [mm], sender, filter, delegate);

    this.chromeObj = Cu.createObjectIn(this.sandbox, {defineAs: "browser"});

    // Sandboxes don't get Xrays for some weird compatibility
    // reason. However, we waive here anyway in case that changes.
    Cu.waiveXrays(this.sandbox).chrome = this.chromeObj;

    let apis = {
      "storage": "chrome://extensions/content/schemas/storage.json",
      "test": "chrome://extensions/content/schemas/test.json",
    };

    let incognito = PrivateBrowsingUtils.isContentWindowPrivate(this.contentWindow);
    this.childManager = new ChildAPIManager(this, mm, Object.keys(apis), {
      type: "content_script",
      url,
      incognito,
    });

    for (let api in apis) {
      Schemas.load(apis[api]);
    }
    Schemas.inject(this.chromeObj, this.childManager);

    injectAPI(api(this), this.chromeObj);

    // This is an iframe with content script API enabled. (See Bug 1214658 for rationale)
    if (isExtensionPage) {
      Cu.waiveXrays(this.contentWindow).chrome = this.chromeObj;
      Cu.waiveXrays(this.contentWindow).browser = this.chromeObj;
    }
  }

  get cloneScope() {
    return this.sandbox;
  }

  execute(script, shouldRun) {
    script.tryInject(this.contentWindow, this.sandbox, shouldRun);
  }

  addScript(script) {
    let state = DocumentManager.getWindowState(this.contentWindow);
    this.execute(script, scheduled => isWhenBeforeOrSame(scheduled, state));

    // Save the script in case it has pending operations in later load
    // states, but only if we're before document_idle, or require cleanup.
    if (state != "document_idle" || script.requiresCleanup) {
      this.scripts.push(script);
    }
  }

  triggerScripts(documentState) {
    for (let script of this.scripts) {
      this.execute(script, scheduled => scheduled == documentState);
    }
    if (documentState == "document_idle") {
      // Don't bother saving scripts after document_idle.
      this.scripts = this.scripts.filter(script => script.requiresCleanup);
    }
  }

  close() {
    super.unload();

    for (let script of this.scripts) {
      if (script.requiresCleanup) {
        script.cleanup(this.contentWindow);
      }
    }

    this.childManager.close();

    // Overwrite the content script APIs with an empty object if the APIs objects are still
    // defined in the content window (See Bug 1214658 for rationale).
    if (this.isExtensionPage && !Cu.isDeadWrapper(this.contentWindow) &&
        Cu.waiveXrays(this.contentWindow).browser === this.chromeObj) {
      Cu.createObjectIn(this.contentWindow, {defineAs: "browser"});
      Cu.createObjectIn(this.contentWindow, {defineAs: "chrome"});
    }
    Cu.nukeSandbox(this.sandbox);
    this.sandbox = null;
  }
}

// Responsible for creating ExtensionContexts and injecting content
// scripts into them when new documents are created.
DocumentManager = {
  extensionCount: 0,

  // Map[windowId -> Map[extensionId -> ExtensionContext]]
  contentScriptWindows: new Map(),

  // Map[windowId -> ExtensionContext]
  extensionPageWindows: new Map(),

  init() {
    Services.obs.addObserver(this, "document-element-inserted", false);
    Services.obs.addObserver(this, "inner-window-destroyed", false);
  },

  uninit() {
    Services.obs.removeObserver(this, "document-element-inserted");
    Services.obs.removeObserver(this, "inner-window-destroyed");
  },

  getWindowState(contentWindow) {
    let readyState = contentWindow.document.readyState;
    if (readyState == "complete") {
      return "document_idle";
    } else if (readyState == "interactive") {
      return "document_end";
    } else {
      return "document_start";
    }
  },

  observe: function(subject, topic, data) {
    if (topic == "document-element-inserted") {
      let document = subject;
      let window = document && document.defaultView;
      if (!document || !document.location || !window) {
        return;
      }

      // Make sure we only load into frames that ExtensionContent.init
      // was called on (i.e., not frames for social or sidebars).
      let mm = getWindowMessageManager(window);
      if (!mm || !ExtensionContent.globals.has(mm)) {
        return;
      }

      // Enable the content script APIs should be available in subframes' window
      // if it is recognized as a valid addon id (see Bug 1214658 for rationale).
      const {CONTENTSCRIPT_PRIVILEGES} = ExtensionManagement.API_LEVELS;
      let extensionId = ExtensionManagement.getAddonIdForWindow(window);

      if (ExtensionManagement.getAPILevelForWindow(window, extensionId) == CONTENTSCRIPT_PRIVILEGES &&
          ExtensionManager.get(extensionId)) {
        DocumentManager.getExtensionPageContext(extensionId, window);
      }

      this.trigger("document_start", window);
      /* eslint-disable mozilla/balanced-listeners */
      window.addEventListener("DOMContentLoaded", this, true);
      window.addEventListener("load", this, true);
      /* eslint-enable mozilla/balanced-listeners */
    } else if (topic == "inner-window-destroyed") {
      let windowId = subject.QueryInterface(Ci.nsISupportsPRUint64).data;

      MessageChannel.abortResponses({innerWindowID: windowId});

      // Close any existent content-script context for the destroyed window.
      if (this.contentScriptWindows.has(windowId)) {
        let extensions = this.contentScriptWindows.get(windowId);
        for (let [, context] of extensions) {
          context.close();
        }

        this.contentScriptWindows.delete(windowId);
      }

      // Close any existent iframe extension page context for the destroyed window.
      if (this.extensionPageWindows.has(windowId)) {
        let context = this.extensionPageWindows.get(windowId);
        context.close();
        this.extensionPageWindows.delete(windowId);
      }
    }
  },

  handleEvent: function(event) {
    let window = event.currentTarget;
    if (event.target != window.document) {
      // We use capturing listeners so we have precedence over content script
      // listeners, but only care about events targeted to the element we're
      // listening on.
      return;
    }
    window.removeEventListener(event.type, this, true);

    // Need to check if we're still on the right page? Greasemonkey does this.

    if (event.type == "DOMContentLoaded") {
      this.trigger("document_end", window);
    } else if (event.type == "load") {
      this.trigger("document_idle", window);
    }
  },

  // Used to executeScript, insertCSS and removeCSS.
  executeScript(global, extensionId, options) {
    let extension = ExtensionManager.get(extensionId);

    let executeInWin = (window) => {
      let deferred = PromiseUtils.defer();
      let script = new Script(extension, options, deferred);

      if (script.matches(window)) {
        let context = this.getContentScriptContext(extensionId, window);
        context.addScript(script);
        return deferred.promise;
      }
      return null;
    };

    let promises = Array.from(this.enumerateWindows(global.docShell), executeInWin)
                        .filter(promise => promise);

    if (!promises.length) {
      return Promise.reject({message: `No matching window`});
    }
    if (options.all_frames) {
      return Promise.all(promises);
    }
    if (promises.length > 1) {
      return Promise.reject({message: `Internal error: Script matched multiple windows`});
    }
    return promises[0];
  },

  enumerateWindows: function* (docShell) {
    let window = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindow);
    yield window;

    for (let i = 0; i < docShell.childCount; i++) {
      let child = docShell.getChildAt(i).QueryInterface(Ci.nsIDocShell);
      yield* this.enumerateWindows(child);
    }
  },

  getContentScriptGlobalsForWindow(window) {
    let winId = getInnerWindowID(window);
    let extensions = this.contentScriptWindows.get(winId);

    if (extensions) {
      return Array.from(extensions.values(), ctx => ctx.sandbox);
    }

    return [];
  },

  getContentScriptContext(extensionId, window) {
    let winId = getInnerWindowID(window);
    if (!this.contentScriptWindows.has(winId)) {
      this.contentScriptWindows.set(winId, new Map());
    }

    let extensions = this.contentScriptWindows.get(winId);
    if (!extensions.has(extensionId)) {
      let context = new ExtensionContext(extensionId, window);
      extensions.set(extensionId, context);
    }

    return extensions.get(extensionId);
  },

  getExtensionPageContext(extensionId, window) {
    let winId = getInnerWindowID(window);

    let context = this.extensionPageWindows.get(winId);
    if (!context) {
      let context = new ExtensionContext(extensionId, window, {isExtensionPage: true});
      this.extensionPageWindows.set(winId, context);
    }

    return context;
  },

  startupExtension(extensionId) {
    if (this.extensionCount == 0) {
      this.init();
    }
    this.extensionCount++;

    let extension = ExtensionManager.get(extensionId);
    for (let global of ExtensionContent.globals.keys()) {
      // Note that we miss windows in the bfcache here. In theory we
      // could execute content scripts on a pageshow event for that
      // window, but that seems extreme.
      for (let window of this.enumerateWindows(global.docShell)) {
        for (let script of extension.scripts) {
          if (script.matches(window)) {
            let context = this.getContentScriptContext(extensionId, window);
            context.addScript(script);
          }
        }
      }
    }
  },

  shutdownExtension(extensionId) {
    // Clean up content-script contexts on extension shutdown.
    for (let [, extensions] of this.contentScriptWindows) {
      let context = extensions.get(extensionId);
      if (context) {
        context.close();
        extensions.delete(extensionId);
      }
    }

    // Clean up iframe extension page contexts on extension shutdown.
    for (let [winId, context] of this.extensionPageWindows) {
      if (context.extensionId == extensionId) {
        context.close();
        this.extensionPageWindows.delete(winId);
      }
    }

    MessageChannel.abortResponses({extensionId});

    this.extensionCount--;
    if (this.extensionCount == 0) {
      this.uninit();
    }
  },

  trigger(when, window) {
    let state = this.getWindowState(window);

    if (state == "document_start") {
      for (let [extensionId, extension] of ExtensionManager.extensions) {
        for (let script of extension.scripts) {
          if (script.matches(window)) {
            let context = this.getContentScriptContext(extensionId, window);
            context.addScript(script);
          }
        }
      }
    } else {
      let contexts = this.contentScriptWindows.get(getInnerWindowID(window)) || new Map();
      for (let context of contexts.values()) {
        context.triggerScripts(state);
      }
    }
  },
};

// Represents a browser extension in the content process.
function BrowserExtensionContent(data) {
  this.id = data.id;
  this.uuid = data.uuid;
  this.data = data;
  this.scripts = data.content_scripts.map(scriptData => new Script(this, scriptData));
  this.webAccessibleResources = new MatchGlobs(data.webAccessibleResources);
  this.whiteListedHosts = new MatchPattern(data.whiteListedHosts);
  this.permissions = data.permissions;

  this.localeData = new LocaleData(data.localeData);

  this.manifest = data.manifest;
  this.baseURI = Services.io.newURI(data.baseURL, null, null);

  let uri = Services.io.newURI(data.resourceURL, null, null);

  if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
    // Extension.jsm takes care of this in the parent.
    ExtensionManagement.startupExtension(this.uuid, uri, this);
  }
}

BrowserExtensionContent.prototype = {
  shutdown() {
    if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
      ExtensionManagement.shutdownExtension(this.uuid);
    }
  },

  localizeMessage(...args) {
    return this.localeData.localizeMessage(...args);
  },

  localize(...args) {
    return this.localeData.localize(...args);
  },
};

ExtensionManager = {
  // Map[extensionId, BrowserExtensionContent]
  extensions: new Map(),

  init() {
    Schemas.init();

    Services.cpmm.addMessageListener("Extension:Startup", this);
    Services.cpmm.addMessageListener("Extension:Shutdown", this);
    Services.cpmm.addMessageListener("Extension:FlushJarCache", this);

    if (Services.cpmm.initialProcessData && "Extension:Extensions" in Services.cpmm.initialProcessData) {
      let extensions = Services.cpmm.initialProcessData["Extension:Extensions"];
      for (let data of extensions) {
        this.extensions.set(data.id, new BrowserExtensionContent(data));
        DocumentManager.startupExtension(data.id);
      }
    }
  },

  get(extensionId) {
    return this.extensions.get(extensionId);
  },

  receiveMessage({name, data}) {
    let extension;
    switch (name) {
      case "Extension:Startup": {
        extension = new BrowserExtensionContent(data);
        this.extensions.set(data.id, extension);
        DocumentManager.startupExtension(data.id);
        Services.cpmm.sendAsyncMessage("Extension:StartupComplete");
        break;
      }

      case "Extension:Shutdown": {
        extension = this.extensions.get(data.id);
        extension.shutdown();
        DocumentManager.shutdownExtension(data.id);
        this.extensions.delete(data.id);
        break;
      }

      case "Extension:FlushJarCache": {
        let nsIFile = Components.Constructor("@mozilla.org/file/local;1", "nsIFile",
                                             "initWithPath");
        let file = new nsIFile(data.path);
        flushJarCache(file);
        Services.cpmm.sendAsyncMessage("Extension:FlushJarCacheComplete");
        break;
      }
    }
  },
};

class ExtensionGlobal {
  constructor(global) {
    this.global = global;

    MessageChannel.addListener(global, "Extension:Capture", this);
    MessageChannel.addListener(global, "Extension:DetectLanguage", this);
    MessageChannel.addListener(global, "Extension:Execute", this);
    MessageChannel.addListener(global, "WebNavigation:GetFrame", this);
    MessageChannel.addListener(global, "WebNavigation:GetAllFrames", this);

    this.windowId = global.content
                          .QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils)
                          .outerWindowID;

    global.sendAsyncMessage("Extension:TopWindowID", {windowId: this.windowId});
  }

  uninit() {
    this.global.sendAsyncMessage("Extension:RemoveTopWindowID", {windowId: this.windowId});
  }

  get messageFilterStrict() {
    return {
      innerWindowID: getInnerWindowID(this.global.content),
    };
  }

  receiveMessage({target, messageName, recipient, data}) {
    switch (messageName) {
      case "Extension:Capture":
        return this.handleExtensionCapture(data.width, data.height, data.options);
      case "Extension:DetectLanguage":
        return this.handleDetectLanguage(target);
      case "Extension:Execute":
        return this.handleExtensionExecute(target, recipient.extensionId, data.options);
      case "WebNavigation:GetFrame":
        return this.handleWebNavigationGetFrame(data.options);
      case "WebNavigation:GetAllFrames":
        return this.handleWebNavigationGetAllFrames();
    }
  }

  handleExtensionCapture(width, height, options) {
    let win = this.global.content;

    const XHTML_NS = "http://www.w3.org/1999/xhtml";
    let canvas = win.document.createElementNS(XHTML_NS, "canvas");
    canvas.width = width;
    canvas.height = height;
    canvas.mozOpaque = true;

    let ctx = canvas.getContext("2d");

    // We need to scale the image to the visible size of the browser,
    // in order for the result to appear as the user sees it when
    // settings like full zoom come into play.
    ctx.scale(canvas.width / win.innerWidth, canvas.height / win.innerHeight);

    ctx.drawWindow(win, win.scrollX, win.scrollY, win.innerWidth, win.innerHeight, "#fff");

    return canvas.toDataURL(`image/${options.format}`, options.quality / 100);
  }

  handleDetectLanguage(target) {
    let doc = target.content.document;

    return promiseDocumentReady(doc).then(() => {
      let elem = doc.documentElement;

      let language = (elem.getAttribute("xml:lang") || elem.getAttribute("lang") ||
                      doc.contentLanguage || null);

      // We only want the last element of the TLD here.
      // Only country codes have any effect on the results, but other
      // values cause no harm.
      let tld = doc.location.hostname.match(/[a-z]*$/)[0];

      // The CLD2 library used by the language detector is capable of
      // analyzing raw HTML. Unfortunately, that takes much more memory,
      // and since it's hosted by emscripten, and therefore can't shrink
      // its heap after it's grown, it has a performance cost.
      // So we send plain text instead.
      let encoder = Cc["@mozilla.org/layout/documentEncoder;1?type=text/plain"].createInstance(Ci.nsIDocumentEncoder);
      encoder.init(doc, "text/plain", encoder.SkipInvisibleContent);
      let text = encoder.encodeToStringWithMaxLength(60 * 1024);

      let encoding = doc.characterSet;

      return LanguageDetector.detectLanguage({language, tld, text, encoding})
                             .then(result => result.language);
    });
  }

  // Used to executeScript, insertCSS and removeCSS.
  handleExtensionExecute(target, extensionId, options) {
    return DocumentManager.executeScript(target, extensionId, options).then(result => {
      try {
        // Make sure we can structured-clone the result value before
        // we try to send it back over the message manager.
        Cu.cloneInto(result, target);
      } catch (e) {
        return Promise.reject({message: "Script returned non-structured-clonable data"});
      }
      return result;
    });
  }

  handleWebNavigationGetFrame({frameId}) {
    return WebNavigationFrames.getFrame(this.global.docShell, frameId);
  }

  handleWebNavigationGetAllFrames() {
    return WebNavigationFrames.getAllFrames(this.global.docShell);
  }
}

this.ExtensionContent = {
  globals: new Map(),

  init(global) {
    this.globals.set(global, new ExtensionGlobal(global));
  },

  uninit(global) {
    this.globals.get(global).uninit();
    this.globals.delete(global);
  },

  // This helper is exported to be integrated in the devtools RDP actors,
  // that can use it to retrieve the existent WebExtensions ContentScripts
  // of a target window and be able to show the ContentScripts source in the
  // DevTools Debugger panel.
  getContentScriptGlobalsForWindow(window) {
    return DocumentManager.getContentScriptGlobalsForWindow(window);
  },
};

ExtensionManager.init();
