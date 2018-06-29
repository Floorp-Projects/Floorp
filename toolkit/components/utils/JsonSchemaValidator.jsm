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

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm", {});
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
  }
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
      for (let property of Object.keys(properties.properties)) {
        log.debug(`in object, checking\n    property: ${property}\n    value: ${param[property]}\n    expected type: ${properties.properties[property].type}`);

        if (!param.hasOwnProperty(property)) {
          if (properties.required && properties.required.includes(property)) {
            log.error(`Object is missing required property ${property}`);
            return [false, null];
          }
          continue;
        }

        let [valid, parsedValue] = validateAndParseParamRecursive(param[property], properties.properties[property]);

        if (!valid) {
          return [false, null];
        }

        parsedObj[property] = parsedValue;
      }

      return [true, parsedObj];
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

        let pathQueryRef = parsedParam.pathname + parsedParam.hash;
        // Make sure that "origin" types won't accept full URLs.
        if (pathQueryRef != "/" && pathQueryRef != "") {
          valid = false;
        } else {
          valid = true;
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
