/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["getModuleClass"];

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
  const path = `chrome://mochitests/content/browser/remote/shared/messagehandler/test/browser/resources/modules/${moduleFolder}/${moduleName}.jsm`;
  try {
    return ChromeUtils.import(path)[moduleName];
  } catch (e) {
    if (e.result == Cr.NS_ERROR_FILE_NOT_FOUND) {
      return null;
    }
    throw e;
  }
};
