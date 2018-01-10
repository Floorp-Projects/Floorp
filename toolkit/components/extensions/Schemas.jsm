/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
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

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  DefaultMap,
  DefaultWeakMap,
} = ExtensionUtils;

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionParent",
                                  "resource://gre/modules/ExtensionParent.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "contentPolicyService",
                                   "@mozilla.org/addons/content-policy;1",
                                   "nsIAddonContentPolicy");

XPCOMUtils.defineLazyGetter(this, "StartupCache", () => ExtensionParent.StartupCache);

this.EXPORTED_SYMBOLS = ["SchemaRoot", "Schemas"];

const {DEBUG} = AppConstants;

const isParentProcess = Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT;

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

function stripDescriptions(json, stripThis = true) {
  if (Array.isArray(json)) {
    for (let i = 0; i < json.length; i++) {
      if (typeof json[i] === "object" && json[i] !== null) {
        json[i] = stripDescriptions(json[i]);
      }
    }
    return json;
  }

  let result = {};

  // Objects are handled much more efficiently, both in terms of memory and
  // CPU, if they have the same shape as other objects that serve the same
  // purpose. So, normalize the order of properties to increase the chances
  // that the majority of schema objects wind up in large shape groups.
  for (let key of Object.keys(json).sort()) {
    if (stripThis && key === "description" && typeof json[key] === "string") {
      continue;
    }

    if (typeof json[key] === "object" && json[key] !== null) {
      result[key] = stripDescriptions(json[key], key !== "properties");
    } else {
      result[key] = json[key];
    }
  }

  return result;
}

function blobbify(json) {
  // We don't actually use descriptions at runtime, and they make up about a
  // third of the size of our structured clone data, so strip them before
  // blobbifying.
  json = stripDescriptions(json);

  return new StructuredCloneHolder(json);
}

async function readJSONAndBlobbify(url) {
  let json = await readJSON(url);

  return blobbify(json);
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
  object = ChromeUtils.waiveXrays(object);

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

/**
 * Defines a lazily-instantiated property descriptor on the given
 * object. Any security wrappers are waived on the object before the
 * property is defined.
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
 *        descriptor object. This will be called, and the property
 *        descriptor installed on the object, the first time the
 *        property is written or read. The function may return
 *        undefined, which will cause the property to be deleted.
 */
function exportLazyProperty(object, prop, getter) {
  object = ChromeUtils.waiveXrays(object);

  let redefine = obj => {
    let desc = getter.call(obj);
    getter = null;

    delete object[prop];
    if (desc) {
      let defaults = {
        configurable: true,
        enumerable: true,
      };

      if (!desc.set && !desc.get) {
        defaults.writable = true;
      }

      Object.defineProperty(object, prop,
                            Object.assign(defaults, desc));
    }
  };

  Object.defineProperty(object, prop, {
    enumerable: true,
    configurable: true,

    get: Cu.exportFunction(function() {
      redefine(this);
      return object[prop];
    }, object),

    set: Cu.exportFunction(function(value) {
      redefine(this);
      object[prop] = value;
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
  let type = typeof value;
  switch (type) {
    case "object":
      if (value === null) {
        return "null";
      }
      if (Array.isArray(value)) {
        return "array";
      }
      break;

    case "number":
      if (value % 1 === 0) {
        return "integer";
      }
  }
  return type;
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
  "getImplementation",
  "isPermissionRevokable",
  "shouldInject",
];

// If the message is a function, call it and return the result.
// Otherwise, assume it's a string.
function forceString(msg) {
  if (typeof msg === "function") {
    return msg();
  }
  return msg;
}

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
      ssm.checkLoadURIWithPrincipal(this.principal,
                                    Services.io.newURI(url),
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
   * Checks whether the given permission can be dynamically revoked or
   * granted.
   *
   * @param {string} permission
   *        The name of the permission to check.
   *
   * @returns {boolean} True if the given permission is revokable.
   */
  isPermissionRevokable(permission) {
    return false;
  }

  /**
   * Returns an error result object with the given message, for return
   * by Type normalization functions.
   *
   * If the context has a `currentTarget` value, this is prepended to
   * the message to indicate the location of the error.
   *
   * @param {string|function} errorMessage
   *        The error message which will be displayed when this is the
   *        only possible matching schema. If a function is passed, it
   *        will be evaluated when the error string is first needed, and
   *        must return a string.
   * @param {string|function} choicesMessage
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
      let {currentTarget} = this;
      return {error: () => `Error processing ${currentTarget}: ${forceString(errorMessage)}`};
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
    let error = forceString(this.error(message).error);
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

      return {result, choices};
    } finally {
      this.currentChoices = currentChoices;
      this.choicePathIndex = choicePathIndex;

      if (choices.size == 1) {
        for (let choice of choices) {
          currentChoices.add(choice);
        }
      } else if (choices.size) {
        this.error(null, () => {
          let array = Array.from(choices, forceString);
          let n = array.length - 1;
          array[n] = `or ${array[n]}`;

          return `must either [${array.join(", ")}]`;
        });
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
 * Represents a schema entry to be injected into an object. Handles the
 * injection, revocation, and permissions of said entry.
 *
 * @param {InjectionContext} context
 *        The injection context for the entry.
 * @param {Entry} entry
 *        The entry to inject.
 * @param {object} parentObject
 *        The object into which to inject this entry.
 * @param {string} name
 *        The property name at which to inject this entry.
 * @param {Array<string>} path
 *        The full path from the root entry to this entry.
 * @param {Entry} parentEntry
 *        The parent entry for the injected entry.
 */
class InjectionEntry {
  constructor(context, entry, parentObj, name, path, parentEntry) {
    this.context = context;
    this.entry = entry;
    this.parentObj = parentObj;
    this.name = name;
    this.path = path;
    this.parentEntry = parentEntry;

    this.injected = null;
    this.lazyInjected = null;
  }

  /**
   * @property {Array<string>} allowedContexts
   *        The list of allowed contexts into which the entry may be
   *        injected.
   */
  get allowedContexts() {
    let {allowedContexts} = this.entry;
    if (allowedContexts.length) {
      return allowedContexts;
    }
    return this.parentEntry.defaultContexts;
  }

  /**
   * @property {boolean} isRevokable
   *        Returns true if this entry may be dynamically injected or
   *        revoked based on its permissions.
   */
  get isRevokable() {
    return (this.entry.permissions &&
            this.entry.permissions.some(perm => this.context.isPermissionRevokable(perm)));
  }

  /**
   * @property {boolean} hasPermission
   *        Returns true if the injection context currently has the
   *        appropriate permissions to access this entry.
   */
  get hasPermission() {
    return (!this.entry.permissions ||
            this.entry.permissions.some(perm => this.context.hasPermission(perm)));
  }

  /**
   * @property {boolean} shouldInject
   *        Returns true if this entry should be injected in the given
   *        context, without respect to permissions.
   */
  get shouldInject() {
    return this.context.shouldInject(this.path.join("."), this.name, this.allowedContexts);
  }

  /**
   * Revokes this entry, removing its property from its parent object,
   * and invalidating its wrappers.
   */
  revoke() {
    if (this.lazyInjected) {
      this.lazyInjected = false;
    } else if (this.injected) {
      if (this.injected.revoke) {
        this.injected.revoke();
      }

      try {
        let unwrapped = ChromeUtils.waiveXrays(this.parentObj);
        delete unwrapped[this.name];
      } catch (e) {
        Cu.reportError(e);
      }

      let {value} = this.injected.descriptor;
      if (value) {
        this.context.revokeChildren(value);
      }

      this.injected = null;
    }
  }

  /**
   * Returns a property descriptor object for this entry, if it should
   * be injected, or undefined if it should not.
   *
   * @returns {object?}
   *        A property descriptor object, or undefined if the property
   *        should be removed.
   */
  getDescriptor() {
    this.lazyInjected = false;

    if (this.injected) {
      let path = [...this.path, this.name];
      throw new Error(`Attempting to re-inject already injected entry: ${path.join(".")}`);
    }

    if (!this.shouldInject) {
      return;
    }

    if (this.isRevokable) {
      this.context.pendingEntries.add(this);
    }

    if (!this.hasPermission) {
      return;
    }

    this.injected = this.entry.getDescriptor(this.path, this.context);
    if (!this.injected) {
      return undefined;
    }

    return this.injected.descriptor;
  }

  /**
   * Injects a lazy property descriptor into the parent object which
   * checks permissions and eligibility for injection the first time it
   * is accessed.
   */
  lazyInject() {
    if (this.lazyInjected || this.injected) {
      let path = [...this.path, this.name];
      throw new Error(`Attempting to re-lazy-inject already injected entry: ${path.join(".")}`);
    }

    this.lazyInjected = true;
    exportLazyProperty(this.parentObj, this.name, () => {
      if (this.lazyInjected) {
        return this.getDescriptor();
      }
    });
  }

  /**
   * Injects or revokes this entry if its current state does not match
   * the context's current permissions.
   */
  permissionsChanged() {
    if (this.injected) {
      this.maybeRevoke();
    } else {
      this.maybeInject();
    }
  }

  maybeInject() {
    if (!this.injected && !this.lazyInjected) {
      this.lazyInject();
    }
  }

  maybeRevoke() {
    if (this.injected && !this.hasPermission) {
      this.revoke();
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

    this.pendingEntries = new Set();
    this.children = new DefaultWeakMap(() => new Map());

    if (params.setPermissionsChangedCallback) {
      params.setPermissionsChangedCallback(
        this.permissionsChanged.bind(this));
    }
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

  /**
   * Updates all injection entries which may need to be updated after a
   * permission change, revoking or re-injecting them as necessary.
   */
  permissionsChanged() {
    for (let entry of this.pendingEntries) {
      try {
        entry.permissionsChanged();
      } catch (e) {
        Cu.reportError(e);
      }
    }
  }

  /**
   * Recursively revokes all child injection entries of the given
   * object.
   *
   * @param {object} object
   *        The object for which to invoke children.
   */
  revokeChildren(object) {
    if (!this.children.has(object)) {
      return;
    }

    let children = this.children.get(object);
    for (let [name, entry] of children.entries()) {
      try {
        entry.revoke();
      } catch (e) {
        Cu.reportError(e);
      }
      children.delete(name);

      // When we revoke children for an object, we consider that object
      // dead. If the entry is ever reified again, a new object is
      // created, with new child entries.
      this.pendingEntries.delete(entry);
    }
    this.children.delete(object);
  }

  _getInjectionEntry(entry, dest, name, path, parentEntry) {
    let injection = new InjectionEntry(this, entry, dest, name, path, parentEntry);

    this.children.get(dest).set(name, injection);

    return injection;
  }

  /**
   * Returns the property descriptor for the given entry.
   *
   * @param {Entry} entry
   *        The entry instance to return a descriptor for.
   * @param {object} dest
   *        The object into which this entry is being injected.
   * @param {string} name
   *        The property name on the destination object where the entry
   *        will be injected.
   * @param {Array<string>} path
   *        The full path from the root injection object to this entry.
   * @param {Entry} parentEntry
   *        The parent entry for this entry.
   *
   * @returns {object?}
   *        A property descriptor object, or null if the entry should
   *        not be injected.
   */
  getDescriptor(entry, dest, name, path, parentEntry) {
    let injection = this._getInjectionEntry(entry, dest, name, path, parentEntry);

    return injection.getDescriptor();
  }

  /**
   * Lazily injects the given entry into the given object.
   *
   * @param {Entry} entry
   *        The entry instance to lazily inject.
   * @param {object} dest
   *        The object into which to inject this entry.
   * @param {string} name
   *        The property name at which to inject the entry.
   * @param {Array<string>} path
   *        The full path from the root injection object to this entry.
   * @param {Entry} parentEntry
   *        The parent entry for this entry.
   */
  injectInto(entry, dest, name, path, parentEntry) {
    let injection = this._getInjectionEntry(entry, dest, name, path, parentEntry);

    injection.lazyInject();
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
  hostname(string, context) {
    let valid = true;

    try {
      valid = new URL(`http://${string}`).host === string;
    } catch (e) {
      valid = false;
    }

    if (!valid) {
      throw new Error(`Invalid hostname ${string}`);
    }

    return string;
  },

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
    void FORMATS.unresolvedRelativeUrl(string, context);
    return FORMATS.relativeUrl(string, context);
  },

  unresolvedRelativeUrl(string, context) {
    if (!string.startsWith("//")) {
      try {
        new URL(string);
      } catch (e) {
        return string;
      }
    }

    throw new SyntaxError(`String ${JSON.stringify(string)} must be a relative URL`);
  },

  imageDataOrStrictRelativeUrl(string, context) {
    // Do not accept a string which resolves as an absolute URL, or any
    // protocol-relative URL, except PNG or JPG data URLs
    if (!string.startsWith("data:image/png;base64,") && !string.startsWith("data:image/jpeg;base64,")) {
      try {
        return FORMATS.strictRelativeUrl(string, context);
      } catch (e) {
        throw new SyntaxError(`String ${JSON.stringify(string)} must be a relative or PNG or JPG data:image URL`);
      }
    }
    return string;
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
   * Returns an object containing property descriptor for use when
   * injecting this entry into an API object.
   *
   * @param {Array<string>} path The API path, e.g. `["storage", "local"]`.
   * @param {InjectionContext} context
   *
   * @returns {object?}
   *        An object containing a `descriptor` property, specifying the
   *        entry's property descriptor, and an optional `revoke`
   *        method, to be called when the entry is being revoked.
   */
  getDescriptor(path, context) {
    return undefined;
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
   * @param {SchemaRoot} root
   *        The root schema for this type.
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
  static parseSchema(root, schema, path, extraProperties = []) {
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
    if (DEBUG) {
      let allowedSet = new Set([...this.EXTRA_PROPERTIES, ...extra]);

      for (let prop of Object.keys(schema)) {
        if (!allowedSet.has(prop)) {
          throw new Error(`Internal error: Namespace ${path.join(".")} has ` +
                          `invalid type property "${prop}" ` +
                          `in type "${schema.id || JSON.stringify(schema)}"`);
        }
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
    if ("aeiou".includes(type[0])) {
      choice = `be an ${type} value`;
    } else {
      choice = `be a ${type} value`;
    }

    return context.error(() => `Expected ${type} instead of ${JSON.stringify(value)}`,
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

  static parseSchema(root, schema, path, extraProperties = []) {
    this.checkSchemaProperties(schema, path, extraProperties);

    let choices = schema.choices.map(t => root.parseSchema(t, path));
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
    if (choices.size <= 1) {
      return error;
    }

    choices = Array.from(choices, forceString);
    let n = choices.length - 1;
    choices[n] = `or ${choices[n]}`;

    let message;
    if (typeof value === "object") {
      message = () => `Value must either: ${choices.join(", ")}`;
    } else {
      message = () => `Value ${JSON.stringify(value)} must either: ${choices.join(", ")}`;
    }

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

  static parseSchema(root, schema, path, extraProperties = []) {
    this.checkSchemaProperties(schema, path, extraProperties);

    let ref = schema.$ref;
    let ns = path.join(".");
    if (ref.includes(".")) {
      [, ns, ref] = /^(.*)\.(.*?)$/.exec(ref);
    }
    return new this(root, schema, ns, ref);
  }

  // For a reference to a type named T declared in namespace NS,
  // namespaceName will be NS and reference will be T.
  constructor(root, schema, namespaceName, reference) {
    super(schema);
    this.root = root;
    this.namespaceName = namespaceName;
    this.reference = reference;
  }

  get targetType() {
    let ns = this.root.getNamespace(this.namespaceName);
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

  static parseSchema(root, schema, path, extraProperties = []) {
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
    return new this(schema,
                    schema.id || undefined,
                    enumeration,
                    schema.minLength || 0,
                    schema.maxLength || Infinity,
                    pattern,
                    format);
  }

  constructor(schema, name, enumeration, minLength, maxLength, pattern, format) {
    super(schema);
    this.name = name;
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

      return context.error(() => `Invalid enumeration value ${JSON.stringify(value)}`,
                           `be one of [${choices}]`);
    }

    if (value.length < this.minLength) {
      return context.error(() => `String ${JSON.stringify(value)} is too short (must be ${this.minLength})`,
                           `be longer than ${this.minLength}`);
    }
    if (value.length > this.maxLength) {
      return context.error(() => `String ${JSON.stringify(value)} is too long (must be ${this.maxLength})`,
                           `be shorter than ${this.maxLength}`);
    }

    if (this.pattern && !this.pattern.test(value)) {
      return context.error(() => `String ${JSON.stringify(value)} must match ${this.pattern}`,
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

  getDescriptor(path, context) {
    if (this.enumeration) {
      let obj = Cu.createObjectIn(context.cloneScope);

      for (let e of this.enumeration) {
        obj[e.toUpperCase()] = e;
      }

      return {
        descriptor: {value: obj},
      };
    }
  }
}

class NullType extends Type {
  normalize(value, context) {
    return this.normalizeBase("null", value, context);
  }

  checkBaseType(baseType) {
    return baseType == "null";
  }
}

let FunctionEntry;
let Event;
let SubModuleType;

class ObjectType extends Type {
  static get EXTRA_PROPERTIES() {
    return ["properties", "patternProperties", "$import", ...super.EXTRA_PROPERTIES];
  }

  static parseSchema(root, schema, path, extraProperties = []) {
    if ("functions" in schema) {
      return SubModuleType.parseSchema(root, schema, path, extraProperties);
    }

    if (DEBUG && !("$extend" in schema)) {
      // Only allow extending "properties" and "patternProperties".
      extraProperties = ["additionalProperties", "isInstanceOf", ...extraProperties];
    }
    this.checkSchemaProperties(schema, path, extraProperties);

    let imported = null;
    if ("$import" in schema) {
      let importPath = schema.$import;
      let idx = importPath.indexOf(".");
      if (idx === -1) {
        imported = [path[0], importPath];
      } else {
        imported = [importPath.slice(0, idx), importPath.slice(idx + 1)];
      }
    }

    let parseProperty = (schema, extraProps = []) => {
      return {
        type: root.parseSchema(schema, path,
                               DEBUG && ["unsupported", "onError", "permissions", "default", ...extraProps]),
        optional: schema.optional || false,
        unsupported: schema.unsupported || false,
        onError: schema.onError || null,
        default: schema.default === undefined ? null : schema.default,
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
      let type = schema.additionalProperties;
      if (type === true) {
        type = {"type": "any"};
      }

      additionalProperties = root.parseSchema(type, path);
    }

    return new this(schema, properties, additionalProperties, patternProperties, schema.isInstanceOf || null, imported);
  }

  constructor(schema, properties, additionalProperties, patternProperties, isInstanceOf, imported) {
    super(schema);
    this.properties = properties;
    this.additionalProperties = additionalProperties;
    this.patternProperties = patternProperties;
    this.isInstanceOf = isInstanceOf;

    if (imported) {
      let [ns, path] = imported;
      ns = Schemas.getNamespace(ns);
      let importedType = ns.get(path);
      if (!importedType) {
        throw new Error(`Internal error: imported type ${path} not found`);
      }

      if (DEBUG && !(importedType instanceof ObjectType)) {
        throw new Error(`Internal error: cannot import non-object type ${path}`);
      }

      this.properties = Object.assign({}, importedType.properties, this.properties);
      this.patternProperties = [...importedType.patternProperties,
                                ...this.patternProperties];
      this.additionalProperties = importedType.additionalProperties || this.additionalProperties;
    }
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

    let klass = ChromeUtils.getClassName(value, true);
    if (klass != "Object") {
      throw context.error(`Expected a plain JavaScript object, got a ${klass}`,
                          `be a plain JavaScript object`);
    }

    return ChromeUtils.shallowClone(value);
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
        result[prop] = propType.default;
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
      result[prop] = propType.default;
    }

    if (error) {
      if (onError == "warn") {
        context.logError(forceString(error.error));
      } else if (onError != "ignore") {
        throw error;
      }

      result[prop] = propType.default;
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
        if (DEBUG) {
          if (Object.keys(this.properties).length ||
              this.patternProperties.length ||
              !(this.additionalProperties instanceof AnyType)) {
            throw new Error("InternalError: isInstanceOf can only be used " +
                            "with objects that are otherwise unrestricted");
          }
        }

        if (ChromeUtils.getClassName(value) !== this.isInstanceOf) {
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
          let r = context.withPath(prop, () => this.additionalProperties.normalize(properties[prop], context));
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

  static parseSchema(root, schema, path, extraProperties = []) {
    this.checkSchemaProperties(schema, path, extraProperties);

    // The path we pass in here is only used for error messages.
    path = [...path, schema.id];
    let functions = schema.functions.filter(fun => !fun.unsupported)
                          .map(fun => FunctionEntry.parseSchema(root, fun, path));

    let events = [];

    if (schema.events) {
      events = schema.events.filter(event => !event.unsupported)
                     .map(event => Event.parseSchema(root, event, path));
    }

    return new this(functions, events);
  }

  constructor(functions, events) {
    super();
    this.functions = functions;
    this.events = events;
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

  static parseSchema(root, schema, path, extraProperties = []) {
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

  static parseSchema(root, schema, path, extraProperties = []) {
    this.checkSchemaProperties(schema, path, extraProperties);

    let items = root.parseSchema(schema.items, path, ["onError"]);

    return new this(schema, items, schema.minItems || 0, schema.maxItems || Infinity);
  }

  constructor(schema, itemType, minItems, maxItems) {
    super(schema);
    this.itemType = itemType;
    this.minItems = minItems;
    this.maxItems = maxItems;
    this.onError = schema.items.onError || null;
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
        if (this.onError == "warn") {
          context.logError(forceString(element.error));
        } else if (this.onError != "ignore") {
          return element;
        }
        continue;
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
    return ["parameters", "async", "returns", "requireUserInput",
            ...super.EXTRA_PROPERTIES];
  }

  static parseSchema(root, schema, path, extraProperties = []) {
    this.checkSchemaProperties(schema, path, extraProperties);

    let isAsync = !!schema.async;
    let isExpectingCallback = typeof schema.async === "string";
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
          type: root.parseSchema(param, path, ["name", "optional", "default"]),
          name: param.name,
          optional: param.optional == null ? isCallback : param.optional,
          default: param.default == undefined ? null : param.default,
        });
      }
    }
    let hasAsyncCallback = false;
    if (isAsync) {
      hasAsyncCallback = (parameters &&
                          parameters.length &&
                          parameters[parameters.length - 1].name == schema.async);
    }

    if (DEBUG) {
      if (isExpectingCallback) {
        throw new Error(`Internal error: Expected a callback parameter ` +
                        `with name ${schema.async}`);
      }

      if (isAsync && schema.returns) {
        throw new Error("Internal error: Async functions must not " +
                        "have return values.");
      }
      if (isAsync && schema.allowAmbiguousOptionalArguments && !hasAsyncCallback) {
        throw new Error("Internal error: Async functions with ambiguous " +
                        "arguments must declare the callback as the last parameter");
      }
    }


    return new this(schema, parameters, isAsync, hasAsyncCallback,
                    !!schema.requireUserInput);
  }

  constructor(schema, parameters, isAsync, hasAsyncCallback, requireUserInput) {
    super(schema);
    this.parameters = parameters;
    this.isAsync = isAsync;
    this.hasAsyncCallback = hasAsyncCallback;
    this.requireUserInput = requireUserInput;
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

  getDescriptor(path, context) {
    return {
      descriptor: {value: this.value},
    };
  }
}

// Represents a "property" defined in a schema namespace that is not a
// constant.
class TypeProperty extends Entry {
  constructor(schema, path, name, type, writable, permissions) {
    super(schema);
    this.path = path;
    this.name = name;
    this.type = type;
    this.writable = writable;
    this.permissions = permissions;
  }

  throwError(context, msg) {
    throw context.makeError(`${msg} for ${this.path.join(".")}.${this.name}.`);
  }

  getDescriptor(path, context) {
    if (this.unsupported) {
      return;
    }

    let apiImpl = context.getImplementation(path.join("."), this.name);

    let getStub = () => {
      this.checkDeprecated(context);
      return apiImpl.getProperty();
    };

    let descriptor = {
      get: Cu.exportFunction(getStub, context.cloneScope),
    };

    if (this.writable) {
      let setStub = (value) => {
        let normalized = this.type.normalize(value, context);
        if (normalized.error) {
          this.throwError(context, forceString(normalized.error));
        }

        apiImpl.setProperty(normalized.value);
      };

      descriptor.set = Cu.exportFunction(setStub, context.cloneScope);
    }

    return {
      descriptor,
      revoke() {
        apiImpl.revoke();
        apiImpl = null;
      },
    };
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
  constructor(root, schema, path, name, reference, properties, permissions) {
    super(schema);
    this.root = root;
    this.name = name;
    this.path = path;
    this.namespaceName = path.join(".");
    this.reference = reference;
    this.properties = properties;
    this.permissions = permissions;
  }

  getDescriptor(path, context) {
    let obj = Cu.createObjectIn(context.cloneScope);

    let ns = this.root.getNamespace(this.namespaceName);
    let type = ns.get(this.reference);
    if (!type && this.reference.includes(".")) {
      let [namespaceName, ref] = this.reference.split(".");
      ns = this.root.getNamespace(namespaceName);
      type = ns.get(ref);
    }

    if (DEBUG) {
      if (!type || !(type instanceof SubModuleType)) {
        throw new Error(`Internal error: ${this.namespaceName}.${this.reference} ` +
                        `is not a sub-module`);
      }
    }
    let subpath = [...path, this.name];

    let functions = type.functions;
    for (let fun of functions) {
      context.injectInto(fun, obj, fun.name, subpath, ns);
    }

    let events = type.events;
    for (let event of events) {
      context.injectInto(event, obj, event.name, subpath, ns);
    }

    // TODO: Inject this.properties.

    return {
      descriptor: {value: obj},
      revoke() {
        let unwrapped = ChromeUtils.waiveXrays(obj);
        for (let fun of functions) {
          try {
            delete unwrapped[fun.name];
          } catch (e) {
            Cu.reportError(e);
          }
        }
      },
    };
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
        this.throwError(context, `Type error for parameter ${parameter.name} (${forceString(r.error)})`);
      }
      return r.value;
    });

    return fixedArgs;
  }
}

// Represents a "function" defined in a schema namespace.
FunctionEntry = class FunctionEntry extends CallEntry {
  static parseSchema(root, schema, path) {
    // When not in DEBUG mode, we just need to know *if* this returns.
    let returns = !!schema.returns;
    if (DEBUG && "returns" in schema) {
      returns = {
        type: root.parseSchema(schema.returns, path, ["optional", "name"]),
        optional: schema.returns.optional || false,
        name: "result",
      };
    }

    return new this(schema, path, schema.name,
                    root.parseSchema(
                      schema, path,
                      ["name", "unsupported", "returns",
                       "permissions",
                       "allowAmbiguousOptionalArguments"]),
                    schema.unsupported || false,
                    schema.allowAmbiguousOptionalArguments || false,
                    returns,
                    schema.permissions || null);
  }

  constructor(schema, path, name, type, unsupported, allowAmbiguousOptionalArguments, returns, permissions) {
    super(schema, path, name, type.parameters, allowAmbiguousOptionalArguments);
    this.unsupported = unsupported;
    this.returns = returns;
    this.permissions = permissions;

    this.isAsync = type.isAsync;
    this.hasAsyncCallback = type.hasAsyncCallback;
    this.requireUserInput = type.requireUserInput;
  }

  checkValue({type, optional, name}, value, context) {
    if (optional && value == null) {
      return;
    }
    if (type.reference === "ExtensionPanel" ||
        type.reference === "ExtensionSidebarPane" ||
        type.reference === "Port") {
      // TODO: We currently treat objects with functions as SubModuleType,
      // which is just wrong, and a bigger yak.  Skipping for now.
      return;
    }
    const {error} = type.normalize(value, context);
    if (error) {
      this.throwError(context, `Type error for ${name} value (${forceString(error)})`);
    }
  }

  checkCallback(args, context) {
    const callback = this.parameters[this.parameters.length - 1];
    for (const [i, param] of callback.type.parameters.entries()) {
      this.checkValue(param, args[i], context);
    }
  }

  getDescriptor(path, context) {
    let apiImpl = context.getImplementation(path.join("."), this.name);

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
        if (DEBUG && this.hasAsyncCallback && callback) {
          let original = callback;
          callback = (...args) => {
            this.checkCallback(args, context);
            original(...args);
          };
        }
        let result = apiImpl.callAsyncFunction(actuals, callback, this.requireUserInput);
        if (DEBUG && this.hasAsyncCallback && !callback) {
          return result.then(result => {
            this.checkCallback([result], context);
            return result;
          });
        }
        return result;
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
        let result = apiImpl.callFunction(actuals);
        if (DEBUG && this.returns) {
          this.checkValue(this.returns, result, context);
        }
        return result;
      };
    }

    return {
      descriptor: {value: Cu.exportFunction(stub, context.cloneScope)},
      revoke() {
        apiImpl.revoke();
        apiImpl = null;
      },
    };
  }
};

// Represents an "event" defined in a schema namespace.
//
// TODO Bug 1369722: we should be able to remove the eslint-disable-line that follows
// once Bug 1369722 has been fixed.
Event = class Event extends CallEntry { // eslint-disable-line no-native-reassign
  static parseSchema(root, event, path) {
    let extraParameters = Array.from(event.extraParameters || [], param => ({
      type: root.parseSchema(param, path, ["name", "optional", "default"]),
      name: param.name,
      optional: param.optional || false,
      default: param.default == undefined ? null : param.default,
    }));

    let extraProperties = ["name", "unsupported", "permissions", "extraParameters",
                           // We ignore these properties for now.
                           "returns", "filters"];

    return new this(event, path, event.name,
                    root.parseSchema(event, path, extraProperties),
                    extraParameters,
                    event.unsupported || false,
                    event.permissions || null);
  }

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

  getDescriptor(path, context) {
    let apiImpl = context.getImplementation(path.join("."), this.name);

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

    let obj = Cu.createObjectIn(context.cloneScope);

    Cu.exportFunction(addStub, obj, {defineAs: "addListener"});
    Cu.exportFunction(removeStub, obj, {defineAs: "removeListener"});
    Cu.exportFunction(hasStub, obj, {defineAs: "hasListener"});

    return {
      descriptor: {value: obj},
      revoke() {
        apiImpl.revoke();
        apiImpl = null;

        let unwrapped = ChromeUtils.waiveXrays(obj);
        delete unwrapped.addListener;
        delete unwrapped.removeListener;
        delete unwrapped.hasListener;
      },
    };
  }
};

const TYPES = Object.freeze(Object.assign(Object.create(null), {
  any: AnyType,
  array: ArrayType,
  boolean: BooleanType,
  function: FunctionType,
  integer: IntegerType,
  null: NullType,
  number: NumberType,
  object: ObjectType,
  string: StringType,
}));

const LOADERS = {
  events: "loadEvent",
  functions: "loadFunction",
  properties: "loadProperty",
  types: "loadType",
};

class Namespace extends Map {
  constructor(root, name, path) {
    super();

    this.root = root;

    this._lazySchemas = [];
    this.initialized = false;

    this.name = name;
    this.path = name ? [...path, name] : [...path];

    this.superNamespace = null;

    this.permissions = null;
    this.allowedContexts = [];
    this.defaultContexts = [];
  }

  /**
   * Adds a JSON Schema object to the set of schemas that represent this
   * namespace.
   *
   * @param {object} schema
   *        A JSON schema object which partially describes this
   *        namespace.
   */
  addSchema(schema) {
    this._lazySchemas.push(schema);

    for (let prop of ["permissions", "allowedContexts", "defaultContexts"]) {
      if (schema[prop]) {
        this[prop] = schema[prop];
      }
    }

    if (schema.$import) {
      this.superNamespace = this.root.getNamespace(schema.$import);
    }
  }

  /**
   * Initializes the keys of this namespace based on the schema objects
   * added via previous `addSchema` calls.
   */
  init() {
    if (this.initialized) {
      return;
    }

    if (this.superNamespace) {
      this._lazySchemas.unshift(...this.superNamespace._lazySchemas);
    }

    for (let type of Object.keys(LOADERS)) {
      this[type] = new DefaultMap(() => []);
    }

    for (let schema of this._lazySchemas) {
      for (let type of schema.types || []) {
        if (!type.unsupported) {
          this.types.get(type.$extend || type.id).push(type);
        }
      }

      for (let [name, prop] of Object.entries(schema.properties || {})) {
        if (!prop.unsupported) {
          this.properties.get(name).push(prop);
        }
      }

      for (let fun of schema.functions || []) {
        if (!fun.unsupported) {
          this.functions.get(fun.name).push(fun);
        }
      }

      for (let event of schema.events || []) {
        if (!event.unsupported) {
          this.events.get(event.name).push(event);
        }
      }
    }

    // For each type of top-level property in the schema object, iterate
    // over all properties of that type, and create a temporary key for
    // each property pointing to its type. Those temporary properties
    // are later used to instantiate an Entry object based on the actual
    // schema object.
    for (let type of Object.keys(LOADERS)) {
      for (let key of this[type].keys()) {
        this.set(key, type);
      }
    }

    this.initialized = true;

    if (DEBUG) {
      for (let key of this.keys()) {
        this.get(key);
      }
    }
  }

  /**
   * Initializes the value of a given key, by parsing the schema object
   * associated with it and replacing its temporary value with an `Entry`
   * instance.
   *
   * @param {string} key
   *        The name of the property to initialize.
   * @param {string} type
   *        The type of property the key represents. Must have a
   *        corresponding entry in the `LOADERS` object, pointing to the
   *        initialization method for that type.
   *
   * @returns {Entry}
   */
  initKey(key, type) {
    let loader = LOADERS[type];

    for (let schema of this[type].get(key)) {
      this.set(key, this[loader](key, schema));
    }

    return this.get(key);
  }

  loadType(name, type) {
    if ("$extend" in type) {
      return this.extendType(type);
    }
    return this.root.parseSchema(type, this.path, ["id"]);
  }

  extendType(type) {
    let targetType = this.get(type.$extend);

    // Only allow extending object and choices types for now.
    if (targetType instanceof ObjectType) {
      type.type = "object";
    } else if (DEBUG) {
      if (!targetType) {
        throw new Error(`Internal error: Attempt to extend a nonexistant type ${type.$extend}`);
      } else if (!(targetType instanceof ChoiceType)) {
        throw new Error(`Internal error: Attempt to extend a non-extensible type ${type.$extend}`);
      }
    }

    let parsed = this.root.parseSchema(type, this.path, ["$extend"]);

    if (DEBUG && parsed.constructor !== targetType.constructor) {
      throw new Error(`Internal error: Bad attempt to extend ${type.$extend}`);
    }

    targetType.extend(parsed);

    return targetType;
  }

  loadProperty(name, prop) {
    if ("$ref" in prop) {
      if (!prop.unsupported) {
        return new SubModuleProperty(this.root,
                                     prop, this.path, name,
                                     prop.$ref, prop.properties || {},
                                     prop.permissions || null);
      }
    } else if ("value" in prop) {
      return new ValueProperty(prop, name, prop.value);
    } else {
      // We ignore the "optional" attribute on properties since we
      // don't inject anything here anyway.
      let type = this.root.parseSchema(prop, [this.name], ["optional", "permissions", "writable"]);
      return new TypeProperty(prop, this.path, name, type, prop.writable || false,
                              prop.permissions || null);
    }
  }

  loadFunction(name, fun) {
    return FunctionEntry.parseSchema(this.root, fun, this.path);
  }

  loadEvent(name, event) {
    return Event.parseSchema(this.root, event, this.path);
  }

  /**
   * Injects the properties of this namespace into the given object.
   *
   * @param {object} dest
   *        The object into which to inject the namespace properties.
   * @param {InjectionContext} context
   *        The injection context with which to inject the properties.
   */
  injectInto(dest, context) {
    for (let name of this.keys()) {
      exportLazyProperty(dest, name, () => {
        let entry = this.get(name);

        return context.getDescriptor(entry, dest, name, this.path, this);
      });
    }
  }

  getDescriptor(path, context) {
    let obj = Cu.createObjectIn(context.cloneScope);

    this.injectInto(obj, context);

    // Only inject the namespace object if it isn't empty.
    if (Object.keys(obj).length) {
      return {
        descriptor: {value: obj},
      };
    }
  }

  keys() {
    this.init();
    return super.keys();
  }

  * entries() {
    for (let key of this.keys()) {
      yield [key, this.get(key)];
    }
  }

  get(key) {
    this.init();
    let value = super.get(key);

    // The initial values of lazily-initialized schema properties are
    // strings, pointing to the type of property, corresponding to one
    // of the entries in the `LOADERS` object.
    if (typeof value === "string") {
      value = this.initKey(key, value);
    }

    return value;
  }

  /**
   * Returns a Namespace object for the given namespace name. If a
   * namespace object with this name does not already exist, it is
   * created. If the name contains any '.' characters, namespaces are
   * recursively created, for each dot-separated component.
   *
   * @param {string} name
   *        The name of the sub-namespace to retrieve.
   * @param {boolean} [create = true]
   *        If true, create any intermediate namespaces which don't
   *        exist.
   *
   * @returns {Namespace}
   */
  getNamespace(name, create = true) {
    let subName;

    let idx = name.indexOf(".");
    if (idx > 0) {
      subName = name.slice(idx + 1);
      name = name.slice(0, idx);
    }

    let ns = super.get(name);
    if (!ns) {
      if (!create) {
        return null;
      }
      ns = new Namespace(this.root, name, this.path);
      this.set(name, ns);
    }

    if (subName) {
      return ns.getNamespace(subName);
    }
    return ns;
  }

  getOwnNamespace(name) {
    return this.getNamespace(name);
  }

  has(key) {
    this.init();
    return super.has(key);
  }
}


/**
 * A root schema namespace containing schema data which is isolated from data in
 * other schema roots. May extend a base namespace, in which case schemas in
 * this root may refer to types in a base, but not vice versa.
 *
 * @param {SchemaRoot|Array<SchemaRoot>|null} base
 *        A base schema root (or roots) from which to derive, or null.
 * @param {Map<string, Array|StructuredCloneHolder>} schemaJSON
 *        A map of schema URLs and corresponding JSON blobs from which to
 *        populate this root namespace.
 */
class SchemaRoot extends Namespace {
  constructor(base, schemaJSON) {
    super(null, "", []);

    this.root = this;
    this.base = base;
    this.schemaJSON = schemaJSON;
  }

  /**
   * Returns the sub-namespace with the given name. If the given namespace
   * doesn't already exist, attempts to find it in the base SchemaRoot before
   * creating a new empty namespace.
   *
   * @param {string} name
   *        The namespace to retrieve.
   * @param {boolean} [create = true]
   *        If true, an empty namespace should be created if one does not
   *        already exist.
   * @returns {Namespace|null}
   */
  getNamespace(name, create = true) {
    let res = this.base && this.base.getNamespace(name, false);
    if (res) {
      return res;
    }
    return super.getNamespace(name, create);
  }

  /**
   * Like getNamespace, but does not take the base SchemaRoot into account.
   *
   * @param {string} name
   *        The namespace to retrieve.
   * @returns {Namespace}
   */
  getOwnNamespace(name) {
    return super.getNamespace(name);
  }

  parseSchema(schema, path, extraProperties = []) {
    let allowedProperties = DEBUG && new Set(extraProperties);

    if ("choices" in schema) {
      return ChoiceType.parseSchema(this, schema, path, allowedProperties);
    } else if ("$ref" in schema) {
      return RefType.parseSchema(this, schema, path, allowedProperties);
    }

    let type = TYPES[schema.type];

    if (DEBUG) {
      allowedProperties.add("type");

      if (!("type" in schema)) {
        throw new Error(`Unexpected value for type: ${JSON.stringify(schema)}`);
      }

      if (!type) {
        throw new Error(`Unexpected type ${schema.type}`);
      }
    }

    return type.parseSchema(this, schema, path, allowedProperties);
  }

  parseSchemas() {
    for (let [key, schema] of this.schemaJSON.entries()) {
      try {
        if (typeof schema.deserialize === "function") {
          schema = schema.deserialize(global);

          // If we're in the parent process, we need to keep the
          // StructuredCloneHolder blob around in order to send to future child
          // processes. If we're in a child, we have no further use for it, so
          // just store the deserialized schema data in its place.
          if (!isParentProcess) {
            this.schemaJSON.set(key, schema);
          }
        }

        this.loadSchema(schema);
      } catch (e) {
        Cu.reportError(e);
      }
    }
  }

  loadSchema(json) {
    for (let namespace of json) {
      this.getOwnNamespace(namespace.namespace)
          .addSchema(namespace);
    }
  }

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
    let ns = this.getNamespace(namespace);
    if (ns && ns.permissions) {
      return ns.permissions.some(perm => wrapperFuncs.hasPermission(perm));
    }
    return true;
  }

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

    if (this.base) {
      this.base.injectInto(dest, context);
    }
    this.injectInto(dest, context);
  }

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
    let ns = this.getNamespace(namespaceName);
    let type = ns.get(prop);

    let result = type.normalize(obj, new Context(context));
    if (result.error) {
      return {error: forceString(result.error)};
    }
    return result;
  }
}

this.Schemas = {
  initialized: false,

  REVOKE: Symbol("@@revoke"),

  // Maps a schema URL to the JSON contained in that schema file. This
  // is useful for sending the JSON across processes.
  schemaJSON: new Map(),

  // A separate map of schema JSON which should be available in all
  // content processes.
  contentSchemaJSON: new Map(),

  _rootSchema: null,

  get rootSchema() {
    if (!this.initialized) {
      this.init();
    }
    return this._rootSchema;
  },

  getNamespace(name) {
    return this.rootSchema.getNamespace(name);
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
    let {data} = msg;
    switch (msg.name) {
      case "Schema:Add":
        // If we're given a Map, the ordering of the initial items
        // matters, so swap with our current data to make sure its
        // entries appear first.
        if (typeof data.get === "function") {
          // Create a new Map so we're sure it's in the same compartment.
          [this.schemaJSON, data] = [new Map(data), this.schemaJSON];
        }

        for (let [url, schema] of data) {
          this.schemaJSON.set(url, schema);
        }
        this.flushSchemas();
        break;

      case "Schema:Delete":
        this.schemaJSON.delete(data.url);
        this.flushSchemas();
        break;
    }
  },

  _needFlush: true,
  flushSchemas() {
    if (this._needFlush) {
      this._needFlush = false;
      XPCOMUtils.defineLazyGetter(this, "_rootSchema", () => {
        this._needFlush = true;

        let rootSchema = new SchemaRoot(null, this.schemaJSON);
        rootSchema.parseSchemas();
        return rootSchema;
      });
    }
  },

  _loadCachedSchemasPromise: null,
  loadCachedSchemas() {
    if (!this._loadCachedSchemasPromise) {
      this._loadCachedSchemasPromise = StartupCache.schemas.getAll().then(results => {
        return results;
      });
    }

    return this._loadCachedSchemasPromise;
  },

  addSchema(url, schema, content = false) {
    this.schemaJSON.set(url, schema);

    if (content) {
      this.contentSchemaJSON.set(url, schema);

      let data = Services.ppmm.initialProcessData;
      data["Extension:Schemas"] = this.contentSchemaJSON;

      Services.ppmm.broadcastAsyncMessage("Schema:Add", [[url, schema]]);
    } else if (this.schemaHook) {
      this.schemaHook([[url, schema]]);
    }

    this.flushSchemas();
  },

  fetch(url) {
    return readJSONAndBlobbify(url);
  },

  processSchema(json) {
    return blobbify(json);
  },

  async load(url, content = false) {
    if (!isParentProcess) {
      return;
    }

    let schemaCache = await this.loadCachedSchemas();

    let blob = (schemaCache.get(url) ||
                await StartupCache.schemas.get(url, readJSONAndBlobbify));

    if (!this.schemaJSON.has(url)) {
      this.addSchema(url, blob, content);
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
    return this.rootSchema.checkPermissions(namespace, wrapperFuncs);
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
    this.rootSchema.inject(dest, wrapperFuncs);
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
    return this.rootSchema.normalize(obj, typeName, context);
  },
};
