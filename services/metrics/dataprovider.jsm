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
 * This is an abstract base type.
 *
 * This type provides the primary interface for storing, retrieving, and
 * serializing data.
 *
 * Each measurement consists of a set of named fields. Each field is primarily
 * identified by a string name, which must be unique within the measurement.
 *
 * Each derived type must define the following properties:
 *
 *   name -- String name of this measurement. This is the primary way
 *     measurements are distinguished within a provider.
 *
 *   version -- Integer version of this measurement. This is a secondary
 *     identifier for a measurement within a provider. The version denotes
 *     the behavior of this measurement and the composition of its fields over
 *     time. When a new field is added or the behavior of an existing field
 *     changes, the version should be incremented. The initial version of a
 *     measurement is typically 1.
 *
 *   fields -- Object defining the fields this measurement holds. Keys in the
 *     object are string field names. Values are objects describing how the
 *     field works. The following properties are recognized:
 *
 *       type -- The string type of this field. This is typically one of the
 *         FIELD_* constants from the Metrics.Storage type.
 *
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

  if (!this.fields) {
    throw new Error("Measurement must define fields.");
  }

  for (let [name, info] in Iterator(this.fields)) {
    if (!info) {
      throw new Error("Field does not contain metadata: " + name);
    }

    if (!info.type) {
      throw new Error("Field is missing required type property: " + name);
    }
  }

  this._log = Log4Moz.repository.getLogger("Services.Metrics.Measurement." + this.name);

  this.id = null;
  this.storage = null;
  this._fields = {};

  this._serializers = {};
  this._serializers[this.SERIALIZE_JSON] = {
    singular: this._serializeJSONSingular.bind(this),
    daily: this._serializeJSONDay.bind(this),
  };
}

Measurement.prototype = Object.freeze({
  SERIALIZE_JSON: "json",

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

  /**
   * Whether this measurement contains the named field.
   *
   * @param name
   *        (string) Name of field.
   *
   * @return bool
   */
  hasField: function (name) {
    return name in this.fields;
  },

  /**
   * The unique identifier for a named field.
   *
   * This will throw if the field is not known.
   *
   * @param name
   *        (string) Name of field.
   */
  fieldID: function (name) {
    let entry = this._fields[name];

    if (!entry) {
      throw new Error("Unknown field: " + name);
    }

    return entry[0];
  },

  fieldType: function (name) {
    let entry = this._fields[name];

    if (!entry) {
      throw new Error("Unknown field: " + name);
    }

    return entry[1];
  },

  _configureStorage: function () {
    return Task.spawn(function configureFields() {
      for (let [name, info] in Iterator(this.fields)) {
        this._log.debug("Registering field: " + name + " " + info.type);

        let id = yield this.storage.registerField(this.id, name, info.type);
        this._fields[name] = [id, info.type];
      }
    }.bind(this));
  },

  //---------------------------------------------------------------------------
  // Data Recording Functions
  //
  // Functions in this section are used to record new values against this
  // measurement instance.
  //
  // Generally speaking, these functions will throw if the specified field does
  // not exist or if the storage function requested is not appropriate for the
  // type of that field. These functions will also return a promise that will
  // be resolved when the underlying storage operation has completed.
  //---------------------------------------------------------------------------

  /**
   * Increment a daily counter field in this measurement by 1.
   *
   * By default, the counter for the current day will be incremented.
   *
   * If the field is not known or is not a daily counter, this will throw.
   *
   *
   *
   * @param field
   *        (string) The name of the field whose value to increment.
   * @param date
   *        (Date) Day on which to increment the counter.
   * @return Promise<>
   */
  incrementDailyCounter: function (field, date=new Date()) {
    return this.storage.incrementDailyCounterFromFieldID(this.fieldID(field),
                                                         date);
  },

  /**
   * Record a new numeric value for a daily discrete numeric field.
   *
   * @param field
   *        (string) The name of the field to append a value to.
   * @param value
   *        (Number) Number to append.
   * @param date
   *        (Date) Day on which to append the value.
   *
   * @return Promise<>
   */
  addDailyDiscreteNumeric: function (field, value, date=new Date()) {
    return this.storage.addDailyDiscreteNumericFromFieldID(
                          this.fieldID(field), value, date);
  },

  /**
   * Record a new text value for a daily discrete text field.
   *
   * This is like `addDailyDiscreteNumeric` but for daily discrete text fields.
   */
  addDailyDiscreteText: function (field, value, date=new Date()) {
    return this.storage.addDailyDiscreteTextFromFieldID(
                          this.fieldID(field), value, date);
  },

  /**
   * Record the last seen value for a last numeric field.
   *
   * @param field
   *        (string) The name of the field to set the value of.
   * @param value
   *        (Number) The value to set.
   * @param date
   *        (Date) When this value was recorded.
   *
   * @return Promise<>
   */
  setLastNumeric: function (field, value, date=new Date()) {
    return this.storage.setLastNumericFromFieldID(this.fieldID(field), value,
                                                  date);
  },

  /**
   * Record the last seen value for a last text field.
   *
   * This is like `setLastNumeric` except for last text fields.
   */
  setLastText: function (field, value, date=new Date()) {
    return this.storage.setLastTextFromFieldID(this.fieldID(field), value,
                                               date);
  },

  /**
   * Record the most recent value for a daily last numeric field.
   *
   * @param field
   *        (string) The name of a daily last numeric field.
   * @param value
   *        (Number) The value to set.
   * @param date
   *        (Date) Day on which to record the last value.
   *
   * @return Promise<>
   */
  setDailyLastNumeric: function (field, value, date=new Date()) {
    return this.storage.setDailyLastNumericFromFieldID(this.fieldID(field),
                                                       value, date);
  },

  /**
   * Record the most recent value for a daily last text field.
   *
   * This is like `setDailyLastNumeric` except for a daily last text field.
   */
  setDailyLastText: function (field, value, date=new Date()) {
    return this.storage.setDailyLastTextFromFieldID(this.fieldID(field),
                                                    value, date);
  },

  //---------------------------------------------------------------------------
  // End of data recording APIs.
  //---------------------------------------------------------------------------

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
      if (!(field in this._fields)) {
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
      if (!(field in this._fields)) {
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
 * Metrics data collection is initiated either by a manager calling a
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
   * Whether the provider only pulls data from other sources.
   *
   * If this is true, the provider pulls data from other sources. By contrast,
   * "push-based" providers subscribe to foreign sources and record/react to
   * external events as they happen.
   *
   * Pull-only providers likely aren't instantiated until a data collection
   * is performed. Thus, implementations cannot rely on a provider instance
   * always being alive. This is an optimization so provider instances aren't
   * dead weight while the application is running.
   *
   * This must be set on the prototype to have an effect.
   */
  pullOnly: false,

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

        yield measurement._configureStorage();

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
    return CommonUtils.laterTickResolvingPromise();
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
    return CommonUtils.laterTickResolvingPromise();
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
    return CommonUtils.laterTickResolvingPromise();
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
    return CommonUtils.laterTickResolvingPromise();
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

  /**
   * Obtain persisted provider state.
   *
   * Provider state consists of key-value pairs of string names and values.
   * Providers can stuff whatever they want into state. They are encouraged to
   * store as little as possible for performance reasons.
   *
   * State is backed by storage and is robust.
   *
   * These functions do not enqueue on storage automatically, so they should
   * be guarded by `enqueueStorageOperation` or some other mutex.
   *
   * @param key
   *        (string) The property to retrieve.
   *
   * @return Promise<string|null> String value on success. null if no state
   *         is available under this key.
   */
  getState: function (key) {
    return this.storage.getProviderState(this.name, key);
  },

  /**
   * Set state for this provider.
   *
   * This is the complementary API for `getState` and obeys the same
   * storage restrictions.
   */
  setState: function (key, value) {
    return this.storage.setProviderState(this.name, key, value);
  },

  _dateToDays: function (date) {
    return Math.floor(date.getTime() / MILLISECONDS_PER_DAY);
  },

  _daysToDate: function (days) {
    return new Date(days * MILLISECONDS_PER_DAY);
  },
});

