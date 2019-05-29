/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {LogManager} = ChromeUtils.import("resource://normandy/lib/LogManager.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "timerManager",
                                   "@mozilla.org/updates/timer-manager;1",
                                   "nsIUpdateTimerManager");

XPCOMUtils.defineLazyModuleGetters(this, {
  RemoteSettings: "resource://services-settings/remote-settings.js",
  FeatureGate: "resource://featuregates/FeatureGate.jsm",
  Storage: "resource://normandy/lib/Storage.jsm",
  FilterExpressions: "resource://gre/modules/components-utils/FilterExpressions.jsm",
  NormandyApi: "resource://normandy/lib/NormandyApi.jsm",
  ClientEnvironment: "resource://normandy/lib/ClientEnvironment.jsm",
  CleanupManager: "resource://normandy/lib/CleanupManager.jsm",
  Uptake: "resource://normandy/lib/Uptake.jsm",
  ActionsManager: "resource://normandy/lib/ActionsManager.jsm",
});

var EXPORTED_SYMBOLS = ["RecipeRunner"];

const log = LogManager.getLogger("recipe-runner");
const TIMER_NAME = "recipe-client-addon-run";
const REMOTE_SETTINGS_COLLECTION = "normandy-recipes";
const PREF_CHANGED_TOPIC = "nsPref:changed";

const TELEMETRY_ENABLED_PREF = "datareporting.healthreport.uploadEnabled";

const PREF_PREFIX = "app.normandy";
const RUN_INTERVAL_PREF = `${PREF_PREFIX}.run_interval_seconds`;
const FIRST_RUN_PREF = `${PREF_PREFIX}.first_run`;
const SHIELD_ENABLED_PREF = `${PREF_PREFIX}.enabled`;
const DEV_MODE_PREF = `${PREF_PREFIX}.dev_mode`;
const API_URL_PREF = `${PREF_PREFIX}.api_url`;
const LAZY_CLASSIFY_PREF = `${PREF_PREFIX}.experiments.lazy_classify`;

const PREFS_TO_WATCH = [
  RUN_INTERVAL_PREF,
  TELEMETRY_ENABLED_PREF,
  SHIELD_ENABLED_PREF,
  API_URL_PREF,
];

XPCOMUtils.defineLazyGetter(this, "gRemoteSettingsClient", () => {
  return RemoteSettings(REMOTE_SETTINGS_COLLECTION, {
    filterFunc: async entry => (await RecipeRunner.checkFilter(entry.recipe)) ? entry : null,
  });
});

XPCOMUtils.defineLazyGetter(this, "gRemoteSettingsGate", () => {
  return FeatureGate.fromId("normandy-remote-settings");
});

/**
 * cacheProxy returns an object Proxy that will memoize properties of the target.
 */
function cacheProxy(target) {
  const cache = new Map();
  return new Proxy(target, {
    get(target, prop, receiver) {
      if (!cache.has(prop)) {
        cache.set(prop, target[prop]);
      }
      return cache.get(prop);
    },
  });
}

var RecipeRunner = {
  async init() {
    this.enabled = null;
    this.checkPrefs(); // sets this.enabled
    this.watchPrefs();

    // Here "first run" means the first run this profile has ever done. This
    // preference is set to true at the end of this function, and never reset to
    // false.
    const firstRun = Services.prefs.getBoolPref(FIRST_RUN_PREF, true);

    // Dev mode is a mode used for development and QA that bypasses the normal
    // timer function of Normandy, to make testing more convenient.
    const devMode = Services.prefs.getBoolPref(DEV_MODE_PREF, false);

    if (this.enabled && (devMode || firstRun)) {
      // In dev mode, if remote settings is enabled, force an immediate sync
      // before running. This ensures that the latest data is used for testing.
      // This is not needed for the first run case, because remote settings
      // already handles empty collections well.
      if (devMode) {
        let remoteSettingsGate = await gRemoteSettingsGate;
        if (await remoteSettingsGate.isEnabled()) {
          await gRemoteSettingsClient.sync();
        }
      }
      await this.run();
    }

    // Update the firstRun pref, to indicate that Normandy has run at least once
    // on this profile.
    if (firstRun) {
      Services.prefs.setBoolPref(FIRST_RUN_PREF, false);
    }
  },

  enable() {
    if (this.enabled) {
      return;
    }
    this.registerTimer();
    this.enabled = true;
  },

  disable() {
    if (this.enabled) {
      this.unregisterTimer();
    }
    // this.enabled may be null, so always set it to false
    this.enabled = false;
  },

  /** Watch for prefs to change, and call this.observer when they do */
  watchPrefs() {
    for (const pref of PREFS_TO_WATCH) {
      Services.prefs.addObserver(pref, this);
    }

    CleanupManager.addCleanupHandler(this.unwatchPrefs.bind(this));
  },

  unwatchPrefs() {
    for (const pref of PREFS_TO_WATCH) {
      Services.prefs.removeObserver(pref, this);
    }
  },

  /** When prefs change, this is fired */
  observe(subject, topic, data) {
    switch (topic) {
      case PREF_CHANGED_TOPIC: {
        const prefName = data;

        switch (prefName) {
          case RUN_INTERVAL_PREF:
            this.updateRunInterval();
            break;

          // explicit fall-through
          case TELEMETRY_ENABLED_PREF:
          case SHIELD_ENABLED_PREF:
          case API_URL_PREF:
            this.checkPrefs();
            break;

          default:
            log.debug(`Observer fired with unexpected pref change: ${prefName}`);
        }

        break;
      }
    }
  },

  checkPrefs() {
    // Only run if Unified Telemetry is enabled.
    if (!Services.prefs.getBoolPref(TELEMETRY_ENABLED_PREF)) {
      log.debug("Disabling RecipeRunner because Unified Telemetry is disabled.");
      this.disable();
      return;
    }

    if (!Services.prefs.getBoolPref(SHIELD_ENABLED_PREF)) {
      log.debug(`Disabling Shield because ${SHIELD_ENABLED_PREF} is set to false`);
      this.disable();
      return;
    }

    if (!Services.policies.isAllowed("Shield")) {
      log.debug("Disabling Shield because it's blocked by policy.");
      this.disable();
      return;
    }

    const apiUrl = Services.prefs.getCharPref(API_URL_PREF);
    if (!apiUrl) {
      log.warn(`Disabling Shield because ${API_URL_PREF} is not set.`);
      this.disable();
      return;
    }
    if (!apiUrl.startsWith("https://")) {
      log.warn(`Disabling Shield because ${API_URL_PREF} is not an HTTPS url: ${apiUrl}.`);
      this.disable();
      return;
    }

    log.debug(`Enabling Shield`);
    this.enable();
  },

  registerTimer() {
    this.updateRunInterval();
    CleanupManager.addCleanupHandler(() => timerManager.unregisterTimer(TIMER_NAME));
  },

  unregisterTimer() {
    timerManager.unregisterTimer(TIMER_NAME);
  },

  updateRunInterval() {
    // Run once every `runInterval` wall-clock seconds. This is managed by setting a "last ran"
    // timestamp, and running if it is more than `runInterval` seconds ago. Even with very short
    // intervals, the timer will only fire at most once every few minutes.
    const runInterval = Services.prefs.getIntPref(RUN_INTERVAL_PREF);
    timerManager.registerTimer(TIMER_NAME, () => this.run(), runInterval);
  },

  async run() {
    this.clearCaches();
    // Unless lazy classification is enabled, prep the classify cache.
    if (!Services.prefs.getBoolPref(LAZY_CLASSIFY_PREF, false)) {
      try {
        await ClientEnvironment.getClientClassification();
      } catch (err) {
        // Try to go on without this data; the filter expressions will
        // gracefully fail without this info if they need it.
      }
    }

    // Fetch recipes before execution in case we fail and exit early.
    let recipesToRun;
    try {
      recipesToRun = await this.loadRecipes();
    } catch (e) {
      // Either we failed at fetching the recipes from server (legacy),
      // or the recipes signature verification failed.
      let status = Uptake.RUNNER_SERVER_ERROR;
      if (/NetworkError/.test(e)) {
        status = Uptake.RUNNER_NETWORK_ERROR;
      } else if (e instanceof NormandyApi.InvalidSignatureError) {
        status = Uptake.RUNNER_INVALID_SIGNATURE;
      }
      await Uptake.reportRunner(status);
      return;
    }

    const actions = new ActionsManager();

    // Execute recipes, if we have any.
    if (recipesToRun.length === 0) {
      log.debug("No recipes to execute");
    } else {
      for (const recipe of recipesToRun) {
        await actions.runRecipe(recipe);
      }
    }

    await actions.finalize();

    await Uptake.reportRunner(Uptake.RUNNER_SUCCESS);
  },

  /**
   * Return the list of recipes to run, filtered for the current environment.
   */
  async loadRecipes() {
    // If RemoteSettings is enabled, we read the list of recipes from there.
    // The JEXL filtering is done via the provided callback (see `gRemoteSettingsClient`).
    if (await FeatureGate.isEnabled("normandy-remote-settings")) {
      // First, fetch recipes whose JEXL filters match.
      const entries = await gRemoteSettingsClient.get();
      // Then, verify the signature of each recipe. It will throw if invalid.
      return Promise.all(entries.map(async ( { recipe, signature } ) => {
        await NormandyApi.verifyObjectSignature(recipe, signature, "recipe");
        return recipe;
      }));
    }

    // Obtain the recipes from the Normandy server (legacy).
    let recipes;
    try {
      recipes = await NormandyApi.fetchRecipes({enabled: true});
      log.debug(
        `Fetched ${recipes.length} recipes from the server: ` +
        recipes.map(r => r.name).join(", ")
      );
    } catch (e) {
      const apiUrl = Services.prefs.getCharPref(API_URL_PREF);
      log.error(`Could not fetch recipes from ${apiUrl}: "${e}"`);
      throw e;
    }
    // Evaluate recipe filters
    const recipesToRun = [];
    for (const recipe of recipes) {
      if (await this.checkFilter(recipe)) {
        recipesToRun.push(recipe);
      }
    }
    return recipesToRun;
  },

  getFilterContext(recipe) {
    const environment = cacheProxy(ClientEnvironment);
    environment.recipe = {
      id: recipe.id,
      arguments: recipe.arguments,
    };
    return {
      env: environment,
      // Backwards compatibility -- see bug 1477255.
      normandy: environment,
    };
  },

  /**
   * Evaluate a recipe's filter expression against the environment.
   * @param {object} recipe
   * @param {string} recipe.filter The expression to evaluate against the environment.
   * @return {boolean} The result of evaluating the filter, cast to a bool, or false
   *                   if an error occurred during evaluation.
   */
  async checkFilter(recipe) {
    const context = this.getFilterContext(recipe);
    let result;
    try {
      result = await FilterExpressions.eval(recipe.filter_expression, context);
    } catch (err) {
      log.error(`Error checking filter for "${recipe.name}". Filter: [${recipe.filter_expression}]. Error: "${err}"`);
      await Uptake.reportRecipe(recipe, Uptake.RECIPE_FILTER_BROKEN);
      return false;
    }

    if (!result) {
      // This represents a terminal state for the given recipe, so
      // report its outcome. Others are reported when executed in
      // ActionsManager.
      await Uptake.reportRecipe(recipe, Uptake.RECIPE_DIDNT_MATCH_FILTER);
      return false;
    }

    return true;
  },

  /**
   * Clear all caches of systems used by RecipeRunner, in preparation
   * for a clean run.
   */
  clearCaches() {
    ClientEnvironment.clearClassifyCache();
    NormandyApi.clearIndexCache();
  },

  /**
   * Clear out cached state and fetch/execute recipes from the given
   * API url. This is used mainly by the mock-recipe-server JS that is
   * executed in the browser console.
   */
  async testRun(baseApiUrl) {
    const oldApiUrl = Services.prefs.getCharPref(API_URL_PREF);
    Services.prefs.setCharPref(API_URL_PREF, baseApiUrl);

    try {
      Storage.clearAllStorage();
      this.clearCaches();
      await this.run();
    } finally {
      Services.prefs.setCharPref(API_URL_PREF, oldApiUrl);
      this.clearCaches();
    }
  },

  /**
   * Offer a mechanism to get access to the lazily-instantiated
   * gRemoteSettingsClient, because if someone instantiates it
   * themselves, it won't have the options we provided in this module,
   * and it will prevent instantiation by this module later.
   *
   * This is only meant to be used in testing, where it is a
   * convenient hook to store data in the underlying remote-settings
   * collection.
   */
  get _remoteSettingsClientForTesting() {
    return gRemoteSettingsClient;
  },
};
