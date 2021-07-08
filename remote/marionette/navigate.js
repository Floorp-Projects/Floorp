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
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  EventDispatcher:
    "chrome://remote/content/marionette/actors/MarionetteEventsParent.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  modal: "chrome://remote/content/marionette/modal.js",
  PageLoadStrategy: "chrome://remote/content/shared/webdriver/Capabilities.jsm",
  TimedPromise: "chrome://remote/content/marionette/sync.js",
  truncate: "chrome://remote/content/marionette/format.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.get(Log.TYPES.MARIONETTE)
);

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
 * @param {Object} options
 * @param {BrowsingContext=} options.browsingContext
 *     The current browsing context. Needed for targets of _parent and _top.
 * @param {URL=} options.future
 *     Destination URL, if known.
 * @param {target=} options.target
 *     Link target, if known.
 *
 * @return {boolean}
 *     Full page load would be expected if future is followed.
 *
 * @throws TypeError
 *     If <code>current</code> is not defined, or any of
 *     <code>current</code> or <code>future</code>  are invalid URLs.
 */
navigate.isLoadEventExpected = function(current, options = {}) {
  const { browsingContext, future, target } = options;

  if (typeof current == "undefined") {
    throw new TypeError("Expected at least one URL");
  }

  if (["_parent", "_top"].includes(target) && !browsingContext) {
    throw new TypeError(
      "Expected browsingContext when target is _parent or _top"
    );
  }

  // Don't wait if the navigation happens in a different browsing context
  if (
    target === "_blank" ||
    (target === "_parent" && browsingContext.parent) ||
    (target === "_top" && browsingContext.top != browsingContext)
  ) {
    return false;
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
    // Fake user activation.
    hasValidUserGestureActivation: true,
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
    browsingContextFn = driver.getBrowsingContext.bind(driver),
    loadEventExpected = true,
    requireBeforeUnload = true,
  } = options;

  const chromeWindow = browsingContextFn().topChromeWindow;
  const pageLoadStrategy = driver.currentSession.pageLoadStrategy;

  // Return immediately if no load event is expected
  if (!loadEventExpected || pageLoadStrategy === PageLoadStrategy.None) {
    await callback();
    return Promise.resolve();
  }

  let rejectNavigation;
  let resolveNavigation;

  let browsingContextChanged = false;
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

  const onDialogOpened = action => {
    if (action === modal.ACTION_OPENED) {
      logger.trace("Canceled page load listener because a dialog opened");
      checkDone({ finished: true });
    }
  };

  const onTimer = timer => {
    // In the case when a document has a beforeunload handler
    // registered, the currently active command will return immediately
    // due to the modal dialog observer.
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

  const onNavigation = (eventName, data) => {
    const browsingContext = browsingContextFn();

    // Ignore events from other browsing contexts than the selected one.
    if (data.browsingContext != browsingContext) {
      return;
    }

    logger.trace(
      truncate`[${data.browsingContext.id}] Received event ${data.type} for ${data.documentURI}`
    );

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
        // Don't require an unload event when a top-level browsing context
        // change occurred.
        if (!seenUnload && !browsingContextChanged) {
          return;
        }
        const result = checkReadyState(pageLoadStrategy, data);
        checkDone(result);
        break;
    }
  };

  // In the case when the currently selected frame is closed,
  // there will be no further load events. Stop listening immediately.
  const onBrowsingContextDiscarded = (subject, topic, why) => {
    // If the BrowsingContext is being discarded to be replaced by another
    // context, we don't want to stop waiting for the pageload to complete, as
    // we will continue listening to the newly created context.
    if (subject == browsingContextFn() && why != "replace") {
      logger.trace(
        "Canceled page load listener " +
          `because browsing context with id ${subject.id} has been removed`
      );
      checkDone({ finished: true });
    }
  };

  // Detect changes to the top-level browsing context to not
  // necessarily require an unload event.
  const onBrowsingContextChanged = event => {
    if (event.target === driver.curBrowser.contentBrowser) {
      browsingContextChanged = true;
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
  driver.curBrowser.tabBrowser?.addEventListener(
    "XULFrameLoaderCreated",
    onBrowsingContextChanged
  );
  driver.dialogObserver.add(onDialogOpened);
  Services.obs.addObserver(
    onBrowsingContextDiscarded,
    "browsing-context-discarded"
  );

  EventDispatcher.on("page-load", onNavigation);

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
      timeout: driver.currentSession.timeouts.pageLoad,
    }
  ).finally(() => {
    // Clean-up all registered listeners and timers
    Services.obs.removeObserver(
      onBrowsingContextDiscarded,
      "browsing-context-discarded"
    );
    chromeWindow.removeEventListener("TabClose", onUnload);
    chromeWindow.removeEventListener("unload", onUnload);
    driver.curBrowser.tabBrowser?.removeEventListener(
      "XULFrameLoaderCreated",
      onBrowsingContextChanged
    );
    driver.dialogObserver?.remove(onDialogOpened);
    unloadTimer?.cancel();

    EventDispatcher.off("page-load", onNavigation);
  });
};
