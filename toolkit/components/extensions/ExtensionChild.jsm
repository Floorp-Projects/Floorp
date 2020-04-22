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
  MessageChannel: "resource://gre/modules/MessageChannel.jsm",
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

const { DefaultMap, LimitedSet, getUniqueId, getWinUtils } = ExtensionUtils;

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

const isContentProcess =
  Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;

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

/**
 * A finalization witness helper that wraps a sendMessage response and
 * guarantees to either get the promise resolved, or rejected when the
 * wrapped promise goes out of scope.
 *
 * Holding a reference to a returned StrongPromise doesn't prevent the
 * wrapped promise from being garbage collected.
 */
const StrongPromise = {
  locations: new Map(),

  wrap(promise, channelId, location) {
    return new Promise((resolve, reject) => {
      this.locations.set(channelId, location);

      const witness = finalizationService.make(
        "extensions-sendMessage-witness",
        channelId
      );
      promise.then(
        value => {
          this.locations.delete(channelId);
          witness.forget();
          resolve(value);
        },
        error => {
          this.locations.delete(channelId);
          witness.forget();
          reject(error);
        }
      );
    });
  },
  observe(subject, topic, channelId) {
    channelId = Number(channelId);
    let location = this.locations.get(channelId);
    this.locations.delete(channelId);

    const message = `Promised response from onMessage listener went out of scope`;
    const error = ChromeUtils.createError(message, location);
    error.mozWebExtLocation = location;
    MessageChannel.abortChannel(channelId, error);
  },
};
Services.obs.addObserver(StrongPromise, "extensions-sendMessage-witness");

// Simple single-event emitter-like helper, exposes the EventManager api.
class SimpleEventAPI extends EventManager {
  constructor(context, name) {
    super({ context, name });
    this.fires = new Set();
    this.register = fire => {
      this.fires.add(fire);
      return () => this.fires.delete(fire);
    };
  }
  emit(...args) {
    return [...this.fires].map(fire => fire.asyncWithoutClone(...args));
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
    this.holdMessage = native ? data => holdMessage(data, this) : holdMessage;

    this.conduit = context.openConduit(this, {
      portId,
      native,
      source: !sender,
      recv: ["PortMessage", "PortDisconnect"],
      send: ["PortMessage"],
    });

    this.onMessage = new SimpleEventAPI(context, "Port.onMessage");
    this.onDisconnect = new SimpleEventAPI(context, "Port.onDisconnect");

    // Public Port object handed to extensions from `connect()` and `onConnect`.
    let api = {
      name,
      sender,
      error: null,
      onMessage: this.onMessage.api(),
      onDisconnect: this.onDisconnect.api(),
      postMessage: this.sendPortMessage.bind(this),
      disconnect: () => this.conduit.close(),
    };
    this.api = Cu.cloneInto(api, context.cloneScope, { cloneFunctions: true });
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

// Handles native messaging for a context, similar to the Messenger below.
class NativeMessenger {
  constructor(context, sender) {
    this.context = context;
    this.conduit = context.openConduit(this, {
      url: sender.url,
      frameId: sender.frameId,
      childId: context.childManager.id,
      query: ["NativeMessage", "PortConnect"],
      recv: ["PortConnect"],
    });

    this.onConnect = new SimpleEventAPI(context, "runtime.onConnect");
    this.onConnectEx = new SimpleEventAPI(context, "runtime.onConnectExternal");
  }

  sendNativeMessage(nativeApp, json) {
    let holder = holdMessage(json, this);
    return this.conduit.queryNativeMessage({ nativeApp, holder });
  }

  connect({ name, native, ...args }) {
    let portId = getUniqueId();
    let port = new Port(this.context, portId, name, !!native);
    this.conduit
      .queryPortConnect({ portId, name, native, ...args })
      .catch(error => port.recvPortDisconnect({ error }));
    return port.api;
  }

  recvPortConnect({ portId, name, sender }) {
    let ex = sender.id === this.context.extension.id;
    let event = ex ? this.onConnect : this.onConnectEx;
    if (this.context.active && event.fires.size) {
      let port = new Port(this.context, portId, name, false, sender);
      return event.emit(port.api).length;
    }
  }
}

/**
 * Each extension context gets its own Messenger object. It handles the
 * basics of sendMessage, onMessage, connect and onConnect.
 *
 * @param {BaseContext} context The context to which this Messenger is tied.
 * @param {Array<nsIMessageListenerManager>} messageManagers
 *     The message managers used to receive messages (e.g. onMessage/onConnect
 *     requests).
 * @param {object} sender Describes this sender to the recipient. This object
 *     is extended further by BaseContext's sendMessage method and appears as
 *     the `sender` object to `onConnect` and `onMessage`.
 *     Do not set the `extensionId`, `contextId` or `tab` properties. The former
 *     two are added by BaseContext's sendMessage, while `sender.tab` is set by
 *     the ProxyMessenger in the main process.
 * @param {object} filter A recipient filter to apply to incoming messages from
 *     the broker. Messages are only handled by this Messenger if all key-value
 *     pairs match the `recipient` as specified by the sender of the message.
 *     In other words, this filter defines the required fields of `recipient`.
 * @param {object} [optionalFilter] An additional filter to apply to incoming
 *     messages. Unlike `filter`, the keys from `optionalFilter` are allowed to
 *     be omitted from `recipient`. Only keys that are present in both
 *     `optionalFilter` and `recipient` are applied to filter incoming messages.
 */
class Messenger {
  constructor(context, messageManagers, sender, filter, optionalFilter) {
    this.context = context;
    this.messageManagers = messageManagers;
    this.sender = sender;
    this.filter = filter;
    this.optionalFilter = optionalFilter;

    // Include the context envType in the sender info.
    this.sender.envType = context.envType;

    // Exclude messages coming from content scripts for the devtools extension contexts
    // (See Bug 1383310).
    this.excludeContentScriptSender = this.context.envType === "devtools_child";
  }

  _sendMessage(messageManager, message, data, recipient) {
    let options = {
      recipient,
      sender: this.sender,
      responseType: MessageChannel.RESPONSE_FIRST,
    };

    return this.context.sendMessage(messageManager, message, data, options);
  }

  sendMessage(messageManager, msg, recipient, responseCallback) {
    let holder = new StructuredCloneHolder(msg);

    let promise = this._sendMessage(
      messageManager,
      "Extension:Message",
      holder,
      recipient
    ).catch(error => {
      if (error.result == MessageChannel.RESULT_NO_HANDLER) {
        return Promise.reject({
          message:
            "Could not establish connection. Receiving end does not exist.",
        });
      } else if (error.result != MessageChannel.RESULT_NO_RESPONSE) {
        return Promise.reject(error);
      }
    });
    holder = null;

    return this.context.wrapPromise(promise, responseCallback);
  }

  _onMessage(name, filter) {
    return new EventManager({
      context: this.context,
      name,
      register: fire => {
        const caller = this.context.getCaller();

        let listener = {
          messageFilterPermissive: this.optionalFilter,
          messageFilterStrict: this.filter,

          filterMessage: (sender, recipient) => {
            // Exclude messages coming from content scripts for the devtools extension contexts
            // (See Bug 1383310).
            if (
              this.excludeContentScriptSender &&
              sender.envType === "content_child"
            ) {
              return false;
            }

            // Ignore the message if it was sent by this Messenger.
            return (
              sender.contextId !== this.context.contextId &&
              filter(sender, recipient)
            );
          },

          receiveMessage: (
            { target, data: holder, sender, recipient, channelId },
            isLastHandler
          ) => {
            if (!this.context.active) {
              return;
            }

            let sendResponse;
            let response = undefined;
            let promise = new Promise(resolve => {
              sendResponse = value => {
                resolve(value);
                response = promise;
              };
            });

            let message = holder.deserialize(
              this.context.cloneScope,
              !isLastHandler
            );
            holder = null;

            sender = Cu.cloneInto(sender, this.context.cloneScope);
            sendResponse = Cu.exportFunction(
              sendResponse,
              this.context.cloneScope
            );

            // Note: We intentionally do not use runSafe here so that any
            // errors are propagated to the message sender.
            let result = fire.raw(message, sender, sendResponse);
            message = null;

            if (result instanceof this.context.cloneScope.Promise) {
              return StrongPromise.wrap(result, channelId, caller);
            } else if (result === true) {
              return StrongPromise.wrap(promise, channelId, caller);
            }
            return response;
          },
        };

        MessageChannel.addListener(
          this.messageManagers,
          "Extension:Message",
          listener
        );
        return () => {
          MessageChannel.removeListener(
            this.messageManagers,
            "Extension:Message",
            listener
          );
        };
      },
    }).api();
  }

  onMessage(name) {
    return this._onMessage(name, sender => sender.id === this.sender.id);
  }

  onMessageExternal(name) {
    return this._onMessage(name, sender => sender.id !== this.sender.id);
  }
}

defineLazyGetter(Messenger.prototype, "nm", function() {
  return new NativeMessenger(this.context, this.sender);
});

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
        let patterns = this.whiteListedHosts.patterns.map(host => host.pattern);

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
          this.whiteListedHosts.patterns.filter(
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

  get whiteListedHosts() {
    return this.policy.allowedOrigins;
  }

  get webAccessibleResources() {
    return this.policy.webAccessibleResources;
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
    if (isContentProcess) {
      MessageChannel.abortResponses({ extensionId: this.id });
    }
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
      if (!getWinUtils(context.contentWindow).isHandlingUserInput) {
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
    let start = Cu.now() * 1000;
    try {
      return callable();
    } finally {
      if (gTimingEnabled) {
        let end = Cu.now() * 1000;
        PerformanceCounters.storeExecutionTime(
          this.context.extension.id,
          this.name,
          end - start,
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
// needs to use remote APIs. It uses the message manager to communicate
// with the ParentAPIManager singleton in ExtensionParent.jsm. It
// handles asynchronous function calls as well as event listeners.
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

var ExtensionChild = {
  BrowserExtensionContent,
  ChildAPIManager,
  Messenger,
};
