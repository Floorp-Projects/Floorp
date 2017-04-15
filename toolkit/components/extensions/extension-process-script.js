/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This script contains the minimum, skeleton content process code that we need
 * in order to lazily load other extension modules when they are first
 * necessary. Anything which is not likely to be needed immediately, or shortly
 * after startup, in *every* browser process live outside of this file.
 */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionManagement",
                                  "resource://gre/modules/ExtensionManagement.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MatchGlobs",
                                  "resource://gre/modules/MatchPattern.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MatchPattern",
                                  "resource://gre/modules/MatchPattern.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MessageChannel",
                                  "resource://gre/modules/MessageChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebNavigationFrames",
                                  "resource://gre/modules/WebNavigationFrames.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionChild",
                                  "resource://gre/modules/ExtensionChild.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionContent",
                                  "resource://gre/modules/ExtensionContent.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionPageChild",
                                  "resource://gre/modules/ExtensionPageChild.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionUtils",
                                  "resource://gre/modules/ExtensionUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "console", () => ExtensionUtils.getConsole());
XPCOMUtils.defineLazyGetter(this, "getInnerWindowID", () => ExtensionUtils.getInnerWindowID);

const isContentProcess = Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;


class ScriptMatcher {
  constructor(extension, options) {
    this.extension = extension;
    this.options = options;

    this._script = null;

    this.allFrames = options.all_frames;
    this.matchAboutBlank = options.match_about_blank;
    this.frameId = options.frame_id;
    this.runAt = options.run_at;

    this.matches = new MatchPattern(options.matches);
    this.excludeMatches = new MatchPattern(options.exclude_matches || null);
    // TODO: MatchPattern should pre-mangle host-only patterns so that we
    // don't need to call a separate match function.
    this.matchesHost = new MatchPattern(options.matchesHost || null);
    this.includeGlobs = options.include_globs && new MatchGlobs(options.include_globs);
    this.excludeGlobs = new MatchGlobs(options.exclude_globs);
  }

  toString() {
    return `[Script {js: [${this.options.js}], matchAboutBlank: ${this.matchAboutBlank}, runAt: ${this.runAt}, matches: ${this.options.matches}}]`;
  }

  get script() {
    if (!this._script) {
      this._script = new ExtensionContent.Script(this.extension.realExtension,
                                                 this.options);
    }
    return this._script;
  }

  preload() {
    let {script} = this;

    script.loadCSS();
    script.compileScripts();
  }

  matchesLoadInfo(uri, loadInfo) {
    if (!this.matchesURI(uri)) {
      return false;
    }

    if (!this.allFrames && !loadInfo.isTopLevelLoad) {
      return false;
    }

    return true;
  }

  matchesURI(uri) {
    if (!(this.matches.matches(uri) || this.matchesHost.matchesIgnoringPath(uri))) {
      return false;
    }

    if (this.excludeMatches.matches(uri)) {
      return false;
    }

    if (this.includeGlobs != null && !this.includeGlobs.matches(uri.spec)) {
      return false;
    }

    if (this.excludeGlobs.matches(uri.spec)) {
      return false;
    }

    return true;
  }

  matchesWindow(window) {
    if (!this.allFrames && this.frameId == null && window.parent !== window) {
      return false;
    }

    let uri = window.document.documentURIObject;
    let principal = window.document.nodePrincipal;

    if (this.matchAboutBlank) {
      // When matching top-level about:blank documents,
      // allow loading into any with a NullPrincipal.
      if (uri.spec === "about:blank" && window === window.parent && principal.isNullPrincipal) {
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
      if (!this.matchAboutBlank) {
        return false;
      }
      uri = principal.URI;
    }

    if (!this.matchesURI(uri)) {
      return false;
    }

    if (this.frameId != null && WebNavigationFrames.getFrameId(window) !== this.frameId) {
      return false;
    }

    // If mozAddonManager is present on this page, don't allow
    // content scripts.
    if (window.navigator.mozAddonManager !== undefined) {
      return false;
    }

    return true;
  }

  injectInto(window) {
    return this.script.injectInto(window);
  }
}

function getMessageManager(contentWindow) {
  let docShell = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDocShell)
                              .QueryInterface(Ci.nsIInterfaceRequestor);
  try {
    return docShell.getInterface(Ci.nsIContentFrameMessageManager);
  } catch (e) {
    // Some windows don't support this interface (hidden window).
    return null;
  }
}

var DocumentManager;
var ExtensionManager;

class ExtensionGlobal {
  constructor(global) {
    this.global = global;

    MessageChannel.addListener(global, "Extension:Capture", this);
    MessageChannel.addListener(global, "Extension:DetectLanguage", this);
    MessageChannel.addListener(global, "Extension:Execute", this);
    MessageChannel.addListener(global, "WebNavigation:GetFrame", this);
    MessageChannel.addListener(global, "WebNavigation:GetAllFrames", this);
  }

  uninit() {
  }

  get messageFilterStrict() {
    return {
      innerWindowID: getInnerWindowID(this.global.content),
    };
  }

  receiveMessage({target, messageName, recipient, data}) {
    switch (messageName) {
      case "Extension:Capture":
        return ExtensionContent.handleExtensionCapture(this.global, data.width, data.height, data.options);
      case "Extension:DetectLanguage":
        return ExtensionContent.handleDetectLanguage(this.global, target);
      case "Extension:Execute":
        let extension = ExtensionManager.get(recipient.extensionId);
        let script = new ScriptMatcher(extension, data.options);

        return ExtensionContent.handleExtensionExecute(this.global, target, data.options, script);
      case "WebNavigation:GetFrame":
        return ExtensionContent.handleWebNavigationGetFrame(this.global, data.options);
      case "WebNavigation:GetAllFrames":
        return ExtensionContent.handleWebNavigationGetAllFrames(this.global);
    }
  }
}

// Responsible for creating ExtensionContexts and injecting content
// scripts into them when new documents are created.
DocumentManager = {
  globals: new Map(),

  // Initialize listeners that we need regardless of whether extensions are
  // enabled.
  earlyInit() {
    Services.obs.addObserver(this, "tab-content-frameloader-created"); // eslint-disable-line mozilla/balanced-listeners
  },

  // Initialize listeners that we need when any extension is enabled.
  init() {
    Services.obs.addObserver(this, "document-element-inserted");
  },
  uninit() {
    Services.obs.removeObserver(this, "document-element-inserted");
  },

  // Initialize listeners that we need when any extension content script is
  // enabled.
  initMatchers() {
    if (isContentProcess) {
      Services.obs.addObserver(this, "http-on-opening-request");
    }
  },
  uninitMatchers() {
    if (isContentProcess) {
      Services.obs.removeObserver(this, "http-on-opening-request");
    }
  },

  // Initialize listeners that we need when any about:blank content script is
  // enabled.
  //
  // Loads of about:blank are special, and do not trigger "document-element-inserted"
  // observers. So if we have any scripts that match about:blank, we also need
  // to observe "content-document-global-created".
  initAboutBlankMatchers() {
    Services.obs.addObserver(this, "content-document-global-created");
  },
  uninitAboutBlankMatchers() {
    Services.obs.removeObserver(this, "content-document-global-created");
  },

  extensionProcessInitialized: false,
  initExtensionProcess() {
    if (this.extensionProcessInitialized || !ExtensionManagement.isExtensionProcess) {
      return;
    }
    this.extensionProcessInitialized = true;

    for (let global of this.globals.keys()) {
      ExtensionPageChild.init(global);
    }
  },

  // Initialize a frame script global which extension contexts may be loaded
  // into.
  initGlobal(global) {
    // Note: {once: true} does not work as expected here.
    global.addEventListener("unload", event => { // eslint-disable-line mozilla/balanced-listeners
      this.uninitGlobal(global);
    });

    this.globals.set(global, new ExtensionGlobal(global));
    this.initExtensionProcess();
    if (this.extensionProcessInitialized && ExtensionManagement.isExtensionProcess) {
      ExtensionPageChild.init(global);
    }
  },
  uninitGlobal(global) {
    if (this.extensionProcessInitialized) {
      ExtensionPageChild.uninit(global);
    }
    this.globals.get(global).uninit();
    this.globals.delete(global);
  },

  initExtension(extension) {
    if (this.extensionCount === 0) {
      this.init();
      this.initExtensionProcess();
    }
    this.extensionCount++;

    for (let script of extension.scripts) {
      this.addContentScript(script);
    }

    this.injectExtensionScripts(extension);
  },
  uninitExtension(extension) {
    for (let script of extension.scripts) {
      this.removeContentScript(script);
    }

    this.extensionCount--;
    if (this.extensionCount === 0) {
      this.uninit();
    }
  },


  extensionCount: 0,
  matchAboutBlankCount: 0,

  contentScripts: new Set(),

  addContentScript(script) {
    if (this.contentScripts.size == 0) {
      this.initMatchers();
    }

    if (script.matchAboutBlank) {
      if (this.matchAboutBlankCount == 0) {
        this.initAboutBlankMatchers();
      }
      this.matchAboutBlankCount++;
    }

    this.contentScripts.add(script);
  },
  removeContentScript(script) {
    this.contentScripts.delete(script);

    if (this.contentScripts.size == 0) {
      this.uninitMatchers();
    }

    if (script.matchAboutBlank) {
      this.matchAboutBlankCount--;
      if (this.matchAboutBlankCount == 0) {
        this.uninitAboutBlankMatchers();
      }
    }
  },

  // Listeners

  observers: {
    async "content-document-global-created"(window) {
      // We only care about about:blank here, since it doesn't trigger
      // "document-element-inserted".
      if ((window.location && window.location.href !== "about:blank") ||
          // Make sure we only load into frames that belong to tabs, or other
          // special areas that we want to load content scripts into.
          !this.globals.has(getMessageManager(window))) {
        return;
      }

      // We can't tell for certain whether the final document will actually be
      // about:blank at this point, though, so wait for the DOM to finish
      // loading and check again before injecting scripts.
      await new Promise(resolve => window.addEventListener(
        "DOMContentLoaded", resolve, {once: true, capture: true}));

      if (window.location.href === "about:blank") {
        this.injectWindowScripts(window);
      }
    },

    "document-element-inserted"(document) {
      let window = document.defaultView;
      if (!document.location || !window ||
          // Make sure we only load into frames that belong to tabs, or other
          // special areas that we want to load content scripts into.
          !this.globals.has(getMessageManager(window))) {
        return;
      }

      this.injectWindowScripts(window);
      this.loadInto(window);
    },

    "http-on-opening-request"(subject, topic, data) {
      // If this request is a docshell load, check whether any of our scripts
      // are likely to be loaded into it, and begin preloading the ones that
      // are.
      let {loadInfo} = subject.QueryInterface(Ci.nsIChannel);
      if (loadInfo) {
        let {externalContentPolicyType: type} = loadInfo;
        if (type === Ci.nsIContentPolicy.TYPE_DOCUMENT ||
            type === Ci.nsIContentPolicy.TYPE_SUBDOCUMENT) {
          this.preloadScripts(subject.URI, loadInfo);
        }
      }
    },

    "tab-content-frameloader-created"(global) {
      this.initGlobal(global);
    },
  },

  observe(subject, topic, data) {
    this.observers[topic].call(this, subject, topic, data);
  },

  // Script loading

  injectExtensionScripts(extension) {
    for (let window of this.enumerateWindows()) {
      for (let script of extension.scripts) {
        if (script.matchesWindow(window)) {
          script.injectInto(window);
        }
      }
    }
  },

  injectWindowScripts(window) {
    for (let script of this.contentScripts) {
      if (script.matchesWindow(window)) {
        script.injectInto(window);
      }
    }
  },

  preloadScripts(uri, loadInfo) {
    for (let script of this.contentScripts) {
      if (script.matchesLoadInfo(uri, loadInfo)) {
        script.preload();
      }
    }
  },

  loadInto(window) {
    let extensionId = ExtensionManagement.getAddonIdForWindow(window);
    if (!extensionId) {
      return;
    }

    let extension = ExtensionManager.get(extensionId);
    if (!extension) {
      throw new Error(`No registered extension for ID ${extensionId}`);
    }

    let apiLevel = ExtensionManagement.getAPILevelForWindow(window, extensionId);
    const levels = ExtensionManagement.API_LEVELS;

    if (apiLevel === levels.CONTENTSCRIPT_PRIVILEGES) {
      ExtensionContent.initExtensionContext(extension.realExtension, window);
    } else if (apiLevel === levels.FULL_PRIVILEGES) {
      ExtensionPageChild.initExtensionContext(extension.realExtension, window);
    } else {
      throw new Error(`Unexpected window with extension ID ${extensionId}`);
    }
  },

  // Helpers

  * enumerateWindows(docShell) {
    if (docShell) {
      let enum_ = docShell.getDocShellEnumerator(docShell.typeContent,
                                                 docShell.ENUMERATE_FORWARDS);

      for (let docShell of XPCOMUtils.IterSimpleEnumerator(enum_, Ci.nsIInterfaceRequestor)) {
        yield docShell.getInterface(Ci.nsIDOMWindow);
      }
    } else {
      for (let global of this.globals.keys()) {
        yield* this.enumerateWindows(global.docShell);
      }
    }
  },
};

/**
 * This class is a minimal stub extension object which loads and instantiates a
 * real extension object when non-basic functionality is needed.
 */
class StubExtension {
  constructor(data) {
    this.data = data;
    this.id = data.id;
    this.uuid = data.uuid;
    this.instanceId = data.instanceId;
    this.manifest = data.manifest;

    this.scripts = data.content_scripts.map(scriptData => new ScriptMatcher(this, scriptData));

    this._realExtension = null;

    this.startup();
  }

  startup() {
    // Extension.jsm takes care of this in the parent.
    if (isContentProcess) {
      let uri = Services.io.newURI(this.data.resourceURL);
      ExtensionManagement.startupExtension(this.uuid, uri, this);
    }
  }

  shutdown() {
    if (isContentProcess) {
      ExtensionManagement.shutdownExtension(this.uuid);
    }
    if (this._realExtension) {
      this._realExtension.shutdown();
    }
  }

  // Lazily create the real extension object when needed.
  get realExtension() {
    if (!this._realExtension) {
      this._realExtension = new ExtensionChild.BrowserExtensionContent(this.data);
    }
    return this._realExtension;
  }

  // Forward functions needed by ExtensionManagement.
  hasPermission(...args) {
    return this.realExtension.hasPermission(...args);
  }
  localize(...args) {
    return this.realExtension.localize(...args);
  }
  get whiteListedHosts() {
    return this.realExtension.whiteListedHosts;
  }
  get webAccessibleResources() {
    return this.realExtension.webAccessibleResources;
  }
}

ExtensionManager = {
  // Map[extensionId -> StubExtension]
  extensions: new Map(),

  init() {
    MessageChannel.setupMessageManagers([Services.cpmm]);

    Services.cpmm.addMessageListener("Extension:Startup", this);
    Services.cpmm.addMessageListener("Extension:Shutdown", this);
    Services.cpmm.addMessageListener("Extension:FlushJarCache", this);

    let procData = Services.cpmm.initialProcessData || {};

    for (let data of procData["Extension:Extensions"] || []) {
      let extension = new StubExtension(data);
      this.extensions.set(data.id, extension);
      DocumentManager.initExtension(extension);
    }

    if (isContentProcess) {
      // Make sure we handle new schema data until Schemas.jsm is loaded.
      if (!procData["Extension:Schemas"]) {
        procData["Extension:Schemas"] = new Map();
      }
      this.schemaJSON = procData["Extension:Schemas"];

      Services.cpmm.addMessageListener("Schema:Add", this);
    }
  },

  get(extensionId) {
    return this.extensions.get(extensionId);
  },

  receiveMessage({name, data}) {
    switch (name) {
      case "Extension:Startup": {
        let extension = new StubExtension(data);

        this.extensions.set(data.id, extension);

        DocumentManager.initExtension(extension);

        Services.cpmm.sendAsyncMessage("Extension:StartupComplete");
        break;
      }

      case "Extension:Shutdown": {
        let extension = this.extensions.get(data.id);
        this.extensions.delete(data.id);

        extension.shutdown();

        DocumentManager.uninitExtension(extension);
        break;
      }

      case "Extension:FlushJarCache": {
        ExtensionUtils.flushJarCache(data.path);
        Services.cpmm.sendAsyncMessage("Extension:FlushJarCacheComplete");
        break;
      }

      case "Schema:Add": {
        this.schemaJSON.set(data.url, data.schema);
        break;
      }
    }
  },
};

DocumentManager.earlyInit();
ExtensionManager.init();
