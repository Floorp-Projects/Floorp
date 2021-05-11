/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = [
  "_RemoteSettingsExperimentLoader",
  "RemoteSettingsExperimentLoader",
  "RemoteDefaultsLoader",
];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ASRouterTargeting: "resource://activity-stream/lib/ASRouterTargeting.jsm",
  TargetingContext: "resource://messaging-system/targeting/Targeting.jsm",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.jsm",
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

const COLLECTION_ID_PREF = "messaging-system.rsexperimentloader.collection_id";
const COLLECTION_ID_FALLBACK = "nimbus-desktop-experiments";
const COLLECTION_REMOTE_DEFAULTS = "nimbus-desktop-defaults";
const ENABLED_PREF = "messaging-system.rsexperimentloader.enabled";
const STUDIES_OPT_OUT_PREF = "app.shield.optoutstudies.enabled";

const TIMER_NAME = "rs-experiment-loader-timer";
const TIMER_LAST_UPDATE_PREF = `app.update.lastUpdateTime.${TIMER_NAME}`;
// Use the same update interval as normandy
const RUN_INTERVAL_PREF = "app.normandy.run_interval_seconds";
const NIMBUS_DEBUG_PREF = "nimbus.debug";

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "COLLECTION_ID",
  COLLECTION_ID_PREF,
  COLLECTION_ID_FALLBACK
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "NIMBUS_DEBUG",
  NIMBUS_DEBUG_PREF,
  false
);

/**
 * Responsible for pre-fetching remotely defined configurations from
 * Remote Settings.
 */
const RemoteDefaultsLoader = {
  async syncRemoteDefaults(reason) {
    log.debug("Fetching remote defaults for NimbusFeatures.");
    try {
      await this._onUpdatesReady(
        await this._remoteSettingsClient.get(),
        reason
      );
    } catch (e) {
      Cu.reportError(e);
    }
    log.debug("Finished fetching remote defaults.");
  },

  async _onUpdatesReady(remoteDefaults = [], reason = "unknown") {
    const matches = [];
    const existingConfigIds = ExperimentManager.store.getAllExistingRemoteConfigIds();

    if (remoteDefaults.length) {
      await ExperimentManager.store.ready();

      // Iterate over remote defaults: at most 1 per feature
      for (let remoteDefault of remoteDefaults) {
        if (!remoteDefault.configurations) {
          continue;
        }
        // Iterate over feature configurations and apply first which matches targeting
        for (let configuration of remoteDefault.configurations) {
          let result;
          try {
            result = await RemoteSettingsExperimentLoader.evaluateJexl(
              configuration.targeting,
              { activeRemoteDefaults: existingConfigIds }
            );
          } catch (e) {
            Cu.reportError(e);
          }
          if (result) {
            log.debug(
              `Setting remote defaults for feature: ${
                remoteDefault.id
              }: ${JSON.stringify(configuration)}`
            );

            matches.push(remoteDefault.id);

            const existing = ExperimentManager.store.getRemoteConfig(
              remoteDefault.id
            );

            ExperimentManager.store.updateRemoteConfigs(
              remoteDefault.id,
              configuration
            );

            // Update Telemetry environment. Note that we should always update during initialization,
            // but after that we don't need to.
            if (
              reason === "init" ||
              !existing ||
              existing.slug !== configuration.slug
            ) {
              ExperimentManager.setRemoteDefaultActive(
                remoteDefault.id,
                configuration.slug
              );
            }
            break;
          } else {
            log.debug(
              `Remote default config ${configuration.slug} for ${remoteDefault.id} did not match due to targeting`
            );
          }
        }
      }
    }

    // Remove any pre-existing configurations that weren't found
    for (const id of existingConfigIds) {
      if (!matches.includes(id)) {
        ExperimentManager.setRemoteDefaultInactive(id);
      }
    }

    // Do final cleanup
    ExperimentManager.store.finalizeRemoteConfigs(matches);
  },
};

XPCOMUtils.defineLazyGetter(RemoteDefaultsLoader, "_remoteSettingsClient", () =>
  RemoteSettings(COLLECTION_REMOTE_DEFAULTS)
);

class _RemoteSettingsExperimentLoader {
  constructor() {
    // Has the timer been set?
    this._initialized = false;
    // Are we in the middle of updating recipes already?
    this._updating = false;

    // Make it possible to override for testing
    this.manager = ExperimentManager;

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
      "studiesEnabled",
      STUDIES_OPT_OUT_PREF,
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
    if (this._initialized || !this.enabled || !this.studiesEnabled) {
      // Resolves any Promise waiting for Remote Settings data
      ExperimentManager.store.finalizeRemoteConfigs([]);
      return;
    }

    this.setTimer();
    CleanupManager.addCleanupHandler(() => this.uninit());
    this._initialized = true;

    await Promise.all([
      this.updateRecipes(),
      RemoteDefaultsLoader.syncRemoteDefaults("init"),
    ]);
  }

  uninit() {
    if (!this._initialized) {
      return;
    }
    timerManager.unregisterTimer(TIMER_NAME);
    this._initialized = false;
    this._updating = false;
  }

  async evaluateJexl(jexlString, customContext) {
    if (
      customContext &&
      !customContext.experiment &&
      !customContext.activeRemoteDefaults
    ) {
      throw new Error(
        "Expected an .experiment or .activeRemoteDefaults property in second param of this function"
      );
    }

    const context = TargetingContext.combineContexts(
      customContext,
      this.manager.createTargetingContext(),
      ASRouterTargeting.Environment
    );

    log.debug("Testing targeting expression:", jexlString);
    const targetingContext = new TargetingContext(context);

    let result = false;
    try {
      result = await targetingContext.evalWithDefault(jexlString);
    } catch (e) {
      log.debug("Targeting failed because of an error");
      Cu.reportError(e);
    }
    return result;
  }

  /**
   * Checks targeting of a recipe if it is defined
   * @param {Recipe} recipe
   * @param {{[key: string]: any}} customContext A custom filter context
   * @returns {Promise<boolean>} Should we process the recipe?
   */
  async checkTargeting(recipe) {
    if (!recipe.targeting) {
      log.debug("No targeting for recipe, so it matches automatically");
      return true;
    }

    const result = await this.evaluateJexl(recipe.targeting, {
      experiment: recipe,
    });

    return Boolean(result);
  }

  /**
   * Get all recipes from remote settings
   * @param {string} trigger What caused the update to occur?
   */
  async updateRecipes(trigger) {
    if (this._updating || !this._initialized) {
      return;
    }
    this._updating = true;

    log.debug("Updating recipes" + (trigger ? ` with trigger ${trigger}` : ""));

    let recipes;
    let loadingError = false;

    try {
      recipes = await this.remoteSettingsClient.get();
      log.debug(`Got ${recipes.length} recipes from Remote Settings`);
    } catch (e) {
      log.debug("Error getting recipes from remote settings.");
      loadingError = true;
      Cu.reportError(e);
    }

    let matches = 0;
    if (recipes && !loadingError) {
      for (const r of recipes) {
        if (await this.checkTargeting(r)) {
          matches++;
          log.debug(`${r.id} matched`);
          await this.manager.onRecipe(r, "rs-loader");
        } else {
          log.debug(`${r.id} did not match due to targeting`);
        }
      }

      log.debug(`${matches} recipes matched. Finalizing ExperimentManager.`);
      this.manager.onFinalize("rs-loader");
    }

    if (trigger !== "timer") {
      const lastUpdateTime = Math.round(Date.now() / 1000);
      Services.prefs.setIntPref(TIMER_LAST_UPDATE_PREF, lastUpdateTime);
    }

    this._updating = false;
  }

  async optInToExperiment({ slug, branch: branchSlug, collection }) {
    log.debug(`Attempting force enrollment with ${slug} / ${branchSlug}`);

    if (!NIMBUS_DEBUG) {
      log.debug(
        `Force enrollment only works when '${NIMBUS_DEBUG_PREF}' is enabled.`
      );
      // More generic error if no debug preference is on.
      throw new Error("Could not opt in.");
    }

    let recipes;
    try {
      recipes = await RemoteSettings(collection || COLLECTION_ID).get();
    } catch (e) {
      Cu.reportError(e);
      throw new Error("Error getting recipes from remote settings.");
    }

    let recipe = recipes.find(r => r.slug === slug);

    if (!recipe) {
      throw new Error(
        `Could not find experiment slug ${slug} in collection ${collection ||
          COLLECTION_ID}.`
      );
    }

    let branch = recipe.branches.find(b => b.slug === branchSlug);
    if (!branch) {
      throw new Error(`Could not find branch slug ${branchSlug} in ${slug}.`);
    }

    return ExperimentManager.forceEnroll(recipe, branch);
  }

  /**
   * Handles feature status based on feature pref and STUDIES_OPT_OUT_PREF.
   * Changing any of them to false will turn off any recipe fetching and
   * processing.
   */
  onEnabledPrefChange(prefName, oldValue, newValue) {
    if (this._initialized && !newValue) {
      this.uninit();
    } else if (!this._initialized && newValue && this.enabled) {
      // If the feature pref is turned on then turn on recipe processing.
      // If the opt in pref is turned on then turn on recipe processing only if
      // the feature pref is also enabled.
      this.init();
    }
  }

  /**
   * Sets a timer to update recipes every this.intervalInSeconds
   */
  setTimer() {
    // The callbacks will be called soon after the timer is registered
    timerManager.registerTimer(
      TIMER_NAME,
      () => {
        this.updateRecipes("timer");
        RemoteDefaultsLoader.syncRemoteDefaults("timer");
      },
      this.intervalInSeconds
    );
    log.debug("Registered update timer");
  }
}

const RemoteSettingsExperimentLoader = new _RemoteSettingsExperimentLoader();
