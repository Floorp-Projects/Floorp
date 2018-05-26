/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://normandy/lib/LogManager.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "timerManager",
                                   "@mozilla.org/updates/timer-manager;1",
                                   "nsIUpdateTimerManager");

XPCOMUtils.defineLazyModuleGetters(this, {
  Storage: "resource://normandy/lib/Storage.jsm",
  FilterExpressions: "resource://gre/modules/components-utils/FilterExpressions.jsm",
  NormandyApi: "resource://normandy/lib/NormandyApi.jsm",
  ClientEnvironment: "resource://normandy/lib/ClientEnvironment.jsm",
  CleanupManager: "resource://normandy/lib/CleanupManager.jsm",
  AddonStudies: "resource://normandy/lib/AddonStudies.jsm",
  Uptake: "resource://normandy/lib/Uptake.jsm",
  ActionsManager: "resource://normandy/lib/ActionsManager.jsm",
});

var EXPORTED_SYMBOLS = ["RecipeRunner"];

const log = LogManager.getLogger("recipe-runner");
const TIMER_NAME = "recipe-client-addon-run";
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
    }
  });
}

var RecipeRunner = {
  async init() {
    this.enabled = null;
    this.checkPrefs(); // sets this.enabled
    this.watchPrefs();

    // Run if enabled immediately on first run, or if dev mode is enabled.
    const firstRun = Services.prefs.getBoolPref(FIRST_RUN_PREF, true);
    const devMode = Services.prefs.getBoolPref(DEV_MODE_PREF, false);

    if (this.enabled && (devMode || firstRun)) {
      await this.run();
    }
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
    if (!apiUrl || !apiUrl.startsWith("https://")) {
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

      let status = Uptake.RUNNER_SERVER_ERROR;
      if (/NetworkError/.test(e)) {
        status = Uptake.RUNNER_NETWORK_ERROR;
      } else if (e instanceof NormandyApi.InvalidSignatureError) {
        status = Uptake.RUNNER_INVALID_SIGNATURE;
      }
      Uptake.reportRunner(status);
      return;
    }

    const actions = new ActionsManager();
    await actions.fetchRemoteActions();
    await actions.preExecution();

    // Evaluate recipe filters
    const recipesToRun = [];
    for (const recipe of recipes) {
      if (await this.checkFilter(recipe)) {
        recipesToRun.push(recipe);
      }
    }

    // Execute recipes, if we have any.
    if (recipesToRun.length === 0) {
      log.debug("No recipes to execute");
    } else {
      for (const recipe of recipesToRun) {
        await actions.runRecipe(recipe);
      }
    }

    await actions.finalize();

    // Close storage connections
    await AddonStudies.close();

    Uptake.reportRunner(Uptake.RUNNER_SUCCESS);
  },

  getFilterContext(recipe) {
    const environment = cacheProxy(ClientEnvironment);
    environment.recipe = {
      id: recipe.id,
      arguments: recipe.arguments,
    };
    return {
      normandy: environment
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
    try {
      const result = await FilterExpressions.eval(recipe.filter_expression, context);
      return !!result;
    } catch (err) {
      log.error(`Error checking filter for "${recipe.name}". Filter: [${recipe.filter_expression}]. Error: "${err}"`);
      return false;
    }
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
};
