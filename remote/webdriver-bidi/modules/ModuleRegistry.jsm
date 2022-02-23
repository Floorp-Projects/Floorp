/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["getModuleClass"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const modules = {
  root: {},
  "windowglobal-in-root": {},
  windowglobal: {},
};

XPCOMUtils.defineLazyModuleGetters(modules.root, {
  log: "chrome://remote/content/webdriver-bidi/modules/root/log.jsm",
  session: "chrome://remote/content/webdriver-bidi/modules/root/session.jsm",
});

XPCOMUtils.defineLazyModuleGetters(modules["windowglobal-in-root"], {
  log:
    "chrome://remote/content/webdriver-bidi/modules/windowglobal-in-root/log.jsm",
});

XPCOMUtils.defineLazyModuleGetters(modules.windowglobal, {
  log: "chrome://remote/content/webdriver-bidi/modules/windowglobal/log.jsm",
});

/**
 * Retrieve the WebDriver BiDi module class matching the provided module name
 * and folder.
 *
 * @param {String} moduleName
 *     The name of the module to get the class for.
 * @param {String} moduleFolder
 *     A valid folder name for modules.
 * @return {Class=}
 *     The class corresponding to the module name and folder, null if no match
 *     was found.
 * @throws {Error}
 *     If the provided module folder is unexpected.
 **/
const getModuleClass = function(moduleName, moduleFolder) {
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
