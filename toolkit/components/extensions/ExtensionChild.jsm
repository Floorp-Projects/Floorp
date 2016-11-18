/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionChild"];

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

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionManagement",
                                  "resource://gre/modules/ExtensionManagement.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MessageChannel",
                                  "resource://gre/modules/MessageChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NativeApp",
                                  "resource://gre/modules/NativeMessaging.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PromiseUtils",
                                  "resource://gre/modules/PromiseUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");

const CATEGORY_EXTENSION_SCRIPTS_ADDON = "webextension-scripts-addon";
const CATEGORY_EXTENSION_SCRIPTS_DEVTOOLS = "webextension-scripts-devtools";

Cu.import("resource://gre/modules/ExtensionCommon.jsm");
Cu.import("resource://gre/modules/ExtensionUtils.jsm");

const {
  DefaultMap,
  EventManager,
  SingletonEventManager,
  SpreadArgs,
  defineLazyGetter,
  getInnerWindowID,
  getMessageManager,
  getUniqueId,
  injectAPI,
  promiseEvent,
} = ExtensionUtils;

const {
  BaseContext,
  LocalAPIImplementation,
  SchemaAPIInterface,
  SchemaAPIManager,
} = ExtensionCommon;

var ExtensionChild;

/**
 * Abstraction for a Port object in the extension API.
 *
 * @param {BaseContext} context The context that owns this port.
 * @param {nsIMessageSender} senderMM The message manager to send messages to.
 * @param {Array<nsIMessageListenerManager>} receiverMMs Message managers to
 *     listen on.
 * @param {string} name Arbitrary port name as defined by the addon.
 * @param {string} id An ID that uniquely identifies this port's channel.
 * @param {object} sender The `port.sender` property.
 * @param {object} recipient The recipient of messages sent from this port.
 */
class Port {
  constructor(context, senderMM, receiverMMs, name, id, sender, recipient) {
    this.context = context;
    this.senderMM = senderMM;
    this.receiverMMs = receiverMMs;
    this.name = name;
    this.id = id;
    this.sender = sender;
    this.recipient = recipient;
    this.disconnected = false;
    this.disconnectListeners = new Set();
    this.unregisterMessageFuncs = new Set();

    // Common options for onMessage and onDisconnect.
    this.handlerBase = {
      messageFilterStrict: {portId: id},

      filterMessage: (sender, recipient) => {
        return sender.contextId !== this.context.contextId;
      },
    };

    this.disconnectHandler = Object.assign({
      receiveMessage: ({data}) => this.disconnectByOtherEnd(data),
    }, this.handlerBase);

    MessageChannel.addListener(this.receiverMMs, "Extension:Port:Disconnect", this.disconnectHandler);

    this.context.callOnClose(this);
  }

  api() {
    let portObj = Cu.createObjectIn(this.context.cloneScope);

    let portError = null;
    let publicAPI = {
      name: this.name,

      disconnect: () => {
        this.disconnect();
      },

      postMessage: json => {
        this.postMessage(json);
      },

      onDisconnect: new EventManager(this.context, "Port.onDisconnect", fire => {
        return this.registerOnDisconnect(error => {
          portError = error && this.context.normalizeError(error);
          fire.withoutClone(portObj);
        });
      }).api(),

      onMessage: new EventManager(this.context, "Port.onMessage", fire => {
        return this.registerOnMessage(msg => {
          msg = Cu.cloneInto(msg, this.context.cloneScope);
          fire.withoutClone(msg, portObj);
        });
      }).api(),

      get error() {
        return portError;
      },
    };

    if (this.sender) {
      publicAPI.sender = this.sender;
    }

    injectAPI(publicAPI, portObj);
    return portObj;
  }

  postMessage(json) {
    if (this.disconnected) {
      throw new this.context.cloneScope.Error("Attempt to postMessage on disconnected port");
    }

    this._sendMessage("Extension:Port:PostMessage", json);
  }

  /**
   * Register a callback that is called when the port is disconnected by the
   * *other* end. The callback is automatically unregistered when the port or
   * context is closed.
   *
   * @param {function} callback Called when the other end disconnects the port.
   *     If the disconnect is caused by an error, the first parameter is an
   *     object with a "message" string property that describes the cause.
   * @returns {function} Function to unregister the listener.
   */
  registerOnDisconnect(callback) {
    let listener = error => {
      if (this.context.active && !this.disconnected) {
        callback(error);
      }
    };
    this.disconnectListeners.add(listener);
    return () => {
      this.disconnectListeners.delete(listener);
    };
  }

  /**
   * Register a callback that is called when a message is received. The callback
   * is automatically unregistered when the port or context is closed.
   *
   * @param {function} callback Called when a message is received.
   * @returns {function} Function to unregister the listener.
   */
  registerOnMessage(callback) {
    let handler = Object.assign({
      receiveMessage: ({data}) => {
        if (this.context.active && !this.disconnected) {
          callback(data);
        }
      },
    }, this.handlerBase);

    let unregister = () => {
      this.unregisterMessageFuncs.delete(unregister);
      MessageChannel.removeListener(this.receiverMMs, "Extension:Port:PostMessage", handler);
    };
    MessageChannel.addListener(this.receiverMMs, "Extension:Port:PostMessage", handler);
    this.unregisterMessageFuncs.add(unregister);
    return unregister;
  }

  _sendMessage(message, data) {
    let options = {
      recipient: Object.assign({}, this.recipient, {portId: this.id}),
      responseType: MessageChannel.RESPONSE_NONE,
    };

    return this.context.sendMessage(this.senderMM, message, data, options);
  }

  handleDisconnection() {
    MessageChannel.removeListener(this.receiverMMs, "Extension:Port:Disconnect", this.disconnectHandler);
    for (let unregister of this.unregisterMessageFuncs) {
      unregister();
    }
    this.context.forgetOnClose(this);
    this.disconnected = true;
  }

  /**
   * Disconnect the port from the other end (which may not even exist).
   *
   * @param {Error|{message: string}} [error] The reason for disconnecting,
   *     if it is an abnormal disconnect.
   */
  disconnectByOtherEnd(error = null) {
    if (this.disconnected) {
      return;
    }

    for (let listener of this.disconnectListeners) {
      listener(error);
    }

    this.handleDisconnection();
  }

  /**
   * Disconnect the port from this end.
   *
   * @param {Error|{message: string}} [error] The reason for disconnecting,
   *     if it is an abnormal disconnect.
   */
  disconnect(error = null) {
    if (this.disconnected) {
      // disconnect() may be called without side effects even after the port is
      // closed - https://developer.chrome.com/extensions/runtime#type-Port
      return;
    }
    this.handleDisconnection();
    if (error) {
      error = {message: this.context.normalizeError(error).message};
    }
    this._sendMessage("Extension:Port:Disconnect", error);
  }

  close() {
    this.disconnect();
  }
}

class NativePort extends Port {
  postMessage(data) {
    data = NativeApp.encodeMessage(this.context, data);

    return super.postMessage(data);
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
    let promise = this._sendMessage(messageManager, "Extension:Message", msg, recipient)
      .catch(error => {
        if (error.result == MessageChannel.RESULT_NO_HANDLER) {
          return Promise.reject({message: "Could not establish connection. Receiving end does not exist."});
        } else if (error.result != MessageChannel.RESULT_NO_RESPONSE) {
          return Promise.reject({message: error.message});
        }
      });

    return this.context.wrapPromise(promise, responseCallback);
  }

  sendNativeMessage(messageManager, msg, recipient, responseCallback) {
    msg = NativeApp.encodeMessage(this.context, msg);
    return this.sendMessage(messageManager, msg, recipient, responseCallback);
  }

  onMessage(name) {
    return new SingletonEventManager(this.context, name, callback => {
      let listener = {
        messageFilterPermissive: this.optionalFilter,
        messageFilterStrict: this.filter,

        filterMessage: (sender, recipient) => {
          // Ignore the message if it was sent by this Messenger.
          return sender.contextId !== this.context.contextId;
        },

        receiveMessage: ({target, data: message, sender, recipient}) => {
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

          message = Cu.cloneInto(message, this.context.cloneScope);
          sender = Cu.cloneInto(sender, this.context.cloneScope);
          sendResponse = Cu.exportFunction(sendResponse, this.context.cloneScope);

          // Note: We intentionally do not use runSafe here so that any
          // errors are propagated to the message sender.
          let result = callback(message, sender, sendResponse);
          if (result instanceof this.context.cloneScope.Promise) {
            return result;
          } else if (result === true) {
            return promise;
          }
          return response;
        },
      };

      MessageChannel.addListener(this.messageManagers, "Extension:Message", listener);
      return () => {
        MessageChannel.removeListener(this.messageManagers, "Extension:Message", listener);
      };
    }).api();
  }

  _connect(messageManager, port, recipient) {
    let msg = {
      name: port.name,
      portId: port.id,
    };

    this._sendMessage(messageManager, "Extension:Connect", msg, recipient).catch(error => {
      if (error.result === MessageChannel.RESULT_NO_HANDLER) {
        error = {message: "Could not establish connection. Receiving end does not exist."};
      } else if (error.result === MessageChannel.RESULT_DISCONNECTED) {
        error = null;
      }
      port.disconnectByOtherEnd(error);
    });

    return port.api();
  }

  connect(messageManager, name, recipient) {
    let portId = getUniqueId();

    let port = new Port(this.context, messageManager, this.messageManagers, name, portId, null, recipient);

    return this._connect(messageManager, port, recipient);
  }

  connectNative(messageManager, name, recipient) {
    let portId = getUniqueId();

    let port = new NativePort(this.context, messageManager, this.messageManagers, name, portId, null, recipient);

    return this._connect(messageManager, port, recipient);
  }

  onConnect(name) {
    return new SingletonEventManager(this.context, name, callback => {
      let listener = {
        messageFilterPermissive: this.optionalFilter,
        messageFilterStrict: this.filter,

        filterMessage: (sender, recipient) => {
          // Ignore the port if it was created by this Messenger.
          return sender.contextId !== this.context.contextId;
        },

        receiveMessage: ({target, data: message, sender}) => {
          let {name, portId} = message;
          let mm = getMessageManager(target);
          let recipient = Object.assign({}, sender);
          if (recipient.tab) {
            recipient.tabId = recipient.tab.id;
            delete recipient.tab;
          }
          let port = new Port(this.context, mm, this.messageManagers, name, portId, sender, recipient);
          this.context.runSafeWithoutClone(callback, port.api());
          return true;
        },
      };

      MessageChannel.addListener(this.messageManagers, "Extension:Connect", listener);
      return () => {
        MessageChannel.removeListener(this.messageManagers, "Extension:Connect", listener);
      };
    }).api();
  }
}

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

var devtoolsAPIManager = new class extends SchemaAPIManager {
  constructor() {
    super("devtools");
    this.initialized = false;
  }

  generateAPIs(...args) {
    if (!this.initialized) {
      this.initialized = true;
      for (let [/* name */, value] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCRIPTS_DEVTOOLS)) {
        this.loadScript(value);
      }
    }
    return super.generateAPIs(...args);
  }

  registerSchemaAPI(namespace, envType, getAPI) {
    if (envType == "devtools_child") {
      super.registerSchemaAPI(namespace, envType, getAPI);
    }
  }
}();

/**
 * An object that runs an remote implementation of an API.
 */
class ProxyAPIImplementation extends SchemaAPIInterface {
  /**
   * @param {string} namespace The full path to the namespace that contains the
   *     `name` member. This may contain dots, e.g. "storage.local".
   * @param {string} name The name of the method or property.
   * @param {ChildAPIManager} childApiManager The owner of this implementation.
   */
  constructor(namespace, name, childApiManager) {
    super();
    this.path = `${namespace}.${name}`;
    this.childApiManager = childApiManager;
  }

  callFunctionNoReturn(args) {
    this.childApiManager.callParentFunctionNoReturn(this.path, args);
  }

  callAsyncFunction(args, callback) {
    return this.childApiManager.callParentAsyncFunction(this.path, args, callback);
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

    this.childApiManager.messageManager.sendAsyncMessage("API:AddListener", {
      childId: this.childApiManager.id,
      listenerId: id,
      path: this.path,
      args,
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

    this.childApiManager.messageManager.sendAsyncMessage("API:RemoveListener", {
      childId: this.childApiManager.id,
      listenerId: id,
      path: this.path,
    });
  }

  hasListener(listener) {
    let map = this.childApiManager.listeners.get(this.path);
    return map.listeners.has(listener);
  }
}

// We create one instance of this class for every extension context that
// needs to use remote APIs. It uses the message manager to communicate
// with the ParentAPIManager singleton in ExtensionParent.jsm. It
// handles asynchronous function calls as well as event listeners.
class ChildAPIManager {
  constructor(context, messageManager, localApis, contextData) {
    this.context = context;
    this.messageManager = messageManager;
    this.url = contextData.url;

    // The root namespace of all locally implemented APIs. If an extension calls
    // an API that does not exist in this object, then the implementation is
    // delegated to the ParentAPIManager.
    this.localApis = localApis;

    this.id = `${context.extension.id}.${context.contextId}`;

    MessageChannel.addListener(messageManager, "API:RunListener", this);
    messageManager.addMessageListener("API:CallResult", this);

    this.messageFilterStrict = {childId: this.id};

    this.listeners = new DefaultMap(() => ({
      ids: new Map(),
      listeners: new Map(),
    }));

    // Map[callId -> Deferred]
    this.callPromises = new Map();

    let params = {
      childId: this.id,
      extensionId: context.extension.id,
      principal: context.principal,
    };
    Object.assign(params, contextData);

    this.messageManager.sendAsyncMessage("API:CreateProxyContext", params);
  }

  receiveMessage({name, messageName, data}) {
    if (data.childId != this.id) {
      return;
    }

    switch (name || messageName) {
      case "API:RunListener":
        let map = this.listeners.get(data.path);
        let listener = map.ids.get(data.listenerId);

        if (listener) {
          return this.context.runSafe(listener, ...data.args);
        }

        Cu.reportError(`Unknown listener at childId=${data.childId} path=${data.path} listenerId=${data.listenerId}\n`);
        break;

      case "API:CallResult":
        let deferred = this.callPromises.get(data.callId);
        if ("error" in data) {
          deferred.reject(data.error);
        } else {
          deferred.resolve(new SpreadArgs(data.result));
        }
        this.callPromises.delete(data.callId);
        break;
    }
  }

  /**
   * Call a function in the parent process and ignores its return value.
   *
   * @param {string} path The full name of the method, e.g. "tabs.create".
   * @param {Array} args The parameters for the function.
   */
  callParentFunctionNoReturn(path, args) {
    this.messageManager.sendAsyncMessage("API:Call", {
      childId: this.id,
      path,
      args,
    });
  }

  /**
   * Calls a function in the parent process and returns its result
   * asynchronously.
   *
   * @param {string} path The full name of the method, e.g. "tabs.create".
   * @param {Array} args The parameters for the function.
   * @param {function(*)} [callback] The callback to be called when the function
   *     completes.
   * @returns {Promise|undefined} Must be void if `callback` is set, and a
   *     promise otherwise. The promise is resolved when the function completes.
   */
  callParentAsyncFunction(path, args, callback) {
    let callId = getUniqueId();
    let deferred = PromiseUtils.defer();
    this.callPromises.set(callId, deferred);

    this.messageManager.sendAsyncMessage("API:Call", {
      childId: this.id,
      callId,
      path,
      args,
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

    let impl = new ProxyAPIImplementation(namespace, name, this);
    return {
      addListener: (listener, ...args) => impl.addListener(listener, args),
      removeListener: (listener) => impl.removeListener(listener),
      hasListener: (listener) => impl.hasListener(listener),
    };
  }

  close() {
    this.messageManager.sendAsyncMessage("API:CloseProxyContext", {childId: this.id});
  }

  get cloneScope() {
    return this.context.cloneScope;
  }

  get principal() {
    return this.context.principal;
  }

  shouldInject(namespace, name, allowedContexts) {
    // Do not generate content script APIs, unless explicitly allowed.
    if (this.context.envType === "content_child" &&
        !allowedContexts.includes("content")) {
      return false;
    }
    if (allowedContexts.includes("addon_parent_only")) {
      return false;
    }

    // Do not generate devtools APIs, unless explicitly allowed.
    if (this.context.envType === "devtools_child" &&
        !allowedContexts.includes("devtools")) {
      return false;
    }

    // Do not generate devtools APIs, unless explicitly allowed.
    if (this.context.envType !== "devtools_child" &&
        allowedContexts.includes("devtools_only")) {
      return false;
    }

    return true;
  }

  getImplementation(namespace, name) {
    let obj = namespace.split(".").reduce(
      (object, prop) => object && object[prop],
      this.localApis);

    if (obj && name in obj) {
      return new LocalAPIImplementation(obj, name, this.context);
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
}

class ExtensionBaseContextChild extends BaseContext {
  /**
   * This ExtensionBaseContextChild represents an addon execution environment
   * that is running in an addon or devtools child process.
   *
   * @param {BrowserExtensionContent} extension This context's owner.
   * @param {object} params
   * @param {string} params.envType One of "addon_child" or "devtools_child".
   * @param {nsIDOMWindow} params.contentWindow The window where the addon runs.
   * @param {string} params.viewType One of "background", "popup", "tab",
   *   "devtools_page" or "devtools_panel".
   * @param {number} [params.tabId] This tab's ID, used if viewType is "tab".
   */
  constructor(extension, params) {
    if (!params.envType) {
      throw new Error("Missing envType");
    }

    super(params.envType, extension);
    let {viewType, uri, contentWindow, tabId} = params;
    this.viewType = viewType;
    this.uri = uri || extension.baseURI;

    this.setContentWindow(contentWindow);

    // This is the MessageSender property passed to extension.
    // It can be augmented by the "page-open" hook.
    let sender = {id: extension.uuid};
    if (viewType == "tab") {
      sender.tabId = tabId;
      this.tabId = tabId;
    }
    if (uri) {
      sender.url = uri.spec;
    }
    this.sender = sender;

    Schemas.exportLazyGetter(contentWindow, "browser", () => {
      let browserObj = Cu.createObjectIn(contentWindow);
      Schemas.inject(browserObj, this.childManager);
      return browserObj;
    });

    Schemas.exportLazyGetter(contentWindow, "chrome", () => {
      let chromeApiWrapper = Object.create(this.childManager);
      chromeApiWrapper.isChromeCompat = true;

      let chromeObj = Cu.createObjectIn(contentWindow);
      Schemas.inject(chromeObj, chromeApiWrapper);
      return chromeObj;
    });
  }

  get cloneScope() {
    return this.contentWindow;
  }

  get principal() {
    return this.contentWindow.document.nodePrincipal;
  }

  get windowId() {
    if (this.viewType == "tab" || this.viewType == "popup") {
      let globalView = ExtensionChild.contentGlobals.get(this.messageManager);
      return globalView ? globalView.windowId : -1;
    }
  }

  // Called when the extension shuts down.
  shutdown() {
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

    if (this.contentWindow) {
      this.contentWindow.close();
    }

    super.unload();
  }
}

defineLazyGetter(ExtensionBaseContextChild.prototype, "messenger", function() {
  let filter = {extensionId: this.extension.id};
  let optionalFilter = {};
  // Addon-generated messages (not necessarily from the same process as the
  // addon itself) are sent to the main process, which forwards them via the
  // parent process message manager. Specific replies can be sent to the frame
  // message manager.
  return new Messenger(this, [Services.cpmm, this.messageManager], this.sender,
                       filter, optionalFilter);
});

class ExtensionPageContextChild extends ExtensionBaseContextChild {
  /**
   * This ExtensionPageContextChild represents a privileged addon
   * execution environment that has full access to the WebExtensions
   * APIs (provided that the correct permissions have been requested).
   *
   * This is the child side of the ExtensionPageContextParent class
   * defined in ExtensionParent.jsm.
   *
   * @param {BrowserExtensionContent} extension This context's owner.
   * @param {object} params
   * @param {nsIDOMWindow} params.contentWindow The window where the addon runs.
   * @param {string} params.viewType One of "background", "popup" or "tab".
   *     "background" and "tab" are used by `browser.extension.getViews`.
   *     "popup" is only used internally to identify page action and browser
   *     action popups and options_ui pages.
   * @param {number} [params.tabId] This tab's ID, used if viewType is "tab".
   */
  constructor(extension, params) {
    super(extension, Object.assign(params, {envType: "addon_child"}));

    this.extension.views.add(this);
  }

  unload() {
    super.unload();
    this.extension.views.delete(this);
  }
}

defineLazyGetter(ExtensionPageContextChild.prototype, "childManager", function() {
  let localApis = {};
  apiManager.generateAPIs(this, localApis);

  let childManager = new ChildAPIManager(this, this.messageManager, localApis, {
    envType: "addon_parent",
    viewType: this.viewType,
    url: this.uri.spec,
    incognito: this.incognito,
  });

  this.callOnClose(childManager);

  if (this.viewType == "background") {
    apiManager.global.initializeBackgroundPage(this.contentWindow);
  }

  return childManager;
});

class DevtoolsContextChild extends ExtensionBaseContextChild {
  /**
   * This DevtoolsContextChild represents a devtools-related addon execution
   * environment that has access to the devtools API namespace and to the same subset
   * of APIs available in a content script execution environment.
   *
   * @param {BrowserExtensionContent} extension This context's owner.
   * @param {object} params
   * @param {nsIDOMWindow} params.contentWindow The window where the addon runs.
   * @param {string} params.viewType One of "devtools_page" or "devtools_panel".
   * @param {object} [params.devtoolsToolboxInfo] This devtools toolbox's information,
   *   used if viewType is "devtools_page" or "devtools_panel".
   */
  constructor(extension, params) {
    super(extension, Object.assign(params, {envType: "devtools_child"}));

    this.devtoolsToolboxInfo = params.devtoolsToolboxInfo;

    this.extension.devtoolsViews.add(this);
  }

  unload() {
    super.unload();
    this.extension.devtoolsViews.delete(this);
  }
}

defineLazyGetter(DevtoolsContextChild.prototype, "childManager", function() {
  let localApis = {};
  devtoolsAPIManager.generateAPIs(this, localApis);

  let childManager = new ChildAPIManager(this, this.messageManager, localApis, {
    envType: "devtools_parent",
    viewType: this.viewType,
    url: this.uri.spec,
    incognito: this.incognito,
  });

  this.callOnClose(childManager);

  return childManager;
});

// All subframes in a tab, background page, popup, etc. have the same view type.
// This class keeps track of such global state.
// Note that this is created even for non-extension tabs because at present we
// do not have a way to distinguish regular tabs from extension tabs at the
// initialization of a frame script.
class ContentGlobal {
  /**
   * @param {nsIContentFrameMessageManager} global The frame script's global.
   */
  constructor(global) {
    this.global = global;
    // Unless specified otherwise assume that the extension page is in a tab,
    // because the majority of all class instances are going to be a tab. Any
    // special views (background page, extension popup) will immediately send an
    // Extension:InitExtensionView message to change the viewType.
    this.viewType = "tab";
    this.tabId = -1;
    this.windowId = -1;
    this.initialized = false;

    this.global.addMessageListener("Extension:InitExtensionView", this);
    this.global.addMessageListener("Extension:SetTabAndWindowId", this);
  }

  uninit() {
    this.global.removeMessageListener("Extension:InitExtensionView", this);
    this.global.removeMessageListener("Extension:SetTabAndWindowId", this);
  }

  ensureInitialized() {
    if (!this.initialized) {
      // Request tab and window ID in case "Extension:InitExtensionView" is not
      // sent (e.g. when `viewType` is "tab").
      let reply = this.global.sendSyncMessage("Extension:GetTabAndWindowId");
      this.handleSetTabAndWindowId(reply[0] || {});
    }
    return this;
  }

  receiveMessage({name, data}) {
    switch (name) {
      case "Extension:InitExtensionView":
        // The view type is initialized once and then fixed.
        this.global.removeMessageListener("Extension:InitExtensionView", this);
        this.viewType = data.viewType;

        if (data.devtoolsToolboxInfo) {
          this.devtoolsToolboxInfo = data.devtoolsToolboxInfo;
        }

        promiseEvent(this.global, "DOMContentLoaded", true).then(() => {
          this.global.sendAsyncMessage("Extension:ExtensionViewLoaded");
        });

        /* FALLTHROUGH */
      case "Extension:SetTabAndWindowId":
        this.handleSetTabAndWindowId(data);
        break;
    }
  }

  handleSetTabAndWindowId(data) {
    let {tabId, windowId} = data;

    if (tabId) {
      // Tab IDs are not expected to change.
      if (this.tabId !== -1 && tabId !== this.tabId) {
        throw new Error("Attempted to change a tabId after it was set");
      }
      this.tabId = tabId;
    }

    if (windowId !== undefined) {
      // Window IDs may change if a tab is moved to a different location.
      // Note: This is the ID of the browser window for the extension API.
      // Do not confuse it with the innerWindowID of DOMWindows!
      this.windowId = windowId;
    }
    this.initialized = true;
  }
}

ExtensionChild = {
  ChildAPIManager,
  Messenger,
  Port,

  // Map<nsIContentFrameMessageManager, ContentGlobal>
  contentGlobals: new Map(),

  // Map<innerWindowId, ExtensionPageContextChild>
  extensionContexts: new Map(),

  initOnce() {
    // This initializes the default message handler for messages targeted at
    // an addon process, in case the addon process receives a message before
    // its Messenger has been instantiated. For example, if a content script
    // sends a message while there is no background page.
    MessageChannel.setupMessageManagers([Services.cpmm]);
  },

  init(global) {
    if (!ExtensionManagement.isExtensionProcess) {
      throw new Error("Cannot init extension page global in current process");
    }

    this.contentGlobals.set(global, new ContentGlobal(global));
  },

  uninit(global) {
    this.contentGlobals.get(global).uninit();
    this.contentGlobals.delete(global);
  },

  /**
   * Create a privileged context at document-element-inserted.
   *
   * @param {BrowserExtensionContent} extension
   *     The extension for which the context should be created.
   * @param {nsIDOMWindow} contentWindow The global of the page.
   */
  createExtensionContext(extension, contentWindow) {
    if (!ExtensionManagement.isExtensionProcess) {
      throw new Error("Cannot create an extension page context in current process");
    }

    let windowId = getInnerWindowID(contentWindow);
    let context = this.extensionContexts.get(windowId);
    if (context) {
      if (context.extension !== extension) {
        throw new Error("A different extension context already exists for this frame");
      }
      throw new Error("An extension context was already initialized for this frame");
    }

    let mm = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDocShell)
                          .QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIContentFrameMessageManager);

    let {viewType, tabId, devtoolsToolboxInfo} = this.contentGlobals.get(mm).ensureInitialized();

    let uri = contentWindow.document.documentURIObject;

    if (devtoolsToolboxInfo) {
      context = new DevtoolsContextChild(extension, {
        viewType, contentWindow, uri, tabId, devtoolsToolboxInfo,
      });
    } else {
      context = new ExtensionPageContextChild(extension, {viewType, contentWindow, uri, tabId});
    }

    this.extensionContexts.set(windowId, context);
  },

  /**
   * Close the ExtensionPageContextChild belonging to the given window, if any.
   *
   * @param {number} windowId The inner window ID of the destroyed context.
   */
  destroyExtensionContext(windowId) {
    let context = this.extensionContexts.get(windowId);
    if (context) {
      context.unload();
      this.extensionContexts.delete(windowId);
    }
  },

  shutdownExtension(extensionId) {
    for (let [windowId, context] of this.extensionContexts) {
      if (context.extension.id == extensionId) {
        context.shutdown();
        this.extensionContexts.delete(windowId);
      }
    }
  },
};
