/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements low-overhead integration between components of the application.
 * This may have different uses depending on the component, including:
 *
 * - Providing product-specific implementations registered at startup.
 * - Using alternative implementations during unit tests.
 * - Allowing add-ons to change specific behaviors.
 *
 * Components may define one or more integration points, each defined by a
 * root integration object whose properties and methods are the public interface
 * and default implementation of the integration point. For example:
 *
 *   const DownloadIntegration = {
 *     getTemporaryDirectory() {
 *       return "/tmp/";
 *     },
 *
 *     getTemporaryFile(name) {
 *       return this.getTemporaryDirectory() + name;
 *     },
 *   };
 *
 * Other parts of the application may register overrides for some or all of the
 * defined properties and methods. The component defining the integration point
 * does not have to be loaded at this stage, because the name of the integration
 * point is the only information required. For example, if the integration point
 * is called "downloads":
 *
 *   Integration.downloads.register(base => ({
 *     getTemporaryDirectory() {
 *       return base.getTemporaryDirectory.call(this) + "subdir/";
 *     },
 *   }));
 *
 * When the component defining the integration point needs to call a method on
 * the integration object, instead of using it directly the component would use
 * the "getCombined" method to retrieve an object that includes all overrides.
 * For example:
 *
 *   let combined = Integration.downloads.getCombined(DownloadIntegration);
 *   Assert.is(combined.getTemporaryFile("file"), "/tmp/subdir/file");
 *
 * Overrides can be registered at startup or at any later time, so each call to
 * "getCombined" may return a different object. The simplest way to create a
 * reference to the combined object that stays updated to the latest version is
 * to define the root object in a JSM and use the "defineModuleGetter" method.
 *
 * *** Registration ***
 *
 * Since the interface is not declared formally, the registrations can happen
 * at startup without loading the component, so they do not affect performance.
 *
 * Hovever, this module does not provide a startup registry, this means that the
 * code that registers and implements the override must be loaded at startup.
 *
 * If performance for the override code is a concern, you can take advantage of
 * the fact that the function used to create the override is called lazily, and
 * include only a stub loader for the final code in an existing startup module.
 *
 * The registration of overrides should be repeated for each process where the
 * relevant integration methods will be called.
 *
 * *** Accessing base methods and properties ***
 *
 * Overrides are included in the prototype chain of the combined object in the
 * same order they were registered, where the first is closest to the root.
 *
 * When defining overrides, you do not need to set the "__proto__" property of
 * the objects you create, because their properties and methods are moved to a
 * new object with the correct prototype. If you do, however, you can call base
 * properties and methods using the "super" keyword. For example:
 *
 *   Integration.downloads.register(base => ({
 *     __proto__: base,
 *     getTemporaryDirectory() {
 *       return super.getTemporaryDirectory() + "subdir/";
 *     },
 *   }));
 *
 * *** State handling ***
 *
 * Storing state directly on the combined integration object using the "this"
 * reference is not recommended. When a new integration is registered, own
 * properties stored on the old combined object are copied to the new combined
 * object using a shallow copy, but the "this" reference for new invocations
 * of the methods will be different.
 *
 * If the root object defines a property that always points to the same object,
 * for example a "state" property, you can safely use it across registrations.
 *
 * Integration overrides provided by restartless add-ons should not use the
 * "this" reference to store state, to avoid conflicts with other add-ons.
 *
 * *** Interaction with XPCOM ***
 *
 * Providing the combined object as an argument to any XPCOM method will
 * generate a console error message, and will throw an exception where possible.
 * For example, you cannot register observers directly on the combined object.
 * This helps preventing mistakes due to the fact that the combined object
 * reference changes when new integration overrides are registered.
 */

"use strict";

var EXPORTED_SYMBOLS = [
  "Integration",
];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * Maps integration point names to IntegrationPoint objects.
 */
const gIntegrationPoints = new Map();

/**
 * This Proxy object creates IntegrationPoint objects using their name as key.
 * The objects will be the same for the duration of the process. For example:
 *
 *   Integration.downloads.register(...);
 *   Integration["addon-provided-integration"].register(...);
 */
var Integration = new Proxy({}, {
  get(target, name) {
    let integrationPoint = gIntegrationPoints.get(name);
    if (!integrationPoint) {
      integrationPoint = new IntegrationPoint();
      gIntegrationPoints.set(name, integrationPoint);
    }
    return integrationPoint;
  },
});

/**
 * Individual integration point for which overrides can be registered.
 */
var IntegrationPoint = function() {
  this._overrideFns = new Set();
  this._combined = {
    // eslint-disable-next-line mozilla/use-chromeutils-generateqi
    QueryInterface() {
      let ex = new Components.Exception(
                   "Integration objects should not be used with XPCOM because" +
                   " they change when new overrides are registered.",
                   Cr.NS_ERROR_NO_INTERFACE);
      Cu.reportError(ex);
      throw ex;
    },
  };
};

this.IntegrationPoint.prototype = {
  /**
   * Ordered set of registered functions defining integration overrides.
   */
  _overrideFns: null,

  /**
   * Combined integration object. When this reference changes, properties
   * defined directly on this object are copied to the new object.
   *
   * Initially, the only property of this object is a "QueryInterface" method
   * that throws an exception, to prevent misuse as a permanent XPCOM listener.
   */
  _combined: null,

  /**
   * Indicates whether the integration object is current based on the list of
   * registered integration overrides.
   */
  _combinedIsCurrent: false,

  /**
   * Registers new overrides for the integration methods. For example:
   *
   *   Integration.nameOfIntegrationPoint.register(base => ({
   *     asyncMethod: Task.async(function* () {
   *       return yield base.asyncMethod.apply(this, arguments);
   *     }),
   *   }));
   *
   * @param overrideFn
   *        Function returning an object defining the methods that should be
   *        overridden. Its only parameter is an object that contains the base
   *        implementation of all the available methods.
   *
   * @note The override function is called every time the list of registered
   *       override functions changes. Thus, it should not have any side
   *       effects or do any other initialization.
   */
  register(overrideFn) {
    this._overrideFns.add(overrideFn);
    this._combinedIsCurrent = false;
  },

  /**
   * Removes a previously registered integration override.
   *
   * Overrides don't usually need to be unregistered, unless they are added by a
   * restartless add-on, in which case they should be unregistered when the
   * add-on is disabled or uninstalled.
   *
   * @param overrideFn
   *        This must be the same function object passed to "register".
   */
  unregister(overrideFn) {
    this._overrideFns.delete(overrideFn);
    this._combinedIsCurrent = false;
  },

  /**
   * Retrieves the dynamically generated object implementing the integration
   * methods. Platform-specific code and add-ons can override methods of this
   * object using the "register" method.
   */
  getCombined(root) {
    if (this._combinedIsCurrent) {
      return this._combined;
    }

    // In addition to enumerating all the registered integration overrides in
    // order, we want to keep any state that was previously stored in the
    // combined object using the "this" reference in integration methods.
    let overrideFnArray = [...this._overrideFns, () => this._combined];

    let combined = root;
    for (let overrideFn of overrideFnArray) {
      try {
        // Obtain a new set of methods from the next override function in the
        // list, specifying the current combined object as the base argument.
        let override = overrideFn(combined);

        // Retrieve a list of property descriptors from the returned object, and
        // use them to build a new combined object whose prototype points to the
        // previous combined object.
        let descriptors = {};
        for (let name of Object.getOwnPropertyNames(override)) {
          descriptors[name] = Object.getOwnPropertyDescriptor(override, name);
        }
        combined = Object.create(combined, descriptors);
      } catch (ex) {
        // Any error will result in the current override being skipped.
        Cu.reportError(ex);
      }
    }

    this._combinedIsCurrent = true;
    return this._combined = combined;
  },

  /**
   * Defines a getter to retrieve the dynamically generated object implementing
   * the integration methods, loading the root implementation lazily from the
   * specified JSM module. For example:
   *
   *   Integration.test.defineModuleGetter(this, "TestIntegration",
   *                    "resource://testing-common/TestIntegration.jsm");
   *
   * @param targetObject
   *        The object on which the lazy getter will be defined.
   * @param name
   *        The name of the getter to define.
   * @param moduleUrl
   *        The URL used to obtain the module.
   * @param symbol [optional]
   *        The name of the symbol exported by the module. This can be omitted
   *        if the name of the exported symbol is equal to the getter name.
   */
  defineModuleGetter(targetObject, name, moduleUrl, symbol) {
    let moduleHolder = {};
    XPCOMUtils.defineLazyModuleGetter(moduleHolder, name, moduleUrl, symbol);
    Object.defineProperty(targetObject, name, {
      get: () => this.getCombined(moduleHolder[name]),
      configurable: true,
      enumerable: true,
    });
  },
};
