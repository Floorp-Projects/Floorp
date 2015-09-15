/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * About the types of objects in this file:
 *
 * - ReflowActor: the actor class used for protocol purposes.
 *   Mostly empty, just gets an instance of LayoutChangesObserver and forwards
 *   its "reflows" events to clients.
 *
 * - Observable: A utility parent class, meant at being extended by classes that
 *   need a start/stop behavior.
 *
 * - LayoutChangesObserver: extends Observable and uses the ReflowObserver, to
 *   track reflows on the page.
 *   Used by the LayoutActor, but is also exported on the module, so can be used
 *   by any other actor that needs it.
 *
 * - Dedicated observers: There's only one of them for now: ReflowObserver which
 *   listens to reflow events via the docshell,
 *   These dedicated classes are used by the LayoutChangesObserver.
 */

const {Ci, Cu} = require("chrome");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
const protocol = require("devtools/server/protocol");
const {method, Arg, RetVal, types} = protocol;
const events = require("sdk/event/core");
const Heritage = require("sdk/core/heritage");
const {setTimeout, clearTimeout} = require("sdk/timers");
const EventEmitter = require("devtools/toolkit/event-emitter");

/**
 * The reflow actor tracks reflows and emits events about them.
 */
var ReflowActor = exports.ReflowActor = protocol.ActorClass({
  typeName: "reflow",

  events: {
    /**
     * The reflows event is emitted when reflows have been detected. The event
     * is sent with an array of reflows that occured. Each item has the
     * following properties:
     * - start {Number}
     * - end {Number}
     * - isInterruptible {Boolean}
     */
    "reflows" : {
      type: "reflows",
      reflows: Arg(0, "array:json")
    }
  },

  initialize: function(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this.tabActor = tabActor;
    this._onReflow = this._onReflow.bind(this);
    this.observer = getLayoutChangesObserver(tabActor);
    this._isStarted = false;
  },

  /**
   * The reflow actor is the first (and last) in its hierarchy to use protocol.js
   * so it doesn't have a parent protocol actor that takes care of its lifetime.
   * So it needs a disconnect method to cleanup.
   */
  disconnect: function() {
    this.destroy();
  },

  destroy: function() {
    this.stop();
    releaseLayoutChangesObserver(this.tabActor);
    this.observer = null;
    this.tabActor = null;

    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Start tracking reflows and sending events to clients about them.
   * This is a oneway method, do not expect a response and it won't return a
   * promise.
   */
  start: method(function() {
    if (!this._isStarted) {
      this.observer.on("reflows", this._onReflow);
      this._isStarted = true;
    }
  }, {oneway: true}),

  /**
   * Stop tracking reflows and sending events to clients about them.
   * This is a oneway method, do not expect a response and it won't return a
   * promise.
   */
  stop: method(function() {
    if (this._isStarted) {
      this.observer.off("reflows", this._onReflow);
      this._isStarted = false;
    }
  }, {oneway: true}),

  _onReflow: function(event, reflows) {
    if (this._isStarted) {
      events.emit(this, "reflows", reflows);
    }
  }
});

/**
 * Usage example of the reflow front:
 *
 * let front = ReflowFront(toolbox.target.client, toolbox.target.form);
 * front.on("reflows", this._onReflows);
 * front.start();
 * // now wait for events to come
 */
exports.ReflowFront = protocol.FrontClass(ReflowActor, {
  initialize: function(client, {reflowActor}) {
    protocol.Front.prototype.initialize.call(this, client, {actor: reflowActor});
    this.manage(this);
  },

  destroy: function() {
    protocol.Front.prototype.destroy.call(this);
  },
});

/**
 * Base class for all sorts of observers we need to create for a given window.
 * @param {TabActor} tabActor
 * @param {Function} callback Executed everytime the observer observes something
 */
function Observable(tabActor, callback) {
  this.tabActor = tabActor;
  this.callback = callback;
}

Observable.prototype = {
  /**
   * Is the observer currently observing
   */
  observing: false,

  /**
   * Start observing whatever it is this observer is supposed to observe
   */
  start: function() {
    if (!this.observing) {
      this._start();
      this.observing = true;
    }
  },

  _start: function() {
    /* To be implemented by sub-classes */
  },

  /**
   * Stop observing
   */
  stop: function() {
    if (this.observing) {
      this._stop();
      this.observing = false;
    }
  },

  _stop: function() {
    /* To be implemented by sub-classes */
  },

  /**
   * To be called by sub-classes when something has been observed
   */
  notifyCallback: function(...args) {
    this.observing && this.callback && this.callback.apply(null, args);
  },

  /**
   * Stop observing and detroy this observer instance
   */
  destroy: function() {
    this.stop();
    this.callback = null;
    this.tabActor = null;
  }
};

/**
 * The LayouChangesObserver will observe reflows as soon as it is started.
 * Some devtools actors may cause reflows and it may be wanted to "hide" these
 * reflows from the LayouChangesObserver consumers.
 * If this is the case, such actors should require this module and use this
 * global function to turn the ignore mode on and off temporarily.
 *
 * Note that if a node is provided, it will be used to force a sync reflow to
 * make sure all reflows which occurred before switching the mode on or off are
 * either observed or ignored depending on the current mode.
 *
 * @param {Boolean} ignore
 * @param {DOMNode} syncReflowNode The node to use to force a sync reflow
 */
var gIgnoreLayoutChanges = false;
exports.setIgnoreLayoutChanges = function(ignore, syncReflowNode) {
  if (syncReflowNode) {
    let forceSyncReflow = syncReflowNode.offsetWidth;
  }
  gIgnoreLayoutChanges = ignore;
}

/**
 * The LayoutChangesObserver class is instantiated only once per given tab
 * and is used to track reflows and dom and style changes in that tab.
 * The LayoutActor uses this class to send reflow events to its clients.
 *
 * This class isn't exported on the module because it shouldn't be instantiated
 * to avoid creating several instances per tabs.
 * Use `getLayoutChangesObserver(tabActor)`
 * and `releaseLayoutChangesObserver(tabActor)`
 * which are exported to get and release instances.
 *
 * The observer loops every EVENT_BATCHING_DELAY ms and checks if layout changes
 * have happened since the last loop iteration. If there are, it sends the
 * corresponding events:
 *
 * - "reflows", with an array of all the reflows that occured,
 *
 * @param {TabActor} tabActor
 */
function LayoutChangesObserver(tabActor) {
  Observable.call(this, tabActor);

  this._startEventLoop = this._startEventLoop.bind(this);

  // Creating the various observers we're going to need
  // For now, just the reflow observer, but later we can add markupMutation,
  // styleSheetChanges and styleRuleChanges
  this._onReflow = this._onReflow.bind(this);
  this.reflowObserver = new ReflowObserver(this.tabActor, this._onReflow);

  EventEmitter.decorate(this);
}

exports.LayoutChangesObserver = LayoutChangesObserver;

LayoutChangesObserver.prototype = Heritage.extend(Observable.prototype, {
  /**
   * How long does this observer waits before emitting a batched reflows event.
   * The lower the value, the more event packets will be sent to clients,
   * potentially impacting performance.
   * The higher the value, the more time we'll wait, this is better for
   * performance but has an effect on how soon changes are shown in the toolbox.
   */
  EVENT_BATCHING_DELAY: 300,

  /**
   * Destroying this instance of LayoutChangesObserver will stop the batched
   * events from being sent.
   */
  destroy: function() {
    this.reflowObserver.destroy();
    this.reflows = null;

    Observable.prototype.destroy.call(this);
  },

  _start: function() {
    this.reflows = [];
    this._startEventLoop();
    this.reflowObserver.start();
  },

  _stop: function() {
    this._stopEventLoop();
    this.reflows = [];
    this.reflowObserver.stop();
  },

  /**
   * Start the event loop, which regularly checks if there are any observer
   * events to be sent as batched events
   * Calls itself in a loop.
   */
  _startEventLoop: function() {
    // Avoid emitting events if the tabActor has been detached (may happen
    // during shutdown)
    if (!this.tabActor.attached) {
      return;
    }

    // Send any reflows we have
    if (this.reflows && this.reflows.length) {
      this.emit("reflows", this.reflows);
      this.reflows = [];
    }
    this.eventLoopTimer = this._setTimeout(this._startEventLoop,
      this.EVENT_BATCHING_DELAY);
  },

  _stopEventLoop: function() {
    this._clearTimeout(this.eventLoopTimer);
  },

  // Exposing set/clearTimeout here to let tests override them if needed
  _setTimeout: function(cb, ms) {
    return setTimeout(cb, ms);
  },
  _clearTimeout: function(t) {
    return clearTimeout(t);
  },

  /**
   * Executed whenever a reflow is observed. Only stacks the reflow in the
   * reflows array.
   * The EVENT_BATCHING_DELAY loop will take care of it later.
   * @param {Number} start When the reflow started
   * @param {Number} end When the reflow ended
   * @param {Boolean} isInterruptible
   */
  _onReflow: function(start, end, isInterruptible) {
    if (gIgnoreLayoutChanges) {
      return;
    }

    // XXX: when/if bug 997092 gets fixed, we will be able to know which
    // elements have been reflowed, which would be a nice thing to add here.
    this.reflows.push({
      start: start,
      end: end,
      isInterruptible: isInterruptible
    });
  }
});

/**
 * Get a LayoutChangesObserver instance for a given window. This function makes
 * sure there is only one instance per window.
 * @param {TabActor} tabActor
 * @return {LayoutChangesObserver}
 */
var observedWindows = new Map();
function getLayoutChangesObserver(tabActor) {
  let observerData = observedWindows.get(tabActor);
  if (observerData) {
    observerData.refCounting ++;
    return observerData.observer;
  }

  let obs = new LayoutChangesObserver(tabActor);
  observedWindows.set(tabActor, {
    observer: obs,
    refCounting: 1 // counting references allows to stop the observer when no
                   // tabActor owns an instance
  });
  obs.start();
  return obs;
};
exports.getLayoutChangesObserver = getLayoutChangesObserver;

/**
 * Release a LayoutChangesObserver instance that was retrieved by
 * getLayoutChangesObserver. This is required to ensure the tabActor reference
 * is removed and the observer is eventually stopped and destroyed.
 * @param {TabActor} tabActor
 */
function releaseLayoutChangesObserver(tabActor) {
  let observerData = observedWindows.get(tabActor);
  if (!observerData) {
    return;
  }

  observerData.refCounting --;
  if (!observerData.refCounting) {
    observerData.observer.destroy();
    observedWindows.delete(tabActor);
  }
};
exports.releaseLayoutChangesObserver = releaseLayoutChangesObserver;

/**
 * Instantiate and start a reflow observer on a given window's document element.
 * Will report any reflow that occurs in this window's docshell.
 * @extends Observable
 * @param {TabActor} tabActor
 * @param {Function} callback Executed everytime a reflow occurs
 */
function ReflowObserver(tabActor, callback) {
  Observable.call(this, tabActor, callback);

  this._onWindowReady = this._onWindowReady.bind(this);
  events.on(this.tabActor, "window-ready", this._onWindowReady);
  this._onWindowDestroyed = this._onWindowDestroyed.bind(this);
  events.on(this.tabActor, "window-destroyed", this._onWindowDestroyed);
}

ReflowObserver.prototype = Heritage.extend(Observable.prototype, {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIReflowObserver,
    Ci.nsISupportsWeakReference]),

  _onWindowReady: function({window}) {
    if (this.observing) {
      this._startListeners([window]);
    }
  },

  _onWindowDestroyed: function({window}) {
    if (this.observing) {
      this._stopListeners([window]);
    }
  },

  _start: function() {
    this._startListeners(this.tabActor.windows);
  },

  _stop: function() {
    if (this.tabActor.attached && this.tabActor.docShell) {
      // It's only worth stopping if the tabActor is still attached
      this._stopListeners(this.tabActor.windows);
    }
  },

  _startListeners: function(windows) {
    for (let window of windows) {
      let docshell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIWebNavigation)
                     .QueryInterface(Ci.nsIDocShell);
      docshell.addWeakReflowObserver(this);
    }
  },

  _stopListeners: function(windows) {
    for (let window of windows) {
      // Corner cases where a global has already been freed may happen, in which
      // case, no need to remove the observer
      try {
        let docshell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIWebNavigation)
                       .QueryInterface(Ci.nsIDocShell);
        docshell.removeWeakReflowObserver(this);
      } catch (e) {}
    }
  },

  reflow: function(start, end) {
    this.notifyCallback(start, end, false);
  },

  reflowInterruptible: function(start, end) {
    this.notifyCallback(start, end, true);
  },

  destroy: function() {
    if (this._isDestroyed) {
      return;
    }
    this._isDestroyed = true;

    events.off(this.tabActor, "window-ready", this._onWindowReady);
    events.off(this.tabActor, "window-destroyed", this._onWindowDestroyed);
    Observable.prototype.destroy.call(this);
  }
});
