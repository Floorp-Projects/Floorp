/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FeatureGateImplementation",
  "resource://featuregates/FeatureGateImplementation.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

var EXPORTED_SYMBOLS = ["FeatureGate"];

XPCOMUtils.defineLazyGetter(this, "gFeatureDefinitionsPromise", async () => {
  const url = "resource://featuregates/feature_definitions.json";
  return fetchFeatureDefinitions(url);
});

const kTargetFacts = new Map([
  ["release", AppConstants.MOZ_UPDATE_CHANNEL === "release"],
  ["beta", AppConstants.MOZ_UPDATE_CHANNEL === "beta"],
  ["dev-edition", AppConstants.MOZ_UPDATE_CHANNEL === "aurora"],
  ["nightly", AppConstants.MOZ_UPDATE_CHANNEL === "nightly"],
  ["win", AppConstants.platform === "win"],
  ["mac", AppConstants.platform === "macosx"],
  ["linux", AppConstants.platform === "linux"],
  ["android", AppConstants.platform === "android"],
]);

async function fetchFeatureDefinitions(url) {
  const res = await fetch(url);
  let definitionsJson = await res.json();
  return new Map(Object.entries(definitionsJson));
}

/**
 * Take a map of conditions to values, and return the value who's conditions
 * match this browser, or the default value in the map.
 *
 * @example `evaluateTargetedValue({default: false, nightly: true})` would
 *          return true on Nightly, and false otherwise.
 * @param {Object} targetedValue An object mapping string conditions to values. The
 *                 conditions are comma separated values such as those specified
 *                 in `kTargetFacts` above. A condition "default" is required, as
 *                 the fallback valued.
 * @param {Map} targetingFacts A map of target facts to use, such as `kTargetFacts`.
 * @returns A value from `targetedValue`.
 */
function evaluateTargetedValue(targetedValue, targetingFacts) {
  if (!Object.hasOwnProperty.call(targetedValue, "default")) {
    throw new Error(
      `Targeted value ${JSON.stringify(targetedValue)} has no default key`
    );
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
      featureDefinitions = await gFeatureDefinitionsPromise;
    }

    if (!featureDefinitions.has(id)) {
      throw new Error(
        `Unknown feature id ${id}. Features must be defined in toolkit/components/featuregates/Features.toml`
      );
    }

    // Make a copy of the definition, since we are about to modify it
    const definition = { ...featureDefinitions.get(id) };
    const targetValueKeys = ["defaultValue", "isPublic"];
    for (const key of targetValueKeys) {
      definition[key] = evaluateTargetedValue(definition[key], kTargetFacts);
    }
    return new FeatureGateImplementation(definition);
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
}
