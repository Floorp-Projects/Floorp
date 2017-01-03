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
 *
 * This file is also the initial entry point for addon processes.
 * ExtensionChild.jsm is responsible for functionality specific to addon
 * processes.
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
XPCOMUtils.defineLazyModuleGetter(this, "PromiseUtils",
                                  "resource://gre/modules/PromiseUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebNavigationFrames",
                                  "resource://gre/modules/WebNavigationFrames.jsm");

Cu.import("resource://gre/modules/ExtensionChild.jsm");
Cu.import("resource://gre/modules/ExtensionCommon.jsm");
Cu.import("resource://gre/modules/ExtensionUtils.jsm");

const {
  EventEmitter,
  LocaleData,
  defineLazyGetter,
  flushJarCache,
  getInnerWindowID,
  promiseDocumentReady,
  runSafeSyncWithoutClone,
} = ExtensionUtils;

const {
  BaseContext,
  SchemaAPIManager,
} = ExtensionCommon;

const {
  ChildAPIManager,
  Messenger,
} = ExtensionChild;

XPCOMUtils.defineLazyGetter(this, "console", ExtensionUtils.getConsole);

const CATEGORY_EXTENSION_SCRIPTS_CONTENT = "webextension-scripts-content";

function isWhenBeforeOrSame(when1, when2) {
  let table = {"document_start": 0,
               "document_end": 1,
               "document_idle": 2};
  return table[when1] <= table[when2];
}

var apiManager = new class extends SchemaAPIManager {
  constructor() {
    super("content");
    this.initialized = false;
  }

  generateAPIs(...args) {
    if (!this.initialized) {
      this.initialized = true;
      for (let [/* name */, value] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCRIPTS_CONTENT)) {
        this.loadScript(value);
      }
    }
    return super.generateAPIs(...args);
  }

  registerSchemaAPI(namespace, envType, getAPI) {
    if (envType == "content_child") {
      super.registerSchemaAPI(namespace, envType, getAPI);
    }
  }
}();

// Represents a content script.
function Script(extension, options, deferred = PromiseUtils.defer()) {
  this.extension = extension;
  this.options = options;
  this.run_at = this.options.run_at;
  this.js = this.options.js || [];
  this.css = this.options.css || [];
  this.remove_css = this.options.remove_css;
  this.match_about_blank = this.options.match_about_blank;

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
    let principal = window.document.nodePrincipal;

    // If mozAddonManager is present on this page, don't allow
    // content scripts.
    if (window.navigator.mozAddonManager !== undefined) {
      return false;
    }

    if (this.match_about_blank) {
      // When matching top-level about:blank documents,
      // allow loading into any with a NullPrincipal.
      if (uri.spec === "about:blank" && window === window.top && principal.isNullPrincipal) {
        return true;
      }

      // When matching about:blank/srcdoc iframes, the checks below
      // need to be performed against the "owner" document's URI.
      if (["about:blank", "about:srcdoc"].includes(uri.spec)) {
        uri = principal.URI;
      }
    }

    // Documents from data: URIs also inherit the principal.
    if (Services.netUtils.URIChainHasFlags(uri, Ci.nsIProtocolHandler.URI_INHERITS_SECURITY_CONTEXT)) {
      if (!this.match_about_blank) {
        return false;
      }
      uri = principal.URI;
    }

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

  /**
   * Tries to inject this script into the given window and sandbox, if
   * there are pending operations for the window's current load state.
   *
   * @param {Window} window
   *        The DOM Window to inject the scripts and CSS into.
   * @param {Sandbox} sandbox
   *        A Sandbox inheriting from `window` in which to evaluate the
   *        injected scripts.
   * @param {function} shouldRun
   *        A function which, when passed the document load state that a
   *        script is expected to run at, returns `true` if we should
   *        currently be injecting scripts for that load state.
   *
   *        For initial injection of a script, this function should
   *        return true if the document is currently in or has already
   *        passed through the given state. For injections triggered by
   *        document state changes, it should only return true if the
   *        given state exactly matches the state that triggered the
   *        change.
   * @param {string} when
   *        The document's current load state, or if triggered by a
   *        document state change, the new document state that triggered
   *        the injection.
   */
  tryInject(window, sandbox, shouldRun, when) {
    if (shouldRun("document_start")) {
      let {cssURLs} = this;
      if (cssURLs.length > 0) {
        let winUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDOMWindowUtils);

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
        url = this.extension.baseURI.resolve(url);

        let options = {
          target: sandbox,
          charset: "UTF-8",
          // Inject asynchronously unless we're expected to inject before any
          // page scripts have run, and we haven't already missed that boat.
          async: this.run_at !== "document_start" || when !== "document_start",
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

/**
 * An execution context for semi-privileged extension content scripts.
 *
 * This is the child side of the ContentScriptContextParent class
 * defined in ExtensionParent.jsm.
 */
class ContentScriptContextChild extends BaseContext {
  constructor(extension, contentWindow, contextOptions = {}) {
    super("content_child", extension);

    let {isExtensionPage} = contextOptions;

    this.isExtensionPage = isExtensionPage;

    this.setContentWindow(contentWindow);

    let frameId = WebNavigationFrames.getFrameId(contentWindow);
    this.frameId = frameId;

    this.scripts = [];

    let contentPrincipal = contentWindow.document.nodePrincipal;
    let ssm = Services.scriptSecurityManager;

    // copy origin attributes from the content window origin attributes to
    // preserve the user context id. overwrite the addonId.
    let attrs = contentPrincipal.originAttributes;
    attrs.addonId = this.extension.id;
    let extensionPrincipal = ssm.createCodebasePrincipal(this.extension.baseURI, attrs);

    let principal;
    if (ssm.isSystemPrincipal(contentPrincipal)) {
      // Make sure we don't hand out the system principal by accident.
      // also make sure that the null principal has the right origin attributes
      principal = ssm.createNullPrincipal(attrs);
    } else {
      principal = [contentPrincipal, extensionPrincipal];
    }

    if (isExtensionPage) {
      if (ExtensionManagement.getAddonIdForWindow(this.contentWindow) != this.extension.id) {
        throw new Error("Invalid target window for this extension context");
      }
      // This is an iframe with content script API enabled and its principal should be the
      // contentWindow itself. (we create a sandbox with the contentWindow as principal and with X-rays disabled
      // because it enables us to create the APIs object in this sandbox object and then copying it
      // into the iframe's window, see Bug 1214658 for rationale)
      this.sandbox = Cu.Sandbox(contentWindow, {
        sandboxPrototype: contentWindow,
        sameZoneAs: contentWindow,
        wantXrays: false,
        isWebExtensionContentScript: true,
      });
    } else {
      // This metadata is required by the Developer Tools, in order for
      // the content script to be associated with both the extension and
      // the tab holding the content page.
      let metadata = {
        "inner-window-id": this.innerWindowID,
        addonId: attrs.addonId,
      };

      this.sandbox = Cu.Sandbox(principal, {
        metadata,
        sandboxPrototype: contentWindow,
        sameZoneAs: contentWindow,
        wantXrays: true,
        isWebExtensionContentScript: true,
        wantExportHelpers: true,
        wantGlobalProperties: ["XMLHttpRequest", "fetch"],
        originAttributes: attrs,
      });

      Cu.evalInSandbox(`
        window.JSON = JSON;
        window.XMLHttpRequest = XMLHttpRequest;
        window.fetch = fetch;
      `, this.sandbox);
    }

    Object.defineProperty(this, "principal", {
      value: Cu.getObjectPrincipal(this.sandbox),
      enumerable: true,
      configurable: true,
    });

    this.url = contentWindow.location.href;

    defineLazyGetter(this, "chromeObj", () => {
      let chromeObj = Cu.createObjectIn(this.sandbox);

      Schemas.inject(chromeObj, this.childManager);
      return chromeObj;
    });

    Schemas.exportLazyGetter(this.sandbox, "browser", () => this.chromeObj);
    Schemas.exportLazyGetter(this.sandbox, "chrome", () => this.chromeObj);

    // This is an iframe with content script API enabled (bug 1214658)
    if (isExtensionPage) {
      Schemas.exportLazyGetter(this.contentWindow,
                               "browser", () => this.chromeObj);
      Schemas.exportLazyGetter(this.contentWindow,
                               "chrome", () => this.chromeObj);
    }
  }

  get cloneScope() {
    return this.sandbox;
  }

  execute(script, shouldRun, when) {
    script.tryInject(this.contentWindow, this.sandbox, shouldRun, when);
  }

  addScript(script, when) {
    let state = DocumentManager.getWindowState(this.contentWindow);
    this.execute(script, scheduled => isWhenBeforeOrSame(scheduled, state), when);

    // Save the script in case it has pending operations in later load
    // states, but only if we're before document_idle, or require cleanup.
    if (state != "document_idle" || script.requiresCleanup) {
      this.scripts.push(script);
    }
  }

  triggerScripts(documentState) {
    for (let script of this.scripts) {
      this.execute(script, scheduled => scheduled == documentState, documentState);
    }
    if (documentState == "document_idle") {
      // Don't bother saving scripts after document_idle.
      this.scripts = this.scripts.filter(script => script.requiresCleanup);
    }
  }

  close() {
    super.unload();

    if (this.contentWindow) {
      for (let script of this.scripts) {
        if (script.requiresCleanup) {
          script.cleanup(this.contentWindow);
        }
      }

      // Overwrite the content script APIs with an empty object if the APIs objects are still
      // defined in the content window (bug 1214658).
      if (this.isExtensionPage) {
        Cu.createObjectIn(this.contentWindow, {defineAs: "browser"});
        Cu.createObjectIn(this.contentWindow, {defineAs: "chrome"});
      }
    }
    Cu.nukeSandbox(this.sandbox);
    this.sandbox = null;
  }
}

defineLazyGetter(ContentScriptContextChild.prototype, "messenger", function() {
  // The |sender| parameter is passed directly to the extension.
  let sender = {id: this.extension.uuid, frameId: this.frameId, url: this.url};
  let filter = {extensionId: this.extension.id};
  let optionalFilter = {frameId: this.frameId};

  return new Messenger(this, [this.messageManager], sender, filter, optionalFilter);
});

defineLazyGetter(ContentScriptContextChild.prototype, "childManager", function() {
  let localApis = {};
  apiManager.generateAPIs(this, localApis);

  let childManager = new ChildAPIManager(this, this.messageManager, localApis, {
    envType: "content_parent",
    url: this.url,
  });

  this.callOnClose(childManager);

  return childManager;
});

// Responsible for creating ExtensionContexts and injecting content
// scripts into them when new documents are created.
DocumentManager = {
  extensionCount: 0,

  // Map[windowId -> Map[extensionId -> ContentScriptContextChild]]
  contentScriptWindows: new Map(),

  // Map[windowId -> ContentScriptContextChild]
  extensionPageWindows: new Map(),

  init() {
    Services.obs.addObserver(this, "content-document-global-created", false);
    Services.obs.addObserver(this, "document-element-inserted", false);
    Services.obs.addObserver(this, "inner-window-destroyed", false);
  },

  uninit() {
    Services.obs.removeObserver(this, "content-document-global-created");
    Services.obs.removeObserver(this, "document-element-inserted");
    Services.obs.removeObserver(this, "inner-window-destroyed");
  },

  getWindowState(contentWindow) {
    let readyState = contentWindow.document.readyState;
    if (readyState == "complete") {
      return "document_idle";
    }
    if (readyState == "interactive") {
      return "document_end";
    }
    return "document_start";
  },

  loadInto(window) {
    // Enable the content script APIs should be available in subframes' window
    // if it is recognized as a valid addon id (see Bug 1214658 for rationale).
    const {
      NO_PRIVILEGES,
      CONTENTSCRIPT_PRIVILEGES,
      FULL_PRIVILEGES,
    } = ExtensionManagement.API_LEVELS;
    let extensionId = ExtensionManagement.getAddonIdForWindow(window);
    let apiLevel = ExtensionManagement.getAPILevelForWindow(window, extensionId);

    if (apiLevel != NO_PRIVILEGES) {
      let extension = ExtensionManager.get(extensionId);
      if (extension) {
        if (apiLevel == CONTENTSCRIPT_PRIVILEGES) {
          DocumentManager.getExtensionPageContext(extension, window);
        } else if (apiLevel == FULL_PRIVILEGES) {
          ExtensionChild.createExtensionContext(extension, window);
        }
      }
    }
  },

  observe: function(subject, topic, data) {
    // For some types of documents (about:blank), we only see the first
    // notification, for others (data: URIs) we only observe the second.
    if (topic == "content-document-global-created" || topic == "document-element-inserted") {
      let document = subject;
      let window = document && document.defaultView;

      if (topic == "content-document-global-created") {
        window = subject;
        document = window && window.document;
      }

      if (!document || !document.location || !window) {
        return;
      }

      // Make sure we only load into frames that ExtensionContent.init
      // was called on (i.e., not frames for social or sidebars).
      let mm = getWindowMessageManager(window);
      if (!mm || !ExtensionContent.globals.has(mm)) {
        return;
      }

      // Load on document-element-inserted, except for about:blank which doesn't
      // see it, and needs special late handling on DOMContentLoaded event.
      if (topic === "document-element-inserted") {
        this.loadInto(window);
        this.trigger("document_start", window);
      }

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

      ExtensionChild.destroyExtensionContext(windowId);
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
      // By this time, we can be sure if this is an explicit about:blank
      // document, and if it needs special late loading and fake trigger.
      if (window.location.href === "about:blank") {
        this.loadInto(window);
        this.trigger("document_start", window);
      }
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
        let context = this.getContentScriptContext(extension, window);
        context.addScript(script);
        return deferred.promise;
      }
      return null;
    };

    let promises = Array.from(this.enumerateWindows(global.docShell), executeInWin)
                        .filter(promise => promise);

    if (!promises.length) {
      let details = {};
      for (let key of ["all_frames", "frame_id", "match_about_blank", "matchesHost"]) {
        if (key in options) {
          details[key] = options[key];
        }
      }

      return Promise.reject({message: `No window matching ${JSON.stringify(details)}`});
    }
    if (!options.all_frames && promises.length > 1) {
      return Promise.reject({message: `Internal error: Script matched multiple windows`});
    }
    return Promise.all(promises);
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

  getContentScriptContext(extension, window) {
    let winId = getInnerWindowID(window);
    if (!this.contentScriptWindows.has(winId)) {
      this.contentScriptWindows.set(winId, new Map());
    }

    let extensions = this.contentScriptWindows.get(winId);
    if (!extensions.has(extension.id)) {
      let context = new ContentScriptContextChild(extension, window);
      extensions.set(extension.id, context);
    }

    return extensions.get(extension.id);
  },

  getExtensionPageContext(extension, window) {
    let winId = getInnerWindowID(window);

    let context = this.extensionPageWindows.get(winId);
    if (!context) {
      let context = new ContentScriptContextChild(extension, window, {isExtensionPage: true});
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
            let context = this.getContentScriptContext(extension, window);
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
      if (context.extension.id == extensionId) {
        context.close();
        this.extensionPageWindows.delete(winId);
      }
    }

    ExtensionChild.shutdownExtension(extensionId);

    MessageChannel.abortResponses({extensionId});

    this.extensionCount--;
    if (this.extensionCount == 0) {
      this.uninit();
    }
  },

  trigger(when, window) {
    if (when === "document_start") {
      for (let extension of ExtensionManager.extensions.values()) {
        for (let script of extension.scripts) {
          if (script.matches(window)) {
            let context = this.getContentScriptContext(extension, window);
            context.addScript(script, when);
          }
        }
      }
    } else {
      let contexts = this.contentScriptWindows.get(getInnerWindowID(window)) || new Map();
      for (let context of contexts.values()) {
        context.triggerScripts(when);
      }
    }
  },
};

// Represents a browser extension in the content process.
class BrowserExtensionContent extends EventEmitter {
  constructor(data) {
    super();

    this.id = data.id;
    this.uuid = data.uuid;
    this.data = data;
    this.instanceId = data.instanceId;

    this.MESSAGE_EMIT_EVENT = `Extension:EmitEvent:${this.instanceId}`;
    Services.cpmm.addMessageListener(this.MESSAGE_EMIT_EVENT, this);

    this.scripts = data.content_scripts.map(scriptData => new Script(this, scriptData));
    this.webAccessibleResources = new MatchGlobs(data.webAccessibleResources);
    this.whiteListedHosts = new MatchPattern(data.whiteListedHosts);
    this.permissions = data.permissions;
    this.principal = data.principal;

    this.localeData = new LocaleData(data.localeData);

    this.manifest = data.manifest;
    this.baseURI = Services.io.newURI(data.baseURL, null, null);

    // Only used in addon processes.
    this.views = new Set();

    // Only used for devtools views.
    this.devtoolsViews = new Set();

    let uri = Services.io.newURI(data.resourceURL, null, null);

    if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
      // Extension.jsm takes care of this in the parent.
      ExtensionManagement.startupExtension(this.uuid, uri, this);
    }
  }

  shutdown() {
    Services.cpmm.removeMessageListener(this.MESSAGE_EMIT_EVENT, this);

    if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
      ExtensionManagement.shutdownExtension(this.uuid);
    }
  }

  emit(event, ...args) {
    Services.cpmm.sendAsyncMessage(this.MESSAGE_EMIT_EVENT, {event, args});

    super.emit(event, ...args);
  }

  receiveMessage({name, data}) {
    if (name === this.MESSAGE_EMIT_EVENT) {
      super.emit(data.event, ...data.args);
    }
  }

  localizeMessage(...args) {
    return this.localeData.localizeMessage(...args);
  }

  localize(...args) {
    return this.localeData.localize(...args);
  }

  hasPermission(perm) {
    let match = /^manifest:(.*)/.exec(perm);
    if (match) {
      return this.manifest[match[1]] != null;
    }
    return this.permissions.has(perm);
  }
}

ExtensionManager = {
  // Map[extensionId, BrowserExtensionContent]
  extensions: new Map(),

  init() {
    Schemas.init();
    ExtensionChild.initOnce();

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
        flushJarCache(data.path);
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
        .then(result => result.language === "un" ? "und" : result.language);
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
    if (ExtensionManagement.isExtensionProcess) {
      ExtensionChild.init(global);
    }
  },

  uninit(global) {
    if (ExtensionManagement.isExtensionProcess) {
      ExtensionChild.uninit(global);
    }
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
