/**
 * @fileoverview A script to export the known globals to a file.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

var fs = require("fs");
var path = require("path");
var helpers = require("../lib/helpers");

const eslintDir = path.join(helpers.rootDir,
                            "tools", "lint", "eslint");

const globalsFile = path.join(eslintDir, "eslint-plugin-mozilla",
                              "lib", "environments", "saved-globals.json");

console.log("Copying modules.json");

const modulesFile = path.join(eslintDir, "modules.json");
const shipModulesFile = path.join(eslintDir, "eslint-plugin-mozilla", "lib",
                                  "modules.json");

fs.writeFileSync(shipModulesFile, fs.readFileSync(modulesFile));

console.log("Generating globals file");

// We only export the configs section.
let environmentGlobals = require("../lib/index.js").environments;

return fs.writeFile(globalsFile, JSON.stringify({environments: environmentGlobals}), err => {
  if (err) {
    console.error(err);
    process.exit(1);
  }

  console.log("Globals file generation complete");
});
