/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const urls = [
  "https://accounts.google.com",
  "https://cnn.com/ampstories/us/why-hurricane-michael-is-a-monster-unlike-any-other",
  "https://stackoverflow.com/questions/927358/how-do-i-undo-the-most-recent-commits-in-git",
  "https://expedia.com/Hotel-Search?destination=New+York%2C+New+York&latLong=40.756680%2C-73.986470&regionId=178293&startDate=&endDate=&rooms=1&_xpid=11905%7C1&adults=2",
  "https://m.imdb.com/title/tt0083943/",
];

const idle_delay = 10000;

async function test(context, commands) {
  await commands.wait.byTime(idle_delay);

  await commands.measure.start();

  // Cold
  for (const url of urls) {
    await commands.navigate(url);
    await commands.wait.byTime(idle_delay);
  }

  await commands.navigate("about:blank");
  await commands.wait.byTime(idle_delay);

  // Warm
  for (const url of urls) {
    await commands.navigate(url);
    await commands.wait.byTime(idle_delay);
  }

  return commands.measure.stop();
}

module.exports = {
  test,
  owner: "Performance Team",
  name: "perfstats",
  description: "Collect perfstats for the given site",
  longDescription: `
  This test launches browsertime with the perfStats option (will collect low-overhead timings, see Bug 1553254).
  The test currently runs a short user journey. A selection of popular sites are visited, first as cold pageloads, and then as warm.
  `,
  usage: `
  ./mach perftest --hook testing/performance/hooks_perfstats.py \
  testing/performance/perftest_perfstats.js --browsertime-iterations 10 \
  --perfherder-metrics name:HttpChannelCompletion_Cache name:HttpChannelCompletion_Network
  `,
};
