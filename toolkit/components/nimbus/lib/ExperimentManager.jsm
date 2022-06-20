/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ExperimentManager", "_ExperimentManager"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ClientEnvironment: "resource://normandy/lib/ClientEnvironment.jsm",
  ExperimentStore: "resource://nimbus/lib/ExperimentStore.jsm",
  NormandyUtils: "resource://normandy/lib/NormandyUtils.jsm",
  Sampling: "resource://gre/modules/components-utils/Sampling.jsm",
  TelemetryEvents: "resource://normandy/lib/TelemetryEvents.jsm",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.jsm",
  FirstStartup: "resource://gre/modules/FirstStartup.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.import(
    "resource://messaging-system/lib/Logger.jsm"
  );
  return new Logger("ExperimentManager");
});

const TELEMETRY_EVENT_OBJECT = "nimbus_experiment";
const TELEMETRY_EXPERIMENT_ACTIVE_PREFIX = "nimbus-";
const TELEMETRY_DEFAULT_EXPERIMENT_TYPE = "nimbus";

const UPLOAD_ENABLED_PREF = "datareporting.healthreport.uploadEnabled";
const STUDIES_OPT_OUT_PREF = "app.shield.optoutstudies.enabled";

const STUDIES_ENABLED_CHANGED = "nimbus:studies-enabled-changed";

function featuresCompat(branch) {
  if (!branch || (!branch.feature && !branch.features)) {
    return [];
  }
  let { features } = branch;
  // In <=v1.5.0 of the Nimbus API, experiments had single feature
  if (!features) {
    features = [branch.feature];
  }

  return features;
}

/**
 * A module for processes Experiment recipes, choosing and storing enrollment state,
 * and sending experiment-related Telemetry.
 */
class _ExperimentManager {
  constructor({ id = "experimentmanager", store } = {}) {
    this.id = id;
    this.store = store || new lazy.ExperimentStore();
    this.sessions = new Map();
    Services.prefs.addObserver(UPLOAD_ENABLED_PREF, this);
    Services.prefs.addObserver(STUDIES_OPT_OUT_PREF, this);
  }

  get studiesEnabled() {
    return (
      Services.prefs.getBoolPref(UPLOAD_ENABLED_PREF) &&
      Services.prefs.getBoolPref(STUDIES_OPT_OUT_PREF)
    );
  }

  /**
   * Creates a targeting context with following filters:
   *
   *   * `activeExperiments`: an array of slugs of all the active experiments
   *   * `isFirstStartup`: a boolean indicating whether or not the current enrollment
   *      is performed during the first startup
   *
   * @returns {Object} A context object
   * @memberof _ExperimentManager
   */
  createTargetingContext() {
    let context = {
      isFirstStartup: lazy.FirstStartup.state === lazy.FirstStartup.IN_PROGRESS,
    };
    Object.defineProperty(context, "activeExperiments", {
      get: async () => {
        await this.store.ready();
        return this.store.getAllActive().map(exp => exp.slug);
      },
    });
    Object.defineProperty(context, "activeRollouts", {
      get: async () => {
        await this.store.ready();
        return this.store.getAllRollouts().map(rollout => rollout.slug);
      },
    });
    return context;
  }

  /**
   * Runs on startup, including before first run
   */
  async onStartup() {
    await this.store.init();
    const restoredExperiments = this.store.getAllActive();
    const restoredRollouts = this.store.getAllRollouts();

    for (const experiment of restoredExperiments) {
      this.setExperimentActive(experiment);
    }
    for (const rollout of restoredRollouts) {
      this.setExperimentActive(rollout);
    }

    this.observe();
  }

  /**
   * Runs every time a Recipe is updated or seen for the first time.
   * @param {RecipeArgs} recipe
   * @param {string} source
   */
  async onRecipe(recipe, source) {
    const { slug, isEnrollmentPaused } = recipe;

    if (!source) {
      throw new Error("When calling onRecipe, you must specify a source.");
    }

    if (!this.sessions.has(source)) {
      this.sessions.set(source, new Set());
    }
    this.sessions.get(source).add(slug);

    if (this.store.has(slug)) {
      this.updateEnrollment(recipe);
    } else if (isEnrollmentPaused) {
      lazy.log.debug(`Enrollment is paused for "${slug}"`);
    } else if (!(await this.isInBucketAllocation(recipe.bucketConfig))) {
      lazy.log.debug("Client was not enrolled because of the bucket sampling");
    } else {
      await this.enroll(recipe, source);
    }
  }

  _checkUnseenEnrollments(
    enrollments,
    sourceToCheck,
    recipeMismatches,
    invalidRecipes,
    invalidBranches
  ) {
    for (const enrollment of enrollments) {
      const { slug, source } = enrollment;
      if (sourceToCheck !== source) {
        continue;
      }
      if (!this.sessions.get(source)?.has(slug)) {
        lazy.log.debug(`Stopping study for recipe ${slug}`);
        try {
          let reason;
          if (recipeMismatches.includes(slug)) {
            reason = "targeting-mismatch";
          } else if (invalidRecipes.includes(slug)) {
            reason = "invalid-recipe";
          } else if (invalidBranches.includes(slug)) {
            reason = "invalid-branch";
          } else {
            reason = "recipe-not-seen";
          }
          this.unenroll(slug, reason);
        } catch (err) {
          Cu.reportError(err);
        }
      }
    }
  }

  /**
   * Removes stored enrollments that were not seen after syncing with Remote Settings
   * Runs when the all recipes been processed during an update, including at first run.
   * @param {string} sourceToCheck
   * @param {object} options Extra context used in telemetry reporting
   */
  onFinalize(
    sourceToCheck,
    { recipeMismatches = [], invalidRecipes = [], invalidBranches = [] } = {}
  ) {
    if (!sourceToCheck) {
      throw new Error("When calling onFinalize, you must specify a source.");
    }
    const activeExperiments = this.store.getAllActive();
    const activeRollouts = this.store.getAllRollouts();
    this._checkUnseenEnrollments(
      activeExperiments,
      sourceToCheck,
      recipeMismatches,
      invalidRecipes,
      invalidBranches
    );
    this._checkUnseenEnrollments(
      activeRollouts,
      sourceToCheck,
      recipeMismatches,
      invalidRecipes,
      invalidBranches
    );

    this.sessions.delete(sourceToCheck);
  }

  /**
   * Bucket configuration specifies a specific percentage of clients that can
   * be enrolled.
   * @param {BucketConfig} bucketConfig
   * @returns {Promise<boolean>}
   */
  isInBucketAllocation(bucketConfig) {
    if (!bucketConfig) {
      lazy.log.debug("Cannot enroll if recipe bucketConfig is not set.");
      return false;
    }

    let id;
    if (bucketConfig.randomizationUnit === "normandy_id") {
      id = lazy.ClientEnvironment.userId;
    } else {
      // Others not currently supported.
      lazy.log.debug(
        `Invalid randomizationUnit: ${bucketConfig.randomizationUnit}`
      );
      return false;
    }

    return lazy.Sampling.bucketSample(
      [id, bucketConfig.namespace],
      bucketConfig.start,
      bucketConfig.count,
      bucketConfig.total
    );
  }

  /**
   * Start a new experiment by enrolling the users
   *
   * @param {RecipeArgs} recipe
   * @param {string} source
   * @returns {Promise<Enrollment>} The experiment object stored in the data store
   * @rejects {Error}
   * @memberof _ExperimentManager
   */
  async enroll(recipe, source) {
    let { slug, branches } = recipe;
    if (this.store.has(slug)) {
      this.sendFailureTelemetry("enrollFailed", slug, "name-conflict");
      throw new Error(`An experiment with the slug "${slug}" already exists.`);
    }

    let storeLookupByFeature = recipe.isRollout
      ? this.store.getRolloutForFeature.bind(this.store)
      : this.store.hasExperimentForFeature.bind(this.store);
    const branch = await this.chooseBranch(slug, branches);
    const features = featuresCompat(branch);
    for (let feature of features) {
      if (storeLookupByFeature(feature?.featureId)) {
        lazy.log.debug(
          `Skipping enrollment for "${slug}" because there is an existing ${
            recipe.isRollout ? "rollout" : "experiment"
          } for this feature.`
        );
        this.sendFailureTelemetry("enrollFailed", slug, "feature-conflict");

        return null;
      }
    }

    return this._enroll(recipe, branch, source);
  }

  _enroll(
    {
      slug,
      experimentType = TELEMETRY_DEFAULT_EXPERIMENT_TYPE,
      userFacingName,
      userFacingDescription,
      featureIds,
      isRollout,
    },
    branch,
    source,
    options = {}
  ) {
    /** @type {Enrollment} */
    const experiment = {
      slug,
      branch,
      active: true,
      enrollmentId: lazy.NormandyUtils.generateUuid(),
      experimentType,
      source,
      userFacingName,
      userFacingDescription,
      lastSeen: new Date().toJSON(),
      featureIds,
    };

    if (typeof isRollout !== "undefined") {
      experiment.isRollout = isRollout;
    }

    // Tag this as a forced enrollment. This prevents all unenrolling unless
    // manually triggered from about:studies
    if (options.force) {
      experiment.force = true;
    }

    if (isRollout) {
      experiment.experimentType = "rollout";
      this.store.addEnrollment(experiment);
      this.setExperimentActive(experiment);
    } else {
      this.store.addEnrollment(experiment);
      this.setExperimentActive(experiment);
    }
    this.sendEnrollmentTelemetry(experiment);

    lazy.log.debug(
      `New ${isRollout ? "rollout" : "experiment"} started: ${slug}, ${
        branch.slug
      }`
    );

    return experiment;
  }

  forceEnroll(recipe, branch, source = "force-enrollment") {
    /**
     * If we happen to be enrolled in an experiment for the same feature
     * we need to unenroll from that experiment.
     * If the experiment has the same slug after unenrollment adding it to the
     * store will overwrite the initial experiment.
     */
    const features = featuresCompat(branch);
    for (let feature of features) {
      let experiment = this.store.getExperimentForFeature(feature?.featureId);
      let rollout = this.store.getRolloutForFeature(feature?.featureId);
      if (experiment) {
        lazy.log.debug(
          `Existing experiment found for the same feature ${feature.featureId}, unenrolling.`
        );

        this.unenroll(experiment.slug, source);
      }
      if (rollout) {
        lazy.log.debug(
          `Existing experiment found for the same feature ${feature.featureId}, unenrolling.`
        );

        this.unenroll(rollout.slug, source);
      }
    }

    recipe.userFacingName = `${recipe.userFacingName} - Forced enrollment`;

    const experiment = this._enroll(
      {
        ...recipe,
        slug: `optin-${recipe.slug}`,
      },
      branch,
      source,
      { force: true }
    );

    Services.obs.notifyObservers(null, "nimbus:force-enroll");

    return experiment;
  }

  /**
   * Update an enrollment that was already set
   *
   * @param {RecipeArgs} recipe
   */
  updateEnrollment(recipe) {
    /** @type Enrollment */
    const enrollment = this.store.get(recipe.slug);

    // Don't update experiments that were already unenrolled.
    if (enrollment.active === false) {
      lazy.log.debug(`Enrollment ${recipe.slug} has expired, aborting.`);
      return false;
    }

    // Stay in the same branch, don't re-sample every time.
    const branch = recipe.branches.find(
      branch => branch.slug === enrollment.branch.slug
    );

    if (!branch) {
      // Our branch has been removed. Unenroll.
      this.unenroll(recipe.slug, "branch-removed");
    }

    return true;
  }

  /**
   * Stop an experiment that is currently active
   *
   * @param {string} slug
   * @param {string} reason
   */
  unenroll(slug, reason = "unknown") {
    const enrollment = this.store.get(slug);
    if (!enrollment) {
      this.sendFailureTelemetry("unenrollFailed", slug, "does-not-exist");
      throw new Error(`Could not find an experiment with the slug "${slug}"`);
    }

    if (!enrollment.active) {
      this.sendFailureTelemetry("unenrollFailed", slug, "already-unenrolled");
      throw new Error(
        `Cannot stop experiment "${slug}" because it is already expired`
      );
    }

    lazy.TelemetryEnvironment.setExperimentInactive(slug);
    // We also need to set the experiment inactive in the Glean Experiment API
    Services.fog.setExperimentInactive(slug);
    this.store.updateExperiment(slug, { active: false });

    lazy.TelemetryEvents.sendEvent("unenroll", TELEMETRY_EVENT_OBJECT, slug, {
      reason,
      branch: enrollment.branch.slug,
      enrollmentId:
        enrollment.enrollmentId || lazy.TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
    });
    // Sent Glean event equivalent
    Glean.nimbusEvents.unenrollment.record({
      experiment: slug,
      branch: enrollment.branch.slug,
      enrollment_id:
        enrollment.enrollmentId || lazy.TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
      reason,
    });

    lazy.log.debug(`Recipe unenrolled: ${slug}`);
  }

  /**
   * Unenroll from all active studies if user opts out.
   */
  observe(aSubject, aTopic, aPrefName) {
    if (!this.studiesEnabled) {
      for (const { slug } of this.store.getAllActive()) {
        this.unenroll(slug, "studies-opt-out");
      }
      for (const { slug } of this.store.getAllRollouts()) {
        this.unenroll(slug, "studies-opt-out");
      }
    }

    Services.obs.notifyObservers(null, STUDIES_ENABLED_CHANGED);
  }

  /**
   * Send Telemetry for undesired event
   *
   * @param {string} eventName
   * @param {string} slug
   * @param {string} reason
   */
  sendFailureTelemetry(eventName, slug, reason) {
    lazy.TelemetryEvents.sendEvent(eventName, TELEMETRY_EVENT_OBJECT, slug, {
      reason,
    });
    if (eventName == "enrollFailed") {
      Glean.nimbusEvents.enrollFailed.record({
        experiment: slug,
        reason,
      });
    } else if (eventName == "unenrollFailed") {
      Glean.nimbusEvents.unenrollFailed.record({
        experiment: slug,
        reason,
      });
    }
  }

  /**
   *
   * @param {Enrollment} experiment
   */
  sendEnrollmentTelemetry({ slug, branch, experimentType, enrollmentId }) {
    lazy.TelemetryEvents.sendEvent("enroll", TELEMETRY_EVENT_OBJECT, slug, {
      experimentType,
      branch: branch.slug,
      enrollmentId:
        enrollmentId || lazy.TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
    });
    Glean.nimbusEvents.enrollment.record({
      experiment: slug,
      branch: branch.slug,
      enrollment_id:
        enrollmentId || lazy.TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
      experiment_type: experimentType,
    });
  }

  /**
   * Sets Telemetry when activating an experiment.
   *
   * @param {Enrollment} experiment
   */
  setExperimentActive(experiment) {
    lazy.TelemetryEnvironment.setExperimentActive(
      experiment.slug,
      experiment.branch.slug,
      {
        type: `${TELEMETRY_EXPERIMENT_ACTIVE_PREFIX}${experiment.experimentType}`,
        enrollmentId:
          experiment.enrollmentId ||
          lazy.TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
      }
    );
    // Report the experiment to the Glean Experiment API
    Services.fog.setExperimentActive(experiment.slug, experiment.branch.slug, {
      type: `${TELEMETRY_EXPERIMENT_ACTIVE_PREFIX}${experiment.experimentType}`,
      enrollmentId:
        experiment.enrollmentId || lazy.TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
    });
  }

  /**
   * Generate Normandy UserId respective to a branch
   * for a given experiment.
   *
   * @param {string} slug
   * @param {Array<{slug: string; ratio: number}>} branches
   * @param {string} namespace
   * @param {number} start
   * @param {number} count
   * @param {number} total
   * @returns {Promise<{[branchName: string]: string}>} An object where
   * the keys are branch names and the values are user IDs that will enroll
   * a user for that particular branch. Also includes a `notInExperiment` value
   * that will not enroll the user in the experiment
   */
  async generateTestIds({ slug, branches, namespace, start, count, total }) {
    const branchValues = {};

    if (!slug || !namespace) {
      throw new Error(`slug, namespace not in expected format`);
    }

    if (!(start < total && count < total)) {
      throw new Error("Must include start, count, and total as integers");
    }

    if (
      !Array.isArray(branches) ||
      branches.filter(branch => branch.slug && branch.ratio).length !==
        branches.length
    ) {
      throw new Error("branches parameter not in expected format");
    }

    while (Object.keys(branchValues).length < branches.length + 1) {
      const id = lazy.NormandyUtils.generateUuid();
      const enrolls = await lazy.Sampling.bucketSample(
        [id, namespace],
        start,
        count,
        total
      );
      // Does this id enroll the user in the experiment
      if (enrolls) {
        // Choose a random branch
        const { slug: pickedBranch } = await this.chooseBranch(
          slug,
          branches,
          id
        );

        if (!Object.keys(branchValues).includes(pickedBranch)) {
          branchValues[pickedBranch] = id;
          lazy.log.debug(`Found a value for "${pickedBranch}"`);
        }
      } else if (!branchValues.notInExperiment) {
        branchValues.notInExperiment = id;
      }
    }
    return branchValues;
  }

  /**
   * Choose a branch randomly.
   *
   * @param {string} slug
   * @param {Branch[]} branches
   * @returns {Promise<Branch>}
   * @memberof _ExperimentManager
   */
  async chooseBranch(slug, branches, userId = lazy.ClientEnvironment.userId) {
    const ratios = branches.map(({ ratio = 1 }) => ratio);

    // It's important that the input be:
    // - Unique per-user (no one is bucketed alike)
    // - Unique per-experiment (bucketing differs across multiple experiments)
    // - Differs from the input used for sampling the recipe (otherwise only
    //   branches that contain the same buckets as the recipe sampling will
    //   receive users)
    const input = `${this.id}-${userId}-${slug}-branch`;

    const index = await lazy.Sampling.ratioSample(input, ratios);
    return branches[index];
  }
}

const ExperimentManager = new _ExperimentManager();
