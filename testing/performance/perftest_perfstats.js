/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const url = "https://www.google.com";

async function test(context, commands) {
  await commands.wait.byTime(5000);

  await commands.navigate(url);
  await commands.wait.byTime(5000);

  return commands.measure.start(url);
}

module.exports = {
  test,
  owner: "Performance Team",
  name: "perfstats",
  description: "Collect perfstats for the given site",
  longDescription: `
  This test launches browsertime with the perfStats option (will collect low-overhead timings, see Bug 1553254).
  The test currently runs a single site, google.com - the top-ranking global Alexa site.
  `,
  usage: `
  ./mach perftest --hook testing/performance/hooks_perfstats.py \
  testing/performance/perftest_perfstats.js --browsertime-iterations 10 \
  --perfherder-metrics name:HttpChannelCompletion_Cache name:HttpChannelCompletion_Network
  `,
};
