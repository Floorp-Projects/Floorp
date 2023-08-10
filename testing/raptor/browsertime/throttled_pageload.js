/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

const fs = require("fs");
const path = require("path");

module.exports = async function (context, commands) {
  context.log.info(
    "Starting a pageload for which we will first enable the dev tools network throttler"
  );

  const url = "https://en.wikipedia.org/wiki/Barack_Obama";

  await commands.navigate("about:blank");
  await commands.wait.byTime(1000);

  // Load the throttling script
  let throttler_script = null;
  let throttlerScriptPath = path.join(
    `${path.resolve(__dirname)}`,
    "utils",
    "NetworkThrottlingUtils.js"
  );
  try {
    throttler_script = fs.readFileSync(throttlerScriptPath, "utf8");
  } catch (error) {
    throw Error(
      `Failed to load network throttler script ${throttlerScriptPath}`
    );
  }

  // Create a throttler and configure the network
  let usage = `
    let networkThrottler = new NetworkThrottler();
    let throttleData = {
      latencyMean: 50,
      latencyMax: 50,
      downloadBPSMean: 250000,
      downloadBPSMax: 250000,
      uploadBPSMean: 250000,
      uploadBPSMax: 250000
    };
    networkThrottler.start(throttleData);
  `;

  throttler_script += usage;
  commands.js.runPrivileged(throttler_script);

  await commands.measure.start(url);

  context.log.info("Throttled pageload test finished.");
  return true;
};
