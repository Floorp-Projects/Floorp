/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  _ExperimentFeature: "resource://nimbus/ExperimentAPI.sys.mjs",
  CleanupManager: "resource://normandy/lib/CleanupManager.sys.mjs",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.sys.mjs",
  JsonSchema: "resource://gre/modules/JsonSchema.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  TargetingContext: "resource://messaging-system/targeting/Targeting.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ASRouterTargeting: "resource://activity-stream/lib/ASRouterTargeting.jsm",
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.importESModule(
    "resource://messaging-system/lib/Logger.sys.mjs"
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

export class _RemoteSettingsExperimentLoader {
  constructor() {
    // Has the timer been set?
    this._initialized = false;
    // Are we in the middle of updating recipes already?
    this._updating = false;

    // Make it possible to override for testing
    this.manager = lazy.ExperimentManager;

    ChromeUtils.defineLazyGetter(this, "remoteSettingsClient", () => {
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

  /**
   * Initialize the loader, updating recipes from Remote Settings.
   *
   * @param {Object} options            additional options.
   * @param {bool}   options.forceSync  force Remote Settings to sync recipe collection
   *                                    before updating recipes; throw if sync fails.
   * @return {Promise}                  which resolves after initialization and recipes
   *                                    are updated.
   */
  async init(options = {}) {
    const { forceSync = false } = options;

    if (this._initialized || !this.enabled || !this.studiesEnabled) {
      return;
    }

    this.setTimer();
    lazy.CleanupManager.addCleanupHandler(() => this.uninit());
    this._initialized = true;

    await this.updateRecipes(undefined, { forceSync });
  }

  uninit() {
    if (!this._initialized) {
      return;
    }
    lazy.timerManager.unregisterTimer(TIMER_NAME);
    this._initialized = false;
    this._updating = false;
  }

  /**
   * Get all recipes from remote settings
   * @param {string} trigger   What caused the update to occur?
   * @param {Object} options            additional options.
   * @param {bool}   options.forceSync  force Remote Settings to sync recipe collection
   *                                    before updating recipes; throw if sync fails.
   * @return {Promise}                  which resolves after recipes are updated.
   */
  async updateRecipes(trigger, options = {}) {
    if (this._updating || !this._initialized) {
      return;
    }

    const { forceSync = false } = options;

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
        forceSync,
        // Throw instead of returning an empty list.
        emptyListFallback: false,
      });
      lazy.log.debug(`Got ${recipes.length} recipes from Remote Settings`);
    } catch (e) {
      lazy.log.debug("Error getting recipes from remote settings.");
      loadingError = true;
      console.error(e);
    }

    let recipeValidator;

    if (validationEnabled) {
      recipeValidator = new lazy.JsonSchema.Validator(
        await SCHEMAS.NimbusExperiment
      );
    }

    const enrollmentsCtx = new EnrollmentsContext(
      this.manager,
      recipeValidator,
      { validationEnabled, shouldCheckTargeting: true }
    );

    if (recipes && !loadingError) {
      for (const recipe of recipes) {
        if (await enrollmentsCtx.checkRecipe(recipe)) {
          await this.manager.onRecipe(recipe, "rs-loader");
        }
      }

      lazy.log.debug(
        `${enrollmentsCtx.matches} recipes matched. Finalizing ExperimentManager.`
      );
      this.manager.onFinalize("rs-loader", enrollmentsCtx.getResults());
    }

    if (trigger !== "timer") {
      const lastUpdateTime = Math.round(Date.now() / 1000);
      Services.prefs.setIntPref(TIMER_LAST_UPDATE_PREF, lastUpdateTime);
    }

    Services.obs.notifyObservers(null, "nimbus:enrollments-updated");

    this._updating = false;
  }

  async optInToExperiment({
    slug,
    branch: branchSlug,
    collection,
    applyTargeting = false,
  }) {
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
      console.error(e);
      throw new Error("Error getting recipes from remote settings.");
    }

    const recipe = recipes.find(r => r.slug === slug);

    if (!recipe) {
      throw new Error(
        `Could not find experiment slug ${slug} in collection ${
          collection || lazy.COLLECTION_ID
        }.`
      );
    }

    const recipeValidator = new lazy.JsonSchema.Validator(
      await SCHEMAS.NimbusExperiment
    );
    const enrollmentsCtx = new EnrollmentsContext(
      this.manager,
      recipeValidator,
      {
        validationEnabled: this.validationEnabled,
        shouldCheckTargeting: applyTargeting,
      }
    );

    if (!(await enrollmentsCtx.checkRecipe(recipe))) {
      const results = enrollmentsCtx.getResults();

      if (results.recipeMismatches.length) {
        throw new Error(`Recipe ${recipe.slug} did not match targeting`);
      } else if (results.invalidRecipes.length) {
        console.error(`Recipe ${recipe.slug} did not match recipe schema`);
      } else if (results.invalidBranches.size) {
        // There will only be one entry becuase we only validated a single recipe.
        for (const branches of results.invalidBranches.values()) {
          for (const branch of branches) {
            console.error(
              `Recipe ${recipe.slug} failed feature validation for branch ${branch}`
            );
          }
        }
      } else if (results.invalidFeatures.length) {
        for (const featureIds of results.invalidFeatures.values()) {
          for (const featureId of featureIds) {
            console.error(
              `Recipe ${recipe.slug} references unknown feature ID ${featureId}`
            );
          }
        }
      }

      throw new Error(
        `Recipe ${recipe.slug} failed validation: ${JSON.stringify(results)}`
      );
    }

    let branch = recipe.branches.find(b => b.slug === branchSlug);
    if (!branch) {
      throw new Error(`Could not find branch slug ${branchSlug} in ${slug}.`);
    }

    await this.manager.forceEnroll(recipe, branch);
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
}

export class EnrollmentsContext {
  constructor(
    experimentManager,
    recipeValidator,
    { validationEnabled = true, shouldCheckTargeting = true } = {}
  ) {
    this.experimentManager = experimentManager;
    this.recipeValidator = recipeValidator;

    this.validationEnabled = validationEnabled;
    this.shouldCheckTargeting = shouldCheckTargeting;
    this.matches = 0;

    this.recipeMismatches = [];
    this.invalidRecipes = [];
    this.invalidBranches = new Map();
    this.invalidFeatures = new Map();
    this.validatorCache = {};
    this.missingLocale = [];
    this.missingL10nIds = new Map();

    this.locale = Services.locale.appLocaleAsBCP47;
  }

  getResults() {
    return {
      recipeMismatches: this.recipeMismatches,
      invalidRecipes: this.invalidRecipes,
      invalidBranches: this.invalidBranches,
      invalidFeatures: this.invalidFeatures,
      missingLocale: this.missingLocale,
      missingL10nIds: this.missingL10nIds,
      locale: this.locale,
      validationEnabled: this.validationEnabled,
    };
  }

  async checkRecipe(recipe) {
    if (recipe.appId !== "firefox-desktop") {
      // Skip over recipes not intended for desktop. Experimenter publishes
      // recipes into a collection per application (desktop goes to
      // `nimbus-desktop-experiments`) but all preview experiments share the
      // same collection (`nimbus-preview`).
      //
      // This is *not* the same as `lazy.APP_ID` which is used to
      // distinguish between desktop Firefox and the desktop background
      // updater.
      return false;
    }

    const validateFeatureSchemas =
      this.validationEnabled && !recipe.featureValidationOptOut;

    if (this.validationEnabled) {
      let validation = this.recipeValidator.validate(recipe);
      if (!validation.valid) {
        console.error(
          `Could not validate experiment recipe ${recipe.id}: ${JSON.stringify(
            validation.errors,
            null,
            2
          )}`
        );
        if (recipe.slug) {
          this.invalidRecipes.push(recipe.slug);
        }
        return false;
      }
    }

    const featureIds =
      recipe.featureIds ??
      recipe.branches
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
          `${recipe.id} uses feature ${featureId} which is not enabled for this application (${lazy.APP_ID}) -- skipping`
        );
        haveAllFeatures = false;
        break;
      }
    }

    if (!haveAllFeatures) {
      return false;
    }

    if (this.shouldCheckTargeting) {
      const match = await this.checkTargeting(recipe);

      if (match) {
        const type = recipe.isRollout ? "rollout" : "experiment";
        lazy.log.debug(`[${type}] ${recipe.id} matched targeting`);
      } else {
        lazy.log.debug(`${recipe.id} did not match due to targeting`);
        this.recipeMismatches.push(recipe.slug);
        return false;
      }
    }

    this.matches++;

    if (
      typeof recipe.localizations === "object" &&
      recipe.localizations !== null
    ) {
      if (
        typeof recipe.localizations[this.locale] !== "object" ||
        recipe.localizations[this.locale] === null
      ) {
        this.missingLocale.push(recipe.slug);
        lazy.log.debug(
          `${recipe.id} is localized but missing locale ${this.locale}`
        );
        return false;
      }
    }

    const result = await this._validateBranches(recipe, validateFeatureSchemas);
    if (!result.valid) {
      if (result.invalidBranchSlugs.length) {
        this.invalidBranches.set(recipe.slug, result.invalidBranchSlugs);
      }
      if (result.invalidFeatureIds.length) {
        this.invalidFeatures.set(recipe.slug, result.invalidFeatureIds);
      }
      if (result.missingL10nIds.length) {
        this.missingL10nIds.set(recipe.slug, result.missingL10nIds);
      }
      lazy.log.debug(`${recipe.id} did not validate`);
      return false;
    }

    return true;
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
      this.experimentManager.createTargetingContext(),
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
      console.error(e);
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
   * Validate the branches of an experiment.
   *
   * @param {object} recipe The recipe object.
   * @param {boolean} validateSchema Whether to validate the feature values
   *        using JSON schemas.
   *
   * @returns {object} The lists of invalid branch slugs and invalid feature
   *                   IDs.
   */
  async _validateBranches({ id, branches, localizations }, validateSchema) {
    const invalidBranchSlugs = [];
    const invalidFeatureIds = new Set();
    const missingL10nIds = new Set();

    if (validateSchema || typeof localizations !== "undefined") {
      for (const [branchIdx, branch] of branches.entries()) {
        const features = branch.features ?? [branch.feature];
        for (const feature of features) {
          const { featureId, value } = feature;
          if (!lazy.NimbusFeatures[featureId]) {
            console.error(
              `Experiment ${id} has unknown featureId: ${featureId}`
            );

            invalidFeatureIds.add(featureId);
            continue;
          }

          let substitutedValue = value;

          if (localizations) {
            // We already know that we have a localization table for this locale
            // because we checked in `checkRecipe`.
            try {
              substitutedValue =
                lazy._ExperimentFeature.substituteLocalizations(
                  value,
                  localizations[Services.locale.appLocaleAsBCP47],
                  missingL10nIds
                );
            } catch (e) {
              if (e?.reason === "l10n-missing-entry") {
                // Skip validation because it *will* fail.
                continue;
              }
              throw e;
            }
          }

          if (validateSchema) {
            let validator;
            if (this.validatorCache[featureId]) {
              validator = this.validatorCache[featureId];
            } else if (lazy.NimbusFeatures[featureId].manifest.schema?.uri) {
              const uri = lazy.NimbusFeatures[featureId].manifest.schema.uri;
              try {
                const schema = await fetch(uri, {
                  credentials: "omit",
                }).then(rsp => rsp.json());

                validator = this.validatorCache[featureId] =
                  new lazy.JsonSchema.Validator(schema);
              } catch (e) {
                throw new Error(
                  `Could not fetch schema for feature ${featureId} at "${uri}": ${e}`
                );
              }
            } else {
              const schema = this._generateVariablesOnlySchema(
                lazy.NimbusFeatures[featureId]
              );
              validator = this.validatorCache[featureId] =
                new lazy.JsonSchema.Validator(schema);
            }

            const result = validator.validate(substitutedValue);
            if (!result.valid) {
              console.error(
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
      }
    }

    return {
      invalidBranchSlugs,
      invalidFeatureIds: Array.from(invalidFeatureIds),
      missingL10nIds: Array.from(missingL10nIds),
      valid:
        invalidBranchSlugs.length === 0 &&
        invalidFeatureIds.size === 0 &&
        missingL10nIds.size === 0,
    };
  }

  _generateVariablesOnlySchema({ featureId, manifest }) {
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
          console.error(
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

export const RemoteSettingsExperimentLoader =
  new _RemoteSettingsExperimentLoader();
