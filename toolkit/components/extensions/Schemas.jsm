/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

const global = this;

Cu.importGlobalProperties(["URL"]);

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  DefaultMap,
  instanceOf,
} = ExtensionUtils;

class DeepMap extends DefaultMap {
  constructor() {
    super(() => new DeepMap());
  }

  getPath(...keys) {
    return keys.reduce((map, prop) => map.get(prop), this);
  }
}

XPCOMUtils.defineLazyServiceGetter(this, "contentPolicyService",
                                   "@mozilla.org/addons/content-policy;1",
                                   "nsIAddonContentPolicy");

this.EXPORTED_SYMBOLS = ["Schemas"];

/* globals Schemas, URL */

function readJSON(url) {
  return new Promise((resolve, reject) => {
    NetUtil.asyncFetch({uri: url, loadUsingSystemPrincipal: true}, (inputStream, status) => {
      if (!Components.isSuccessCode(status)) {
        // Convert status code to a string
        let e = Components.Exception("", status);
        reject(new Error(`Error while loading '${url}' (${e.name})`));
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

/**
 * Defines a lazy getter for the given property on the given object. Any
 * security wrappers are waived on the object before the property is
 * defined, and the getter and setter methods are wrapped for the target
 * scope.
 *
 * The given getter function is guaranteed to be called only once, even
 * if the target scope retrieves the wrapped getter from the property
 * descriptor and calls it directly.
 *
 * @param {object} object
 *        The object on which to define the getter.
 * @param {string|Symbol} prop
 *        The property name for which to define the getter.
 * @param {function} getter
 *        The function to call in order to generate the final property
 *        value.
 */
function exportLazyGetter(object, prop, getter) {
  object = Cu.waiveXrays(object);

  let redefine = value => {
    if (value === undefined) {
      delete object[prop];
    } else {
      Object.defineProperty(object, prop, {
        enumerable: true,
        configurable: true,
        writable: true,
        value,
      });
    }

    getter = null;

    return value;
  };

  Object.defineProperty(object, prop, {
    enumerable: true,
    configurable: true,

    get: Cu.exportFunction(function() {
      return redefine(getter.call(this));
    }, object),

    set: Cu.exportFunction(value => {
      redefine(value);
    }, object),
  });
}

const POSTPROCESSORS = {
  convertImageDataToURL(imageData, context) {
    let document = context.cloneScope.document;
    let canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    canvas.width = imageData.width;
    canvas.height = imageData.height;
    canvas.getContext("2d").putImageData(imageData, 0, 0);

    return canvas.toDataURL("image/png");
  },
};

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

// Methods of Context that are used by Schemas.normalize. These methods can be
// overridden at the construction of Context.
const CONTEXT_FOR_VALIDATION = [
  "checkLoadURL",
  "hasPermission",
  "logError",
];

// Methods of Context that are used by Schemas.inject.
// Callers of Schemas.inject should implement all of these methods.
const CONTEXT_FOR_INJECTION = [
  ...CONTEXT_FOR_VALIDATION,
  "shouldInject",
  "getImplementation",
];

/**
 * A context for schema validation and error reporting. This class is only used
 * internally within Schemas.
 */
class Context {
  /**
   * @param {object} params Provides the implementation of this class.
   * @param {Array<string>} overridableMethods
   */
  constructor(params, overridableMethods = CONTEXT_FOR_VALIDATION) {
    this.params = params;

    this.path = [];
    this.preprocessors = {
      localize(value, context) {
        return value;
      },
    };
    this.postprocessors = POSTPROCESSORS;
    this.isChromeCompat = false;

    this.currentChoices = new Set();
    this.choicePathIndex = 0;

    for (let method of overridableMethods) {
      if (method in params) {
        this[method] = params[method].bind(params);
      }
    }

    let props = ["preprocessors", "isChromeCompat"];
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

  /**
   * Checks whether `url` may be loaded by the extension in this context.
   *
   * @param {string} url The URL that the extension wished to load.
   * @returns {boolean} Whether the context may load `url`.
   */
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
 * Holds methods that run the actual implementation of the extension APIs. These
 * methods are only called if the extension API invocation matches the signature
 * as defined in the schema. Otherwise an error is reported to the context.
 */
class InjectionContext extends Context {
  constructor(params) {
    super(params, CONTEXT_FOR_INJECTION);
  }

  /**
   * Check whether the API should be injected.
   *
   * @abstract
   * @param {string} namespace The namespace of the API. This may contain dots,
   *     e.g. in the case of "devtools.inspectedWindow".
   * @param {string} [name] The name of the property in the namespace.
   *     `null` if we are checking whether the namespace should be injected.
   * @param {Array<string>} allowedContexts A list of additional contexts in which
   *     this API should be available. May include any of:
   *         "main" - The main chrome browser process.
   *         "addon" - An addon process.
   *         "content" - A content process.
   * @returns {boolean} Whether the API should be injected.
   */
  shouldInject(namespace, name, allowedContexts) {
    throw new Error("Not implemented");
  }

  /**
   * Generate the implementation for `namespace`.`name`.
   *
   * @abstract
   * @param {string} namespace The full path to the namespace of the API, minus
   *     the name of the method or property. E.g. "storage.local".
   * @param {string} name The name of the method, property or event.
   * @returns {SchemaAPIInterface} The implementation of the API.
   */
  getImplementation(namespace, name) {
    throw new Error("Not implemented");
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

    /**
     * @property {string} [postprocessor]
     * If set to a string value, and a postprocessor of the same is
     * defined in the validation context, it will be applied to this
     * value after any normalization.
     */
    this.postprocessor = schema.postprocess || null;

    /**
     * @property {Array<string>} allowedContexts A list of allowed contexts
     * to consider before generating the API.
     * These are not parsed by the schema, but passed to `shouldInject`.
     */
    this.allowedContexts = schema.allowedContexts || [];
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
   * Postprocess the given result with the postprocessor declared in
   * `postprocessor`.
   *
   * @param {object} result
   * @param {Context} context
   * @returns {object}
   */
  postprocess(result, context) {
    if (result.error || !this.postprocessor) {
      return result;
    }

    let value = context.postprocessors[this.postprocessor](result.value, context);
    return {value};
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

  /**
   * Injects JS values for the entry into the extension API
   * namespace. The default implementation is to do nothing.
   * `context` is used to call the actual implementation
   * of a given function or event.
   *
   * @param {Array<string>} path The API path, e.g. `["storage", "local"]`.
   * @param {string} name The method name, e.g. "get".
   * @param {object} dest The object where `path`.`name` should be stored.
   * @param {InjectionContext} context
   */
  inject(path, name, dest, context) {
  }
}

// Corresponds either to a type declared in the "types" section of the
// schema or else to any type object used throughout the schema.
class Type extends Entry {
  /**
   * @property {Array<string>} EXTRA_PROPERTIES
   *        An array of extra properties which may be present for
   *        schemas of this type.
   */
  static get EXTRA_PROPERTIES() {
    return ["description", "deprecated", "preprocess", "postprocess", "allowedContexts"];
  }

  /**
   * Parses the given schema object and returns an instance of this
   * class which corresponds to its properties.
   *
   * @param {object} schema
   *        A JSON schema object which corresponds to a definition of
   *        this type.
   * @param {Array<string>} path
   *        The path to this schema object from the root schema,
   *        corresponding to the property names and array indices
   *        traversed during parsing in order to arrive at this schema
   *        object.
   * @param {Array<string>} [extraProperties]
   *        An array of extra property names which are valid for this
   *        schema in the current context.
   * @returns {Type}
   *        An instance of this type which corresponds to the given
   *        schema object.
   * @static
   */
  static parseSchema(schema, path, extraProperties = []) {
    this.checkSchemaProperties(schema, path, extraProperties);

    return new this(schema);
  }

  /**
   * Checks that all of the properties present in the given schema
   * object are valid properties for this type, and throws if invalid.
   *
   * @param {object} schema
   *        A JSON schema object.
   * @param {Array<string>} path
   *        The path to this schema object from the root schema,
   *        corresponding to the property names and array indices
   *        traversed during parsing in order to arrive at this schema
   *        object.
   * @param {Array<string>} [extra]
   *        An array of extra property names which are valid for this
   *        schema in the current context.
   * @throws {Error}
   *        An error describing the first invalid property found in the
   *        schema object.
   */
  static checkSchemaProperties(schema, path, extra = []) {
    let allowedSet = new Set([...this.EXTRA_PROPERTIES, ...extra]);

    for (let prop of Object.keys(schema)) {
      if (!allowedSet.has(prop)) {
        throw new Error(`Internal error: Namespace ${path.join(".")} has invalid type property "${prop}" in type "${schema.id || JSON.stringify(schema)}"`);
      }
    }
  }

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
    return this.postprocess({value}, context);
  }

  checkBaseType(baseType) {
    return true;
  }
}

// An untagged union type.
class ChoiceType extends Type {
  static get EXTRA_PROPERTIES() {
    return ["choices", ...super.EXTRA_PROPERTIES];
  }

  static parseSchema(schema, path, extraProperties = []) {
    this.checkSchemaProperties(schema, path, extraProperties);

    let choices = schema.choices.map(t => Schemas.parseSchema(t, path));
    return new this(schema, choices);
  }

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
  static get EXTRA_PROPERTIES() {
    return ["$ref", ...super.EXTRA_PROPERTIES];
  }

  static parseSchema(schema, path, extraProperties = []) {
    this.checkSchemaProperties(schema, path, extraProperties);

    let ref = schema.$ref;
    let ns = path[0];
    if (ref.includes(".")) {
      [ns, ref] = ref.split(".");
    }
    return new this(schema, ns, ref);
  }

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
  static get EXTRA_PROPERTIES() {
    return ["enum", "minLength", "maxLength", "pattern", "format",
            ...super.EXTRA_PROPERTIES];
  }

  static parseSchema(schema, path, extraProperties = []) {
    this.checkSchemaProperties(schema, path, extraProperties);

    let enumeration = schema.enum || null;
    if (enumeration) {
      // The "enum" property is either a list of strings that are
      // valid values or else a list of {name, description} objects,
      // where the .name values are the valid values.
      enumeration = enumeration.map(e => {
        if (typeof(e) == "object") {
          return e.name;
        }
        return e;
      });
    }

    let pattern = null;
    if (schema.pattern) {
      try {
        pattern = parsePattern(schema.pattern);
      } catch (e) {
        throw new Error(`Internal error: Invalid pattern ${JSON.stringify(schema.pattern)}`);
      }
    }

    let format = null;
    if (schema.format) {
      if (!(schema.format in FORMATS)) {
        throw new Error(`Internal error: Invalid string format ${schema.format}`);
      }
      format = FORMATS[schema.format];
    }
    return new this(schema, enumeration,
                    schema.minLength || 0,
                    schema.maxLength || Infinity,
                    pattern,
                    format);
  }

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
        return this.postprocess({value}, context);
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
      exportLazyGetter(dest, name, () => {
        let obj = Cu.createObjectIn(dest);
        for (let e of this.enumeration) {
          obj[e.toUpperCase()] = e;
        }
        return obj;
      });
    }
  }
}

let SubModuleType;
class ObjectType extends Type {
  static get EXTRA_PROPERTIES() {
    return ["properties", "patternProperties", ...super.EXTRA_PROPERTIES];
  }

  static parseSchema(schema, path, extraProperties = []) {
    if ("functions" in schema) {
      return SubModuleType.parseSchema(schema, path, extraProperties);
    }

    if (!("$extend" in schema)) {
      // Only allow extending "properties" and "patternProperties".
      extraProperties = ["additionalProperties", "isInstanceOf", ...extraProperties];
    }
    this.checkSchemaProperties(schema, path, extraProperties);

    let parseProperty = (schema, extraProps = []) => {
      return {
        type: Schemas.parseSchema(schema, path,
                                  ["unsupported", "onError", "permissions", ...extraProps]),
        optional: schema.optional || false,
        unsupported: schema.unsupported || false,
        onError: schema.onError || null,
      };
    };

    // Parse explicit "properties" object.
    let properties = Object.create(null);
    for (let propName of Object.keys(schema.properties || {})) {
      properties[propName] = parseProperty(schema.properties[propName], ["optional"]);
    }

    // Parse regexp properties from "patternProperties" object.
    let patternProperties = [];
    for (let propName of Object.keys(schema.patternProperties || {})) {
      let pattern;
      try {
        pattern = parsePattern(propName);
      } catch (e) {
        throw new Error(`Internal error: Invalid property pattern ${JSON.stringify(propName)}`);
      }

      patternProperties.push({
        pattern,
        type: parseProperty(schema.patternProperties[propName]),
      });
    }

    // Parse "additionalProperties" schema.
    let additionalProperties = null;
    if (schema.additionalProperties) {
      additionalProperties = Schemas.parseSchema(schema.additionalProperties, path);
    }

    return new this(schema, properties, additionalProperties, patternProperties, schema.isInstanceOf || null);
  }

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

  /**
   * Extracts the enumerable properties of the given object, including
   * function properties which would normally be omitted by X-ray
   * wrappers.
   *
   * @param {object} value
   * @param {Context} context
   *        The current parse context.
   * @returns {object}
   *        An object with an `error` or `value` property.
   */
  extractProperties(value, context) {
    // |value| should be a JS Xray wrapping an object in the
    // extension compartment. This works well except when we need to
    // access callable properties on |value| since JS Xrays don't
    // support those. To work around the problem, we verify that
    // |value| is a plain JS object (i.e., not anything scary like a
    // Proxy). Then we copy the properties out of it into a normal
    // object using a waiver wrapper.

    let klass = Cu.getClassName(value, true);
    if (klass != "Object") {
      throw context.error(`Expected a plain JavaScript object, got a ${klass}`,
                          `be a plain JavaScript object`);
    }

    let properties = Object.create(null);

    let waived = Cu.waiveXrays(value);
    for (let prop of Object.getOwnPropertyNames(waived)) {
      let desc = Object.getOwnPropertyDescriptor(waived, prop);
      if (desc.get || desc.set) {
        throw context.error("Objects cannot have getters or setters on properties",
                            "contain no getter or setter properties");
      }
      // Chrome ignores non-enumerable properties.
      if (desc.enumerable) {
        properties[prop] = Cu.unwaiveXrays(desc.value);
      }
    }

    return properties;
  }

  checkProperty(context, prop, propType, result, properties, remainingProps) {
    let {type, optional, unsupported, onError} = propType;
    let error = null;

    if (unsupported) {
      if (prop in properties) {
        error = context.error(`Property "${prop}" is unsupported by Firefox`,
                              `not contain an unsupported "${prop}" property`);
      }
    } else if (prop in properties) {
      if (optional && (properties[prop] === null || properties[prop] === undefined)) {
        result[prop] = null;
      } else {
        let r = context.withPath(prop, () => type.normalize(properties[prop], context));
        if (r.error) {
          error = r;
        } else {
          result[prop] = r.value;
          properties[prop] = r.value;
        }
      }
      remainingProps.delete(prop);
    } else if (!optional) {
      error = context.error(`Property "${prop}" is required`,
                            `contain the required "${prop}" property`);
    } else if (optional !== "omit-key-if-missing") {
      result[prop] = null;
    }

    if (error) {
      if (onError == "warn") {
        context.logError(error.error);
      } else if (onError != "ignore") {
        throw error;
      }

      result[prop] = null;
    }
  }

  normalize(value, context) {
    try {
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
        return this.postprocess({value}, context);
      }

      let properties = this.extractProperties(value, context);
      let remainingProps = new Set(Object.keys(properties));

      let result = {};
      for (let prop of Object.keys(this.properties)) {
        this.checkProperty(context, prop, this.properties[prop], result,
                           properties, remainingProps);
      }

      for (let prop of Object.keys(properties)) {
        for (let {pattern, type} of this.patternProperties) {
          if (pattern.test(prop)) {
            this.checkProperty(context, prop, type, result,
                               properties, remainingProps);
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

      return this.postprocess({value: result}, context);
    } catch (e) {
      if (e.error) {
        return e;
      }
      throw e;
    }
  }
}

// This type is just a placeholder to be referred to by
// SubModuleProperty. No value is ever expected to have this type.
SubModuleType = class SubModuleType extends Type {
  static get EXTRA_PROPERTIES() {
    return ["functions", "events", "properties", ...super.EXTRA_PROPERTIES];
  }

  static parseSchema(schema, path, extraProperties = []) {
    this.checkSchemaProperties(schema, path, extraProperties);

    // The path we pass in here is only used for error messages.
    path = [...path, schema.id];
    let functions = schema.functions.map(fun => Schemas.parseFunction(path, fun));

    return new this(functions);
  }

  constructor(functions) {
    super();
    this.functions = functions;
  }
};

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
  static get EXTRA_PROPERTIES() {
    return ["minimum", "maximum", ...super.EXTRA_PROPERTIES];
  }

  static parseSchema(schema, path, extraProperties = []) {
    this.checkSchemaProperties(schema, path, extraProperties);

    return new this(schema, schema.minimum || -Infinity, schema.maximum || Infinity);
  }

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

    return this.postprocess(r, context);
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
  static get EXTRA_PROPERTIES() {
    return ["items", "minItems", "maxItems", ...super.EXTRA_PROPERTIES];
  }

  static parseSchema(schema, path, extraProperties = []) {
    this.checkSchemaProperties(schema, path, extraProperties);

    let items = Schemas.parseSchema(schema.items, path);

    return new this(schema, items, schema.minItems || 0, schema.maxItems || Infinity);
  }

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

    return this.postprocess({value: result}, context);
  }

  checkBaseType(baseType) {
    return baseType == "array";
  }
}

class FunctionType extends Type {
  static get EXTRA_PROPERTIES() {
    return ["parameters", "async", "returns", ...super.EXTRA_PROPERTIES];
  }

  static parseSchema(schema, path, extraProperties = []) {
    this.checkSchemaProperties(schema, path, extraProperties);

    let isAsync = !!schema.async;
    let isExpectingCallback = isAsync;
    let parameters = null;
    if ("parameters" in schema) {
      parameters = [];
      for (let param of schema.parameters) {
        // Callbacks default to optional for now, because of promise
        // handling.
        let isCallback = isAsync && param.name == schema.async;
        if (isCallback) {
          isExpectingCallback = false;
        }

        parameters.push({
          type: Schemas.parseSchema(param, path, ["name", "optional", "default"]),
          name: param.name,
          optional: param.optional == null ? isCallback : param.optional,
          default: param.default == undefined ? null : param.default,
        });
      }
    }
    if (isExpectingCallback) {
      throw new Error(`Internal error: Expected a callback parameter with name ${schema.async}`);
    }

    let hasAsyncCallback = false;
    if (isAsync) {
      if (parameters && parameters.length && parameters[parameters.length - 1].name == schema.async) {
        hasAsyncCallback = true;
      }
      if (schema.returns) {
        throw new Error("Internal error: Async functions must not have return values.");
      }
      if (schema.allowAmbiguousOptionalArguments && !hasAsyncCallback) {
        throw new Error("Internal error: Async functions with ambiguous arguments must declare the callback as the last parameter");
      }
    }

    return new this(schema, parameters, isAsync, hasAsyncCallback);
  }

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

    let apiImpl = context.getImplementation(path.join("."), name);

    let getStub = () => {
      this.checkDeprecated(context);
      return apiImpl.getProperty();
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

        apiImpl.setProperty(normalized.value);
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
  constructor(schema, name, namespaceName, reference, properties) {
    super(schema);
    this.name = name;
    this.namespaceName = namespaceName;
    this.reference = reference;
    this.properties = properties;
  }

  inject(path, name, dest, context) {
    exportLazyGetter(dest, name, () => {
      let obj = Cu.createObjectIn(dest);

      let ns = Schemas.namespaces.get(this.namespaceName);
      let type = ns.get(this.reference);
      if (!type && this.reference.includes(".")) {
        let [namespaceName, ref] = this.reference.split(".");
        ns = Schemas.namespaces.get(namespaceName);
        type = ns.get(ref);
      }
      if (!type || !(type instanceof SubModuleType)) {
        throw new Error(`Internal error: ${this.namespaceName}.${this.reference} is not a sub-module`);
      }

      let functions = type.functions;
      for (let fun of functions) {
        let subpath = path.concat(name);
        let namespace = subpath.join(".");
        let allowedContexts = fun.allowedContexts.length ? fun.allowedContexts : ns.defaultContexts;
        if (context.shouldInject(namespace, fun.name, allowedContexts)) {
          fun.inject(subpath, fun.name, obj, context);
        }
      }

      // TODO: Inject this.properties.

      return obj;
    });
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
    // |args| but with default values in place of omitted optional parameters.
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
        fixedArgs[parameterIndex] = parameter.default;
        if (check(parameterIndex + 1, argIndex)) {
          return true;
        }
      }

      if (argIndex == args.length) {
        return false;
      }

      let arg = args[argIndex];
      if (!parameter.type.checkBaseType(getValueBaseType(arg))) {
        // For Chrome compatibility, use the default value if null or undefined
        // is explicitly passed but is not a valid argument in this position.
        if (parameter.optional && (arg === null || arg === undefined)) {
          fixedArgs[parameterIndex] = Cu.cloneInto(parameter.default, global);
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
      // The last argument for asynchronous methods is either a function or null.
      // This is specifically done for runtime.sendMessage.
      if (this.hasAsyncCallback && typeof(args[args.length - 1]) != "function") {
        args.push(null);
      }
      return args;
    }
    let success = check(0, 0);
    if (!success) {
      this.throwError(context, "Incorrect argument types");
    }

    // Now we normalize (and fully type check) all non-omitted arguments.
    fixedArgs = fixedArgs.map((arg, parameterIndex) => {
      if (arg === null) {
        return null;
      }
      let parameter = this.parameters[parameterIndex];
      let r = parameter.type.normalize(arg, context);
      if (r.error) {
        this.throwError(context, `Type error for parameter ${parameter.name} (${r.error})`);
      }
      return r.value;
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

    exportLazyGetter(dest, name, () => {
      let apiImpl = context.getImplementation(path.join("."), name);

      let stub;
      if (this.isAsync) {
        stub = (...args) => {
          this.checkDeprecated(context);
          let actuals = this.checkParameters(args, context);
          let callback = null;
          if (this.hasAsyncCallback) {
            callback = actuals.pop();
          }
          if (callback === null && context.isChromeCompat) {
            // We pass an empty stub function as a default callback for
            // the `chrome` API, so promise objects are not returned,
            // and lastError values are reported immediately.
            callback = () => {};
          }
          return apiImpl.callAsyncFunction(actuals, callback);
        };
      } else if (!this.returns) {
        stub = (...args) => {
          this.checkDeprecated(context);
          let actuals = this.checkParameters(args, context);
          return apiImpl.callFunctionNoReturn(actuals);
        };
      } else {
        stub = (...args) => {
          this.checkDeprecated(context);
          let actuals = this.checkParameters(args, context);
          return apiImpl.callFunction(actuals);
        };
      }
      return Cu.exportFunction(stub, dest);
    });
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

    exportLazyGetter(dest, name, () => {
      let apiImpl = context.getImplementation(path.join("."), name);

      let addStub = (listener, ...args) => {
        listener = this.checkListener(listener, context);
        let actuals = this.checkParameters(args, context);
        apiImpl.addListener(listener, actuals);
      };

      let removeStub = (listener) => {
        listener = this.checkListener(listener, context);
        apiImpl.removeListener(listener);
      };

      let hasStub = (listener) => {
        listener = this.checkListener(listener, context);
        return apiImpl.hasListener(listener);
      };

      let obj = Cu.createObjectIn(dest);

      Cu.exportFunction(addStub, obj, {defineAs: "addListener"});
      Cu.exportFunction(removeStub, obj, {defineAs: "removeListener"});
      Cu.exportFunction(hasStub, obj, {defineAs: "hasListener"});

      return obj;
    });
  }
}

const TYPES = Object.freeze(Object.assign(Object.create(null), {
  any: AnyType,
  array: ArrayType,
  boolean: BooleanType,
  function: FunctionType,
  integer: IntegerType,
  number: NumberType,
  object: ObjectType,
  string: StringType,
}));

this.Schemas = {
  initialized: false,

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
      ns.name = namespaceName;
      ns.permissions = null;
      ns.allowedContexts = [];
      ns.defaultContexts = [];
      this.namespaces.set(namespaceName, ns);
    }
    ns.set(symbol, value);
  },

  parseSchema(schema, path, extraProperties = []) {
    let allowedProperties = new Set(extraProperties);

    if ("choices" in schema) {
      return ChoiceType.parseSchema(schema, path, allowedProperties);
    } else if ("$ref" in schema) {
      return RefType.parseSchema(schema, path, allowedProperties);
    }

    if (!("type" in schema)) {
      throw new Error(`Unexpected value for type: ${JSON.stringify(schema)}`);
    }

    allowedProperties.add("type");

    let type = TYPES[schema.type];
    if (!type) {
      throw new Error(`Unexpected type ${schema.type}`);
    }
    return type.parseSchema(schema, path, allowedProperties);
  },

  parseFunction(path, fun) {
    let f = new FunctionEntry(fun, path, fun.name,
                              this.parseSchema(fun, path,
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
      this.register(namespaceName, type.id, this.parseSchema(type, [namespaceName], ["id"]));
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

    let parsed = this.parseSchema(type, [namespaceName], ["$extend"]);
    if (parsed.constructor !== targetType.constructor) {
      throw new Error(`Internal error: Bad attempt to extend ${type.$extend}`);
    }

    targetType.extend(parsed);
  },

  loadProperty(namespaceName, name, prop) {
    if ("$ref" in prop) {
      if (!prop.unsupported) {
        this.register(namespaceName, name, new SubModuleProperty(prop, name, namespaceName, prop.$ref,
                                                                 prop.properties || {}));
      }
    } else if ("value" in prop) {
      this.register(namespaceName, name, new ValueProperty(prop, name, prop.value));
    } else {
      // We ignore the "optional" attribute on properties since we
      // don't inject anything here anyway.
      let type = this.parseSchema(prop, [namespaceName], ["optional", "writable"]);
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
        type: this.parseSchema(param, [namespaceName], ["name", "optional", "default"]),
        name: param.name,
        optional: param.optional || false,
        default: param.default == undefined ? null : param.default,
      };
    });

    // We ignore these properties for now.
    /* eslint-disable no-unused-vars */
    let returns = event.returns;
    let filters = event.filters;
    /* eslint-enable no-unused-vars */

    let type = this.parseSchema(event, [namespaceName],
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

    this.flushSchemas();
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "Schema:Add":
        this.schemaJSON.set(msg.data.url, msg.data.schema);
        this.flushSchemas();
        break;

      case "Schema:Delete":
        this.schemaJSON.delete(msg.data.url);
        this.flushSchemas();
        break;
    }
  },

  flushSchemas() {
    XPCOMUtils.defineLazyGetter(this, "namespaces",
                                () => this.parseSchemas());
  },

  parseSchemas() {
    Object.defineProperty(this, "namespaces", {
      enumerable: true,
      configurable: true,
      value: new Map(),
    });

    for (let json of this.schemaJSON.values()) {
      this.loadSchema(json);
    }

    return this.namespaces;
  },

  loadSchema(json) {
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

      let ns = this.namespaces.get(name);
      ns.permissions = namespace.permissions || null;
      ns.allowedContexts = namespace.allowedContexts || [];
      ns.defaultContexts = namespace.defaultContexts || [];
    }
  },

  load(url) {
    if (Services.appinfo.processType != Services.appinfo.PROCESS_TYPE_CONTENT) {
      return readJSON(url).then(json => {
        this.schemaJSON.set(url, json);

        let data = Services.ppmm.initialProcessData;
        data["Extension:Schemas"] = this.schemaJSON;

        Services.ppmm.broadcastAsyncMessage("Schema:Add", {url, schema: json});

        this.flushSchemas();
      });
    }
  },

  unload(url) {
    this.schemaJSON.delete(url);

    let data = Services.ppmm.initialProcessData;
    data["Extension:Schemas"] = this.schemaJSON;

    Services.ppmm.broadcastAsyncMessage("Schema:Delete", {url});

    this.flushSchemas();
  },

  /**
   * Checks whether a given object has the necessary permissions to
   * expose the given namespace.
   *
   * @param {string} namespace
   *        The top-level namespace to check permissions for.
   * @param {object} wrapperFuncs
   *        Wrapper functions for the given context.
   * @param {function} wrapperFuncs.hasPermission
   *        A function which, when given a string argument, returns true
   *        if the context has the given permission.
   * @returns {boolean}
   *        True if the context has permission for the given namespace.
   */
  checkPermissions(namespace, wrapperFuncs) {
    let ns = this.namespaces.get(namespace);
    if (ns && ns.permissions) {
      return ns.permissions.some(perm => wrapperFuncs.hasPermission(perm));
    }
    return true;
  },

  exportLazyGetter,

  /**
   * Inject registered extension APIs into `dest`.
   *
   * @param {object} dest The root namespace for the APIs.
   *     This object is usually exposed to extensions as "chrome" or "browser".
   * @param {object} wrapperFuncs An implementation of the InjectionContext
   *     interface, which runs the actual functionality of the generated API.
   */
  inject(dest, wrapperFuncs) {
    let context = new InjectionContext(wrapperFuncs);

    let createNamespace = ns => {
      let obj = Cu.createObjectIn(dest);

      for (let [name, entry] of ns) {
        let allowedContexts = entry.allowedContexts;
        if (!allowedContexts.length) {
          allowedContexts = ns.defaultContexts;
        }

        if (context.shouldInject(ns.name, name, allowedContexts)) {
          entry.inject([ns.name], name, obj, context);
        }
      }

      // Remove the namespace object if it is empty
      if (Object.keys(obj).length) {
        return obj;
      }
    };

    let createNestedNamespaces = (parent, namespaces) => {
      for (let [prop, namespace] of namespaces) {
        if (namespace instanceof DeepMap) {
          exportLazyGetter(parent, prop, () => {
            let obj = Cu.createObjectIn(parent);
            createNestedNamespaces(obj, namespace);
            return obj;
          });
        } else {
          exportLazyGetter(parent, prop,
                           () => createNamespace(namespace));
        }
      }
    };

    let nestedNamespaces = new DeepMap();
    for (let ns of this.namespaces.values()) {
      if (ns.permissions && !ns.permissions.some(perm => context.hasPermission(perm))) {
        continue;
      }

      if (!wrapperFuncs.shouldInject(ns.name, null, ns.allowedContexts)) {
        continue;
      }

      if (ns.name.includes(".")) {
        let path = ns.name.split(".");
        let leafName = path.pop();

        let parent = nestedNamespaces.getPath(...path);

        parent.set(leafName, ns);
      } else {
        exportLazyGetter(dest, ns.name,
                         () => createNamespace(ns));
      }
    }

    createNestedNamespaces(dest, nestedNamespaces);
  },

  /**
   * Normalize `obj` according to the loaded schema for `typeName`.
   *
   * @param {object} obj The object to normalize against the schema.
   * @param {string} typeName The name in the format namespace.propertyname
   * @param {object} context An implementation of Context. Any validation errors
   *     are reported to the given context.
   * @returns {object} The normalized object.
   */
  normalize(obj, typeName, context) {
    let [namespaceName, prop] = typeName.split(".");
    let ns = this.namespaces.get(namespaceName);
    let type = ns.get(prop);

    return type.normalize(obj, new Context(context));
  },
};
