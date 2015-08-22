/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionContent"];

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
XPCOMUtils.defineLazyModuleGetter(this, "MatchPattern",
                                  "resource://gre/modules/MatchPattern.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
let {
  runSafeWithoutClone,
  MessageBroker,
  Messenger,
  ignoreEvent,
  injectAPI,
} = ExtensionUtils;

function isWhenBeforeOrSame(when1, when2)
{
  let table = {"document_start": 0,
               "document_end": 1,
               "document_idle": 2};
  return table[when1] <= table[when2];
}

// This is the fairly simple API that we inject into content
// scripts.
let api = context => { return {
  runtime: {
    connect: function(extensionId, connectInfo) {
      let name = connectInfo && connectInfo.name || "";
      let recipient = extensionId ? {extensionId} : {extensionId: context.extensionId};
      return context.messenger.connect(context.messageManager, name, recipient);
    },

    getManifest: function(context) {
      return context.extension.getManifest();
    },

    getURL: function(path) {
      return context.extension.baseURI.resolve(url);
    },

    onConnect: context.messenger.onConnect("runtime.onConnect"),

    onMessage: context.messenger.onMessage("runtime.onMessage"),

    sendMessage: function(...args) {
      let extensionId, message, options, responseCallback;
      if (args.length == 1) {
        message = args[0];
      } else if (args.length == 2) {
        [message, responseCallback] = args;
      } else {
        [extensionId, message, options, responseCallback] = args;
      }

      let recipient = extensionId ? {extensionId} : {extensionId: context.extensionId};
      context.messenger.sendMessage(context.messageManager, message, recipient, responseCallback);
    },
  },

  extension: {
    getURL: function(path) {
      return context.extension.baseURI.resolve(url);
    },

    inIncognitoContext: PrivateBrowsingUtils.isContentWindowPrivate(context.contentWindow),
  },
}};

// Represents a content script.
function Script(options)
{
  this.options = options;
  this.run_at = this.options.run_at;
  this.js = this.options.js || [];
  this.css = this.options.css || [];

  this.matches_ = new MatchPattern(this.options.matches);
  this.exclude_matches_ = new MatchPattern(this.options.exclude_matches || null);

  // TODO: Support glob patterns.
}

Script.prototype = {
  matches(window) {
    let uri = window.document.documentURIObject;
    if (!this.matches_.matches(uri)) {
      return false;
    }

    if (this.exclude_matches_.matches(uri)) {
      return false;
    }

    if (!this.options.all_frames && window.top != window) {
      return false;
    }

    // TODO: match_about_blank.

    return true;
  },

  tryInject(extension, window, sandbox, shouldRun) {
    if (!this.matches(window)) {
      return;
    }

    if (shouldRun("document_start")) {
      let winUtils = window.QueryInterface(Ci.nsIInterfaceRequestor).
        getInterface(Ci.nsIDOMWindowUtils);

      for (let url of this.css) {
        url = extension.baseURI.resolve(url);
        runSafeWithoutClone(winUtils.loadSheetUsingURIString, url, winUtils.AUTHOR_SHEET);
      }

      if (this.options.cssCode) {
        let url = "data:text/css;charset=utf-8," + encodeURIComponent(this.options.cssCode);
        runSafeWithoutClone(winUtils.loadSheetUsingURIString, url, winUtils.AUTHOR_SHEET);
      }
    }

    let scheduled = this.run_at || "document_idle";
    if (shouldRun(scheduled)) {
      for (let url of this.js) {
        // On gonk we need to load the resources asynchronously because the
        // app: channels only support asyncOpen. This is safe only in the
        // `document_idle` state.
        if (AppConstants.platform == "gonk" && scheduled != "document_idle") {
          Cu.reportError(`Script injection: ignoring ${url} at ${scheduled}`);
        }
        url = extension.baseURI.resolve(url);

        let options = {
          target: sandbox,
          charset: "UTF-8",
          async: AppConstants.platform == "gonk"
        }
        Services.scriptloader.loadSubScriptWithOptions(url, options);
      }

      if (this.options.jsCode) {
        Cu.evalInSandbox(this.options.jsCode, sandbox, "latest");
      }
    }
  },
};

function getWindowMessageManager(contentWindow)
{
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

// Scope in which extension content script code can run. It uses
// Cu.Sandbox to run the code. There is a separate scope for each
// frame.
function ExtensionContext(extensionId, contentWindow)
{
  this.extension = ExtensionManager.get(extensionId);
  this.extensionId = extensionId;
  this.contentWindow = contentWindow;

  let utils = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
  let outerWindowId = utils.outerWindowID;
  let frameId = contentWindow == contentWindow.top ? 0 : outerWindowId;
  this.frameId = frameId;

  let mm = getWindowMessageManager(contentWindow);
  this.messageManager = mm;

  let prin = [contentWindow];
  if (Services.scriptSecurityManager.isSystemPrincipal(contentWindow.document.nodePrincipal)) {
    // Make sure we don't hand out the system principal by accident.
    prin = Cc["@mozilla.org/nullprincipal;1"].createInstance(Ci.nsIPrincipal);
  }

  this.sandbox = Cu.Sandbox(prin, {sandboxPrototype: contentWindow, wantXrays: true});

  let delegate = {
    getSender(context, target, sender) {
      // Nothing to do here.
    }
  };

  let url = contentWindow.location.href;
  let broker = ExtensionContent.getBroker(mm);
  this.messenger = new Messenger(this, broker, {id: extensionId, frameId, url},
                                 {id: extensionId, frameId}, delegate);

  let chromeObj = Cu.createObjectIn(this.sandbox, {defineAs: "browser"});
  this.sandbox.wrappedJSObject.chrome = this.sandbox.wrappedJSObject.browser;
  injectAPI(api(this), chromeObj);

  this.onClose = new Set();
}

ExtensionContext.prototype = {
  get cloneScope() {
    return this.sandbox;
  },

  execute(script, shouldRun) {
    script.tryInject(this.extension, this.contentWindow, this.sandbox, shouldRun);
  },

  callOnClose(obj) {
    this.onClose.add(obj);
    Cu.nukeSandbox(this.sandbox);
  },

  forgetOnClose(obj) {
    this.onClose.delete(obj);
  },

  close() {
    for (let obj of this.onClose) {
      obj.close();
    }
  },
};

// Responsible for creating ExtensionContexts and injecting content
// scripts into them when new documents are created.
let DocumentManager = {
  extensionCount: 0,

  // WeakMap[window -> Map[extensionId -> ExtensionContext]]
  windows: new WeakMap(),

  init() {
    Services.obs.addObserver(this, "document-element-inserted", false);
    Services.obs.addObserver(this, "dom-window-destroyed", false);
  },

  uninit() {
    Services.obs.removeObserver(this, "document-element-inserted");
    Services.obs.removeObserver(this, "dom-window-destroyed");
  },

  getWindowState(contentWindow) {
    let readyState = contentWindow.document.readyState;
    if (readyState == "loading") {
      return "document_start";
    } else if (readyState == "interactive") {
      return "document_end";
    } else {
      return "document_idle";
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

      this.windows.delete(window);

      this.trigger("document_start", window);
      window.addEventListener("DOMContentLoaded", this, true);
      window.addEventListener("load", this, true);
    } else if (topic == "dom-window-destroyed") {
      let window = subject;
      if (!this.windows.has(window)) {
        return;
      }

      let extensions = this.windows.get(window);
      for (let [extensionId, context] of extensions) {
        context.close();
      }

      this.windows.delete(window);
    }
  },

  handleEvent: function(event) {
    let window = event.target.defaultView;
    window.removeEventListener(event.type, this, true);

    // Need to check if we're still on the right page? Greasemonkey does this.

    if (event.type == "DOMContentLoaded") {
      this.trigger("document_end", window);
    } else if (event.type == "load") {
      this.trigger("document_idle", window);
    }
  },

  executeScript(global, extensionId, script) {
    let window = global.content;
    let extensions = this.windows.get(window);
    if (!extensions) {
      return;
    }
    let context = extensions.get(extensionId);
    if (!context) {
      return;
    }

    // TODO: Somehow make sure we have the right permissions for this origin!

    // FIXME: Script should be executed only if current state has
    // already reached its run_at state, or we have to keep it around
    // somewhere to execute later.
    context.execute(script, scheduled => true);
  },

  enumerateWindows: function*(docShell) {
    let window = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindow)
    yield [window, this.getWindowState(window)];

    for (let i = 0; i < docShell.childCount; i++) {
      let child = docShell.getChildAt(i).QueryInterface(Ci.nsIDocShell);
      yield* this.enumerateWindows(child);
    }
  },

  getContext(extensionId, window) {
    if (!this.windows.has(window)) {
      this.windows.set(window, new Map());
    }
    let extensions = this.windows.get(window);
    if (!extensions.has(extensionId)) {
      let context = new ExtensionContext(extensionId, window);
      extensions.set(extensionId, context);
    }
    return extensions.get(extensionId);
  },

  startupExtension(extensionId) {
    if (this.extensionCount == 0) {
      this.init();
    }
    this.extensionCount++;

    let extension = ExtensionManager.get(extensionId);
    for (let global of ExtensionContent.globals.keys()) {
      for (let [window, state] of this.enumerateWindows(global.docShell)) {
        for (let script of extension.scripts) {
          if (script.matches(window)) {
            let context = this.getContext(extensionId, window);
            context.execute(script, scheduled => isWhenBeforeOrSame(scheduled, state));
          }
        }
      }
    }
  },

  shutdownExtension(extensionId) {
    for (let global of ExtensionContent.globals.keys()) {
      for (let [window, state] of this.enumerateWindows(global.docShell)) {
        let extensions = this.windows.get(window);
        if (!extensions) {
          continue;
        }
        let context = extensions.get(extensionId);
        if (context) {
          context.close();
          extensions.delete(extensionId);
        }
      }
    }

    this.extensionCount--;
    if (this.extensionCount == 0) {
      this.uninit();
    }
  },

  trigger(when, window) {
    let state = this.getWindowState(window);
    for (let [extensionId, extension] of ExtensionManager.extensions) {
      for (let script of extension.scripts) {
        if (script.matches(window)) {
          let context = this.getContext(extensionId, window);
          context.execute(script, scheduled => scheduled == state);
        }
      }
    }
  },
};

// Represents a browser extension in the content process.
function BrowserExtensionContent(data)
{
  this.id = data.id;
  this.uuid = data.uuid;
  this.data = data;
  this.scripts = [ for (scriptData of data.content_scripts) new Script(scriptData) ];
  this.webAccessibleResources = data.webAccessibleResources;
  this.whiteListedHosts = data.whiteListedHosts;

  this.manifest = data.manifest;
  this.baseURI = Services.io.newURI(data.baseURL, null, null);

  let uri = Services.io.newURI(data.resourceURL, null, null);

  if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
    // Extension.jsm takes care of this in the parent.
    ExtensionManagement.startupExtension(this.uuid, uri, this);
  }
};

BrowserExtensionContent.prototype = {
  shutdown() {
    if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
      ExtensionManagement.shutdownExtension(this.uuid);
    }
  },
};

let ExtensionManager = {
  // Map[extensionId, BrowserExtensionContent]
  extensions: new Map(),

  init() {
    Services.cpmm.addMessageListener("Extension:Startup", this);
    Services.cpmm.addMessageListener("Extension:Shutdown", this);

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
    case "Extension:Startup":
      extension = new BrowserExtensionContent(data);
      this.extensions.set(data.id, extension);
      DocumentManager.startupExtension(data.id);
      break;

    case "Extension:Shutdown":
      extension = this.extensions.get(data.id);
      extension.shutdown();
      DocumentManager.shutdownExtension(data.id);
      this.extensions.delete(data.id);
      break;
    }
  }
};

this.ExtensionContent = {
  globals: new Map(),

  init(global) {
    let broker = new MessageBroker([global]);
    this.globals.set(global, broker);

    global.addMessageListener("Extension:Execute", this);

    let windowId = global.content
                         .QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils)
                         .outerWindowID;
    global.sendAsyncMessage("Extension:TopWindowID", {windowId});
  },

  uninit(global) {
    this.globals.delete(global);

    let windowId = global.content
                         .QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils)
                         .outerWindowID;
    global.sendAsyncMessage("Extension:RemoveTopWindowID", {windowId});
  },

  getBroker(messageManager) {
    return this.globals.get(messageManager);
  },

  receiveMessage({target, name, data}) {
    switch (name) {
    case "Extension:Execute":
      data.options.matches = "<all_urls>";
      let script = new Script(data.options);
      let {extensionId} = data;
      DocumentManager.executeScript(target, extensionId, script);
      break;
    }
  },
};

ExtensionManager.init();
