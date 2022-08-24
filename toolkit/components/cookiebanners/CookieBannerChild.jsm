/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["CookieBannerChild"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

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

XPCOMUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "CookieBannerChild",
    maxLogLevelPref: "cookiebanners.bannerClicking.logLevel",
  });
});

class CookieBannerChild extends JSWindowActorChild {
  #clickRule;
  #originalBannerDisplay = null;
  #observerCleanUp;

  async handleEvent(event) {
    if (event.type != "DOMContentLoaded") {
      return;
    }

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

    lazy.logConsole.debug(
      `Send message to get rule for ${principal.baseDomain}`
    );
    let rule;

    try {
      rule = await this.sendQuery("CookieBanner::GetClickRule", {});
    } catch (e) {
      lazy.logConsole.warn("Failed to get click rule from parent.");
    }

    lazy.logConsole.debug("Got rule:", rule);
    // We can stop here if we don't have a rule or the target button to click.
    if (!rule?.target) {
      this.#maybeSendTestMessage();
      return;
    }

    this.#clickRule = rule;

    await this.handleCookieBanner();

    this.#maybeSendTestMessage();
  }

  didDestroy() {
    // Clean up the observer and timer if needed.
    this.#observerCleanUp?.();
  }

  /**
   * The function to perform the core logic of handing the cookie banner. It
   * will detect the banner and click the banner button whenever possible
   * according to the given click rule.
   *
   * @returns A promise which resolves when it finishes auto clicking.
   */
  async handleCookieBanner() {
    // First, we detect if the banner is shown on the page
    let bannerFound = await this.#detectBanner();

    if (!bannerFound) {
      // The banner was never shown.
      return;
    }

    // Hide the banner.
    this.#hideBanner();

    let successClick = false;
    try {
      successClick = await this.#clickTarget();
    } finally {
      if (!successClick) {
        // We cannot successfully click the target button. Show the banner on
        // the page so that user can interact with the banner.
        this.#showBanner();
      }
    }
  }

  /**
   * The helper function to observe the changes on the document with a timeout.
   * It will call the check function when it observes mutations on the document
   * body. Once the check function returns true, it will resolve with true.
   * Otherwise, it will resolve to false when it times out.
   *
   * @param {function} [checkFn] The check function.
   * @param {Number} timeout The time out of the observer.
   * @returns If the check function returns true before time out.
   */
  #promiseObserve(checkFn, timeout) {
    if (this.#observerCleanUp) {
      throw new Error(
        "The promiseObserve is called before previous one resolves."
      );
    }

    return new Promise(resolve => {
      let win = this.contentWindow;
      let timer;

      let observer = new win.MutationObserver(() => {
        if (checkFn?.()) {
          cleanup(true, observer, timer);
        }
      });

      timer = lazy.setTimeout(() => {
        cleanup(false, observer);
      }, timeout);

      observer.observe(win.document.body, {
        attributes: true,
        subtree: true,
        childList: true,
      });

      let cleanup = (result, observer, timer) => {
        if (observer) {
          observer.disconnect();
          observer = null;
        }

        if (timer) {
          lazy.clearTimeout(timer);
        }

        this.#observerCleanUp = null;
        resolve(result);
      };

      // The clean up function to clean unfinished observer and timer when the
      // actor destroys.
      this.#observerCleanUp = () => {
        cleanup(false, observer, timer);
      };
    });
  }

  // Detecting if the banner is shown on the page.
  async #detectBanner() {
    if (!this.#clickRule) {
      return false;
    }
    lazy.logConsole.debug("Starting to detect the banner");

    let detector = () => {
      let banner = this.document.querySelector(this.#clickRule.presence);

      return banner && this.#isVisible(banner);
    };

    let found = detector();

    // If we couldn't detect the banner at the beginning, we register an
    // observer with the timeout to observe if the banner was shown within the
    // timeout.
    if (
      !found &&
      !(await this.#promiseObserve(detector, lazy.observeTimeout))
    ) {
      lazy.logConsole.debug("Couldn't detect the banner");
      return false;
    }

    lazy.logConsole.debug("Detected the banner");

    return true;
  }

  // Clicking the target button.
  async #clickTarget() {
    if (!this.#clickRule) {
      return false;
    }
    lazy.logConsole.debug("Starting to detect the target button");

    let target = this.document.querySelector(this.#clickRule.target);

    // The target button is not available. We register an observer to wait until
    // it's ready.
    if (!target) {
      await this.#promiseObserve(() => {
        target = this.document.querySelector(this.#clickRule.target);

        return !!target;
      }, lazy.observeTimeout);

      if (!target) {
        lazy.logConsole.debug("Cannot find the target button.");
        return false;
      }
    }

    lazy.logConsole.debug("Found the target button, click it.");
    target.click();
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
  #hideBanner() {
    let banner = this.document.querySelector(this.#clickRule.hide);

    if (this.#originalBannerDisplay) {
      // We've hidden the banner.
      return;
    }
    this.#originalBannerDisplay = banner.style.display;

    // Change the display of the banner right before the style flush occurs to
    // avoid the unnecessary sync reflow.
    banner.ownerGlobal.requestAnimationFrame(() => {
      banner.style.display = "none";
    });
  }

  // The helper function to show the banner by reverting the display of the
  // banner to the original value.
  #showBanner() {
    if (this.#originalBannerDisplay === null) {
      // We've never hidden the banner.
      return;
    }
    let banner = this.document.querySelector(this.#clickRule.hide);

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
