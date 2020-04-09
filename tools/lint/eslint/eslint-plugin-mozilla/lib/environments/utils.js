/**
 * @fileoverview Provides utilities for setting up environments.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

var path = require("path");
var helpers = require("../helpers");
var globals = require("../globals");

/**
 * Obtains the globals for a list of files.
 *
 * @param {Array.<String>} files
 *   The array of files to get globals for. The paths are relative to the topsrcdir.
 * @returns {Object}
 *   Returns an object with keys of the global names and values of if they are
 *   writable or not.
 */
function getGlobalsForScripts(environmentName, files, extraDefinitions) {
  let fileGlobals = extraDefinitions;
  const root = helpers.rootDir;
  for (const file of files) {
    const fileName = path.join(root, file);
    try {
      fileGlobals = fileGlobals.concat(globals.getGlobalsForFile(fileName));
    } catch (e) {
      console.error(`Could not load globals from file ${fileName}: ${e}`);
      console.error(
        `You may need to update the mappings for the ${environmentName} environment`
      );
      throw new Error(`Could not load globals from file ${fileName}: ${e}`);
    }
  }

  var globalObjects = {};
  for (let global of fileGlobals) {
    globalObjects[global.name] = global.writable;
  }
  return globalObjects;
}

module.exports = {
  getScriptGlobals(
    environmentName,
    files,
    extraDefinitions = [],
    extraEnv = {}
  ) {
    if (helpers.isMozillaCentralBased()) {
      return {
        globals: getGlobalsForScripts(environmentName, files, extraDefinitions),
        ...extraEnv,
      };
    }
    return helpers.getSavedEnvironmentItems(environmentName);
  },
};
