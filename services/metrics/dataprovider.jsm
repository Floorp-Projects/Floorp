/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

#ifndef MERGED_COMPARTMENT

this.EXPORTED_SYMBOLS = [
  "Measurement",
  "Provider",
];

const {utils: Cu} = Components;

const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;

#endif

Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/preferences.js");
Cu.import("resource://services-common/utils.js");



/**
 * Represents a collection of related pieces/fields of data.
 *
 * This is an abstract base type. Providers implement child types that
 * implement core functions such as `registerStorage`.
 *
 * This type provides the primary interface for storing, retrieving, and
 * serializing data.
 *
 * Each derived type must define a `name` and `version` property. These must be
 * a string name and integer version, respectively. The `name` is used to
 * identify the measurement within a `Provider`. The version is to denote the
 * behavior of the `Measurement` and the composition of its fields over time.
 * When a new field is added or the behavior of an existing field changes
 * (perhaps the method for storing it has changed), the version should be
 * incremented.
 *
 * Each measurement consists of a set of named fields. Each field is primarily
 * identified by a string name, which must be unique within the measurement.
 *
 * For fields backed by the SQLite metrics storage backend, fields must have a
 * strongly defined type. Valid types include daily counters, daily discrete
 * text values, etc. See `MetricsStorageSqliteBackend.FIELD_*`.
 *
 * FUTURE: provide hook points for measurements to supplement with custom
 * storage needs.
 */
this.Measurement = function () {
  if (!this.name) {
    throw new Error("Measurement must have a name.");
  }

  if (!this.version) {
    throw new Error("Measurement must have a version.");
  }

  if (!Number.isInteger(this.version)) {
    throw new Error("Measurement's version must be an integer: " + this.version);
  }

  this._log = Log4Moz.repository.getLogger("Services.Metrics.Measurement." + this.name);

  this.id = null;
  this.storage = null;
  this._fieldsByName = new Map();

  this._serializers = {};
  this._serializers[this.SERIALIZE_JSON] = {
    singular: this._serializeJSONSingular.bind(this),
    daily: this._serializeJSONDay.bind(this),
  };
}

Measurement.prototype = Object.freeze({
  SERIALIZE_JSON: "json",

  /**
   * Configures the storage backend so that it can store this measurement.
   *
   * Implementations must return a promise which is resolved when storage has
   * been configured.
   *
   * Most implementations will typically call into this.registerStorageField()
   * to configure fields in storage.
   *
   * FUTURE: Provide method for upgrading from older measurement versions.
   */
  configureStorage: function () {
    throw new Error("configureStorage() must be implemented.");
  },

  /**
   * Obtain a serializer for this measurement.
   *
   * Implementations should return an object with the following keys:
   *
   *   singular -- Serializer for singular data.
   *   daily -- Serializer for daily data.
   *
   * Each item is a function that takes a single argument: the data to
   * serialize. The passed data is a subset of that returned from
   * this.getValues(). For "singular," data.singular is passed. For "daily",
   * data.days.get(<day>) is passed.
   *
   * This function receives a single argument: the serialization format we
   * are requesting. This is one of the SERIALIZE_* constants on this base type.
   *
   * For SERIALIZE_JSON, the function should return an object that
   * JSON.stringify() knows how to handle. This could be an anonymous object or
   * array or any object with a property named `toJSON` whose value is a
   * function. The returned object will be added to a larger document
   * containing the results of all `serialize` calls.
   *
   * The default implementation knows how to serialize built-in types using
   * very simple logic. If small encoding size is a goal, the default
   * implementation may not be suitable. If an unknown field type is
   * encountered, the default implementation will error.
   *
   * @param format
   *        (string) A SERIALIZE_* constant defining what serialization format
   *        to use.
   */
  serializer: function (format) {
    if (!(format in this._serializers)) {
      throw new Error("Don't know how to serialize format: " + format);
    }

    return this._serializers[format];
  },

  hasField: function (name) {
    return this._fieldsByName.has(name);
  },

  fieldID: function (name) {
    let entry = this._fieldsByName.get(name);

    if (!entry) {
      throw new Error("Unknown field: " + name);
    }

    return entry[0];
  },

  fieldType: function (name) {
    let entry = this._fieldsByName.get(name);

    if (!entry) {
      throw new Error("Unknown field: " + name);
    }

    return entry[1];
  },

  /**
   * Register a named field with storage that's attached to this measurement.
   *
   * This is typically called during `configureStorage`. The `Measurement`
   * implementation passes the field name and its type (one of the
   * storage.FIELD_* constants). The storage backend then allocates space
   * for this named field. A side-effect of calling this is that the field's
   * storage ID is stored in this._fieldsByName and subsequent calls to the
   * storage modifiers below will know how to reference this field in the
   * storage backend.
   *
   * @param name
   *        (string) The name of the field being registered.
   * @param type
   *        (string) A field type name. This is typically one of the
   *        storage.FIELD_* constants. It could also be a custom type
   *        (presumably registered by this measurement or provider).
   */
  registerStorageField: function (name, type) {
    this._log.debug("Registering field: " + name + " " + type);

    let deferred = Promise.defer();

    let self = this;
    this.storage.registerField(this.id, name, type).then(
      function onSuccess(id) {
        self._fieldsByName.set(name, [id, type]);
        deferred.resolve();
      }, deferred.reject);

    return deferred.promise;
  },

  incrementDailyCounter: function (field, date=new Date()) {
    return this.storage.incrementDailyCounterFromFieldID(this.fieldID(field),
                                                         date);
  },

  addDailyDiscreteNumeric: function (field, value, date=new Date()) {
    return this.storage.addDailyDiscreteNumericFromFieldID(
                          this.fieldID(field), value, date);
  },

  addDailyDiscreteText: function (field, value, date=new Date()) {
    return this.storage.addDailyDiscreteTextFromFieldID(
                          this.fieldID(field), value, date);
  },

  setLastNumeric: function (field, value, date=new Date()) {
    return this.storage.setLastNumericFromFieldID(this.fieldID(field), value,
                                                  date);
  },

  setLastText: function (field, value, date=new Date()) {
    return this.storage.setLastTextFromFieldID(this.fieldID(field), value,
                                               date);
  },

  setDailyLastNumeric: function (field, value, date=new Date()) {
    return this.storage.setDailyLastNumericFromFieldID(this.fieldID(field),
                                                       value, date);
  },

  setDailyLastText: function (field, value, date=new Date()) {
    return this.storage.setDailyLastTextFromFieldID(this.fieldID(field),
                                                    value, date);
  },

  /**
   * Obtain all values stored for this measurement.
   *
   * The default implementation obtains all known types from storage. If the
   * measurement provides custom types or stores values somewhere other than
   * storage, it should define its own implementation.
   *
   * This returns a promise that resolves to a data structure which is
   * understood by the measurement's serialize() function.
   */
  getValues: function () {
    return this.storage.getMeasurementValues(this.id);
  },

  deleteLastNumeric: function (field) {
    return this.storage.deleteLastNumericFromFieldID(this.fieldID(field));
  },

  deleteLastText: function (field) {
    return this.storage.deleteLastTextFromFieldID(this.fieldID(field));
  },

  _serializeJSONSingular: function (data) {
    let result = {"_v": this.version};

    for (let [field, data] of data) {
      // There could be legacy fields in storage we no longer care about.
      if (!this._fieldsByName.has(field)) {
        continue;
      }

      let type = this.fieldType(field);

      switch (type) {
        case this.storage.FIELD_LAST_NUMERIC:
        case this.storage.FIELD_LAST_TEXT:
          result[field] = data[1];
          break;

        case this.storage.FIELD_DAILY_COUNTER:
        case this.storage.FIELD_DAILY_DISCRETE_NUMERIC:
        case this.storage.FIELD_DAILY_DISCRETE_TEXT:
        case this.storage.FIELD_DAILY_LAST_NUMERIC:
        case this.storage.FIELD_DAILY_LAST_TEXT:
          continue;

        default:
          throw new Error("Unknown field type: " + type);
      }
    }

    return result;
  },

  _serializeJSONDay: function (data) {
    let result = {"_v": this.version};

    for (let [field, data] of data) {
      if (!this._fieldsByName.has(field)) {
        continue;
      }

      let type = this.fieldType(field);

      switch (type) {
        case this.storage.FIELD_DAILY_COUNTER:
        case this.storage.FIELD_DAILY_DISCRETE_NUMERIC:
        case this.storage.FIELD_DAILY_DISCRETE_TEXT:
        case this.storage.FIELD_DAILY_LAST_NUMERIC:
        case this.storage.FIELD_DAILY_LAST_TEXT:
          result[field] = data;
          break;

        case this.storage.FIELD_LAST_NUMERIC:
        case this.storage.FIELD_LAST_TEXT:
          continue;

        default:
          throw new Error("Unknown field type: " + type);
      }
    }

    return result;
  },
});


/**
 * An entity that emits data.
 *
 * A `Provider` consists of a string name (must be globally unique among all
 * known providers) and a set of `Measurement` instances.
 *
 * The main role of a `Provider` is to produce metrics data and to store said
 * data in the storage backend.
 *
 * Metrics data collection is initiated either by a collector calling a
 * `collect*` function on `Provider` instances or by the `Provider` registering
 * to some external event and then reacting whenever they occur.
 *
 * `Provider` implementations interface directly with a storage backend. For
 * common stored values (daily counters, daily discrete values, etc),
 * implementations should interface with storage via the various helper
 * functions on the `Measurement` instances. For custom stored value types,
 * implementations will interact directly with the low-level storage APIs.
 *
 * Because multiple providers exist and could be responding to separate
 * external events simultaneously and because not all operations performed by
 * storage can safely be performed in parallel, writing directly to storage at
 * event time is dangerous. Therefore, interactions with storage must be
 * deferred until it is safe to perform them.
 *
 * This typically looks something like:
 *
 *   // This gets called when an external event worthy of recording metrics
 *   // occurs. The function receives a numeric value associated with the event.
 *   function onExternalEvent (value) {
 *     let now = new Date();
 *     let m = this.getMeasurement("foo", 1);
 *
 *     this.enqueueStorageOperation(function storeExternalEvent() {
 *
 *       // We interface with storage via the `Measurement` helper functions.
 *       // These each return a promise that will be resolved when the
 *       // operation finishes. We rely on behavior of storage where operations
 *       // are executed single threaded and sequentially. Therefore, we only
 *       // need to return the final promise.
 *       m.incrementDailyCounter("foo", now);
 *       return m.addDailyDiscreteNumericValue("my_value", value, now);
 *     }.bind(this));
 *
 *   }
 *
 *
 * `Provider` is an abstract base class. Implementations must define a few
 * properties:
 *
 *   name
 *     The `name` property should be a string defining the provider's name. The
 *     name must be globally unique for the application. The name is used as an
 *     identifier to distinguish providers from each other.
 *
 *   measurementTypes
 *     This must be an array of `Measurement`-derived types. Note that elements
 *     in the array are the type functions, not instances. Instances of the
 *     `Measurement` are created at run-time by the `Provider` and are bound
 *     to the provider and to a specific storage backend.
 */
this.Provider = function () {
  if (!this.name) {
    throw new Error("Provider must define a name.");
  }

  if (!Array.isArray(this.measurementTypes)) {
    throw new Error("Provider must define measurement types.");
  }

  this._log = Log4Moz.repository.getLogger("Services.Metrics.Provider." + this.name);

  this.measurements = null;
  this.storage = null;
}

Provider.prototype = Object.freeze({
  /**
   * Whether the provider provides only constant data.
   *
   * If this is true, the provider likely isn't instantiated until
   * `collectConstantData` is called and the provider may be torn down after
   * this function has finished.
   *
   * This is an optimization so provider instances aren't dead weight while the
   * application is running.
   *
   * This must be set on the prototype for the optimization to be realized.
   */
  constantOnly: false,

  /**
   * Obtain a `Measurement` from its name and version.
   *
   * If the measurement is not found, an Error is thrown.
   */
  getMeasurement: function (name, version) {
    if (!Number.isInteger(version)) {
      throw new Error("getMeasurement expects an integer version. Got: " + version);
    }

    let m = this.measurements.get([name, version].join(":"));

    if (!m) {
      throw new Error("Unknown measurement: " + name + " v" + version);
    }

    return m;
  },

  /**
   * Initializes preferences storage for this provider.
   *
   * Providers are allocated preferences storage under a pref branch named
   * after the provider.
   *
   * This function is typically only called by the entity that constructs the
   * Provider instance.
   */
  initPreferences: function (branchParent) {
    if (!branchParent.endsWith(".")) {
      throw new Error("branchParent must end with '.': " + branchParent);
    }

    this._prefs = new Preferences(branchParent + this.name + ".");
  },

  init: function (storage) {
    if (this.storage !== null) {
      throw new Error("Provider() not called. Did the sub-type forget to call it?");
    }

    if (this.storage) {
      throw new Error("Provider has already been initialized.");
    }

    this.measurements = new Map();
    this.storage = storage;

    let self = this;
    return Task.spawn(function init() {
      for (let measurementType of self.measurementTypes) {
        let measurement = new measurementType();

        measurement.provider = self;
        measurement.storage = self.storage;

        let id = yield storage.registerMeasurement(self.name, measurement.name,
                                                   measurement.version);

        measurement.id = id;

        yield measurement.configureStorage();

        self.measurements.set([measurement.name, measurement.version].join(":"),
                              measurement);
      }

      let promise = self.onInit();

      if (!promise || typeof(promise.then) != "function") {
        throw new Error("onInit() does not return a promise.");
      }

      yield promise;
    });
  },

  shutdown: function () {
    let promise = this.onShutdown();

    if (!promise || typeof(promise.then) != "function") {
      throw new Error("onShutdown implementation does not return a promise.");
    }

    return promise;
  },

  /**
   * Hook point for implementations to perform initialization activity.
   *
   * If a `Provider` instance needs to register observers, etc, it should
   * implement this function.
   *
   * Implementations should return a promise which is resolved when
   * initialization activities have completed.
   */
  onInit: function () {
    return Promise.resolve();
  },

  /**
   * Hook point for shutdown of instances.
   *
   * This is the opposite of `onInit`. If a `Provider` needs to unregister
   * observers, etc, this is where it should do it.
   *
   * Implementations should return a promise which is resolved when
   * shutdown activities have completed.
   */
  onShutdown: function () {
    return Promise.resolve();
  },

  /**
   * Collects data that doesn't change during the application's lifetime.
   *
   * Implementations should return a promise that resolves when all data has
   * been collected and storage operations have been finished.
   *
   * @return Promise<>
   */
  collectConstantData: function () {
    return Promise.resolve();
  },

  /**
   * Collects data approximately every day.
   *
   * For long-running applications, this is called approximately every day.
   * It may or may not be called every time the application is run. It also may
   * be called more than once per day.
   *
   * Implementations should return a promise that resolves when all data has
   * been collected and storage operations have completed.
   *
   * @return Promise<>
   */
  collectDailyData: function () {
    return Promise.resolve();
  },

  /**
   * Queue a deferred storage operation.
   *
   * Deferred storage operations are the preferred method for providers to
   * interact with storage. When collected data is to be added to storage,
   * the provider creates a function that performs the necessary storage
   * interactions and then passes that function to this function. Pending
   * storage operations will be executed sequentially by a coordinator.
   *
   * The passed function should return a promise which will be resolved upon
   * completion of storage interaction.
   */
  enqueueStorageOperation: function (func) {
    return this.storage.enqueueOperation(func);
  },

  getState: function (key) {
    let name = this.name;
    let storage = this.storage;
    return storage.enqueueOperation(function get() {
      return storage.getProviderState(name, key);
    });
  },

  setState: function (key, value) {
    let name = this.name;
    let storage = this.storage;
    return storage.enqueueOperation(function set() {
      return storage.setProviderState(name, key, value);
    });
  },

  _dateToDays: function (date) {
    return Math.floor(date.getTime() / MILLISECONDS_PER_DAY);
  },

  _daysToDate: function (days) {
    return new Date(days * MILLISECONDS_PER_DAY);
  },
});

