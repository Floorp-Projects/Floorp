/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Extension"];

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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/devtools/event-emitter.js");

XPCOMUtils.defineLazyModuleGetter(this, "Locale",
                                  "resource://gre/modules/Locale.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MatchPattern",
                                  "resource://gre/modules/MatchPattern.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

Cu.import("resource://gre/modules/ExtensionManagement.jsm");

// Register built-in parts of the API. Other parts may be registered
// in browser/, mobile/, or b2g/.
ExtensionManagement.registerScript("chrome://extensions/content/ext-alarms.js");
ExtensionManagement.registerScript("chrome://extensions/content/ext-backgroundPage.js");
ExtensionManagement.registerScript("chrome://extensions/content/ext-notifications.js");
ExtensionManagement.registerScript("chrome://extensions/content/ext-i18n.js");
ExtensionManagement.registerScript("chrome://extensions/content/ext-idle.js");
ExtensionManagement.registerScript("chrome://extensions/content/ext-runtime.js");
ExtensionManagement.registerScript("chrome://extensions/content/ext-extension.js");
ExtensionManagement.registerScript("chrome://extensions/content/ext-webNavigation.js");
ExtensionManagement.registerScript("chrome://extensions/content/ext-webRequest.js");
ExtensionManagement.registerScript("chrome://extensions/content/ext-storage.js");
ExtensionManagement.registerScript("chrome://extensions/content/ext-test.js");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
let {
  MessageBroker,
  Messenger,
  injectAPI,
} = ExtensionUtils;

let scriptScope = this;

// This object loads the ext-*.js scripts that define the extension API.
let Management = {
  initialized: false,
  scopes: [],
  apis: [],
  emitter: new EventEmitter(),

  // Loads all the ext-*.js scripts currently registered.
  lazyInit() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    for (let script of ExtensionManagement.getScripts()) {
      let scope = {extensions: this,
                   global: scriptScope,
                   ExtensionPage: ExtensionPage,
                   GlobalManager: GlobalManager};
      Services.scriptloader.loadSubScript(script, scope, "UTF-8");

      // Save the scope to avoid it being garbage collected.
      this.scopes.push(scope);
    }
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

  // Mash together into a single object all the APIs registered by the
  // functions above. Return the merged object.
  generateAPIs(extension, context) {
    let obj = {};

    // Recursively copy properties from source to dest.
    function copy(dest, source) {
      for (let prop in source) {
        if (typeof(source[prop]) == "object") {
          if (!(prop in dest)) {
            dest[prop] = {};
          }
          copy(dest[prop], source[prop]);
        } else {
          dest[prop] = source[prop];
        }
      }
    }

    for (let api of this.apis) {
      if (api.permission) {
        if (!extension.hasPermission(api.permission)) {
          continue;
        }
      }

      api = api.api(extension, context);
      copy(obj, api);
    }

    return obj;
  },

  // The ext-*.js scripts can ask to be notified for certain hooks.
  on(hook, callback) {
    this.emitter.on(hook, callback);
  },

  // Ask to run all the callbacks that are registered for a given hook.
  emit(hook, ...args) {
    this.lazyInit();
    this.emitter.emit(hook, ...args);
  },

  off(hook, callback) {
    this.emitter.off(hook, callback);
  }
};

// A MessageBroker that's used to send and receive messages for
// extension pages (which run in the chrome process).
let globalBroker = new MessageBroker([Services.mm, Services.ppmm]);

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
function ExtensionPage(extension, params)
{
  let {type, contentWindow, uri, docShell} = params;
  this.extension = extension;
  this.type = type;
  this.contentWindow = contentWindow || null;
  this.uri = uri || extension.baseURI;
  this.onClose = new Set();

  // This is the sender property passed to the Messenger for this
  // page. It can be augmented by the "page-open" hook.
  let sender = {id: extension.id};
  if (uri) {
    sender.url = uri.spec;
  }
  let delegate = {};
  Management.emit("page-load", this, params, sender, delegate);

  let filter = {id: extension.id};
  this.messenger = new Messenger(this, globalBroker, sender, filter, delegate);

  this.extension.views.add(this);
}

ExtensionPage.prototype = {
  get cloneScope() {
    return this.contentWindow;
  },

  callOnClose(obj) {
    this.onClose.add(obj);
  },

  forgetOnClose(obj) {
    this.onClose.delete(obj);
  },

  // Called when the extension shuts down.
  shutdown() {
    Management.emit("page-shutdown", this);
    this.unload();
  },

  // This method is called when an extension page navigates away or
  // its tab is closed.
  unload() {
    Management.emit("page-unload", this);

    this.extension.views.delete(this);

    for (let obj of this.onClose) {
      obj.close();
    }
  },
};

// Responsible for loading extension APIs into the right globals.
let GlobalManager = {
  // Number of extensions currently enabled.
  count: 0,

  // Map[docShell -> {extension, context}] where context is an ExtensionPage.
  docShells: new Map(),

  // Map[extension ID -> Extension]. Determines which extension is
  // responsible for content under a particular extension ID.
  extensionMap: new Map(),

  init(extension) {
    if (this.count == 0) {
      Services.obs.addObserver(this, "content-document-global-created", false);
    }
    this.count++;

    this.extensionMap.set(extension.id, extension);
  },

  uninit(extension) {
    this.count--;
    if (this.count == 0) {
      Services.obs.removeObserver(this, "content-document-global-created");
    }

    for (let [docShell, data] of this.docShells) {
      if (extension == data.extension) {
        this.docShells.delete(docShell);
      }
    }

    this.extensionMap.delete(extension.id);
  },

  injectInDocShell(docShell, extension, context) {
    this.docShells.set(docShell, {extension, context});
  },

  observe(contentWindow, topic, data) {
    function inject(extension, context) {
      let chromeObj = Cu.createObjectIn(contentWindow, {defineAs: "browser"});
      contentWindow.wrappedJSObject.chrome = contentWindow.wrappedJSObject.browser;
      let api = Management.generateAPIs(extension, context);
      injectAPI(api, chromeObj);
    }

    let docShell = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIWebNavigation)
                                .QueryInterface(Ci.nsIDocShellTreeItem)
                                .sameTypeRootTreeItem
                                .QueryInterface(Ci.nsIDocShell);

    if (this.docShells.has(docShell)) {
      let {extension, context} = this.docShells.get(docShell);
      inject(extension, context);
      return;
    }

    // We don't inject into sub-frames of a UI page.
    if (contentWindow != contentWindow.top) {
      return;
    }

    // Find the add-on associated with this document via the
    // principal's originAttributes. This value is computed by
    // extensionURIToAddonID, which ensures that we don't inject our
    // API into webAccessibleResources.
    let principal = contentWindow.document.nodePrincipal;
    let id = principal.originAttributes.addonId;
    if (!this.extensionMap.has(id)) {
      return;
    }
    let extension = this.extensionMap.get(id);
    let uri = contentWindow.document.documentURIObject;
    let context = new ExtensionPage(extension, {type: "tab", contentWindow, uri, docShell});
    inject(extension, context);

    let eventHandler = docShell.chromeEventHandler;
    let listener = event => {
      eventHandler.removeEventListener("unload", listener);
      context.unload();
    };
    eventHandler.addEventListener("unload", listener, true);
  },
};

// We create one instance of this class per extension. |addonData|
// comes directly from bootstrap.js when initializing.
this.Extension = function(addonData)
{
  let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
  let uuid = uuidGenerator.generateUUID().number;
  uuid = uuid.substring(1, uuid.length - 1); // Strip of { and } off the UUID.
  this.uuid = uuid;

  this.addonData = addonData;
  this.id = addonData.id;
  this.baseURI = Services.io.newURI("moz-extension://" + uuid, null, null);
  this.manifest = null;
  this.localeMessages = null;

  this.views = new Set();

  this.onStartup = null;

  this.hasShutdown = false;
  this.onShutdown = new Set();

  this.permissions = new Set();
  this.whiteListedHosts = null;
  this.webAccessibleResources = new Set();

  this.emitter = new EventEmitter();
}

Extension.prototype = {
  on(hook, f) {
    return this.emitter.on(hook, f);
  },

  off(hook, f) {
    return this.emitter.off(hook, f);
  },

  emit(...args) {
    return this.emitter.emit(...args);
  },

  testMessage(...args) {
    Management.emit("test-message", this, ...args);
  },

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
      content_scripts: this.manifest.content_scripts || [],
      webAccessibleResources: this.webAccessibleResources,
      whiteListedHosts: this.whiteListedHosts.serialize(),
    };
  },

  // https://developer.chrome.com/extensions/i18n
  localizeMessage(message, substitutions) {
    if (message in this.localeMessages) {
      let str = this.localeMessages[message].message;

      if (!substitutions) {
        substitutions = [];
      }
      if (!Array.isArray(substitutions)) {
        substitutions = [substitutions];
      }

      // https://developer.chrome.com/extensions/i18n-messages
      // |str| may contain substrings of the form $1 or $PLACEHOLDER$.
      // In the former case, we replace $n with substitutions[n - 1].
      // In the latter case, we consult the placeholders array.
      // The placeholder may itself use $n to refer to substitutions.
      let replacer = (matched, name) => {
        if (name.length == 1 && name[0] >= '1' && name[0] <= '9') {
          return substitutions[parseInt(name) - 1];
        } else {
          let content = this.localeMessages[message].placeholders[name].content;
          if (content[0] == '$') {
            return replacer(matched, content[1]);
          } else {
            return content;
          }
        }
      };
      return str.replace(/\$([A-Za-z_@]+)\$/, replacer)
                .replace(/\$([0-9]+)/, replacer)
                .replace(/\$\$/, "$");
    }

    // Check for certain pre-defined messages.
    if (message == "@@extension_id") {
      return this.id;
    } else if (message == "@@ui_locale") {
      return Locale.getLocale();
    } else if (message == "@@bidi_dir") {
      return "ltr"; // FIXME
    }

    Cu.reportError(`Unknown localization message ${message}`);
    return "??";
  },

  localize(str) {
    if (!str) {
      return str;
    }

    if (str.startsWith("__MSG_") && str.endsWith("__")) {
      let message = str.substring("__MSG_".length, str.length - "__".length);
      return this.localizeMessage(message);
    }

    return str;
  },

  readJSON(uri) {
    return new Promise((resolve, reject) => {
      NetUtil.asyncFetch({uri, loadUsingSystemPrincipal: true}, (inputStream, status) => {
        if (!Components.isSuccessCode(status)) {
          reject(status);
          return;
        }
        let text = NetUtil.readInputStreamToString(inputStream, inputStream.available());
        try {
          resolve(JSON.parse(text));
        } catch (e) {
          reject(e);
        }
      });
    });
  },

  readManifest() {
    let manifestURI = Services.io.newURI("manifest.json", null, this.baseURI);
    return this.readJSON(manifestURI);
  },

  readLocaleFile(locale) {
    let dir = locale.replace("-", "_");
    let url = `_locales/${dir}/messages.json`;
    let uri = Services.io.newURI(url, null, this.baseURI);
    return this.readJSON(uri);
  },

  readLocaleMessages() {
    let locales = [];

    // We need to base this off of this.addonData.resourceURI rather
    // than baseURI since baseURI is a moz-extension URI, which always
    // QIs to nsIFileURL.
    let uri = Services.io.newURI("_locales", null, this.addonData.resourceURI);
    if (uri instanceof Ci.nsIFileURL) {
      let file = uri.file;
      let enumerator;
      try {
        enumerator = file.directoryEntries;
      } catch (e) {
        return {};
      }
      while (enumerator.hasMoreElements()) {
        let file = enumerator.getNext().QueryInterface(Ci.nsIFile);
        locales.push({
          name: file.leafName,
          locales: [file.leafName.replace("_", "-")]
        });
      }
    }

    if (uri instanceof Ci.nsIJARURI && uri.JARFile instanceof Ci.nsIFileURL) {
      let file = uri.JARFile.file;
      let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(Ci.nsIZipReader);
      try {
        zipReader.open(file);
        let enumerator = zipReader.findEntries("_locales/*");
        while (enumerator.hasMore()) {
          let name = enumerator.getNext();
          let match = name.match(new RegExp("_locales\/([^/]*)"));
          if (match && match[1]) {
            locales.push({
              name: match[1],
              locales: [match[1].replace("_", "-")]
            });
          }
        }
      } finally {
        zipReader.close();
      }
    }

    let locale = Locale.findClosestLocale(locales);
    if (locale) {
      return this.readLocaleFile(locale.name).catch(() => {});
    }
    return {};
  },

  runManifest(manifest) {
    let permissions = manifest.permissions || [];
    let webAccessibleResources = manifest.web_accessible_resources || [];

    let whitelist = [];
    for (let perm of permissions) {
      if (perm.match(/:\/\//)) {
        whitelist.push(perm);
      } else {
        this.permissions.add(perm);
      }
    }
    this.whiteListedHosts = new MatchPattern(whitelist);

    let resources = new Set();
    for (let url of webAccessibleResources) {
      resources.add(url);
    }
    this.webAccessibleResources = resources;

    for (let directive in manifest) {
      Management.emit("manifest_" + directive, directive, this, manifest);
    }

    let data = Services.ppmm.initialProcessData;
    if (!data["Extension:Extensions"]) {
      data["Extension:Extensions"] = [];
    }
    let serial = this.serialize();
    data["Extension:Extensions"].push(serial);
    Services.ppmm.broadcastAsyncMessage("Extension:Startup", serial);
  },

  callOnClose(obj) {
    this.onShutdown.add(obj);
  },

  forgetOnClose(obj) {
    this.onShutdown.delete(obj);
  },

  startup() {
    try {
      ExtensionManagement.startupExtension(this.uuid, this.addonData.resourceURI, this);
    } catch (e) {
      return Promise.reject(e);
    }

    return Promise.all([this.readManifest(), this.readLocaleMessages()]).then(([manifest, messages]) => {
      if (this.hasShutdown) {
        return;
      }

      GlobalManager.init(this);

      this.manifest = manifest;
      this.localeMessages = messages;

      Management.emit("startup", this);

      this.runManifest(manifest);
    }).catch(e => {
      dump(`Extension error: ${e} ${e.fileName}:${e.lineNumber}\n`);
      Cu.reportError(e);
      throw e;
    });
  },

  shutdown() {
    this.hasShutdown = true;
    if (!this.manifest) {
      return;
    }

    GlobalManager.uninit(this);

    for (let view of this.views) {
      view.shutdown();
    }

    for (let obj of this.onShutdown) {
      obj.close();
    }

    Management.emit("shutdown", this);

    Services.ppmm.broadcastAsyncMessage("Extension:Shutdown", {id: this.id});

    ExtensionManagement.shutdownExtension(this.uuid);
  },

  hasPermission(perm) {
    return this.permissions.has(perm);
  },
};

