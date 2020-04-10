/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * @typedef {import("./@types/ExperimentManager").RecipeArgs} RecipeArgs
 * @typedef {import("./@types/ExperimentManager").Enrollment} Enrollment
 * @typedef {import("./@types/ExperimentManager").Branch} Branch
 */

const EXPORTED_SYMBOLS = ["ExperimentManager", "_ExperimentManager"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ClientEnvironment: "resource://normandy/lib/ClientEnvironment.jsm",
  ExperimentStore:
    "resource://messaging-system/experiments/ExperimentStore.jsm",
  LogManager: "resource://normandy/lib/LogManager.jsm",
  NormandyUtils: "resource://normandy/lib/NormandyUtils.jsm",
  Sampling: "resource://gre/modules/components-utils/Sampling.jsm",
  TelemetryEvents: "resource://normandy/lib/TelemetryEvents.jsm",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.jsm",
});

// This is included with event telemetry e.g. "enroll"
// TODO: Add a new type called "messaging_study"
const EVENT_TELEMETRY_STUDY_TYPE = "preference_study";
// This is used by Telemetry.setExperimentActive
const TELEMETRY_EXPERIMENT_TYPE_PREFIX = "normandy-";
// Also included in telemetry
const DEFAULT_EXPERIMENT_TYPE = "messaging_experiment";

/**
 * A module for processes Experiment recipes, choosing and storing enrollment state,
 * and sending experiment-related Telemetry.
 */
class _ExperimentManager {
  constructor({ id = "experimentmanager", storeId } = {}) {
    this.id = id;
    this.store = new ExperimentStore(storeId);
    this.slugsSeenInThisSession = new Set();
    this.log = LogManager.getLogger("ExperimentManager");
  }

  /**
   * Runs on startup, including before first run
   */
  async onStartup() {
    const restoredExperiments = this.store.getAllActive();

    for (const experiment of restoredExperiments) {
      this.setExperimentActive(experiment);
    }
  }

  /**
   * Runs every time a Recipe is updated or seen for the first time.
   * @param {RecipeArgs} recipe
   */
  async onRecipe(recipe) {
    const { slug, isEnrollmentPaused } = recipe;

    this.slugsSeenInThisSession.add(slug);

    if (this.store.has(slug)) {
      this.updateEnrollment(recipe);
    } else if (isEnrollmentPaused) {
      this.log.debug(`Enrollment is paused for "${slug}"`);
    } else {
      await this.enroll(recipe);
    }
  }

  // Runs when the all recipes been processed during an update, including at first run.
  onFinalize() {
    const activeExperiments = this.store.getAllActive();

    for (const experiment of activeExperiments) {
      const { slug } = experiment;
      if (!this.slugsSeenInThisSession.has(slug)) {
        this.log.debug(`Stopping study for recipe ${slug}`);
        try {
          this.unenroll(slug, "recipe-not-seen");
        } catch (err) {
          Cu.reportError(err);
        }
      }
    }

    this.slugsSeenInThisSession.clear();
  }

  /**
   * Start a new experiment by enrolling the users
   *
   * @param {RecipeArgs} recipe
   * @returns {Promise<Enrollment>} The experiment object stored in the data store
   * @rejects {Error}
   * @memberof _ExperimentManager
   */
  async enroll({ slug, branches, experimentType = DEFAULT_EXPERIMENT_TYPE }) {
    if (this.store.has(slug)) {
      this.sendFailureTelemetry("enrollFailed", slug, "name-conflict");
      throw new Error(`An experiment with the slug "${slug}" already exists.`);
    }

    const enrollmentId = NormandyUtils.generateUuid();
    const branch = await this.chooseBranch(slug, branches);

    if (branch.groups && this.store.hasExperimentForGroups(branch.groups)) {
      this.log.debug(
        `Skipping enrollment for "${slug}" because there is an existing experiment for one of its groups.`
      );
      this.sendFailureTelemetry("enrollFailed", slug, "group-conflict");
      throw new Error(`An experiment with a conflicting group already exists.`);
    }

    /** @type {Enrollment} */
    const experiment = {
      slug,
      branch,
      active: true,
      enrollmentId,
      experimentType,
    };

    this.store.addExperiment(experiment);
    this.setExperimentActive(experiment);
    this.sendEnrollmentTelemetry(experiment);

    this.log.debug(`New experiment started: ${slug}, ${branch.slug}`);

    return experiment;
  }

  /**
   * Update an enrollment that was already set
   *
   * @param {RecipeArgs} recipe
   */
  updateEnrollment(recipe) {
    /** @type Enrollment */
    const experiment = this.store.get(recipe.slug);

    // Don't update experiments that were already unenrolled.
    if (experiment.active === false) {
      this.log.debug(`Enrollment ${recipe.slug} has expired, aborting.`);
      return;
    }

    // Stay in the same branch, don't re-sample every time.
    const branch = recipe.branches.find(
      branch => branch.slug === experiment.branch
    );

    if (!branch) {
      // Our branch has been removed. Unenroll.
      this.unenroll(recipe.slug, "branch-removed");
    }
  }

  /**
   * Stop an experiment that is currently active
   *
   * @param {string} slug
   * @param {object} [options]
   * @param {string} [options.reason]
   */
  unenroll(slug, { reason = "unknown" } = {}) {
    const experiment = this.store.get(slug);
    if (!experiment) {
      this.sendFailureTelemetry("unenrollFailed", slug, "does-not-exist");
      throw new Error(`Could not find an experiment with the slug "${slug}"`);
    }

    if (!experiment.active) {
      this.sendFailureTelemetry("unenrollFailed", slug, "already-unenrolled");
      throw new Error(
        `Cannot stop experiment "${slug}" because it is already expired`
      );
    }

    this.store.updateExperiment(slug, { active: false });

    TelemetryEnvironment.setExperimentInactive(slug);
    TelemetryEvents.sendEvent("unenroll", EVENT_TELEMETRY_STUDY_TYPE, slug, {
      reason,
      branch: experiment.branch.slug,
      enrollmentId:
        experiment.enrollmentId || TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
    });

    this.log.debug(`Experiment unenrolled: ${slug}}`);
  }

  /**
   * Send Telemetry for undesired event
   *
   * @param {string} eventName
   * @param {string} slug
   * @param {string} reason
   */
  sendFailureTelemetry(eventName, slug, reason) {
    TelemetryEvents.sendEvent(eventName, EVENT_TELEMETRY_STUDY_TYPE, slug, {
      reason,
    });
  }

  /**
   *
   * @param {Enrollment} experiment
   */
  sendEnrollmentTelemetry({ slug, branch, experimentType, enrollmentId }) {
    TelemetryEvents.sendEvent("enroll", EVENT_TELEMETRY_STUDY_TYPE, slug, {
      experimentType,
      branch: branch.slug,
      enrollmentId: enrollmentId || TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
    });
  }

  /**
   * Sets Telemetry when activating an experiment.
   *
   * @param {Enrollment} experiment
   * @memberof _ExperimentManager
   */
  setExperimentActive(experiment) {
    TelemetryEnvironment.setExperimentActive(
      experiment.slug,
      experiment.branch.slug,
      {
        type: `${TELEMETRY_EXPERIMENT_TYPE_PREFIX}${experiment.experimentType}`,
        enrollmentId:
          experiment.enrollmentId || TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
      }
    );
  }

  /**
   * Choose a branch randomly.
   *
   * @param {string} slug
   * @param {Branch[]} branches
   * @returns {Promise<Branch>}
   * @memberof _ExperimentManager
   */
  async chooseBranch(slug, branches) {
    const ratios = branches.map(({ ratio = 1 }) => ratio);
    const userId = ClientEnvironment.userId;

    // It's important that the input be:
    // - Unique per-user (no one is bucketed alike)
    // - Unique per-experiment (bucketing differs across multiple experiments)
    // - Differs from the input used for sampling the recipe (otherwise only
    //   branches that contain the same buckets as the recipe sampling will
    //   receive users)
    const input = `${this.id}-${userId}-${slug}-branch`;

    const index = await Sampling.ratioSample(input, ratios);
    return branches[index];
  }
}

const ExperimentManager = new _ExperimentManager();
