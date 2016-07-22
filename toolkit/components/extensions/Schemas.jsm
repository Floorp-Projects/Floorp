/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.importGlobalProperties(["URL"]);

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  instanceOf,
} = ExtensionUtils;

XPCOMUtils.defineLazyServiceGetter(this, "contentPolicyService",
                                   "@mozilla.org/addons/content-policy;1",
                                   "nsIAddonContentPolicy");

this.EXPORTED_SYMBOLS = ["Schemas"];

/* globals Schemas, URL */

function readJSON(url) {
  return new Promise((resolve, reject) => {
    NetUtil.asyncFetch({uri: url, loadUsingSystemPrincipal: true}, (inputStream, status) => {
      if (!Components.isSuccessCode(status)) {
        reject(new Error(status));
        return;
      }
      try {
        let text = NetUtil.readInputStreamToString(inputStream, inputStream.available());

        // Chrome JSON files include a license comment that we need to
        // strip off for this to be valid JSON. As a hack, we just
        // look for the first '[' character, which signals the start
        // of the JSON content.
        let index = text.indexOf("[");
        text = text.slice(index);

        resolve(JSON.parse(text));
      } catch (e) {
        reject(e);
      }
    });
  });
}

// Parses a regular expression, with support for the Python extended
// syntax that allows setting flags by including the string (?im)
function parsePattern(pattern) {
  let flags = "";
  let match = /^\(\?([im]*)\)(.*)/.exec(pattern);
  if (match) {
    [, flags, pattern] = match;
  }
  return new RegExp(pattern, flags);
}

function getValueBaseType(value) {
  let t = typeof(value);
  if (t == "object") {
    if (value === null) {
      return "null";
    } else if (Array.isArray(value)) {
      return "array";
    } else if (Object.prototype.toString.call(value) == "[object ArrayBuffer]") {
      return "binary";
    }
  } else if (t == "number") {
    if (value % 1 == 0) {
      return "integer";
    }
  }
  return t;
}

class Context {
  constructor(params) {
    this.params = params;

    this.path = [];
    this.preprocessors = {
      localize(value, context) {
        return value;
      },
    };

    this.currentChoices = new Set();
    this.choicePathIndex = 0;

    let methods = ["addListener", "callFunction",
                   "callFunctionNoReturn", "callAsyncFunction",
                   "hasPermission",
                   "hasListener", "removeListener",
                   "getProperty", "setProperty",
                   "checkLoadURL", "logError"];
    for (let method of methods) {
      if (method in params) {
        this[method] = params[method].bind(params);
      }
    }

    let props = ["preprocessors"];
    for (let prop of props) {
      if (prop in params) {
        if (prop in this && typeof this[prop] == "object") {
          Object.assign(this[prop], params[prop]);
        } else {
          this[prop] = params[prop];
        }
      }
    }
  }

  get choicePath() {
    let path = this.path.slice(this.choicePathIndex);
    return path.join(".");
  }

  get cloneScope() {
    return this.params.cloneScope;
  }

  get url() {
    return this.params.url;
  }

  get principal() {
    return this.params.principal || Services.scriptSecurityManager.createNullPrincipal({});
  }

  checkLoadURL(url) {
    let ssm = Services.scriptSecurityManager;
    try {
      ssm.checkLoadURIStrWithPrincipal(this.principal, url,
                                       ssm.DISALLOW_INHERIT_PRINCIPAL);
    } catch (e) {
      return false;
    }
    return true;
  }

  /**
   * Checks whether this context has the given permission.
   *
   * @param {string} permission
   *        The name of the permission to check.
   *
   * @returns {boolean} True if the context has the given permission.
   */
  hasPermission(permission) {
    return false;
  }

  /**
   * Returns an error result object with the given message, for return
   * by Type normalization functions.
   *
   * If the context has a `currentTarget` value, this is prepended to
   * the message to indicate the location of the error.
   *
   * @param {string} errorMessage
   *        The error message which will be displayed when this is the
   *        only possible matching schema.
   * @param {string} choicesMessage
   *        The message describing the valid what constitutes a valid
   *        value for this schema, which will be displayed when multiple
   *        schema choices are available and none match.
   *
   *        A caller may pass `null` to prevent a choice from being
   *        added, but this should *only* be done from code processing a
   *        choices type.
   * @returns {object}
   */
  error(errorMessage, choicesMessage = undefined) {
    if (choicesMessage !== null) {
      let {choicePath} = this;
      if (choicePath) {
        choicesMessage = `.${choicePath} must ${choicesMessage}`;
      }

      this.currentChoices.add(choicesMessage);
    }

    if (this.currentTarget) {
      return {error: `Error processing ${this.currentTarget}: ${errorMessage}`};
    }
    return {error: errorMessage};
  }

  /**
   * Creates an `Error` object belonging to the current unprivileged
   * scope. If there is no unprivileged scope associated with this
   * context, the message is returned as a string.
   *
   * If the context has a `currentTarget` value, this is prepended to
   * the message, in the same way as for the `error` method.
   *
   * @param {string} message
   * @returns {Error}
   */
  makeError(message) {
    let {error} = this.error(message);
    if (this.cloneScope) {
      return new this.cloneScope.Error(error);
    }
    return error;
  }

  /**
   * Logs the given error to the console. May be overridden to enable
   * custom logging.
   *
   * @param {Error|string} error
   */
  logError(error) {
    Cu.reportError(error);
  }

  /**
   * Returns the name of the value currently being normalized. For a
   * nested object, this is usually approximately equivalent to the
   * JavaScript property accessor for that property. Given:
   *
   *   { foo: { bar: [{ baz: x }] } }
   *
   * When processing the value for `x`, the currentTarget is
   * 'foo.bar.0.baz'
   */
  get currentTarget() {
    return this.path.join(".");
  }

  /**
   * Executes the given callback, and returns an array of choice strings
   * passed to {@see #error} during its execution.
   *
   * @param {function} callback
   * @returns {object}
   *          An object with a `result` property containing the return
   *          value of the callback, and a `choice` property containing
   *          an array of choices.
   */
  withChoices(callback) {
    let {currentChoices, choicePathIndex} = this;

    let choices = new Set();
    this.currentChoices = choices;
    this.choicePathIndex = this.path.length;

    try {
      let result = callback();

      return {result, choices: Array.from(choices)};
    } finally {
      this.currentChoices = currentChoices;
      this.choicePathIndex = choicePathIndex;

      choices = Array.from(choices);
      if (choices.length == 1) {
        currentChoices.add(choices[0]);
      } else if (choices.length) {
        let n = choices.length - 1;
        choices[n] = `or ${choices[n]}`;

        this.error(null, `must either [${choices.join(", ")}]`);
      }
    }
  }

  /**
   * Appends the given component to the `currentTarget` path to indicate
   * that it is being processed, calls the given callback function, and
   * then restores the original path.
   *
   * This is used to identify the path of the property being processed
   * when reporting type errors.
   *
   * @param {string} component
   * @param {function} callback
   * @returns {*}
   */
  withPath(component, callback) {
    this.path.push(component);
    try {
      return callback();
    } finally {
      this.path.pop();
    }
  }
}


/**
 * The methods in this singleton represent the "format" specifier for
 * JSON Schema string types.
 *
 * Each method either returns a normalized version of the original
 * value, or throws an error if the value is not valid for the given
 * format.
 */
const FORMATS = {
  url(string, context) {
    let url = new URL(string).href;

    if (!context.checkLoadURL(url)) {
      throw new Error(`Access denied for URL ${url}`);
    }
    return url;
  },

  relativeUrl(string, context) {
    if (!context.url) {
      // If there's no context URL, return relative URLs unresolved, and
      // skip security checks for them.
      try {
        new URL(string);
      } catch (e) {
        return string;
      }
    }

    let url = new URL(string, context.url).href;

    if (!context.checkLoadURL(url)) {
      throw new Error(`Access denied for URL ${url}`);
    }
    return url;
  },

  strictRelativeUrl(string, context) {
    // Do not accept a string which resolves as an absolute URL, or any
    // protocol-relative URL.
    if (!string.startsWith("//")) {
      try {
        new URL(string);
      } catch (e) {
        return FORMATS.relativeUrl(string, context);
      }
    }

    throw new SyntaxError(`String ${JSON.stringify(string)} must be a relative URL`);
  },

  contentSecurityPolicy(string, context) {
    let error = contentPolicyService.validateAddonCSP(string);
    if (error != null) {
      throw new SyntaxError(error);
    }
    return string;
  },

  date(string, context) {
    // A valid ISO 8601 timestamp.
    const PATTERN = /^\d{4}-\d{2}-\d{2}(T\d{2}:\d{2}:\d{2}(\.\d{3})?(Z|([-+]\d{2}:?\d{2})))?$/;
    if (!PATTERN.test(string)) {
      throw new Error(`Invalid date string ${string}`);
    }
    // Our pattern just checks the format, we could still have invalid
    // values (e.g., month=99 or month=02 and day=31).  Let the Date
    // constructor do the dirty work of validating.
    if (isNaN(new Date(string))) {
      throw new Error(`Invalid date string ${string}`);
    }
    return string;
  },
};

// Schema files contain namespaces, and each namespace contains types,
// properties, functions, and events. An Entry is a base class for
// types, properties, functions, and events.
class Entry {
  constructor(schema = {}) {
    /**
     * If set to any value which evaluates as true, this entry is
     * deprecated, and any access to it will result in a deprecation
     * warning being logged to the browser console.
     *
     * If the value is a string, it will be appended to the deprecation
     * message. If it contains the substring "${value}", it will be
     * replaced with a string representation of the value being
     * processed.
     *
     * If the value is any other truthy value, a generic deprecation
     * message will be emitted.
     */
    this.deprecated = false;
    if ("deprecated" in schema) {
      this.deprecated = schema.deprecated;
    }

    /**
     * @property {string} [preprocessor]
     * If set to a string value, and a preprocessor of the same is
     * defined in the validation context, it will be applied to this
     * value prior to any normalization.
     */
    this.preprocessor = schema.preprocess || null;
  }

  /**
   * Preprocess the given value with the preprocessor declared in
   * `preprocessor`.
   *
   * @param {*} value
   * @param {Context} context
   * @returns {*}
   */
  preprocess(value, context) {
    if (this.preprocessor) {
      return context.preprocessors[this.preprocessor](value, context);
    }
    return value;
  }

  /**
   * Logs a deprecation warning for this entry, based on the value of
   * its `deprecated` property.
   *
   * @param {Context} context
   * @param {value} [value]
   */
  logDeprecation(context, value = null) {
    let message = "This property is deprecated";
    if (typeof(this.deprecated) == "string") {
      message = this.deprecated;
      if (message.includes("${value}")) {
        try {
          value = JSON.stringify(value);
        } catch (e) {
          value = String(value);
        }
        message = message.replace(/\$\{value\}/g, () => value);
      }
    }

    context.logError(context.makeError(message));
  }

  /**
   * Checks whether the entry is deprecated and, if so, logs a
   * deprecation message.
   *
   * @param {Context} context
   * @param {value} [value]
   */
  checkDeprecated(context, value = null) {
    if (this.deprecated) {
      this.logDeprecation(context, value);
    }
  }

  // Injects JS values for the entry into the extension API
  // namespace. The default implementation is to do
  // nothing. |context| is used to call the actual implementation
  // of a given function or event. It's an object with properties
  // callFunction, addListener, removeListener, and hasListener.
  inject(path, name, dest, context) {
  }
}

// Corresponds either to a type declared in the "types" section of the
// schema or else to any type object used throughout the schema.
class Type extends Entry {
  // Takes a value, checks that it has the correct type, and returns a
  // "normalized" version of the value. The normalized version will
  // include "nulls" in place of omitted optional properties. The
  // result of this function is either {error: "Some type error"} or
  // {value: <normalized-value>}.
  normalize(value, context) {
    return context.error("invalid type");
  }

  // Unlike normalize, this function does a shallow check to see if
  // |baseType| (one of the possible getValueBaseType results) is
  // valid for this type. It returns true or false. It's used to fill
  // in optional arguments to functions before actually type checking

  checkBaseType(baseType) {
    return false;
  }

  // Helper method that simply relies on checkBaseType to implement
  // normalize. Subclasses can choose to use it or not.
  normalizeBase(type, value, context) {
    if (this.checkBaseType(getValueBaseType(value))) {
      this.checkDeprecated(context, value);
      return {value: this.preprocess(value, context)};
    }

    let choice;
    if (/^[aeiou]/.test(type)) {
      choice = `be an ${type} value`;
    } else {
      choice = `be a ${type} value`;
    }

    return context.error(`Expected ${type} instead of ${JSON.stringify(value)}`,
                         choice);
  }
}

// Type that allows any value.
class AnyType extends Type {
  normalize(value, context) {
    this.checkDeprecated(context, value);
    return {value};
  }

  checkBaseType(baseType) {
    return true;
  }
}

// An untagged union type.
class ChoiceType extends Type {
  constructor(schema, choices) {
    super(schema);
    this.choices = choices;
  }

  extend(type) {
    this.choices.push(...type.choices);

    return this;
  }

  normalize(value, context) {
    this.checkDeprecated(context, value);

    let error;
    let {choices, result} = context.withChoices(() => {
      for (let choice of this.choices) {
        let r = choice.normalize(value, context);
        if (!r.error) {
          return r;
        }

        error = r;
      }
    });

    if (result) {
      return result;
    }
    if (choices.length <= 1) {
      return error;
    }

    let n = choices.length - 1;
    choices[n] = `or ${choices[n]}`;

    let message = `Value must either: ${choices.join(", ")}`;

    return context.error(message, null);
  }

  checkBaseType(baseType) {
    return this.choices.some(t => t.checkBaseType(baseType));
  }
}

// This is a reference to another type--essentially a typedef.
class RefType extends Type {
  // For a reference to a type named T declared in namespace NS,
  // namespaceName will be NS and reference will be T.
  constructor(schema, namespaceName, reference) {
    super(schema);
    this.namespaceName = namespaceName;
    this.reference = reference;
  }

  get targetType() {
    let ns = Schemas.namespaces.get(this.namespaceName);
    let type = ns.get(this.reference);
    if (!type) {
      throw new Error(`Internal error: Type ${this.reference} not found`);
    }
    return type;
  }

  normalize(value, context) {
    this.checkDeprecated(context, value);
    return this.targetType.normalize(value, context);
  }

  checkBaseType(baseType) {
    return this.targetType.checkBaseType(baseType);
  }
}

class StringType extends Type {
  constructor(schema, enumeration, minLength, maxLength, pattern, format) {
    super(schema);
    this.enumeration = enumeration;
    this.minLength = minLength;
    this.maxLength = maxLength;
    this.pattern = pattern;
    this.format = format;
  }

  normalize(value, context) {
    let r = this.normalizeBase("string", value, context);
    if (r.error) {
      return r;
    }
    value = r.value;

    if (this.enumeration) {
      if (this.enumeration.includes(value)) {
        return {value};
      }

      let choices = this.enumeration.map(JSON.stringify).join(", ");

      return context.error(`Invalid enumeration value ${JSON.stringify(value)}`,
                           `be one of [${choices}]`);
    }

    if (value.length < this.minLength) {
      return context.error(`String ${JSON.stringify(value)} is too short (must be ${this.minLength})`,
                           `be longer than ${this.minLength}`);
    }
    if (value.length > this.maxLength) {
      return context.error(`String ${JSON.stringify(value)} is too long (must be ${this.maxLength})`,
                           `be shorter than ${this.maxLength}`);
    }

    if (this.pattern && !this.pattern.test(value)) {
      return context.error(`String ${JSON.stringify(value)} must match ${this.pattern}`,
                           `match the pattern ${this.pattern.toSource()}`);
    }

    if (this.format) {
      try {
        r.value = this.format(r.value, context);
      } catch (e) {
        return context.error(String(e), `match the format "${this.format.name}"`);
      }
    }

    return r;
  }

  checkBaseType(baseType) {
    return baseType == "string";
  }

  inject(path, name, dest, context) {
    if (this.enumeration) {
      let obj = Cu.createObjectIn(dest, {defineAs: name});
      for (let e of this.enumeration) {
        let key = e.toUpperCase();
        obj[key] = e;
      }
    }
  }
}

class ObjectType extends Type {
  constructor(schema, properties, additionalProperties, patternProperties, isInstanceOf) {
    super(schema);
    this.properties = properties;
    this.additionalProperties = additionalProperties;
    this.patternProperties = patternProperties;
    this.isInstanceOf = isInstanceOf;
  }

  extend(type) {
    for (let key of Object.keys(type.properties)) {
      if (key in this.properties) {
        throw new Error(`InternalError: Attempt to extend an object with conflicting property "${key}"`);
      }
      this.properties[key] = type.properties[key];
    }

    this.patternProperties.push(...type.patternProperties);

    return this;
  }

  checkBaseType(baseType) {
    return baseType == "object";
  }

  // FIXME: Bug 1265371 - Refactor normalize and parseType in Schemas.jsm to reduce complexity
  normalize(value, context) { // eslint-disable-line complexity
    let v = this.normalizeBase("object", value, context);
    if (v.error) {
      return v;
    }
    value = v.value;

    if (this.isInstanceOf) {
      if (Object.keys(this.properties).length ||
          this.patternProperties.length ||
          !(this.additionalProperties instanceof AnyType)) {
        throw new Error("InternalError: isInstanceOf can only be used with objects that are otherwise unrestricted");
      }

      if (!instanceOf(value, this.isInstanceOf)) {
        return context.error(`Object must be an instance of ${this.isInstanceOf}`,
                             `be an instance of ${this.isInstanceOf}`);
      }

      // This is kind of a hack, but we can't normalize things that
      // aren't JSON, so we just return them.
      return {value};
    }

    // |value| should be a JS Xray wrapping an object in the
    // extension compartment. This works well except when we need to
    // access callable properties on |value| since JS Xrays don't
    // support those. To work around the problem, we verify that
    // |value| is a plain JS object (i.e., not anything scary like a
    // Proxy). Then we copy the properties out of it into a normal
    // object using a waiver wrapper.

    let klass = Cu.getClassName(value, true);
    if (klass != "Object") {
      return context.error(`Expected a plain JavaScript object, got a ${klass}`,
                           `be a plain JavaScript object`);
    }

    let properties = Object.create(null);
    {
      // |waived| is scoped locally to avoid accessing it when we
      // don't mean to.
      let waived = Cu.waiveXrays(value);
      for (let prop of Object.getOwnPropertyNames(waived)) {
        let desc = Object.getOwnPropertyDescriptor(waived, prop);
        if (desc.get || desc.set) {
          return context.error("Objects cannot have getters or setters on properties",
                               "contain no getter or setter properties");
        }
        if (!desc.enumerable) {
          // Chrome ignores non-enumerable properties.
          continue;
        }
        properties[prop] = Cu.unwaiveXrays(desc.value);
      }
    }

    let remainingProps = new Set(Object.keys(properties));

    let checkProperty = (prop, propType, result) => {
      let {type, optional, unsupported} = propType;
      if (unsupported) {
        if (prop in properties) {
          return context.error(`Property "${prop}" is unsupported by Firefox`,
                               `not contain an unsupported "${prop}" property`);
        }
      } else if (prop in properties) {
        if (optional && (properties[prop] === null || properties[prop] === undefined)) {
          result[prop] = null;
        } else {
          let r = context.withPath(prop, () => type.normalize(properties[prop], context));
          if (r.error) {
            return r;
          }
          result[prop] = r.value;
          properties[prop] = r.value;
        }
        remainingProps.delete(prop);
      } else if (!optional) {
        return context.error(`Property "${prop}" is required`,
                             `contain the required "${prop}" property`);
      } else {
        result[prop] = null;
      }
    };

    let result = {};
    for (let prop of Object.keys(this.properties)) {
      let error = checkProperty(prop, this.properties[prop], result);
      if (error) {
        let {onError} = this.properties[prop];
        if (onError == "warn") {
          context.logError(error.error);
        } else if (onError != "ignore") {
          return error;
        }

        result[prop] = null;
        remainingProps.delete(prop);
      }
    }

    for (let prop of Object.keys(properties)) {
      for (let {pattern, type} of this.patternProperties) {
        if (pattern.test(prop)) {
          let error = checkProperty(prop, type, result);
          if (error) {
            return error;
          }
        }
      }
    }

    if (this.additionalProperties) {
      for (let prop of remainingProps) {
        let type = this.additionalProperties;
        let r = context.withPath(prop, () => type.normalize(properties[prop], context));
        if (r.error) {
          return r;
        }
        result[prop] = r.value;
      }
    } else if (remainingProps.size == 1) {
      return context.error(`Unexpected property "${[...remainingProps]}"`,
                           `not contain an unexpected "${[...remainingProps]}" property`);
    } else if (remainingProps.size) {
      let props = [...remainingProps].sort().join(", ");
      return context.error(`Unexpected properties: ${props}`,
                           `not contain the unexpected properties [${props}]`);
    }

    return {value: result};
  }
}

// This type is just a placeholder to be referred to by
// SubModuleProperty. No value is ever expected to have this type.
class SubModuleType extends Type {
  constructor(functions) {
    super();
    this.functions = functions;
  }
}

class NumberType extends Type {
  normalize(value, context) {
    let r = this.normalizeBase("number", value, context);
    if (r.error) {
      return r;
    }

    if (isNaN(r.value) || !Number.isFinite(r.value)) {
      return context.error("NaN and infinity are not valid",
                           "be a finite number");
    }

    return r;
  }

  checkBaseType(baseType) {
    return baseType == "number" || baseType == "integer";
  }
}

class IntegerType extends Type {
  constructor(schema, minimum, maximum) {
    super(schema);
    this.minimum = minimum;
    this.maximum = maximum;
  }

  normalize(value, context) {
    let r = this.normalizeBase("integer", value, context);
    if (r.error) {
      return r;
    }
    value = r.value;

    // Ensure it's between -2**31 and 2**31-1
    if (!Number.isSafeInteger(value)) {
      return context.error("Integer is out of range",
                           "be a valid 32 bit signed integer");
    }

    if (value < this.minimum) {
      return context.error(`Integer ${value} is too small (must be at least ${this.minimum})`,
                           `be at least ${this.minimum}`);
    }
    if (value > this.maximum) {
      return context.error(`Integer ${value} is too big (must be at most ${this.maximum})`,
                           `be no greater than ${this.maximum}`);
    }

    return r;
  }

  checkBaseType(baseType) {
    return baseType == "integer";
  }
}

class BooleanType extends Type {
  normalize(value, context) {
    return this.normalizeBase("boolean", value, context);
  }

  checkBaseType(baseType) {
    return baseType == "boolean";
  }
}

class ArrayType extends Type {
  constructor(schema, itemType, minItems, maxItems) {
    super(schema);
    this.itemType = itemType;
    this.minItems = minItems;
    this.maxItems = maxItems;
  }

  normalize(value, context) {
    let v = this.normalizeBase("array", value, context);
    if (v.error) {
      return v;
    }
    value = v.value;

    let result = [];
    for (let [i, element] of value.entries()) {
      element = context.withPath(String(i), () => this.itemType.normalize(element, context));
      if (element.error) {
        return element;
      }
      result.push(element.value);
    }

    if (result.length < this.minItems) {
      return context.error(`Array requires at least ${this.minItems} items; you have ${result.length}`,
                           `have at least ${this.minItems} items`);
    }

    if (result.length > this.maxItems) {
      return context.error(`Array requires at most ${this.maxItems} items; you have ${result.length}`,
                           `have at most ${this.maxItems} items`);
    }

    return {value: result};
  }

  checkBaseType(baseType) {
    return baseType == "array";
  }
}

class FunctionType extends Type {
  constructor(schema, parameters, isAsync, hasAsyncCallback) {
    super(schema);
    this.parameters = parameters;
    this.isAsync = isAsync;
    this.hasAsyncCallback = hasAsyncCallback;
  }

  normalize(value, context) {
    return this.normalizeBase("function", value, context);
  }

  checkBaseType(baseType) {
    return baseType == "function";
  }
}

// Represents a "property" defined in a schema namespace with a
// particular value. Essentially this is a constant.
class ValueProperty extends Entry {
  constructor(schema, name, value) {
    super(schema);
    this.name = name;
    this.value = value;
  }

  inject(path, name, dest, context) {
    dest[name] = this.value;
  }
}

// Represents a "property" defined in a schema namespace that is not a
// constant.
class TypeProperty extends Entry {
  constructor(schema, namespaceName, name, type, writable) {
    super(schema);
    this.namespaceName = namespaceName;
    this.name = name;
    this.type = type;
    this.writable = writable;
  }

  throwError(context, msg) {
    throw context.makeError(`${msg} for ${this.namespaceName}.${this.name}.`);
  }

  inject(path, name, dest, context) {
    if (this.unsupported) {
      return;
    }

    let getStub = () => {
      this.checkDeprecated(context);
      return context.getProperty(path, name);
    };

    let desc = {
      configurable: false,
      enumerable: true,

      get: Cu.exportFunction(getStub, dest),
    };

    if (this.writable) {
      let setStub = (value) => {
        let normalized = this.type.normalize(value, context);
        if (normalized.error) {
          this.throwError(context, normalized.error);
        }

        context.setProperty(path, name, normalized.value);
      };

      desc.set = Cu.exportFunction(setStub, dest);
    }

    Object.defineProperty(dest, name, desc);
  }
}

class SubModuleProperty extends Entry {
  // A SubModuleProperty represents a tree of objects and properties
  // to expose to an extension. Currently we support only a limited
  // form of sub-module properties, where "$ref" points to a
  // SubModuleType containing a list of functions and "properties" is
  // a list of additional simple properties.
  //
  // name: Name of the property stuff is being added to.
  // namespaceName: Namespace in which the property lives.
  // reference: Name of the type defining the functions to add to the property.
  // properties: Additional properties to add to the module (unsupported).
  constructor(name, namespaceName, reference, properties) {
    super();
    this.name = name;
    this.namespaceName = namespaceName;
    this.reference = reference;
    this.properties = properties;
  }

  inject(path, name, dest, wrapperFuncs) {
    let obj = Cu.createObjectIn(dest, {defineAs: name});

    let ns = Schemas.namespaces.get(this.namespaceName);
    let type = ns.get(this.reference);
    if (!type || !(type instanceof SubModuleType)) {
      throw new Error(`Internal error: ${this.namespaceName}.${this.reference} is not a sub-module`);
    }

    let functions = type.functions;
    for (let fun of functions) {
      fun.inject(path.concat(name), fun.name, obj, wrapperFuncs);
    }

    // TODO: Inject this.properties.
  }
}

// This class is a base class for FunctionEntrys and Events. It takes
// care of validating parameter lists (i.e., handling of optional
// parameters and parameter type checking).
class CallEntry extends Entry {
  constructor(schema, path, name, parameters, allowAmbiguousOptionalArguments) {
    super(schema);
    this.path = path;
    this.name = name;
    this.parameters = parameters;
    this.allowAmbiguousOptionalArguments = allowAmbiguousOptionalArguments;
  }

  throwError(context, msg) {
    throw context.makeError(`${msg} for ${this.path.join(".")}.${this.name}.`);
  }

  checkParameters(args, context) {
    let fixedArgs = [];

    // First we create a new array, fixedArgs, that is the same as
    // |args| but with null values in place of omitted optional
    // parameters.
    let check = (parameterIndex, argIndex) => {
      if (parameterIndex == this.parameters.length) {
        if (argIndex == args.length) {
          return true;
        }
        return false;
      }

      let parameter = this.parameters[parameterIndex];
      if (parameter.optional) {
        // Try skipping it.
        fixedArgs[parameterIndex] = null;
        if (check(parameterIndex + 1, argIndex)) {
          return true;
        }
      }

      if (argIndex == args.length) {
        return false;
      }

      let arg = args[argIndex];
      if (!parameter.type.checkBaseType(getValueBaseType(arg))) {
        if (parameter.optional && (arg === null || arg === undefined)) {
          fixedArgs[parameterIndex] = null;
        } else {
          return false;
        }
      } else {
        fixedArgs[parameterIndex] = arg;
      }

      return check(parameterIndex + 1, argIndex + 1);
    };

    if (this.allowAmbiguousOptionalArguments) {
      // When this option is set, it's up to the implementation to
      // parse arguments.
      return args;
    } else {
      let success = check(0, 0);
      if (!success) {
        this.throwError(context, "Incorrect argument types");
      }
    }

    // Now we normalize (and fully type check) all non-omitted arguments.
    fixedArgs = fixedArgs.map((arg, parameterIndex) => {
      if (arg === null) {
        return null;
      } else {
        let parameter = this.parameters[parameterIndex];
        let r = parameter.type.normalize(arg, context);
        if (r.error) {
          this.throwError(context, `Type error for parameter ${parameter.name} (${r.error})`);
        }
        return r.value;
      }
    });

    return fixedArgs;
  }
}

// Represents a "function" defined in a schema namespace.
class FunctionEntry extends CallEntry {
  constructor(schema, path, name, type, unsupported, allowAmbiguousOptionalArguments, returns, permissions) {
    super(schema, path, name, type.parameters, allowAmbiguousOptionalArguments);
    this.unsupported = unsupported;
    this.returns = returns;
    this.permissions = permissions;

    this.isAsync = type.isAsync;
    this.hasAsyncCallback = type.hasAsyncCallback;
  }

  inject(path, name, dest, context) {
    if (this.unsupported) {
      return;
    }

    if (this.permissions && !this.permissions.some(perm => context.hasPermission(perm))) {
      return;
    }

    let stub;
    if (this.isAsync) {
      stub = (...args) => {
        this.checkDeprecated(context);
        let actuals = this.checkParameters(args, context);
        let callback = null;
        if (this.hasAsyncCallback) {
          callback = actuals.pop();
        }
        return context.callAsyncFunction(path, name, actuals, callback);
      };
    } else if (!this.returns) {
      stub = (...args) => {
        this.checkDeprecated(context);
        let actuals = this.checkParameters(args, context);
        return context.callFunctionNoReturn(path, name, actuals);
      };
    } else {
      stub = (...args) => {
        this.checkDeprecated(context);
        let actuals = this.checkParameters(args, context);
        return context.callFunction(path, name, actuals);
      };
    }
    Cu.exportFunction(stub, dest, {defineAs: name});
  }
}

// Represents an "event" defined in a schema namespace.
class Event extends CallEntry {
  constructor(schema, path, name, type, extraParameters, unsupported, permissions) {
    super(schema, path, name, extraParameters);
    this.type = type;
    this.unsupported = unsupported;
    this.permissions = permissions;
  }

  checkListener(listener, context) {
    let r = this.type.normalize(listener, context);
    if (r.error) {
      this.throwError(context, "Invalid listener");
    }
    return r.value;
  }

  inject(path, name, dest, context) {
    if (this.unsupported) {
      return;
    }

    if (this.permissions && !this.permissions.some(perm => context.hasPermission(perm))) {
      return;
    }

    let addStub = (listener, ...args) => {
      listener = this.checkListener(listener, context);
      let actuals = this.checkParameters(args, context);
      return context.addListener(this.path, name, listener, actuals);
    };

    let removeStub = (listener) => {
      listener = this.checkListener(listener, context);
      return context.removeListener(this.path, name, listener);
    };

    let hasStub = (listener) => {
      listener = this.checkListener(listener, context);
      return context.hasListener(this.path, name, listener);
    };

    let obj = Cu.createObjectIn(dest, {defineAs: name});
    Cu.exportFunction(addStub, obj, {defineAs: "addListener"});
    Cu.exportFunction(removeStub, obj, {defineAs: "removeListener"});
    Cu.exportFunction(hasStub, obj, {defineAs: "hasListener"});
  }
}

this.Schemas = {
  initialized: false,

  // Set of URLs that we have loaded via the load() method.
  loadedUrls: new Set(),

  // Maps a schema URL to the JSON contained in that schema file. This
  // is useful for sending the JSON across processes.
  schemaJSON: new Map(),

  // Map[<schema-name> -> Map[<symbol-name> -> Entry]]
  // This keeps track of all the schemas that have been loaded so far.
  namespaces: new Map(),

  register(namespaceName, symbol, value) {
    let ns = this.namespaces.get(namespaceName);
    if (!ns) {
      ns = new Map();
      ns.permissions = null;
      this.namespaces.set(namespaceName, ns);
    }
    ns.set(symbol, value);
  },

  // FIXME: Bug 1265371 - Refactor normalize and parseType in Schemas.jsm to reduce complexity
  parseType(path, type, extraProperties = []) { // eslint-disable-line complexity
    let allowedProperties = new Set(extraProperties);

    // Do some simple validation of our own schemas.
    function checkTypeProperties(...extra) {
      let allowedSet = new Set([...allowedProperties, ...extra, "description", "deprecated", "preprocess"]);
      for (let prop of Object.keys(type)) {
        if (!allowedSet.has(prop)) {
          throw new Error(`Internal error: Namespace ${path.join(".")} has invalid type property "${prop}" in type "${type.id || JSON.stringify(type)}"`);
        }
      }
    }

    if ("choices" in type) {
      checkTypeProperties("choices");

      let choices = type.choices.map(t => this.parseType(path, t));
      return new ChoiceType(type, choices);
    } else if ("$ref" in type) {
      checkTypeProperties("$ref");
      let ref = type.$ref;
      let ns = path[0];
      if (ref.includes(".")) {
        [ns, ref] = ref.split(".");
      }
      return new RefType(type, ns, ref);
    }

    if (!("type" in type)) {
      throw new Error(`Unexpected value for type: ${JSON.stringify(type)}`);
    }

    allowedProperties.add("type");

    // Otherwise it's a normal type...
    if (type.type == "string") {
      checkTypeProperties("enum", "minLength", "maxLength", "pattern", "format");

      let enumeration = type.enum || null;
      if (enumeration) {
        // The "enum" property is either a list of strings that are
        // valid values or else a list of {name, description} objects,
        // where the .name values are the valid values.
        enumeration = enumeration.map(e => {
          if (typeof(e) == "object") {
            return e.name;
          } else {
            return e;
          }
        });
      }

      let pattern = null;
      if (type.pattern) {
        try {
          pattern = parsePattern(type.pattern);
        } catch (e) {
          throw new Error(`Internal error: Invalid pattern ${JSON.stringify(type.pattern)}`);
        }
      }

      let format = null;
      if (type.format) {
        if (!(type.format in FORMATS)) {
          throw new Error(`Internal error: Invalid string format ${type.format}`);
        }
        format = FORMATS[type.format];
      }
      return new StringType(type, enumeration,
                            type.minLength || 0,
                            type.maxLength || Infinity,
                            pattern,
                            format);
    } else if (type.type == "object" && "functions" in type) {
      checkTypeProperties("functions");

      // The path we pass in here is only used for error messages.
      let functions = type.functions.map(fun => this.parseFunction(path.concat(type.id), fun));

      return new SubModuleType(functions);
    } else if (type.type == "object") {
      let parseProperty = (type, extraProps = []) => {
        return {
          type: this.parseType(path, type,
                               ["unsupported", "onError", "permissions", ...extraProps]),
          optional: type.optional || false,
          unsupported: type.unsupported || false,
          onError: type.onError || null,
        };
      };

      let properties = Object.create(null);
      for (let propName of Object.keys(type.properties || {})) {
        properties[propName] = parseProperty(type.properties[propName], ["optional"]);
      }

      let patternProperties = [];
      for (let propName of Object.keys(type.patternProperties || {})) {
        let pattern;
        try {
          pattern = parsePattern(propName);
        } catch (e) {
          throw new Error(`Internal error: Invalid property pattern ${JSON.stringify(propName)}`);
        }

        patternProperties.push({
          pattern,
          type: parseProperty(type.patternProperties[propName]),
        });
      }

      let additionalProperties = null;
      if (type.additionalProperties) {
        additionalProperties = this.parseType(path, type.additionalProperties);
      }

      if ("$extend" in type) {
        // Only allow extending "properties" and "patternProperties".
        checkTypeProperties("properties", "patternProperties");
      } else {
        checkTypeProperties("properties", "additionalProperties", "patternProperties", "isInstanceOf");
      }
      return new ObjectType(type, properties, additionalProperties, patternProperties, type.isInstanceOf || null);
    } else if (type.type == "array") {
      checkTypeProperties("items", "minItems", "maxItems");
      return new ArrayType(type, this.parseType(path, type.items),
                           type.minItems || 0, type.maxItems || Infinity);
    } else if (type.type == "number") {
      checkTypeProperties();
      return new NumberType(type);
    } else if (type.type == "integer") {
      checkTypeProperties("minimum", "maximum");
      return new IntegerType(type, type.minimum || -Infinity, type.maximum || Infinity);
    } else if (type.type == "boolean") {
      checkTypeProperties();
      return new BooleanType(type);
    } else if (type.type == "function") {
      let isAsync = !!type.async;
      let parameters = null;
      if ("parameters" in type) {
        parameters = [];
        for (let param of type.parameters) {
          // Callbacks default to optional for now, because of promise
          // handling.
          let isCallback = isAsync && param.name == type.async;

          parameters.push({
            type: this.parseType(path, param, ["name", "optional"]),
            name: param.name,
            optional: param.optional == null ? isCallback : param.optional,
          });
        }
      }

      let hasAsyncCallback = false;
      if (isAsync) {
        if (parameters && parameters.length && parameters[parameters.length - 1].name == type.async) {
          hasAsyncCallback = true;
        }
        if (type.returns || type.allowAmbiguousOptionalArguments) {
          throw new Error(`Internal error: Async functions must not have return values or ambiguous arguments.`);
        }
      }

      checkTypeProperties("parameters", "async", "returns");
      return new FunctionType(type, parameters, isAsync, hasAsyncCallback);
    } else if (type.type == "any") {
      // Need to see what minimum and maximum are supposed to do here.
      checkTypeProperties("minimum", "maximum");
      return new AnyType(type);
    } else {
      throw new Error(`Unexpected type ${type.type}`);
    }
  },

  parseFunction(path, fun) {
    let f = new FunctionEntry(fun, path, fun.name,
                              this.parseType(path, fun,
                                             ["name", "unsupported", "returns",
                                              "permissions",
                                              "allowAmbiguousOptionalArguments"]),
                              fun.unsupported || false,
                              fun.allowAmbiguousOptionalArguments || false,
                              fun.returns || null,
                              fun.permissions || null);
    return f;
  },

  loadType(namespaceName, type) {
    if ("$extend" in type) {
      this.extendType(namespaceName, type);
    } else {
      this.register(namespaceName, type.id, this.parseType([namespaceName], type, ["id"]));
    }
  },

  extendType(namespaceName, type) {
    let ns = Schemas.namespaces.get(namespaceName);
    let targetType = ns && ns.get(type.$extend);

    // Only allow extending object and choices types for now.
    if (targetType instanceof ObjectType) {
      type.type = "object";
    } else if (!targetType) {
      throw new Error(`Internal error: Attempt to extend a nonexistant type ${type.$extend}`);
    } else if (!(targetType instanceof ChoiceType)) {
      throw new Error(`Internal error: Attempt to extend a non-extensible type ${type.$extend}`);
    }

    let parsed = this.parseType([namespaceName], type, ["$extend"]);
    if (parsed.constructor !== targetType.constructor) {
      throw new Error(`Internal error: Bad attempt to extend ${type.$extend}`);
    }

    targetType.extend(parsed);
  },

  loadProperty(namespaceName, name, prop) {
    if ("$ref" in prop) {
      if (!prop.unsupported) {
        this.register(namespaceName, name, new SubModuleProperty(name, namespaceName, prop.$ref,
                                                                 prop.properties || {}));
      }
    } else if ("value" in prop) {
      this.register(namespaceName, name, new ValueProperty(prop, name, prop.value));
    } else {
      // We ignore the "optional" attribute on properties since we
      // don't inject anything here anyway.
      let type = this.parseType([namespaceName], prop, ["optional", "writable"]);
      this.register(namespaceName, name, new TypeProperty(prop, namespaceName, name, type, prop.writable || false));
    }
  },

  loadFunction(namespaceName, fun) {
    let f = this.parseFunction([namespaceName], fun);
    this.register(namespaceName, fun.name, f);
  },

  loadEvent(namespaceName, event) {
    let extras = event.extraParameters || [];
    extras = extras.map(param => {
      return {
        type: this.parseType([namespaceName], param, ["name", "optional"]),
        name: param.name,
        optional: param.optional || false,
      };
    });

    // We ignore these properties for now.
    /* eslint-disable no-unused-vars */
    let returns = event.returns;
    let filters = event.filters;
    /* eslint-enable no-unused-vars */

    let type = this.parseType([namespaceName], event,
                              ["name", "unsupported", "permissions",
                               "extraParameters", "returns", "filters"]);

    let e = new Event(event, [namespaceName], event.name, type, extras,
                      event.unsupported || false,
                      event.permissions || null);
    this.register(namespaceName, event.name, e);
  },

  init() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
      let data = Services.cpmm.initialProcessData;
      let schemas = data["Extension:Schemas"];
      if (schemas) {
        this.schemaJSON = schemas;
      }
      Services.cpmm.addMessageListener("Schema:Add", this);
    }
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "Schema:Add":
        this.schemaJSON.set(msg.data.url, msg.data.schema);
        break;
    }
  },

  load(url) {
    let loadFromJSON = json => {
      for (let namespace of json) {
        let name = namespace.namespace;

        let types = namespace.types || [];
        for (let type of types) {
          this.loadType(name, type);
        }

        let properties = namespace.properties || {};
        for (let propertyName of Object.keys(properties)) {
          this.loadProperty(name, propertyName, properties[propertyName]);
        }

        let functions = namespace.functions || [];
        for (let fun of functions) {
          this.loadFunction(name, fun);
        }

        let events = namespace.events || [];
        for (let event of events) {
          this.loadEvent(name, event);
        }

        if (namespace.permissions) {
          let ns = this.namespaces.get(name);
          ns.permissions = namespace.permissions;
        }
      }
    };

    if (Services.appinfo.processType != Services.appinfo.PROCESS_TYPE_CONTENT) {
      return readJSON(url).then(json => {
        this.schemaJSON.set(url, json);

        let data = Services.ppmm.initialProcessData;
        data["Extension:Schemas"] = this.schemaJSON;

        Services.ppmm.broadcastAsyncMessage("Schema:Add", {url, schema: json});

        loadFromJSON(json);
      });
    } else {
      if (this.loadedUrls.has(url)) {
        return;
      }
      this.loadedUrls.add(url);

      let schema = this.schemaJSON.get(url);
      loadFromJSON(schema);
    }
  },

  inject(dest, wrapperFuncs) {
    let context = new Context(wrapperFuncs);

    for (let [namespace, ns] of this.namespaces) {
      if (ns.permissions && !ns.permissions.some(perm => context.hasPermission(perm))) {
        continue;
      }

      let obj = Cu.createObjectIn(dest, {defineAs: namespace});
      for (let [name, entry] of ns) {
        if (wrapperFuncs.shouldInject(namespace, name)) {
          entry.inject([namespace], name, obj, context);
        }
      }

      if (!Object.keys(obj).length) {
        delete dest[namespace];
      }
    }
  },

  normalize(obj, typeName, context) {
    let [namespaceName, prop] = typeName.split(".");
    let ns = this.namespaces.get(namespaceName);
    let type = ns.get(prop);

    return type.normalize(obj, new Context(context));
  },
};
