// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
/* eslint-env node */

/*
Ensure the `--firefox.preference=network.http.http3.enable:true` is
set for this test.
*/

async function captureNetworkRequest(commands) {
  var capture_network_request = [];
  var capture_resource = await commands.js.run(`
    return performance.getEntriesByType("resource");
  `);
  for (var i = 0; i < capture_resource.length; i++) {
    capture_network_request.push(capture_resource[i].name);
  }
  return capture_network_request;
}

async function waitForScrollRequestsEnd(
  prevCount,
  maxStableCount,
  timeout,
  commands,
  context
) {
  let starttime = await commands.js.run(`return performance.now();`);
  let endtime = await commands.js.run(`return performance.now();`);
  let changing = true;
  let newCount = -1;
  let stableCount = 0;

  while (
    ((await commands.js.run(`return performance.now();`)) - starttime <
      timeout) &
    changing
  ) {
    // Wait a bit before making another round
    await commands.wait.byTime(100);
    newCount = (await captureNetworkRequest(commands)).length;
    context.log.debug(`${newCount}, ${prevCount}, ${stableCount}`);

    // Check if we are approaching stability
    if (newCount == prevCount) {
      // Gather the end time now
      if (stableCount == 0) {
        endtime = await commands.js.run(`return performance.now();`);
      }
      stableCount++;
    } else {
      prevCount = newCount;
      stableCount = 0;
    }

    if (stableCount >= maxStableCount) {
      // Stability achieved
      changing = false;
    }
  }

  return {
    start: starttime,
    end: endtime,
    numResources: newCount,
  };
}

async function test(context, commands) {
  let rootUrl = "https://www.facebook.com/lambofgod/";
  let waitTime = 1000;
  let numScrolls = 5;

  const average = arr => arr.reduce((p, c) => p + c, 0) / arr.length;

  if (typeof context.options.browsertime !== "undefined") {
    if (typeof context.options.browsertime.waitTime !== "undefined") {
      waitTime = context.options.browsertime.waitTime;
    }
    if (typeof context.options.browsertime.numScrolls !== "undefined") {
      numScrolls = context.options.browsertime.numScrolls;
    }
  }

  // Make firefox learn of HTTP/3 server
  await commands.navigate(rootUrl);

  let cycles = 1;
  for (let cycle = 0; cycle < cycles; cycle++) {
    // Measure the pageload
    await commands.measure.start("pageload");
    await commands.navigate(rootUrl);
    await commands.measure.stop();

    // Initial scroll to make the new user popup show
    await commands.js.runAndWait(
      `window.scrollTo({ top: 1000, behavior: 'smooth' })`
    );
    await commands.wait.byTime(1000);
    await commands.click.byLinkTextAndWait("Not Now");

    let vals = [];
    let badIterations = 0;
    for (let iteration = 0; iteration < numScrolls; iteration++) {
      // Clear old resources
      await commands.js.run(`performance.clearResourceTimings();`);

      // Get current resource count
      let currCount = (await captureNetworkRequest(commands)).length;

      // Scroll to a ridiculously high value for "infinite" down-scrolling
      commands.js.runAndWait(`
        window.scrollTo({ top: 100000000 })
      `);

      /*
      The maxStableCount of 22 was chosen as a trade-off between fast iterations
      and minimizing the number of bad iterations.
      */
      let newInfo = await waitForScrollRequestsEnd(
        currCount,
        22,
        120000,
        commands,
        context
      );

      // Gather metrics
      let ndiff = newInfo.numResources - currCount;
      let tdiff = (newInfo.end - newInfo.start) / 1000;

      // Check if we had a bad iteration
      if (ndiff == 0) {
        context.log.info("Bad iteration, redoing...");
        iteration--;
        badIterations++;
        if (badIterations == 5) {
          throw new Error("Too many bad scroll iterations occurred");
        }
        continue;
      }

      vals.push(ndiff / tdiff);

      // Wait X seconds before scrolling again
      await commands.wait.byTime(waitTime);
    }

    if (!vals.length) {
      throw new Error("No requestsPerSecond values were obtained");
    }

    commands.measure.result[0].browserScripts.pageinfo.requestsPerSecond = average(
      vals
    );
  }
}

module.exports = {
  test,
  owner: "Network Team",
  component: "netwerk",
  name: "facebook-scroll",
  description: "Measures the number of requests per second after a scroll.",
};
