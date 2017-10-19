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
const rulesFile = path.join(eslintDir, "eslint-plugin-mozilla",
                            "lib", "rules", "saved-rules-data.json");

console.log("Copying modules.json");

const modulesFile = path.join(eslintDir, "modules.json");
const shipModulesFile = path.join(eslintDir, "eslint-plugin-mozilla", "lib",
                                  "modules.json");

fs.writeFileSync(shipModulesFile, fs.readFileSync(modulesFile));

console.log("Generating globals file");

// Export the environments.
let environmentGlobals = require("../lib/index.js").environments;

return fs.writeFile(globalsFile, JSON.stringify({environments: environmentGlobals}), err => {
  if (err) {
    console.error(err);
    process.exit(1);
  }

  console.log("Globals file generation complete");

  console.log("Creating rules data file");
  // Also export data for the use-services.js rule
  let rulesData = {
    "use-services.js": require("../lib/rules/use-services.js")().getServicesInterfaceMap()
  };

  return fs.writeFile(rulesFile, JSON.stringify({rulesData}), err1 => {
    if (err1) {
      console.error(err1);
      process.exit(1);
    }

    console.log("Globals file generation complete");
  });
});
