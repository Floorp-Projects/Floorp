/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["navigate"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  error: "chrome://marionette/content/error.js",
  EventDispatcher:
    "chrome://marionette/content/actors/MarionetteEventsParent.jsm",
  Log: "chrome://marionette/content/log.js",
  MarionettePrefs: "chrome://marionette/content/prefs.js",
  modal: "chrome://marionette/content/modal.js",
  PageLoadStrategy: "chrome://marionette/content/capabilities.js",
  registerEventsActor:
    "chrome://marionette/content/actors/MarionetteEventsParent.jsm",
  TimedPromise: "chrome://marionette/content/sync.js",
  truncate: "chrome://marionette/content/format.js",
  unregisterEventsActor:
    "chrome://marionette/content/actors/MarionetteEventsParent.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

// Timeouts used to check if a new navigation has been initiated.
const TIMEOUT_BEFOREUNLOAD_EVENT = 200;
const TIMEOUT_UNLOAD_EVENT = 5000;

/** @namespace */
this.navigate = {};

/**
 * Checks the value of readyState for the current page
 * load activity, and resolves the command if the load
 * has been finished. It also takes care of the selected
 * page load strategy.
 *
 * @param {PageLoadStrategy} pageLoadStrategy
 *     Strategy when navigation is considered as finished.
 * @param {object} eventData
 * @param {string} eventData.documentURI
 *     Current document URI of the document.
 * @param {string} eventData.readyState
 *     Current ready state of the document.
 *
 * @return {boolean}
 *     True if the page load has been finished.
 */
function checkReadyState(pageLoadStrategy, eventData = {}) {
  const { documentURI, readyState } = eventData;

  const result = { error: null, finished: false };

  switch (readyState) {
    case "interactive":
      if (documentURI.startsWith("about:certerror")) {
        result.error = new error.InsecureCertificateError();
        result.finished = true;
      } else if (/about:.*(error)\?/.exec(documentURI)) {
        result.error = new error.UnknownError(
          `Reached error page: ${documentURI}`
        );
        result.finished = true;

        // Return early with a page load strategy of eager, and also
        // special-case about:blocked pages which should be treated as
        // non-error pages but do not raise a pageshow event. about:blank
        // is also treaded specifically here, because it gets temporary
        // loaded for new content processes, and we only want to rely on
        // complete loads for it.
      } else if (
        (pageLoadStrategy === PageLoadStrategy.Eager &&
          documentURI != "about:blank") ||
        /about:blocked\?/.exec(documentURI)
      ) {
        result.finished = true;
      }
      break;

    case "complete":
      result.finished = true;
      break;
  }

  return result;
}

/**
 * Determines if we expect to get a DOM load event (DOMContentLoaded)
 * on navigating to the <code>future</code> URL.
 *
 * @param {URL} current
 *     URL the browser is currently visiting.
 * @param {URL=} future
 *     Destination URL, if known.
 *
 * @return {boolean}
 *     Full page load would be expected if future is followed.
 *
 * @throws TypeError
 *     If <code>current</code> is not defined, or any of
 *     <code>current</code> or <code>future</code>  are invalid URLs.
 */
navigate.isLoadEventExpected = function(current, future = undefined) {
  // assume we will go somewhere exciting
  if (typeof current == "undefined") {
    throw new TypeError("Expected at least one URL");
  }

  // Assume we will go somewhere exciting
  if (typeof future == "undefined") {
    return true;
  }

  // Assume javascript:<whatever> will modify the current document
  // but this is not an entirely safe assumption to make,
  // considering it could be used to set window.location
  if (future.protocol == "javascript:") {
    return false;
  }

  // If hashes are present and identical
  if (
    current.href.includes("#") &&
    future.href.includes("#") &&
    current.hash === future.hash
  ) {
    return false;
  }

  return true;
};

/**
 * Load the given URL in the specified browsing context.
 *
 * @param {CanonicalBrowsingContext} browsingContext
 *     Browsing context to load the URL into.
 * @param {string} url
 *     URL to navigate to.
 */
navigate.navigateTo = async function(browsingContext, url) {
  const opts = {
    loadFlags: Ci.nsIWebNavigation.LOAD_FLAGS_IS_LINK,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  };
  browsingContext.loadURI(url, opts);
};

/**
 * Reload the page.
 *
 * @param {CanonicalBrowsingContext} browsingContext
 *     Browsing context to refresh.
 */
navigate.refresh = async function(browsingContext) {
  const flags = Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
  browsingContext.reload(flags);
};

/**
 * Execute a callback and wait for a possible navigation to complete
 *
 * @param {GeckoDriver} driver
 *     Reference to driver instance.
 * @param {Function} callback
 *     Callback to execute that might trigger a navigation.
 * @param {Object} options
 * @param {BrowsingContext=} browsingContext
 *     Browsing context to observe. Defaults to the current browsing context.
 * @param {boolean=} loadEventExpected
 *     If false, return immediately and don't wait for
 *     the navigation to be completed. Defaults to true.
 * @param {boolean=} requireBeforeUnload
 *     If false and no beforeunload event is fired, abort waiting
 *     for the navigation. Defaults to true.
 */
navigate.waitForNavigationCompleted = async function waitForNavigationCompleted(
  driver,
  callback,
  options = {}
) {
  const {
    browsingContext = driver.getBrowsingContext(),
    loadEventExpected = true,
    requireBeforeUnload = true,
  } = options;

  const chromeWindow = browsingContext.topChromeWindow;
  const pageLoadStrategy = driver.capabilities.get("pageLoadStrategy");

  // Return immediately if no load event is expected
  if (!loadEventExpected || pageLoadStrategy === PageLoadStrategy.None) {
    await callback();
    return Promise.resolve();
  }

  let rejectNavigation;
  let resolveNavigation;

  let seenBeforeUnload = false;
  let seenUnload = false;

  let unloadTimer;

  const checkDone = ({ finished, error }) => {
    if (finished) {
      if (error) {
        rejectNavigation(error);
      } else {
        resolveNavigation();
      }
    }
  };

  const onDialogOpened = (action, dialog, win) => {
    // Only care about modals of the currently selected window.
    if (win !== chromeWindow) {
      return;
    }

    if (action === modal.ACTION_OPENED) {
      logger.trace("Canceled page load listener because a dialog opened");
      checkDone({ finished: true });
    }
  };

  const onTimer = timer => {
    // In the case when a document has a beforeunload handler
    // registered, the currently active command will return immediately
    // due to the modal dialog observer in proxy.js.
    //
    // Otherwise the timeout waiting for the document to start
    // navigating is increased by 5000 ms to ensure a possible load
    // event is not missed. In the common case such an event should
    // occur pretty soon after beforeunload, and we optimise for this.
    if (seenBeforeUnload) {
      seenBeforeUnload = false;
      unloadTimer.initWithCallback(
        onTimer,
        TIMEOUT_UNLOAD_EVENT,
        Ci.nsITimer.TYPE_ONE_SHOT
      );

      // If no page unload has been detected, ensure to properly stop
      // the load listener, and return from the currently active command.
    } else if (!seenUnload) {
      logger.trace(
        "Canceled page load listener because no navigation " +
          "has been detected"
      );
      checkDone({ finished: true });
    }
  };

  const onNavigation = ({ json }, message) => {
    let data = MarionettePrefs.useActors ? message : json;

    if (MarionettePrefs.useActors) {
      // Only care about navigation events from the actor of the current frame.
      // Bug 1674329: Always use the currently active browsing context,
      // and not the original one to not cause hangs for remoteness changes.
      if (data.browsingContext != driver.getBrowsingContext()) {
        return;
      }
    } else if (data.browsingContext.browserId != browsingContext.browserId) {
      return;
    }

    logger.trace(truncate`Received event ${data.type} for ${data.documentURI}`);

    switch (data.type) {
      case "beforeunload":
        seenBeforeUnload = true;
        break;

      case "pagehide":
        seenUnload = true;
        break;

      case "hashchange":
      case "popstate":
        checkDone({ finished: true });
        break;

      case "DOMContentLoaded":
      case "pageshow":
        if (!seenUnload) {
          return;
        }
        const result = checkReadyState(pageLoadStrategy, data);
        checkDone(result);
        break;
    }
  };

  // In the case when the currently selected frame is closed,
  // there will be no further load events. Stop listening immediately.
  const onBrowsingContextDiscarded = (subject, topic) => {
    // With the currentWindowGlobal gone the browsing context hasn't been
    // replaced due to a remoteness change but closed.
    if (subject == browsingContext && !subject.currentWindowGlobal) {
      logger.trace(
        "Canceled page load listener " +
          `because frame with id ${subject.id} has been removed`
      );
      checkDone({ finished: true });
    }
  };

  const onUnload = event => {
    logger.trace(
      "Canceled page load listener " +
        "because the top-browsing context has been closed"
    );
    checkDone({ finished: true });
  };

  chromeWindow.addEventListener("TabClose", onUnload);
  chromeWindow.addEventListener("unload", onUnload);
  driver.dialogObserver.add(onDialogOpened);
  Services.obs.addObserver(
    onBrowsingContextDiscarded,
    "browsing-context-discarded"
  );

  if (MarionettePrefs.useActors) {
    // Register the JSWindowActor pair for events as used by Marionette
    registerEventsActor();
    EventDispatcher.on("page-load", onNavigation);
  } else {
    driver.mm.addMessageListener(
      "Marionette:NavigationEvent",
      onNavigation,
      true
    );
  }

  return new TimedPromise(
    async (resolve, reject) => {
      rejectNavigation = reject;
      resolveNavigation = resolve;

      try {
        await callback();

        // Certain commands like clickElement can cause a navigation. Setup a timer
        // to check if a "beforeunload" event has been emitted within the given
        // time frame. If not resolve the Promise.
        if (!requireBeforeUnload) {
          unloadTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
          unloadTimer.initWithCallback(
            onTimer,
            TIMEOUT_BEFOREUNLOAD_EVENT,
            Ci.nsITimer.TYPE_ONE_SHOT
          );
        }
      } catch (e) {
        // Executing the callback above could destroy the actor pair before the
        // command returns. Such an error has to be ignored.
        if (e.name !== "AbortError") {
          checkDone({ finished: true, error: e });
        }
      }
    },
    {
      timeout: driver.timeouts.pageLoad,
    }
  ).finally(() => {
    // Clean-up all registered listeners and timers
    Services.obs.removeObserver(
      onBrowsingContextDiscarded,
      "browsing-context-discarded"
    );
    chromeWindow.removeEventListener("TabClose", onUnload);
    chromeWindow.removeEventListener("unload", onUnload);
    driver.dialogObserver?.remove(onDialogOpened);
    unloadTimer?.cancel();

    if (MarionettePrefs.useActors) {
      EventDispatcher.off("page-load", onNavigation);
      unregisterEventsActor();
    } else {
      driver.mm.removeMessageListener(
        "Marionette:NavigationEvent",
        onNavigation,
        true
      );
    }
  });
};
