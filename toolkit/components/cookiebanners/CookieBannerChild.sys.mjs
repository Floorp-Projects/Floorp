/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serviceMode",
  "cookiebanners.service.mode",
  Ci.nsICookieBannerService.MODE_DISABLED
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serviceModePBM",
  "cookiebanners.service.mode.privateBrowsing",
  Ci.nsICookieBannerService.MODE_DISABLED
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "prefDetectOnly",
  "cookiebanners.service.detectOnly",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "bannerClickingEnabled",
  "cookiebanners.bannerClicking.enabled",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "observeTimeout",
  "cookiebanners.bannerClicking.timeout",
  3000
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "testing",
  "cookiebanners.bannerClicking.testing",
  false
);

ChromeUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "CookieBannerChild",
    maxLogLevelPref: "cookiebanners.bannerClicking.logLevel",
  });
});

export class CookieBannerChild extends JSWindowActorChild {
  // Caches the enabled state to ensure we only compute it once for the lifetime
  // of the actor. Particularly the private browsing check can be expensive.
  #isEnabledCached = null;
  #clickRules;
  #originalBannerDisplay = null;
  #observerCleanUp;
  #observerCleanUpTimer;
  // Indicates whether the page "load" event occurred.
  #didLoad = false;

  // Used to keep track of click telemetry for the current window.
  #telemetryStatus = {
    currentStage: null,
    success: false,
    successStage: null,
    failReason: null,
    bannerVisibilityFail: false,
  };
  // For measuring the cookie banner handling duration.
  #gleanBannerHandlingTimer = null;

  handleEvent(event) {
    if (!this.#isEnabled) {
      // Automated tests may still expect the test message to be sent.
      this.#maybeSendTestMessage();
      return;
    }

    switch (event.type) {
      case "DOMContentLoaded":
        this.#onDOMContentLoaded();
        break;
      case "load":
        this.#onLoad();
        break;
      default:
        lazy.logConsole.warn(`Unexpected event ${event.type}.`, event);
    }
  }

  get #isPrivateBrowsing() {
    return lazy.PrivateBrowsingUtils.isContentWindowPrivate(this.contentWindow);
  }

  /**
   * Whether the feature is enabled based on pref state.
   * @type {boolean} true if feature is enabled, false otherwise.
   */
  get #isEnabled() {
    if (this.#isEnabledCached != null) {
      return this.#isEnabledCached;
    }

    let checkIsEnabled = () => {
      if (!lazy.bannerClickingEnabled) {
        return false;
      }
      if (this.#isPrivateBrowsing) {
        return lazy.serviceModePBM != Ci.nsICookieBannerService.MODE_DISABLED;
      }
      return lazy.serviceMode != Ci.nsICookieBannerService.MODE_DISABLED;
    };

    this.#isEnabledCached = checkIsEnabled();
    return this.#isEnabledCached;
  }

  /**
   * Whether the feature is enabled in detect-only-mode where cookie banner
   * detection events are dispatched, but banners aren't handled.
   * @type {boolean} true if feature mode is enabled, false otherwise.
   */
  get #isDetectOnly() {
    // We can't be in detect-only-mode if fully disabled.
    if (!this.#isEnabled) {
      return false;
    }
    return lazy.prefDetectOnly;
  }

  /**
   * @returns {boolean} Whether we handled a banner for the current load by
   * injecting cookies.
   */
  get #hasInjectedCookieForCookieBannerHandling() {
    return this.docShell?.currentDocumentChannel?.loadInfo
      ?.hasInjectedCookieForCookieBannerHandling;
  }

  /**
   * Checks whether we handled a banner for this site by injecting cookies and
   * dispatches events.
   * @returns {boolean} Whether we handled the banner and dispatched events.
   */
  #dispatchEventsForBannerHandledByInjection() {
    if (!this.#hasInjectedCookieForCookieBannerHandling) {
      return false;
    }
    // Strictly speaking we don't actively detect a banner when we handle it by
    // cookie injection. We still dispatch "cookiebannerdetected" in this case
    // for consistency.
    this.sendAsyncMessage("CookieBanner::DetectedBanner");
    this.sendAsyncMessage("CookieBanner::HandledBanner");
    return true;
  }

  /**
   * Handler for DOMContentLoaded events which is the entry point for cookie
   * banner handling.
   */
  async #onDOMContentLoaded() {
    lazy.logConsole.debug("onDOMContentLoaded", { didLoad: this.#didLoad });
    this.#didLoad = false;
    this.#telemetryStatus.currentStage = "dom_content_loaded";

    let principal = this.document?.nodePrincipal;

    // We only apply banner auto-clicking if the document has a content
    // principal.
    if (!principal?.isContentPrincipal) {
      return;
    }

    // We don't need to do auto-clicking if it's not a http/https page.
    if (!principal.schemeIs("http") && !principal.schemeIs("https")) {
      return;
    }

    lazy.logConsole.debug("Send message to get rule", {
      baseDomain: principal.baseDomain,
      isTopLevel: this.browsingContext == this.browsingContext?.top,
    });
    let rules;

    try {
      rules = await this.sendQuery("CookieBanner::GetClickRules", {});
    } catch (e) {
      lazy.logConsole.warn("Failed to get click rule from parent.", e);
      return;
    }

    lazy.logConsole.debug("Got rules:", rules);
    // We can stop here if we don't have a rule.
    if (!rules.length) {
      // If the cookie injector has handled the banner and there are no click
      // rules we still need to dispatch a "cookiebannerhandled" event.
      let dispatchedEvents = this.#dispatchEventsForBannerHandledByInjection();
      // Record telemetry about handling the banner via cookie injection.
      // Note: The success state recorded here may be invalid if the given
      // cookie fails to handle the banner. Since we don't have a presence
      // detector for this rule we can't determine whether the banner is still
      // showing or not.
      if (dispatchedEvents) {
        this.#telemetryStatus.failReason = null;
        this.#telemetryStatus.success = true;
        this.#telemetryStatus.successStage = "cookie_injected";
      }

      this.#maybeSendTestMessage();
      return;
    }

    this.#clickRules = rules;

    if (!this.#isDetectOnly) {
      // Start a timer to measure how long it takes for the banner to appear and
      // be handled.
      this.#gleanBannerHandlingTimer =
        Glean.cookieBannersClick.handleDuration.start();
    }

    let { bannerHandled, bannerDetected, matchedRule } =
      await this.handleCookieBanner();

    let dispatchedEventsForCookieInjection =
      this.#dispatchEventsForBannerHandledByInjection();
    // A cookie injection followed by not detecting the banner via querySelector
    // is a success state. Record that in telemetry.
    // Note: The success state reported may be invalid in edge cases where both
    // the cookie injection and the banner detection via query selector fails.
    if (dispatchedEventsForCookieInjection && !bannerDetected) {
      this.#telemetryStatus.success = true;
      this.#telemetryStatus.successStage = "cookie_injected";
    }

    // 1. Detected event.
    if (bannerDetected) {
      lazy.logConsole.info("Detected cookie banner.", {
        url: this.document?.location.href,
      });
      // Avoid dispatching a duplicate "cookiebannerdetected" event.
      if (!dispatchedEventsForCookieInjection) {
        this.sendAsyncMessage("CookieBanner::DetectedBanner");
      }
    }

    // 2. Handled event.
    if (bannerHandled) {
      lazy.logConsole.info("Handled cookie banner.", {
        url: this.document?.location.href,
        rule: matchedRule,
      });

      // Stop the timer to record how long it took to handle the banner.
      lazy.logConsole.debug(
        "Telemetry timer: stop and accumulate",
        this.#gleanBannerHandlingTimer
      );
      Glean.cookieBannersClick.handleDuration.stopAndAccumulate(
        this.#gleanBannerHandlingTimer
      );

      // Avoid dispatching a duplicate "cookiebannerhandled" event.
      if (!dispatchedEventsForCookieInjection) {
        this.sendAsyncMessage("CookieBanner::HandledBanner");
      }
    } else if (!this.#isDetectOnly) {
      // Cancel the timer we didn't handle the banner.
      Glean.cookieBannersClick.handleDuration.cancel(
        this.#gleanBannerHandlingTimer
      );
    }

    this.#maybeSendTestMessage();
  }

  /**
   * Handler for "load" events. Used as a signal to stop observing the DOM for
   * cookie banners after a timeout.
   */
  #onLoad() {
    this.#didLoad = true;

    // Exit early if we are not handling banners for this site.
    if (!this.#clickRules?.length) {
      return;
    }

    lazy.logConsole.debug("Observed 'load' event", {
      href: this.document?.location.href,
      hasActiveObserver: !!this.#observerCleanUp,
      observerCleanupTimer: this.#observerCleanUpTimer,
    });

    // Update stage for click telemetry.
    if (!this.#telemetryStatus.success) {
      this.#telemetryStatus.currentStage = "mutation_post_load";
    }

    this.#startObserverCleanupTimer();
  }

  /**
   * If there is an active mutation observer, start a timeout to unregister it.
   */
  #startObserverCleanupTimer() {
    // We limit how long we observe cookie banner mutations for performance
    // reasons. If not present initially on DOMContentLoaded, cookie banners are
    // expected to show up during or shortly after page load.
    if (!this.#observerCleanUp || this.#observerCleanUpTimer) {
      return;
    }
    lazy.logConsole.debug("Starting MutationObserver cleanup timeout");
    this.#observerCleanUpTimer = lazy.setTimeout(() => {
      lazy.logConsole.debug(
        `MutationObserver timeout after ${lazy.observeTimeout}ms.`
      );
      this.#observerCleanUp();
    }, lazy.observeTimeout);
  }

  didDestroy() {
    this.#reportTelemetry();

    // Clean up the observer and timer if needed.
    this.#observerCleanUp?.();
  }

  #reportTelemetry() {
    // Nothing to report, banner handling didn't run.
    if (
      this.#telemetryStatus.successStage == null &&
      this.#telemetryStatus.failReason == null
    ) {
      lazy.logConsole.debug(
        "Skip telemetry",
        this.#telemetryStatus,
        this.#clickRules
      );
      return;
    }

    let { success, successStage, currentStage, failReason } =
      this.#telemetryStatus;

    // Check if we got interrupted during an observe.
    if (this.#observerCleanUp && !success) {
      failReason = "actor_destroyed";
    }

    let status, reason;
    if (success) {
      status = "success";
      reason = successStage;
    } else {
      status = "fail";
      reason = failReason;
    }

    // Increment general success or failure counter.
    Glean.cookieBannersClick.result[status].add(1);
    // Increment reason counters.
    if (reason) {
      Glean.cookieBannersClick.result[`${status}_${reason}`].add(1);
    } else {
      lazy.logConsole.debug(
        "Could not determine success / fail reason for telemetry."
      );
    }

    lazy.logConsole.debug("Submitted clickResult telemetry", status, reason, {
      success,
      successStage,
      currentStage,
      failReason,
    });
  }

  /**
   * The function to perform the core logic of handing the cookie banner. It
   * will detect the banner and click the banner button whenever possible
   * according to the given click rules.
   * If the service mode pref is set to detect only mode we will only attempt to
   * find the cookie banner element and return early.
   *
   * @returns A promise which resolves when it finishes auto clicking.
   */
  async handleCookieBanner() {
    lazy.logConsole.debug("handleCookieBanner", this.document?.location.href);

    // First, we detect if the banner is shown on the page
    let rules = await this.#detectBanner();

    if (!rules.length) {
      // The banner was never shown.
      this.#telemetryStatus.success = false;
      if (this.#telemetryStatus.bannerVisibilityFail) {
        this.#telemetryStatus.failReason = "banner_not_visible";
      } else {
        this.#telemetryStatus.failReason = "banner_not_found";
      }

      return { bannerHandled: false, bannerDetected: false };
    }

    // No rule with valid button to click. This can happen if we're in
    // MODE_REJECT and there are only opt-in buttons available.
    // This also applies when detect-only mode is enabled. We only want to
    // dispatch events matching the current service mode.
    if (rules.every(rule => rule.target == null)) {
      this.#telemetryStatus.success = false;
      this.#telemetryStatus.failReason = "no_rule_for_mode";
      return { bannerHandled: false, bannerDetected: false };
    }

    // If the cookie banner prefs only enable detection but not handling we're done here.
    if (this.#isDetectOnly) {
      return { bannerHandled: false, bannerDetected: true };
    }

    // Hide the banner.
    let matchedRule = this.#hideBanner(rules);

    let successClick = false;
    try {
      successClick = await this.#clickTarget(rules);
    } finally {
      if (!successClick) {
        // We cannot successfully click the target button. Show the banner on
        // the page so that user can interact with the banner.
        this.#showBanner(matchedRule);
      }
    }

    if (successClick) {
      // For telemetry, Keep track of in which stage we successfully handled the banner.
      this.#telemetryStatus.successStage = this.#telemetryStatus.currentStage;
    } else {
      this.#telemetryStatus.failReason = "button_not_found";
      this.#telemetryStatus.successStage = null;
    }
    this.#telemetryStatus.success = successClick;

    return { bannerHandled: successClick, bannerDetected: true, matchedRule };
  }

  /**
   * The helper function to observe the changes on the document with a timeout.
   * It will call the check function when it observes mutations on the document
   * body. Once the check function returns a truthy value, it will resolve with
   * that value. Otherwise, it will resolve with null on timeout.
   *
   * @param {function} [checkFn] - The check function.
   * @returns {Promise} - A promise which resolves with the return value of the
   * check function or null if the function times out.
   */
  #promiseObserve(checkFn) {
    if (this.#observerCleanUp) {
      throw new Error(
        "The promiseObserve is called before previous one resolves."
      );
    }
    lazy.logConsole.debug("#promiseObserve", { didLoad: this.#didLoad });

    return new Promise(resolve => {
      let win = this.contentWindow;

      let observer = new win.MutationObserver(mutationList => {
        lazy.logConsole.debug(
          "#promiseObserve: Mutation observed",
          mutationList
        );

        let result = checkFn?.();
        if (result) {
          cleanup(result, observer);
        }
      });

      observer.observe(win.document.body, {
        attributes: true,
        subtree: true,
        childList: true,
      });

      let cleanup = (result, observer) => {
        lazy.logConsole.debug(
          "#promiseObserve cleanup",
          result,
          observer,
          this.#observerCleanUpTimer
        );
        if (observer) {
          observer.disconnect();
          observer = null;
        }

        if (this.#observerCleanUpTimer) {
          lazy.clearTimeout(this.#observerCleanUpTimer);
        }

        this.#observerCleanUp = null;
        resolve(result);
      };

      // The clean up function to clean unfinished observer and timer when the
      // actor destroys.
      this.#observerCleanUp = () => {
        cleanup(null, observer);
      };

      // If we already observed a load event we can start the cleanup timer
      // straight away.
      // Otherwise wait for the load event via the #onLoad method.
      if (this.#didLoad) {
        this.#startObserverCleanupTimer();
      }
    });
  }

  // Detecting if the banner is shown on the page.
  async #detectBanner() {
    if (!this.#clickRules?.length) {
      return [];
    }
    lazy.logConsole.debug("Starting to detect the banner");

    // Returns an array of rules for which a cookie banner exists for the
    // current site.
    let presenceDetector = () => {
      lazy.logConsole.debug("presenceDetector start");
      let matchingRules = this.#clickRules.filter(rule => {
        let { presence, skipPresenceVisibilityCheck } = rule;

        let banner = this.document.querySelector(presence);
        lazy.logConsole.debug("Testing banner el presence", {
          result: banner,
          rule,
          presence,
        });

        if (!banner) {
          return false;
        }
        if (skipPresenceVisibilityCheck) {
          return true;
        }

        let isVisible = this.#isVisible(banner);
        // Store visibility of banner element to keep track of why detection
        // failed.
        this.#telemetryStatus.bannerVisibilityFail = !isVisible;

        return isVisible;
      });

      // For no rules matched return null explicitly so #promiseObserve knows we
      // want to keep observing.
      if (!matchingRules.length) {
        return null;
      }
      return matchingRules;
    };

    lazy.logConsole.debug("Initial call to presenceDetector");
    let rules = presenceDetector();

    // If we couldn't detect the banner at the beginning, we register an
    // observer with the timeout to observe if the banner was shown within the
    // timeout.
    if (!rules?.length) {
      lazy.logConsole.debug(
        "Initial presenceDetector failed, registering MutationObserver",
        rules
      );
      this.#telemetryStatus.currentStage = "mutation_pre_load";
      rules = await this.#promiseObserve(presenceDetector, lazy.observeTimeout);
    }

    if (!rules?.length) {
      lazy.logConsole.debug("Couldn't detect the banner", rules);
      return [];
    }

    lazy.logConsole.debug("Detected the banner for rules", rules);

    return rules;
  }

  // Clicking the target button.
  async #clickTarget(rules) {
    lazy.logConsole.debug("Starting to detect the target button");

    let targetEl;
    for (let rule of rules) {
      targetEl = this.document.querySelector(rule.target);
      if (targetEl) {
        break;
      }
    }

    // The target button is not available. We register an observer to wait until
    // it's ready.
    if (!targetEl) {
      targetEl = await this.#promiseObserve(() => {
        for (let rule of rules) {
          let el = this.document.querySelector(rule.target);

          lazy.logConsole.debug("Testing button el presence", {
            result: el,
            rule,
            target: rule.target,
          });

          if (el) {
            lazy.logConsole.debug(
              "Found button from rule",
              rule,
              rule.target,
              el
            );
            return el;
          }
        }
        return null;
      }, lazy.observeTimeout);

      if (!targetEl) {
        lazy.logConsole.debug("Cannot find the target button.");
        return false;
      }
    }

    lazy.logConsole.debug("Found the target button, click it.", targetEl);
    targetEl.click();
    return true;
  }

  // The helper function to check if the given element if visible.
  #isVisible(element) {
    return element.checkVisibility({
      checkOpacity: true,
      checkVisibilityCSS: true,
    });
  }

  // The helper function to hide the banner. It will store the original display
  // value of the banner, so it can be used to show the banner later if needed.
  #hideBanner(rules) {
    if (this.#originalBannerDisplay) {
      // We've already hidden the banner.
      return null;
    }

    let banner;
    let rule;
    for (let r of rules) {
      banner = this.document.querySelector(r.hide);
      if (banner) {
        rule = r;
        break;
      }
    }
    // Failed to find banner el to hide.
    if (!banner) {
      lazy.logConsole.debug(
        "Failed to find banner element to hide from rules.",
        rules
      );
      return null;
    }

    lazy.logConsole.debug("Found banner element to hide from rules.", rules);

    this.#originalBannerDisplay = banner.style.display;

    // Change the display of the banner right before the style flush occurs to
    // avoid the unnecessary sync reflow.
    banner.ownerGlobal.requestAnimationFrame(() => {
      banner.style.display = "none";
    });

    return rule;
  }

  // The helper function to show the banner by reverting the display of the
  // banner to the original value.
  #showBanner({ hide }) {
    if (this.#originalBannerDisplay === null) {
      // We've never hidden the banner.
      return;
    }
    let banner = this.document.querySelector(hide);

    // Banner no longer present or destroyed or content window has been
    // destroyed.
    if (!banner || Cu.isDeadWrapper(banner) || !banner.ownerGlobal) {
      return;
    }

    let originalDisplay = this.#originalBannerDisplay;
    this.#originalBannerDisplay = null;

    // Change the display of the banner right before the style flush occurs to
    // avoid the unnecessary sync reflow.
    banner.ownerGlobal.requestAnimationFrame(() => {
      banner.style.display = originalDisplay;
    });
  }

  #maybeSendTestMessage() {
    if (lazy.testing) {
      let win = this.contentWindow;

      // Report the clicking is finished after the style has been flushed.
      win.requestAnimationFrame(() => {
        win.setTimeout(() => {
          this.sendAsyncMessage("CookieBanner::Test-FinishClicking");
        }, 0);
      });
    }
  }
}
