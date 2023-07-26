/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ExtensionProcessScript:
    "resource://gre/modules/ExtensionProcessScript.sys.mjs",
  ExtensionTelemetry: "resource://gre/modules/ExtensionTelemetry.sys.mjs",
  LanguageDetector:
    "resource://gre/modules/translation/LanguageDetector.sys.mjs",
  Schemas: "resource://gre/modules/Schemas.sys.mjs",
  WebNavigationFrames: "resource://gre/modules/WebNavigationFrames.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "styleSheetService",
  "@mozilla.org/content/style-sheet-service;1",
  "nsIStyleSheetService"
);

const Timer = Components.Constructor(
  "@mozilla.org/timer;1",
  "nsITimer",
  "initWithCallback"
);

const ScriptError = Components.Constructor(
  "@mozilla.org/scripterror;1",
  "nsIScriptError",
  "initWithWindowID"
);

import {
  ExtensionChild,
  ExtensionActivityLogChild,
} from "resource://gre/modules/ExtensionChild.sys.mjs";
import { ExtensionCommon } from "resource://gre/modules/ExtensionCommon.sys.mjs";
import { ExtensionUtils } from "resource://gre/modules/ExtensionUtils.sys.mjs";

const {
  DefaultMap,
  DefaultWeakMap,
  getInnerWindowID,
  promiseDocumentIdle,
  promiseDocumentLoaded,
  promiseDocumentReady,
} = ExtensionUtils;

const {
  BaseContext,
  CanOfAPIs,
  SchemaAPIManager,
  defineLazyGetter,
  runSafeSyncWithoutClone,
} = ExtensionCommon;

const { BrowserExtensionContent, ChildAPIManager, Messenger } = ExtensionChild;

ChromeUtils.defineLazyGetter(lazy, "isContentScriptProcess", () => {
  return (
    Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_CONTENT ||
    !WebExtensionPolicy.useRemoteWebExtensions ||
    // Thunderbird still loads some content in the parent process.
    AppConstants.MOZ_APP_NAME == "thunderbird"
  );
});

var DocumentManager;

const CATEGORY_EXTENSION_SCRIPTS_CONTENT = "webextension-scripts-content";

var apiManager = new (class extends SchemaAPIManager {
  constructor() {
    super("content", lazy.Schemas);
    this.initialized = false;
  }

  lazyInit() {
    if (!this.initialized) {
      this.initialized = true;
      this.initGlobal();
      for (let { value } of Services.catMan.enumerateCategory(
        CATEGORY_EXTENSION_SCRIPTS_CONTENT
      )) {
        this.loadScript(value);
      }
    }
  }
})();

const SCRIPT_EXPIRY_TIMEOUT_MS = 5 * 60 * 1000;
const SCRIPT_CLEAR_TIMEOUT_MS = 5 * 1000;

const CSS_EXPIRY_TIMEOUT_MS = 30 * 60 * 1000;
const CSSCODE_EXPIRY_TIMEOUT_MS = 10 * 60 * 1000;

const scriptCaches = new WeakSet();
const sheetCacheDocuments = new DefaultWeakMap(() => new WeakSet());

class CacheMap extends DefaultMap {
  constructor(timeout, getter, extension) {
    super(getter);

    this.expiryTimeout = timeout;

    scriptCaches.add(this);

    // This ensures that all the cached scripts and stylesheets are deleted
    // from the cache and the xpi is no longer actively used.
    // See Bug 1435100 for rationale.
    extension.once("shutdown", () => {
      this.clear(-1);
    });
  }

  get(url) {
    let promise = super.get(url);

    promise.lastUsed = Date.now();
    if (promise.timer) {
      promise.timer.cancel();
    }
    promise.timer = Timer(
      this.delete.bind(this, url),
      this.expiryTimeout,
      Ci.nsITimer.TYPE_ONE_SHOT
    );

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
      // Delete the entry if expired or if clear has been called with timeout -1
      // (which is used to force the cache to clear all the entries, e.g. when the
      // extension is shutting down).
      if (timeout === -1 || now - promise.lastUsed >= timeout) {
        this.delete(url);
      }
    }
  }
}

class ScriptCache extends CacheMap {
  constructor(options, extension) {
    super(SCRIPT_EXPIRY_TIMEOUT_MS, null, extension);
    this.options = options;
  }

  defaultConstructor(url) {
    let promise = ChromeUtils.compileScript(url, this.options);
    promise.then(script => {
      promise.script = script;
    });
    return promise;
  }
}

/**
 * Shared base class for the two specialized CSS caches:
 * CSSCache (for the "url"-based stylesheets) and CSSCodeCache
 * (for the stylesheet defined by plain CSS content as a string).
 */
class BaseCSSCache extends CacheMap {
  constructor(expiryTimeout, defaultConstructor, extension) {
    super(expiryTimeout, defaultConstructor, extension);
  }

  addDocument(key, document) {
    sheetCacheDocuments.get(this.get(key)).add(document);
  }

  deleteDocument(key, document) {
    sheetCacheDocuments.get(this.get(key)).delete(document);
  }

  delete(key) {
    if (this.has(key)) {
      let promise = this.get(key);

      // Never remove a sheet from the cache if it's still being used by a
      // document. Rule processors can be shared between documents with the
      // same preloaded sheet, so we only lose by removing them while they're
      // still in use.
      let docs = ChromeUtils.nondeterministicGetWeakSetKeys(
        sheetCacheDocuments.get(promise)
      );
      if (docs.length) {
        return;
      }
    }

    super.delete(key);
  }
}

/**
 * Cache of the preloaded stylesheet defined by url.
 */
class CSSCache extends BaseCSSCache {
  constructor(sheetType, extension) {
    super(
      CSS_EXPIRY_TIMEOUT_MS,
      url => {
        let uri = Services.io.newURI(url);
        return lazy.styleSheetService
          .preloadSheetAsync(uri, sheetType)
          .then(sheet => {
            return { url, sheet };
          });
      },
      extension
    );
  }
}

/**
 * Cache of the preloaded stylesheet defined by plain CSS content as a string,
 * the key of the cached stylesheet is the hash of its "CSSCode" string.
 */
class CSSCodeCache extends BaseCSSCache {
  constructor(sheetType, extension) {
    super(
      CSSCODE_EXPIRY_TIMEOUT_MS,
      hash => {
        if (!this.has(hash)) {
          // Do not allow the getter to be used to lazily create the cached stylesheet,
          // the cached CSSCode stylesheet has to be explicitly set.
          throw new Error(
            "Unexistent cached cssCode stylesheet: " + Error().stack
          );
        }

        return super.get(hash);
      },
      extension
    );

    // Store the preferred sheetType (used to preload the expected stylesheet type in
    // the addCSSCode method).
    this.sheetType = sheetType;
  }

  addCSSCode(hash, cssCode) {
    if (this.has(hash)) {
      // This cssCode have been already cached, no need to create it again.
      return;
    }
    // The `webext=style` portion is added metadata to help us distinguish
    // different kinds of data URL loads that are triggered with the
    // SystemPrincipal. It shall be removed with bug 1699425.
    const uri = Services.io.newURI(
      "data:text/css;extension=style;charset=utf-8," +
        encodeURIComponent(cssCode)
    );
    const value = lazy.styleSheetService
      .preloadSheetAsync(uri, this.sheetType)
      .then(sheet => {
        return { sheet, uri };
      });

    super.set(hash, value);
  }
}

defineLazyGetter(
  BrowserExtensionContent.prototype,
  "staticScripts",
  function () {
    return new ScriptCache({ hasReturnValue: false }, this);
  }
);

defineLazyGetter(
  BrowserExtensionContent.prototype,
  "dynamicScripts",
  function () {
    return new ScriptCache({ hasReturnValue: true }, this);
  }
);

defineLazyGetter(BrowserExtensionContent.prototype, "userCSS", function () {
  return new CSSCache(Ci.nsIStyleSheetService.USER_SHEET, this);
});

defineLazyGetter(BrowserExtensionContent.prototype, "authorCSS", function () {
  return new CSSCache(Ci.nsIStyleSheetService.AUTHOR_SHEET, this);
});

// These two caches are similar to the above but specialized to cache the cssCode
// using an hash computed from the cssCode string as the key (instead of the generated data
// URI which can be pretty long for bigger injected cssCode).
defineLazyGetter(BrowserExtensionContent.prototype, "userCSSCode", function () {
  return new CSSCodeCache(Ci.nsIStyleSheetService.USER_SHEET, this);
});

defineLazyGetter(
  BrowserExtensionContent.prototype,
  "authorCSSCode",
  function () {
    return new CSSCodeCache(Ci.nsIStyleSheetService.AUTHOR_SHEET, this);
  }
);

// Represents a content script.
class Script {
  /**
   * @param {BrowserExtensionContent} extension
   * @param {WebExtensionContentScript|object} matcher
   *        An object with a "matchesWindowGlobal" method and content script
   *        execution details. This is usually a plain WebExtensionContentScript
   *        except when the script is run via `tabs.executeScript`. In this
   *        case, the object may have some extra properties:
   *        wantReturnValue, removeCSS, cssOrigin, jsCode
   */
  constructor(extension, matcher) {
    this.scriptType = "content_script";
    this.extension = extension;
    this.matcher = matcher;

    this.runAt = this.matcher.runAt;
    this.js = this.matcher.jsPaths;
    this.css = this.matcher.cssPaths.slice();
    this.cssCodeHash = null;

    this.removeCSS = this.matcher.removeCSS;
    this.cssOrigin = this.matcher.cssOrigin;

    this.cssCache =
      extension[this.cssOrigin === "user" ? "userCSS" : "authorCSS"];
    this.cssCodeCache =
      extension[this.cssOrigin === "user" ? "userCSSCode" : "authorCSSCode"];
    this.scriptCache =
      extension[matcher.wantReturnValue ? "dynamicScripts" : "staticScripts"];

    /** @type {WeakSet<Document>} A set of documents injected into. */
    this.injectedInto = new WeakSet();

    if (matcher.wantReturnValue) {
      this.compileScripts();
      this.loadCSS();
    }
  }

  get requiresCleanup() {
    return !this.removeCSS && (!!this.css.length || this.cssCodeHash);
  }

  async addCSSCode(cssCode) {
    if (!cssCode) {
      return;
    }

    // Store the hash of the cssCode.
    const buffer = await crypto.subtle.digest(
      "SHA-1",
      new TextEncoder().encode(cssCode)
    );
    this.cssCodeHash = String.fromCharCode(...new Uint16Array(buffer));

    // Cache and preload the cssCode stylesheet.
    this.cssCodeCache.addCSSCode(this.cssCodeHash, cssCode);
  }

  compileScripts() {
    return this.js.map(url => this.scriptCache.get(url));
  }

  loadCSS() {
    return this.css.map(url => this.cssCache.get(url));
  }

  preload() {
    this.loadCSS();
    this.compileScripts();
  }

  cleanup(window) {
    if (this.requiresCleanup) {
      if (window) {
        let { windowUtils } = window;

        let type =
          this.cssOrigin === "user"
            ? windowUtils.USER_SHEET
            : windowUtils.AUTHOR_SHEET;

        for (let url of this.css) {
          this.cssCache.deleteDocument(url, window.document);

          if (!window.closed) {
            runSafeSyncWithoutClone(
              windowUtils.removeSheetUsingURIString,
              url,
              type
            );
          }
        }

        const { cssCodeHash } = this;

        if (cssCodeHash && this.cssCodeCache.has(cssCodeHash)) {
          if (!window.closed) {
            this.cssCodeCache.get(cssCodeHash).then(({ uri }) => {
              runSafeSyncWithoutClone(windowUtils.removeSheet, uri, type);
            });
          }
          this.cssCodeCache.deleteDocument(cssCodeHash, window.document);
        }
      }

      // Clear any sheets that were kept alive past their timeout as
      // a result of living in this document.
      this.cssCodeCache.clear(CSSCODE_EXPIRY_TIMEOUT_MS);
      this.cssCache.clear(CSS_EXPIRY_TIMEOUT_MS);
    }
  }

  matchesWindowGlobal(windowGlobal, ignorePermissions) {
    return this.matcher.matchesWindowGlobal(windowGlobal, ignorePermissions);
  }

  async injectInto(window, reportExceptions = true) {
    if (
      !lazy.isContentScriptProcess ||
      this.injectedInto.has(window.document)
    ) {
      return;
    }
    this.injectedInto.add(window.document);

    let context = this.extension.getContext(window);
    for (let script of this.matcher.jsPaths) {
      context.logActivity(this.scriptType, script, {
        url: window.location.href,
      });
    }

    try {
      if (this.runAt === "document_end") {
        await promiseDocumentReady(window.document);
      } else if (this.runAt === "document_idle") {
        await Promise.race([
          promiseDocumentIdle(window),
          promiseDocumentLoaded(window.document),
        ]);
      }

      return this.inject(context, reportExceptions);
    } catch (e) {
      return Promise.reject(context.normalizeError(e));
    }
  }

  /**
   * Tries to inject this script into the given window and sandbox, if
   * there are pending operations for the window's current load state.
   *
   * @param {BaseContext} context
   *        The content script context into which to inject the scripts.
   * @param {boolean} reportExceptions
   *        Defaults to true and reports any exception directly to the console
   *        and no exception will be thrown out of this function.
   * @returns {Promise<any>}
   *        Resolves to the last value in the evaluated script, when
   *        execution is complete.
   */
  async inject(context, reportExceptions = true) {
    DocumentManager.lazyInit();
    if (this.requiresCleanup) {
      context.addScript(this);
    }

    const { cssCodeHash } = this;

    let cssPromise;
    if (this.css.length || cssCodeHash) {
      let window = context.contentWindow;
      let { windowUtils } = window;

      let type =
        this.cssOrigin === "user"
          ? windowUtils.USER_SHEET
          : windowUtils.AUTHOR_SHEET;

      if (this.removeCSS) {
        for (let url of this.css) {
          this.cssCache.deleteDocument(url, window.document);

          runSafeSyncWithoutClone(
            windowUtils.removeSheetUsingURIString,
            url,
            type
          );
        }

        if (cssCodeHash && this.cssCodeCache.has(cssCodeHash)) {
          const { uri } = await this.cssCodeCache.get(cssCodeHash);
          this.cssCodeCache.deleteDocument(cssCodeHash, window.document);

          runSafeSyncWithoutClone(windowUtils.removeSheet, uri, type);
        }
      } else {
        cssPromise = Promise.all(this.loadCSS()).then(sheets => {
          let window = context.contentWindow;
          if (!window) {
            return;
          }

          for (let { url, sheet } of sheets) {
            this.cssCache.addDocument(url, window.document);

            runSafeSyncWithoutClone(windowUtils.addSheet, sheet, type);
          }
        });

        if (cssCodeHash) {
          cssPromise = cssPromise.then(async () => {
            const { sheet } = await this.cssCodeCache.get(cssCodeHash);
            this.cssCodeCache.addDocument(cssCodeHash, window.document);

            runSafeSyncWithoutClone(windowUtils.addSheet, sheet, type);
          });
        }

        // We're loading stylesheets via the stylesheet service, which means
        // that the normal mechanism for blocking layout and onload for pending
        // stylesheets aren't in effect (since there's no document to block). So
        // we need to do something custom here, similar to what we do for
        // scripts. Blocking parsing is overkill, since we really just want to
        // block layout and onload. But we have an API to do the former and not
        // the latter, so we do it that way. This hopefully isn't a performance
        // problem since there are no network loads involved, and since we cache
        // the stylesheets on first load. We should fix this up if it does becomes
        // a problem.
        if (this.css.length) {
          context.contentWindow.document.blockParsing(cssPromise, {
            blockScriptCreated: false,
          });
        }
      }
    }

    let scripts = this.getCompiledScripts(context);
    if (scripts instanceof Promise) {
      scripts = await scripts;
    }

    // Make sure we've injected any related CSS before we run content scripts.
    await cssPromise;

    let result;

    const { extension } = context;

    // The evaluations below may throw, in which case the promise will be
    // automatically rejected.
    lazy.ExtensionTelemetry.contentScriptInjection.stopwatchStart(
      extension,
      context
    );
    try {
      for (let script of scripts) {
        result = script.executeInGlobal(context.cloneScope, {
          reportExceptions,
        });
      }

      if (this.matcher.jsCode) {
        result = Cu.evalInSandbox(
          this.matcher.jsCode,
          context.cloneScope,
          "latest",
          "sandbox eval code",
          1
        );
      }
    } finally {
      lazy.ExtensionTelemetry.contentScriptInjection.stopwatchFinish(
        extension,
        context
      );
    }

    return result;
  }

  /**
   *  Get the compiled scripts (if they are already precompiled and cached) or a promise which resolves
   *  to the precompiled scripts (once they have been compiled and cached).
   *
   * @param {BaseContext} context
   *        The document to block the parsing on, if the scripts are not yet precompiled and cached.
   *
   * @returns {Array<PreloadedScript> | Promise<Array<PreloadedScript>>}
   *          Returns an array of preloaded scripts if they are already available, or a promise which
   *          resolves to the array of the preloaded scripts once they are precompiled and cached.
   */
  getCompiledScripts(context) {
    let scriptPromises = this.compileScripts();
    let scripts = scriptPromises.map(promise => promise.script);

    // If not all scripts are already available in the cache, block
    // parsing and wait all promises to resolve.
    if (!scripts.every(script => script)) {
      let promise = Promise.all(scriptPromises);

      // If there is any syntax error, the script promises will be rejected.
      //
      // Notify the exception directly to the console so that it can
      // be displayed in the web console by flagging the error with the right
      // innerWindowID.
      for (const p of scriptPromises) {
        p.catch(error => {
          Services.console.logMessage(
            new ScriptError(
              `${error.name}: ${error.message}`,
              error.fileName,
              null,
              error.lineNumber,
              error.columnNumber,
              Ci.nsIScriptError.errorFlag,
              "content javascript",
              context.innerWindowID
            )
          );
        });
      }

      // If we're supposed to inject at the start of the document load,
      // and we haven't already missed that point, block further parsing
      // until the scripts have been loaded.
      const { document } = context.contentWindow;
      if (
        this.runAt === "document_start" &&
        document.readyState !== "complete"
      ) {
        document.blockParsing(promise, { blockScriptCreated: false });
      }

      return promise;
    }

    return scripts;
  }
}

// Represents a user script.
class UserScript extends Script {
  /**
   * @param {BrowserExtensionContent} extension
   * @param {WebExtensionContentScript|object} matcher
   *        An object with a "matchesWindowGlobal" method and content script
   *        execution details.
   */
  constructor(extension, matcher) {
    super(extension, matcher);
    this.scriptType = "user_script";

    // This is an opaque object that the extension provides, it is associated to
    // the particular userScript and it is passed as a parameter to the custom
    // userScripts APIs defined by the extension.
    this.scriptMetadata = matcher.userScriptOptions.scriptMetadata;
    this.apiScriptURL =
      extension.manifest.user_scripts &&
      extension.manifest.user_scripts.api_script;

    // Add the apiScript to the js scripts to compile.
    if (this.apiScriptURL) {
      this.js = [this.apiScriptURL].concat(this.js);
    }

    // WeakMap<ContentScriptContextChild, Sandbox>
    this.sandboxes = new DefaultWeakMap(context => {
      return this.createSandbox(context);
    });
  }

  async inject(context) {
    DocumentManager.lazyInit();

    let scripts = this.getCompiledScripts(context);
    if (scripts instanceof Promise) {
      scripts = await scripts;
    }

    let apiScript, sandboxScripts;

    if (this.apiScriptURL) {
      [apiScript, ...sandboxScripts] = scripts;
    } else {
      sandboxScripts = scripts;
    }

    // Load and execute the API script once per context.
    if (apiScript) {
      context.executeAPIScript(apiScript);
    }

    let userScriptSandbox = this.sandboxes.get(context);

    context.callOnClose({
      close: () => {
        // Destroy the userScript sandbox when the related ContentScriptContextChild instance
        // is being closed.
        this.sandboxes.delete(context);
        Cu.nukeSandbox(userScriptSandbox);
      },
    });

    // Notify listeners subscribed to the userScripts.onBeforeScript API event,
    // to allow extension API script to provide its custom APIs to the userScript.
    if (apiScript) {
      context.userScriptsEvents.emit(
        "on-before-script",
        this.scriptMetadata,
        userScriptSandbox
      );
    }

    for (let script of sandboxScripts) {
      script.executeInGlobal(userScriptSandbox);
    }
  }

  createSandbox(context) {
    const { contentWindow } = context;
    const contentPrincipal = contentWindow.document.nodePrincipal;
    const ssm = Services.scriptSecurityManager;

    let principal;
    if (contentPrincipal.isSystemPrincipal) {
      principal = ssm.createNullPrincipal(contentPrincipal.originAttributes);
    } else {
      principal = [contentPrincipal];
    }

    const sandbox = Cu.Sandbox(principal, {
      sandboxName: `User Script registered by ${this.extension.policy.debugName}`,
      sandboxPrototype: contentWindow,
      sameZoneAs: contentWindow,
      wantXrays: true,
      wantGlobalProperties: ["XMLHttpRequest", "fetch", "WebSocket"],
      originAttributes: contentPrincipal.originAttributes,
      metadata: {
        "inner-window-id": context.innerWindowID,
        addonId: this.extension.policy.id,
      },
    });

    return sandbox;
  }
}

var contentScripts = new DefaultWeakMap(matcher => {
  const extension = lazy.ExtensionProcessScript.extensions.get(
    matcher.extension
  );

  if ("userScriptOptions" in matcher) {
    return new UserScript(extension, matcher);
  }

  return new Script(extension, matcher);
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

    let frameId = lazy.WebNavigationFrames.getFrameId(contentWindow);
    this.frameId = frameId;

    this.browsingContextId = contentWindow.docShell.browsingContext.id;

    this.scripts = [];

    let contentPrincipal = contentWindow.document.nodePrincipal;
    let ssm = Services.scriptSecurityManager;

    // Copy origin attributes from the content window origin attributes to
    // preserve the user context id.
    let attrs = contentPrincipal.originAttributes;
    let extensionPrincipal = ssm.createContentPrincipal(
      this.extension.baseURI,
      attrs
    );

    this.isExtensionPage = contentPrincipal.equals(extensionPrincipal);

    if (this.isExtensionPage) {
      // This is an iframe with content script API enabled and its principal
      // should be the contentWindow itself. We create a sandbox with the
      // contentWindow as principal and with X-rays disabled because it
      // enables us to create the APIs object in this sandbox object and then
      // copying it into the iframe's window.  See bug 1214658.
      this.sandbox = Cu.Sandbox(contentWindow, {
        sandboxName: `Web-Accessible Extension Page ${extension.policy.debugName}`,
        sandboxPrototype: contentWindow,
        sameZoneAs: contentWindow,
        wantXrays: false,
        isWebExtensionContentScript: true,
      });
    } else {
      let principal;
      if (contentPrincipal.isSystemPrincipal) {
        // Make sure we don't hand out the system principal by accident.
        // Also make sure that the null principal has the right origin attributes.
        principal = ssm.createNullPrincipal(attrs);
      } else {
        principal = [contentPrincipal, extensionPrincipal];
      }
      // This metadata is required by the Developer Tools, in order for
      // the content script to be associated with both the extension and
      // the tab holding the content page.
      let metadata = {
        "inner-window-id": this.innerWindowID,
        addonId: extensionPrincipal.addonId,
      };

      let isMV2 = extension.manifestVersion == 2;
      let wantGlobalProperties;
      if (isMV2) {
        // In MV2, fetch/XHR support cross-origin requests.
        // WebSocket was also included to avoid CSP effects (bug 1676024).
        wantGlobalProperties = ["XMLHttpRequest", "fetch", "WebSocket"];
      } else {
        // In MV3, fetch/XHR have the same capabilities as the web page.
        wantGlobalProperties = [];
      }
      this.sandbox = Cu.Sandbox(principal, {
        metadata,
        sandboxName: `Content Script ${extension.policy.debugName}`,
        sandboxPrototype: contentWindow,
        sameZoneAs: contentWindow,
        wantXrays: true,
        isWebExtensionContentScript: true,
        wantExportHelpers: true,
        wantGlobalProperties,
        originAttributes: attrs,
      });

      // Preserve a copy of the original Error and Promise globals from the sandbox object,
      // which are used in the WebExtensions internals (before any content script code had
      // any chance to redefine them).
      this.cloneScopePromise = this.sandbox.Promise;
      this.cloneScopeError = this.sandbox.Error;

      if (isMV2) {
        // Preserve a copy of the original window's XMLHttpRequest and fetch
        // in a content object (fetch is manually binded to the window
        // to prevent it from raising a TypeError because content object is not
        // a real window).
        Cu.evalInSandbox(
          `
          this.content = {
            XMLHttpRequest: window.XMLHttpRequest,
            fetch: window.fetch.bind(window),
            WebSocket: window.WebSocket,
          };

          window.JSON = JSON;
          window.XMLHttpRequest = XMLHttpRequest;
          window.fetch = fetch;
          window.WebSocket = WebSocket;
        `,
          this.sandbox
        );
      } else {
        // The sandbox's JSON API can deal with values from the sandbox and the
        // contentWindow, but window.JSON cannot (and it could potentially be
        // spoofed by the web page). jQuery.parseJSON relies on window.JSON.
        Cu.evalInSandbox("window.JSON = JSON;", this.sandbox);
      }
    }

    Object.defineProperty(this, "principal", {
      value: Cu.getObjectPrincipal(this.sandbox),
      enumerable: true,
      configurable: true,
    });

    this.url = contentWindow.location.href;

    defineLazyGetter(this, "chromeObj", () => {
      let chromeObj = Cu.createObjectIn(this.sandbox);

      this.childManager.inject(chromeObj);
      return chromeObj;
    });

    lazy.Schemas.exportLazyGetter(
      this.sandbox,
      "browser",
      () => this.chromeObj
    );
    lazy.Schemas.exportLazyGetter(this.sandbox, "chrome", () => this.chromeObj);

    // Keep track if the userScript API script has been already executed in this context
    // (e.g. because there are more then one UserScripts that match the related webpage
    // and so the UserScript apiScript has already been executed).
    this.hasUserScriptAPIs = false;

    // A lazy created EventEmitter related to userScripts-specific events.
    defineLazyGetter(this, "userScriptsEvents", () => {
      return new ExtensionCommon.EventEmitter();
    });
  }

  injectAPI() {
    if (!this.isExtensionPage) {
      throw new Error("Cannot inject extension API into non-extension window");
    }

    // This is an iframe with content script API enabled (See Bug 1214658)
    lazy.Schemas.exportLazyGetter(
      this.contentWindow,
      "browser",
      () => this.chromeObj
    );
    lazy.Schemas.exportLazyGetter(
      this.contentWindow,
      "chrome",
      () => this.chromeObj
    );
  }

  async logActivity(type, name, data) {
    ExtensionActivityLogChild.log(this, type, name, data);
  }

  get cloneScope() {
    return this.sandbox;
  }

  async executeAPIScript(apiScript) {
    // Execute the UserScript apiScript only once per context (e.g. more then one UserScripts
    // match the same webpage and the apiScript has already been executed).
    if (apiScript && !this.hasUserScriptAPIs) {
      this.hasUserScriptAPIs = true;
      apiScript.executeInGlobal(this.cloneScope);
    }
  }

  addScript(script) {
    if (script.requiresCleanup) {
      this.scripts.push(script);
    }
  }

  close() {
    super.unload();

    // Cleanup the scripts even if the contentWindow have been destroyed.
    for (let script of this.scripts) {
      script.cleanup(this.contentWindow);
    }

    if (this.contentWindow) {
      // Overwrite the content script APIs with an empty object if the APIs objects are still
      // defined in the content window (See Bug 1214658).
      if (this.isExtensionPage) {
        Cu.createObjectIn(this.contentWindow, { defineAs: "browser" });
        Cu.createObjectIn(this.contentWindow, { defineAs: "chrome" });
      }
    }
    Cu.nukeSandbox(this.sandbox);

    this.sandbox = null;
  }
}

defineLazyGetter(ContentScriptContextChild.prototype, "messenger", function () {
  return new Messenger(this);
});

defineLazyGetter(
  ContentScriptContextChild.prototype,
  "childManager",
  function () {
    apiManager.lazyInit();

    let localApis = {};
    let can = new CanOfAPIs(this, apiManager, localApis);

    let childManager = new ChildAPIManager(this, this.messageManager, can, {
      envType: "content_parent",
      url: this.url,
    });

    this.callOnClose(childManager);

    return childManager;
  }
);

// Responsible for creating ExtensionContexts and injecting content
// scripts into them when new documents are created.
DocumentManager = {
  // Map[windowId -> Map[ExtensionChild -> ContentScriptContextChild]]
  contexts: new Map(),

  initialized: false,

  lazyInit() {
    if (this.initialized) {
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

      for (let cache of ChromeUtils.nondeterministicGetWeakSetKeys(
        scriptCaches
      )) {
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

export var ExtensionContent = {
  BrowserExtensionContent,

  contentScripts,

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

  // For test use only.
  getContextByExtensionId(extensionId, window) {
    return DocumentManager.getContext(extensionId, window);
  },

  async handleDetectLanguage({ windows }) {
    let wgc = WindowGlobalChild.getByInnerWindowId(windows[0]);
    let doc = wgc.browsingContext.window.document;
    await promiseDocumentReady(doc);

    // The CLD2 library can analyze HTML, but that uses more memory, and
    // emscripten can't shrink its heap, so we use plain text instead.
    let encoder = Cu.createDocumentEncoder("text/plain");
    encoder.init(doc, "text/plain", Ci.nsIDocumentEncoder.SkipInvisibleContent);

    let result = await lazy.LanguageDetector.detectLanguage({
      language:
        doc.documentElement.getAttribute("xml:lang") ||
        doc.documentElement.getAttribute("lang") ||
        doc.contentLanguage ||
        null,
      tld: doc.location.hostname.match(/[a-z]*$/)[0],
      text: encoder.encodeToStringWithMaxLength(60 * 1024),
      encoding: doc.characterSet,
    });
    return result.language === "un" ? "und" : result.language;
  },

  // Activate MV3 content scripts in all same-origin frames for this tab.
  handleActivateScripts({ options, windows }) {
    let policy = WebExtensionPolicy.getByID(options.id);

    // Order content scripts by run_at timing.
    let runAt = { document_start: [], document_end: [], document_idle: [] };
    for (let matcher of policy.contentScripts) {
      runAt[matcher.runAt].push(this.contentScripts.get(matcher));
    }

    // If we got here, checks in TabManagerBase.activateScripts assert:
    // 1) this is a MV3 extension, with Origin Controls,
    // 2) with a host permission (or content script) for the tab's top origin,
    // 3) and that host permission hasn't been granted yet.

    // We treat the action click as implicit user's choice to activate the
    // extension on the current site, so we can safely run (matching) content
    // scripts in all sameOriginWithTop frames while ignoring host permission.

    let { browsingContext } = WindowGlobalChild.getByInnerWindowId(windows[0]);
    for (let bc of browsingContext.getAllBrowsingContextsInSubtree()) {
      let wgc = bc.currentWindowContext.windowGlobalChild;
      if (wgc?.sameOriginWithTop) {
        // This is TOCTOU safe: if a frame navigated after same-origin check,
        // wgc.isClosed would be true and .matchesWindowGlobal() would fail.
        const runScript = cs => {
          if (cs.matchesWindowGlobal(wgc, /* ignorePermissions */ true)) {
            return cs.injectInto(bc.window);
          }
        };

        // Inject all matching content scripts in proper run_at order.
        Promise.all(runAt.document_start.map(runScript))
          .then(() => Promise.all(runAt.document_end.map(runScript)))
          .then(() => Promise.all(runAt.document_idle.map(runScript)));
      }
    }
  },

  // Used to executeScript, insertCSS and removeCSS.
  async handleActorExecute({ options, windows }) {
    let policy = WebExtensionPolicy.getByID(options.extensionId);
    // `WebExtensionContentScript` uses `MozDocumentMatcher::Matches` to ensure
    // that a script can be run in a document. That requires either `frameId`
    // or `allFrames` to be set. When `frameIds` (plural) is used, we force
    // `allFrames` to be `true` in order to match any frame. This is OK because
    // `executeInWin()` below looks up the window for the given `frameIds`
    // immediately before `script.injectInto()`. Due to this, we won't run
    // scripts in windows with non-matching `frameId`, despite `allFrames`
    // being set to `true`.
    if (options.frameIds) {
      options.allFrames = true;
    }
    let matcher = new WebExtensionContentScript(policy, options);

    Object.assign(matcher, {
      wantReturnValue: options.wantReturnValue,
      removeCSS: options.removeCSS,
      cssOrigin: options.cssOrigin,
      jsCode: options.jsCode,
    });
    let script = contentScripts.get(matcher);

    // Add the cssCode to the script, so that it can be converted into a cached URL.
    await script.addCSSCode(options.cssCode);
    delete options.cssCode;

    const executeInWin = innerId => {
      let wg = WindowGlobalChild.getByInnerWindowId(innerId);
      if (wg?.isCurrentGlobal && script.matchesWindowGlobal(wg)) {
        let bc = wg.browsingContext;

        return {
          frameId: bc.parent ? bc.id : 0,
          // Disable exception reporting directly to the console
          // in order to pass the exceptions back to the callsite.
          promise: script.injectInto(bc.window, false),
        };
      }
    };

    let promisesWithFrameIds = windows.map(executeInWin).filter(obj => obj);

    let result = await Promise.all(
      promisesWithFrameIds.map(async ({ frameId, promise }) => {
        if (!options.returnResultsWithFrameIds) {
          return promise;
        }

        try {
          const result = await promise;

          return { frameId, result };
        } catch (error) {
          return { frameId, error };
        }
      })
    ).catch(
      // This is useful when we do not return results/errors with frame IDs in
      // the promises above.
      e => Promise.reject({ message: e.message })
    );

    try {
      // Check if the result can be structured-cloned before sending back.
      return Cu.cloneInto(result, this);
    } catch (e) {
      let path = options.jsPaths.slice(-1)[0] ?? "<anonymous code>";
      let message = `Script '${path}' result is non-structured-clonable data`;
      return Promise.reject({ message, fileName: path });
    }
  },
};

/**
 * Child side of the ExtensionContent process actor, handles some tabs.* APIs.
 */
export class ExtensionContentChild extends JSProcessActorChild {
  receiveMessage({ name, data }) {
    if (!lazy.isContentScriptProcess) {
      return;
    }
    switch (name) {
      case "DetectLanguage":
        return ExtensionContent.handleDetectLanguage(data);
      case "Execute":
        return ExtensionContent.handleActorExecute(data);
      case "ActivateScripts":
        return ExtensionContent.handleActivateScripts(data);
    }
  }
}
