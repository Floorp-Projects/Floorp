/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FeatureGate",
  "resource://featuregates/FeatureGate.jsm"
);

var EXPORTED_SYMBOLS = ["FeatureGateImplementation"];

/** An individual feature gate that can be re-used for more advanced usage. */
class FeatureGateImplementation {
  // Note that the following comment is *not* a jsdoc. Making it a jsdoc would
  // makes sphinx-js expose it to users. This feature shouldn't be used by
  // users, and so should not be in the docs. Sphinx-js does not respect the
  // @private marker on a constructor (https://github.com/erikrose/sphinx-js/issues/71).
  /*
   * This constructor should only be used directly in tests.
   * ``FeatureGate.fromId`` should be used instead for most cases.
   *
   * @private
   *
   * @param {object} definition Description of the feature gate.
   * @param {string} definition.id
   * @param {string} definition.title
   * @param {string} definition.description
   * @param {string} definition.descriptionLinks
   * @param {boolean} definition.restartRequired
   * @param {string} definition.type
   * @param {string} definition.preference
   * @param {string} definition.defaultValue
   * @param {object} definition.isPublic
   * @param {object} definition.bugNumbers
   */
  constructor(definition) {
    this._definition = definition;
    this._observers = new Set();
  }

  // The below are all getters instead of direct access to make it easy to provide JSDocs.

  /**
   * A short string used to refer to this feature in code.
   * @type string
   */
  get id() {
    return this._definition.id;
  }

  /**
   * A Fluent string ID that will resolve to some text to identify this feature to users.
   * @type string
   */
  get title() {
    return this._definition.title;
  }

  /**
   * A Fluent string ID that will resolve to a longer string to show to users that explains the feature.
   * @type string
   */
  get description() {
    return this._definition.description;
  }

  get descriptionLinks() {
    return this._definition.descriptionLinks;
  }

  /**
   * Whether this feature requires a browser restart to take effect after toggling.
   * @type boolean
   */
  get restartRequired() {
    return this._definition.restartRequired;
  }

  /**
   * The type of feature. Currently only booleans are supported. This may be
   * richer than JS types in the future, such as enum values.
   * @type string
   */
  get type() {
    return this._definition.type;
  }

  /**
   * The name of the preference that stores the value of this feature.
   *
   * This preference should not be read directly, but instead its values should
   * be accessed via FeatureGate#addObserver or FeatureGate#getValue. This
   * property is provided for backwards compatibility.
   *
   * @type string
   */
  get preference() {
    return this._definition.preference;
  }

  /**
   * The default value for the feature gate for this update channel.
   * @type boolean
   */
  get defaultValue() {
    return this._definition.defaultValue;
  }

  /** The default value before any targeting evaluation. */
  get defaultValueOriginalValue() {
    // This will probably be overwritten by the loader, but if not provide a default.
    return (
      this._definition.defaultValueOriginalValue || {
        default: this._definition.defaultValue,
      }
    );
  }

  /**
   * Check what the default value of this feature gate would be on another
   * browser with different facts, such as on another platform.
   *
   * @param {Map} extraFacts
   *   A `Map` of hypothetical facts to consider, such as {'windows': true} to
   *   check what the value of this feature would be on Windows.
   */
  defaultValueWith(extraFacts) {
    return FeatureGate.evaluateTargetedValue(
      this.defaultValueOriginalValue,
      extraFacts,
      { mergeFactsWithDefault: true }
    );
  }

  /**
   * If this feature should be exposed to users in an advanced settings panel
   * for this build of Firefox.
   *
   * @type boolean
   */
  get isPublic() {
    return this._definition.isPublic;
  }

  /** The isPublic before any targeting evaluation. */
  get isPublicOriginalValue() {
    // This will probably be overwritten by the loader, but if not provide a default.
    return (
      this._definition.isPublicOriginalValue || {
        default: this._definition.isPublic,
      }
    );
  }

  /**
   * Check if this feature is available on another browser with different
   * facts, such as on another platform.
   *
   * @param {Map} extraFacts
   *   A `Map` of hypothetical facts to consider, such as {'windows': true} to
   *   check if this feature would be available on Windows.
   */
  isPublicWith(extraFacts) {
    return FeatureGate.evaluateTargetedValue(
      this.isPublicOriginalValue,
      extraFacts,
      { mergeFactsWithDefault: true }
    );
  }

  /**
   * Bug numbers associated with this feature.
   * @type Array<number>
   */
  get bugNumbers() {
    return this._definition.bugNumbers;
  }

  /**
   * Get the current value of this feature gate. Implementors should avoid
   * storing the result to avoid missing changes to the feature's value.
   * Consider using :func:`addObserver` if it is necessary to store the value
   * of the feature.
   *
   * @async
   * @returns {Promise<boolean>} A promise for the value associated with this feature.
   */
  // Note that this is async for potential future use of a storage backend besides preferences.
  async getValue() {
    return Services.prefs.getBoolPref(this.preference, this.defaultValue);
  }

  /**
   * An alias of `getValue` for boolean typed feature gates.
   *
   * @async
   * @returns {Promise<boolean>} A promise for the value associated with this feature.
   * @throws {Error} If the feature is not a boolean.
   */
  // Note that this is async for potential future use of a storage backend besides preferences.
  async isEnabled() {
    if (this.type !== "boolean") {
      throw new Error(
        `Tried to call isEnabled when type is not boolean (it is ${this.type})`
      );
    }
    return this.getValue();
  }

  /**
   * Add an observer for changes to this feature. When the observer is added,
   * `onChange` will asynchronously be called with the current value of the
   * preference. If the feature is of type boolean and currently enabled,
   * `onEnable` will additionally be called.
   *
   * @param {object} observer Functions to be called when the feature changes.
   *        All observer functions are optional.
   * @param {Function()} [observer.onEnable] Called when the feature becomes enabled.
   * @param {Function()} [observer.onDisable] Called when the feature becomes disabled.
   * @param {Function(newValue: boolean)} [observer.onChange] Called when the
   *        feature's state changes to any value. The new value will be passed to the
   *        function.
   * @returns {Promise<boolean>} The current value of the feature.
   */
  async addObserver(observer) {
    if (this._observers.size === 0) {
      Services.prefs.addObserver(this.preference, this);
    }

    this._observers.add(observer);

    if (this.type === "boolean" && (await this.isEnabled())) {
      this._callObserverMethod(observer, "onEnable");
    }
    // onDisable should not be called, because features should be assumed
    // disabled until onEnabled is called for the first time.

    return this.getValue();
  }

  /**
   * Remove an observer of changes from this feature
   * @param observer The observer that was passed to addObserver to remove.
   */
  removeObserver(observer) {
    this._observers.delete(observer);
    if (this._observers.size === 0) {
      Services.prefs.removeObserver(this.preference, this);
    }
  }

  /**
   * Removes all observers from this instance of the feature gate.
   */
  removeAllObservers() {
    if (this._observers.size > 0) {
      this._observers.clear();
      Services.prefs.removeObserver(this.preference, this);
    }
  }

  _callObserverMethod(observer, method, ...args) {
    if (method in observer) {
      try {
        observer[method](...args);
      } catch (err) {
        Cu.reportError(err);
      }
    }
  }

  /**
   * Observes changes to the preference storing the enabled state of the
   * feature. The observer is dynamically added only when observer have been
   * added.
   * @private
   */
  async observe(aSubject, aTopic, aData) {
    if (aTopic === "nsPref:changed" && aData === this.preference) {
      const value = await this.getValue();
      for (const observer of this._observers) {
        this._callObserverMethod(observer, "onChange", value);

        if (value) {
          this._callObserverMethod(observer, "onEnable");
        } else {
          this._callObserverMethod(observer, "onDisable");
        }
      }
    } else {
      Cu.reportError(
        new Error(`Unexpected event observed: ${aSubject}, ${aTopic}, ${aData}`)
      );
    }
  }
}
