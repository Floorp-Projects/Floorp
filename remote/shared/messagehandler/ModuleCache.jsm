/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ModuleCache"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  getMessageHandlerClass:
    "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
});

const protocols = {
  bidi: {},
  test: {},
};
XPCOMUtils.defineLazyModuleGetters(protocols.bidi, {
  // Additional protocols might use a different registry for their modules,
  // in which case this will no longer be a constant but will instead depend on
  // the protocol owning the MessageHandler. See Bug 1722464.
  getModuleClass:
    "chrome://remote/content/webdriver-bidi/modules/ModuleRegistry.jsm",
});
XPCOMUtils.defineLazyModuleGetters(protocols.test, {
  getModuleClass:
    "chrome://mochitests/content/browser/remote/shared/messagehandler/test/browser/resources/modules/ModuleRegistry.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

/**
 * ModuleCache instances are dedicated to lazily create and cache the instances
 * of all the modules related to a specific MessageHandler instance.
 *
 * ModuleCache also implements the logic to resolve the path to the file for a
 * given module, which depends both on the current MessageHandler context and on
 * the expected destination.
 *
 * In order to implement module logic in any context, separate module files
 * should be created for each situation. For instance, for a given module,
 * - ${MODULES_FOLDER}/root/{ModuleName}.jsm contains the implementation for
 *   commands intended for the destination ROOT, and will be created for a ROOT
 *   MessageHandler only. Typically, they will run in the parent process.
 * - ${MODULES_FOLDER}/windowglobal/{ModuleName}.jsm contains the implementation
 *   for commands intended for a WINDOW_GLOBAL destination, and will be created
 *   for a WINDOW_GLOBAL MessageHandler only. Those will usually run in a
 *   content process.
 * - ${MODULES_FOLDER}/windowglobal-in-root/{ModuleName}.jsm also handles
 *   commands intended for a WINDOW_GLOBAL destination, but they will be created
 *   for the ROOT MessageHandler and will run in the parent process. This can be
 *   useful if some code has to be executed in the parent process, even though
 *   the final destination is a WINDOW_GLOBAL.
 * - And so on, as more MessageHandler types get added, more combinations will
 *   follow based on the same pattern:
 *   - {contextName}/{ModuleName}.jsm
 *   - or {destinationType}-in-{currentType}/{ModuleName}.jsm
 *
 * All those implementations are optional. If a module cannot be found, based on
 * the logic detailed above, the MessageHandler will assume that the command
 * should simply be forwarded to the next layer of the network.
 */
class ModuleCache {
  /*
   * @param {MessageHandler} messageHandler
   *     The MessageHandler instance which owns this ModuleCache instance.
   */
  constructor(messageHandler) {
    this.messageHandler = messageHandler;
    this._messageHandlerType = messageHandler.constructor.type;

    // Use the module class from the WebDriverBiDi ModuleRegistry if we
    // are not using test modules.
    this._protocol = Services.prefs.getBoolPref(
      "remote.messagehandler.modulecache.useBrowserTestRoot",
      false
    )
      ? protocols.test
      : protocols.bidi;

    // Map of absolute module paths to module instances.
    this._modules = new Map();
  }

  /**
   * Destroy all instantiated modules.
   */
  destroy() {
    this._modules.forEach(module => module?.destroy());
  }

  /**
   * Retrieve all module classes matching the provided module name to reach the
   * provided destination, from the current context.
   *
   * This corresponds to the path a command can take to reach its destination.
   * A command's method must be implemented in one of the classes returned by
   * getAllModuleClasses in order to be successfully handled.
   *
   * @param {String} moduleName
   *     The name of the module.
   * @param {Destination} destination
   *     The destination.
   * @return {Array.<class<Module>=>}
   *     An array of Module classes.
   */
  getAllModuleClasses(moduleName, destination) {
    const destinationType = destination.type;
    const folders = [
      this._getModuleFolder(this._messageHandlerType, destinationType),
    ];

    // Bug 1733242: Extend the implementation of this method to handle workers.
    // It assumes layers have at most one level of nesting, for instance
    // "root -> windowglobal", but it wouldn't work for something such as
    // "root -> windowglobal -> worker".
    if (destinationType !== this._messageHandlerType) {
      folders.push(this._getModuleFolder(destinationType, destinationType));
    }

    return folders
      .map(folder => this._protocol.getModuleClass(moduleName, folder))
      .filter(cls => !!cls);
  }

  /**
   * Get a module instance corresponding to the provided moduleName and
   * destination. If no existing module can be found in the cache, ModuleCache
   * will attempt to import the module file and create a new instance, which
   * will then be cached and returned for subsequent calls.
   *
   * @param {String} moduleName
   *     The name of the module which should implement the command.
   * @param {CommandDestination} destination
   *     The destination of the command for which we need to instantiate a
   *     module. See MessageHandler.jsm for the CommandDestination typedef.
   * @return {Object=}
   *     A module instance corresponding to the provided moduleName and
   *     destination, or null if it could not be instantiated.
   */
  getModuleInstance(moduleName, destination) {
    const key = `${moduleName}-${destination.type}`;

    if (this._modules.has(key)) {
      // If there is already a cached instance (potentially null) for the
      // module name + destination type pair, return it.
      return this._modules.get(key);
    }

    const moduleFolder = this._getModuleFolder(
      this._messageHandlerType,
      destination.type
    );
    const ModuleClass = this._protocol.getModuleClass(moduleName, moduleFolder);

    let module = null;
    if (ModuleClass) {
      module = new ModuleClass(this.messageHandler);
      lazy.logger.trace(
        `Module ${moduleFolder}/${moduleName}.jsm found for ${destination.type}`
      );
    } else {
      lazy.logger.trace(
        `Module ${moduleFolder}/${moduleName}.jsm not found for ${destination.type}`
      );
    }

    this._modules.set(key, module);
    return module;
  }

  /**
   * Check if the given module exists for the destination.
   *
   * @param {String} moduleName
   *     The name of the module.
   * @param {Destination} destination
   *     The destination.
   * @returns {Boolean}
   *     True if the module exists.
   */
  hasModule(moduleName, destination) {
    const classes = this.getAllModuleClasses(moduleName, destination);
    return classes.length != 0;
  }

  toString() {
    return `[object ${this.constructor.name} ${this.messageHandler.name}]`;
  }

  _getModuleFolder(originType, destinationType) {
    const originPath = lazy.getMessageHandlerClass(originType).modulePath;
    if (originType === destinationType) {
      // If the command is targeting the current type, the module is expected to
      // be in eg "windowglobal/${moduleName}.jsm".
      return originPath;
    }
    // If the command is targeting another type, the module is expected to
    // be in a composed folder eg "windowglobal-in-root/${moduleName}.jsm".
    const destinationPath = lazy.getMessageHandlerClass(destinationType)
      .modulePath;
    return `${destinationPath}-in-${originPath}`;
  }
}
