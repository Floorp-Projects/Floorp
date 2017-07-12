/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionUtils"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ConsoleAPI",
                                  "resource://gre/modules/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MessageChannel",
                                  "resource://gre/modules/MessageChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

function getConsole() {
  return new ConsoleAPI({
    maxLogLevelPref: "extensions.webextensions.log.level",
    prefix: "WebExtensions",
  });
}

XPCOMUtils.defineLazyGetter(this, "console", getConsole);

const appinfo = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);

let nextId = 0;
XPCOMUtils.defineLazyGetter(this, "uniqueProcessID", () => appinfo.uniqueProcessID);

function getUniqueId() {
  return `${nextId++}-${uniqueProcessID}`;
}

async function promiseFileContents(file) {
  let res = await OS.File.read(file.path);
  return res.buffer;
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

function withHandlingUserInput(window, callable) {
  let handle = getWinUtils(window).setHandlingUserInput(true);
  try {
    return callable();
  } finally {
    handle.destruct();
  }
}

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

this.ExtensionUtils = {
  defineLazyGetter,
  flushJarCache,
  getConsole,
  getInnerWindowID,
  getMessageManager,
  getUniqueId,
  filterStack,
  getWinUtils,
  instanceOf,
  normalizeTime,
  promiseDocumentLoaded,
  promiseDocumentReady,
  promiseEvent,
  promiseFileContents,
  promiseObserved,
  runSafe,
  runSafeSync,
  runSafeSyncWithoutClone,
  runSafeWithoutClone,
  withHandlingUserInput,
  DefaultMap,
  DefaultWeakMap,
  EventEmitter,
  ExtensionError,
  LimitedSet,
  MessageManagerProxy,
};
