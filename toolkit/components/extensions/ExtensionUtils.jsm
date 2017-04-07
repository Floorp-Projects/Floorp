/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionUtils"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

const INTEGER = /^[1-9]\d*$/;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ConsoleAPI",
                                  "resource://gre/modules/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionManagement",
                                  "resource://gre/modules/ExtensionManagement.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "IndexedDB",
                                  "resource://gre/modules/IndexedDB.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MessageChannel",
                                  "resource://gre/modules/MessageChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "styleSheetService",
                                   "@mozilla.org/content/style-sheet-service;1",
                                   "nsIStyleSheetService");

/* globals IDBKeyRange */

function getConsole() {
  return new ConsoleAPI({
    maxLogLevelPref: "extensions.webextensions.log.level",
    prefix: "WebExtensions",
  });
}

XPCOMUtils.defineLazyGetter(this, "console", getConsole);

let nextId = 0;
XPCOMUtils.defineLazyGetter(this, "uniqueProcessID", () => Services.appinfo.uniqueProcessID);

function getUniqueId() {
  return `${nextId++}-${uniqueProcessID}`;
}

let StartupCache = {
  DB_NAME: "ExtensionStartupCache",

  SCHEMA_VERSION: 2,

  STORE_NAMES: Object.freeze(["locales", "manifests", "schemas"]),

  dbPromise: null,

  cacheInvalidated: 0,

  initDB(db) {
    for (let name of StartupCache.STORE_NAMES) {
      try {
        db.deleteObjectStore(name);
      } catch (e) {
        // Don't worry if the store doesn't already exist.
      }
      db.createObjectStore(name, {keyPath: "key"});
    }
  },

  clearAddonData(id) {
    let range = IDBKeyRange.bound([id], [id, "\uFFFF"]);

    return Promise.all([
      this.locales.delete(range),
      this.manifests.delete(range),
    ]).catch(e => {
      // Ignore the error. It happens when we try to flush the add-on
      // data after the AddonManager has flushed the entire startup cache.
    });
  },

  async reallyOpen(invalidate = false) {
    if (this.dbPromise) {
      let db = await this.dbPromise;
      db.close();
    }

    if (invalidate) {
      this.cacheInvalidated = ExtensionManagement.cacheInvalidated;

      if (Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT) {
        IndexedDB.deleteDatabase(this.DB_NAME, {storage: "persistent"});
      }
    }

    return IndexedDB.open(this.DB_NAME,
                          {storage: "persistent", version: this.SCHEMA_VERSION},
                          db => this.initDB(db));
  },

  async open() {
    if (ExtensionManagement.cacheInvalidated > this.cacheInvalidated) {
      this.dbPromise = this.reallyOpen(true);
    } else if (!this.dbPromise) {
      this.dbPromise = this.reallyOpen();
    }

    return this.dbPromise;
  },

  observe(subject, topic, data) {
    if (topic === "startupcache-invalidate") {
      this.dbPromise = this.reallyOpen(true).catch(e => {});
    }
  },
};

Services.obs.addObserver(StartupCache, "startupcache-invalidate");

class CacheStore {
  constructor(storeName) {
    this.storeName = storeName;
  }

  async get(key, createFunc) {
    let db;
    let result;
    try {
      db = await StartupCache.open();

      result = await db.objectStore(this.storeName)
                      .get(key);
    } catch (e) {
      Cu.reportError(e);

      return createFunc(key);
    }

    if (result === undefined) {
      let value = await createFunc(key);
      result = {key, value};

      db.objectStore(this.storeName, "readwrite")
        .put(result);
    }

    return result && result.value;
  }

  async getAll() {
    let result = new Map();
    try {
      let db = await StartupCache.open();

      let results = await db.objectStore(this.storeName)
                            .getAll();
      for (let {key, value} of results) {
        result.set(key, value);
      }
    } catch (e) {
      Cu.reportError(e);
    }

    return result;
  }

  async delete(key) {
    let db = await StartupCache.open();

    return db.objectStore(this.storeName, "readwrite").delete(key);
  }
}

for (let name of StartupCache.STORE_NAMES) {
  StartupCache[name] = new CacheStore(name);
}

/**
 * An Error subclass for which complete error messages are always passed
 * to extensions, rather than being interpreted as an unknown error.
 */
class ExtensionError extends Error {}

function filterStack(error) {
  return String(error.stack).replace(/(^.*(Task\.jsm|Promise-backend\.js).*\n)+/gm, "<Promise Chain>\n");
}

// Run a function and report exceptions.
function runSafeSyncWithoutClone(f, ...args) {
  try {
    return f(...args);
  } catch (e) {
    dump(`Extension error: ${e} ${e.fileName} ${e.lineNumber}\n[[Exception stack\n${filterStack(e)}Current stack\n${filterStack(Error())}]]\n`);
    Cu.reportError(e);
  }
}

// Run a function and report exceptions.
function runSafeWithoutClone(f, ...args) {
  if (typeof(f) != "function") {
    dump(`Extension error: expected function\n${filterStack(Error())}`);
    return;
  }

  Promise.resolve().then(() => {
    runSafeSyncWithoutClone(f, ...args);
  });
}

// Run a function, cloning arguments into context.cloneScope, and
// report exceptions. |f| is expected to be in context.cloneScope.
function runSafeSync(context, f, ...args) {
  if (context.unloaded) {
    Cu.reportError("runSafeSync called after context unloaded");
    return;
  }

  try {
    args = Cu.cloneInto(args, context.cloneScope);
  } catch (e) {
    Cu.reportError(e);
    dump(`runSafe failure: cloning into ${context.cloneScope}: ${e}\n\n${filterStack(Error())}`);
  }
  return runSafeSyncWithoutClone(f, ...args);
}

// Run a function, cloning arguments into context.cloneScope, and
// report exceptions. |f| is expected to be in context.cloneScope.
function runSafe(context, f, ...args) {
  try {
    args = Cu.cloneInto(args, context.cloneScope);
  } catch (e) {
    Cu.reportError(e);
    dump(`runSafe failure: cloning into ${context.cloneScope}: ${e}\n\n${filterStack(Error())}`);
  }
  if (context.unloaded) {
    dump(`runSafe failure: context is already unloaded ${filterStack(new Error())}\n`);
    return undefined;
  }
  return runSafeWithoutClone(f, ...args);
}

// Return true if the given value is an instance of the given
// native type.
function instanceOf(value, type) {
  return {}.toString.call(value) == `[object ${type}]`;
}

// Extend the object |obj| with the property descriptors of each object in
// |args|.
function extend(obj, ...args) {
  for (let arg of args) {
    let props = [...Object.getOwnPropertyNames(arg),
                 ...Object.getOwnPropertySymbols(arg)];
    for (let prop of props) {
      let descriptor = Object.getOwnPropertyDescriptor(arg, prop);
      Object.defineProperty(obj, prop, descriptor);
    }
  }

  return obj;
}

/**
 * Similar to a WeakMap, but creates a new key with the given
 * constructor if one is not present.
 */
class DefaultWeakMap extends WeakMap {
  constructor(defaultConstructor, init) {
    super(init);
    this.defaultConstructor = defaultConstructor;
  }

  get(key) {
    if (!this.has(key)) {
      this.set(key, this.defaultConstructor(key));
    }
    return super.get(key);
  }
}

class DefaultMap extends Map {
  constructor(defaultConstructor, init) {
    super(init);
    this.defaultConstructor = defaultConstructor;
  }

  get(key) {
    if (!this.has(key)) {
      this.set(key, this.defaultConstructor(key));
    }
    return super.get(key);
  }
}

const _winUtils = new DefaultWeakMap(win => {
  return win.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIDOMWindowUtils);
});
const getWinUtils = win => _winUtils.get(win);

function getInnerWindowID(window) {
  return getWinUtils(window).currentInnerWindowID;
}

class SpreadArgs extends Array {
  constructor(args) {
    super();
    this.push(...args);
  }
}

// Manages icon details for toolbar buttons in the |pageAction| and
// |browserAction| APIs.
let IconDetails = {
  // Normalizes the various acceptable input formats into an object
  // with icon size as key and icon URL as value.
  //
  // If a context is specified (function is called from an extension):
  // Throws an error if an invalid icon size was provided or the
  // extension is not allowed to load the specified resources.
  //
  // If no context is specified, instead of throwing an error, this
  // function simply logs a warning message.
  normalize(details, extension, context = null) {
    let result = {};

    try {
      if (details.imageData) {
        let imageData = details.imageData;

        if (typeof imageData == "string") {
          imageData = {"19": imageData};
        }

        for (let size of Object.keys(imageData)) {
          if (!INTEGER.test(size)) {
            throw new ExtensionError(`Invalid icon size ${size}, must be an integer`);
          }
          result[size] = imageData[size];
        }
      }

      if (details.path) {
        let path = details.path;
        if (typeof path != "object") {
          path = {"19": path};
        }

        let baseURI = context ? context.uri : extension.baseURI;

        for (let size of Object.keys(path)) {
          if (!INTEGER.test(size)) {
            throw new ExtensionError(`Invalid icon size ${size}, must be an integer`);
          }

          let url = baseURI.resolve(path[size]);

          // The Chrome documentation specifies these parameters as
          // relative paths. We currently accept absolute URLs as well,
          // which means we need to check that the extension is allowed
          // to load them. This will throw an error if it's not allowed.
          try {
            Services.scriptSecurityManager.checkLoadURIStrWithPrincipal(
              extension.principal, url,
              Services.scriptSecurityManager.DISALLOW_SCRIPT);
          } catch (e) {
            throw new ExtensionError(`Illegal URL ${url}`);
          }

          result[size] = url;
        }
      }
    } catch (e) {
      // Function is called from extension code, delegate error.
      if (context) {
        throw e;
      }
      // If there's no context, it's because we're handling this
      // as a manifest directive. Log a warning rather than
      // raising an error.
      extension.manifestError(`Invalid icon data: ${e}`);
    }

    return result;
  },

  // Returns the appropriate icon URL for the given icons object and the
  // screen resolution of the given window.
  getPreferredIcon(icons, extension = null, size = 16) {
    const DEFAULT = "chrome://browser/content/extension.svg";

    let bestSize = null;
    if (icons[size]) {
      bestSize = size;
    } else if (icons[2 * size]) {
      bestSize = 2 * size;
    } else {
      let sizes = Object.keys(icons)
                        .map(key => parseInt(key, 10))
                        .sort((a, b) => a - b);

      bestSize = sizes.find(candidate => candidate > size) || sizes.pop();
    }

    if (bestSize) {
      return {size: bestSize, icon: icons[bestSize]};
    }

    return {size, icon: DEFAULT};
  },

  convertImageURLToDataURL(imageURL, contentWindow, browserWindow, size = 18) {
    return new Promise((resolve, reject) => {
      let image = new contentWindow.Image();
      image.onload = function() {
        let canvas = contentWindow.document.createElement("canvas");
        let ctx = canvas.getContext("2d");
        let dSize = size * browserWindow.devicePixelRatio;

        // Scales the image while maintaing width to height ratio.
        // If the width and height differ, the image is centered using the
        // smaller of the two dimensions.
        let dWidth, dHeight, dx, dy;
        if (this.width > this.height) {
          dWidth = dSize;
          dHeight = image.height * (dSize / image.width);
          dx = 0;
          dy = (dSize - dHeight) / 2;
        } else {
          dWidth = image.width * (dSize / image.height);
          dHeight = dSize;
          dx = (dSize - dWidth) / 2;
          dy = 0;
        }

        canvas.width = dSize;
        canvas.height = dSize;
        ctx.drawImage(this, 0, 0, this.width, this.height, dx, dy, dWidth, dHeight);
        resolve(canvas.toDataURL("image/png"));
      };
      image.onerror = reject;
      image.src = imageURL;
    });
  },

  // These URLs should already be properly escaped, but make doubly sure CSS
  // string escape characters are escaped here, since they could lead to a
  // sandbox break.
  escapeUrl(url) {
    return url.replace(/[\\\s"]/g, encodeURIComponent);
  },
};

const LISTENERS = Symbol("listeners");
const ONCE_MAP = Symbol("onceMap");

class EventEmitter {
  constructor() {
    this[LISTENERS] = new Map();
    this[ONCE_MAP] = new WeakMap();
  }

  /**
   * Adds the given function as a listener for the given event.
   *
   * The listener function may optionally return a Promise which
   * resolves when it has completed all operations which event
   * dispatchers may need to block on.
   *
   * @param {string} event
   *       The name of the event to listen for.
   * @param {function(string, ...any)} listener
   *        The listener to call when events are emitted.
   */
  on(event, listener) {
    if (!this[LISTENERS].has(event)) {
      this[LISTENERS].set(event, new Set());
    }

    this[LISTENERS].get(event).add(listener);
  }

  /**
   * Removes the given function as a listener for the given event.
   *
   * @param {string} event
   *       The name of the event to stop listening for.
   * @param {function(string, ...any)} listener
   *        The listener function to remove.
   */
  off(event, listener) {
    if (this[LISTENERS].has(event)) {
      let set = this[LISTENERS].get(event);

      set.delete(listener);
      set.delete(this[ONCE_MAP].get(listener));
      if (!set.size) {
        this[LISTENERS].delete(event);
      }
    }
  }

  /**
   * Adds the given function as a listener for the given event once.
   *
   * @param {string} event
   *       The name of the event to listen for.
   * @param {function(string, ...any)} listener
   *        The listener to call when events are emitted.
   */
  once(event, listener) {
    let wrapper = (...args) => {
      this.off(event, wrapper);
      this[ONCE_MAP].delete(listener);

      return listener(...args);
    };
    this[ONCE_MAP].set(listener, wrapper);

    this.on(event, wrapper);
  }


  /**
   * Triggers all listeners for the given event, and returns a promise
   * which resolves when all listeners have been called, and any
   * promises they have returned have likewise resolved.
   *
   * @param {string} event
   *       The name of the event to emit.
   * @param {any} args
   *        Arbitrary arguments to pass to the listener functions, after
   *        the event name.
   * @returns {Promise}
   */
  emit(event, ...args) {
    let listeners = this[LISTENERS].get(event) || new Set();

    let promises = Array.from(listeners, listener => {
      return runSafeSyncWithoutClone(listener, event, ...args);
    });

    return Promise.all(promises);
  }
}

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
      let consoleService = Cc["@mozilla.org/consoleservice;1"]
        .getService(Ci.nsIConsoleService);
      consoleService.logMessage(scriptError);
    },
    removeListener: function(callback) {},
    hasListener: function(callback) {},
  };
}

// Copy an API object from |source| into the scope |dest|.
function injectAPI(source, dest) {
  for (let prop in source) {
    // Skip names prefixed with '_'.
    if (prop[0] == "_") {
      continue;
    }

    let desc = Object.getOwnPropertyDescriptor(source, prop);
    if (typeof(desc.value) == "function") {
      Cu.exportFunction(desc.value, dest, {defineAs: prop});
    } else if (typeof(desc.value) == "object") {
      let obj = Cu.createObjectIn(dest, {defineAs: prop});
      injectAPI(desc.value, obj);
    } else {
      Object.defineProperty(dest, prop, desc);
    }
  }
}

/**
 * A set with a limited number of slots, which flushes older entries as
 * newer ones are added.
 */
class LimitedSet extends Set {
  constructor(limit, iterable = undefined) {
    super(iterable);
    this.limit = limit;
  }

  truncate(limit) {
    for (let item of this) {
      if (this.size <= limit) {
        break;
      }
      this.delete(item);
    }
  }

  add(item) {
    if (!this.has(item) && this.size >= this.limit) {
      this.truncate(this.limit - 1);
    }
    super.add(item);
  }
}

/**
 * Returns a Promise which resolves when the given document's DOM has
 * fully loaded.
 *
 * @param {Document} doc The document to await the load of.
 * @returns {Promise<Document>}
 */
function promiseDocumentReady(doc) {
  if (doc.readyState == "interactive" || doc.readyState == "complete") {
    return Promise.resolve(doc);
  }

  return new Promise(resolve => {
    doc.addEventListener("DOMContentLoaded", function onReady(event) {
      if (event.target === event.currentTarget) {
        doc.removeEventListener("DOMContentLoaded", onReady, true);
        resolve(doc);
      }
    }, true);
  });
}

/**
 * Returns a Promise which resolves when the given document is fully
 * loaded.
 *
 * @param {Document} doc The document to await the load of.
 * @returns {Promise<Document>}
 */
function promiseDocumentLoaded(doc) {
  if (doc.readyState == "complete") {
    return Promise.resolve(doc);
  }

  return new Promise(resolve => {
    doc.defaultView.addEventListener("load", function(event) {
      resolve(doc);
    }, {once: true});
  });
}

/**
 * Returns a Promise which resolves when the given event is dispatched to the
 * given element.
 *
 * @param {Element} element
 *        The element on which to listen.
 * @param {string} eventName
 *        The event to listen for.
 * @param {boolean} [useCapture = true]
 *        If true, listen for the even in the capturing rather than
 *        bubbling phase.
 * @param {Event} [test]
 *        An optional test function which, when called with the
 *        observer's subject and data, should return true if this is the
 *        expected event, false otherwise.
 * @returns {Promise<Event>}
 */
function promiseEvent(element, eventName, useCapture = true, test = event => true) {
  return new Promise(resolve => {
    function listener(event) {
      if (test(event)) {
        element.removeEventListener(eventName, listener, useCapture);
        resolve(event);
      }
    }
    element.addEventListener(eventName, listener, useCapture);
  });
}

/**
 * Returns a Promise which resolves the given observer topic has been
 * observed.
 *
 * @param {string} topic
 *        The topic to observe.
 * @param {function(nsISupports, string)} [test]
 *        An optional test function which, when called with the
 *        observer's subject and data, should return true if this is the
 *        expected notification, false otherwise.
 * @returns {Promise<object>}
 */
function promiseObserved(topic, test = () => true) {
  return new Promise(resolve => {
    let observer = (subject, topic, data) => {
      if (test(subject, data)) {
        Services.obs.removeObserver(observer, topic);
        resolve({subject, data});
      }
    };
    Services.obs.addObserver(observer, topic);
  });
}

function getMessageManager(target) {
  if (target instanceof Ci.nsIFrameLoaderOwner) {
    return target.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;
  }
  return target.QueryInterface(Ci.nsIMessageSender);
}

function flushJarCache(jarPath) {
  Services.obs.notifyObservers(null, "flush-cache-entry", jarPath);
}

function PlatformInfo() {
  return Object.freeze({
    os: (function() {
      let os = AppConstants.platform;
      if (os == "macosx") {
        os = "mac";
      }
      return os;
    })(),
    arch: (function() {
      let abi = Services.appinfo.XPCOMABI;
      let [arch] = abi.split("-");
      if (arch == "x86") {
        arch = "x86-32";
      } else if (arch == "x86_64") {
        arch = "x86-64";
      }
      return arch;
    })(),
  });
}

/**
 * Convert any of several different representations of a date/time to a Date object.
 * Accepts several formats:
 * a Date object, an ISO8601 string, or a number of milliseconds since the epoch as
 * either a number or a string.
 *
 * @param {Date|string|number} date
 *      The date to convert.
 * @returns {Date}
 *      A Date object
 */
function normalizeTime(date) {
  // Of all the formats we accept the "number of milliseconds since the epoch as a string"
  // is an outlier, everything else can just be passed directly to the Date constructor.
  return new Date((typeof date == "string" && /^\d+$/.test(date))
                        ? parseInt(date, 10) : date);
}

const stylesheetMap = new DefaultMap(url => {
  let uri = Services.io.newURI(url);
  return styleSheetService.preloadSheet(uri, styleSheetService.AGENT_SHEET);
});

/**
 * Defines a lazy getter for the given property on the given object. The
 * first time the property is accessed, the return value of the getter
 * is defined on the current `this` object with the given property name.
 * Importantly, this means that a lazy getter defined on an object
 * prototype will be invoked separately for each object instance that
 * it's accessed on.
 *
 * @param {object} object
 *        The prototype object on which to define the getter.
 * @param {string|Symbol} prop
 *        The property name for which to define the getter.
 * @param {function} getter
 *        The function to call in order to generate the final property
 *        value.
 */
function defineLazyGetter(object, prop, getter) {
  let redefine = (obj, value) => {
    Object.defineProperty(obj, prop, {
      enumerable: true,
      configurable: true,
      writable: true,
      value,
    });
    return value;
  };

  Object.defineProperty(object, prop, {
    enumerable: true,
    configurable: true,

    get() {
      return redefine(this, getter.call(this));
    },

    set(value) {
      redefine(this, value);
    },
  });
}

function findPathInObject(obj, path, printErrors = true) {
  let parent;
  for (let elt of path.split(".")) {
    if (!obj || !(elt in obj)) {
      if (printErrors) {
        Cu.reportError(`WebExtension API ${path} not found (it may be unimplemented by Firefox).`);
      }
      return null;
    }

    parent = obj;
    obj = obj[elt];
  }

  if (typeof obj === "function") {
    return obj.bind(parent);
  }
  return obj;
}

/**
 * Acts as a proxy for a message manager or message manager owner, and
 * tracks docShell swaps so that messages are always sent to the same
 * receiver, even if it is moved to a different <browser>.
 *
 * @param {nsIMessageSender|Element} target
 *        The target message manager on which to send messages, or the
 *        <browser> element which owns it.
 */
class MessageManagerProxy {
  constructor(target) {
    this.listeners = new DefaultMap(() => new Map());

    if (target instanceof Ci.nsIMessageSender) {
      Object.defineProperty(this, "messageManager", {
        value: target,
        configurable: true,
        writable: true,
      });
    } else {
      this.addListeners(target);
    }
  }

  /**
   * Disposes of the proxy object, removes event listeners, and drops
   * all references to the underlying message manager.
   *
   * Must be called before the last reference to the proxy is dropped,
   * unless the underlying message manager or <browser> is also being
   * destroyed.
   */
  dispose() {
    if (this.eventTarget) {
      this.removeListeners(this.eventTarget);
      this.eventTarget = null;
    } else {
      this.messageManager = null;
    }
  }

  /**
   * Returns true if the given target is the same as, or owns, the given
   * message manager.
   *
   * @param {nsIMessageSender|MessageManagerProxy|Element} target
   *        The message manager, MessageManagerProxy, or <browser>
   *        element agaisnt which to match.
   * @param {nsIMessageSender} messageManager
   *        The message manager against which to match `target`.
   *
   * @returns {boolean}
   *        True if `messageManager` is the same object as `target`, or
   *        `target` is a MessageManagerProxy or <browser> element that
   *        is tied to it.
   */
  static matches(target, messageManager) {
    return target === messageManager || target.messageManager === messageManager;
  }

  /**
   * @property {nsIMessageSender|null} messageManager
   *        The message manager that is currently being proxied. This
   *        may change during the life of the proxy object, so should
   *        not be stored elsewhere.
   */
  get messageManager() {
    return this.eventTarget && this.eventTarget.messageManager;
  }

  /**
   * Sends a message on the proxied message manager.
   *
   * @param {array} args
   *        Arguments to be passed verbatim to the underlying
   *        sendAsyncMessage method.
   * @returns {undefined}
   */
  sendAsyncMessage(...args) {
    if (this.messageManager) {
      return this.messageManager.sendAsyncMessage(...args);
    }
    /* globals uneval */
    Cu.reportError(`Cannot send message: Other side disconnected: ${uneval(args)}`);
  }

  get isDisconnected() {
    return !this.messageManager;
  }

  /**
   * Adds a message listener to the current message manager, and
   * transfers it to the new message manager after a docShell swap.
   *
   * @param {string} message
   *        The name of the message to listen for.
   * @param {nsIMessageListener} listener
   *        The listener to add.
   * @param {boolean} [listenWhenClosed = false]
   *        If true, the listener will receive messages which were sent
   *        after the remote side of the listener began closing.
   */
  addMessageListener(message, listener, listenWhenClosed = false) {
    this.messageManager.addMessageListener(message, listener, listenWhenClosed);
    this.listeners.get(message).set(listener, listenWhenClosed);
  }

  /**
   * Adds a message listener from the current message manager.
   *
   * @param {string} message
   *        The name of the message to stop listening for.
   * @param {nsIMessageListener} listener
   *        The listener to remove.
   */
  removeMessageListener(message, listener) {
    this.messageManager.removeMessageListener(message, listener);

    let listeners = this.listeners.get(message);
    listeners.delete(listener);
    if (!listeners.size) {
      this.listeners.delete(message);
    }
  }

  /**
   * @private
   * Iterates over all of the currently registered message listeners.
   */
  * iterListeners() {
    for (let [message, listeners] of this.listeners) {
      for (let [listener, listenWhenClosed] of listeners) {
        yield {message, listener, listenWhenClosed};
      }
    }
  }

  /**
   * @private
   * Adds docShell swap listeners to the message manager owner.
   *
   * @param {Element} target
   *        The target element.
   */
  addListeners(target) {
    target.addEventListener("SwapDocShells", this);

    for (let {message, listener, listenWhenClosed} of this.iterListeners()) {
      target.addMessageListener(message, listener, listenWhenClosed);
    }

    this.eventTarget = target;
  }

  /**
   * @private
   * Removes docShell swap listeners to the message manager owner.
   *
   * @param {Element} target
   *        The target element.
   */
  removeListeners(target) {
    target.removeEventListener("SwapDocShells", this);

    for (let {message, listener} of this.iterListeners()) {
      target.removeMessageListener(message, listener);
    }
  }

  handleEvent(event) {
    if (event.type == "SwapDocShells") {
      this.removeListeners(this.eventTarget);
      this.addListeners(event.detail);
    }
  }
}

/**
 * Classify an individual permission from a webextension manifest
 * as a host/origin permission, an api permission, or a regular permission.
 *
 * @param {string} perm  The permission string to classify
 *
 * @returns {object}
 *          An object with exactly one of the following properties:
 *          "origin" to indicate this is a host/origin permission.
 *          "api" to indicate this is an api permission
 *                (as used for webextensions experiments).
 *          "permission" to indicate this is a regular permission.
 */
function classifyPermission(perm) {
  let match = /^(\w+)(?:\.(\w+)(?:\.\w+)*)?$/.exec(perm);
  if (!match) {
    return {origin: perm};
  } else if (match[1] == "experiments" && match[2]) {
    return {api: match[2]};
  }
  return {permission: perm};
}

this.ExtensionUtils = {
  classifyPermission,
  defineLazyGetter,
  extend,
  findPathInObject,
  flushJarCache,
  getConsole,
  getInnerWindowID,
  getMessageManager,
  getUniqueId,
  filterStack,
  getWinUtils,
  ignoreEvent,
  injectAPI,
  instanceOf,
  normalizeTime,
  promiseDocumentLoaded,
  promiseDocumentReady,
  promiseEvent,
  promiseObserved,
  runSafe,
  runSafeSync,
  runSafeSyncWithoutClone,
  runSafeWithoutClone,
  stylesheetMap,
  DefaultMap,
  DefaultWeakMap,
  EventEmitter,
  ExtensionError,
  IconDetails,
  LimitedSet,
  MessageManagerProxy,
  SpreadArgs,
  StartupCache,
};

XPCOMUtils.defineLazyGetter(this.ExtensionUtils, "PlatformInfo", PlatformInfo);
