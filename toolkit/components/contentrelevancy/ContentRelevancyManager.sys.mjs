/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  getTopFrecentUrls:
    "resource://gre/modules/contentrelevancy/private/InputUtils.sys.mjs",
  getMostRecentUrls:
    "resource://gre/modules/contentrelevancy/private/InputUtils.sys.mjs",
  RelevancyStore: "resource://gre/modules/RustRelevancy.sys.mjs",
});

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

// Default total URLs to fetch from Places.
const DEFAULT_TOTAL_URLS = 100;

// File name of the relevancy database
const RELEVANCY_STORE_FILENAME = "content-relevancy.sqlite";

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
  async init() {
    if (this.initialized) {
      return;
    }

    lazy.log.info("Initializing the manager");

    Services.prefs.addObserver(PREF_CONTENT_RELEVANCY_ENABLED, this);
    if (this.shouldEnable) {
      await this.#enable();
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

  get #storePath() {
    return PathUtils.join(
      Services.dirsvc.get("ProfLD", Ci.nsIFile).path,
      RELEVANCY_STORE_FILENAME
    );
  }

  async #enable() {
    // Init the relevancy store.
    const path = this.#storePath;
    lazy.log.info(`Initializing RelevancyStore: ${path}`);

    try {
      this.#_store = await lazy.RelevancyStore.init(path);
    } catch (error) {
      lazy.log.error(`Error initializing RelevancyStore: ${error}`);
      return;
    }

    this.#startUpTimer();
  }

  /**
   * The reciprocal of `#enable()`, ensure this is safe to call when you add
   * new disabling code here. It should be so even if `#enable()` hasn't been
   * called.
   */
  #disable() {
    this.#_store = null;
    lazy.timerManager.unregisterTimer(TIMER_ID);
  }

  async #toggleFeature() {
    if (this.shouldEnable) {
      await this.#enable();
    } else {
      this.#disable();
    }
  }

  /**
   * nsITimerCallback
   */
  notify() {
    lazy.log.info("Background job timer fired");
    this.#doClassification();
  }

  get isInProgress() {
    return this.#isInProgress;
  }

  /**
   * Perform classification based on browsing history.
   *
   * It will fetch up to `DEFAULT_TOTAL_URLS` (could be fewer) URLs from
   * top frecent URLs and use most recent URLs as a fallback if the former is
   * insufficient.
   */
  async #doClassification() {
    if (this.isInProgress) {
      lazy.log.info(
        "Another classification is in progress, aborting interest classification"
      );
      return;
    }

    // Set a flag indicating this classification. Ensure it's cleared upon early
    // exit points & success.
    this.#isInProgress = true;

    try {
      lazy.log.info("Fetching input data for interest classification");
      let urls = await lazy.getTopFrecentUrls(DEFAULT_TOTAL_URLS);
      if (urls.length < DEFAULT_TOTAL_URLS) {
        const recentUrls = await lazy.getMostRecentUrls(
          DEFAULT_TOTAL_URLS - urls.length
        );
        urls = [...urls, ...recentUrls];
      }

      lazy.log.info("Starting interest classification");
      await this.#doClassificationHelper(urls);
    } catch (error) {
      if (error instanceof StoreNotAvailableError) {
        lazy.log.error("#store became null, aborting interest classification");
      } else {
        lazy.log.error("Classification error: " + (error.reason ?? error));
      }
    } finally {
      this.#isInProgress = false;
    }

    lazy.log.info("Finished interest classification");
  }

  /**
   * Classification helper. Use the getter `this.#store` rather than `#_store`
   * to access the store so that when it becomes null, a `StoreNotAvailableError`
   * will be raised. Likewise, other store related errors should be propagated
   * to the caller if you want to perform custom error handling in this helper.
   *
   * @param {Array} urls
   *   An array of URLs.
   * @throws {StoreNotAvailableError}
   *   Thrown when the store became unavailable (i.e. set to null elsewhere).
   * @throws {RelevancyAPIError}
   *   Thrown for other API errors on the store.
   */
  async #doClassificationHelper(urls) {
    // The following logs are unnecessary, only used to suppress the linting error.
    // TODO(nanj): delete me once the following TODO is done.
    if (!this.#store) {
      lazy.log.error("#store became null, aborting interest classification");
    }
    lazy.log.info("Classification input: " + urls);

    // TODO(nanj): uncomment the following once `ingest()` is implemented.
    // await this.#store.ingest(urls);
  }

  /**
   * Exposed for testing.
   */
  async _test_doClassification(urls) {
    await this.#doClassificationHelper(urls);
  }

  /**
   * Internal getter for `#_store` used by for classification. It will throw
   * a `StoreNotAvailableError` is the store is not ready.
   */
  get #store() {
    if (!this._isStoreReady) {
      throw new StoreNotAvailableError("Store is not available");
    }

    return this.#_store;
  }

  /**
   * Whether or not the store is ready (i.e. not null).
   */
  get _isStoreReady() {
    return !!this.#_store;
  }

  /**
   * nsIObserver
   */
  async observe(subj, topic, data) {
    switch (topic) {
      case "nsPref:changed":
        if (data === PREF_CONTENT_RELEVANCY_ENABLED) {
          await this.#toggleFeature();
        }
        break;
    }
  }

  // The `RustRelevancy` store.
  #_store;

  // Whether or not the module is initialized.
  #initialized = false;

  // Whether or not there is an in-progress classification. Used to prevent
  // duplicate classification tasks.
  #isInProgress = false;
}

/**
 * Error raised when attempting to access a null store.
 */
class StoreNotAvailableError extends Error {
  constructor(message, ...params) {
    super(message, ...params);
    this.name = "StoreNotAvailableError";
  }
}

export var ContentRelevancyManager = new RelevancyManager();
