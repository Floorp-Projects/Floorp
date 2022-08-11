/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = [
  "_RemoteSettingsExperimentLoader",
  "RemoteSettingsExperimentLoader",
];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ASRouterTargeting: "resource://activity-stream/lib/ASRouterTargeting.jsm",
  TargetingContext: "resource://messaging-system/targeting/Targeting.jsm",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  CleanupManager: "resource://normandy/lib/CleanupManager.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  JsonSchema: "resource://gre/modules/JsonSchema.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.import(
    "resource://messaging-system/lib/Logger.jsm"
  );
  return new Logger("RSLoader");
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "timerManager",
  "@mozilla.org/updates/timer-manager;1",
  "nsIUpdateTimerManager"
);

const COLLECTION_ID_PREF = "messaging-system.rsexperimentloader.collection_id";
const COLLECTION_ID_FALLBACK = "nimbus-desktop-experiments";
const ENABLED_PREF = "messaging-system.rsexperimentloader.enabled";

const TIMER_NAME = "rs-experiment-loader-timer";
const TIMER_LAST_UPDATE_PREF = `app.update.lastUpdateTime.${TIMER_NAME}`;
// Use the same update interval as normandy
const RUN_INTERVAL_PREF = "app.normandy.run_interval_seconds";
const NIMBUS_DEBUG_PREF = "nimbus.debug";
const NIMBUS_VALIDATION_PREF = "nimbus.validation.enabled";
const NIMBUS_APPID_PREF = "nimbus.appId";

const STUDIES_ENABLED_CHANGED = "nimbus:studies-enabled-changed";

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "COLLECTION_ID",
  COLLECTION_ID_PREF,
  COLLECTION_ID_FALLBACK
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "NIMBUS_DEBUG",
  NIMBUS_DEBUG_PREF,
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "APP_ID",
  NIMBUS_APPID_PREF,
  "firefox-desktop"
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
    this.manager = lazy.ExperimentManager;

    XPCOMUtils.defineLazyGetter(this, "remoteSettingsClient", () => {
      return lazy.RemoteSettings(lazy.COLLECTION_ID);
    });

    Services.obs.addObserver(this, STUDIES_ENABLED_CHANGED);

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

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "validationEnabled",
      NIMBUS_VALIDATION_PREF,
      true
    );
  }

  get studiesEnabled() {
    return this.manager.studiesEnabled;
  }

  async init() {
    if (this._initialized || !this.enabled || !this.studiesEnabled) {
      return;
    }

    this.setTimer();
    lazy.CleanupManager.addCleanupHandler(() => this.uninit());
    this._initialized = true;

    await this.updateRecipes();
  }

  uninit() {
    if (!this._initialized) {
      return;
    }
    lazy.timerManager.unregisterTimer(TIMER_NAME);
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

    const context = lazy.TargetingContext.combineContexts(
      customContext,
      this.manager.createTargetingContext(),
      lazy.ASRouterTargeting.Environment
    );

    lazy.log.debug("Testing targeting expression:", jexlString);
    const targetingContext = new lazy.TargetingContext(context, {
      source: customContext.source,
    });

    let result = null;
    try {
      result = await targetingContext.evalWithDefault(jexlString);
    } catch (e) {
      lazy.log.debug("Targeting failed because of an error", e);
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
      lazy.log.debug("No targeting for recipe, so it matches automatically");
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

    // Since this method is async, the enabled pref could change between await
    // points. We don't want to half validate experiments, so we cache this to
    // keep it consistent throughout updating.
    const validationEnabled = this.validationEnabled;

    this._updating = true;

    lazy.log.debug(
      "Updating recipes" + (trigger ? ` with trigger ${trigger}` : "")
    );

    let recipes;
    let loadingError = false;

    try {
      recipes = await this.remoteSettingsClient.get({
        // Throw instead of returning an empty list.
        emptyListFallback: false,
      });
      lazy.log.debug(`Got ${recipes.length} recipes from Remote Settings`);
    } catch (e) {
      lazy.log.debug("Error getting recipes from remote settings.");
      loadingError = true;
      Cu.reportError(e);
    }

    let recipeValidator;

    if (validationEnabled) {
      recipeValidator = new lazy.JsonSchema.Validator(
        await SCHEMAS.NimbusExperiment
      );
    }

    let matches = 0;
    let recipeMismatches = [];
    let invalidRecipes = [];
    let invalidBranches = new Map();
    let invalidFeatures = new Map();
    let validatorCache = {};

    if (recipes && !loadingError) {
      for (const r of recipes) {
        if (validationEnabled) {
          let validation = recipeValidator.validate(r);
          if (!validation.valid) {
            Cu.reportError(
              `Could not validate experiment recipe ${r.id}: ${JSON.stringify(
                validation.errors,
                undefined,
                2
              )}`
            );
            if (r.slug) {
              invalidRecipes.push(r.slug);
            }
            continue;
          }
        }

        const featureIds =
          r.featureIds ??
          r.branches
            .flatMap(branch => branch.features ?? [branch.feature])
            .map(featureDef => featureDef.featureId);

        let haveAllFeatures = true;

        for (const featureId of featureIds) {
          const feature = lazy.NimbusFeatures[featureId];

          // If validation is enabled, we want to catch this later in
          // _validateBranches to collect the correct stats for telemetry.
          if (!feature) {
            continue;
          }

          if (!feature.applications.includes(lazy.APP_ID)) {
            lazy.log.debug(
              `${r.id} uses feature ${featureId} which is not enabled for this application (${lazy.APP_ID}) -- skipping`
            );
            haveAllFeatures = false;
            break;
          }
        }
        if (!haveAllFeatures) {
          continue;
        }

        let type = r.isRollout ? "rollout" : "experiment";

        if (validationEnabled) {
          const result = await this._validateBranches(r, validatorCache);
          if (!result.valid) {
            if (result.invalidBranchSlugs.length) {
              invalidBranches.set(r.slug, result.invalidBranchSlugs);
            }
            if (result.invalidFeatureIds.length) {
              invalidFeatures.set(r.slug, result.invalidFeatureIds);
            }
            lazy.log.debug(`${r.id} did not validate`);
            continue;
          }
        }

        if (await this.checkTargeting(r)) {
          matches++;
          lazy.log.debug(`[${type}] ${r.id} matched`);
          await this.manager.onRecipe(r, "rs-loader");
        } else {
          lazy.log.debug(`${r.id} did not match due to targeting`);
          recipeMismatches.push(r.slug);
        }
      }

      lazy.log.debug(
        `${matches} recipes matched. Finalizing ExperimentManager.`
      );
      this.manager.onFinalize("rs-loader", {
        recipeMismatches,
        invalidRecipes,
        invalidBranches,
        invalidFeatures,
        validationEnabled,
      });
    }

    if (trigger !== "timer") {
      const lastUpdateTime = Math.round(Date.now() / 1000);
      Services.prefs.setIntPref(TIMER_LAST_UPDATE_PREF, lastUpdateTime);
    }

    this._updating = false;
  }

  async optInToExperiment({ slug, branch: branchSlug, collection }) {
    lazy.log.debug(`Attempting force enrollment with ${slug} / ${branchSlug}`);

    if (!lazy.NIMBUS_DEBUG) {
      lazy.log.debug(
        `Force enrollment only works when '${NIMBUS_DEBUG_PREF}' is enabled.`
      );
      // More generic error if no debug preference is on.
      throw new Error("Could not opt in.");
    }

    if (!this.studiesEnabled) {
      lazy.log.debug(
        "Force enrollment does not work when studies are disabled."
      );
      throw new Error("Could not opt in: studies are disabled.");
    }

    let recipes;
    try {
      recipes = await lazy
        .RemoteSettings(collection || lazy.COLLECTION_ID)
        .get({
          // Throw instead of returning an empty list.
          emptyListFallback: false,
        });
    } catch (e) {
      Cu.reportError(e);
      throw new Error("Error getting recipes from remote settings.");
    }

    let recipe = recipes.find(r => r.slug === slug);

    if (!recipe) {
      throw new Error(
        `Could not find experiment slug ${slug} in collection ${collection ||
          lazy.COLLECTION_ID}.`
      );
    }

    let branch = recipe.branches.find(b => b.slug === branchSlug);
    if (!branch) {
      throw new Error(`Could not find branch slug ${branchSlug} in ${slug}.`);
    }

    return lazy.ExperimentManager.forceEnroll(recipe, branch);
  }

  /**
   * Handles feature status based on feature pref and STUDIES_OPT_OUT_PREF.
   * Changing any of them to false will turn off any recipe fetching and
   * processing.
   */
  onEnabledPrefChange() {
    if (this._initialized && !(this.enabled && this.studiesEnabled)) {
      this.uninit();
    } else if (!this._initialized && this.enabled && this.studiesEnabled) {
      // If the feature pref is turned on then turn on recipe processing.
      // If the opt in pref is turned on then turn on recipe processing only if
      // the feature pref is also enabled.
      this.init();
    }
  }

  observe(aSubect, aTopic, aData) {
    if (aTopic === STUDIES_ENABLED_CHANGED) {
      this.onEnabledPrefChange();
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
    lazy.timerManager.registerTimer(
      TIMER_NAME,
      () => this.updateRecipes("timer"),
      this.intervalInSeconds
    );
    lazy.log.debug("Registered update timer");
  }

  /**
   * Validate the branches of an experiment using schemas
   *
   * @param {object} recipe The recipe object.
   * @param {object} validatorCache A cache of JSON Schema validators keyed by feature
   *                                ID.
   *
   * @returns {object} The lists of invalid branch slugs and invalid feature
   *                   IDs.
   */
  async _validateBranches({ id, branches }, validatorCache = {}) {
    const invalidBranchSlugs = [];
    const invalidFeatureIds = new Set();

    for (const [branchIdx, branch] of branches.entries()) {
      const features = branch.features ?? [branch.feature];
      for (const feature of features) {
        const { featureId, value } = feature;
        if (!lazy.NimbusFeatures[featureId]) {
          Cu.reportError(
            `Experiment ${id} has unknown featureId: ${featureId}`
          );

          invalidFeatureIds.add(featureId);
          continue;
        }

        let validator;
        if (validatorCache[featureId]) {
          validator = validatorCache[featureId];
        } else if (lazy.NimbusFeatures[featureId].manifest.schema?.uri) {
          const uri = lazy.NimbusFeatures[featureId].manifest.schema.uri;
          try {
            const schema = await fetch(uri, { credentials: "omit" }).then(rsp =>
              rsp.json()
            );
            validator = validatorCache[
              featureId
            ] = new lazy.JsonSchema.Validator(schema);
          } catch (e) {
            throw new Error(
              `Could not fetch schema for feature ${featureId} at "${uri}": ${e}`
            );
          }
        } else {
          const schema = this._generateVariablesOnlySchema(
            featureId,
            lazy.NimbusFeatures[featureId].manifest
          );
          validator = validatorCache[featureId] = new lazy.JsonSchema.Validator(
            schema
          );
        }

        const result = validator.validate(value);
        if (!result.valid) {
          Cu.reportError(
            `Experiment ${id} branch ${branchIdx} feature ${featureId} does not validate: ${JSON.stringify(
              result.errors,
              undefined,
              2
            )}`
          );
          invalidBranchSlugs.push(branch.slug);
        }
      }
    }

    return {
      invalidBranchSlugs,
      invalidFeatureIds: Array.from(invalidFeatureIds),
      valid: invalidBranchSlugs.length === 0 && invalidFeatureIds.size === 0,
    };
  }

  _generateVariablesOnlySchema(featureId, manifest) {
    // See-also: https://github.com/mozilla/experimenter/blob/main/app/experimenter/features/__init__.py#L21-L64
    const schema = {
      $schema: "https://json-schema.org/draft/2019-09/schema",
      title: featureId,
      description: manifest.description,
      type: "object",
      properties: {},
      additionalProperties: false,
    };

    for (const [varName, desc] of Object.entries(manifest.variables)) {
      const prop = {};
      switch (desc.type) {
        case "boolean":
        case "string":
          prop.type = desc.type;
          break;

        case "int":
          prop.type = "integer";
          break;

        case "json":
          // NB: Don't set a type of json fields, since they can be of any type.
          break;

        default:
          // NB: Experimenter doesn't outright reject invalid types either.
          Cu.reportError(
            `Feature ID ${featureId} has variable ${varName} with invalid FML type: ${prop.type}`
          );
          break;
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
