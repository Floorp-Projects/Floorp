/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env xpcshell */
/* globals print, quit, arguments */
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { RecipeRunner } = ChromeUtils.import(
  "resource://normandy/lib/RecipeRunner.jsm"
);

if (arguments.length !== 1) {
  print("Usage: capabilities-script.js OUTFILE");
  quit(1);
}

main(...arguments);

async function main(outPath) {
  const capabililitySet = RecipeRunner.getCapabilities();
  await OS.File.writeAtomic(
    outPath,
    JSON.stringify(
      {
        capabilities: Array.from(capabililitySet),
      },
      null,
      4
    )
  );
}
