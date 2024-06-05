/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  // This is an unfortunate exception where we depend on ASRouter because
  // Nimbus has this dependency.
  // This implementation is written in a way where it will avoid requiring
  // this module if it's not available.
  ASRouterTargeting:
    // eslint-disable-next-line mozilla/no-browser-refs-in-toolkit
    "resource:///modules/asrouter/ASRouterTargeting.sys.mjs",
  FeatureGateImplementation:
    "resource://featuregates/FeatureGateImplementation.sys.mjs",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.sys.mjs",
  TargetingContext: "resource://messaging-system/targeting/Targeting.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "gFeatureDefinitionsPromise", async () => {
  const url = "resource://featuregates/feature_definitions.json";
  return fetchFeatureDefinitions(url);
});

const kCustomTargeting = {
  // For default values, although something like `channel == 'nightly'` kinda
  // works, local builds don't have that update channel set in that way so it
  // doesn't, and then tests fail because the defaults for the FeatureGate
  // do not match the default value in the prefs code.
  // We may in future want other things from AppConstants here, too.
  nightly_build: AppConstants.NIGHTLY_BUILD,
  thunderbird: AppConstants.MOZ_APP_NAME == "thunderbird",
};

ChromeUtils.defineLazyGetter(lazy, "defaultContexts", () => {
  let ASRouterEnv = {};
  try {
    ASRouterEnv = lazy.ASRouterTargeting.Environment;
  } catch (ex) {
    // No ASRouter; just keep going.
  }
  return [
    kCustomTargeting,
    lazy.ExperimentManager.createTargetingContext(),
    ASRouterEnv,
  ];
});

function getCombinedContext(...contexts) {
  let combined = lazy.TargetingContext.combineContexts(
    ...lazy.defaultContexts,
    ...contexts
  );
  return new lazy.TargetingContext(combined, {
    source: "featuregate",
  });
}

async function fetchFeatureDefinitions(url) {
  const res = await fetch(url);
  let definitionsJson = await res.json();
  return new Map(Object.entries(definitionsJson));
}

async function buildFeatureGateImplementation(definition) {
  const targetValueKeys = ["defaultValue", "isPublic"];
  for (const key of targetValueKeys) {
    definition[key] = await FeatureGate.evaluateJexlValue(
      definition[key + "Jexl"]
    );
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
export class FeatureGate {
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
      definitions[definitions.length] = await buildFeatureGateImplementation(
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
    if (!Services.appinfo.crashReporterEnabled) {
      return;
    }
    let features = await FeatureGate.all();
    let enabledFeatures = [];
    for (let feature of features) {
      if (await feature.getValue()) {
        enabledFeatures.push(feature.preference);
      }
    }
    Services.appinfo.annotateCrashReport(
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

  /**
   * Take a jexl expression and evaluate it against the standard Nimbus
   * context, extended with some additional properties defined in
   * kCustomTargeting.
   *
   * @param {String} jexlExpression The expression to evaluate.
   * @param {Object[]?} additionalContexts Any additional context properties
   *                                    that should be taken into account.
   *
   * @returns {Promise<boolean>} Resolves to either true or false if successful,
   *          or null if there was some problem with the jexl expression (which
   *          will also log an error to the console).
   */
  static async evaluateJexlValue(jexlExpression, ...additionalContexts) {
    let result = null;
    let context = getCombinedContext(...additionalContexts);
    try {
      result = !!(await context.evalWithDefault(jexlExpression));
    } catch (ex) {
      console.error(ex);
    }
    return result;
  }
}
