// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
/* eslint-env node */

/*
Ensure the `--firefox.preference=network.http.http3.enable:true` is
set for this test.
*/

async function getNumImagesLoaded(elementSelector, commands) {
  return commands.js.run(`
    let sum = 0;
    document.querySelectorAll(${elementSelector}).forEach(e => {
      sum += e.complete & e.naturalHeight != 0;
    });
    return sum;
  `);
}

async function waitForImgLoadEnd(
  prevCount,
  maxStableCount,
  iterationDelay,
  timeout,
  commands,
  context,
  counter,
  elementSelector
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
    await commands.wait.byTime(iterationDelay);
    newCount = await counter(elementSelector, commands);
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
  let rootUrl = "https://www.google.com/search?q=kittens&tbm=isch";
  let waitTime = 1000;
  let numScrolls = 10;

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

    async function getHeight() {
      return commands.js.run(`return document.body.scrollHeight;`);
    }

    let vals = [];
    let badIterations = 0;
    let prevHeight = 0;
    for (let iteration = 0; iteration < numScrolls; iteration++) {
      // Get current image count
      let currCount = await getNumImagesLoaded(`".isv-r img"`, commands);
      prevHeight = await getHeight();

      // Scroll to a ridiculously high value for "infinite" down-scrolling
      commands.js.runAndWait(`
        window.scrollTo({ top: 100000000 })
      `);

      /*
      The maxStableCount of 22 was chosen as a trade-off between fast iterations
      and minimizing the number of bad iterations.
      */
      let results = await waitForImgLoadEnd(
        currCount,
        22,
        100,
        120000,
        commands,
        context,
        getNumImagesLoaded,
        `".isv-r img"`
      );

      // Gather metrics
      let ndiff = results.numResources - currCount;
      let tdiff = (results.end - results.start) / 1000;

      // Check if we had a bad iteration
      if (ndiff == 0) {
        // Check if the end of the search results was reached
        if (prevHeight == (await getHeight())) {
          context.log.info("Reached end of page.");
          break;
        }
        context.log.info("Bad iteration, redoing...");
        iteration--;
        badIterations++;
        if (badIterations == 5) {
          throw new Error("Too many bad scroll iterations occurred");
        }
        continue;
      }

      context.log.info(`${ndiff}, ${tdiff}`);
      vals.push(ndiff / tdiff);

      // Wait X seconds before scrolling again
      await commands.wait.byTime(waitTime);
    }

    if (!vals.length) {
      throw new Error("No requestsPerSecond values were obtained");
    }

    commands.measure.result[0].browserScripts.pageinfo.imagesPerSecond = average(
      vals
    );

    // Test clicking and and opening an image
    await commands.wait.byTime(waitTime);
    commands.click.byJs(`
      const links = document.querySelectorAll(".islib"); links[links.length-1]
    `);
    let results = await waitForImgLoadEnd(
      0,
      22,
      50,
      120000,
      commands,
      context,
      getNumImagesLoaded,
      `"#islsp img"`
    );
    commands.measure.result[0].browserScripts.pageinfo.imageLoadTime =
      results.end - results.start;
  }
}

module.exports = {
  test,
  owner: "Network Team",
  component: "netwerk",
  name: "g-image",
  description: "Measures the number of images per second after a scroll.",
};
