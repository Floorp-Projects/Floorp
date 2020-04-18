/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = [
  "_RemoteSettingsExperimentLoader",
  "RemoteSettingsExperimentLoader",
];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ASRouterTargeting: "resource://activity-stream/lib/ASRouterTargeting.jsm",
  ExperimentManager:
    "resource://messaging-system/experiments/ExperimentManager.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  CleanupManager: "resource://normandy/lib/CleanupManager.jsm",
});

XPCOMUtils.defineLazyGetter(this, "log", () => {
  const { Logger } = ChromeUtils.import(
    "resource://messaging-system/lib/Logger.jsm"
  );
  return new Logger("RSLoader");
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "timerManager",
  "@mozilla.org/updates/timer-manager;1",
  "nsIUpdateTimerManager"
);

const COLLECTION_ID = "messaging-experiments";
const ENABLED_PREF = "messaging-system.rsexperimentloader.enabled";

const TIMER_NAME = "rs-experiment-loader-timer";
const TIMER_LAST_UPDATE_PREF = `app.update.lastUpdateTime.${TIMER_NAME}`;
// Use the same update interval as normandy
const RUN_INTERVAL_PREF = "app.normandy.run_interval_seconds";

class _RemoteSettingsExperimentLoader {
  constructor() {
    // Has the timer been set?
    this._initialized = false;
    // Are we in the middle of updating recipes already?
    this._updating = false;

    XPCOMUtils.defineLazyGetter(this, "remoteSettingsClient", () => {
      return RemoteSettings(COLLECTION_ID);
    });

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "enabled",
      ENABLED_PREF,
      false,
      this.onEnabledPrefChange.bind(this)
    );

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "intervalInSeconds",
      RUN_INTERVAL_PREF,
      21600,
      () => this.setTimer()
    );
  }

  async init() {
    if (this._initialized || !this.enabled) {
      return;
    }

    this.setTimer();
    CleanupManager.addCleanupHandler(() => this.uninit());
    this._initialized = true;

    await this.updateRecipes();
  }

  uninit() {
    if (!this._initialized) {
      return;
    }
    timerManager.unregisterTimer(TIMER_NAME);
    this._initialized = false;
  }

  async updateRecipes(trigger) {
    if (this._updating || !this._initialized) {
      return;
    }
    this._updating = true;

    log.debug("Updating recipes");

    let recipes;
    let loadingError = false;

    try {
      recipes = await this.remoteSettingsClient.get();
    } catch (e) {
      loadingError = true;
      Cu.reportError(e);
    }

    if (recipes && !loadingError) {
      log.debug("Updating ExperimentManager with new recipes");
      for (const r of recipes) {
        if (
          await ASRouterTargeting.isMatch(
            r.filter_expression,
            ExperimentManager.filterContext,
            err => {
              log.debug("Targeting failed because of an error");
              Cu.reportError(err);
            }
          )
        ) {
          log.debug(`${r.id} passed filter_expression`);
          await ExperimentManager.onRecipe(r.arguments, "rs-loader");
        } else {
          log.debug(`${r.id} failed filter_expression`);
        }
      }

      ExperimentManager.onFinalize("rs-loader");
    }

    if (trigger !== "timer") {
      const lastUpdateTime = Math.round(Date.now() / 1000);
      Services.prefs.setIntPref(TIMER_LAST_UPDATE_PREF, lastUpdateTime);
    }

    this._updating = false;
  }

  onEnabledPrefChange(prefName, oldValue, newValue) {
    if (this._initialized && !newValue) {
      this.uninit();
    } else if (!this._initialized && newValue) {
      this.init();
    }
  }

  setTimer() {
    // When this function is called, updateRecipes is called immediately
    // and then every this.intervalInSeconds
    timerManager.registerTimer(
      TIMER_NAME,
      () => this.updateRecipes("timer"),
      this.intervalInSeconds
    );
    log.debug("Registered update timer");
  }
}

const RemoteSettingsExperimentLoader = new _RemoteSettingsExperimentLoader();
