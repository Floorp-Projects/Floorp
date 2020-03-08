/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var USERSCRIPT_PREFNAME = "extensions.webextensions.userScripts.enabled";
var USERSCRIPT_DISABLED_ERRORMSG = `userScripts APIs are currently experimental and must be enabled with the ${USERSCRIPT_PREFNAME} preference.`;

ChromeUtils.defineModuleGetter(
  this,
  "Schemas",
  "resource://gre/modules/Schemas.jsm"
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "userScriptsEnabled",
  USERSCRIPT_PREFNAME,
  false
);

var { ExtensionError } = ExtensionUtils;

const TYPEOF_PRIMITIVES = ["bigint", "boolean", "number", "string", "symbol"];

/**
 * Represents a user script in the child content process.
 *
 * This class implements the API object that is passed as a parameter to the
 * browser.userScripts.onBeforeScript API Event.
 *
 * @param {Object} params
 * @param {ContentScriptContextChild} params.context
 *        The context which has registered the userScripts.onBeforeScript listener.
 * @param {PlainJSONValue}            params.metadata
 *        An opaque user script metadata value (as set in userScripts.register).
 * @param {Sandbox}                   params.scriptSandbox
 *        The Sandbox object of the userScript.
 */
class UserScript {
  constructor({ context, metadata, scriptSandbox }) {
    this.context = context;
    this.extension = context.extension;
    this.apiSandbox = context.cloneScope;
    this.metadata = metadata;
    this.scriptSandbox = scriptSandbox;

    this.ScriptError = scriptSandbox.Error;
    this.ScriptPromise = scriptSandbox.Promise;
  }

  /**
   * Returns the API object provided to the userScripts.onBeforeScript listeners.
   *
   * @returns {Object}
   *          The API object with the properties and methods to export
   *          to the extension code.
   */
  api() {
    return {
      metadata: this.metadata,
      defineGlobals: sourceObject => this.defineGlobals(sourceObject),
      export: value => this.export(value),
    };
  }

  /**
   * Define all the properties of a given plain object as lazy getters of the
   * userScript global object.
   *
   * @param {Object} sourceObject
   *        A set of objects and methods to export into the userScript scope as globals.
   *
   * @throws {context.Error}
   *         Throws an apiScript error when sourceObject is not a plain object.
   */
  defineGlobals(sourceObject) {
    let className;
    try {
      className = ChromeUtils.getClassName(sourceObject, true);
    } catch (e) {
      // sourceObject is not an object;
    }

    if (className !== "Object") {
      throw new this.context.Error(
        "Invalid sourceObject type, plain object expected."
      );
    }

    this.exportLazyGetters(sourceObject, this.scriptSandbox);
  }

  /**
   * Convert a given value to make it accessible to the userScript code.
   *
   * - any property value that is already accessible to the userScript code is returned unmodified by
   *   the lazy getter
   * - any apiScript's Function is wrapped using the `wrapFunction` method
   * - any apiScript's Object is lazily exported (and the same wrappers are lazily applied to its
   *   properties).
   *
   * @param {any} valueToExport
   *        A value to convert into an object accessible to the userScript.
   *
   * @param {Object} privateOptions
   *        A set of options used when this method is called internally (not exposed in the
   *        api object exported to the onBeforeScript listeners).
   * @param {Error}  Error
   *        The Error constructor to use to report errors (defaults to the apiScript context's Error
   *        when missing).
   * @param {Error}  errorMessage
   *        A custom error message to report exporting error on values not allowed.
   *
   * @returns {any}
   *        The resulting userScript object.
   *
   * @throws {context.Error | privateOptions.Error}
   *         Throws an error when the value is not allowed and it can't be exported into an allowed one.
   */
  export(valueToExport, privateOptions = {}) {
    const ExportError = privateOptions.Error || this.context.Error;

    if (this.canAccess(valueToExport, this.scriptSandbox)) {
      // Return the value unmodified if the userScript principal is already allowed
      // to access it.
      return valueToExport;
    }

    let className;

    try {
      className = ChromeUtils.getClassName(valueToExport, true);
    } catch (e) {
      // sourceObject is not an object;
    }

    if (className === "Function") {
      return this.wrapFunction(valueToExport);
    }

    if (className === "Object") {
      return this.exportLazyGetters(valueToExport);
    }

    if (className === "Array") {
      return this.exportArray(valueToExport);
    }

    let valueType = className || typeof valueToExport;
    throw new ExportError(
      privateOptions.errorMessage ||
        `${valueType} cannot be exported to the userScript`
    );
  }

  /**
   * Export all the elements of the `srcArray` into a newly created userScript array.
   *
   * @param {Array} srcArray
   *        The apiScript array to export to the userScript code.
   *
   * @returns {Array}
   *          The resulting userScript array.
   *
   * @throws {UserScriptError}
   *         Throws an error when the array can't be exported successfully.
   */
  exportArray(srcArray) {
    const destArray = Cu.cloneInto([], this.scriptSandbox);

    for (let [idx, value] of this.shallowCloneEntries(srcArray)) {
      destArray[idx] = this.export(value, {
        errorMessage: `Error accessing disallowed element at index "${idx}"`,
        Error: this.UserScriptError,
      });
    }

    return destArray;
  }

  /**
   * Export all the properties of the `src` plain object as lazy getters on the `dest` object,
   * or in a newly created userScript object if `dest` is `undefined`.
   *
   * @param {Object} src
   *        A set of properties to define on a `dest` object as lazy getters.
   * @param {Object} [dest]
   *        An optional `dest` object (a new userScript object is created by default when not specified).
   *
   * @returns {Object}
   *          The resulting userScript object.
   */
  exportLazyGetters(src, dest = undefined) {
    dest = dest || Cu.createObjectIn(this.scriptSandbox);

    for (let [key, value] of this.shallowCloneEntries(src)) {
      Schemas.exportLazyGetter(dest, key, () => {
        return this.export(value, {
          // Lazy properties will raise an error for properties with not allowed
          // values to the userScript scope, and so we have to raise an userScript
          // Error here.
          Error: this.ScriptError,
          errorMessage: `Error accessing disallowed property "${key}"`,
        });
      });
    }

    return dest;
  }

  /**
   * Export and wrap an apiScript function to provide the following behaviors:
   *   - errors throws from an exported function are checked by `handleAPIScriptError`
   *   - returned apiScript's Promises (not accessible to the userScript) are converted into a
   *     userScript's Promise
   *   - check if the returned or resolved value is accessible to the userScript code
   *     (and raise a userScript error if it is not)
   *
   * @param {Function} fn
   *        The apiScript function to wrap
   *
   * @returns {Object}
   *          The resulting userScript function.
   */
  wrapFunction(fn) {
    return Cu.exportFunction((...args) => {
      let res;
      try {
        // Checks that all the elements in the `...args` array are allowed to be
        // received from the apiScript.
        for (let arg of args) {
          if (!this.canAccess(arg, this.apiSandbox)) {
            throw new this.ScriptError(
              `Parameter not accessible to the userScript API`
            );
          }
        }

        res = fn(...args);
      } catch (err) {
        this.handleAPIScriptError(err);
      }

      // Prevent execution of proxy traps while checking if the return value is a Promise.
      if (!Cu.isProxy(res) && res instanceof this.context.Promise) {
        return this.ScriptPromise.resolve().then(async () => {
          let value;

          try {
            value = await res;
          } catch (err) {
            this.handleAPIScriptError(err);
          }

          return this.ensureAccessible(value);
        });
      }

      return this.ensureAccessible(res);
    }, this.scriptSandbox);
  }

  /**
   * Shallow clone the source object and iterate over its Object properties (or Array elements),
   * which allow us to safely iterate over all its properties (including callable objects that
   * would be hidden by the xrays vision, but excluding any property that could be tricky, e.g.
   * getters).
   *
   * @param {Object|Array} obj
   *        The Object or Array object to shallow clone and iterate over.
   */
  *shallowCloneEntries(obj) {
    const clonedObj = ChromeUtils.shallowClone(obj);

    for (let entry of Object.entries(clonedObj)) {
      yield entry;
    }
  }

  /**
   * Check if the given value is accessible to the targetScope.
   *
   * @param {any}     val
   *        The value to check.
   * @param {Sandbox} targetScope
   *        The targetScope that should be able to access the value.
   *
   * @returns {boolean}
   */
  canAccess(val, targetScope) {
    if (val == null || TYPEOF_PRIMITIVES.includes(typeof val)) {
      return true;
    }

    // Disallow objects that are coming from principals that are not
    // subsumed by the targetScope's principal.
    try {
      const targetPrincipal = Cu.getObjectPrincipal(targetScope);
      if (!targetPrincipal.subsumes(Cu.getObjectPrincipal(val))) {
        return false;
      }
    } catch (err) {
      Cu.reportError(err);
      return false;
    }

    return true;
  }

  /**
   * Check if the value returned (or resolved) from an apiScript method is accessible
   * to the userScript code, and throw a userScript Error if it is not allowed.
   *
   * @param {any} res
   *        The value to return/resolve.
   *
   * @returns {any}
   *          The exported value.
   *
   * @throws {Error}
   *         Throws a userScript error when the value is not accessible to the userScript scope.
   */
  ensureAccessible(res) {
    if (this.canAccess(res, this.scriptSandbox)) {
      return res;
    }

    throw new this.ScriptError("Return value not accessible to the userScript");
  }

  /**
   * Handle the error raised (and rejected promise returned) from apiScript functions exported to the
   * userScript.
   *
   * @param {any} err
   *        The value to return/resolve.
   *
   * @throws {any}
   *         This method is expected to throw:
   *         - any value that is already accessible to the userScript code is forwarded unmodified
   *         - any value that is not accessible to the userScript code is logged in the console
   *           (to make it easier to investigate the underlying issue) and converted into a
   *           userScript Error (with the generic "An unexpected apiScript error occurred" error
   *           message accessible to the userScript)
   */
  handleAPIScriptError(err) {
    if (this.canAccess(err, this.scriptSandbox)) {
      throw err;
    }

    // Log the actual error on the console and raise a generic userScript Error
    // on error objects that can't be accessed by the UserScript principal.
    try {
      const debugName = this.extension.policy.debugName;
      Cu.reportError(
        `An unexpected apiScript error occurred for '${debugName}': ${err} :: ${err.stack}`
      );
    } catch (e) {}

    throw new this.ScriptError(`An unexpected apiScript error occurred`);
  }
}

this.userScriptsContent = class extends ExtensionAPI {
  getAPI(context) {
    return {
      userScripts: {
        onBeforeScript: new EventManager({
          context,
          name: "userScripts.onBeforeScript",
          register: fire => {
            if (!userScriptsEnabled) {
              throw new ExtensionError(USERSCRIPT_DISABLED_ERRORMSG);
            }

            let handler = (event, metadata, scriptSandbox, eventResult) => {
              const us = new UserScript({
                context,
                metadata,
                scriptSandbox,
              });

              const apiObj = Cu.cloneInto(us.api(), context.cloneScope, {
                cloneFunctions: true,
              });

              Object.defineProperty(apiObj, "global", {
                value: scriptSandbox,
                enumerable: true,
                configurable: true,
                writable: true,
              });

              fire.raw(apiObj);
            };

            context.userScriptsEvents.on("on-before-script", handler);
            return () => {
              context.userScriptsEvents.off("on-before-script", handler);
            };
          },
        }).api(),
      },
    };
  }
};
