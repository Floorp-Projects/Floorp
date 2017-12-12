/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This module contains utilities and base classes for logic which is
 * common between the parent and child process, and in particular
 * between ExtensionParent.jsm and ExtensionChild.jsm.
 */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

/* exported ExtensionCommon */

this.EXPORTED_SYMBOLS = ["ExtensionCommon"];

Cu.importGlobalProperties(["fetch"]);

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  ConsoleAPI: "resource://gre/modules/Console.jsm",
  MessageChannel: "resource://gre/modules/MessageChannel.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Schemas: "resource://gre/modules/Schemas.jsm",
});

XPCOMUtils.defineLazyServiceGetter(this, "styleSheetService",
                                   "@mozilla.org/content/style-sheet-service;1",
                                   "nsIStyleSheetService");

const global = Cu.getGlobalForObject(this);

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  DefaultMap,
  DefaultWeakMap,
  EventEmitter,
  ExtensionError,
  defineLazyGetter,
  filterStack,
  getConsole,
  getInnerWindowID,
  getUniqueId,
  getWinUtils,
} = ExtensionUtils;

XPCOMUtils.defineLazyGetter(this, "console", getConsole);

var ExtensionCommon;

/**
 * A sentinel class to indicate that an array of values should be
 * treated as an array when used as a promise resolution value, but as a
 * spread expression (...args) when passed to a callback.
 */
class SpreadArgs extends Array {
  constructor(args) {
    super();
    this.push(...args);
  }
}

/**
 * Like SpreadArgs, but also indicates that the array values already
 * belong to the target compartment, and should not be cloned before
 * being passed.
 *
 * The `unwrappedValues` property contains an Array object which belongs
 * to the target compartment, and contains the same unwrapped values
 * passed the NoCloneSpreadArgs constructor.
 */
class NoCloneSpreadArgs {
  constructor(args) {
    this.unwrappedValues = args;
  }

  [Symbol.iterator]() {
    return this.unwrappedValues[Symbol.iterator]();
  }
}

/**
 * Base class for WebExtension APIs.  Each API creates a new class
 * that inherits from this class, the derived class is instantiated
 * once for each extension that uses the API.
 */
class ExtensionAPI extends ExtensionUtils.EventEmitter {
  constructor(extension) {
    super();

    this.extension = extension;

    extension.once("shutdown", () => {
      if (this.onShutdown) {
        this.onShutdown(extension.shutdownReason);
      }
      this.extension = null;
    });
  }

  destroy() {
  }

  onManifestEntry(entry) {
  }

  getAPI(context) {
    throw new Error("Not Implemented");
  }
}

var ExtensionAPIs = {
  apis: new Map(),

  load(apiName) {
    let api = this.apis.get(apiName);

    if (api.loadPromise) {
      return api.loadPromise;
    }

    let {script, schema} = api;

    let addonId = `${apiName}@experiments.addons.mozilla.org`;
    api.sandbox = Cu.Sandbox(global, {
      wantXrays: false,
      sandboxName: script,
      addonId,
      metadata: {addonID: addonId},
    });

    api.sandbox.ExtensionAPI = ExtensionAPI;

    // Create a console getter which lazily provide a ConsoleAPI instance.
    XPCOMUtils.defineLazyGetter(api.sandbox, "console", () => {
      return new ConsoleAPI({prefix: addonId});
    });

    Services.scriptloader.loadSubScript(script, api.sandbox, "UTF-8");

    api.loadPromise = Schemas.load(schema).then(() => {
      let API = Cu.evalInSandbox("API", api.sandbox);
      API.prototype.namespace = apiName;
      return API;
    });

    return api.loadPromise;
  },

  unload(apiName) {
    let api = this.apis.get(apiName);

    let {schema} = api;

    Schemas.unload(schema);
    Cu.nukeSandbox(api.sandbox);

    api.sandbox = null;
    api.loadPromise = null;
  },

  register(namespace, schema, script) {
    if (this.apis.has(namespace)) {
      throw new Error(`API namespace already exists: ${namespace}`);
    }

    this.apis.set(namespace, {schema, script});
  },

  unregister(namespace) {
    if (!this.apis.has(namespace)) {
      throw new Error(`API namespace does not exist: ${namespace}`);
    }

    this.apis.delete(namespace);
  },
};

/**
 * This class contains the information we have about an individual
 * extension.  It is never instantiated directly, instead subclasses
 * for each type of process extend this class and add members that are
 * relevant for that process.
 * @abstract
 */
class BaseContext {
  constructor(envType, extension) {
    this.envType = envType;
    this.onClose = new Set();
    this.checkedLastError = false;
    this._lastError = null;
    this.contextId = getUniqueId();
    this.unloaded = false;
    this.extension = extension;
    this.jsonSandbox = null;
    this.active = true;
    this.incognito = null;
    this.messageManager = null;
    this.docShell = null;
    this.contentWindow = null;
    this.innerWindowID = 0;
  }

  setContentWindow(contentWindow) {
    let {document} = contentWindow;
    let {docShell} = document;

    this.innerWindowID = getInnerWindowID(contentWindow);
    this.messageManager = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIContentFrameMessageManager);

    if (this.incognito == null) {
      this.incognito = PrivateBrowsingUtils.isContentWindowPrivate(contentWindow);
    }

    MessageChannel.setupMessageManagers([this.messageManager]);

    let onPageShow = event => {
      if (!event || event.target === document) {
        this.docShell = docShell;
        this.contentWindow = contentWindow;
        this.active = true;
      }
    };
    let onPageHide = event => {
      if (!event || event.target === document) {
        // Put this off until the next tick.
        Promise.resolve().then(() => {
          this.docShell = null;
          this.contentWindow = null;
          this.active = false;
        });
      }
    };

    onPageShow();
    contentWindow.addEventListener("pagehide", onPageHide, true);
    contentWindow.addEventListener("pageshow", onPageShow, true);
    this.callOnClose({
      close: () => {
        onPageHide();
        if (this.active) {
          contentWindow.removeEventListener("pagehide", onPageHide, true);
          contentWindow.removeEventListener("pageshow", onPageShow, true);
        }
      },
    });
  }

  get cloneScope() {
    throw new Error("Not implemented");
  }

  get principal() {
    throw new Error("Not implemented");
  }

  runSafe(callback, ...args) {
    return this.applySafe(callback, args);
  }

  runSafeWithoutClone(callback, ...args) {
    return this.applySafeWithoutClone(callback, args);
  }

  applySafe(callback, args) {
    if (this.unloaded) {
      Cu.reportError("context.runSafe called after context unloaded");
    } else if (!this.active) {
      Cu.reportError("context.runSafe called while context is inactive");
    } else {
      try {
        let {cloneScope} = this;
        args = args.map(arg => Cu.cloneInto(arg, cloneScope));
      } catch (e) {
        Cu.reportError(e);
        dump(`runSafe failure: cloning into ${this.cloneScope}: ${e}\n\n${filterStack(Error())}`);
      }

      return this.applySafeWithoutClone(callback, args);
    }
  }

  applySafeWithoutClone(callback, args) {
    if (this.unloaded) {
      Cu.reportError("context.runSafeWithoutClone called after context unloaded");
    } else if (!this.active) {
      Cu.reportError("context.runSafeWithoutClone called while context is inactive");
    } else {
      try {
        return Reflect.apply(callback, null, args);
      } catch (e) {
        dump(`Extension error: ${e} ${e.fileName} ${e.lineNumber}\n[[Exception stack\n${filterStack(e)}Current stack\n${filterStack(Error())}]]\n`);
        Cu.reportError(e);
      }
    }
  }

  checkLoadURL(url, options = {}) {
    // As an optimization, f the URL starts with the extension's base URL,
    // don't do any further checks. It's always allowed to load it.
    if (url.startsWith(this.extension.baseURL)) {
      return true;
    }

    return ExtensionUtils.checkLoadURL(url, this.principal, options);
  }

  /**
   * Safely call JSON.stringify() on an object that comes from an
   * extension.
   *
   * @param {array<any>} args Arguments for JSON.stringify()
   * @returns {string} The stringified representation of obj
   */
  jsonStringify(...args) {
    if (!this.jsonSandbox) {
      this.jsonSandbox = Cu.Sandbox(this.principal, {
        sameZoneAs: this.cloneScope,
        wantXrays: false,
      });
    }

    return Cu.waiveXrays(this.jsonSandbox.JSON).stringify(...args);
  }

  callOnClose(obj) {
    this.onClose.add(obj);
  }

  forgetOnClose(obj) {
    this.onClose.delete(obj);
  }

  /**
   * A wrapper around MessageChannel.sendMessage which adds the extension ID
   * to the recipient object, and ensures replies are not processed after the
   * context has been unloaded.
   *
   * @param {nsIMessageManager} target
   * @param {string} messageName
   * @param {object} data
   * @param {object} [options]
   * @param {object} [options.sender]
   * @param {object} [options.recipient]
   *
   * @returns {Promise}
   */
  sendMessage(target, messageName, data, options = {}) {
    options.recipient = Object.assign({extensionId: this.extension.id}, options.recipient);
    options.sender = options.sender || {};

    options.sender.extensionId = this.extension.id;
    options.sender.contextId = this.contextId;

    return MessageChannel.sendMessage(target, messageName, data, options);
  }

  get lastError() {
    this.checkedLastError = true;
    return this._lastError;
  }

  set lastError(val) {
    this.checkedLastError = false;
    this._lastError = val;
  }

  /**
   * Normalizes the given error object for use by the target scope. If
   * the target is an error object which belongs to that scope, it is
   * returned as-is. If it is an ordinary object with a `message`
   * property, it is converted into an error belonging to the target
   * scope. If it is an Error object which does *not* belong to the
   * clone scope, it is reported, and converted to an unexpected
   * exception error.
   *
   * @param {Error|object} error
   * @returns {Error}
   */
  normalizeError(error) {
    if (error instanceof this.cloneScope.Error) {
      return error;
    }
    let message, fileName;
    if (error && typeof error === "object" &&
        (ChromeUtils.getClassName(error) === "Object" ||
         error instanceof ExtensionError ||
         this.principal.subsumes(Cu.getObjectPrincipal(error)))) {
      message = error.message;
      fileName = error.fileName;
    } else {
      Cu.reportError(error);
    }
    message = message || "An unexpected error occurred";
    return new this.cloneScope.Error(message, fileName);
  }

  /**
   * Sets the value of `.lastError` to `error`, calls the given
   * callback, and reports an error if the value has not been checked
   * when the callback returns.
   *
   * @param {object} error An object with a `message` property. May
   *     optionally be an `Error` object belonging to the target scope.
   * @param {function} callback The callback to call.
   * @returns {*} The return value of callback.
   */
  withLastError(error, callback) {
    this.lastError = this.normalizeError(error);
    try {
      return callback();
    } finally {
      if (!this.checkedLastError) {
        Cu.reportError(`Unchecked lastError value: ${this.lastError}`);
      }
      this.lastError = null;
    }
  }

  /**
   * Wraps the given promise so it can be safely returned to extension
   * code in this context.
   *
   * If `callback` is provided, however, it is used as a completion
   * function for the promise, and no promise is returned. In this case,
   * the callback is called when the promise resolves or rejects. In the
   * latter case, `lastError` is set to the rejection value, and the
   * callback function must check `browser.runtime.lastError` or
   * `extension.runtime.lastError` in order to prevent it being reported
   * to the console.
   *
   * @param {Promise} promise The promise with which to wrap the
   *     callback. May resolve to a `SpreadArgs` instance, in which case
   *     each element will be used as a separate argument.
   *
   *     Unless the promise object belongs to the cloneScope global, its
   *     resolution value is cloned into cloneScope prior to calling the
   *     `callback` function or resolving the wrapped promise.
   *
   * @param {function} [callback] The callback function to wrap
   *
   * @returns {Promise|undefined} If callback is null, a promise object
   *     belonging to the target scope. Otherwise, undefined.
   */
  wrapPromise(promise, callback = null) {
    let applySafe = this.applySafe.bind(this);
    if (Cu.getGlobalForObject(promise) === this.cloneScope) {
      applySafe = this.applySafeWithoutClone.bind(this);
    }

    if (callback) {
      promise.then(
        args => {
          if (this.unloaded) {
            dump(`Promise resolved after context unloaded\n`);
          } else if (!this.active) {
            dump(`Promise resolved while context is inactive\n`);
          } else if (args instanceof NoCloneSpreadArgs) {
            this.applySafeWithoutClone(callback, args.unwrappedValues);
          } else if (args instanceof SpreadArgs) {
            applySafe(callback, args);
          } else {
            applySafe(callback, [args]);
          }
        },
        error => {
          this.withLastError(error, () => {
            if (this.unloaded) {
              dump(`Promise rejected after context unloaded\n`);
            } else if (!this.active) {
              dump(`Promise rejected while context is inactive\n`);
            } else {
              this.applySafeWithoutClone(callback, []);
            }
          });
        });
    } else {
      return new this.cloneScope.Promise((resolve, reject) => {
        promise.then(
          value => {
            if (this.unloaded) {
              dump(`Promise resolved after context unloaded\n`);
            } else if (!this.active) {
              dump(`Promise resolved while context is inactive\n`);
            } else if (value instanceof NoCloneSpreadArgs) {
              let values = value.unwrappedValues;
              this.applySafeWithoutClone(resolve, values.length == 1 ? [values[0]] : [values]);
            } else if (value instanceof SpreadArgs) {
              applySafe(resolve, value.length == 1 ? value : [value]);
            } else {
              applySafe(resolve, [value]);
            }
          },
          value => {
            if (this.unloaded) {
              dump(`Promise rejected after context unloaded: ${value && value.message}\n`);
            } else if (!this.active) {
              dump(`Promise rejected while context is inactive: ${value && value.message}\n`);
            } else {
              this.applySafeWithoutClone(reject, [this.normalizeError(value)]);
            }
          });
      });
    }
  }

  unload() {
    this.unloaded = true;

    MessageChannel.abortResponses({
      extensionId: this.extension.id,
      contextId: this.contextId,
    });

    for (let obj of this.onClose) {
      obj.close();
    }
  }

  /**
   * A simple proxy for unload(), for use with callOnClose().
   */
  close() {
    this.unload();
  }
}

/**
 * An object that runs the implementation of a schema API. Instantiations of
 * this interfaces are used by Schemas.jsm.
 *
 * @interface
 */
class SchemaAPIInterface {
  /**
   * Calls this as a function that returns its return value.
   *
   * @abstract
   * @param {Array} args The parameters for the function.
   * @returns {*} The return value of the invoked function.
   */
  callFunction(args) {
    throw new Error("Not implemented");
  }

  /**
   * Calls this as a function and ignores its return value.
   *
   * @abstract
   * @param {Array} args The parameters for the function.
   */
  callFunctionNoReturn(args) {
    throw new Error("Not implemented");
  }

  /**
   * Calls this as a function that completes asynchronously.
   *
   * @abstract
   * @param {Array} args The parameters for the function.
   * @param {function(*)} [callback] The callback to be called when the function
   *     completes.
   * @param {boolean} [requireUserInput=false] If true, the function should
   *                  fail if the browser is not currently handling user input.
   * @returns {Promise|undefined} Must be void if `callback` is set, and a
   *     promise otherwise. The promise is resolved when the function completes.
   */
  callAsyncFunction(args, callback, requireUserInput = false) {
    throw new Error("Not implemented");
  }

  /**
   * Retrieves the value of this as a property.
   *
   * @abstract
   * @returns {*} The value of the property.
   */
  getProperty() {
    throw new Error("Not implemented");
  }

  /**
   * Assigns the value to this as property.
   *
   * @abstract
   * @param {string} value The new value of the property.
   */
  setProperty(value) {
    throw new Error("Not implemented");
  }

  /**
   * Registers a `listener` to this as an event.
   *
   * @abstract
   * @param {function} listener The callback to be called when the event fires.
   * @param {Array} args Extra parameters for EventManager.addListener.
   * @see EventManager.addListener
   */
  addListener(listener, args) {
    throw new Error("Not implemented");
  }

  /**
   * Checks whether `listener` is listening to this as an event.
   *
   * @abstract
   * @param {function} listener The event listener.
   * @returns {boolean} Whether `listener` is registered with this as an event.
   * @see EventManager.hasListener
   */
  hasListener(listener) {
    throw new Error("Not implemented");
  }

  /**
   * Unregisters `listener` from this as an event.
   *
   * @abstract
   * @param {function} listener The event listener.
   * @see EventManager.removeListener
   */
  removeListener(listener) {
    throw new Error("Not implemented");
  }

  /**
   * Revokes the implementation object, and prevents any further method
   * calls from having external effects.
   *
   * @abstract
   */
  revoke() {
    throw new Error("Not implemented");
  }
}

/**
 * An object that runs a locally implemented API.
 */
class LocalAPIImplementation extends SchemaAPIInterface {
  /**
   * Constructs an implementation of the `name` method or property of `pathObj`.
   *
   * @param {object} pathObj The object containing the member with name `name`.
   * @param {string} name The name of the implemented member.
   * @param {BaseContext} context The context in which the schema is injected.
   */
  constructor(pathObj, name, context) {
    super();
    this.pathObj = pathObj;
    this.name = name;
    this.context = context;
  }

  revoke() {
    if (this.pathObj[this.name][Schemas.REVOKE]) {
      this.pathObj[this.name][Schemas.REVOKE]();
    }

    this.pathObj = null;
    this.name = null;
    this.context = null;
  }

  callFunction(args) {
    return this.pathObj[this.name](...args);
  }

  callFunctionNoReturn(args) {
    this.pathObj[this.name](...args);
  }

  callAsyncFunction(args, callback, requireUserInput) {
    let promise;
    try {
      if (requireUserInput) {
        if (!getWinUtils(this.context.contentWindow).isHandlingUserInput) {
          throw new ExtensionError(`${this.name} may only be called from a user input handler`);
        }
      }
      promise = this.pathObj[this.name](...args) || Promise.resolve();
    } catch (e) {
      promise = Promise.reject(e);
    }
    return this.context.wrapPromise(promise, callback);
  }

  getProperty() {
    return this.pathObj[this.name];
  }

  setProperty(value) {
    this.pathObj[this.name] = value;
  }

  addListener(listener, args) {
    try {
      this.pathObj[this.name].addListener.call(null, listener, ...args);
    } catch (e) {
      throw this.context.normalizeError(e);
    }
  }

  hasListener(listener) {
    return this.pathObj[this.name].hasListener.call(null, listener);
  }

  removeListener(listener) {
    this.pathObj[this.name].removeListener.call(null, listener);
  }
}

// Recursively copy properties from source to dest.
function deepCopy(dest, source) {
  for (let prop in source) {
    let desc = Object.getOwnPropertyDescriptor(source, prop);
    if (typeof(desc.value) == "object") {
      if (!(prop in dest)) {
        dest[prop] = {};
      }
      deepCopy(dest[prop], source[prop]);
    } else {
      Object.defineProperty(dest, prop, desc);
    }
  }
}

function getChild(map, key) {
  let child = map.children.get(key);
  if (!child) {
    child = {
      modules: new Set(),
      children: new Map(),
    };

    map.children.set(key, child);
  }
  return child;
}

function getPath(map, path) {
  for (let key of path) {
    map = getChild(map, key);
  }
  return map;
}

/**
 * Manages loading and accessing a set of APIs for a specific extension
 * context.
 *
 * @param {BaseContext} context
 *        The context to manage APIs for.
 * @param {SchemaAPIManager} apiManager
 *        The API manager holding the APIs to manage.
 * @param {object} root
 *        The root object into which APIs will be injected.
 */
class CanOfAPIs {
  constructor(context, apiManager, root) {
    this.context = context;
    this.scopeName = context.envType;
    this.apiManager = apiManager;
    this.root = root;

    this.apiPaths = new Map();

    this.apis = new Map();
  }

  /**
   * Synchronously loads and initializes an ExtensionAPI instance.
   *
   * @param {string} name
   *        The name of the API to load.
   */
  loadAPI(name) {
    if (this.apis.has(name)) {
      return;
    }

    let {extension} = this.context;

    let api = this.apiManager.getAPI(name, extension, this.scopeName);
    if (!api) {
      return;
    }

    this.apis.set(name, api);

    deepCopy(this.root, api.getAPI(this.context));
  }

  /**
   * Asynchronously loads and initializes an ExtensionAPI instance.
   *
   * @param {string} name
   *        The name of the API to load.
   */
  async asyncLoadAPI(name) {
    if (this.apis.has(name)) {
      return;
    }

    let {extension} = this.context;
    if (!Schemas.checkPermissions(name, extension)) {
      return;
    }

    let api = await this.apiManager.asyncGetAPI(name, extension, this.scopeName);
    // Check again, because async;
    if (this.apis.has(name)) {
      return;
    }

    this.apis.set(name, api);

    deepCopy(this.root, api.getAPI(this.context));
  }

  /**
   * Finds the API at the given path from the root object, and
   * synchronously loads the API that implements it if it has not
   * already been loaded.
   *
   * @param {string} path
   *        The "."-separated path to find.
   * @returns {*}
   */
  findAPIPath(path) {
    if (this.apiPaths.has(path)) {
      return this.apiPaths.get(path);
    }

    let obj = this.root;
    let modules = this.apiManager.modulePaths;

    for (let key of path.split(".")) {
      if (!obj) {
        return;
      }
      modules = getChild(modules, key);

      for (let name of modules.modules) {
        if (!this.apis.has(name)) {
          this.loadAPI(name);
        }
      }

      obj = obj[key];
    }

    this.apiPaths.set(path, obj);
    return obj;
  }

  /**
   * Finds the API at the given path from the root object, and
   * asynchronously loads the API that implements it if it has not
   * already been loaded.
   *
   * @param {string} path
   *        The "."-separated path to find.
   * @returns {Promise<*>}
   */
  async asyncFindAPIPath(path) {
    if (this.apiPaths.has(path)) {
      return this.apiPaths.get(path);
    }

    let obj = this.root;
    let modules = this.apiManager.modulePaths;

    for (let key of path.split(".")) {
      if (!obj) {
        return;
      }
      modules = getChild(modules, key);

      for (let name of modules.modules) {
        if (!this.apis.has(name)) {
          await this.asyncLoadAPI(name);
        }
      }

      if (typeof obj[key] === "function") {
        obj = obj[key].bind(obj);
      } else {
        obj = obj[key];
      }
    }

    this.apiPaths.set(path, obj);
    return obj;
  }
}

/**
 * @class APIModule
 * @abstract
 *
 * @property {string} url
 *       The URL of the script which contains the module's
 *       implementation. This script must define a global property
 *       matching the modules name, which must be a class constructor
 *       which inherits from {@link ExtensionAPI}.
 *
 * @property {string} schema
 *       The URL of the JSON schema which describes the module's API.
 *
 * @property {Array<string>} scopes
 *       The list of scope names into which the API may be loaded.
 *
 * @property {Array<string>} manifest
 *       The list of top-level manifest properties which will trigger
 *       the module to be loaded, and its `onManifestEntry` method to be
 *       called.
 *
 * @property {Array<string>} events
 *       The list events which will trigger the module to be loaded, and
 *       its appropriate event handler method to be called. Currently
 *       only accepts "startup".
 *
 * @property {Array<string>} permissions
 *       An optional list of permissions, any of which must be present
 *       in order for the module to load.
 *
 * @property {Array<Array<string>>} paths
 *       A list of paths from the root API object which, when accessed,
 *       will cause the API module to be instantiated and injected.
 */

/**
 * This object loads the ext-*.js scripts that define the extension API.
 *
 * This class instance is shared with the scripts that it loads, so that the
 * ext-*.js scripts and the instantiator can communicate with each other.
 */
class SchemaAPIManager extends EventEmitter {
  /**
   * @param {string} processType
   *     "main" - The main, one and only chrome browser process.
   *     "addon" - An addon process.
   *     "content" - A content process.
   *     "devtools" - A devtools process.
   *     "proxy" - A proxy script process.
   */
  constructor(processType) {
    super();
    this.processType = processType;
    this.global = this._createExtGlobal();

    this.modules = new Map();
    this.modulePaths = {children: new Map(), modules: new Set()};
    this.manifestKeys = new Map();
    this.eventModules = new DefaultMap(() => new Set());

    this._modulesJSONLoaded = false;

    this.schemaURLs = new Map();

    this.apis = new DefaultWeakMap(() => new Map());

    this._scriptScopes = [];
  }

  async loadModuleJSON(urls) {
    function fetchJSON(url) {
      return fetch(url).then(resp => resp.json());
    }

    for (let json of await Promise.all(urls.map(fetchJSON))) {
      this.registerModules(json);
    }

    this._modulesJSONLoaded = true;

    return new StructuredCloneHolder({
      modules: this.modules,
      modulePaths: this.modulePaths,
      manifestKeys: this.manifestKeys,
      eventModules: this.eventModules,
      schemaURLs: this.schemaURLs,
    });
  }

  initModuleData(moduleData) {
    if (!this._modulesJSONLoaded) {
      let data = moduleData.deserialize({});

      this.modules = data.modules;
      this.modulePaths = data.modulePaths;
      this.manifestKeys = data.manifestKeys;
      this.eventModules = new DefaultMap(() => new Set(),
                                         data.eventModules);
      this.schemaURLs = data.schemaURLs;
    }

    this._modulesJSONLoaded = true;
  }

  /**
   * Registers a set of ExtensionAPI modules to be lazily loaded and
   * managed by this manager.
   *
   * @param {object} obj
   *        An object containing property for eacy API module to be
   *        registered. Each value should be an object implementing the
   *        APIModule interface.
   */
  registerModules(obj) {
    for (let [name, details] of Object.entries(obj)) {
      details.namespaceName = name;

      if (this.modules.has(name)) {
        throw new Error(`Module '${name}' already registered`);
      }
      this.modules.set(name, details);

      if (details.schema) {
        let content = (details.scopes &&
                       (details.scopes.includes("content_parent") ||
                        details.scopes.includes("content_child")));
        this.schemaURLs.set(details.schema, {content});
      }

      for (let event of details.events || []) {
        this.eventModules.get(event).add(name);
      }

      for (let key of details.manifest || []) {
        if (this.manifestKeys.has(key)) {
          throw new Error(`Manifest key '${key}' already registered by '${this.manifestKeys.get(key)}'`);
        }

        this.manifestKeys.set(key, name);
      }

      for (let path of details.paths || []) {
        getPath(this.modulePaths, path).modules.add(name);
      }
    }
  }

  /**
   * Emits an `onManifestEntry` event for the top-level manifest entry
   * on all relevant {@link ExtensionAPI} instances for the given
   * extension.
   *
   * The API modules will be synchronously loaded if they have not been
   * loaded already.
   *
   * @param {Extension} extension
   *        The extension for which to emit the events.
   * @param {string} entry
   *        The name of the top-level manifest entry.
   *
   * @returns {*}
   */
  emitManifestEntry(extension, entry) {
    let apiName = this.manifestKeys.get(entry);
    if (apiName) {
      let api = this.getAPI(apiName, extension);
      return api.onManifestEntry(entry);
    }
  }
  /**
   * Emits an `onManifestEntry` event for the top-level manifest entry
   * on all relevant {@link ExtensionAPI} instances for the given
   * extension.
   *
   * The API modules will be asynchronously loaded if they have not been
   * loaded already.
   *
   * @param {Extension} extension
   *        The extension for which to emit the events.
   * @param {string} entry
   *        The name of the top-level manifest entry.
   *
   * @returns {Promise<*>}
   */
  async asyncEmitManifestEntry(extension, entry) {
    let apiName = this.manifestKeys.get(entry);
    if (apiName) {
      let api = await this.asyncGetAPI(apiName, extension);
      return api.onManifestEntry(entry);
    }
  }

  /**
   * Returns the {@link ExtensionAPI} instance for the given API module,
   * for the given extension, in the given scope, synchronously loading
   * and instantiating it if necessary.
   *
   * @param {string} name
   *        The name of the API module to load.
   * @param {Extension} extension
   *        The extension for which to load the API.
   * @param {string} [scope = null]
   *        The scope type for which to retrieve the API, or null if not
   *        being retrieved for a particular scope.
   *
   * @returns {ExtensionAPI?}
   */
  getAPI(name, extension, scope = null) {
    if (!this._checkGetAPI(name, extension, scope)) {
      return;
    }

    let apis = this.apis.get(extension);
    if (apis.has(name)) {
      return apis.get(name);
    }

    let module = this.loadModule(name);

    let api = new module(extension);
    apis.set(name, api);
    return api;
  }
  /**
   * Returns the {@link ExtensionAPI} instance for the given API module,
   * for the given extension, in the given scope, asynchronously loading
   * and instantiating it if necessary.
   *
   * @param {string} name
   *        The name of the API module to load.
   * @param {Extension} extension
   *        The extension for which to load the API.
   * @param {string} [scope = null]
   *        The scope type for which to retrieve the API, or null if not
   *        being retrieved for a particular scope.
   *
   * @returns {Promise<ExtensionAPI>?}
   */
  async asyncGetAPI(name, extension, scope = null) {
    if (!this._checkGetAPI(name, extension, scope)) {
      return;
    }

    let apis = this.apis.get(extension);
    if (apis.has(name)) {
      return apis.get(name);
    }

    let module = await this.asyncLoadModule(name);

    // Check again, because async.
    if (apis.has(name)) {
      return apis.get(name);
    }

    let api = new module(extension);
    apis.set(name, api);
    return api;
  }

  /**
   * Synchronously loads an API module, if not already loaded, and
   * returns its ExtensionAPI constructor.
   *
   * @param {string} name
   *        The name of the module to load.
   *
   * @returns {class}
   */
  loadModule(name) {
    let module = this.modules.get(name);
    if (module.loaded) {
      return this.global[name];
    }

    this._checkLoadModule(module, name);

    Services.scriptloader.loadSubScript(module.url, this.global, "UTF-8");

    module.loaded = true;

    return this.global[name];
  }
  /**
   * aSynchronously loads an API module, if not already loaded, and
   * returns its ExtensionAPI constructor.
   *
   * @param {string} name
   *        The name of the module to load.
   *
   * @returns {Promise<class>}
   */
  asyncLoadModule(name) {
    let module = this.modules.get(name);
    if (module.loaded) {
      return Promise.resolve(this.global[name]);
    }
    if (module.asyncLoaded) {
      return module.asyncLoaded;
    }

    this._checkLoadModule(module, name);

    module.asyncLoaded = ChromeUtils.compileScript(module.url).then(script => {
      script.executeInGlobal(this.global);

      module.loaded = true;

      return this.global[name];
    });

    return module.asyncLoaded;
  }

  /**
   * Checks whether the given API module may be loaded for the given
   * extension, in the given scope.
   *
   * @param {string} name
   *        The name of the API module to check.
   * @param {Extension} extension
   *        The extension for which to check the API.
   * @param {string} [scope = null]
   *        The scope type for which to check the API, or null if not
   *        being checked for a particular scope.
   *
   * @returns {boolean}
   *        Whether the module may be loaded.
   */
  _checkGetAPI(name, extension, scope = null) {
    let module = this.modules.get(name);

    if (module.permissions && !module.permissions.some(perm => extension.hasPermission(perm))) {
      return false;
    }

    if (!scope) {
      return true;
    }

    if (!module.scopes.includes(scope)) {
      return false;
    }

    if (!Schemas.checkPermissions(module.namespaceName, extension)) {
      return false;
    }

    return true;
  }

  _checkLoadModule(module, name) {
    if (!module) {
      throw new Error(`Module '${name}' does not exist`);
    }
    if (module.asyncLoaded) {
      throw new Error(`Module '${name}' currently being lazily loaded`);
    }
    if (this.global[name]) {
      throw new Error(`Module '${name}' conflicts with existing global property`);
    }
  }


  /**
   * Create a global object that is used as the shared global for all ext-*.js
   * scripts that are loaded via `loadScript`.
   *
   * @returns {object} A sandbox that is used as the global by `loadScript`.
   */
  _createExtGlobal() {
    let global = Cu.Sandbox(Services.scriptSecurityManager.getSystemPrincipal(), {
      wantXrays: false,
      sandboxName: `Namespace of ext-*.js scripts for ${this.processType} (from: resource://gre/modules/ExtensionCommon.jsm)`,
    });

    Object.assign(global, {
      Cc,
      ChromeUtils,
      ChromeWorker,
      Ci,
      Cr,
      Cu,
      ExtensionAPI,
      ExtensionCommon,
      MatchGlob,
      MatchPattern,
      MatchPatternSet,
      StructuredCloneHolder,
      XPCOMUtils,
      extensions: this,
      global,
    });

    Cu.import("resource://gre/modules/AppConstants.jsm", global);

    XPCOMUtils.defineLazyGetter(global, "console", getConsole);

    XPCOMUtils.defineLazyModuleGetters(global, {
      ExtensionUtils: "resource://gre/modules/ExtensionUtils.jsm",
      XPCOMUtils: "resource://gre/modules/XPCOMUtils.jsm",
    });

    return global;
  }

  /**
   * Load an ext-*.js script. The script runs in its own scope, if it wishes to
   * share state with another script it can assign to the `global` variable. If
   * it wishes to communicate with this API manager, use `extensions`.
   *
   * @param {string} scriptUrl The URL of the ext-*.js script.
   */
  loadScript(scriptUrl) {
    // Create the object in the context of the sandbox so that the script runs
    // in the sandbox's context instead of here.
    let scope = Cu.createObjectIn(this.global);

    Services.scriptloader.loadSubScript(scriptUrl, scope, "UTF-8");

    // Save the scope to avoid it being garbage collected.
    this._scriptScopes.push(scope);
  }

  /**
   * Mash together all the APIs from `apis` into `obj`.
   *
   * @param {BaseContext} context The context for which the API bindings are
   *     generated.
   * @param {Array} apis A list of objects, see `registerSchemaAPI`.
   * @param {object} obj The destination of the API.
   */
  static generateAPIs(context, apis, obj) {
    function hasPermission(perm) {
      return context.extension.hasPermission(perm, true);
    }
    for (let api of apis) {
      if (Schemas.checkPermissions(api.namespace, {hasPermission})) {
        api = api.getAPI(context);
        deepCopy(obj, api);
      }
    }
  }
}

function LocaleData(data) {
  this.defaultLocale = data.defaultLocale;
  this.selectedLocale = data.selectedLocale;
  this.locales = data.locales || new Map();
  this.warnedMissingKeys = new Set();

  // Map(locale-name -> Map(message-key -> localized-string))
  //
  // Contains a key for each loaded locale, each of which is a
  // Map of message keys to their localized strings.
  this.messages = data.messages || new Map();

  if (data.builtinMessages) {
    this.messages.set(this.BUILTIN, data.builtinMessages);
  }
}

LocaleData.prototype = {
  // Representation of the object to send to content processes. This
  // should include anything the content process might need.
  serialize() {
    return {
      defaultLocale: this.defaultLocale,
      selectedLocale: this.selectedLocale,
      messages: this.messages,
      locales: this.locales,
    };
  },

  BUILTIN: "@@BUILTIN_MESSAGES",

  has(locale) {
    return this.messages.has(locale);
  },

  // https://developer.chrome.com/extensions/i18n
  localizeMessage(message, substitutions = [], options = {}) {
    let defaultOptions = {
      defaultValue: "",
      cloneScope: null,
    };

    let locales = this.availableLocales;
    if (options.locale) {
      locales = new Set([this.BUILTIN, options.locale, this.defaultLocale]
                        .filter(locale => this.messages.has(locale)));
    }

    options = Object.assign(defaultOptions, options);

    // Message names are case-insensitive, so normalize them to lower-case.
    message = message.toLowerCase();
    for (let locale of locales) {
      let messages = this.messages.get(locale);
      if (messages.has(message)) {
        let str = messages.get(message);

        if (!str.includes("$")) {
          return str;
        }

        if (!Array.isArray(substitutions)) {
          substitutions = [substitutions];
        }

        let replacer = (matched, index, dollarSigns) => {
          if (index) {
            // This is not quite Chrome-compatible. Chrome consumes any number
            // of digits following the $, but only accepts 9 substitutions. We
            // accept any number of substitutions.
            index = parseInt(index, 10) - 1;
            return index in substitutions ? substitutions[index] : "";
          }
          // For any series of contiguous `$`s, the first is dropped, and
          // the rest remain in the output string.
          return dollarSigns;
        };
        return str.replace(/\$(?:([1-9]\d*)|(\$+))/g, replacer);
      }
    }

    // Check for certain pre-defined messages.
    if (message == "@@ui_locale") {
      return this.uiLocale;
    } else if (message.startsWith("@@bidi_")) {
      let rtl = Services.locale.isAppLocaleRTL;

      if (message == "@@bidi_dir") {
        return rtl ? "rtl" : "ltr";
      } else if (message == "@@bidi_reversed_dir") {
        return rtl ? "ltr" : "rtl";
      } else if (message == "@@bidi_start_edge") {
        return rtl ? "right" : "left";
      } else if (message == "@@bidi_end_edge") {
        return rtl ? "left" : "right";
      }
    }

    if (!this.warnedMissingKeys.has(message)) {
      let error = `Unknown localization message ${message}`;
      if (options.cloneScope) {
        error = new options.cloneScope.Error(error);
      }
      Cu.reportError(error);
      this.warnedMissingKeys.add(message);
    }
    return options.defaultValue;
  },

  // Localize a string, replacing all |__MSG_(.*)__| tokens with the
  // matching string from the current locale, as determined by
  // |this.selectedLocale|.
  //
  // This may not be called before calling either |initLocale| or
  // |initAllLocales|.
  localize(str, locale = this.selectedLocale) {
    if (!str) {
      return str;
    }

    return str.replace(/__MSG_([A-Za-z0-9@_]+?)__/g, (matched, message) => {
      return this.localizeMessage(message, [], {locale, defaultValue: matched});
    });
  },

  // Validates the contents of a locale JSON file, normalizes the
  // messages into a Map of message key -> localized string pairs.
  addLocale(locale, messages, extension) {
    let result = new Map();

    let isPlainObject = obj => (obj && typeof obj === "object" &&
                                ChromeUtils.getClassName(obj) === "Object");

    // Chrome does not document the semantics of its localization
    // system very well. It handles replacements by pre-processing
    // messages, replacing |$[a-zA-Z0-9@_]+$| tokens with the value of their
    // replacements. Later, it processes the resulting string for
    // |$[0-9]| replacements.
    //
    // Again, it does not document this, but it accepts any number
    // of sequential |$|s, and replaces them with that number minus
    // 1. It also accepts |$| followed by any number of sequential
    // digits, but refuses to process a localized string which
    // provides more than 9 substitutions.
    if (!isPlainObject(messages)) {
      extension.packagingError(`Invalid locale data for ${locale}`);
      return result;
    }

    for (let key of Object.keys(messages)) {
      let msg = messages[key];

      if (!isPlainObject(msg) || typeof(msg.message) != "string") {
        extension.packagingError(`Invalid locale message data for ${locale}, message ${JSON.stringify(key)}`);
        continue;
      }

      // Substitutions are case-insensitive, so normalize all of their names
      // to lower-case.
      let placeholders = new Map();
      if (isPlainObject(msg.placeholders)) {
        for (let key of Object.keys(msg.placeholders)) {
          placeholders.set(key.toLowerCase(), msg.placeholders[key]);
        }
      }

      let replacer = (match, name) => {
        let replacement = placeholders.get(name.toLowerCase());
        if (isPlainObject(replacement) && "content" in replacement) {
          return replacement.content;
        }
        return "";
      };

      let value = msg.message.replace(/\$([A-Za-z0-9@_]+)\$/g, replacer);

      // Message names are also case-insensitive, so normalize them to lower-case.
      result.set(key.toLowerCase(), value);
    }

    this.messages.set(locale, result);
    return result;
  },

  get acceptLanguages() {
    let result = Services.prefs.getComplexValue("intl.accept_languages", Ci.nsIPrefLocalizedString).data;
    return result.split(/\s*,\s*/g);
  },


  get uiLocale() {
    return Services.locale.getAppLocaleAsBCP47();
  },
};

defineLazyGetter(LocaleData.prototype, "availableLocales", function() {
  return new Set([this.BUILTIN, this.selectedLocale, this.defaultLocale]
                 .filter(locale => this.messages.has(locale)));
});

/**
* This is a generic class for managing event listeners.
 *
 * @example
 * new EventManager(context, "api.subAPI", fire => {
 *   let listener = (...) => {
 *     // Fire any listeners registered with addListener.
 *     fire.async(arg1, arg2);
 *   };
 *   // Register the listener.
 *   SomehowRegisterListener(listener);
 *   return () => {
 *     // Return a way to unregister the listener.
 *     SomehowUnregisterListener(listener);
 *   };
 * }).api()
 *
 * The result is an object with addListener, removeListener, and
 * hasListener methods. `context` is an add-on scope (either an
 * ExtensionContext in the chrome process or ExtensionContext in a
 * content process). `name` is for debugging. `register` is a function
 * to register the listener. `register` should return an
 * unregister function that will unregister the listener.
 * @constructor
 *
 * @param {BaseContext} context
 *        An object representing the extension instance using this event.
 * @param {string} name
 *        A name used only for debugging.
 * @param {functon} register
 *        A function called whenever a new listener is added.
 */
function EventManager(context, name, register) {
  this.context = context;
  this.name = name;
  this.register = register;
  this.unregister = new Map();
  this.inputHandling = false;
}

EventManager.prototype = {
  addListener(callback, ...args) {
    if (this.unregister.has(callback)) {
      return;
    }

    let shouldFire = () => {
      if (this.context.unloaded) {
        dump(`${this.name} event fired after context unloaded.\n`);
      } else if (!this.context.active) {
        dump(`${this.name} event fired while context is inactive.\n`);
      } else if (this.unregister.has(callback)) {
        return true;
      }
      return false;
    };

    let fire = {
      sync: (...args) => {
        if (shouldFire()) {
          return this.context.applySafe(callback, args);
        }
      },
      async: (...args) => {
        return Promise.resolve().then(() => {
          if (shouldFire()) {
            return this.context.applySafe(callback, args);
          }
        });
      },
      raw: (...args) => {
        if (!shouldFire()) {
          throw new Error("Called raw() on unloaded/inactive context");
        }
        return Reflect.apply(callback, null, args);
      },
      asyncWithoutClone: (...args) => {
        return Promise.resolve().then(() => {
          if (shouldFire()) {
            return this.context.applySafeWithoutClone(callback, args);
          }
        });
      },
    };


    let unregister = this.register(fire, ...args);
    this.unregister.set(callback, unregister);
    this.context.callOnClose(this);
  },

  removeListener(callback) {
    if (!this.unregister.has(callback)) {
      return;
    }

    let unregister = this.unregister.get(callback);
    this.unregister.delete(callback);
    try {
      unregister();
    } catch (e) {
      Cu.reportError(e);
    }
    if (this.unregister.size == 0) {
      this.context.forgetOnClose(this);
    }
  },

  hasListener(callback) {
    return this.unregister.has(callback);
  },

  revoke() {
    for (let callback of this.unregister.keys()) {
      this.removeListener(callback);
    }
  },

  close() {
    this.revoke();
  },

  api() {
    return {
      addListener: (...args) => this.addListener(...args),
      removeListener: (...args) => this.removeListener(...args),
      hasListener: (...args) => this.hasListener(...args),
      setUserInput: this.inputHandling,
      [Schemas.REVOKE]: () => this.revoke(),
    };
  },
};

// Simple API for event listeners where events never fire.
function ignoreEvent(context, name) {
  return {
    addListener: function(callback) {
      let id = context.extension.id;
      let frame = Components.stack.caller;
      let msg = `In add-on ${id}, attempting to use listener "${name}", which is unimplemented.`;
      let scriptError = Cc["@mozilla.org/scripterror;1"]
        .createInstance(Ci.nsIScriptError);
      scriptError.init(msg, frame.filename, null, frame.lineNumber,
                       frame.columnNumber, Ci.nsIScriptError.warningFlag,
                       "content javascript");
      Services.console.logMessage(scriptError);
    },
    removeListener: function(callback) {},
    hasListener: function(callback) {},
  };
}


const stylesheetMap = new DefaultMap(url => {
  let uri = Services.io.newURI(url);
  return styleSheetService.preloadSheet(uri, styleSheetService.AGENT_SHEET);
});


ExtensionCommon = {
  BaseContext,
  CanOfAPIs,
  EventManager,
  ExtensionAPI,
  ExtensionAPIs,
  LocalAPIImplementation,
  LocaleData,
  NoCloneSpreadArgs,
  SchemaAPIInterface,
  SchemaAPIManager,
  SpreadArgs,
  ignoreEvent,
  stylesheetMap,
};
