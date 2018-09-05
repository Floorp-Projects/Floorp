/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ExtensionUtils"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "setTimeout",
                               "resource://gre/modules/Timer.jsm");
ChromeUtils.defineModuleGetter(this, "TelemetryStopwatch",
                               "resource://gre/modules/TelemetryStopwatch.jsm");

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
  return win.windowUtils;
});
const getWinUtils = win => _winUtils.get(win);

function getInnerWindowID(window) {
  return getWinUtils(window).currentInnerWindowID;
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

const chromeModifierKeyMap = {
  "Alt": "alt",
  "Command": "accel",
  "Ctrl": "accel",
  "MacCtrl": "control",
  "Shift": "shift",
};

/**
 * Get a trimmed version of the given string if it is longer than 80 chars (used in telemetry
 * when a string may be longer than allowed).
 *
 * @param {string} str
 *        The original string content.
 *
 * @returns {string}
 *          The trimmed version of the string when longer than 80 chars, or the given string
 *          unmodified otherwise.
 */
function getTrimmedString(str) {
  if (str.length <= 80) {
    return str;
  }

  const length = str.length;

  // Trim the string to prevent a flood of warnings messages logged internally by recordEvent,
  // the trimmed version is going to be composed by the first 40 chars and the last 37 and 3 dots
  // that joins the two parts, to visually indicate that the string has been trimmed.
  return `${str.slice(0, 40)}...${str.slice(length - 37, length)}`;
}

/**
 * This is a internal helper object which contains a collection of helpers used to make it easier
 * to collect extension telemetry (in both the general histogram and in the one keyed by addon id).
 *
 * This helper object is not exported from ExtensionUtils, it is used by the ExtensionTelemetry
 * Proxy which is exported and used by the callers to record telemetry data for one of the
 * supported metrics.
 */
const ExtensionTelemetryHelpers = {
  // Allow callers to refer to the existing metrics by accessing it as properties of the
  // ExtensionTelemetry.metrics (e.g. ExtensionTelemetry.metrics.extensionStartup).

  // Cache of the metrics helper lazily created by the ExtensionTelemetry Proxy.
  _metricsMap: new Map(),

  // Map of the base histogram ids for the metrics recorded for the extensions.
  _histograms: {
    "extensionStartup": "WEBEXT_EXTENSION_STARTUP_MS",
    "backgroundPageLoad": "WEBEXT_BACKGROUND_PAGE_LOAD_MS",
    "browserActionPopupOpen": "WEBEXT_BROWSERACTION_POPUP_OPEN_MS",
    "browserActionPreloadResult": "WEBEXT_BROWSERACTION_POPUP_PRELOAD_RESULT_COUNT",
    "contentScriptInjection": "WEBEXT_CONTENT_SCRIPT_INJECTION_MS",
    "pageActionPopupOpen": "WEBEXT_PAGEACTION_POPUP_OPEN_MS",
    "storageLocalGetJSON": "WEBEXT_STORAGE_LOCAL_GET_MS",
    "storageLocalSetJSON": "WEBEXT_STORAGE_LOCAL_SET_MS",
    "storageLocalGetIDB": "WEBEXT_STORAGE_LOCAL_IDB_GET_MS",
    "storageLocalSetIDB": "WEBEXT_STORAGE_LOCAL_IDB_SET_MS",
  },
  // Wraps a call to a TelemetryStopwatch method.
  /**
   * Wraps a call to a TelemetryStopwatch method for a given metric and extension.
   *
   * @param {string} method
   *        The stopwatch method to call ("start", "finish" or "cancel").
   * @param {string} metric
   *        The stopwatch metric to record (used to retrieve the base histogram id from the _histogram object).
   * @param {Extension | BrowserExtensionContent} extension
   *        The extension to record the telemetry for.
   * @param {any | undefined} [obj = extension]
   *        An optional telemetry stopwatch object (which defaults to the extension parameter when missing).
   */
  _wrappedStopwatchMethod(method, metric, extension, obj = extension) {
    if (!extension) {
      throw new Error(`Mandatory extension parameter is undefined`);
    }

    const baseId = this._histograms[metric];
    if (!baseId) {
      throw new Error(`Unknown metric ${metric}`);
    }

    // Record metric in the general histogram.
    TelemetryStopwatch[method](baseId, obj);

    // Record metric in the histogram keyed by addon id.
    let extensionId = getTrimmedString(extension.id);
    TelemetryStopwatch[`${method}Keyed`](`${baseId}_BY_ADDONID`, extensionId, obj);
  },
  /**
   * Record a telemetry category and/or value for a given metric.
   *
   * @param {string} metric
   *        The metric to record (used to retrieve the base histogram id from the _histogram object).
   * @param {Object}                              options
   * @param {Extension | BrowserExtensionContent} options.extension
   *        The extension to record the telemetry for.
   * @param {string | undefined}                  [options.category]
   *        An optional histogram category.
   * @param {number | undefined}                  [options.value]
   *        An optional value to record.
   */
  _histogramAdd(metric, {category, extension, value}) {
    if (!extension) {
      throw new Error(`Mandatory extension parameter is undefined`);
    }

    const baseId = this._histograms[metric];
    if (!baseId) {
      throw new Error(`Unknown metric ${metric}`);
    }

    const histogram = Services.telemetry.getHistogramById(baseId);
    if (typeof category === "string") {
      histogram.add(category, value);
    } else {
      histogram.add(value);
    }

    const keyedHistogram = Services.telemetry.getKeyedHistogramById(`${baseId}_BY_ADDONID`);
    const extensionId = getTrimmedString(extension.id);

    if (typeof category === "string") {
      keyedHistogram.add(extensionId, category, value);
    } else {
      keyedHistogram.add(extensionId, value);
    }
  },
};

/**
 * This proxy object provides the telemetry helpers for the currently supported metrics (the ones listed in
 * ExtensionTelemetryHelpers._histograms), the telemetry helpers for a particular metric are lazily created
 * when the related property is being accessed on this object for the first time, e.g.:
 *
 *      ExtensionTelemetry.extensionStartup.stopwatchStart(extension);
 *      ExtensionTelemetry.browserActionPreloadResult.histogramAdd({category: "Shown", extension});
 */
const ExtensionTelemetry = new Proxy(ExtensionTelemetryHelpers, {
  get(target, prop, receiver) {
    if (!(prop in target._histograms)) {
      throw new Error(`Unknown metric ${prop}`);
    }

    // Lazily create and cache the metric result object.
    if (!target._metricsMap.has(prop)) {
      target._metricsMap.set(prop, {
        // Stopwatch histogram helpers.
        stopwatchStart: target._wrappedStopwatchMethod.bind(target, "start", prop),
        stopwatchFinish: target._wrappedStopwatchMethod.bind(target, "finish", prop),
        stopwatchCancel: target._wrappedStopwatchMethod.bind(target, "cancel", prop),
        // Result histogram helpers.
        histogramAdd: target._histogramAdd.bind(target, prop),
      });
    }

    return target._metricsMap.get(prop);
  },
});

var ExtensionUtils = {
  chromeModifierKeyMap,
  flushJarCache,
  getInnerWindowID,
  getMessageManager,
  getTrimmedString,
  getUniqueId,
  filterStack,
  getWinUtils,
  promiseDocumentIdle,
  promiseDocumentLoaded,
  promiseDocumentReady,
  promiseEvent,
  promiseObserved,
  promiseTimeout,
  DefaultMap,
  DefaultWeakMap,
  ExtensionError,
  ExtensionTelemetry,
  LimitedSet,
};
