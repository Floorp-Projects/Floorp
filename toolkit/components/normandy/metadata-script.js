/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env xpcshell */
/* globals print, quit, arguments */
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

  if (Services.appinfo.OS === "WINNT") {
    absPath = path = path.replace(/\//g, "\\");
  }

  if (!PathUtils.isAbsolute(path)) {
    absPath = Services.dirsvc.get("CurWorkD", Ci.nsIFile).path;

    const components = PathUtils.splitRelative(path, {
      allowEmpty: true,
      allowParentDir: true,
      allowCurrentDir: true,
    }).filter(c => c.length);

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
