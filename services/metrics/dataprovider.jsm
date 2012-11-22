/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "MetricsCollectionResult",
  "MetricsMeasurement",
  "MetricsProvider",
];

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/commonjs/promise/core.js");
Cu.import("resource://services-common/log4moz.js");


/**
 * Represents a measurement of data.
 *
 * This is how data is recorded and represented. Each instance of this type
 * represents a related set of data.
 *
 * Each data set has some basic metadata associated with it. This includes a
 * name and version.
 *
 * This type is meant to be an abstract base type. Child types should define
 * a `fields` property which is a mapping of field names to metadata describing
 * that field. This field constitutes the "schema" of the measurement/type.
 *
 * Data is added to instances by calling `setValue()`. Values are validated
 * against the schema at add time.
 *
 * Field Specification
 * ===================
 *
 * The `fields` property is a mapping of string field names to a mapping of
 * metadata describing the field. This mapping can have the following
 * properties:
 *
 *   type -- A string corresponding to the TYPE_* property name describing a
 *           field type. The TYPE_* properties are defined on this type. e.g.
 *           "TYPE_STRING".
 *
 *   optional -- If true, this field is optional. If omitted, the field is
 *               required.
 *
 * @param name
 *        (string) Name of this data set.
 * @param version
 *        (Number) Integer version of the data in this set.
 */
this.MetricsMeasurement = function MetricsMeasurement(name, version) {
  if (!this.fields) {
    throw new Error("fields not defined on instance. You are likely using " +
                    "this type incorrectly.");
  }

  if (!name) {
    throw new Error("Must define a name for this measurement.");
  }

  if (!version) {
    throw new Error("Must define a version for this measurement.");
  }

  if (!Number.isInteger(version)) {
    throw new Error("version must be an integer: " + version);
  }

  this.name = name;
  this.version = version;

  this.values = new Map();
}

MetricsMeasurement.prototype = {
  /**
   * An unsigned integer field stored in 32 bits.
   *
   * This holds values from 0 to 2^32 - 1.
   */
  TYPE_UINT32: {
    validate: function validate(value) {
      if (!Number.isInteger(value)) {
        throw new Error("UINT32 field expects an integer. Got " + value);
      }

      if (value < 0) {
        throw new Error("UINT32 field expects a positive integer. Got " + value);
      }

      if (value >= 0xffffffff) {
        throw new Error("Value is too large to fit within 32 bits: " + value);
      }
    },
  },

  /**
   * A string field.
   *
   * Values must be valid UTF-8 strings.
   */
  TYPE_STRING: {
    validate: function validate(value) {
      if (typeof(value) != "string") {
        throw new Error("STRING field expects a string. Got " + typeof(value));
      }
    },
  },

  /**
   * Set the value of a field.
   *
   * This is ultimately how fields are set. All field sets should go through
   * this function.
   *
   * Values are validated when they are set. If the value passed does not
   * validate against the field's specification, an Error will be thrown.
   *
   * @param name
   *        (string) The name of the field whose value to set.
   * @param value
   *        The value to set the field to.
   */
  setValue: function setValue(name, value) {
    if (!this.fields[name]) {
      throw new Error("Attempting to set unknown field: " + name);
    }

    let type = this.fields[name].type;

    if (!(type in this)) {
      throw new Error("Unknown field type: " + type);
    }

    this[type].validate(value);
    this.values.set(name, value);
  },

  /**
   * Obtain the value of a named field.
   *
   * @param name
   *        (string) The name of the field to retrieve.
   */
  getValue: function getValue(name) {
    return this.values.get(name);
  },

  /**
   * Validate that this instance is in conformance with the specification.
   *
   * This ensures all required fields are present. Field value validation
   * occurs when individual fields are set.
   */
  validate: function validate() {
    for (let field in this.fields) {
      let spec = this.fields[field];

      if (!spec.optional && !(field in this.values)) {
        throw new Error("Required field not defined: " + field);
      }
    }
  },

  toJSON: function toJSON() {
    let fields = {};
    for (let [k, v] of this.values) {
      fields[k] = v;
    }

    return {
      name: this.name,
      version: this.version,
      fields: fields,
    };
  },
};

Object.freeze(MetricsMeasurement.prototype);


/**
 * Entity which provides metrics data for recording.
 *
 * This essentially provides an interface that different systems must implement
 * to provide collected metrics data.
 *
 * This type consists of various collect* functions. These functions are called
 * by the metrics collector at different points during the application's
 * lifetime. These functions return a `MetricsCollectionResult` instance.
 * This type behaves a lot like a promise. It has a `onFinished()` that can chain
 * deferred events until after the result is populated.
 *
 * Implementations of collect* functions should call `createResult()` to create
 * a new `MetricsCollectionResult` instance. They should then register
 * expected measurements with this instance, define a `populate` function on
 * it, then return the instance.
 *
 * It is important for the collect* functions to just create the empty
 * `MetricsCollectionResult` and nothing more. This is to enable the callee
 * to handle errors gracefully. If the collect* function were to raise, the
 * callee may not receive a `MetricsCollectionResult` instance and it would not
 * know what data is missing.
 *
 * See the documentation for `MetricsCollectionResult` for details on how
 * to perform population.
 *
 * Receivers of created `MetricsCollectionResult` instances should wait
 * until population has finished. They can do this by chaining on to the
 * promise inside that instance by calling `onFinished()`.
 *
 * The collect* functions can return null to signify that they will never
 * provide any data. This is the default implementation. An implemented
 * collect* function should *never* return null. Instead, it should return
 * a `MetricsCollectionResult` with expected measurements that has finished
 * populating (i.e. an empty result).
 *
 * @param name
 *        (string) The name of this provider.
 */
this.MetricsProvider = function MetricsProvider(name) {
  if (!name) {
    throw new Error("MetricsProvider must have a name.");
  }

  if (typeof(name) != "string") {
    throw new Error("name must be a string. Got: " + typeof(name));
  }

  this._log = Log4Moz.repository.getLogger("Services.Metrics.MetricsProvider");

  this.name = name;
}

MetricsProvider.prototype = {
  /**
   * Collects constant measurements.
   *
   * Constant measurements are data that doesn't change during the lifetime of
   * the application/process. The metrics collector only needs to call this
   * once per `MetricsProvider` instance per process lifetime.
   */
  collectConstantMeasurements: function collectConstantMeasurements() {
    return null;
  },

  /**
   * Create a new `MetricsCollectionResult` tied to this provider.
   */
  createResult: function createResult() {
    return new MetricsCollectionResult(this.name);
  },
};

Object.freeze(MetricsProvider.prototype);


/**
 * Holds the result of metrics collection.
 *
 * This is the type eventually returned by the MetricsProvider.collect*
 * functions. It holds all results and any state/errors that occurred while
 * collecting.
 *
 * This type is essentially a container for `MetricsMeasurement` instances that
 * provides some smarts useful for capturing state.
 *
 * The first things consumers of new instances should do is define the set of
 * expected measurements this result will contain via `expectMeasurement`. If
 * population of this instance is aborted or times out, downstream consumers
 * will know there is missing data.
 *
 * Next, they should define the `populate` property to a function that
 * populates the instance.
 *
 * The `populate` function implementation should add empty `MetricsMeasurement`
 * instances to the result via `addMeasurement`. Then, it should populate these
 * measurements via `setValue`.
 *
 * It is preferred to populate via this type instead of directly on
 * `MetricsMeasurement` instances so errors with data population can be
 * captured and reported.
 *
 * Once population has finished, `finish()` must be called.
 *
 * @param name
 *        (string) The name of the provider this result came from.
 */
this.MetricsCollectionResult = function MetricsCollectionResult(name) {
  if (!name || typeof(name) != "string") {
    throw new Error("Must provide name argument to MetricsCollectionResult.");
  }

  this._log = Log4Moz.repository.getLogger("Services.Metrics.MetricsCollectionResult");

  this.name = name;

  this.measurements = new Map();
  this.expectedMeasurements = new Set();
  this.errors = [];

  this.populate = function populate() {
    throw new Error("populate() must be defined on MetricsCollectionResult " +
                    "instance.");
  };

  this._deferred = Promise.defer();
}

MetricsCollectionResult.prototype = {
  /**
   * The Set of `MetricsMeasurement` names currently missing from this result.
   */
  get missingMeasurements() {
    let missing = new Set();

    for (let name of this.expectedMeasurements) {
      if (this.measurements.has(name)) {
        continue;
      }

      missing.add(name);
    }

    return missing;
  },

  /**
   * Record that this result is expected to provide a named measurement.
   *
   * This function should be called ASAP on new `MetricsCollectionResult`
   * instances. It defines expectations about what data should be present.
   *
   * @param name
   *        (string) The name of the measurement this result should contain.
   */
  expectMeasurement: function expectMeasurement(name) {
    this.expectedMeasurements.add(name);
  },

  /**
   * Add a `MetricsMeasurement` to this result.
   */
  addMeasurement: function addMeasurement(data) {
    if (!(data instanceof MetricsMeasurement)) {
      throw new Error("addMeasurement expects a MetricsMeasurement instance.");
    }

    if (!this.expectedMeasurements.has(data.name)) {
      throw new Error("Not expecting this measurement: " + data.name);
    }

    if (this.measurements.has(data.name)) {
      throw new Error("Measurement of this name already present: " + data.name);
    }

    this.measurements.set(data.name, data);
  },

  /**
   * Sets the value of a field in a registered measurement instance.
   *
   * This is a convenience function to set a field on a measurement. If an
   * error occurs, it will record that error in the errors container.
   *
   * Attempting to set a value on a measurement that does not exist results
   * in an Error being thrown. Attempting a bad assignment on an existing
   * measurement will not throw unless `rethrow` is true.
   *
   * @param name
   *        (string) The `MetricsMeasurement` on which to set the value.
   * @param field
   *        (string) The field we are setting.
   * @param value
   *        The value being set.
   * @param rethrow
   *        (bool) Whether to rethrow any errors encountered.
   *
   * @return bool
   *         Whether the assignment was successful.
   */
  setValue: function setValue(name, field, value, rethrow=false) {
    let m = this.measurements.get(name);
    if (!m) {
      throw new Error("Attempting to operate on an undefined measurement: " +
                      name);
    }

    try {
      m.setValue(field, value);
      return true;
    } catch (ex) {
      this.addError(ex);

      if (rethrow) {
        throw ex;
      }

      return false;
    }
  },

  /**
   * Record an error that was encountered when populating this result.
   */
  addError: function addError(error) {
    this.errors.push(error);
  },

  /**
   * Aggregate another MetricsCollectionResult into this one.
   *
   * Instances can only be aggregated together if they belong to the same
   * provider (they have the same name).
   */
  aggregate: function aggregate(other) {
    if (!(other instanceof MetricsCollectionResult)) {
      throw new Error("aggregate expects a MetricsCollectionResult instance.");
    }

    if (this.name != other.name) {
      throw new Error("Can only aggregate MetricsCollectionResult from " +
                      "the same provider. " + this.name + " != " + other.name);
    }

    for (let name of other.expectedMeasurements) {
      this.expectedMeasurements.add(name);
    }

    for (let [name, m] of other.measurements) {
      if (this.measurements.has(name)) {
        throw new Error("Incoming result has same measurement as us: " + name);
      }

      this.measurements.set(name, m);
    }

    this.errors = this.errors.concat(other.errors);
  },

  toJSON: function toJSON() {
    let o = {
      measurements: {},
      missing: [],
      errors: [],
    };

    for (let [name, value] of this.measurements) {
      o.measurements[name] = value;
    }

    for (let missing of this.missingMeasurements) {
      o.missing.push(missing);
    }

    for (let error of this.errors) {
      if (error.message) {
        o.errors.push(error.message);
      } else {
        o.errors.push(error);
      }
    }

    return o;
  },

  /**
   * Signal that population of the result has finished.
   *
   * This will resolve the internal promise.
   */
  finish: function finish() {
    this._deferred.resolve(this);
  },

  /**
   * Chain deferred behavior until after the result has finished population.
   *
   * This is a wrapped around the internal promise's `then`.
   *
   * We can't call this "then" because the core promise library will get
   * confused.
   */
  onFinished: function onFinished(onFulfill, onError) {
    return this._deferred.promise.then(onFulfill, onError);
  },
};

Object.freeze(MetricsCollectionResult.prototype);

