// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
/* eslint-env node */

/*
Ensure the `--firefox.preference=network.http.http3.enable:true` is
set for this test.
*/

async function getNumLoaded(commands) {
  return commands.js.run(`
    let sum = 0;
    document.querySelectorAll("#imgContainer img").forEach(e => {
      sum += e.complete & e.naturalHeight != 0;
    });
    return sum;
  `);
}

async function waitForImgLoadEnd(
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
    newCount = await getNumLoaded(commands);
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
  let rootUrl = "https://lucaspardue.com/quictilesfetch.html";
  let cycles = 5;

  if (
    (typeof context.options.browsertime !== "undefined") &
    (typeof context.options.browsertime.cycles !== "undefined")
  ) {
    cycles = context.options.browsertime.cycles;
  }

  // Make firefox learn of HTTP/3 server
  // XXX: Need to build an HTTP/3-specific conditioned profile
  // to handle these pre-navigations.
  await commands.navigate(rootUrl);

  let combos = [
    [100, 1],
    [100, 100],
    [300, 300],
  ];
  for (let cycle = 0; cycle < cycles; cycle++) {
    for (let combo = 0; combo < combos.length; combo++) {
      await commands.measure.start("pageload");
      await commands.navigate(rootUrl);
      await commands.measure.stop();
      let last = commands.measure.result.length - 1;
      commands.measure.result[
        last
      ].browserScripts.pageinfo.url = `LucasQUIC (r=${combos[combo][0]}, p=${combos[combo][1]})`;

      // Set the input fields
      await commands.js.runAndWait(`
        document.querySelector("#maxReq").setAttribute(
          "value",
          ${combos[combo][0]}
        )
      `);
      await commands.js.runAndWait(`
        document.querySelector("#reqGroup").setAttribute(
          "value",
          ${combos[combo][1]}
        )
      `);

      // Start the test and wait for the images to finish loading
      commands.click.byJs(`document.querySelector("button")`);
      let results = await waitForImgLoadEnd(0, 40, 120000, commands, context);

      commands.measure.result[last].browserScripts.pageinfo.resourceLoadTime =
        results.end - results.start;
      commands.measure.result[last].browserScripts.pageinfo.imagesLoaded =
        results.numResources;
      commands.measure.result[last].browserScripts.pageinfo.imagesMissed =
        combos[combo][0] - results.numResources;
    }
  }
}

module.exports = {
  test,
  owner: "Network Team",
  name: "lq-fetch",
  component: "netwerk",
  description: "Measures the amount of time it takes to load a set of images.",
};
