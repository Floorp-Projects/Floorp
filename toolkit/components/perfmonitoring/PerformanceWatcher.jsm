// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * An API for being informed of slow add-ons and tabs.
 *
 * Generally, this API is both more CPU-efficient and more battery-efficient
 * than PerformanceStats. As PerformanceStats, this API does not provide any
 * information during the startup or shutdown of Firefox.
 *
 * = Examples =
 *
 * Example use: reporting whenever a specific add-on slows down Firefox.
 * let listener = function(source, details) {
 *   // This listener is triggered whenever the addon causes Firefox to miss
 *   // frames. Argument `source` contains information about the source of the
 *   // slowdown (including the process in which it happens), while `details`
 *   // contains performance statistics.
 *   console.log(`Oops, add-on ${source.addonId} seems to be slowing down Firefox.`, details);
 * };
 * PerformanceWatcher.addPerformanceListener({addonId: "myaddon@myself.name"}, listener);
 *
 * Example use: reporting whenever any webpage slows down Firefox.
 * let listener = function(alerts) {
 *   // This listener is triggered whenever any window causes Firefox to miss
 *   // frames. FieldArgument `source` contains information about the source of the
 *   // slowdown (including the process in which it happens), while `details`
 *   // contains performance statistics.
 *   for (let {source, details} of alerts) {
 *     console.log(`Oops, window ${source.windowId} seems to be slowing down Firefox.`, details);
 * };
 * // Special windowId 0 lets us to listen to all webpages.
 * PerformanceWatcher.addPerformanceListener({windowId: 0}, listener);
 *
 *
 * = How this works =
 *
 * This high-level API is based on the lower-level nsIPerformanceStatsService.
 * At the end of each event (including micro-tasks), the nsIPerformanceStatsService
 * updates its internal performance statistics and determines whether any
 * add-on/window in the current process has exceeded the jank threshold.
 *
 * The PerformanceWatcher maintains low-level performance observers in each
 * process and forwards alerts to the main process. Internal observers collate
 * low-level main process alerts and children process alerts and notify clients
 * of this API.
 */

this.EXPORTED_SYMBOLS = ["PerformanceWatcher"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

// `true` if the code is executed in content, `false` otherwise
let isContent = Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;

if (!isContent) {
  // Initialize communication with children.
  //
  // To keep the protocol simple, the children inform the parent whenever a slow
  // add-on/tab is detected. We do not attempt to implement thresholds.
  Services.ppmm.loadProcessScript("resource://gre/modules/PerformanceWatcher-content.js",
    true/*including future processes*/);

  Services.ppmm.addMessageListener("performancewatcher-propagate-notifications",
    (...args) => ChildManager.notifyObservers(...args)
  );
}

// Configure the performance stats service to inform us in case of jank.
let performanceStatsService = Cc["@mozilla.org/toolkit/performance-stats-service;1"].
  getService(Ci.nsIPerformanceStatsService);
performanceStatsService.jankAlertThreshold = 64000 /* us */;


/**
 * Handle communications with child processes. Handle listening to
 * either a single add-on id (including the special add-on id "*",
 * which is notified for all add-ons) or a single window id (including
 * the special window id 0, which is notified for all windows).
 *
 * Acquire through `ChildManager.getAddon` and `ChildManager.getWindow`.
 */
function ChildManager(map, key) {
  this.key = key;
  this._map = map;
  this._listeners = new Set();
}
ChildManager.prototype = {
  /**
   * Add a listener, which will be notified whenever a child process
   * reports a slow performance alert for this addon/window.
   */
  addListener: function(listener) {
    this._listeners.add(listener);
  },
  /**
   * Remove a listener.
   */
  removeListener: function(listener) {
    let deleted = this._listeners.delete(listener);
    if (!deleted) {
      throw new Error("Unknown listener");
    }
  },

  listeners: function() {
    return this._listeners.values();
  }
};

/**
 * Dispatch child alerts to observers.
 *
 * Triggered by messages from content processes.
 */
ChildManager.notifyObservers = function({data: {addons, windows}}) {
  if (addons && addons.length > 0) {
    // Dispatch the entire list to universal listeners
    this._notify(ChildManager.getAddon("*").listeners(), addons);

    // Dispatch individual alerts to individual listeners
    for (let {source, details} of addons) {
      this._notify(ChildManager.getAddon(source.addonId).listeners(), source, details);
    }
  }
  if (windows && windows.length > 0) {
    // Dispatch the entire list to universal listeners
    this._notify(ChildManager.getWindow(0).listeners(), windows);

    // Dispatch individual alerts to individual listeners
    for (let {source, details} of windows) {
      this._notify(ChildManager.getWindow(source.windowId).listeners(), source, details);
    }
  }
};

ChildManager._notify = function(targets, ...args) {
  for (let target of targets) {
    target(...args);
  }
};

ChildManager.getAddon = function(key) {
  return this._get(this._addons, key);
};
ChildManager._addons = new Map();

ChildManager.getWindow = function(key) {
  return this._get(this._windows, key);
};
ChildManager._windows = new Map();

ChildManager._get = function(map, key) {
  let result = map.get(key);
  if (!result) {
    result = new ChildManager(map, key);
    map.set(key ,result);
  }
  return result;
};
let gListeners = new WeakMap();

/**
 * An object in charge of managing all the observables for a single
 * target (window/addon/all windows/all addons).
 *
 * In a content process, a target is represented by a single observable.
 * The situation is more sophisticated in a parent process, as a target
 * has both an in-process observable and several observables across children
 * processes.
 *
 * This class abstracts away the difference to simplify the work of
 * (un)registering observers for targets.
 *
 * @param {object} target The target being observed, as an object
 * with one of the following fields:
 *   - {string} addonId Either "*" for the universal add-on observer
 *     or the add-on id of an addon. Note that this class does not
 *     check whether the add-on effectively exists, and that observers
 *     may be registered for an add-on before the add-on is installed
 *     or started.
 *   - {xul:tab} tab A single tab. It must already be initialized.
 *   - {number} windowId Either 0 for the universal window observer
 *     or the outer window id of the window.
 */
function Observable(target) {
  // A mapping from `listener` (function) to `Observer`.
  this._observers = new Map();
  if ("addonId" in target) {
    this._key = `addonId: ${target.addonId}`;
    this._process = performanceStatsService.getObservableAddon(target.addonId);
    this._children = isContent ? null : ChildManager.getAddon(target.addonId);
    this._isBuffered = target.addonId == "*";
  } else if ("tab" in target || "windowId" in target) {
    let windowId;
    if ("tab" in target) {
      windowId = target.tab.linkedBrowser.outerWindowID;
      // By convention, outerWindowID may not be 0.
    } else if ("windowId" in target) {
      windowId = target.windowId;
    }
    if (windowId == undefined || windowId == null) {
      throw new TypeError(`No outerWindowID. Perhaps the target is a tab that is not initialized yet.`);
    }
    this._key = `tab-windowId: ${windowId}`;
    this._process = performanceStatsService.getObservableWindow(windowId);
    this._children = isContent ? null : ChildManager.getWindow(windowId);
    this._isBuffered = windowId == 0;
  } else {
    throw new TypeError("Unexpected target");
  }
}
Observable.prototype = {
  addJankObserver: function(listener) {
    if (this._observers.has(listener)) {
      throw new TypeError(`Listener already registered for target ${this._key}`);
    }
    if (this._children) {
      this._children.addListener(listener);
    }
    let observer = this._isBuffered ? new BufferedObserver(listener)
      : new Observer(listener);
    // Store the observer to be able to call `this._process.removeJankObserver`.
    this._observers.set(listener, observer);

    this._process.addJankObserver(observer);
  },
  removeJankObserver: function(listener) {
    let observer = this._observers.get(listener);
    if (!observer) {
      throw new TypeError(`No listener for target ${this._key}`);
    }
    this._observers.delete(listener);

    if (this._children) {
      this._children.removeListener(listener);
    }

    this._process.removeJankObserver(observer);
    observer.dispose();
  },
};

/**
 * Get a cached observable for a given target.
 */
Observable.get = function(target) {
  let key;
  if ("addonId" in target) {
    key = target.addonId;
  } else if ("tab" in target) {
    // We do not want to use a tab as a key, as this would prevent it from
    // being garbage-collected.
    key = target.tab.linkedBrowser.outerWindowID;
  } else if ("windowId" in target) {
    key = target.windowId;
  }
  if (key == null) {
    throw new TypeError(`Could not extract a key from ${JSON.stringify(target)}. Could the target be an unitialized tab?`);
  }
  let observable = this._cache.get(key);
  if (!observable) {
    observable = new Observable(target);
    this._cache.set(key, observable);
  }
  return observable;
};
Observable._cache = new Map();

/**
 * Wrap a listener callback as an unbuffered nsIPerformanceObserver.
 *
 * Each observation is propagated immediately to the listener.
 */
function Observer(listener) {
  // Make sure that monitoring stays alive (in all processes) at least as
  // long as the observer.
  this._monitor = PerformanceStats.getMonitor(["jank", "cpow"]);
  this._listener = listener;
}
Observer.prototype = {
  observe: function(...args) {
    this._listener(...args);
  },
  dispose: function() {
    this._monitor.dispose();
    this.observe = function poison() {
      throw new Error("Internal error: I should have stopped receiving notifications");
    }
  },
};

/**
 * Wrap a listener callback as an buffered nsIPerformanceObserver.
 *
 * Observations are buffered and dispatch in the next tick to the listener.
 */
function BufferedObserver(listener) {
  Observer.call(this, listener);
  this._buffer = [];
  this._isDispatching = false;
  this._pending = null;
}
BufferedObserver.prototype = Object.create(Observer.prototype);
BufferedObserver.prototype.observe = function(source, details) {
  this._buffer.push({source, details});
  if (!this._isDispatching) {
    this._isDispatching = true;
    Services.tm.mainThread.dispatch(() => {
      // Grab buffer, in case something in the listener could modify it.
      let buffer = this._buffer;
      this._buffer = [];

      // As of this point, any further observations need to use the new buffer
      // and a new dispatcher.
      this._isDispatching = false;

      this._listener(buffer);
    }, Ci.nsIThread.DISPATCH_NORMAL);
  }
};

this.PerformanceWatcher = {
  /**
   * Add a listener informed whenever we receive a slow performance alert
   * in the application.
   *
   * @param {object} target An object with one of the following fields:
   *  - {string} addonId Either "*" to observe all add-ons or a full add-on ID.
   *      to observe a single add-on.
   *  - {number} windowId Either 0 to observe all windows or an outer window ID
   *      to observe a single tab.
   *  - {xul:browser} tab To observe a single tab.
   * @param {function} listener A function that will be triggered whenever
   *    the target causes a slow performance notification. The notification may
   *    have originated in any process of the application.
   *
   *    If the listener listens to a single add-on/webpage, it is triggered with
   *    the following arguments:
   *       source: {groupId, name, addonId, windowId, isSystem, processId}
   *         Information on the source of the notification.
   *       details: {reason, highestJank, highestCPOW} Information on the
   *         notification.
   *
   *    If the listener listens to all add-ons/all webpages, it is triggered with
   *    an array of {source, details}, as described above.
   */
  addPerformanceListener: function(target, listener) {
    if (typeof listener != "function") {
      throw new TypeError();
    }
    let observable = Observable.get(target);
    observable.addJankObserver(listener);
  },
  removePerformanceListener: function(target, listener) {
    if (typeof listener != "function") {
      throw new TypeError();
    }
    let observable = Observable.get(target);
    observable.removeJankObserver(listener);
  },
};
