/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env xpcshell */
/* globals print, quit, arguments */
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/services.jsm");
const { RecipeRunner } = ChromeUtils.import(
  "resource://normandy/lib/RecipeRunner.jsm"
);

if (arguments.length !== 1) {
  print("Usage: capabilities-script.js OUTFILE");
  quit(1);
}

main(...arguments);

function resolvePath(path) {
  let absPath = path;

  if (!PathUtils.isAbsolute(path)) {
    const components = path.split(new RegExp("[/\\\\]")).filter(c => c.length);
    absPath = Services.dirsvc.get("CurWorkD", Ci.nsIFile).path;

    for (const component of components) {
      switch (component) {
        case ".":
          break;

        case "..":
          absPath = PathUtils.parent(absPath);
          break;

        default:
          absPath = PathUtils.join(absPath, component);
      }
    }
  }

  return absPath;
}

async function main(outPath) {
  const capabililitySet = RecipeRunner.getCapabilities();
  await IOUtils.writeUTF8(
    resolvePath(outPath),
    JSON.stringify(
      {
        capabilities: Array.from(capabililitySet),
      },
      null,
      4
    )
  );
}
