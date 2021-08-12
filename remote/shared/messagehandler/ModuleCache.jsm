/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ModuleCache"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

// Import the ModuleRegistry so that all modules are properly referenced.
ChromeUtils.import(
  "chrome://remote/content/webdriver-bidi/modules/ModuleRegistry.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",

  getMessageHandlerClass:
    "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

// Additional protocols might use a different root folder for their modules,
// in which case this will no longer be a constant but will instead depend on
// the protocol owning the MessageHandler. See Bug 1722464.
const MODULES_FOLDER = "chrome://remote/content/webdriver-bidi/modules";

const TEST_MODULES_FOLDER =
  "chrome://mochitests/content/browser/remote/shared/messagehandler/test/browser/resources/modules";

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
    this._messageHandlerPath = messageHandler.constructor.modulePath;

    this._modulesRootFolder = MODULES_FOLDER;
    // TODO: Temporary workaround to use a different folder for tests.
    // After Bug 1722464 lands, we should be able to set custom root folders
    // per session and we can remove this workaround.
    if (
      Services.prefs.getBoolPref(
        "remote.messagehandler.modulecache.useBrowserTestRoot",
        false
      )
    ) {
      this._modulesRootFolder = TEST_MODULES_FOLDER;
    }

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
    const moduleFullPath = this._getModuleFullPath(moduleName, destination);

    if (!this._modules.has(moduleFullPath)) {
      try {
        const ModuleClass = ChromeUtils.import(moduleFullPath)[moduleName];
        this._modules.set(moduleFullPath, new ModuleClass(this.messageHandler));
        logger.trace(`Module ${moduleName} created for ${moduleFullPath}`);
      } catch (e) {
        // If the module could not be imported, set null in the module cache
        // so that the root MessageHandler can forward the message to the next
        // layer.
        this._modules.set(moduleFullPath, null);
        logger.trace(`No module ${moduleName} found for ${moduleFullPath}`);
      }
    }

    return this._modules.get(moduleFullPath);
  }

  toString() {
    return `[object ${this.constructor.name} ${this.messageHandler.key}]`;
  }

  _getModuleFullPath(moduleName, destination) {
    let moduleFolder;
    if (this.messageHandler.constructor.type === destination.type) {
      // If the command is targeting the current type, the module is expected to
      // be in eg "windowglobal/${moduleName}.jsm".
      moduleFolder = this._messageHandlerPath;
    } else {
      // If the command is targeting another type, the module is expected to
      // be in a composed folder eg "windowglobal-in-root/${moduleName}.jsm".
      const MessageHandlerClass = getMessageHandlerClass(destination.type);
      const destinationPath = MessageHandlerClass.modulePath;
      moduleFolder = `${destinationPath}-in-${this._messageHandlerPath}`;
    }

    return `${this._modulesRootFolder}/${moduleFolder}/${moduleName}.jsm`;
  }
}
