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

ChromeUtils.import("resource://gre/modules/MessageChannel.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  ExtensionChild: "resource://gre/modules/ExtensionChild.jsm",
  ExtensionCommon: "resource://gre/modules/ExtensionCommon.jsm",
  ExtensionContent: "resource://gre/modules/ExtensionContent.jsm",
  ExtensionPageChild: "resource://gre/modules/ExtensionPageChild.jsm",
});

ChromeUtils.import("resource://gre/modules/ExtensionUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "console", () => ExtensionCommon.getConsole());

const {
  DefaultWeakMap,
  getInnerWindowID,
} = ExtensionUtils;

// We need to avoid touching Services.appinfo here in order to prevent
// the wrong version from being cached during xpcshell test startup.
// eslint-disable-next-line mozilla/use-services
const appinfo = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);
const isContentProcess = appinfo.processType == appinfo.PROCESS_TYPE_CONTENT;

function tryMatchPatternSet(patterns, options) {
  try {
    return new MatchPatternSet(patterns, options);
  } catch (e) {
    Cu.reportError(e);
    return new MatchPatternSet([]);
  }
}

function parseScriptOptions(options, restrictSchemes = true) {
  return {
    allFrames: options.all_frames,
    matchAboutBlank: options.match_about_blank,
    frameID: options.frame_id,
    runAt: options.run_at,
    hasActiveTabPermission: options.hasActiveTabPermission,

    matches: tryMatchPatternSet(options.matches, {restrictSchemes}),
    excludeMatches: tryMatchPatternSet(options.exclude_matches || [], {restrictSchemes}),
    includeGlobs: options.include_globs && options.include_globs.map(glob => new MatchGlob(glob)),
    excludeGlobs: options.exclude_globs && options.exclude_globs.map(glob => new MatchGlob(glob)),

    jsPaths: options.js || [],
    cssPaths: options.css || [],
  };
}

var extensions = new DefaultWeakMap(policy => {
  let data = policy.initData;
  if (data.serialize) {
    // We have an actual Extension rather than serialized extension
    // data, so serialize it now to make sure we have consistent inputs
    // between parent and child processes.
    data = data.serialize();
  }

  let extension = new ExtensionChild.BrowserExtensionContent(data);
  extension.policy = policy;
  return extension;
});

var contentScripts = new DefaultWeakMap(matcher => {
  return new ExtensionContent.Script(extensions.get(matcher.extension),
                                     matcher);
});

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
    this.global.addMessageListener("Extension:SetFrameData", this);

    this.frameData = null;

    MessageChannel.addListener(global, "Extension:Capture", this);
    MessageChannel.addListener(global, "Extension:DetectLanguage", this);
    MessageChannel.addListener(global, "Extension:Execute", this);
    MessageChannel.addListener(global, "WebNavigation:GetFrame", this);
    MessageChannel.addListener(global, "WebNavigation:GetAllFrames", this);
  }

  get messageFilterStrict() {
    return {
      innerWindowID: getInnerWindowID(this.global.content),
    };
  }

  getFrameData(force = false) {
    if (!this.frameData && force) {
      this.frameData = this.global.sendSyncMessage("Extension:GetTabAndWindowId")[0];
    }
    return this.frameData;
  }

  async receiveMessage({target, messageName, recipient, data, name}) {
    switch (name) {
      case "Extension:SetFrameData":
        if (this.frameData) {
          Object.assign(this.frameData, data);
        } else {
          this.frameData = data;
        }
        if (data.viewType && WebExtensionPolicy.isExtensionProcess) {
          ExtensionPageChild.expectViewLoad(this.global, data.viewType);
        }
        return;
    }

    switch (messageName) {
      case "Extension:Capture":
        return ExtensionContent.handleExtensionCapture(this.global, data.width, data.height, data.options);
      case "Extension:DetectLanguage":
        return ExtensionContent.handleDetectLanguage(this.global, target);
      case "Extension:Execute":
        let policy = WebExtensionPolicy.getByID(recipient.extensionId);

        let matcher = new WebExtensionContentScript(policy, parseScriptOptions(data.options, !policy.hasPermission("mozillaAddons")));

        Object.assign(matcher, {
          wantReturnValue: data.options.wantReturnValue,
          removeCSS: data.options.remove_css,
          cssOrigin: data.options.css_origin,
          jsCode: data.options.jsCode,
        });

        let script = contentScripts.get(matcher);

        // Add the cssCode to the script, so that it can be converted into a cached URL.
        await script.addCSSCode(data.options.cssCode);
        delete data.options.cssCode;

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

  // Initialize a frame script global which extension contexts may be loaded
  // into.
  initGlobal(global) {
    this.globals.set(global, new ExtensionGlobal(global));
    // eslint-disable-next-line mozilla/balanced-listeners
    global.addEventListener("unload", () => {
      this.globals.delete(global);
    });
  },

  initExtension(extension) {
    this.injectExtensionScripts(extension);
  },

  // Listeners

  observe(subject, topic, data) {
    if (topic == "tab-content-frameloader-created") {
      this.initGlobal(subject);
    }
  },

  // Script loading

  injectExtensionScripts(extension) {
    for (let window of this.enumerateWindows()) {
      let runAt = {document_start: [], document_end: [], document_idle: []};

      for (let script of extension.contentScripts) {
        if (script.matchesWindow(window)) {
          runAt[script.runAt].push(script);
        }
      }

      let inject = matcher => contentScripts.get(matcher).injectInto(window);
      let injectAll = matchers => Promise.all(matchers.map(inject));

      // Intentionally using `.then` instead of `await`, we only need to
      // chain injecting other scripts into *this* window, not all windows.
      injectAll(runAt.document_start)
        .then(() => injectAll(runAt.document_end))
        .then(() => injectAll(runAt.document_idle));
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
      window = window.parent;

      let principal = window.document.nodePrincipal;

      if (Services.scriptSecurityManager.isSystemPrincipal(principal)) {
        // The add-on manager is a special case, since it contains extension
        // options pages in same-type <browser> frames.
        if (window.location.href === "about:addons") {
          return true;
        }
      }

      if (principal.addonId !== addonId) {
        return false;
      }
    }

    return true;
  },

  loadInto(policy, window) {
    let extension = extensions.get(policy);
    if (WebExtensionPolicy.isExtensionProcess && this.checkParentFrames(window, policy.id)) {
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
  // WeakMap<WebExtensionPolicy, Map<string, WebExtensionContentScript>>
  registeredContentScripts: new DefaultWeakMap((extension) => new Map()),

  init() {
    MessageChannel.setupMessageManagers([Services.cpmm]);

    Services.cpmm.addMessageListener("Extension:Startup", this);
    Services.cpmm.addMessageListener("Extension:Shutdown", this);
    Services.cpmm.addMessageListener("Extension:FlushJarCache", this);
    Services.cpmm.addMessageListener("Extension:RegisterContentScript", this);
    Services.cpmm.addMessageListener("Extension:UnregisterContentScripts", this);

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

  initExtensionPolicy(extension) {
    let policy = WebExtensionPolicy.getByID(extension.id);
    if (!policy) {
      let localizeCallback, allowedOrigins, webAccessibleResources;
      let restrictSchemes = !extension.permissions.has("mozillaAddons");

      if (extension.localize) {
        // We have a real Extension object.
        localizeCallback = extension.localize.bind(extension);
        allowedOrigins = extension.whiteListedHosts;
        webAccessibleResources = extension.webAccessibleResources;
      } else {
        // We have serialized extension data;
        localizeCallback = str => extensions.get(policy).localize(str);
        allowedOrigins = new MatchPatternSet(extension.whiteListedHosts, {restrictSchemes});
        webAccessibleResources = extension.webAccessibleResources.map(host => new MatchGlob(host));
      }

      policy = new WebExtensionPolicy({
        id: extension.id,
        mozExtensionHostname: extension.uuid,
        name: extension.name,
        baseURL: extension.resourceURL,

        permissions: Array.from(extension.permissions),
        allowedOrigins,
        webAccessibleResources,

        contentSecurityPolicy: extension.manifest.content_security_policy,

        localizeCallback,

        backgroundScripts: (extension.manifest.background &&
                            extension.manifest.background.scripts),

        contentScripts: extension.contentScripts.map(script => parseScriptOptions(script, restrictSchemes)),
      });

      policy.debugName = `${JSON.stringify(policy.name)} (ID: ${policy.id}, ${policy.getURL()})`;

      // Register any existent dynamically registered content script for the extension
      // when a content process is started for the first time (which also cover
      // a content process that crashed and it has been recreated).
      const registeredContentScripts = this.registeredContentScripts.get(policy);

      if (extension.registeredContentScripts) {
        for (let [scriptId, options] of extension.registeredContentScripts) {
          const parsedOptions = parseScriptOptions(options, restrictSchemes);
          const script = new WebExtensionContentScript(policy, parsedOptions);
          policy.registerContentScript(script);
          registeredContentScripts.set(scriptId, script);
        }
      }

      policy.active = true;
      policy.initData = extension;
    }
    return policy;
  },

  initExtension(data) {
    let policy = this.initExtensionPolicy(data);

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

        if (policy) {
          if (extensions.has(policy)) {
            extensions.get(policy).shutdown();
          }

          if (isContentProcess) {
            policy.active = false;
          }
        }
        Services.cpmm.sendAsyncMessage("Extension:ShutdownComplete");
        break;
      }

      case "Extension:FlushJarCache": {
        ExtensionUtils.flushJarCache(data.path);
        Services.cpmm.sendAsyncMessage("Extension:FlushJarCacheComplete");
        break;
      }

      case "Schema:Add": {
        // If we're given a Map, the ordering of the initial items
        // matters, so swap with our current data to make sure its
        // entries appear first.
        if (typeof data.get === "function") {
          [this.schemaJSON, data] = [data, this.schemaJSON];

          Services.cpmm.initialProcessData["Extension:Schemas"] =
            this.schemaJSON;
        }

        for (let [url, schema] of data) {
          this.schemaJSON.set(url, schema);
        }
        break;
      }

      case "Extension:RegisterContentScript": {
        let policy = WebExtensionPolicy.getByID(data.id);

        if (policy) {
          const registeredContentScripts = this.registeredContentScripts.get(policy);

          if (registeredContentScripts.has(data.scriptId)) {
            Cu.reportError(new Error(
              `Registering content script ${data.scriptId} on ${data.id} more than once`));
          } else {
            try {
              const parsedOptions = parseScriptOptions(data.options, !policy.hasPermission("mozillaAddons"));
              const script = new WebExtensionContentScript(policy, parsedOptions);
              policy.registerContentScript(script);
              registeredContentScripts.set(data.scriptId, script);
            } catch (e) {
              Cu.reportError(e);
            }
          }
        }

        Services.cpmm.sendAsyncMessage("Extension:RegisterContentScriptComplete");
        break;
      }

      case "Extension:UnregisterContentScripts": {
        let policy = WebExtensionPolicy.getByID(data.id);

        if (policy) {
          const registeredContentScripts = this.registeredContentScripts.get(policy);

          for (const scriptId of data.scriptIds) {
            const script = registeredContentScripts.get(scriptId);
            if (script) {
              try {
                policy.unregisterContentScript(script);
                registeredContentScripts.delete(scriptId);
              } catch (e) {
                Cu.reportError(e);
              }
            }
          }
        }

        Services.cpmm.sendAsyncMessage("Extension:UnregisterContentScriptsComplete");
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
  QueryInterface: ChromeUtils.generateQI([Ci.mozIExtensionProcessScript]),

  get wrappedJSObject() { return this; },

  getFrameData(global, force) {
    let extGlobal = DocumentManager.globals.get(global);
    return extGlobal && extGlobal.getFrameData(force);
  },

  initExtension(extension) {
    return ExtensionManager.initExtensionPolicy(extension);
  },

  initExtensionDocument(policy, doc) {
    if (DocumentManager.globals.has(getMessageManager(doc.defaultView))) {
      DocumentManager.loadInto(policy, doc.defaultView);
    }
  },

  getExtensionChild(id) {
    let policy = WebExtensionPolicy.getByID(id);
    if (policy) {
      return extensions.get(policy);
    }
  },

  preloadContentScript(contentScript) {
    contentScripts.get(contentScript).preload();
  },

  loadContentScript(contentScript, window) {
    if (DocumentManager.globals.has(getMessageManager(window))) {
      contentScripts.get(contentScript).injectInto(window);
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ExtensionProcessScript]);

DocumentManager.earlyInit();
ExtensionManager.init();
