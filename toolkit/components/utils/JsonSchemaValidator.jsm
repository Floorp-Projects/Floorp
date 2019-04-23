/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file implements a not-quite standard JSON schema validator. It differs
 * from the spec in a few ways:
 *
 *  - the spec doesn't allow custom types to be defined, but this validator
 *    defines "URL", "URLorEmpty", "origin" etc.
 * - Strings are automatically converted to nsIURIs for the appropriate types.
 * - It doesn't support "pattern" when matching strings.
 * - The boolean type accepts (and casts) 0 and 1 as valid values.
 */

"use strict";

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

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

var JsonSchemaValidator = {
  validateAndParseParameters(param, properties) {
    return validateAndParseParamRecursive(param, properties);
  },
};

function validateAndParseParamRecursive(param, properties) {
  if (properties.enum) {
    if (properties.enum.includes(param)) {
      return [true, param];
    }
    return [false, null];
  }

  log.debug(`checking @${param}@ for type ${properties.type}`);

  if (Array.isArray(properties.type)) {
    log.debug("type is an array");
    // For an array of types, the value is valid if it matches any of the listed
    // types. To check this, make versions of the object definition that include
    // only one type at a time, and check the value against each one.
    for (const type of properties.type) {
      let typeProperties = Object.assign({}, properties, {type});
      log.debug(`checking subtype ${type}`);
      let [valid, data] = validateAndParseParamRecursive(param, typeProperties);
      if (valid) {
        return [true, data];
      }
    }
    // None of the types matched
    return [false, null];
  }

  switch (properties.type) {
    case "boolean":
    case "number":
    case "integer":
    case "string":
    case "URL":
    case "URLorEmpty":
    case "origin":
      return validateAndParseSimpleParam(param, properties.type);

    case "array":
      if (!Array.isArray(param)) {
        log.error("Array expected but not received");
        return [false, null];
      }

      // strict defaults to true if not present
      let strict = true;
      if ("strict" in properties) {
        strict = properties.strict;
      }

      let parsedArray = [];
      for (let item of param) {
        log.debug(`in array, checking @${item}@ for type ${properties.items.type}`);
        let [valid, parsedValue] = validateAndParseParamRecursive(item, properties.items);
        if (!valid) {
          if (strict) {
            return [false, null];
          }
          continue;
        }

        parsedArray.push(parsedValue);
      }

      return [true, parsedArray];

    case "object": {
      if (typeof(param) != "object") {
        log.error("Object expected but not received");
        return [false, null];
      }

      let parsedObj = {};
      let patternProperties = [];
      if ("patternProperties" in properties) {
        for (let propName of Object.keys(properties.patternProperties || {})) {
          let pattern;
          try {
            pattern = new RegExp(propName);
          } catch (e) {
            throw new Error(`Internal error: Invalid property pattern ${propName}`);
          }
          patternProperties.push({
            pattern,
            schema: properties.patternProperties[propName],
          });
        }
      }

      if (properties.required) {
        for (let required of properties.required) {
          if (!(required in param)) {
            log.error(`Object is missing required property ${required}`);
            return [false, null];
          }
        }
      }

      for (let item of Object.keys(param)) {
        let schema;
        if ("properties" in properties &&
            properties.properties.hasOwnProperty(item)) {
          schema = properties.properties[item];
        } else if (patternProperties.length) {
          for (let patternProperty of patternProperties) {
            if (patternProperty.pattern.test(item)) {
              schema = patternProperty.schema;
              break;
            }
          }
        }
        if (schema) {
          let [valid, parsedValue] = validateAndParseParamRecursive(param[item], schema);
          if (!valid) {
            return [false, null];
          }
          parsedObj[item] = parsedValue;
        }
      }
      return [true, parsedObj];
    }

    case "JSON":
      if (typeof(param) == "object") {
        return [true, param];
      }
      try {
        let json = JSON.parse(param);
        if (typeof(json) != "object") {
          log.error("JSON was not an object");
          return [false, null];
        }
        return [true, json];
      } catch (e) {
        log.error("JSON string couldn't be parsed");
        return [false, null];
      }
  }

  return [false, null];
}

function validateAndParseSimpleParam(param, type) {
  let valid = false;
  let parsedParam = param;

  switch (type) {
    case "boolean":
      if (typeof(param) == "boolean") {
        valid = true;
      } else if (typeof(param) == "number" &&
                 (param == 0 || param == 1)) {
        valid = true;
        parsedParam = !!param;
      }
      break;

    case "number":
    case "string":
      valid = (typeof(param) == type);
      break;

    // integer is an alias to "number" that some JSON schema tools use
    case "integer":
      valid = (typeof(param) == "number");
      break;

    case "origin":
      if (typeof(param) != "string") {
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
            log.error(`Ignoring parameter "${param}" - origin was expected but received full URL.`);
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
      if (typeof(param) != "string") {
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
        valid = false;
      }
      break;
  }

  return [valid, parsedParam];
}
