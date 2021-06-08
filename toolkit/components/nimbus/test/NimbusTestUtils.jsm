/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
Cu.importGlobalProperties(["fetch"]);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  _ExperimentManager: "resource://nimbus/lib/ExperimentManager.jsm",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.jsm",
  ExperimentStore: "resource://nimbus/lib/ExperimentStore.jsm",
  NormandyUtils: "resource://normandy/lib/NormandyUtils.jsm",
  FileTestUtils: "resource://testing-common/FileTestUtils.jsm",
  _RemoteSettingsExperimentLoader:
    "resource://nimbus/lib/RemoteSettingsExperimentLoader.jsm",
  Ajv: "resource://testing-common/ajv-4.1.1.js",
});

const { SYNC_DATA_PREF_BRANCH, SYNC_DEFAULTS_PREF_BRANCH } = ExperimentStore;

const PATH = FileTestUtils.getTempFile("shared-data-map").path;

XPCOMUtils.defineLazyGetter(this, "fetchExperimentSchema", async () => {
  const response = await fetch(
    "resource://testing-common/NimbusExperiment.schema.json"
  );
  const schema = await response.json();
  if (!schema) {
    throw new Error("Failed to load NimbusSchema");
  }
  return schema.definitions.NimbusExperiment;
});

const EXPORTED_SYMBOLS = ["ExperimentTestUtils", "ExperimentFakes"];

const ExperimentTestUtils = {
  /**
   * Checks if an experiment is valid acording to existing schema
   * @param {NimbusExperiment} experiment
   */
  async validateExperiment(experiment) {
    const schema = await fetchExperimentSchema;
    const ajv = new Ajv({ async: "co*", allErrors: true });
    const validator = ajv.compile(schema);
    validator(experiment);
    if (validator.errors?.length) {
      throw new Error(
        "Experiment not valid:" + JSON.stringify(validator.errors, undefined, 2)
      );
    }
    return experiment;
  },
};

const ExperimentFakes = {
  manager(store) {
    return new _ExperimentManager({ store: store || this.store() });
  },
  store() {
    return new ExperimentStore("FakeStore", { path: PATH, isParent: true });
  },
  waitForExperimentUpdate(ExperimentAPI, options) {
    if (!options) {
      throw new Error("Must specify an expected recipe update");
    }

    return new Promise(resolve => ExperimentAPI.on("update", options, resolve));
  },
  remoteDefaultsHelper({
    feature,
    store = ExperimentManager.store,
    configuration,
  }) {
    if (!store._isReady) {
      throw new Error("Store not ready, need to `await ExperimentAPI.ready()`");
    }
    store.updateRemoteConfigs(feature.featureId, configuration);

    return feature.ready();
  },
  async enrollWithFeatureConfig(
    featureConfig,
    { manager = ExperimentManager } = {}
  ) {
    await manager.store.ready();
    let recipe = this.recipe(
      `${featureConfig.featureId}-experiment-${Math.random()}`,
      {
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
            feature: featureConfig,
          },
        ],
      }
    );
    let {
      enrollmentPromise,
      doExperimentCleanup,
    } = this.enrollmentHelper(recipe, { manager });

    await enrollmentPromise;

    return doExperimentCleanup;
  },
  enrollmentHelper(recipe = {}, { manager = ExperimentManager } = {}) {
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
      manager.enroll(recipe);
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
    return new ExperimentStore("FakeStore", { isParent: false });
  },
  rsLoader() {
    const loader = new _RemoteSettingsExperimentLoader();
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
      enrollmentId: NormandyUtils.generateUuid(),
      branch: {
        slug: "treatment",
        feature: {
          featureId: "test-feature",
          value: { title: "hello", enabled: true },
        },
        ...props,
      },
      source: "test",
      isEnrollmentPaused: true,
      ...props,
    };
  },
  recipe(slug = NormandyUtils.generateUuid(), props = {}) {
    return {
      // This field is required for populating remote settings
      id: NormandyUtils.generateUuid(),
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
          feature: { featureId: "test-feature", value: { enabled: true } },
        },
        {
          slug: "treatment",
          ratio: 1,
          feature: {
            featureId: "test-feature",
            value: { title: "hello", enabled: true },
          },
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
      ...props,
    };
  },
};
