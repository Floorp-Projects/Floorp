/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file implements a not-quite standard JSON schema validator. It differs
 * from the spec in a few ways:
 *
 *  - the spec doesn't allow custom types to be defined, but this validator
 *    defines "URL", "URLorEmpty", "origin" etc.
 * - Strings are automatically converted to `URL` objects for the appropriate
 *   types.
 * - It doesn't support "pattern" when matching strings.
 * - The boolean type accepts (and casts) 0 and 1 as valid values.
 */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm");
  return new ConsoleAPI({
    prefix: "JsonSchemaValidator.jsm",
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
  });
});

var EXPORTED_SYMBOLS = ["JsonSchemaValidator"];

/**
 * To validate a single value, use the static `JsonSchemaValidator.validate`
 * method.  If you need to validate multiple values, you instead might want to
 * make a JsonSchemaValidator instance with the options you need and then call
 * the `validate` instance method.
 */
class JsonSchemaValidator {
  /**
   * Validates a value against a schema.
   *
   * @param {*} value
   *   The value to validate.
   * @param {object} schema
   *   The schema to validate against.
   * @param {boolean} allowArrayNonMatchingItems
   *   When true:
   *     Invalid items in arrays will be ignored, and they won't be included in
   *     result.parsedValue.
   *   When false:
   *     Invalid items in arrays will cause validation to fail.
   * @param {boolean} allowObjectExplicitUndefinedProperties
   *   When true:
   *     `someProperty: undefined` will be allowed for non-required properties.
   *   When false:
   *     `someProperty: undefined` will cause validation to fail even for
   *     properties that are not required.
   * @param {boolean} allowObjectNullAsUndefinedProperties
   *   When true:
   *     `someProperty: null` will be allowed for non-required properties whose
   *     expected types are non-null.
   *   When false:
   *     `someProperty: null` will cause validation to fail for non-required
   *     properties, except for properties whose expected types are null.
   * @param {boolean} allowObjectExtraProperties
   *   When true:
   *     Properties that are not defined in the schema will be ignored, and they
   *     won't be included in result.parsedValue.
   *   When false:
   *     Properties that are not defined in the schema will cause validation to
   *     fail.
   * @return {object}
   *   The result of the validation, an object that looks like this:
   *
   *   {
   *     valid,
   *     parsedValue,
   *     error: {
   *       message,
   *       rootValue,
   *       rootSchema,
   *       invalidValue,
   *       invalidPropertyNameComponents,
   *     }
   *   }
   *
   *   {boolean} valid
   *     True if validation is successful, false if not.
   *   {*} parsedValue
   *     If validation is successful, this is the validated value.  It can
   *     differ from the passed-in value in the following ways:
   *       * If a type in the schema is "URL" or "URLorEmpty", the passed-in
   *         value can use a string instead and it will be converted into a
   *         `URL` object in parsedValue.
   *       * Some of the `allow*` parameters control the properties that appear.
   *         See above.
   *   {Error} error
   *     If validation fails, `error` will be present.  It contains a number of
   *     properties useful for understanding the validation failure.
   *   {string} error.message
   *     The validation failure message.
   *   {*} error.rootValue
   *     The passed-in value.
   *   {object} error.rootSchema
   *     The passed-in schema.
   *   {*} invalidValue
   *     The value that caused validation to fail.  If the passed-in value is a
   *     scalar type, this will be the value itself.  If the value is an object
   *     or array, it will be the specific nested value in the object or array
   *     that caused validation to fail.
   *   {array} invalidPropertyNameComponents
   *     If the passed-in value is an object or array, this will contain the
   *     names of the object properties or array indexes where invalidValue can
   *     be found.  For example, assume the passed-in value is:
   *       { foo: { bar: { baz: 123 }}}
   *     And assume `baz` should be a string instead of a number.  Then
   *     invalidValue will be 123, and invalidPropertyNameComponents will be
   *     ["foo", "bar", "baz"], indicating that the erroneous property in the
   *     passed-in object is `foo.bar.baz`.
   */
  static validate(
    value,
    schema,
    {
      allowArrayNonMatchingItems = false,
      allowExplicitUndefinedProperties = false,
      allowNullAsUndefinedProperties = false,
      allowExtraProperties = false,
    } = {}
  ) {
    let validator = new JsonSchemaValidator({
      allowArrayNonMatchingItems,
      allowExplicitUndefinedProperties,
      allowNullAsUndefinedProperties,
      allowExtraProperties,
    });
    return validator.validate(value, schema);
  }

  /**
   * Constructor.
   *
   * @param {boolean} allowArrayNonMatchingItems
   *   See the static `validate` method above.
   * @param {boolean} allowObjectExplicitUndefinedProperties
   *   See the static `validate` method above.
   * @param {boolean} allowObjectNullAsUndefinedProperties
   *   See the static `validate` method above.
   * @param {boolean} allowObjectExtraProperties
   *   See the static `validate` method above.
   */
  constructor({
    allowArrayNonMatchingItems = false,
    allowExplicitUndefinedProperties = false,
    allowNullAsUndefinedProperties = false,
    allowExtraProperties = false,
  } = {}) {
    this.allowArrayNonMatchingItems = allowArrayNonMatchingItems;
    this.allowExplicitUndefinedProperties = allowExplicitUndefinedProperties;
    this.allowNullAsUndefinedProperties = allowNullAsUndefinedProperties;
    this.allowExtraProperties = allowExtraProperties;
  }

  /**
   * Validates a value against a schema.
   *
   * @param {*} value
   *   The value to validate.
   * @param {object} schema
   *   The schema to validate against.
   * @return {object}
   *   The result object.  See the static `validate` method above.
   */
  validate(value, schema) {
    return this._validateRecursive(value, schema, [], {
      rootValue: value,
      rootSchema: schema,
    });
  }

  // eslint-disable-next-line complexity
  _validateRecursive(param, properties, keyPath, state) {
    log.debug(`checking @${param}@ for type ${properties.type}`);

    if (Array.isArray(properties.type)) {
      log.debug("type is an array");
      // For an array of types, the value is valid if it matches any of the
      // listed types. To check this, make versions of the object definition
      // that include only one type at a time, and check the value against each
      // one.
      for (const type of properties.type) {
        let typeProperties = Object.assign({}, properties, { type });
        log.debug(`checking subtype ${type}`);
        let result = this._validateRecursive(
          param,
          typeProperties,
          keyPath,
          state
        );
        if (result.valid) {
          return result;
        }
      }
      // None of the types matched
      return {
        valid: false,
        error: new JsonSchemaValidatorError({
          message:
            `The value '${valueToString(param)}' does not match any type in ` +
            valueToString(properties.type),
          value: param,
          keyPath,
          state,
        }),
      };
    }

    switch (properties.type) {
      case "boolean":
      case "number":
      case "integer":
      case "string":
      case "URL":
      case "URLorEmpty":
      case "origin":
      case "null": {
        let result = this._validateSimpleParam(
          param,
          properties.type,
          keyPath,
          state
        );
        if (!result.valid) {
          return result;
        }
        if (properties.enum && typeof result.parsedValue !== "boolean") {
          if (!properties.enum.includes(param)) {
            return {
              valid: false,
              error: new JsonSchemaValidatorError({
                message:
                  `The value '${valueToString(param)}' is not one of the ` +
                  `enumerated values ${valueToString(properties.enum)}`,
                value: param,
                keyPath,
                state,
              }),
            };
          }
        }
        return result;
      }

      case "array":
        if (!Array.isArray(param)) {
          log.error("Array expected but not received");
          return {
            valid: false,
            error: new JsonSchemaValidatorError({
              message:
                `The value '${valueToString(param)}' does not match the ` +
                `expected type 'array'`,
              value: param,
              keyPath,
              state,
            }),
          };
        }

        let parsedArray = [];
        for (let i = 0; i < param.length; i++) {
          let item = param[i];
          log.debug(
            `in array, checking @${item}@ for type ${properties.items.type}`
          );
          let result = this._validateRecursive(
            item,
            properties.items,
            keyPath.concat(i),
            state
          );
          if (!result.valid) {
            if (
              ("strict" in properties && properties.strict) ||
              (!("strict" in properties) && !this.allowArrayNonMatchingItems)
            ) {
              return result;
            }
            continue;
          }

          parsedArray.push(result.parsedValue);
        }

        return { valid: true, parsedValue: parsedArray };

      case "object": {
        if (typeof param != "object" || !param) {
          log.error("Object expected but not received");
          return {
            valid: false,
            error: new JsonSchemaValidatorError({
              message:
                `The value '${valueToString(param)}' does not match the ` +
                `expected type 'object'`,
              value: param,
              keyPath,
              state,
            }),
          };
        }

        let parsedObj = {};
        let patternProperties = [];
        if ("patternProperties" in properties) {
          for (let prop of Object.keys(properties.patternProperties || {})) {
            let pattern;
            try {
              pattern = new RegExp(prop);
            } catch (e) {
              throw new Error(
                `Internal error: Invalid property pattern ${prop}`
              );
            }
            patternProperties.push({
              pattern,
              schema: properties.patternProperties[prop],
            });
          }
        }

        if (properties.required) {
          for (let required of properties.required) {
            if (!(required in param)) {
              log.error(`Object is missing required property ${required}`);
              return {
                valid: false,
                error: new JsonSchemaValidatorError({
                  message: `Object is missing required property '${required}'`,
                  value: param,
                  keyPath,
                  state,
                }),
              };
            }
          }
        }

        for (let item of Object.keys(param)) {
          let schema;
          if (
            "properties" in properties &&
            properties.properties.hasOwnProperty(item)
          ) {
            schema = properties.properties[item];
          } else if (patternProperties.length) {
            for (let patternProperty of patternProperties) {
              if (patternProperty.pattern.test(item)) {
                schema = patternProperty.schema;
                break;
              }
            }
          }
          if (!schema) {
            let allowExtraProperties =
              !properties.strict && this.allowExtraProperties;
            if (allowExtraProperties) {
              continue;
            }
            return {
              valid: false,
              error: new JsonSchemaValidatorError({
                message: `Object has unexpected property '${item}'`,
                value: param,
                keyPath,
                state,
              }),
            };
          }
          let allowExplicitUndefinedProperties =
            !properties.strict && this.allowExplicitUndefinedProperties;
          let allowNullAsUndefinedProperties =
            !properties.strict && this.allowNullAsUndefinedProperties;
          let isUndefined =
            (!allowExplicitUndefinedProperties && !(item in param)) ||
            (allowExplicitUndefinedProperties && param[item] === undefined) ||
            (allowNullAsUndefinedProperties && param[item] === null);
          if (isUndefined) {
            continue;
          }
          let result = this._validateRecursive(
            param[item],
            schema,
            keyPath.concat(item),
            state
          );
          if (!result.valid) {
            return result;
          }
          parsedObj[item] = result.parsedValue;
        }
        return { valid: true, parsedValue: parsedObj };
      }

      case "JSON":
        if (typeof param == "object") {
          return { valid: true, parsedValue: param };
        }
        try {
          let json = JSON.parse(param);
          if (typeof json != "object") {
            log.error("JSON was not an object");
            return {
              valid: false,
              error: new JsonSchemaValidatorError({
                message: `JSON was not an object: ${valueToString(param)}`,
                value: param,
                keyPath,
                state,
              }),
            };
          }
          return { valid: true, parsedValue: json };
        } catch (e) {
          log.error("JSON string couldn't be parsed");
          return {
            valid: false,
            error: new JsonSchemaValidatorError({
              message: `JSON string could not be parsed: ${valueToString(
                param
              )}`,
              value: param,
              keyPath,
              state,
            }),
          };
        }
    }

    return {
      valid: false,
      error: new JsonSchemaValidatorError({
        message: `Invalid schema property type: ${valueToString(
          properties.type
        )}`,
        value: param,
        keyPath,
        state,
      }),
    };
  }

  _validateSimpleParam(param, type, keyPath, state) {
    let valid = false;
    let parsedParam = param;
    let error = undefined;

    switch (type) {
      case "boolean":
        if (typeof param == "boolean") {
          valid = true;
        } else if (typeof param == "number" && (param == 0 || param == 1)) {
          valid = true;
          parsedParam = !!param;
        }
        break;

      case "number":
      case "string":
        valid = typeof param == type;
        break;

      // integer is an alias to "number" that some JSON schema tools use
      case "integer":
        valid = typeof param == "number";
        break;

      case "null":
        valid = param === null;
        break;

      case "origin":
        if (typeof param != "string") {
          break;
        }

        try {
          parsedParam = new URL(param);

          if (parsedParam.protocol == "file:") {
            // Treat the entire file URL as an origin.
            // Note this is stricter than the current Firefox policy,
            // but consistent with Chrome.
            // See https://bugzilla.mozilla.org/show_bug.cgi?id=803143
            valid = true;
          } else {
            let pathQueryRef = parsedParam.pathname + parsedParam.hash;
            // Make sure that "origin" types won't accept full URLs.
            if (pathQueryRef != "/" && pathQueryRef != "") {
              log.error(
                `Ignoring parameter "${param}" - origin was expected but received full URL.`
              );
              valid = false;
            } else {
              valid = true;
            }
          }
        } catch (ex) {
          valid = false;
        }
        break;

      case "URL":
      case "URLorEmpty":
        if (typeof param != "string") {
          break;
        }

        if (type == "URLorEmpty" && param === "") {
          valid = true;
          break;
        }

        try {
          parsedParam = new URL(param);
          valid = true;
        } catch (ex) {
          if (!param.startsWith("http")) {
            log.error(
              `Ignoring parameter "${param}" - scheme (http or https) must be specified.`
            );
          }
          valid = false;
        }
        break;
    }

    if (!valid && !error) {
      error = new JsonSchemaValidatorError({
        message:
          `The value '${valueToString(param)}' does not match the expected ` +
          `type '${type}'`,
        value: param,
        keyPath,
        state,
      });
    }

    let result = {
      valid,
      parsedValue: parsedParam,
    };
    if (error) {
      result.error = error;
    }
    return result;
  }
}

class JsonSchemaValidatorError extends Error {
  constructor({ message, value, keyPath, state } = {}, ...args) {
    if (keyPath.length) {
      message +=
        ". " +
        `The invalid value is property '${keyPath.join(".")}' in ` +
        JSON.stringify(state.rootValue);
    }
    super(message, ...args);
    this.name = "JsonSchemaValidatorError";
    this.rootValue = state.rootValue;
    this.rootSchema = state.rootSchema;
    this.invalidPropertyNameComponents = keyPath;
    this.invalidValue = value;
  }
}

function valueToString(value) {
  try {
    return JSON.stringify(value);
  } catch (ex) {}
  return String(value);
}
