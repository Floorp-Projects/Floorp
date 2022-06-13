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

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);
XPCOMUtils.defineLazyModuleGetters(this, {
  ASRouterTargeting: "resource://activity-stream/lib/ASRouterTargeting.jsm",
  TargetingContext: "resource://messaging-system/targeting/Targeting.jsm",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  CleanupManager: "resource://normandy/lib/CleanupManager.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  JsonSchema: "resource://gre/modules/JsonSchema.jsm",
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

const SCHEMAS = {
  get NimbusExperiment() {
    return fetch("resource://nimbus/schemas/NimbusExperiment.schema.json", {
      credentials: "omit",
    })
      .then(rsp => rsp.json())
      .then(json => json.definitions.NimbusExperiment);
  },
};

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
    this._updating = false;
  }

  async evaluateJexl(jexlString, customContext) {
    if (customContext && !customContext.experiment) {
      throw new Error(
        "Expected an .experiment property in second param of this function"
      );
    }

    if (!customContext.source) {
      throw new Error(
        "Expected a .source property that identifies which targeting expression is being evaluated."
      );
    }

    const context = TargetingContext.combineContexts(
      customContext,
      this.manager.createTargetingContext(),
      ASRouterTargeting.Environment
    );

    log.debug("Testing targeting expression:", jexlString);
    const targetingContext = new TargetingContext(context, {
      source: customContext.source,
    });

    let result = null;
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
      source: recipe.slug,
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

    const recipeValidator = new JsonSchema.Validator(
      await SCHEMAS.NimbusExperiment
    );

    let matches = 0;
    let recipeMismatches = [];
    let invalidRecipes = [];
    let invalidBranches = [];
    let validatorCache = {};

    if (recipes && !loadingError) {
      for (const r of recipes) {
        let validation = recipeValidator.validate(r);
        if (!validation.valid) {
          Cu.reportError(
            `Could not validate experiment recipe ${r.id}: ${JSON.stringify(
              validation.errors,
              undefined,
              2
            )}`
          );
          invalidRecipes.push(r.slug);
          continue;
        }

        let type = r.isRollout ? "rollout" : "experiment";

        if (!(await this._validateBranches(r, validatorCache))) {
          invalidBranches.push(r.slug);
          log.debug(`${r.id} did not validate`);
          continue;
        }

        if (await this.checkTargeting(r)) {
          matches++;
          log.debug(`[${type}] ${r.id} matched`);
          await this.manager.onRecipe(r, "rs-loader");
        } else {
          log.debug(`${r.id} did not match due to targeting`);
          recipeMismatches.push(r.slug);
        }
      }

      log.debug(`${matches} recipes matched. Finalizing ExperimentManager.`);
      this.manager.onFinalize("rs-loader", {
        recipeMismatches,
        invalidRecipes,
        invalidBranches,
      });
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
    if (this.intervalInSeconds === 0) {
      // Used in tests where we want to turn this mechanism off
      return;
    }
    // The callbacks will be called soon after the timer is registered
    timerManager.registerTimer(
      TIMER_NAME,
      () => this.updateRecipes("timer"),
      this.intervalInSeconds
    );
    log.debug("Registered update timer");
  }

  /**
   * Validate the branches of an experiment using schemas
   *
   * @param recipe The recipe object.
   * @param validatorCache A cache of JSON Schema validators keyed by feature
   *                       ID.
   *
   * @returns Whether or not the branches pass validation.
   */
  async _validateBranches({ id, branches }, validatorCache = {}) {
    for (const [branchIdx, branch] of branches.entries()) {
      const features = branch.features ?? [branch.feature];
      for (const feature of features) {
        const { featureId, value } = feature;
        if (!NimbusFeatures[featureId]) {
          Cu.reportError(
            `Experiment ${id} has unknown featureId: ${featureId}`
          );
          return false;
        }

        let validator;
        if (validatorCache[featureId]) {
          validator = validatorCache[featureId];
        } else if (NimbusFeatures[featureId].manifest.schema?.uri) {
          const uri = NimbusFeatures[featureId].manifest.schema.uri;
          try {
            const schema = await fetch(uri, { credentials: "omit" }).then(rsp =>
              rsp.json()
            );
            validator = validatorCache[featureId] = new JsonSchema.Validator(
              schema
            );
          } catch (e) {
            throw new Error(
              `Could not fetch schema for feature ${featureId} at "${uri}": ${e}`
            );
          }
        } else {
          const schema = this._generateVariablesOnlySchema(
            featureId,
            NimbusFeatures[featureId].manifest
          );
          validator = validatorCache[featureId] = new JsonSchema.Validator(
            schema
          );
        }

        if (feature.enabled ?? true) {
          const result = validator.validate(value);
          if (!result.valid) {
            Cu.reportError(
              `Experiment ${id} branch ${branchIdx} feature ${featureId} does not validate: ${JSON.stringify(
                result.errors,
                undefined,
                2
              )}`
            );
            return false;
          }
        } else {
          log.debug(
            `Experiment ${id} branch ${branchIdx} feature ${featureId} disabled; skipping validation`
          );
        }
      }
    }

    return true;
  }

  _generateVariablesOnlySchema(featureId, manifest) {
    // See-also: https://github.com/mozilla/experimenter/blob/main/app/experimenter/features/__init__.py#L21-L64
    const schema = {
      $schema: "https://json-schema.org/draft/2019-09/schema",
      title: featureId,
      description: manifest.description,
      type: "object",
      properties: {},
      additionalProperties: true,
    };

    for (const [varName, desc] of Object.entries(manifest.variables)) {
      const prop = {};
      switch (desc.type) {
        case "boolean":
        case "int":
        case "string":
          prop.type = desc.type;
          break;

        case "json":
          // NB: Experimenter presently ignores the json type, it will still be
          // allowed under additionalProperties.
          continue;

        default:
          // NB: Experimenter doesn't outright reject invalid types either.
          Cu.reportError(
            `Feature ID ${featureId} has variable ${varName} with invalid FML type: ${prop.type}`
          );
          continue;
      }

      if (prop.type === "string" && !!desc.enum) {
        prop.enum = [...desc.enum];
      }

      schema.properties[varName] = prop;
    }

    return schema;
  }
}

const RemoteSettingsExperimentLoader = new _RemoteSettingsExperimentLoader();
