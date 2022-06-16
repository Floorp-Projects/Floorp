/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "FeatureGateImplementation",
  "resource://featuregates/FeatureGateImplementation.jsm"
);

var EXPORTED_SYMBOLS = ["FeatureGate"];

XPCOMUtils.defineLazyGetter(lazy, "gFeatureDefinitionsPromise", async () => {
  const url = "resource://featuregates/feature_definitions.json";
  return fetchFeatureDefinitions(url);
});

async function fetchFeatureDefinitions(url) {
  const res = await fetch(url);
  let definitionsJson = await res.json();
  return new Map(Object.entries(definitionsJson));
}

function buildFeatureGateImplementation(definition) {
  const targetValueKeys = ["defaultValue", "isPublic"];
  for (const key of targetValueKeys) {
    definition[`${key}OriginalValue`] = definition[key];
    definition[key] = FeatureGate.evaluateTargetedValue(definition[key]);
  }
  return new lazy.FeatureGateImplementation(definition);
}

let featureGatePrefObserver = {
  onChange() {
    FeatureGate.annotateCrashReporter();
  },
  // Ignore onEnable and onDisable since onChange is called in both cases.
  onEnable() {},
  onDisable() {},
};

const kFeatureGateCache = new Map();

/** A high level control for turning features on and off. */
class FeatureGate {
  /*
   * This is structured as a class with static methods to that sphinx-js can
   * easily document it. This constructor is required for sphinx-js to detect
   * this class for documentation.
   */

  constructor() {}

  /**
   * Constructs a feature gate object that is defined in ``Features.toml``.
   * This is the primary way to create a ``FeatureGate``.
   *
   * @param {string} id The ID of the feature's definition in `Features.toml`.
   * @param {string} testDefinitionsUrl A URL from which definitions can be fetched. Only use this in tests.
   * @throws If the ``id`` passed is not defined in ``Features.toml``.
   */
  static async fromId(id, testDefinitionsUrl = undefined) {
    let featureDefinitions;
    if (testDefinitionsUrl) {
      featureDefinitions = await fetchFeatureDefinitions(testDefinitionsUrl);
    } else {
      featureDefinitions = await lazy.gFeatureDefinitionsPromise;
    }

    if (!featureDefinitions.has(id)) {
      throw new Error(
        `Unknown feature id ${id}. Features must be defined in toolkit/components/featuregates/Features.toml`
      );
    }

    // Make a copy of the definition, since we are about to modify it
    return buildFeatureGateImplementation({ ...featureDefinitions.get(id) });
  }

  /**
   * Constructs feature gate objects for each of the definitions in ``Features.toml``.
   * @param {string} testDefinitionsUrl A URL from which definitions can be fetched. Only use this in tests.
   */
  static async all(testDefinitionsUrl = undefined) {
    let featureDefinitions;
    if (testDefinitionsUrl) {
      featureDefinitions = await fetchFeatureDefinitions(testDefinitionsUrl);
    } else {
      featureDefinitions = await lazy.gFeatureDefinitionsPromise;
    }

    let definitions = [];
    for (let definition of featureDefinitions.values()) {
      // Make a copy of the definition, since we are about to modify it
      definitions[definitions.length] = buildFeatureGateImplementation(
        Object.assign({}, definition)
      );
    }
    return definitions;
  }

  static async observePrefChangesForCrashReportAnnotation(
    testDefinitionsUrl = undefined
  ) {
    let featureDefinitions = await FeatureGate.all(testDefinitionsUrl);

    for (let definition of featureDefinitions.values()) {
      FeatureGate.addObserver(
        definition.id,
        featureGatePrefObserver,
        testDefinitionsUrl
      );
    }
  }

  static async annotateCrashReporter() {
    let crashReporter = Cc["@mozilla.org/toolkit/crash-reporter;1"].getService(
      Ci.nsICrashReporter
    );
    if (!crashReporter?.enabled) {
      return;
    }
    let features = await FeatureGate.all();
    let enabledFeatures = [];
    for (let feature of features) {
      if (await feature.getValue()) {
        enabledFeatures.push(feature.preference);
      }
    }
    crashReporter.annotateCrashReport(
      "ExperimentalFeatures",
      enabledFeatures.join(",")
    );
  }

  /**
   * Add an observer for a feature gate by ID. If the feature is of type
   * boolean and currently enabled, `onEnable` will be called.
   *
   * The underlying feature gate instance will be shared with all other callers
   * of this function, and share an observer.
   *
   * @param {string} id The ID of the feature's definition in `Features.toml`.
   * @param {object} observer Functions to be called when the feature changes.
   *        All observer functions are optional.
   * @param {Function()} [observer.onEnable] Called when the feature becomes enabled.
   * @param {Function()} [observer.onDisable] Called when the feature becomes disabled.
   * @param {Function(newValue)} [observer.onChange] Called when the
   *        feature's state changes to any value. The new value will be passed to the
   *        function.
   * @param {string} testDefinitionsUrl A URL from which definitions can be fetched. Only use this in tests.
   * @returns {Promise<boolean>} The current value of the feature.
   */
  static async addObserver(id, observer, testDefinitionsUrl = undefined) {
    if (!kFeatureGateCache.has(id)) {
      kFeatureGateCache.set(
        id,
        await FeatureGate.fromId(id, testDefinitionsUrl)
      );
    }
    const feature = kFeatureGateCache.get(id);
    return feature.addObserver(observer);
  }

  /**
   * Remove an observer of changes from this feature
   * @param {string} id The ID of the feature's definition in `Features.toml`.
   * @param observer Then observer that was passed to addObserver to remove.
   */
  static async removeObserver(id, observer) {
    let feature = kFeatureGateCache.get(id);
    if (!feature) {
      return;
    }
    feature.removeObserver(observer);
    if (feature._observers.size === 0) {
      kFeatureGateCache.delete(id);
    }
  }

  /**
   * Get the current value of this feature gate. Implementors should avoid
   * storing the result to avoid missing changes to the feature's value.
   * Consider using :func:`addObserver` if it is necessary to store the value
   * of the feature.
   *
   * @async
   * @param {string} id The ID of the feature's definition in `Features.toml`.
   * @returns {Promise<boolean>} A promise for the value associated with this feature.
   */
  static async getValue(id, testDefinitionsUrl = undefined) {
    let feature = kFeatureGateCache.get(id);
    if (!feature) {
      feature = await FeatureGate.fromId(id, testDefinitionsUrl);
    }
    return feature.getValue();
  }

  /**
   * An alias of `getValue` for boolean typed feature gates.
   *
   * @async
   * @param {string} id The ID of the feature's definition in `Features.toml`.
   * @returns {Promise<boolean>} A promise for the value associated with this feature.
   * @throws {Error} If the feature is not a boolean.
   */
  static async isEnabled(id, testDefinitionsUrl = undefined) {
    let feature = kFeatureGateCache.get(id);
    if (!feature) {
      feature = await FeatureGate.fromId(id, testDefinitionsUrl);
    }
    return feature.isEnabled();
  }

  static targetingFacts = new Map([
    [
      "release",
      AppConstants.MOZ_UPDATE_CHANNEL === "release" || AppConstants.IS_ESR,
    ],
    ["beta", AppConstants.MOZ_UPDATE_CHANNEL === "beta"],
    ["early_beta_or_earlier", AppConstants.EARLY_BETA_OR_EARLIER],
    ["dev-edition", AppConstants.MOZ_DEV_EDITION],
    ["nightly", AppConstants.NIGHTLY_BUILD],
    ["win", AppConstants.platform === "win"],
    ["mac", AppConstants.platform === "macosx"],
    ["linux", AppConstants.platform === "linux"],
    ["android", AppConstants.platform === "android"],
  ]);

  /**
   * Take a map of conditions to values, and return the value who's conditions
   * match this browser, or the default value in the map.
   *
   * @example
   *   Calling the function as
   *
   *       evaluateTargetedValue({"default": false, "nightly,linux": true})
   *
   *   would return true on Nightly on Linux, and false otherwise.
   *
   * @param {Object} targetedValue
   *   An object mapping string conditions to values. The conditions are comma
   *   separated values specified in `targetingFacts`. A condition "default" is
   *   required, as the fallback valued. All conditions must be true.
   *
   *   If multiple values have conditions that match, then an arbitrary one will
   *   be returned. Specifically, the one returned first in an `Object.entries`
   *   iteration of the targetedValue.
   *
   * @param {Map} [targetingFacts]
   *   A map of target facts to use. Defaults to facts about the current browser.
   *
   * @param {boolean} [options.mergeFactsWithDefault]
   *   If set to true, the value passed for `targetingFacts` will be merged with
   *   the default facts.
   *
   * @returns A value from `targetedValue`.
   */
  static evaluateTargetedValue(
    targetedValue,
    targetingFacts = FeatureGate.targetingFacts,
    { mergeFactsWithDefault = false } = {}
  ) {
    if (!Object.hasOwnProperty.call(targetedValue, "default")) {
      throw new Error(
        `Targeted value ${JSON.stringify(targetedValue)} has no default key`
      );
    }

    if (mergeFactsWithDefault) {
      const combinedFacts = new Map(FeatureGate.targetingFacts);
      for (const [key, value] of targetingFacts.entries()) {
        combinedFacts.set(key, value);
      }
      targetingFacts = combinedFacts;
    }

    for (const [key, value] of Object.entries(targetedValue)) {
      if (key === "default") {
        continue;
      }
      if (key.split(",").every(part => targetingFacts.get(part))) {
        return value;
      }
    }

    return targetedValue.default;
  }
}
