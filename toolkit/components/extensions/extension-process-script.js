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
XPCOMUtils.defineLazyModuleGetter(this, "MessageChannel",
                                  "resource://gre/modules/MessageChannel.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionChild",
                                  "resource://gre/modules/ExtensionChild.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionContent",
                                  "resource://gre/modules/ExtensionContent.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionPageChild",
                                  "resource://gre/modules/ExtensionPageChild.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "console", () => ExtensionUtils.getConsole());

const {
  DefaultWeakMap,
  getInnerWindowID,
} = ExtensionUtils;

// We need to avoid touching Services.appinfo here in order to prevent
// the wrong version from being cached during xpcshell test startup.
const appinfo = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);
const isContentProcess = appinfo.processType == appinfo.PROCESS_TYPE_CONTENT;

function parseScriptOptions(options) {
  return {
    allFrames: options.all_frames,
    matchAboutBlank: options.match_about_blank,
    frameID: options.frame_id,
    runAt: options.run_at,

    matches: new MatchPatternSet(options.matches),
    excludeMatches: new MatchPatternSet(options.exclude_matches || []),
    includeGlobs: options.include_globs && options.include_globs.map(glob => new MatchGlob(glob)),
    excludeGlobs: options.exclude_globs && options.exclude_globs.map(glob => new MatchGlob(glob)),

    jsPaths: options.js || [],
    cssPaths: options.css || [],
  };
}

var extensions = new DefaultWeakMap(policy => {
  let extension = new ExtensionChild.BrowserExtensionContent(policy.initData);
  extension.policy = policy;
  return extension;
});

class ScriptMatcher {
  constructor(matcher) {
    this.matcher = matcher;

    this._script = null;
  }

  get script() {
    if (!this._script) {
      this._script = new ExtensionContent.Script(extensions.get(this.matcher.extension),
                                                 this.matcher);
    }
    return this._script;
  }

  preload() {
    let {script} = this;

    script.loadCSS();
    script.compileScripts();
  }

  matchesWindow(window) {
    return this.matcher.matchesWindow(window);
  }

  injectInto(window) {
    return this.script.injectInto(window);
  }
}

function getMessageManager(window) {
  let docShell = window.document.docShell.QueryInterface(Ci.nsIInterfaceRequestor);
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
        let policy = WebExtensionPolicy.getByID(recipient.extensionId);

        let matcher = new WebExtensionContentScript(policy, parseScriptOptions(data.options));

        let options = Object.assign(matcher, {
          wantReturnValue: data.options.wantReturnValue,
          removeCSS: data.options.remove_css,
          cssOrigin: data.options.css_origin,
          cssCode: data.options.cssCode,
          jsCode: data.options.jsCode,
        });

        let script = new ScriptMatcher(options);

        return ExtensionContent.handleExtensionExecute(this.global, target, data.options, script);
      case "WebNavigation:GetFrame":
        return ExtensionContent.handleWebNavigationGetFrame(this.global, data.options);
      case "WebNavigation:GetAllFrames":
        return ExtensionContent.handleWebNavigationGetAllFrames(this.global);
    }
  }
}

let scriptMatchers = new DefaultWeakMap(matcher => new ScriptMatcher(matcher));

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

    this.injectExtensionScripts(extension);
  },
  uninitExtension(extension) {
    this.extensionCount--;
    if (this.extensionCount === 0) {
      this.uninit();
    }
  },

  extensionCount: 0,

  // Listeners

  observers: {
    "document-element-inserted"(document) {
      let window = document.defaultView;
      if (!document.location || !window ||
          // Make sure we only load into frames that belong to tabs, or other
          // special areas that we want to load content scripts into.
          !this.globals.has(getMessageManager(window))) {
        return;
      }

      this.loadInto(window);
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
      for (let script of extension.contentScripts) {
        if (script.matchesWindow(window)) {
          scriptMatchers.get(script).injectInto(window);
        }
      }
    }
  },

  /**
   * Checks that all parent frames for the given withdow either have the
   * same add-on ID, or are special chrome-privileged documents such as
   * about:addons or developer tools panels.
   *
   * @param {Window} window
   *        The window to check.
   * @param {string} addonId
   *        The add-on ID to check.
   * @returns {boolean}
   */
  checkParentFrames(window, addonId) {
    while (window.parent !== window) {
      let {frameElement} = window;
      window = window.parent;

      let principal = window.document.nodePrincipal;

      if (Services.scriptSecurityManager.isSystemPrincipal(principal)) {
        // The add-on manager is a special case, since it contains extension
        // options pages in same-type <browser> frames.
        if (window.location.href === "about:addons") {
          return true;
        }

        // NOTE: Special handling for devtools panels using a chrome iframe here
        // for the devtools panel, it is needed because a content iframe breaks
        // switching between docked and undocked mode (see bug 1075490).
        if (frameElement &&
            frameElement.mozMatchesSelector("browser[webextension-view-type='devtools_panel']")) {
          return true;
        }
      }

      if (principal.addonId !== addonId) {
        return false;
      }
    }

    return true;
  },

  loadInto(window) {
    let {addonId} = Cu.getObjectPrincipal(window);
    if (!addonId) {
      return;
    }

    let policy = WebExtensionPolicy.getByID(addonId);
    if (!policy) {
      throw new Error(`No registered extension for ID ${addonId}`);
    }

    let extension = extensions.get(policy);
    if (this.checkParentFrames(window, addonId) && ExtensionManagement.isExtensionProcess) {
      // We're in a top-level extension frame, or a sub-frame thereof,
      // in the extension process. Inject the full extension page API.
      ExtensionPageChild.initExtensionContext(extension, window);
    } else {
      // We're in a content sub-frame or not in the extension process.
      // Only inject a minimal content script API.
      ExtensionContent.initExtensionContext(extension, window);
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

ExtensionManager = {
  init() {
    MessageChannel.setupMessageManagers([Services.cpmm]);

    Services.cpmm.addMessageListener("Extension:Startup", this);
    Services.cpmm.addMessageListener("Extension:Shutdown", this);
    Services.cpmm.addMessageListener("Extension:FlushJarCache", this);

    let procData = Services.cpmm.initialProcessData || {};

    for (let data of procData["Extension:Extensions"] || []) {
      this.initExtension(data);
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

  initExtension(data) {
    let policy;
    if (isContentProcess) {
      policy = new WebExtensionPolicy({
        id: data.id,
        mozExtensionHostname: data.uuid,
        baseURL: data.resourceURL,

        permissions: Array.from(data.permissions),
        allowedOrigins: new MatchPatternSet(data.whiteListedHosts),
        webAccessibleResources: data.webAccessibleResources.map(host => new MatchGlob(host)),

        contentSecurityPolicy: data.manifest.content_security_policy,

        localizeCallback: str => extensions.get(policy).localize(str),

        backgroundScripts: (data.manifest.background &&
                            data.manifest.background.scripts),

        contentScripts: (data.manifest.content_scripts || []).map(parseScriptOptions),
      });

      policy.active = true;
    } else {
      policy = WebExtensionPolicy.getByID(data.id);
    }

    policy.initData = data;

    DocumentManager.initExtension(policy);
  },

  receiveMessage({name, data}) {
    switch (name) {
      case "Extension:Startup": {
        this.initExtension(data);

        Services.cpmm.sendAsyncMessage("Extension:StartupComplete");
        break;
      }

      case "Extension:Shutdown": {
        let policy = WebExtensionPolicy.getByID(data.id);

        if (extensions.has(policy)) {
          extensions.get(policy).shutdown();
        }

        DocumentManager.uninitExtension(policy);

        if (isContentProcess) {
          policy.active = false;
        }
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

function ExtensionProcessScript() {
  if (!ExtensionProcessScript.singleton) {
    ExtensionProcessScript.singleton = this;
  }
  return ExtensionProcessScript.singleton;
}

ExtensionProcessScript.singleton = null;

ExtensionProcessScript.prototype = {
  classID: Components.ID("{21f9819e-4cdf-49f9-85a0-850af91a5058}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.mozIExtensionProcessScript]),

  preloadContentScript(contentScript) {
    scriptMatchers.get(contentScript).preload();
  },

  loadContentScript(contentScript, window) {
    if (DocumentManager.globals.has(getMessageManager(window))) {
      scriptMatchers.get(contentScript).injectInto(window);
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ExtensionProcessScript]);

DocumentManager.earlyInit();
ExtensionManager.init();
