/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file handles extension background service worker logic that runs in the
 * child process.
 */

import {
  ExtensionChild,
  ExtensionActivityLogChild,
} from "resource://gre/modules/ExtensionChild.sys.mjs";

import { ExtensionCommon } from "resource://gre/modules/ExtensionCommon.sys.mjs";
import {
  ExtensionPageChild,
  getContextChildManagerGetter,
} from "resource://gre/modules/ExtensionPageChild.sys.mjs";
import { ExtensionUtils } from "resource://gre/modules/ExtensionUtils.sys.mjs";

const { BaseContext, redefineGetter } = ExtensionCommon;

const {
  ChildAPIManager,
  ChildLocalAPIImplementation,
  MessageEvent,
  Messenger,
  Port,
  ProxyAPIImplementation,
  SimpleEventAPI,
} = ExtensionChild;

const { DefaultMap, getUniqueId } = ExtensionUtils;

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
    return function (port, ...args) {
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
    return function (message, sender) {
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
        return function (port) {
          eventListener.callListener([], {
            apiObjectType: Ci.mozIExtensionListenerCallOptions.RUNTIME_PORT,
            apiObjectDescriptor: {
              portId: port.portId,
              name: port.name,
            },
          });
        };
      case "Port.onMessage":
        return function (message, port) {
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

  get api() {
    // No need to clone this for the worker, it's on a separate JSRuntime.
    return redefineGetter(this, "api", this.getAPI());
  }
}

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

  /**
   * @typedef {object} ExtensionPortDescriptor
   * https://phabricator.services.mozilla.com/D196385?id=801874#inline-1093734
   *
   * @returns {ExtensionPortDescriptor}
   */
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
          this.context.forgetOnClose(listener._callOnClose);
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
      function (...args) {
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

class WorkerContextChild extends BaseContext {
  /**
   * This WorkerContextChild represents an addon execution environment
   * that is running on the worker thread in an extension child process.
   *
   * @param {BrowserExtensionContent} extension This context's owner.
   * @param {object}                         params
   * @param {mozIExtensionServiceWorkerInfo} params.serviceWorkerInfo
   */
  constructor(extension, { serviceWorkerInfo }) {
    if (
      !serviceWorkerInfo?.scriptURL ||
      !serviceWorkerInfo?.clientInfoId ||
      !serviceWorkerInfo?.principal
    ) {
      throw new Error("Missing or invalid serviceWorkerInfo");
    }

    super("addon_child", extension);
    this.viewType = "background_worker";
    this.uri = Services.io.newURI(serviceWorkerInfo.scriptURL);
    this.workerClientInfoId = serviceWorkerInfo.clientInfoId;
    this.workerDescriptorId = serviceWorkerInfo.descriptorId;
    this.workerPrincipal = serviceWorkerInfo.principal;
    this.incognito = serviceWorkerInfo.principal.privateBrowsingId > 0;

    // A mozIExtensionAPIRequest being processed (set by the withAPIRequest
    // method while executing a given callable, can be optionally used by
    // the API implementation methods to access the mozIExtensionAPIRequest
    // being processed and customize their result if necessary to handle
    // requests originated by the webidl bindings).
    this.webidlAPIRequest = null;

    // This context uses a plain object as a cloneScope (anyway the values
    // moved across thread are going to be automatically serialized/deserialized
    // as structure clone data, we may remove this if we are changing the
    // internals to not use the context.cloneScope).
    this.workerCloneScope = {
      Promise,
      // The instances of this Error constructor will be recognized by the
      // ExtensionAPIRequestHandler as errors that should be propagated to
      // the worker thread and received by extension code that originated
      // the API request.
      Error: ExtensionUtils.WorkerExtensionError,
    };
  }

  getCreateProxyContextData() {
    const { workerDescriptorId } = this;
    return { workerDescriptorId };
  }

  openConduit(subject, address) {
    let proc = ChromeUtils.domProcessChild;
    let conduit = proc.getActor("ProcessConduits").openConduit(subject, {
      id: subject.id || getUniqueId(),
      extensionId: this.extension.id,
      envType: this.envType,
      workerScriptURL: this.uri.spec,
      workerDescriptorId: this.workerDescriptorId,
      ...address,
    });
    this.callOnClose(conduit);
    conduit.setCloseCallback(() => {
      this.forgetOnClose(conduit);
    });
    return conduit;
  }

  notifyWorkerLoaded() {
    this.childManager.conduit.sendContextLoaded({
      childId: this.childManager.id,
      extensionId: this.extension.id,
      workerDescriptorId: this.workerDescriptorId,
    });
  }

  withAPIRequest(request, callable) {
    this.webidlAPIRequest = request;
    try {
      return callable();
    } finally {
      this.webidlAPIRequest = null;
    }
  }

  getAPIRequest() {
    return this.webidlAPIRequest;
  }

  /**
   * Captures the most recent stack frame from the WebIDL API request being
   * processed.
   *
   * @returns {SavedFrame?}
   */
  getCaller() {
    return this.webidlAPIRequest?.callerSavedFrame;
  }

  logActivity(type, name, data) {
    ExtensionActivityLogChild.log(this, type, name, data);
  }

  get cloneScope() {
    return this.workerCloneScope;
  }

  get principal() {
    return this.workerPrincipal;
  }

  get tabId() {
    return -1;
  }

  get useWebIDLBindings() {
    return true;
  }

  shutdown() {
    this.unload();
  }

  unload() {
    if (this.unloaded) {
      return;
    }

    super.unload();
  }

  get childManager() {
    const childManager = getContextChildManagerGetter(
      { envType: "addon_parent" },
      WebIDLChildAPIManager
    ).call(this);
    return redefineGetter(this, "childManager", childManager);
  }

  get messenger() {
    return redefineGetter(this, "messenger", new WorkerMessenger(this));
  }
}

export var ExtensionWorkerChild = {
  /** @type {Map<number, WorkerContextChild>} */
  extensionWorkerContexts: new Map(),

  apiManager: ExtensionPageChild.apiManager,

  /**
   * Create an extension worker context (on a mozExtensionAPIRequest with
   * requestType "initWorkerContext").
   *
   * @param {BrowserExtensionContent} extension
   *     The extension for which the context should be created.
   * @param {mozIExtensionServiceWorkerInfo} serviceWorkerInfo
   */
  initExtensionWorkerContext(extension, serviceWorkerInfo) {
    if (!WebExtensionPolicy.isExtensionProcess) {
      throw new Error(
        "Cannot create an extension worker context in current process"
      );
    }

    const swId = serviceWorkerInfo.descriptorId;
    let context = this.extensionWorkerContexts.get(swId);
    if (context) {
      if (context.extension !== extension) {
        throw new Error(
          "A different extension context already exists for this service worker"
        );
      }
      throw new Error(
        "An extension context was already initialized for this service worker"
      );
    }

    context = new WorkerContextChild(extension, { serviceWorkerInfo });
    this.extensionWorkerContexts.set(swId, context);
  },

  /**
   * Get an existing extension worker context for the given extension and
   * service worker.
   *
   * @param {BrowserExtensionContent} extension
   *     The extension for which the context should be created.
   * @param {mozIExtensionServiceWorkerInfo} serviceWorkerInfo
   *
   * @returns {WorkerContextChild}
   */
  getExtensionWorkerContext(extension, serviceWorkerInfo) {
    if (!serviceWorkerInfo) {
      return null;
    }

    const context = this.extensionWorkerContexts.get(
      serviceWorkerInfo.descriptorId
    );

    if (context?.extension === extension) {
      return context;
    }

    return null;
  },

  /**
   * Notify the main process when an extension worker script has been loaded.
   *
   * @param {number} descriptorId The service worker descriptor ID of the destroyed context.
   * @param {WebExtensionPolicy} policy
   */
  notifyExtensionWorkerContextLoaded(descriptorId, policy) {
    let context = this.extensionWorkerContexts.get(descriptorId);
    if (context) {
      if (context.extension.id !== policy.id) {
        Cu.reportError(
          new Error(
            `ServiceWorker ${descriptorId} does not belong to the expected extension: ${policy.id}`
          )
        );
        return;
      }
      context.notifyWorkerLoaded();
    }
  },

  /**
   * Close the WorkerContextChild belonging to the given service worker, if any.
   *
   * @param {number} descriptorId The service worker descriptor ID of the destroyed context.
   */
  destroyExtensionWorkerContext(descriptorId) {
    let context = this.extensionWorkerContexts.get(descriptorId);
    if (context) {
      context.unload();
      this.extensionWorkerContexts.delete(descriptorId);
    }
  },

  shutdownExtension(extensionId) {
    for (let [workerClientInfoId, context] of this.extensionWorkerContexts) {
      if (context.extension.id == extensionId) {
        context.shutdown();
        this.extensionWorkerContexts.delete(workerClientInfoId);
      }
    }
  },
};
