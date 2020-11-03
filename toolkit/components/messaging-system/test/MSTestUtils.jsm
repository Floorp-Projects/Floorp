/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.importGlobalProperties(["fetch"]);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  _ExperimentManager:
    "resource://messaging-system/experiments/ExperimentManager.jsm",
  ExperimentStore:
    "resource://messaging-system/experiments/ExperimentStore.jsm",
  NormandyUtils: "resource://normandy/lib/NormandyUtils.jsm",
  FileTestUtils: "resource://testing-common/FileTestUtils.jsm",
  _RemoteSettingsExperimentLoader:
    "resource://messaging-system/lib/RemoteSettingsExperimentLoader.jsm",
  Ajv: "resource://testing-common/ajv-4.1.1.js",
});

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
          featureId: "aboutwelcome",
          enabled: true,
          value: { title: "hello" },
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
          feature: { featureId: "aboutwelcome", enabled: true, value: null },
        },
        {
          slug: "treatment",
          ratio: 1,
          feature: {
            featureId: "aboutwelcome",
            enabled: true,
            value: { title: "hello" },
          },
        },
      ],
      bucketConfig: {
        namespace: "mstest-utils",
        randomizationUnit: "normandy_id",
        start: 0,
        count: 100,
        total: 1000,
      },
      userFacingName: "Messaging System recipe",
      userFacingDescription: "Messaging System MSTestUtils recipe",
      ...props,
    };
  },
};
