/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionContext"];

/*
 * This file handles addon logic that is independent of the chrome process.
 * When addons run out-of-process, this is the main entry point.
 * Its primary function is managing addon globals.
 *
 * Don't put contentscript logic here, use ExtensionContent.jsm instead.
 */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");

const CATEGORY_EXTENSION_SCRIPTS_ADDON = "webextension-scripts-addon";

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  BaseContext,
  ChildAPIManager,
  LocalAPIImplementation,
  Messenger,
  SchemaAPIManager,
} = ExtensionUtils;

// There is a circular dependency between Extension.jsm and us.
// Long-term this file should not reference Extension.jsm (because they would
// live in different processes), but for now use lazy getters.
XPCOMUtils.defineLazyGetter(this, "findPathInObject",
  () => Cu.import("resource://gre/modules/Extension.jsm", {}).findPathInObject);
XPCOMUtils.defineLazyGetter(this, "Management",
  () => Cu.import("resource://gre/modules/Extension.jsm", {}).Management);
XPCOMUtils.defineLazyGetter(this, "ParentAPIManager",
  () => Cu.import("resource://gre/modules/Extension.jsm", {}).ParentAPIManager);

var apiManager = new class extends SchemaAPIManager {
  constructor() {
    super("addon");
    this.initialized = false;
  }

  generateAPIs(...args) {
    if (!this.initialized) {
      this.initialized = true;
      for (let [/* name */, value] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCRIPTS_ADDON)) {
        this.loadScript(value);
      }
    }
    return super.generateAPIs(...args);
  }

  registerSchemaAPI(namespace, envType, getAPI) {
    if (envType == "addon_child") {
      super.registerSchemaAPI(namespace, envType, getAPI);
    }
  }
}();

// A class that behaves identical to a ChildAPIManager, except
// 1) creation of the ProxyContext in the parent is synchronous, and
// 2) APIs without a local implementation and marked as incompatible with the
//    out-of-process model fall back to directly invoking the parent methods.
// TODO(robwu): Remove this when all APIs have migrated.
class WannabeChildAPIManager extends ChildAPIManager {
  createProxyContextInConstructor(data) {
    // Create a structured clone to simulate IPC.
    data = Object.assign({}, data);
    let {principal} = data;  // Not structurally cloneable.
    delete data.principal;
    data = Cu.cloneInto(data, {});
    data.principal = principal;
    data.cloneScopeInProcess = this.context.cloneScope;
    let name = "API:CreateProxyContext";
    // The <browser> that receives messages from `this.messageManager`.
    let target = this.context.contentWindow
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDocShell)
      .chromeEventHandler;
    ParentAPIManager.receiveMessage({name, data, target});

    let proxyContext = ParentAPIManager.proxyContexts.get(this.id);
    // Many APIs rely on this, so temporarily add it to keep the commit small.
    proxyContext.setContentWindow(this.context.contentWindow);

    // Synchronously unload the ProxyContext because we synchronously create it.
    this.context.callOnClose({close: proxyContext.unload.bind(proxyContext)});
  }

  getFallbackImplementation(namespace, name) {
    // This is gross and should be removed ASAP.
    let shouldSynchronouslyUseParentAPI = true;
    // The test API is known to be fully compatible with webext-oop,
    // except for events due to bugzil.la/1300234
    if (namespace == "test" && name != "onMessage") {
      shouldSynchronouslyUseParentAPI = false;
    }
    if (shouldSynchronouslyUseParentAPI) {
      let proxyContext = ParentAPIManager.proxyContexts.get(this.id);
      let apiObj = findPathInObject(proxyContext.apiObj, namespace, false);
      if (apiObj && name in apiObj) {
        return new LocalAPIImplementation(apiObj, name, this.context);
      }
      // If we got here, then it means that the JSON schema claimed that the API
      // will be available, but no actual implementation is given.
      // You should either provide an implementation or rewrite the JSON schema.
    }

    return super.getFallbackImplementation(namespace, name);
  }
}

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
this.ExtensionContext = class extends BaseContext {
  constructor(extension, params) {
    super("addon_child", extension);
    if (Services.appinfo.processType != Services.appinfo.PROCESS_TYPE_DEFAULT) {
      // This check is temporary. It should be removed once the proxy creation
      // is asynchronous.
      throw new Error("ExtensionContext cannot be created in child processes");
    }

    let {type, uri, contentWindow} = params;
    this.type = type;
    this.uri = uri || extension.baseURI;

    this.setContentWindow(contentWindow);

    // This is the MessageSender property passed to extension.
    // It can be augmented by the "page-open" hook.
    let sender = {id: extension.uuid};
    if (uri) {
      sender.url = uri.spec;
    }
    Management.emit("page-load", this, params, sender);

    let filter = {extensionId: extension.id};
    let optionalFilter = {};
    // Addon-generated messages (not necessarily from the same process as the
    // addon itself) are sent to the main process, which forwards them via the
    // parent process message manager. Specific replies can be sent to the frame
    // message manager.
    this.messenger = new Messenger(this, [Services.cpmm, this.messageManager], sender, filter, optionalFilter);

    let localApis = {};
    apiManager.generateAPIs(this, localApis);
    this.childManager = new WannabeChildAPIManager(this, this.messageManager, localApis, {
      envType: "addon_parent",
      viewType: type,
      url: uri.spec,
    });
    let chromeApiWrapper = Object.create(this.childManager);
    chromeApiWrapper.isChromeCompat = true;

    let browserObj = Cu.createObjectIn(contentWindow, {defineAs: "browser"});
    let chromeObj = Cu.createObjectIn(contentWindow, {defineAs: "chrome"});
    Schemas.inject(browserObj, this.childManager);
    Schemas.inject(chromeObj, chromeApiWrapper);

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
    this.childManager.close();

    if (this.externallyVisible) {
      this.extension.views.delete(this);
    }
  }
};


