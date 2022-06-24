/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { GeckoViewChildModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewChildModule.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  clearTimeout: "resource://gre/modules/Timer.jsm",
  setTimeoutWithTarget: "resource://gre/modules/Timer.jsm",
  SessionHistory: "resource://gre/modules/sessionstore/SessionHistory.jsm",
});

const NO_INDEX = Number.MAX_SAFE_INTEGER;
const LAST_INDEX = Number.MAX_SAFE_INTEGER - 1;
const DEFAULT_INTERVAL_MS = 1500;

// This pref controls whether or not we send updates to the parent on a timeout
// or not, and should only be used for tests or debugging.
const TIMEOUT_DISABLED_PREF = "browser.sessionstore.debug.no_auto_updates";

const PREF_INTERVAL = "browser.sessionstore.interval";
const PREF_SESSION_COLLECTION = "browser.sessionstore.platform_collection";

class Handler {
  constructor(store) {
    this.store = store;
  }

  get mm() {
    return this.store.mm;
  }

  get eventDispatcher() {
    return this.store.eventDispatcher;
  }

  get messageQueue() {
    return this.store.messageQueue;
  }

  get stateChangeNotifier() {
    return this.store.stateChangeNotifier;
  }
}

/**
 * Listens for state change notifcations from webProgress and notifies each
 * registered observer for either the start of a page load, or its completion.
 */
class StateChangeNotifier extends Handler {
  constructor(store) {
    super(store);

    this._observers = new Set();
    const ifreq = this.mm.docShell.QueryInterface(Ci.nsIInterfaceRequestor);
    const webProgress = ifreq.getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(
      this,
      Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
    );
  }

  /**
   * Adds a given observer |obs| to the set of observers that will be notified
   * when when a new document starts or finishes loading.
   *
   * @param obs (object)
   */
  addObserver(obs) {
    this._observers.add(obs);
  }

  /**
   * Notifies all observers that implement the given |method|.
   *
   * @param method (string)
   */
  notifyObservers(method) {
    for (const obs of this._observers) {
      if (typeof obs[method] == "function") {
        obs[method]();
      }
    }
  }

  /**
   * @see nsIWebProgressListener.onStateChange
   */
  onStateChange(webProgress, request, stateFlags, status) {
    // Ignore state changes for subframes because we're only interested in the
    // top-document starting or stopping its load.
    if (!webProgress.isTopLevel || webProgress.DOMWindow != this.mm.content) {
      return;
    }

    // onStateChange will be fired when loading the initial about:blank URI for
    // a browser, which we don't actually care about. This is particularly for
    // the case of unrestored background tabs, where the content has not yet
    // been restored: we don't want to accidentally send any updates to the
    // parent when the about:blank placeholder page has loaded.
    if (!this.mm.docShell.hasLoadedNonBlankURI) {
      return;
    }

    if (stateFlags & Ci.nsIWebProgressListener.STATE_START) {
      this.notifyObservers("onPageLoadStarted");
    } else if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
      this.notifyObservers("onPageLoadCompleted");
    }
  }
}
StateChangeNotifier.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIWebProgressListener",
  "nsISupportsWeakReference",
]);

/**
 * Listens for changes to the session history. Whenever the user navigates
 * we will collect URLs and everything belonging to session history.
 *
 * Causes a SessionStore:update message to be sent that contains the current
 * session history.
 *
 * Example:
 *   {entries: [{url: "about:mozilla", ...}, ...], index: 1}
 */
class SessionHistoryListener extends Handler {
  constructor(store) {
    super(store);

    this._fromIdx = NO_INDEX;

    // The state change observer is needed to handle initial subframe loads.
    // It will redundantly invalidate with the SHistoryListener in some cases
    // but these invalidations are very cheap.
    this.stateChangeNotifier.addObserver(this);

    // By adding the SHistoryListener immediately, we will unfortunately be
    // notified of every history entry as the tab is restored. We don't bother
    // waiting to add the listener later because these notifications are cheap.
    // We will likely only collect once since we are batching collection on
    // a delay.
    this.mm.docShell
      .QueryInterface(Ci.nsIWebNavigation)
      .sessionHistory.legacySHistory.addSHistoryListener(this);

    // Listen for page title changes.
    this.mm.addEventListener("DOMTitleChanged", this);
  }

  uninit() {
    const sessionHistory = this.mm.docShell.QueryInterface(Ci.nsIWebNavigation)
      .sessionHistory;
    if (sessionHistory) {
      sessionHistory.legacySHistory.removeSHistoryListener(this);
    }
  }

  collect() {
    // We want to send down a historychange even for full collects in case our
    // session history is a partial session history, in which case we don't have
    // enough information for a full update. collectFrom(-1) tells the collect
    // function to collect all data avaliable in this process.
    if (this.mm.docShell) {
      this.collectFrom(-1);
    }
  }

  // History can grow relatively big with the nested elements, so if we don't have to, we
  // don't want to send the entire history all the time. For a simple optimization
  // we keep track of the smallest index from after any change has occured and we just send
  // the elements from that index. If something more complicated happens we just clear it
  // and send the entire history. We always send the additional info like the current selected
  // index (so for going back and forth between history entries we set the index to LAST_INDEX
  // if nothing else changed send an empty array and the additonal info like the selected index)
  collectFrom(idx) {
    if (this._fromIdx <= idx) {
      // If we already know that we need to update history fromn index N we can ignore any changes
      // tha happened with an element with index larger than N.
      // Note: initially we use NO_INDEX which is MAX_SAFE_INTEGER which means we don't ignore anything
      // here, and in case of navigation in the history back and forth we use LAST_INDEX which ignores
      // only the subsequent navigations, but not any new elements added.
      return;
    }

    this._fromIdx = idx;
    this.messageQueue.push("historychange", () => {
      if (this._fromIdx === NO_INDEX) {
        return null;
      }

      const history = SessionHistory.collect(this.mm.docShell, this._fromIdx);
      this._fromIdx = NO_INDEX;
      return history;
    });
  }

  handleEvent(event) {
    this.collect();
  }

  onPageLoadCompleted() {
    this.collect();
  }

  onPageLoadStarted() {
    this.collect();
  }

  OnHistoryNewEntry(newURI, oldIndex) {
    // We ought to collect the previously current entry as well, see bug 1350567.
    // TODO: Reenable partial history collection for performance
    // this.collectFrom(oldIndex);
    this.collect();
  }

  OnHistoryGotoIndex(index, gotoURI) {
    // We ought to collect the previously current entry as well, see bug 1350567.
    // TODO: Reenable partial history collection for performance
    // this.collectFrom(LAST_INDEX);
    this.collect();
  }

  OnHistoryPurge(numEntries) {
    this.collect();
  }

  OnHistoryReload(reloadURI, reloadFlags) {
    this.collect();
    return true;
  }

  OnHistoryReplaceEntry(index) {
    this.collect();
  }
}
SessionHistoryListener.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsISHistoryListener",
  "nsISupportsWeakReference",
]);

/**
 * Listens for scroll position changes. Whenever the user scrolls the top-most
 * frame we update the scroll position and will restore it when requested.
 *
 * Causes a SessionStore:update message to be sent that contains the current
 * scroll positions as a tree of strings. If no frame of the whole frame tree
 * is scrolled this will return null so that we don't tack a property onto
 * the tabData object in the parent process.
 *
 * Example:
 *   {scroll: "100,100", zoom: {resolution: "1.5", displaySize:
 *   {height: "1600", width: "1000"}}, children:
 *   [null, null, {scroll: "200,200"}]}
 */
class ScrollPositionListener extends Handler {
  constructor(store) {
    super(store);

    SessionStoreUtils.addDynamicFrameFilteredListener(
      this.mm,
      "mozvisualscroll",
      this,
      /* capture */ false,
      /* system group */ true
    );

    SessionStoreUtils.addDynamicFrameFilteredListener(
      this.mm,
      "mozvisualresize",
      this,
      /* capture */ false,
      /* system group */ true
    );

    this.stateChangeNotifier.addObserver(this);
  }

  handleEvent() {
    this.messageQueue.push("scroll", () => this.collect());
  }

  onPageLoadCompleted() {
    this.messageQueue.push("scroll", () => this.collect());
  }

  onPageLoadStarted() {
    this.messageQueue.push("scroll", () => null);
  }

  collect() {
    // TODO: Keep an eye on bug 1525259; we may not have to manually store zoom
    // Save the current document resolution.
    let zoom = 1;
    const scrolldata =
      SessionStoreUtils.collectScrollPosition(this.mm.content) || {};
    const domWindowUtils = this.mm.content.windowUtils;
    zoom = domWindowUtils.getResolution();
    scrolldata.zoom = {};
    scrolldata.zoom.resolution = zoom;

    // Save some data that'll help in adjusting the zoom level
    // when restoring in a different screen orientation.
    const displaySize = {};
    const width = {},
      height = {};
    domWindowUtils.getContentViewerSize(width, height);

    displaySize.width = width.value;
    displaySize.height = height.value;

    scrolldata.zoom.displaySize = displaySize;

    return scrolldata;
  }
}

/**
 * Listens for changes to input elements. Whenever the value of an input
 * element changes we will re-collect data for the current frame tree and send
 * a message to the parent process.
 *
 * Causes a SessionStore:update message to be sent that contains the form data
 * for all reachable frames.
 *
 * Example:
 *   {
 *     formdata: {url: "http://mozilla.org/", id: {input_id: "input value"}},
 *     children: [
 *       null,
 *       {url: "http://sub.mozilla.org/", id: {input_id: "input value 2"}}
 *     ]
 *   }
 */
class FormDataListener extends Handler {
  constructor(store) {
    super(store);

    SessionStoreUtils.addDynamicFrameFilteredListener(
      this.mm,
      "input",
      this,
      true
    );
    this.stateChangeNotifier.addObserver(this);
  }

  handleEvent() {
    this.messageQueue.push("formdata", () => this.collect());
  }

  onPageLoadStarted() {
    this.messageQueue.push("formdata", () => null);
  }

  collect() {
    return SessionStoreUtils.collectFormData(this.mm.content);
  }
}

/**
 * A message queue that takes collected data and will take care of sending it
 * to the chrome process. It allows flushing using synchronous messages and
 * takes care of any race conditions that might occur because of that. Changes
 * will be batched if they're pushed in quick succession to avoid a message
 * flood.
 */
class MessageQueue extends Handler {
  constructor(store) {
    super(store);

    /**
     * A map (string -> lazy fn) holding lazy closures of all queued data
     * collection routines. These functions will return data collected from the
     * docShell.
     */
    this._data = new Map();

    /**
     * The delay (in ms) used to delay sending changes after data has been
     * invalidated.
     */
    this.BATCH_DELAY_MS = 1000;

    /**
     * The minimum idle period (in ms) we need for sending data to chrome process.
     */
    this.NEEDED_IDLE_PERIOD_MS = 5;

    /**
     * Timeout for waiting an idle period to send data. We will set this from
     * the pref "browser.sessionstore.interval".
     */
    this._timeoutWaitIdlePeriodMs = null;

    /**
     * The current timeout ID, null if there is no queue data. We use timeouts
     * to damp a flood of data changes and send lots of changes as one batch.
     */
    this._timeout = null;

    /**
     * Whether or not sending batched messages on a timer is disabled. This should
     * only be used for debugging or testing. If you need to access this value,
     * you should probably use the timeoutDisabled getter.
     */
    this._timeoutDisabled = false;

    /**
     * True if there is already a send pending idle dispatch, set to prevent
     * scheduling more than one. If false there may or may not be one scheduled.
     */
    this._idleScheduled = false;

    this.timeoutDisabled = Services.prefs.getBoolPref(
      TIMEOUT_DISABLED_PREF,
      false
    );
    this.sessionCollection = Services.prefs.getBoolPref(
      PREF_SESSION_COLLECTION,
      false
    );
    this._timeoutWaitIdlePeriodMs = Services.prefs.getIntPref(
      PREF_INTERVAL,
      DEFAULT_INTERVAL_MS
    );

    Services.prefs.addObserver(TIMEOUT_DISABLED_PREF, this);
    Services.prefs.addObserver(PREF_INTERVAL, this);
  }

  /**
   * True if batched messages are not being fired on a timer. This should only
   * ever be true when debugging or during tests.
   */
  get timeoutDisabled() {
    return this._timeoutDisabled;
  }

  /**
   * Disables sending batched messages on a timer. Also cancels any pending
   * timers.
   */
  set timeoutDisabled(val) {
    this._timeoutDisabled = val;

    if (val && this._timeout) {
      clearTimeout(this._timeout);
      this._timeout = null;
    }
  }

  uninit() {
    this.cleanupTimers();
  }

  /**
   * Cleanup pending idle callback and timer.
   */
  cleanupTimers() {
    this._idleScheduled = false;
    if (this._timeout) {
      clearTimeout(this._timeout);
      this._timeout = null;
    }
  }

  observe(subject, topic, data) {
    if (topic == "nsPref:changed") {
      switch (data) {
        case TIMEOUT_DISABLED_PREF:
          this.timeoutDisabled = Services.prefs.getBoolPref(
            TIMEOUT_DISABLED_PREF,
            false
          );
          break;
        case PREF_INTERVAL:
          this._timeoutWaitIdlePeriodMs = Services.prefs.getIntPref(
            PREF_INTERVAL,
            DEFAULT_INTERVAL_MS
          );
          break;
        default:
          debug`Received unknown message: ${data}`;
          break;
      }
    }
  }

  /**
   * Pushes a given |value| onto the queue. The given |key| represents the type
   * of data that is stored and can override data that has been queued before
   * but has not been sent to the parent process, yet.
   *
   * @param key (string)
   *        A unique identifier specific to the type of data this is passed.
   * @param fn (function)
   *        A function that returns the value that will be sent to the parent
   *        process.
   */
  push(key, fn) {
    this._data.set(key, fn);

    if (!this._timeout && !this._timeoutDisabled) {
      // Wait a little before sending the message to batch multiple changes.
      this._timeout = setTimeoutWithTarget(
        () => this.sendWhenIdle(),
        this.BATCH_DELAY_MS,
        this.mm.tabEventTarget
      );
    }
  }

  /**
   * Sends queued data when the remaining idle time is enough or waiting too
   * long; otherwise, request an idle time again. If the |deadline| is not
   * given, this function is going to schedule the first request.
   *
   * @param deadline (object)
   *        An IdleDeadline object passed by idleDispatch().
   */
  sendWhenIdle(deadline) {
    if (!this.mm.content) {
      // The frameloader is being torn down. Nothing more to do.
      return;
    }

    if (deadline) {
      if (
        deadline.didTimeout ||
        deadline.timeRemaining() > this.NEEDED_IDLE_PERIOD_MS
      ) {
        this.send();
        return;
      }
    } else if (this._idleScheduled) {
      // Bail out if there's a pending run.
      return;
    }
    ChromeUtils.idleDispatch(deadline_ => this.sendWhenIdle(deadline_), {
      timeout: this._timeoutWaitIdlePeriodMs,
    });
    this._idleScheduled = true;
  }

  /**
   * Sends queued data to the chrome process.
   *
   * @param options (object)
   *        {isFinal: true} to signal this is the final message sent on unload
   */
  send(options = {}) {
    // Looks like we have been called off a timeout after the tab has been
    // closed. The docShell is gone now and we can just return here as there
    // is nothing to do.
    if (!this.mm.docShell) {
      return;
    }

    this.cleanupTimers();

    const data = {};
    for (const [key, func] of this._data) {
      const value = func();

      if (value || (key != "storagechange" && key != "historychange")) {
        data[key] = value;
      }
    }

    this._data.clear();

    try {
      // Send all data to the parent process.
      this.eventDispatcher.sendRequest({
        type: "GeckoView:StateUpdated",
        data,
        isFinal: options.isFinal || false,
        epoch: this.store.epoch,
      });
    } catch (ex) {
      if (ex && ex.result == Cr.NS_ERROR_OUT_OF_MEMORY) {
        warn`Failed to save session state`;
      }
    }
  }
}

class SessionStateAggregator extends GeckoViewChildModule {
  constructor(aModuleName, aMessageManager) {
    super(aModuleName, aMessageManager);

    this.mm = aMessageManager;
    this.messageQueue = new MessageQueue(this);
    this.stateChangeNotifier = new StateChangeNotifier(this);

    this.handlers = [
      new SessionHistoryListener(this),
      this.stateChangeNotifier,
      this.messageQueue,
    ];
    if (!this.sessionCollection) {
      this.handlers.push(
        new FormDataListener(this),
        new ScrollPositionListener(this)
      );
    }

    this.messageManager.addMessageListener("GeckoView:FlushSessionState", this);
  }

  receiveMessage(aMsg) {
    debug`receiveMessage: ${aMsg.name}`;

    switch (aMsg.name) {
      case "GeckoView:FlushSessionState":
        this.flush();
        break;
    }
  }

  flush() {
    // Flush the message queue, send the latest updates.
    this.messageQueue.send();
  }

  onUnload() {
    // Upon frameLoader destruction, send a final update message to
    // the parent and flush all data currently held in the child.
    this.messageQueue.send({ isFinal: true });

    for (const handler of this.handlers) {
      if (handler.uninit) {
        handler.uninit();
      }
    }

    // We don't need to take care of any StateChangeNotifier observers as they
    // will die with the content script.
  }
}

// TODO: Bug 1648158 Move SessionAggregator to the parent process
class DummySessionStateAggregator extends GeckoViewChildModule {
  constructor(aModuleName, aMessageManager) {
    super(aModuleName, aMessageManager);
    this.messageManager.addMessageListener("GeckoView:FlushSessionState", this);
  }

  receiveMessage(aMsg) {
    debug`receiveMessage: ${aMsg.name}`;

    switch (aMsg.name) {
      case "GeckoView:FlushSessionState":
        // Do nothing
        break;
    }
  }
}

const { debug, warn } = SessionStateAggregator.initLogging(
  "SessionStateAggregator"
);

const module = Services.appinfo.sessionHistoryInParent
  ? // If history is handled in the parent we don't need a session aggregator
    // TODO: Bug 1648158 remove this and do everything in the parent
    DummySessionStateAggregator.create(this)
  : SessionStateAggregator.create(this);
