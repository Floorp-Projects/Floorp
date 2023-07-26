/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  getMessageHandlerClass:
    "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  RootMessageHandler:
    "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs",
});

const protocols = {
  bidi: {},
  test: {},
};
// eslint-disable-next-line mozilla/lazy-getter-object-name
ChromeUtils.defineESModuleGetters(protocols.bidi, {
  // Additional protocols might use a different registry for their modules,
  // in which case this will no longer be a constant but will instead depend on
  // the protocol owning the MessageHandler. See Bug 1722464.
  modules:
    "chrome://remote/content/webdriver-bidi/modules/ModuleRegistry.sys.mjs",
});
// eslint-disable-next-line mozilla/lazy-getter-object-name
ChromeUtils.defineESModuleGetters(protocols.test, {
  modules:
    "chrome://mochitests/content/browser/remote/shared/messagehandler/test/browser/resources/modules/ModuleRegistry.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

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
 * - ${MODULES_FOLDER}/root/{ModuleName}.sys.mjs contains the implementation for
 *   commands intended for the destination ROOT, and will be created for a ROOT
 *   MessageHandler only. Typically, they will run in the parent process.
 * - ${MODULES_FOLDER}/windowglobal/{ModuleName}.sys.mjs contains the implementation
 *   for commands intended for a WINDOW_GLOBAL destination, and will be created
 *   for a WINDOW_GLOBAL MessageHandler only. Those will usually run in a
 *   content process.
 * - ${MODULES_FOLDER}/windowglobal-in-root/{ModuleName}.sys.mjs also handles
 *   commands intended for a WINDOW_GLOBAL destination, but they will be created
 *   for the ROOT MessageHandler and will run in the parent process. This can be
 *   useful if some code has to be executed in the parent process, even though
 *   the final destination is a WINDOW_GLOBAL.
 * - And so on, as more MessageHandler types get added, more combinations will
 *   follow based on the same pattern:
 *   - {contextName}/{ModuleName}.sys.mjs
 *   - or {destinationType}-in-{currentType}/{ModuleName}.sys.mjs
 *
 * All those implementations are optional. If a module cannot be found, based on
 * the logic detailed above, the MessageHandler will assume that the command
 * should simply be forwarded to the next layer of the network.
 */
export class ModuleCache {
  #messageHandler;
  #messageHandlerType;
  #modules;
  #protocol;

  /*
   * @param {MessageHandler} messageHandler
   *     The MessageHandler instance which owns this ModuleCache instance.
   */
  constructor(messageHandler) {
    this.#messageHandler = messageHandler;
    this.#messageHandlerType = messageHandler.constructor.type;

    // Map of absolute module paths to module instances.
    this.#modules = new Map();

    // Use the module class from the WebDriverBiDi ModuleRegistry if we
    // are not using test modules.
    this.#protocol = Services.prefs.getBoolPref(
      "remote.messagehandler.modulecache.useBrowserTestRoot",
      false
    )
      ? protocols.test
      : protocols.bidi;
  }

  /**
   * Destroy all instantiated modules.
   */
  destroy() {
    this.#modules.forEach(module => module?.destroy());
  }

  /**
   * Retrieve all module classes matching the provided module name to reach the
   * provided destination, from the current context.
   *
   * This corresponds to the path a command can take to reach its destination.
   * A command's method must be implemented in one of the classes returned by
   * getAllModuleClasses in order to be successfully handled.
   *
   * @param {string} moduleName
   *     The name of the module.
   * @param {Destination} destination
   *     The destination.
   * @returns {Array<class<Module>|null>}
   *     An array of Module classes.
   */
  getAllModuleClasses(moduleName, destination) {
    const destinationType = destination.type;
    const classes = [
      this.#getModuleClass(
        moduleName,
        this.#messageHandlerType,
        destinationType
      ),
    ];

    // Bug 1733242: Extend the implementation of this method to handle workers.
    // It assumes layers have at most one level of nesting, for instance
    // "root -> windowglobal", but it wouldn't work for something such as
    // "root -> windowglobal -> worker".
    if (destinationType !== this.#messageHandlerType) {
      classes.push(
        this.#getModuleClass(moduleName, destinationType, destinationType)
      );
    }

    return classes.filter(cls => !!cls);
  }

  /**
   * Get a module instance corresponding to the provided moduleName and
   * destination. If no existing module can be found in the cache, ModuleCache
   * will attempt to import the module file and create a new instance, which
   * will then be cached and returned for subsequent calls.
   *
   * @param {string} moduleName
   *     The name of the module which should implement the command.
   * @param {CommandDestination} destination
   *     The destination of the command for which we need to instantiate a
   *     module. See MessageHandler.sys.mjs for the CommandDestination typedef.
   * @returns {object=}
   *     A module instance corresponding to the provided moduleName and
   *     destination, or null if it could not be instantiated.
   */
  getModuleInstance(moduleName, destination) {
    const key = `${moduleName}-${destination.type}`;

    if (this.#modules.has(key)) {
      // If there is already a cached instance (potentially null) for the
      // module name + destination type pair, return it.
      return this.#modules.get(key);
    }

    const ModuleClass = this.#getModuleClass(
      moduleName,
      this.#messageHandlerType,
      destination.type
    );

    let module = null;
    if (ModuleClass) {
      module = new ModuleClass(this.#messageHandler);
    }

    this.#modules.set(key, module);
    return module;
  }

  /**
   * Check if the given module exists for the destination.
   *
   * @param {string} moduleName
   *     The name of the module.
   * @param {Destination} destination
   *     The destination.
   * @returns {boolean}
   *     True if the module exists.
   */
  hasModuleClass(moduleName, destination) {
    const classes = this.getAllModuleClasses(moduleName, destination);
    return !!classes.length;
  }

  toString() {
    return `[object ${this.constructor.name} ${this.#messageHandler.name}]`;
  }

  /**
   * Retrieve the module class matching the provided module name and folder.
   *
   * @param {string} moduleName
   *     The name of the module to get the class for.
   * @param {string} originType
   *     The MessageHandler type from where the command comes.
   * @param {string} destinationType
   *     The MessageHandler type where the command should go to.
   * @returns {Class=}
   *     The class corresponding to the module name and folder, null if no match
   *     was found.
   * @throws {Error}
   *     If the provided module folder is unexpected.
   */
  #getModuleClass = function (moduleName, originType, destinationType) {
    if (
      destinationType === lazy.RootMessageHandler.type &&
      originType !== destinationType
    ) {
      // If we are trying to reach the root layer from a lower layer, no module
      // class should attempt to handle the command in the current layer and
      // the command should be forwarded unconditionally.
      return null;
    }

    const moduleFolder = this.#getModuleFolder(originType, destinationType);
    if (!this.#protocol.modules[moduleFolder]) {
      throw new Error(
        `Invalid module folder "${moduleFolder}", expected one of "${Object.keys(
          this.#protocol.modules
        )}"`
      );
    }

    let moduleClass = null;
    if (this.#protocol.modules[moduleFolder][moduleName]) {
      moduleClass = this.#protocol.modules[moduleFolder][moduleName];
    }

    if (moduleClass) {
      lazy.logger.trace(
        `Module ${moduleFolder}/${moduleName}.sys.mjs found for ${destinationType}`
      );
    } else {
      lazy.logger.trace(
        `Module ${moduleFolder}/${moduleName}.sys.mjs not found for ${destinationType}`
      );
    }

    return moduleClass;
  };

  #getModuleFolder(originType, destinationType) {
    const originPath = lazy.getMessageHandlerClass(originType).modulePath;
    if (originType === destinationType) {
      // If the command is targeting the current type, the module is expected to
      // be in eg "windowglobal/${moduleName}.sys.mjs".
      return originPath;
    }

    // If the command is targeting another type, the module is expected to
    // be in a composed folder eg "windowglobal-in-root/${moduleName}.sys.mjs".
    const destinationPath =
      lazy.getMessageHandlerClass(destinationType).modulePath;
    return `${destinationPath}-in-${originPath}`;
  }
}
