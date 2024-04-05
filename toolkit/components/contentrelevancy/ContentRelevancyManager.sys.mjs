/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "timerManager",
  "@mozilla.org/updates/timer-manager;1",
  "nsIUpdateTimerManager"
);

const PREF_CONTENT_RELEVANCY_ENABLED = "toolkit.contentRelevancy.enabled";

// Constants used by `nsIUpdateTimerManager` for a cross-session timer.
const TIMER_ID = "content-relevancy-timer";
const PREF_TIMER_LAST_UPDATE = `app.update.lastUpdateTime.${TIMER_ID}`;
const PREF_TIMER_INTERVAL = "toolkit.contentRelevancy.timerInterval";
// Set the timer interval to 1 day for validation.
const DEFAULT_TIMER_INTERVAL_SECONDS = 1 * 24 * 60 * 60;

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  return console.createInstance({
    prefix: "ContentRelevancyManager",
    maxLogLevel: Services.prefs.getBoolPref(
      "toolkit.contentRelevancy.log",
      false
    )
      ? "Debug"
      : "Error",
  });
});

class RelevancyManager {
  get initialized() {
    return this.#initialized;
  }

  /**
   * Init the manager. An update timer is registered if the feature is enabled.
   * The pref observer is always set so we can toggle the feature without restarting
   * the browser.
   *
   * Note that this should be called once only. `#enable` and `#disable` can be
   * used to toggle the feature once the manager is initialized.
   */
  init() {
    if (this.initialized) {
      return;
    }

    lazy.log.info("Initializing the manager");

    Services.prefs.addObserver(PREF_CONTENT_RELEVANCY_ENABLED, this);
    if (this.shouldEnable) {
      this.#enable();
    }

    this.#initialized = true;
  }

  uninit() {
    if (!this.initialized) {
      return;
    }

    lazy.log.info("Uninitializing the manager");

    Services.prefs.removeObserver(PREF_CONTENT_RELEVANCY_ENABLED, this);
    this.#disable();

    this.#initialized = false;
  }

  /**
   * Determine whether the feature should be enabled based on prefs and Nimbus.
   */
  get shouldEnable() {
    return Services.prefs.getBoolPref(PREF_CONTENT_RELEVANCY_ENABLED, false);
  }

  #startUpTimer() {
    // Log the last timer tick for debugging.
    const lastTick = Services.prefs.getIntPref(PREF_TIMER_LAST_UPDATE, 0);
    if (lastTick) {
      lazy.log.debug(
        `Last timer tick: ${lastTick}s (${
          Math.round(Date.now() / 1000) - lastTick
        })s ago`
      );
    } else {
      lazy.log.debug("Last timer tick: none");
    }

    const interval = Services.prefs.getIntPref(
      PREF_TIMER_INTERVAL,
      DEFAULT_TIMER_INTERVAL_SECONDS
    );
    lazy.timerManager.registerTimer(
      TIMER_ID,
      this,
      interval,
      interval != 0 // Do not skip the first timer tick for a zero interval for testing
    );
  }

  #enable() {
    this.#startUpTimer();
  }

  /**
   * The reciprocal of `#enable()`, ensure this is safe to call when you add
   * new disabling code here. It should be so even if `#enable()` hasn't been
   * called.
   */
  #disable() {
    lazy.timerManager.unregisterTimer(TIMER_ID);
  }

  #toggleFeature() {
    if (this.shouldEnable) {
      this.#enable();
    } else {
      this.#disable();
    }
  }

  /**
   * nsITimerCallback
   */
  notify() {
    lazy.log.info("Background job timer fired");
    // TODO(nanj): Wire up the relevancy Rust component here.
  }

  /**
   * nsIObserver
   */
  observe(subj, topic, data) {
    switch (topic) {
      case "nsPref:changed":
        if (data === PREF_CONTENT_RELEVANCY_ENABLED) {
          this.#toggleFeature();
        }
        break;
    }
  }

  // Whether or not the module is initialized.
  #initialized = false;
}

export var ContentRelevancyManager = new RelevancyManager();
