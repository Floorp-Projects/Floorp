/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ExtensionUtils"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "ConsoleAPI",
                               "resource://gre/modules/Console.jsm");
ChromeUtils.defineModuleGetter(this, "setTimeout",
                               "resource://gre/modules/Timer.jsm");

function getConsole() {
  return new ConsoleAPI({
    maxLogLevelPref: "extensions.webextensions.log.level",
    prefix: "WebExtensions",
  });
}

XPCOMUtils.defineLazyGetter(this, "console", getConsole);

// xpcshell doesn't handle idle callbacks well.
XPCOMUtils.defineLazyGetter(this, "idleTimeout",
                            () => Services.appinfo.name === "XPCShell" ? 500 : undefined);

// It would be nicer to go through `Services.appinfo`, but some tests need to be
// able to replace that field with a custom implementation before it is first
// called.
// eslint-disable-next-line mozilla/use-services
const appinfo = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);

let nextId = 0;
const uniqueProcessID = appinfo.uniqueProcessID;
// Store the process ID in a 16 bit field left shifted to end of a
// double's mantissa.
// Note: We can't use bitwise ops here, since they truncate to a 32 bit
// integer and we need all 53 mantissa bits.
const processIDMask = (uniqueProcessID & 0xffff) * (2 ** 37);

function getUniqueId() {
  // Note: We can't use bitwise ops here, since they truncate to a 32 bit
  // integer and we need all 53 mantissa bits.
  return processIDMask + nextId++;
}

function promiseTimeout(delay) {
  return new Promise(resolve => setTimeout(resolve, delay));
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

// Return true if the given value is an instance of the given
// native type.
function instanceOf(value, type) {
  return (value && typeof value === "object" &&
          ChromeUtils.getClassName(value) === type);
}

/**
 * Similar to a WeakMap, but creates a new key with the given
 * constructor if one is not present.
 */
class DefaultWeakMap extends WeakMap {
  constructor(defaultConstructor = undefined, init = undefined) {
    super(init);
    if (defaultConstructor) {
      this.defaultConstructor = defaultConstructor;
    }
  }

  get(key) {
    let value = super.get(key);
    if (value === undefined && !this.has(key)) {
      value = this.defaultConstructor(key);
      this.set(key, value);
    }
    return value;
  }
}

class DefaultMap extends Map {
  constructor(defaultConstructor = undefined, init = undefined) {
    super(init);
    if (defaultConstructor) {
      this.defaultConstructor = defaultConstructor;
    }
  }

  get(key) {
    let value = super.get(key);
    if (value === undefined && !this.has(key)) {
      value = this.defaultConstructor(key);
      this.set(key, value);
    }
    return value;
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
    let listeners = this[LISTENERS].get(event);
    if (!listeners) {
      listeners = new Set();
      this[LISTENERS].set(event, listeners);
    }

    listeners.add(listener);
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
    let set = this[LISTENERS].get(event);
    if (set) {
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
   * Triggers all listeners for the given event. If any listeners return
   * a value, returns a promise which resolves when all returned
   * promises have resolved. Otherwise, returns undefined.
   *
   * @param {string} event
   *       The name of the event to emit.
   * @param {any} args
   *        Arbitrary arguments to pass to the listener functions, after
   *        the event name.
   * @returns {Promise?}
   */
  emit(event, ...args) {
    let listeners = this[LISTENERS].get(event);

    if (listeners) {
      let promises = [];

      for (let listener of listeners) {
        try {
          let result = listener(event, ...args);
          if (result !== undefined) {
            promises.push(result);
          }
        } catch (e) {
          Cu.reportError(e);
        }
      }

      if (promises.length) {
        return Promise.all(promises);
      }
    }
  }
}

/**
 * A set with a limited number of slots, which flushes older entries as
 * newer ones are added.
 *
 * @param {integer} limit
 *        The maximum size to trim the set to after it grows too large.
 * @param {integer} [slop = limit * .25]
 *        The number of extra entries to allow in the set after it
 *        reaches the size limit, before it is truncated to the limit.
 * @param {iterable} [iterable]
 *        An iterable of initial entries to add to the set.
 */
class LimitedSet extends Set {
  constructor(limit, slop = Math.round(limit * .25), iterable = undefined) {
    super(iterable);
    this.limit = limit;
    this.slop = slop;
  }

  truncate(limit) {
    for (let item of this) {
      // Live set iterators can ge relatively expensive, since they need
      // to be updated after every modification to the set. Since
      // breaking out of the loop early will keep the iterator alive
      // until the next full GC, we're currently better off finishing
      // the entire loop even after we're done truncating.
      if (this.size > limit) {
        this.delete(item);
      }
    }
  }

  add(item) {
    if (this.size >= this.limit + this.slop && !this.has(item)) {
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
  * Returns a Promise which resolves when the given window's document's DOM has
  * fully loaded, the <head> stylesheets have fully loaded, and we have hit an
  * idle time.
  *
  * @param {Window} window The window whose document we will await
                           the readiness of.
  * @returns {Promise<IdleDeadline>}
  */
function promiseDocumentIdle(window) {
  return window.document.documentReadyForIdle.then(() => {
    return new Promise(resolve =>
      window.requestIdleCallback(resolve, {timeout: idleTimeout}));
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
    doc.defaultView.addEventListener("load", () => resolve(doc), {once: true});
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
  if (target.frameLoader) {
    return target.frameLoader.messageManager;
  }
  return target;
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
    this.closed = false;

    if (target instanceof Ci.nsIMessageSender) {
      this.messageManager = target;
    } else {
      this.addListeners(target);
    }

    Services.obs.addObserver(this, "message-manager-close");
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
    }
    this.messageManager = null;

    Services.obs.removeObserver(this, "message-manager-close");
  }

  observe(subject, topic, data) {
    if (topic === "message-manager-close") {
      if (subject === this.messageManager) {
        this.closed = true;
      }
    }
  }

  /**
   * Returns true if the given target is the same as, or owns, the given
   * message manager.
   *
   * @param {nsIMessageSender|MessageManagerProxy|Element} target
   *        The message manager, MessageManagerProxy, or <browser>
   *        element against which to match.
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
    return this.closed || !this.messageManager;
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

    this.eventTarget = target;
    this.messageManager = target.messageManager;

    for (let {message, listener, listenWhenClosed} of this.iterListeners()) {
      this.messageManager.addMessageListener(message, listener, listenWhenClosed);
    }
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
      this.messageManager.removeMessageListener(message, listener);
    }
  }

  handleEvent(event) {
    if (event.type == "SwapDocShells") {
      this.removeListeners(this.eventTarget);
      this.addListeners(event.detail);
    }
  }
}

function checkLoadURL(url, principal, options) {
  let ssm = Services.scriptSecurityManager;

  let flags = ssm.STANDARD;
  if (!options.allowScript) {
    flags |= ssm.DISALLOW_SCRIPT;
  }
  if (!options.allowInheritsPrincipal) {
    flags |= ssm.DISALLOW_INHERIT_PRINCIPAL;
  }
  if (options.dontReportErrors) {
    flags |= ssm.DONT_REPORT_ERRORS;
  }

  try {
    ssm.checkLoadURIWithPrincipal(principal,
                                  Services.io.newURI(url),
                                  flags);
  } catch (e) {
    return false;
  }
  return true;
}

function makeWidgetId(id) {
  id = id.toLowerCase();
  // FIXME: This allows for collisions.
  return id.replace(/[^a-z0-9_-]/g, "_");
}

const chromeModifierKeyMap = {
  "Alt": "alt",
  "Command": "accel",
  "Ctrl": "accel",
  "MacCtrl": "control",
  "Shift": "shift",
};

var ExtensionUtils = {
  checkLoadURL,
  chromeModifierKeyMap,
  defineLazyGetter,
  flushJarCache,
  getConsole,
  getInnerWindowID,
  getMessageManager,
  getUniqueId,
  filterStack,
  getWinUtils,
  instanceOf,
  makeWidgetId,
  normalizeTime,
  promiseDocumentIdle,
  promiseDocumentLoaded,
  promiseDocumentReady,
  promiseEvent,
  promiseObserved,
  promiseTimeout,
  runSafeSyncWithoutClone,
  withHandlingUserInput,
  DefaultMap,
  DefaultWeakMap,
  EventEmitter,
  ExtensionError,
  LimitedSet,
  MessageManagerProxy,
};
