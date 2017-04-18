/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionContent"];

/* globals ExtensionContent */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LanguageDetector",
                                  "resource:///modules/translation/LanguageDetector.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MessageChannel",
                                  "resource://gre/modules/MessageChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebNavigationFrames",
                                  "resource://gre/modules/WebNavigationFrames.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "styleSheetService",
                                   "@mozilla.org/content/style-sheet-service;1",
                                   "nsIStyleSheetService");

const DocumentEncoder = Components.Constructor(
  "@mozilla.org/layout/documentEncoder;1?type=text/plain",
  "nsIDocumentEncoder", "init");

const Timer = Components.Constructor("@mozilla.org/timer;1", "nsITimer", "initWithCallback");

Cu.import("resource://gre/modules/ExtensionChild.jsm");
Cu.import("resource://gre/modules/ExtensionCommon.jsm");
Cu.import("resource://gre/modules/ExtensionUtils.jsm");

const {
  DefaultMap,
  DefaultWeakMap,
  defineLazyGetter,
  getInnerWindowID,
  getWinUtils,
  promiseDocumentLoaded,
  promiseDocumentReady,
  runSafeSyncWithoutClone,
} = ExtensionUtils;

const {
  BaseContext,
  CanOfAPIs,
  SchemaAPIManager,
} = ExtensionCommon;

const {
  BrowserExtensionContent,
  ChildAPIManager,
  Messenger,
} = ExtensionChild;

XPCOMUtils.defineLazyGetter(this, "console", ExtensionUtils.getConsole);


var DocumentManager;

const CATEGORY_EXTENSION_SCRIPTS_CONTENT = "webextension-scripts-content";

var apiManager = new class extends SchemaAPIManager {
  constructor() {
    super("content");
    this.initialized = false;
  }

  lazyInit() {
    if (!this.initialized) {
      this.initialized = true;
      for (let [/* name */, value] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCRIPTS_CONTENT)) {
        this.loadScript(value);
      }
    }
  }
}();

const SCRIPT_EXPIRY_TIMEOUT_MS = 5 * 60 * 1000;
const SCRIPT_CLEAR_TIMEOUT_MS = 5 * 1000;

const CSS_EXPIRY_TIMEOUT_MS = 30 * 60 * 1000;

const scriptCaches = new WeakSet();
const sheetCacheDocuments = new DefaultWeakMap(() => new WeakSet());

class CacheMap extends DefaultMap {
  constructor(timeout, getter) {
    super(getter);

    this.expiryTimeout = timeout;

    scriptCaches.add(this);
  }

  get(url) {
    let promise = super.get(url);

    promise.lastUsed = Date.now();
    if (promise.timer) {
      promise.timer.cancel();
    }
    promise.timer = Timer(this.delete.bind(this, url),
                          this.expiryTimeout,
                          Ci.nsITimer.TYPE_ONE_SHOT);

    return promise;
  }

  delete(url) {
    if (this.has(url)) {
      super.get(url).timer.cancel();
    }

    super.delete(url);
  }

  clear(timeout = SCRIPT_CLEAR_TIMEOUT_MS) {
    let now = Date.now();
    for (let [url, promise] of this.entries()) {
      if (now - promise.lastUsed >= timeout) {
        this.delete(url);
      }
    }
  }
}

class ScriptCache extends CacheMap {
  constructor(options) {
    super(SCRIPT_EXPIRY_TIMEOUT_MS,
          url => ChromeUtils.compileScript(url, options));
  }
}

class CSSCache extends CacheMap {
  constructor(sheetType) {
    super(CSS_EXPIRY_TIMEOUT_MS, url => {
      let uri = Services.io.newURI(url);
      return styleSheetService.preloadSheetAsync(uri, sheetType).then(sheet => {
        return {url, sheet};
      });
    });
  }

  addDocument(url, document) {
    sheetCacheDocuments.get(this.get(url)).add(document);
  }

  deleteDocument(url, document) {
    sheetCacheDocuments.get(this.get(url)).delete(document);
  }

  delete(url) {
    if (this.has(url)) {
      let promise = this.get(url);

      // Never remove a sheet from the cache if it's still being used by a
      // document. Rule processors can be shared between documents with the
      // same preloaded sheet, so we only lose by removing them while they're
      // still in use.
      let docs = ChromeUtils.nondeterministicGetWeakSetKeys(sheetCacheDocuments.get(promise));
      if (docs.length) {
        return;
      }
    }

    super.delete(url);
  }
}

defineLazyGetter(BrowserExtensionContent.prototype, "staticScripts", () => {
  return new ScriptCache({hasReturnValue: false});
});

defineLazyGetter(BrowserExtensionContent.prototype, "dynamicScripts", () => {
  return new ScriptCache({hasReturnValue: true});
});

defineLazyGetter(BrowserExtensionContent.prototype, "userCSS", () => {
  return new CSSCache(Ci.nsIStyleSheetService.USER_SHEET);
});

defineLazyGetter(BrowserExtensionContent.prototype, "authorCSS", () => {
  return new CSSCache(Ci.nsIStyleSheetService.AUTHOR_SHEET);
});

// Represents a content script.
class Script {
  constructor(extension, options) {
    this.extension = extension;
    this.options = options;

    this.runAt = this.options.run_at;
    this.js = this.options.js || [];
    this.css = this.options.css || [];
    this.remove_css = this.options.remove_css;
    this.css_origin = this.options.css_origin;

    this.cssCache = extension[this.css_origin === "user" ? "userCSS"
                                                         : "authorCSS"];
    this.scriptCache = extension[options.wantReturnValue ? "dynamicScripts"
                                                         : "staticScripts"];

    if (options.wantReturnValue) {
      this.compileScripts();
      this.loadCSS();
    }

    this.requiresCleanup = !this.remove_css && (this.css.length > 0 || options.cssCode);
  }

  compileScripts() {
    return this.js.map(url => this.scriptCache.get(url));
  }

  loadCSS() {
    return this.cssURLs.map(url => this.cssCache.get(url));
  }

  cleanup(window) {
    if (!this.remove_css && this.cssURLs.length) {
      let winUtils = getWinUtils(window);

      let type = this.css_origin === "user" ? winUtils.USER_SHEET : winUtils.AUTHOR_SHEET;
      for (let url of this.cssURLs) {
        this.cssCache.deleteDocument(url, window.document);
        runSafeSyncWithoutClone(winUtils.removeSheetUsingURIString, url, type);
      }

      // Clear any sheets that were kept alive past their timeout as
      // a result of living in this document.
      this.cssCache.clear(CSS_EXPIRY_TIMEOUT_MS);
    }
  }

  async injectInto(window) {
    let context = this.extension.getContext(window);

    if (this.runAt === "document_end") {
      await promiseDocumentReady(window.document);
    } else if (this.runAt === "document_idle") {
      await promiseDocumentLoaded(window.document);
    }

    return this.inject(context);
  }

  /**
   * Tries to inject this script into the given window and sandbox, if
   * there are pending operations for the window's current load state.
   *
   * @param {BaseContext} context
   *        The content script context into which to inject the scripts.
   * @returns {Promise<any>}
   *        Resolves to the last value in the evaluated script, when
   *        execution is complete.
   */
  async inject(context) {
    DocumentManager.lazyInit();
    if (this.requiresCleanup) {
      context.addScript(this);
    }

    let cssPromise;
    if (this.cssURLs.length) {
      let window = context.contentWindow;
      let winUtils = getWinUtils(window);

      let type = this.css_origin === "user" ? winUtils.USER_SHEET : winUtils.AUTHOR_SHEET;

      if (this.remove_css) {
        for (let url of this.cssURLs) {
          this.cssCache.deleteDocument(url, window.document);

          runSafeSyncWithoutClone(winUtils.removeSheetUsingURIString, url, type);
        }
      } else {
        cssPromise = Promise.all(this.loadCSS()).then(sheets => {
          let window = context.contentWindow;
          if (!window) {
            return;
          }

          for (let {url, sheet} of sheets) {
            this.cssCache.addDocument(url, window.document);

            runSafeSyncWithoutClone(winUtils.addSheet, sheet, type);
          }
        });
      }
    }

    let scriptsPromise = Promise.all(this.compileScripts());

    // If we're supposed to inject at the start of the document load,
    // and we haven't already missed that point, block further parsing
    // until the scripts have been loaded.
    let {document} = context.contentWindow;
    if (this.runAt === "document_start" && document.readyState !== "complete") {
      document.blockParsing(scriptsPromise);
    }

    let scripts = await scriptsPromise;
    let result;

    // The evaluations below may throw, in which case the promise will be
    // automatically rejected.
    for (let script of scripts) {
      result = script.executeInGlobal(context.cloneScope);
    }

    if (this.options.jsCode) {
      result = Cu.evalInSandbox(this.options.jsCode, context.cloneScope, "latest");
    }

    await cssPromise;
    return result;
  }
}

defineLazyGetter(Script.prototype, "cssURLs", function() {
  // We can handle CSS urls (css) and CSS code (cssCode).
  let urls = this.css.slice();

  if (this.options.cssCode) {
    urls.push("data:text/css;charset=utf-8," + encodeURIComponent(this.options.cssCode));
  }

  return urls;
});

/**
 * An execution context for semi-privileged extension content scripts.
 *
 * This is the child side of the ContentScriptContextParent class
 * defined in ExtensionParent.jsm.
 */
class ContentScriptContextChild extends BaseContext {
  constructor(extension, contentWindow) {
    super("content_child", extension);

    this.setContentWindow(contentWindow);

    let frameId = WebNavigationFrames.getFrameId(contentWindow);
    this.frameId = frameId;

    this.scripts = [];

    let contentPrincipal = contentWindow.document.nodePrincipal;
    let ssm = Services.scriptSecurityManager;

    // Copy origin attributes from the content window origin attributes to
    // preserve the user context id.
    let attrs = contentPrincipal.originAttributes;
    let extensionPrincipal = ssm.createCodebasePrincipal(this.extension.baseURI, attrs);

    this.isExtensionPage = contentPrincipal.equals(extensionPrincipal);

    let principal;
    if (ssm.isSystemPrincipal(contentPrincipal)) {
      // Make sure we don't hand out the system principal by accident.
      // also make sure that the null principal has the right origin attributes
      principal = ssm.createNullPrincipal(attrs);
    } else if (this.isExtensionPage) {
      principal = contentPrincipal;
    } else {
      principal = [contentPrincipal, extensionPrincipal];
    }

    if (this.isExtensionPage) {
      // This is an iframe with content script API enabled and its principal
      // should be the contentWindow itself. We create a sandbox with the
      // contentWindow as principal and with X-rays disabled because it
      // enables us to create the APIs object in this sandbox object and then
      // copying it into the iframe's window.  See bug 1214658.
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
        addonId: extensionPrincipal.addonId,
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
  }

  injectAPI() {
    if (!this.isExtensionPage) {
      throw new Error("Cannot inject extension API into non-extension window");
    }

    // This is an iframe with content script API enabled (bug 1214658)
    Schemas.exportLazyGetter(this.contentWindow,
                             "browser", () => this.chromeObj);
    Schemas.exportLazyGetter(this.contentWindow,
                             "chrome", () => this.chromeObj);
  }

  get cloneScope() {
    return this.sandbox;
  }

  addScript(script) {
    if (script.requiresCleanup) {
      this.scripts.push(script);
    }
  }

  close() {
    super.unload();

    if (this.contentWindow) {
      for (let script of this.scripts) {
        script.cleanup(this.contentWindow);
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
  let sender = {id: this.extension.id, frameId: this.frameId, url: this.url};
  let filter = {extensionId: this.extension.id};
  let optionalFilter = {frameId: this.frameId};

  return new Messenger(this, [this.messageManager], sender, filter, optionalFilter);
});

defineLazyGetter(ContentScriptContextChild.prototype, "childManager", function() {
  apiManager.lazyInit();

  let localApis = {};
  let can = new CanOfAPIs(this, apiManager, localApis);

  let childManager = new ChildAPIManager(this, this.messageManager, can, {
    envType: "content_parent",
    url: this.url,
  });

  this.callOnClose(childManager);

  return childManager;
});

// Responsible for creating ExtensionContexts and injecting content
// scripts into them when new documents are created.
DocumentManager = {
  // Map[windowId -> Map[ExtensionChild -> ContentScriptContextChild]]
  contexts: new Map(),

  initialized: false,

  lazyInit() {
    if (this.initalized) {
      return;
    }
    this.initialized = true;

    Services.obs.addObserver(this, "inner-window-destroyed");
    Services.obs.addObserver(this, "memory-pressure");
  },

  uninit() {
    Services.obs.removeObserver(this, "inner-window-destroyed");
    Services.obs.removeObserver(this, "memory-pressure");
  },

  observers: {
    "inner-window-destroyed"(subject, topic, data) {
      let windowId = subject.QueryInterface(Ci.nsISupportsPRUint64).data;

      MessageChannel.abortResponses({innerWindowID: windowId});

      // Close any existent content-script context for the destroyed window.
      if (this.contexts.has(windowId)) {
        let extensions = this.contexts.get(windowId);
        for (let context of extensions.values()) {
          context.close();
        }

        this.contexts.delete(windowId);
      }
    },
    "memory-pressure"(subject, topic, data) {
      let timeout = data === "heap-minimize" ? 0 : undefined;

      for (let cache of ChromeUtils.nondeterministicGetWeakSetKeys(scriptCaches)) {
        cache.clear(timeout);
      }
    },
  },

  observe(subject, topic, data) {
    this.observers[topic].call(this, subject, topic, data);
  },

  shutdownExtension(extension) {
    for (let extensions of this.contexts.values()) {
      let context = extensions.get(extension);
      if (context) {
        context.close();
        extensions.delete(extension);
      }
    }
  },

  getContexts(window) {
    let winId = getInnerWindowID(window);

    let extensions = this.contexts.get(winId);
    if (!extensions) {
      extensions = new Map();
      this.contexts.set(winId, extensions);
    }

    return extensions;
  },

  // For test use only.
  getContext(extensionId, window) {
    for (let [extension, context] of this.getContexts(window)) {
      if (extension.id === extensionId) {
        return context;
      }
    }
  },

  getContentScriptGlobals(window) {
    let extensions = this.contexts.get(getInnerWindowID(window));

    if (extensions) {
      return Array.from(extensions.values(), ctx => ctx.sandbox);
    }

    return [];
  },

  initExtensionContext(extension, window) {
    extension.getContext(window).injectAPI();
  },
};

this.ExtensionContent = {
  BrowserExtensionContent,
  Script,

  shutdownExtension(extension) {
    DocumentManager.shutdownExtension(extension);
  },

  // This helper is exported to be integrated in the devtools RDP actors,
  // that can use it to retrieve the existent WebExtensions ContentScripts
  // of a target window and be able to show the ContentScripts source in the
  // DevTools Debugger panel.
  getContentScriptGlobals(window) {
    return DocumentManager.getContentScriptGlobals(window);
  },

  initExtensionContext(extension, window) {
    DocumentManager.initExtensionContext(extension, window);
  },

  getContext(extension, window) {
    let extensions = DocumentManager.getContexts(window);

    let context = extensions.get(extension);
    if (!context) {
      context = new ContentScriptContextChild(extension, window);
      extensions.set(extension, context);
    }
    return context;
  },

  handleExtensionCapture(global, width, height, options) {
    let win = global.content;

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
  },

  handleDetectLanguage(global, target) {
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
      let encoder = new DocumentEncoder(doc, "text/plain", Ci.nsIDocumentEncoder.SkipInvisibleContent);
      let text = encoder.encodeToStringWithMaxLength(60 * 1024);

      let encoding = doc.characterSet;

      return LanguageDetector.detectLanguage({language, tld, text, encoding})
        .then(result => result.language === "un" ? "und" : result.language);
    });
  },

  // Used to executeScript, insertCSS and removeCSS.
  async handleExtensionExecute(global, target, options, script) {
    let executeInWin = (window) => {
      if (script.matchesWindow(window)) {
        return script.injectInto(window);
      }
      return null;
    };

    let promises = Array.from(this.enumerateWindows(global.docShell), executeInWin)
                        .filter(promise => promise);

    if (!promises.length) {
      if (options.frame_id) {
        return Promise.reject({message: `Frame not found, or missing host permission`});
      }

      let frames = options.all_frames ? ", and any iframes" : "";
      return Promise.reject({message: `Missing host permission for the tab${frames}`});
    }
    if (!options.all_frames && promises.length > 1) {
      return Promise.reject({message: `Internal error: Script matched multiple windows`});
    }

    let result = await Promise.all(promises);

    try {
      // Make sure we can structured-clone the result value before
      // we try to send it back over the message manager.
      Cu.cloneInto(result, target);
    } catch (e) {
      const {js} = options;
      const fileName = js.length ? js[js.length - 1] : "<anonymous code>";
      const message = `Script '${fileName}' result is non-structured-clonable data`;
      return Promise.reject({message, fileName});
    }

    return result;
  },

  handleWebNavigationGetFrame(global, {frameId}) {
    return WebNavigationFrames.getFrame(global.docShell, frameId);
  },

  handleWebNavigationGetAllFrames(global) {
    return WebNavigationFrames.getAllFrames(global.docShell);
  },

  // Helpers

  * enumerateWindows(docShell) {
    let enum_ = docShell.getDocShellEnumerator(docShell.typeContent,
                                               docShell.ENUMERATE_FORWARDS);

    for (let docShell of XPCOMUtils.IterSimpleEnumerator(enum_, Ci.nsIInterfaceRequestor)) {
      yield docShell.getInterface(Ci.nsIDOMWindow);
    }
  },
};
