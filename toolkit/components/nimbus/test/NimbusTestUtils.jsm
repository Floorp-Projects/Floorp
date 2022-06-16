/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
Cu.importGlobalProperties(["fetch"]);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  _ExperimentManager: "resource://nimbus/lib/ExperimentManager.jsm",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.jsm",
  ExperimentStore: "resource://nimbus/lib/ExperimentStore.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  NormandyUtils: "resource://normandy/lib/NormandyUtils.jsm",
  FileTestUtils: "resource://testing-common/FileTestUtils.jsm",
  _RemoteSettingsExperimentLoader:
    "resource://nimbus/lib/RemoteSettingsExperimentLoader.jsm",
  sinon: "resource://testing-common/Sinon.jsm",
  FeatureManifest: "resource://nimbus/FeatureManifest.js",
  JsonSchema: "resource://gre/modules/JsonSchema.jsm",
});

const {
  SYNC_DATA_PREF_BRANCH,
  SYNC_DEFAULTS_PREF_BRANCH,
} = lazy.ExperimentStore;

const PATH = lazy.FileTestUtils.getTempFile("shared-data-map").path;

async function fetchSchema(url) {
  const response = await fetch(url);
  const schema = await response.json();
  if (!schema) {
    throw new Error(`Failed to load ${url}`);
  }
  return schema;
}

const EXPORTED_SYMBOLS = ["ExperimentTestUtils", "ExperimentFakes"];

const ExperimentTestUtils = {
  _validateSchema(schema, value, errorMsg) {
    const result = lazy.JsonSchema.validate(value, schema, {
      shortCircuit: false,
    });
    if (result.errors.length) {
      throw new Error(
        `${errorMsg}: ${JSON.stringify(result.errors, undefined, 2)}`
      );
    }
    return value;
  },

  _validateFeatureValueEnum({ branch }) {
    let { features } = branch;
    for (let feature of features) {
      // If we're not using a real feature skip this check
      if (!lazy.FeatureManifest[feature.featureId]) {
        return true;
      }
      let { variables } = lazy.FeatureManifest[feature.featureId];
      for (let varName of Object.keys(variables)) {
        let varValue = feature.value[varName];
        if (
          varValue &&
          variables[varName].enum &&
          !variables[varName].enum.includes(varValue)
        ) {
          throw new Error(
            `${varName} should have one of the following values: ${JSON.stringify(
              variables[varName].enum
            )} but has value '${varValue}'`
          );
        }
      }
    }
    return true;
  },

  /**
   * Checks if an experiment is valid acording to existing schema
   */
  async validateExperiment(experiment) {
    const schema = await fetchSchema(
      "resource://nimbus/schemas/NimbusExperiment.schema.json"
    );

    // Ensure that the `featureIds` field is properly set
    const { branches } = experiment;
    branches.forEach(branch => {
      branch.features.map(({ featureId }) => {
        if (!experiment.featureIds.includes(featureId)) {
          throw new Error(
            `Branch(${branch.slug}) contains feature(${featureId}) but that's not declared in recipe(${experiment.slug}).featureIds`
          );
        }
      });
    });

    return this._validateSchema(
      schema,
      experiment,
      `Experiment ${experiment.slug} not valid`
    );
  },
  async validateEnrollment(enrollment) {
    const schema = await fetchSchema(
      "resource://nimbus/schemas/NimbusEnrollment.schema.json"
    );

    // We still have single feature experiment recipes for backwards
    // compatibility testing but we don't do schema validation
    if (!enrollment.branch.features && enrollment.branch.feature) {
      return true;
    }

    return (
      this._validateFeatureValueEnum(enrollment) &&
      this._validateSchema(
        schema,
        enrollment,
        `Enrollment ${enrollment.slug} is not valid`
      )
    );
  },
  async validateRollouts(rollout) {
    const schema = await fetchSchema(
      "resource://nimbus/schemas/NimbusEnrollment.schema.json"
    );

    return this._validateSchema(
      schema,
      rollout,
      `Rollout configuration ${rollout.slug} is not valid`
    );
  },
  /**
   * Add features for tests.
   *
   * These features will only be visible to the JS Nimbus client. The native
   * Nimbus client will have no access.
   *
   * @params features A list of |_NimbusFeature|s.
   *
   * @returns A cleanup function to remove the features once the test has completed.
   */
  addTestFeatures(...features) {
    for (const feature of features) {
      lazy.NimbusFeatures[feature.featureId] = feature;
    }
    return () => {
      for (const { featureId } of features) {
        delete lazy.NimbusFeatures[featureId];
      }
    };
  },
};

const ExperimentFakes = {
  manager(store) {
    let sandbox = lazy.sinon.createSandbox();
    let manager = new lazy._ExperimentManager({ store: store || this.store() });
    // We want calls to `store.addEnrollment` to implicitly validate the
    // enrollment before saving to store
    let origAddExperiment = manager.store.addEnrollment.bind(manager.store);
    sandbox.stub(manager.store, "addEnrollment").callsFake(async enrollment => {
      await ExperimentTestUtils.validateEnrollment(enrollment);
      return origAddExperiment(enrollment);
    });

    return manager;
  },
  store() {
    return new lazy.ExperimentStore("FakeStore", {
      path: PATH,
      isParent: true,
    });
  },
  waitForExperimentUpdate(ExperimentAPI, options) {
    if (!options) {
      throw new Error("Must specify an expected recipe update");
    }

    return new Promise(resolve => ExperimentAPI.on("update", options, resolve));
  },
  async enrollWithRollout(
    featureConfig,
    { manager = lazy.ExperimentManager, source } = {}
  ) {
    await manager.store.init();
    const rollout = this.rollout(`${featureConfig.featureId}-rollout`, {
      branch: {
        slug: `${featureConfig.featureId}-rollout-branch`,
        features: [featureConfig],
      },
    });
    if (source) {
      rollout.source = source;
    }
    await ExperimentTestUtils.validateRollouts(rollout);
    // After storing the remote configuration to store and updating the feature
    // we want to flush so that NimbusFeature usage in content process also
    // receives the update
    await manager.store.addEnrollment(rollout);
    manager.store._syncToChildren({ flush: true });

    let unenrollCompleted = slug =>
      new Promise(resolve =>
        manager.store.on(`update:${slug}`, (event, enrollment) => {
          if (enrollment.slug === rollout.slug && !enrollment.active) {
            manager.store._deleteForTests(rollout.slug);
            resolve();
          }
        })
      );

    return () => {
      let promise = unenrollCompleted(rollout.slug);
      manager.unenroll(rollout.slug, "cleanup");
      return promise;
    };
  },
  async enrollWithFeatureConfig(
    featureConfig,
    { manager = lazy.ExperimentManager } = {}
  ) {
    await manager.store.ready();
    // Use id passed in featureConfig value to compute experimentId
    // This help filter telemetry events (such as expose) in race conditions when telemetry
    // from multiple experiments with same featureId co-exist in snapshot
    let experimentId = `${featureConfig.featureId}${
      featureConfig?.value?.id ? "-" + featureConfig?.value?.id : ""
    }-experiment-${Math.random()}`;

    let recipe = this.recipe(experimentId, {
      bucketConfig: {
        namespace: "mstest-utils",
        randomizationUnit: "normandy_id",
        start: 0,
        count: 1000,
        total: 1000,
      },
      branches: [
        {
          slug: "control",
          ratio: 1,
          features: [featureConfig],
        },
      ],
    });
    let {
      enrollmentPromise,
      doExperimentCleanup,
    } = this.enrollmentHelper(recipe, { manager });

    await enrollmentPromise;

    return doExperimentCleanup;
  },
  enrollmentHelper(recipe = {}, { manager = lazy.ExperimentManager } = {}) {
    let enrollmentPromise = new Promise(resolve =>
      manager.store.on(`update:${recipe.slug}`, (event, experiment) => {
        if (experiment.active) {
          manager.store._syncToChildren({ flush: true });
          resolve(experiment);
        }
      })
    );
    let unenrollCompleted = slug =>
      new Promise(resolve =>
        manager.store.on(`update:${slug}`, (event, experiment) => {
          if (!experiment.active) {
            // Removes recipe from file storage which
            // (normally the users archive of past experiments)
            manager.store._deleteForTests(recipe.slug);
            resolve();
          }
        })
      );
    let doExperimentCleanup = async () => {
      for (let experiment of manager.store.getAllActive()) {
        let promise = unenrollCompleted(experiment.slug);
        manager.unenroll(experiment.slug, "cleanup");
        await promise;
      }
      if (manager.store.getAllActive().length) {
        throw new Error("Cleanup failed");
      }
    };

    if (recipe.slug) {
      if (!manager.store._isReady) {
        throw new Error("Manager store not ready, call `manager.onStartup`");
      }
      manager.enroll(recipe, "enrollmentHelper");
    }

    return { enrollmentPromise, doExperimentCleanup };
  },
  // Experiment store caches in prefs Enrollments for fast sync access
  cleanupStorePrefCache() {
    try {
      Services.prefs.deleteBranch(SYNC_DATA_PREF_BRANCH);
      Services.prefs.deleteBranch(SYNC_DEFAULTS_PREF_BRANCH);
    } catch (e) {
      // Expected if nothing is cached
    }
  },
  childStore() {
    return new lazy.ExperimentStore("FakeStore", { isParent: false });
  },
  rsLoader() {
    const loader = new lazy._RemoteSettingsExperimentLoader();
    // Replace RS client with a fake
    Object.defineProperty(loader, "remoteSettingsClient", {
      value: { get: () => Promise.resolve([]) },
    });
    // Replace xman with a fake
    loader.manager = this.manager();

    return loader;
  },
  experiment(slug, props = {}) {
    return {
      slug,
      active: true,
      enrollmentId: lazy.NormandyUtils.generateUuid(),
      branch: {
        slug: "treatment",
        features: [
          {
            featureId: "testFeature",
            value: { testInt: 123, enabled: true },
          },
        ],
        ...props,
      },
      source: "NimbusTestUtils",
      isEnrollmentPaused: true,
      experimentType: "NimbusTestUtils",
      userFacingName: "NimbusTestUtils",
      userFacingDescription: "NimbusTestUtils",
      lastSeen: new Date().toJSON(),
      featureIds: props?.branch?.features?.map(f => f.featureId) || [
        "testFeature",
      ],
      ...props,
    };
  },
  rollout(slug, props = {}) {
    return {
      slug,
      active: true,
      enrollmentId: lazy.NormandyUtils.generateUuid(),
      isRollout: true,
      branch: {
        slug: "treatment",
        features: [
          {
            featureId: "testFeature",
            value: { testInt: 123, enabled: true },
          },
        ],
        ...props,
      },
      source: "NimbusTestUtils",
      isEnrollmentPaused: true,
      experimentType: "rollout",
      userFacingName: "NimbusTestUtils",
      userFacingDescription: "NimbusTestUtils",
      lastSeen: new Date().toJSON(),
      featureIds: (props?.branch?.features || props?.features)?.map(
        f => f.featureId
      ) || ["testFeature"],
      ...props,
    };
  },
  recipe(slug = lazy.NormandyUtils.generateUuid(), props = {}) {
    return {
      // This field is required for populating remote settings
      id: lazy.NormandyUtils.generateUuid(),
      schemaVersion: "1.7.0",
      appName: "firefox_desktop",
      appId: "firefox-desktop",
      channel: "nightly",
      slug,
      isEnrollmentPaused: false,
      probeSets: [],
      startDate: null,
      endDate: null,
      proposedEnrollment: 7,
      referenceBranch: "control",
      application: "firefox-desktop",
      branches: [
        {
          slug: "control",
          ratio: 1,
          features: [
            {
              featureId: "testFeature",
              value: { testInt: 123, enabled: true },
            },
          ],
        },
        {
          slug: "treatment",
          ratio: 1,
          features: [
            {
              featureId: "testFeature",
              value: { testInt: 123, enabled: true },
            },
          ],
        },
      ],
      bucketConfig: {
        namespace: "nimbus-test-utils",
        randomizationUnit: "normandy_id",
        start: 0,
        count: 100,
        total: 1000,
      },
      userFacingName: "Nimbus recipe",
      userFacingDescription: "NimbusTestUtils recipe",
      featureIds: props?.branches?.[0].features?.map(f => f.featureId) || [
        "testFeature",
      ],
      ...props,
    };
  },
};
