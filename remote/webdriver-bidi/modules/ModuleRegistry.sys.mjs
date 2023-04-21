/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const modules = {
  root: {},
  "windowglobal-in-root": {},
  windowglobal: {},
};

// eslint-disable-next-line mozilla/lazy-getter-object-name
ChromeUtils.defineESModuleGetters(modules.root, {
  browsingContext:
    "chrome://remote/content/webdriver-bidi/modules/root/browsingContext.sys.mjs",
  input: "chrome://remote/content/webdriver-bidi/modules/root/input.sys.mjs",
  log: "chrome://remote/content/webdriver-bidi/modules/root/log.sys.mjs",
  network:
    "chrome://remote/content/webdriver-bidi/modules/root/network.sys.mjs",
  script: "chrome://remote/content/webdriver-bidi/modules/root/script.sys.mjs",
  session:
    "chrome://remote/content/webdriver-bidi/modules/root/session.sys.mjs",
});

// eslint-disable-next-line mozilla/lazy-getter-object-name
ChromeUtils.defineESModuleGetters(modules["windowglobal-in-root"], {
  browsingContext:
    "chrome://remote/content/webdriver-bidi/modules/windowglobal-in-root/browsingContext.sys.mjs",
  log:
    "chrome://remote/content/webdriver-bidi/modules/windowglobal-in-root/log.sys.mjs",
  script:
    "chrome://remote/content/webdriver-bidi/modules/windowglobal-in-root/script.sys.mjs",
});

// eslint-disable-next-line mozilla/lazy-getter-object-name
ChromeUtils.defineESModuleGetters(modules.windowglobal, {
  browsingContext:
    "chrome://remote/content/webdriver-bidi/modules/windowglobal/browsingContext.sys.mjs",
  input:
    "chrome://remote/content/webdriver-bidi/modules/windowglobal/input.sys.mjs",
  log:
    "chrome://remote/content/webdriver-bidi/modules/windowglobal/log.sys.mjs",
  script:
    "chrome://remote/content/webdriver-bidi/modules/windowglobal/script.sys.mjs",
});

/**
 * Retrieve the WebDriver BiDi module class matching the provided module name
 * and folder.
 *
 * @param {string} moduleName
 *     The name of the module to get the class for.
 * @param {string} moduleFolder
 *     A valid folder name for modules.
 * @returns {Class=}
 *     The class corresponding to the module name and folder, null if no match
 *     was found.
 * @throws {Error}
 *     If the provided module folder is unexpected.
 */
export const getModuleClass = function(moduleName, moduleFolder) {
  if (!modules[moduleFolder]) {
    throw new Error(
      `Invalid module folder "${moduleFolder}", expected one of "${Object.keys(
        modules
      )}"`
    );
  }

  if (!modules[moduleFolder][moduleName]) {
    return null;
  }

  return modules[moduleFolder][moduleName];
};
