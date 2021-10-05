/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* exported ExtensionChild */

var EXPORTED_SYMBOLS = ["ExtensionChild", "ExtensionActivityLogChild"];

/**
 * This file handles addon logic that is independent of the chrome process and
 * may run in all web content and extension processes.
 *
 * Don't put contentscript logic here, use ExtensionContent.jsm instead.
 */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "finalizationService",
  "@mozilla.org/toolkit/finalizationwitness;1",
  "nsIFinalizationWitnessService"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  ExtensionContent: "resource://gre/modules/ExtensionContent.jsm",
  ExtensionPageChild: "resource://gre/modules/ExtensionPageChild.jsm",
  ExtensionProcessScript: "resource://gre/modules/ExtensionProcessScript.jsm",
  NativeApp: "resource://gre/modules/NativeMessaging.jsm",
  PerformanceCounters: "resource://gre/modules/PerformanceCounters.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
});

// We're using the pref to avoid loading PerformanceCounters.jsm for nothing.
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gTimingEnabled",
  "extensions.webextensions.enablePerformanceCounters",
  false
);
const { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);
const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

const { DefaultMap, ExtensionError, LimitedSet, getUniqueId } = ExtensionUtils;

const {
  defineLazyGetter,
  EventEmitter,
  EventManager,
  LocalAPIImplementation,
  LocaleData,
  NoCloneSpreadArgs,
  SchemaAPIInterface,
  withHandlingUserInput,
} = ExtensionCommon;

const { sharedData } = Services.cpmm;

const MSG_SET_ENABLED = "Extension:ActivityLog:SetEnabled";
const MSG_LOG = "Extension:ActivityLog:DoLog";

const ExtensionActivityLogChild = {
  _initialized: false,
  enabledExtensions: new Set(),

  init() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    Services.cpmm.addMessageListener(MSG_SET_ENABLED, this);

    this.enabledExtensions = new Set(
      Services.cpmm.sharedData.get("extensions/logging")
    );
  },

  receiveMessage({ name, data }) {
    if (name === MSG_SET_ENABLED) {
      if (data.value) {
        this.enabledExtensions.add(data.id);
      } else {
        this.enabledExtensions.delete(data.id);
      }
    }
  },

  async log(context, type, name, data) {
    this.init();
    let { id } = context.extension;
    if (this.enabledExtensions.has(id)) {
      this._sendActivity({
        timeStamp: Date.now(),
        id,
        viewType: context.viewType,
        type,
        name,
        data,
        browsingContextId: context.browsingContextId,
      });
    }
  },

  _sendActivity(data) {
    Services.cpmm.sendAsyncMessage(MSG_LOG, data);
  },
};

// A helper to allow us to distinguish trusted errors from unsanitized errors.
// Extensions can create plain objects with arbitrary properties (such as
// mozWebExtLocation), but not create instances of ExtensionErrorHolder.
class ExtensionErrorHolder {
  constructor(trustedErrorObject) {
    this.trustedErrorObject = trustedErrorObject;
  }
}

/**
 * A finalization witness helper that wraps a sendMessage response and
 * guarantees to either get the promise resolved, or rejected when the
 * wrapped promise goes out of scope.
 */
const StrongPromise = {
  stillAlive: new Map(),

  wrap(promise, location) {
    let id = String(getUniqueId());
    let witness = finalizationService.make("extensions-onMessage-witness", id);

    return new Promise((resolve, reject) => {
      this.stillAlive.set(id, { reject, location });
      promise.then(resolve, reject).finally(() => {
        this.stillAlive.delete(id);
        witness.forget();
      });
    });
  },

  observe(subject, topic, id) {
    let message = "Promised response from onMessage listener went out of scope";
    let { reject, location } = this.stillAlive.get(id);
    reject(new ExtensionErrorHolder({ message, mozWebExtLocation: location }));
    this.stillAlive.delete(id);
  },
};
Services.obs.addObserver(StrongPromise, "extensions-onMessage-witness");

// Simple single-event emitter-like helper, exposes the EventManager api.
class SimpleEventAPI extends EventManager {
  constructor(context, name) {
    super({ context, name });
    this.fires = new Set();
    this.register = fire => {
      this.fires.add(fire);
      fire.location = context.getCaller();
      return () => this.fires.delete(fire);
    };
  }
  emit(...args) {
    return [...this.fires].map(fire => fire.asyncWithoutClone(...args));
  }
}

// runtime.OnMessage event helper, handles custom async/sendResponse logic.
class MessageEvent extends SimpleEventAPI {
  emit(holder, sender) {
    if (!this.fires.size || !this.context.active) {
      return { received: false };
    }

    sender = Cu.cloneInto(sender, this.context.cloneScope);
    let message = holder.deserialize(this.context.cloneScope);

    let responses = [...this.fires]
      .map(fire => this.wrapResponse(fire, message, sender))
      .filter(x => x !== undefined);

    return !responses.length
      ? { received: true, response: false }
      : Promise.race(responses).then(
          value => ({ response: true, value }),
          error => Promise.reject(this.unwrapOrSanitizeError(error))
        );
  }

  unwrapOrSanitizeError(error) {
    if (error instanceof ExtensionErrorHolder) {
      return error.trustedErrorObject;
    }
    // If not a wrapped error, sanitize it and convert to ExtensionError, so
    // that context.normalizeError will use the error message.
    return new ExtensionError(error?.message ?? "An unexpected error occurred");
  }

  wrapResponse(fire, message, sender) {
    let response, sendResponse;
    let promise = new Promise(resolve => {
      sendResponse = Cu.exportFunction(value => {
        resolve(value);
        response = promise;
      }, this.context.cloneScope);
    });

    let result;
    try {
      result = fire.raw(message, sender, sendResponse);
    } catch (e) {
      return Promise.reject(e);
    }
    if (
      result &&
      typeof result === "object" &&
      Cu.getClassName(result, true) === "Promise" &&
      this.context.principal.subsumes(Cu.getObjectPrincipal(result))
    ) {
      return StrongPromise.wrap(result, fire.location);
    } else if (result === true) {
      return StrongPromise.wrap(promise, fire.location);
    }
    return response;
  }
}

function holdMessage(data, native = null) {
  if (native && AppConstants.platform !== "android") {
    data = NativeApp.encodeMessage(native.context, data);
  }
  return new StructuredCloneHolder(data);
}

// Implements the runtime.Port extension API object.
class Port {
  /**
   * @param {BaseContext} context The context that owns this port.
   * @param {number} portId Uniquely identifies this port's channel.
   * @param {string} name Arbitrary port name as defined by the addon.
   * @param {boolean} native Is this a Port for native messaging.
   * @param {object} sender The `Port.sender` property.
   */
  constructor(context, portId, name, native, sender) {
    this.context = context;
    this.name = name;
    this.sender = sender;
    this.holdMessage = native ? data => holdMessage(data, this) : holdMessage;
    this.conduit = context.openConduit(this, {
      portId,
      native,
      source: !sender,
      recv: ["PortMessage", "PortDisconnect"],
      send: ["PortMessage"],
    });
    this.initEventManagers();
  }

  initEventManagers() {
    const { context } = this;
    this.onMessage = new SimpleEventAPI(context, "Port.onMessage");
    this.onDisconnect = new SimpleEventAPI(context, "Port.onDisconnect");
  }

  getAPI() {
    // Public Port object handed to extensions from `connect()` and `onConnect`.
    return {
      name: this.name,
      sender: this.sender,
      error: null,
      onMessage: this.onMessage.api(),
      onDisconnect: this.onDisconnect.api(),
      postMessage: this.sendPortMessage.bind(this),
      disconnect: () => this.conduit.close(),
    };
  }

  recvPortMessage({ holder }) {
    this.onMessage.emit(holder.deserialize(this.api), this.api);
  }

  recvPortDisconnect({ error = null }) {
    this.conduit.close();
    if (this.context.active) {
      this.api.error = error && this.context.normalizeError(error);
      this.onDisconnect.emit(this.api);
    }
  }

  sendPortMessage(json) {
    if (this.conduit.actor) {
      return this.conduit.sendPortMessage({ holder: this.holdMessage(json) });
    }
    throw new this.context.Error("Attempt to postMessage on disconnected port");
  }
}

defineLazyGetter(Port.prototype, "api", function() {
  let api = this.getAPI();
  return Cu.cloneInto(api, this.context.cloneScope, { cloneFunctions: true });
});

/**
 * Each extension context gets its own Messenger object. It handles the
 * basics of sendMessage, onMessage, connect and onConnect.
 */
class Messenger {
  constructor(context) {
    this.context = context;
    this.conduit = context.openConduit(this, {
      childId: context.childManager.id,
      query: ["NativeMessage", "RuntimeMessage", "PortConnect"],
      recv: ["RuntimeMessage", "PortConnect"],
    });
    this.initEventManagers();
  }

  initEventManagers() {
    const { context } = this;
    this.onConnect = new SimpleEventAPI(context, "runtime.onConnect");
    this.onConnectEx = new SimpleEventAPI(context, "runtime.onConnectExternal");
    this.onMessage = new MessageEvent(context, "runtime.onMessage");
    this.onMessageEx = new MessageEvent(context, "runtime.onMessageExternal");
  }

  sendNativeMessage(nativeApp, json) {
    let holder = holdMessage(json, this);
    return this.conduit.queryNativeMessage({ nativeApp, holder });
  }

  sendRuntimeMessage({ extensionId, message, callback, ...args }) {
    let response = this.conduit.queryRuntimeMessage({
      extensionId: extensionId || this.context.extension.id,
      holder: holdMessage(message),
      ...args,
    });
    // If |response| is a rejected promise, the value will be sanitized by
    // wrapPromise, according to the rules of context.normalizeError.
    return this.context.wrapPromise(response, callback);
  }

  connect({ name, native, ...args }) {
    let portId = getUniqueId();
    let port = new Port(this.context, portId, name, !!native);
    this.conduit
      .queryPortConnect({ portId, name, native, ...args })
      .catch(error => port.recvPortDisconnect({ error }));
    return port.api;
  }

  recvPortConnect({ extensionId, portId, name, sender }) {
    let event = sender.id === extensionId ? this.onConnect : this.onConnectEx;
    if (this.context.active && event.fires.size) {
      let port = new Port(this.context, portId, name, false, sender);
      return event.emit(port.api).length;
    }
  }

  recvRuntimeMessage({ extensionId, holder, sender }) {
    let event = sender.id === extensionId ? this.onMessage : this.onMessageEx;
    return event.emit(holder, sender);
  }
}

// For test use only.
var ExtensionManager = {
  extensions: new Map(),
};

// Represents a browser extension in the content process.
class BrowserExtensionContent extends EventEmitter {
  constructor(policy) {
    super();

    this.policy = policy;
    this.instanceId = policy.instanceId;
    this.optionalPermissions = policy.optionalPermissions;

    if (WebExtensionPolicy.isExtensionProcess) {
      Object.assign(this, this.getSharedData("extendedData"));
    }

    this.MESSAGE_EMIT_EVENT = `Extension:EmitEvent:${this.instanceId}`;
    Services.cpmm.addMessageListener(this.MESSAGE_EMIT_EVENT, this);

    let restrictSchemes = !this.hasPermission("mozillaAddons");

    this.apiManager = this.getAPIManager();

    this._manifest = null;
    this._localeData = null;

    this.baseURI = Services.io.newURI(`moz-extension://${this.uuid}/`);
    this.baseURL = this.baseURI.spec;

    this.principal = Services.scriptSecurityManager.createContentPrincipal(
      this.baseURI,
      {}
    );

    // Only used in addon processes.
    this.views = new Set();

    // Only used for devtools views.
    this.devtoolsViews = new Set();

    /* eslint-disable mozilla/balanced-listeners */
    this.on("add-permissions", (ignoreEvent, permissions) => {
      if (permissions.permissions.length) {
        let perms = new Set(this.policy.permissions);
        for (let perm of permissions.permissions) {
          perms.add(perm);
        }
        this.policy.permissions = perms;
      }

      if (permissions.origins.length) {
        let patterns = this.allowedOrigins.patterns.map(host => host.pattern);

        this.policy.allowedOrigins = new MatchPatternSet(
          [...patterns, ...permissions.origins],
          { restrictSchemes, ignorePath: true }
        );
      }
    });

    this.on("remove-permissions", (ignoreEvent, permissions) => {
      if (permissions.permissions.length) {
        let perms = new Set(this.policy.permissions);
        for (let perm of permissions.permissions) {
          perms.delete(perm);
        }
        this.policy.permissions = perms;
      }

      if (permissions.origins.length) {
        let origins = permissions.origins.map(
          origin => new MatchPattern(origin, { ignorePath: true }).pattern
        );

        this.policy.allowedOrigins = new MatchPatternSet(
          this.allowedOrigins.patterns.filter(
            host => !origins.includes(host.pattern)
          )
        );
      }
    });
    /* eslint-enable mozilla/balanced-listeners */

    ExtensionManager.extensions.set(this.id, this);
  }

  get id() {
    return this.policy.id;
  }

  get uuid() {
    return this.policy.mozExtensionHostname;
  }

  get permissions() {
    return new Set(this.policy.permissions);
  }

  get allowedOrigins() {
    return this.policy.allowedOrigins;
  }

  getSharedData(key, value) {
    return sharedData.get(`extension/${this.id}/${key}`);
  }

  get localeData() {
    if (!this._localeData) {
      this._localeData = new LocaleData(this.getSharedData("locales"));
    }
    return this._localeData;
  }

  get manifest() {
    if (!this._manifest) {
      this._manifest = this.getSharedData("manifest");
    }
    return this._manifest;
  }

  get manifestVersion() {
    return this.manifest.manifest_version;
  }

  get privateBrowsingAllowed() {
    return this.policy.privateBrowsingAllowed;
  }

  canAccessWindow(window) {
    return this.policy.canAccessWindow(window);
  }

  getAPIManager() {
    let apiManagers = [ExtensionPageChild.apiManager];

    if (this.dependencies) {
      for (let id of this.dependencies) {
        let extension = ExtensionProcessScript.getExtensionChild(id);
        if (extension) {
          apiManagers.push(extension.experimentAPIManager);
        }
      }
    }

    if (this.childModules) {
      this.experimentAPIManager = new ExtensionCommon.LazyAPIManager(
        "addon",
        this.childModules,
        this.schemaURLs
      );

      apiManagers.push(this.experimentAPIManager);
    }

    if (apiManagers.length == 1) {
      return apiManagers[0];
    }

    return new ExtensionCommon.MultiAPIManager("addon", apiManagers.reverse());
  }

  shutdown() {
    ExtensionManager.extensions.delete(this.id);
    ExtensionContent.shutdownExtension(this);
    Services.cpmm.removeMessageListener(this.MESSAGE_EMIT_EVENT, this);
    this.emit("shutdown");
  }

  getContext(window) {
    return ExtensionContent.getContext(this, window);
  }

  emit(event, ...args) {
    Services.cpmm.sendAsyncMessage(this.MESSAGE_EMIT_EVENT, { event, args });
    super.emit(event, ...args);
  }

  receiveMessage({ name, data }) {
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
    // If the permission is a "manifest property" permission, we check if the extension
    // does have the required property in its manifest.
    let manifest_ = "manifest:";
    if (perm.startsWith(manifest_)) {
      // Handle nested "manifest property" permission (e.g. as in "manifest:property.nested").
      let value = this.manifest;
      for (let prop of perm.substr(manifest_.length).split(".")) {
        if (!value) {
          break;
        }
        value = value[prop];
      }

      return value != null;
    }
    return this.permissions.has(perm);
  }
}

/**
 * An object that runs an remote implementation of an API.
 */
class ProxyAPIImplementation extends SchemaAPIInterface {
  /**
   * @param {string} namespace The full path to the namespace that contains the
   *     `name` member. This may contain dots, e.g. "storage.local".
   * @param {string} name The name of the method or property.
   * @param {ChildAPIManager} childApiManager The owner of this implementation.
   * @param {boolean} alreadyLogged Whether the child already logged the event.
   */
  constructor(namespace, name, childApiManager, alreadyLogged = false) {
    super();
    this.path = `${namespace}.${name}`;
    this.childApiManager = childApiManager;
    this.alreadyLogged = alreadyLogged;
  }

  revoke() {
    let map = this.childApiManager.listeners.get(this.path);
    for (let listener of map.listeners.keys()) {
      this.removeListener(listener);
    }

    this.path = null;
    this.childApiManager = null;
  }

  callFunctionNoReturn(args) {
    this.childApiManager.callParentFunctionNoReturn(this.path, args);
  }

  callAsyncFunction(args, callback, requireUserInput) {
    if (requireUserInput) {
      let context = this.childApiManager.context;
      if (!context.contentWindow.windowUtils.isHandlingUserInput) {
        let err = new context.cloneScope.Error(
          `${this.path} may only be called from a user input handler`
        );
        return context.wrapPromise(Promise.reject(err), callback);
      }
    }
    return this.childApiManager.callParentAsyncFunction(
      this.path,
      args,
      callback,
      { alreadyLogged: this.alreadyLogged }
    );
  }

  addListener(listener, args) {
    let map = this.childApiManager.listeners.get(this.path);

    if (map.listeners.has(listener)) {
      // TODO: Called with different args?
      return;
    }

    let id = getUniqueId();

    map.ids.set(id, listener);
    map.listeners.set(listener, id);

    this.childApiManager.conduit.sendAddListener({
      childId: this.childApiManager.id,
      listenerId: id,
      path: this.path,
      args,
      alreadyLogged: this.alreadyLogged,
    });
  }

  removeListener(listener) {
    let map = this.childApiManager.listeners.get(this.path);

    if (!map.listeners.has(listener)) {
      return;
    }

    let id = map.listeners.get(listener);
    map.listeners.delete(listener);
    map.ids.delete(id);
    map.removedIds.add(id);

    this.childApiManager.conduit.sendRemoveListener({
      childId: this.childApiManager.id,
      listenerId: id,
      path: this.path,
      alreadyLogged: this.alreadyLogged,
    });
  }

  hasListener(listener) {
    let map = this.childApiManager.listeners.get(this.path);
    return map.listeners.has(listener);
  }
}

class ChildLocalAPIImplementation extends LocalAPIImplementation {
  constructor(pathObj, namespace, name, childApiManager) {
    super(pathObj, name, childApiManager.context);
    this.childApiManagerId = childApiManager.id;
    this.fullname = `${namespace}.${name}`;
  }

  /**
   * Call the given function and also log the call as appropriate
   * (i.e., with PerformanceCounters and/or activity logging)
   *
   * @param {function} callable The actual implementation to invoke.
   * @param {array} args Arguments to the function call.
   * @returns {any} The return result of callable.
   */
  callAndLog(callable, args) {
    this.context.logActivity("api_call", this.fullname, { args });
    let start = Cu.now();
    try {
      return callable();
    } finally {
      ChromeUtils.addProfilerMarker(
        "ExtensionChild",
        { startTime: start },
        `${this.context.extension.id}, api_call: ${this.fullname}`
      );
      if (gTimingEnabled) {
        let end = Cu.now() * 1000;
        PerformanceCounters.storeExecutionTime(
          this.context.extension.id,
          this.name,
          end - start * 1000,
          this.childApiManagerId
        );
      }
    }
  }

  callFunction(args) {
    return this.callAndLog(() => super.callFunction(args), args);
  }

  callFunctionNoReturn(args) {
    return this.callAndLog(() => super.callFunctionNoReturn(args), args);
  }

  callAsyncFunction(args, callback, requireUserInput) {
    return this.callAndLog(
      () => super.callAsyncFunction(args, callback, requireUserInput),
      args
    );
  }
}

// We create one instance of this class for every extension context that
// needs to use remote APIs. It uses the the JSWindowActor and
// JSProcessActor Conduits actors (see ConduitsChild.jsm) to communicate
// with the ParentAPIManager singleton in ExtensionParent.jsm.
// It handles asynchronous function calls as well as event listeners.
class ChildAPIManager {
  constructor(context, messageManager, localAPICan, contextData) {
    this.context = context;
    this.messageManager = messageManager;
    this.url = contextData.url;

    // The root namespace of all locally implemented APIs. If an extension calls
    // an API that does not exist in this object, then the implementation is
    // delegated to the ParentAPIManager.
    this.localApis = localAPICan.root;
    this.apiCan = localAPICan;
    this.schema = this.apiCan.apiManager.schema;

    this.id = `${context.extension.id}.${context.contextId}`;

    this.conduit = context.openConduit(this, {
      childId: this.id,
      send: ["CreateProxyContext", "APICall", "AddListener", "RemoveListener"],
      recv: ["CallResult", "RunListener"],
    });

    this.conduit.sendCreateProxyContext({
      childId: this.id,
      extensionId: context.extension.id,
      principal: context.principal,
      ...contextData,
    });

    this.listeners = new DefaultMap(() => ({
      ids: new Map(),
      listeners: new Map(),
      removedIds: new LimitedSet(10),
    }));

    // Map[callId -> Deferred]
    this.callPromises = new Map();

    this.permissionsChangedCallbacks = new Set();
    this.updatePermissions = null;
    if (this.context.extension.optionalPermissions.length) {
      this.updatePermissions = () => {
        for (let callback of this.permissionsChangedCallbacks) {
          try {
            callback();
          } catch (err) {
            Cu.reportError(err);
          }
        }
      };
      this.context.extension.on("add-permissions", this.updatePermissions);
      this.context.extension.on("remove-permissions", this.updatePermissions);
    }
  }

  inject(obj) {
    this.schema.inject(obj, this);
  }

  recvCallResult(data) {
    let deferred = this.callPromises.get(data.callId);
    this.callPromises.delete(data.callId);
    if ("error" in data) {
      deferred.reject(data.error);
    } else {
      let result = data.result.deserialize(this.context.cloneScope);

      deferred.resolve(new NoCloneSpreadArgs(result));
    }
  }

  recvRunListener(data) {
    let map = this.listeners.get(data.path);
    let listener = map.ids.get(data.listenerId);

    if (listener) {
      let args = data.args.deserialize(this.context.cloneScope);
      let fire = () => this.context.applySafeWithoutClone(listener, args);
      return Promise.resolve(
        data.handlingUserInput
          ? withHandlingUserInput(this.context.contentWindow, fire)
          : fire()
      ).then(result => {
        if (result !== undefined) {
          return new StructuredCloneHolder(result, this.context.cloneScope);
        }
        return result;
      });
    }
    if (!map.removedIds.has(data.listenerId)) {
      Services.console.logStringMessage(
        `Unknown listener at childId=${data.childId} path=${data.path} listenerId=${data.listenerId}\n`
      );
    }
  }

  /**
   * Call a function in the parent process and ignores its return value.
   *
   * @param {string} path The full name of the method, e.g. "tabs.create".
   * @param {Array} args The parameters for the function.
   */
  callParentFunctionNoReturn(path, args) {
    this.conduit.sendAPICall({ childId: this.id, path, args });
  }

  /**
   * Calls a function in the parent process and returns its result
   * asynchronously.
   *
   * @param {string} path The full name of the method, e.g. "tabs.create".
   * @param {Array} args The parameters for the function.
   * @param {function(*)} [callback] The callback to be called when the function
   *     completes.
   * @param {object} [options] Extra options.
   * @returns {Promise|undefined} Must be void if `callback` is set, and a
   *     promise otherwise. The promise is resolved when the function completes.
   */
  callParentAsyncFunction(path, args, callback, options = {}) {
    let callId = getUniqueId();
    let deferred = PromiseUtils.defer();
    this.callPromises.set(callId, deferred);

    // Any child api that calls into a parent function will have already
    // logged the api_call.  Flag it so the parent doesn't log again.
    let { alreadyLogged = true } = options;

    // TODO: conduit.queryAPICall()
    this.conduit.sendAPICall({
      childId: this.id,
      callId,
      path,
      args,
      options: { alreadyLogged },
    });
    return this.context.wrapPromise(deferred.promise, callback);
  }

  /**
   * Create a proxy for an event in the parent process. The returned event
   * object shares its internal state with other instances. For instance, if
   * `removeListener` is used on a listener that was added on another object
   * through `addListener`, then the event is unregistered.
   *
   * @param {string} path The full name of the event, e.g. "tabs.onCreated".
   * @returns {object} An object with the addListener, removeListener and
   *   hasListener methods. See SchemaAPIInterface for documentation.
   */
  getParentEvent(path) {
    path = path.split(".");

    let name = path.pop();
    let namespace = path.join(".");

    let impl = new ProxyAPIImplementation(namespace, name, this, true);
    return {
      addListener: (listener, ...args) => impl.addListener(listener, args),
      removeListener: listener => impl.removeListener(listener),
      hasListener: listener => impl.hasListener(listener),
    };
  }

  close() {
    // Reports CONDUIT_CLOSED on the parent side.
    this.conduit.close();

    if (this.updatePermissions) {
      this.context.extension.off("add-permissions", this.updatePermissions);
      this.context.extension.off("remove-permissions", this.updatePermissions);
    }
  }

  get cloneScope() {
    return this.context.cloneScope;
  }

  get principal() {
    return this.context.principal;
  }

  get manifestVersion() {
    return this.context.manifestVersion;
  }

  shouldInject(namespace, name, allowedContexts) {
    // Do not generate content script APIs, unless explicitly allowed.
    if (
      this.context.envType === "content_child" &&
      !allowedContexts.includes("content")
    ) {
      return false;
    }

    // Do not generate devtools APIs, unless explicitly allowed.
    if (
      this.context.envType === "devtools_child" &&
      !allowedContexts.includes("devtools")
    ) {
      return false;
    }

    // Do not generate devtools APIs, unless explicitly allowed.
    if (
      this.context.envType !== "devtools_child" &&
      allowedContexts.includes("devtools_only")
    ) {
      return false;
    }

    // Do not generate content_only APIs, unless explicitly allowed.
    if (
      this.context.envType !== "content_child" &&
      allowedContexts.includes("content_only")
    ) {
      return false;
    }

    return true;
  }

  getImplementation(namespace, name) {
    this.apiCan.findAPIPath(`${namespace}.${name}`);
    let obj = this.apiCan.findAPIPath(namespace);

    if (obj && name in obj) {
      return new ChildLocalAPIImplementation(obj, namespace, name, this);
    }

    return this.getFallbackImplementation(namespace, name);
  }

  getFallbackImplementation(namespace, name) {
    // No local API found, defer implementation to the parent.
    return new ProxyAPIImplementation(namespace, name, this);
  }

  hasPermission(permission) {
    return this.context.extension.hasPermission(permission);
  }

  isPermissionRevokable(permission) {
    return this.context.extension.optionalPermissions.includes(permission);
  }

  setPermissionsChangedCallback(callback) {
    this.permissionsChangedCallbacks.add(callback);
  }
}

/**
 * SimpleEventAPI subclass specialized for the worker port events
 * used by WorkerMessenger.
 */
class WorkerRuntimePortEvent extends SimpleEventAPI {
  api() {
    return {
      ...super.api(),
      createListenerForAPIRequest: (...args) =>
        this.createListenerForAPIRequest(...args),
    };
  }

  createListenerForAPIRequest(request) {
    const { eventListener } = request;
    return function(port, ...args) {
      return eventListener.callListener(args, {
        apiObjectType: Ci.mozIExtensionListenerCallOptions.RUNTIME_PORT,
        apiObjectDescriptor: { portId: port.portId, name: port.name },
      });
    };
  }
}

/**
 * SimpleEventAPI subclass specialized for the worker runtime messaging events
 * used by WorkerMessenger.
 */
class WorkerMessageEvent extends MessageEvent {
  api() {
    return {
      ...super.api(),
      createListenerForAPIRequest: (...args) =>
        this.createListenerForAPIRequest(...args),
    };
  }

  createListenerForAPIRequest(request) {
    const { eventListener } = request;
    return function(message, sender) {
      return eventListener.callListener([message, sender], {
        eventListenerType:
          Ci.mozIExtensionListenerCallOptions.CALLBACK_SEND_RESPONSE,
      });
    };
  }
}

/**
 * MessageEvent subclass specialized for the worker's port API events
 * used by WorkerPort.
 */
class WorkerPortEvent extends SimpleEventAPI {
  api() {
    return {
      ...super.api(),
      createListenerForAPIRequest: (...args) =>
        this.createListenerForAPIRequest(...args),
    };
  }

  createListenerForAPIRequest(request) {
    const { eventListener } = request;
    switch (this.name) {
      case "Port.onDisconnect":
        return function(port) {
          eventListener.callListener([], {
            apiObjectType: Ci.mozIExtensionListenerCallOptions.RUNTIME_PORT,
            apiObjectDescriptor: {
              portId: port.portId,
              name: port.name,
            },
          });
        };
      case "Port.onMessage":
        return function(message, port) {
          eventListener.callListener([message], {
            apiObjectType: Ci.mozIExtensionListenerCallOptions.RUNTIME_PORT,
            apiObjectDescriptor: {
              portId: port.portId,
              name: port.name,
            },
          });
        };
    }
    return undefined;
  }
}

/**
 * Port subclass specialized for the workers and used by WorkerMessager.
 */
class WorkerPort extends Port {
  constructor(context, portId, name, native, sender) {
    const { viewType, contextId } = context;
    if (viewType !== "background_worker") {
      throw new Error(
        `Unexpected viewType "${viewType}" on context ${contextId}`
      );
    }

    super(context, portId, name, native, sender);
    this.portId = portId;
  }

  initEventManagers() {
    const { context } = this;
    this.onMessage = new WorkerPortEvent(context, "Port.onMessage");
    this.onDisconnect = new WorkerPortEvent(context, "Port.onDisconnect");
  }

  getAPI() {
    const api = super.getAPI();
    // Add the portId to the API object, needed by the WorkerMessenger
    // to retrieve the port given the apiObjectId part of the
    // mozIExtensionAPIRequest sent from the ExtensionPort webidl.
    api.portId = this.portId;
    return api;
  }
}

defineLazyGetter(WorkerPort.prototype, "api", function() {
  // No need to clone the API object for the worker, because it runs
  // on a different JSRuntime and doesn't have direct access to this
  // object.
  return this.getAPI();
});

/**
 * A Messenger subclass specialized for the background service worker.
 */
class WorkerMessenger extends Messenger {
  constructor(context) {
    const { viewType, contextId } = context;
    if (viewType !== "background_worker") {
      throw new Error(
        `Unexpected viewType "${viewType}" on context ${contextId}`
      );
    }

    super(context);

    // Used by WebIDL API requests to get a port instance given the apiObjectId
    // received in the API request coming from the ExtensionPort instance
    // returned in the thread where the request was originating from.
    this.portsById = new Map();
    this.context.callOnClose(this);
  }

  initEventManagers() {
    const { context } = this;
    this.onConnect = new WorkerRuntimePortEvent(context, "runtime.onConnect");
    this.onConnectEx = new WorkerRuntimePortEvent(
      context,
      "runtime.onConnectExternal"
    );
    this.onMessage = new WorkerMessageEvent(this.context, "runtime.onMessage");
    this.onMessageEx = new WorkerMessageEvent(
      context,
      "runtime.onMessageExternal"
    );
  }

  close() {
    this.portsById.clear();
  }

  getPortById(portId) {
    return this.portsById.get(portId);
  }

  connect({ name, native, ...args }) {
    let portId = getUniqueId();
    let port = new WorkerPort(this.context, portId, name, !!native);
    this.conduit
      .queryPortConnect({ portId, name, native, ...args })
      .catch(error => port.recvPortDisconnect({ error }));
    this.portsById.set(`${portId}`, port);
    // Extension worker calls this method through the WebIDL bindings,
    // and the Port instance returned by the runtime.connect/connectNative
    // methods will be an instance of ExtensionPort webidl interface based
    // on the ExtensionPortDescriptor dictionary returned by this method.
    return { portId, name };
  }

  recvPortConnect({ extensionId, portId, name, sender }) {
    let event = sender.id === extensionId ? this.onConnect : this.onConnectEx;
    if (this.context.active && event.fires.size) {
      let port = new WorkerPort(this.context, portId, name, false, sender);
      this.portsById.set(`${port.portId}`, port);
      return event.emit(port).length;
    }
  }
}

/**
 * APIImplementation subclass specialized for handling mozIExtensionAPIRequests
 * originated from webidl bindings.
 *
 * Provides a createListenerForAPIRequest method which is used by
 * WebIDLChildAPIManager to retrieve an API event specific wrapper
 * for the mozIExtensionEventListener for the API events that needs
 * special handling (e.g. runtime.onConnect).
 *
 * createListenerForAPIRequest delegates to the API event the creation
 * of the special event listener wrappers, the EventManager api objects
 * for the events that needs special wrapper are expected to provide
 * a method with the same name.
 */
class ChildLocalWebIDLAPIImplementation extends ChildLocalAPIImplementation {
  constructor(pathObj, namespace, name, childApiManager) {
    super(pathObj, namespace, name, childApiManager);
    this.childApiManager = childApiManager;
  }

  createListenerForAPIRequest(request) {
    return this.pathObj[this.name].createListenerForAPIRequest?.(request);
  }

  setProperty() {
    // mozIExtensionAPIRequest doesn't support this requestType at the moment,
    // setting a pref would just replace the previous value on the wrapper
    // object living in the owner thread.
    // To be implemented if we have an actual use case where that is needed.
    throw new Error("Unexpected call to setProperty");
  }

  hasListener(listener) {
    // hasListener is implemented in C++ by ExtensionEventManager, and so
    // a call to this method is unexpected.
    throw new Error("Unexpected call to hasListener");
  }
}

/**
 * APIImplementation subclass specialized for handling API requests related
 * to an API Object type.
 *
 * Retrieving the apiObject instance is delegated internally to the
 * ExtensionAPI subclass that implements the request apiNamespace,
 * through an optional getAPIObjectForRequest method expected to be
 * available on the ExtensionAPI class.
 */
class ChildWebIDLObjectTypeImplementation extends ChildLocalWebIDLAPIImplementation {
  constructor(request, childApiManager) {
    const { apiNamespace, apiName, apiObjectType, apiObjectId } = request;
    const api = childApiManager.getExtensionAPIInstance(apiNamespace);
    const pathObj = api.getAPIObjectForRequest?.(
      childApiManager.context,
      request
    );
    if (!pathObj) {
      throw new Error(`apiObject instance not found for ${request}`);
    }
    super(pathObj, apiNamespace, apiName, childApiManager);
    this.fullname = `${apiNamespace}.${apiObjectType}(${apiObjectId}).${apiName}`;
  }
}

/**
 * A ChildAPIManager subclass specialized for handling mozIExtensionAPIRequest
 * originated from the WebIDL bindings.
 *
 * Currently used only for the extension contexts related to the background
 * service worker.
 */
class WebIDLChildAPIManager extends ChildAPIManager {
  constructor(...args) {
    super(...args);
    // Map<apiPathToEventString, WeakMap<nsIExtensionEventListener, Function>>
    //
    // apiPathToEventString is a string that represents the full API path
    // related to the event name (e.g. "runtime.onConnect", or "runtime.Port.onMessage")
    this.eventListenerWrappers = new DefaultMap(() => new WeakMap());
  }

  getImplementation(namespace, name) {
    this.apiCan.findAPIPath(`${namespace}.${name}`);
    let obj = this.apiCan.findAPIPath(namespace);

    if (obj && name in obj) {
      return new ChildLocalWebIDLAPIImplementation(obj, namespace, name, this);
    }

    return this.getFallbackImplementation(namespace, name);
  }

  getImplementationForRequest(request) {
    const { apiNamespace, apiName, apiObjectType } = request;
    if (apiObjectType) {
      return new ChildWebIDLObjectTypeImplementation(request, this);
    }
    return this.getImplementation(apiNamespace, apiName);
  }

  /**
   * Handles an ExtensionAPIRequest originated by the Extension APIs WebIDL bindings.
   *
   * @param {mozIExtensionAPIRequest} request
   *        The object that represents the API request received
   *        (including arguments, an event listener wrapper etc)
   *
   * @returns {mozIExtensionAPIRequestResult}
   *          Result for the API request, either a value to be returned
   *          (which has to be a value that can be structure cloned
   *          if the request was originated from the worker thread) or
   *          an error to raise to the extension code.
   */
  handleWebIDLAPIRequest(request) {
    try {
      const impl = this.getImplementationForRequest(request);
      let result;
      this.context.withAPIRequest(request, () => {
        if (impl instanceof ProxyAPIImplementation) {
          result = this.handleForProxyAPIImplementation(request, impl);
        } else {
          result = this.callAPIImplementation(request, impl);
        }
      });

      return {
        type: Ci.mozIExtensionAPIRequestResult.RETURN_VALUE,
        value: result,
      };
    } catch (error) {
      return this.handleExtensionError(error);
    }
  }

  /**
   * Convert an error raised while handling an API request,
   * into the expected mozIExtensionAPIRequestResult.
   *
   * @param {Error | WorkerExtensionError} error
   * @returns {mozIExtensionAPIRequestResult}
   */

  handleExtensionError(error) {
    // Propagate an extension error to the caller on the worker thread.
    if (error instanceof this.context.Error) {
      return {
        type: Ci.mozIExtensionAPIRequestResult.EXTENSION_ERROR,
        value: error,
      };
    }

    // Otherwise just log it and throw a generic error.
    Cu.reportError(error);
    return {
      type: Ci.mozIExtensionAPIRequestResult.EXTENSION_ERROR,
      value: new this.context.Error("An unexpected error occurred"),
    };
  }

  /**
   * Handle the given mozIExtensionAPIRequest using the given
   * APIImplementation instance.
   *
   * @param {mozIExtensionAPIRequest} request
   * @param {ChildLocalWebIDLAPIImplementation | ProxyAPIImplementation} impl
   * @returns {any}
   * @throws {Error | WorkerExtensionError}
   */

  callAPIImplementation(request, impl) {
    const { requestType, normalizedArgs } = request;

    switch (requestType) {
      // TODO (Bug 1728328): follow up to take callAsyncFunction requireUserInput
      // parameter into account (until then callAsyncFunction, callFunction
      // and callFunctionNoReturn calls do not differ yet).
      case "callAsyncFunction":
      case "callFunction":
      case "callFunctionNoReturn":
      case "getProperty":
        return impl[requestType](normalizedArgs);
      case "addListener": {
        const listener = this.getOrCreateListenerWrapper(request, impl);
        impl.addListener(listener, normalizedArgs);

        return undefined;
      }
      case "removeListener": {
        const listener = this.getListenerWrapper(request);
        if (listener) {
          // Remove the previously added listener and forget the cleanup
          // observer previously passed to context.callOnClose.
          listener._callOnClose.close();
          this.contet.forgetOnclose(listener._callOnClose);
          this.forgetListenerWrapper(request);
        }
        return undefined;
      }
      default:
        throw new Error(
          `Unexpected requestType ${requestType} while handling "${request}"`
        );
    }
  }

  /**
   * Handle the given mozIExtensionAPIRequest using the given
   * ProxyAPIImplementation instance.
   *
   * @param {mozIExtensionAPIRequest} request
   * @param {ProxyAPIImplementation} impl
   * @returns {any}
   * @throws {Error | WorkerExtensionError}
   */

  handleForProxyAPIImplementation(request, impl) {
    const { requestType } = request;
    switch (requestType) {
      case "callAsyncFunction":
      case "callFunctionNoReturn":
      case "addListener":
      case "removeListener":
        return this.callAPIImplementation(request, impl);
      default:
        // Any other request types (e.g. getProperty or callFunction) are
        // unexpected and so we raise a more detailed error to be logged
        // on the browser console (while the extension will receive the
        // generic "An unexpected error occurred" one).
        throw new Error(
          `Unexpected requestType ${requestType} while handling "${request}"`
        );
    }
  }

  getAPIPathForWebIDLRequest(request) {
    const { apiNamespace, apiName, apiObjectType } = request;
    if (apiObjectType) {
      return `${apiNamespace}.${apiObjectType}.${apiName}`;
    }

    return `${apiNamespace}.${apiName}`;
  }

  /**
   * Return an ExtensionAPI class instance given its namespace.
   *
   * @param {string} namespace
   * @returns {ExtensionAPI}
   */
  getExtensionAPIInstance(namespace) {
    return this.apiCan.apis.get(namespace);
  }

  getOrCreateListenerWrapper(request, impl) {
    let listener = this.getListenerWrapper(request);
    if (listener) {
      return listener;
    }

    // Look for special wrappers that are needed for some API events
    // (e.g. runtime.onMessage/onConnect/...).
    if (impl instanceof ChildLocalWebIDLAPIImplementation) {
      listener = impl.createListenerForAPIRequest(request);
    }

    const { eventListener } = request;
    listener =
      listener ??
      function(...args) {
        // Default wrapper just forwards all the arguments to the
        // extension callback (all arguments has to be structure cloneable
        // if the extension callback is on the worker thread).
        eventListener.callListener(args);
      };
    listener._callOnClose = {
      close: () => {
        this.eventListenerWrappers.delete(eventListener);
        // Failing to send the request to remove the listener in the parent
        // process shouldn't prevent the extension or context shutdown,
        // otherwise we would leak a WebExtensionPolicy instance.
        try {
          impl.removeListener(listener);
        } catch (err) {
          // Removing a listener when the extension context is being closed can
          // fail if the API is proxied to the parent process and the conduit
          // has been already closed, and so we ignore the error if we are not
          // processing a call proxied to the parent process.
          if (impl instanceof ChildLocalWebIDLAPIImplementation) {
            Cu.reportError(err);
          }
        }
      },
    };
    this.storeListenerWrapper(request, listener);
    this.context.callOnClose(listener._callOnClose);
    return listener;
  }

  getListenerWrapper(request) {
    const { eventListener } = request;
    if (!(eventListener instanceof Ci.mozIExtensionEventListener)) {
      throw new Error(`Unexpected eventListener type for request: ${request}`);
    }
    const apiPath = this.getAPIPathForWebIDLRequest(request);
    if (!this.eventListenerWrappers.has(apiPath)) {
      return undefined;
    }
    return this.eventListenerWrappers.get(apiPath).get(eventListener);
  }

  storeListenerWrapper(request, listener) {
    const { eventListener } = request;
    if (!(eventListener instanceof Ci.mozIExtensionEventListener)) {
      throw new Error(`Missing eventListener for request: ${request}`);
    }
    const apiPath = this.getAPIPathForWebIDLRequest(request);
    this.eventListenerWrappers.get(apiPath).set(eventListener, listener);
  }

  forgetListenerWrapper(request) {
    const { eventListener } = request;
    if (!(eventListener instanceof Ci.mozIExtensionEventListener)) {
      throw new Error(`Missing eventListener for request: ${request}`);
    }
    const apiPath = this.getAPIPathForWebIDLRequest(request);
    if (this.eventListenerWrappers.has(apiPath)) {
      this.eventListenerWrappers.get(apiPath).delete(eventListener);
    }
  }
}

var ExtensionChild = {
  BrowserExtensionContent,
  ChildAPIManager,
  Messenger,
  WebIDLChildAPIManager,
  WorkerMessenger,
};
