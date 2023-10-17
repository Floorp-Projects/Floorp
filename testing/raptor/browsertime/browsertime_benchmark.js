/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

module.exports = async function (context, commands) {
  context.log.info("Starting a browsertime benchamrk");
  let url = context.options.browsertime.url;
  let page_cycles = context.options.browsertime.page_cycles;
  let page_cycle_delay = context.options.browsertime.page_cycle_delay;
  let post_startup_delay = context.options.browsertime.post_startup_delay;
  let page_timeout = context.options.timeouts.pageLoad;
  let ret = false;
  let expose_profiler = context.options.browsertime.expose_profiler;

  context.log.info(
    "Waiting for %d ms (post_startup_delay)",
    post_startup_delay
  );
  await commands.wait.byTime(post_startup_delay);

  for (let count = 0; count < page_cycles; count++) {
    context.log.info("Navigating to about:blank");
    await commands.navigate("about:blank");

    context.log.info("Cycle %d, waiting for %d ms", count, page_cycle_delay);
    await commands.wait.byTime(page_cycle_delay);

    context.log.info("Cycle %d, starting the measure", count);
    if (expose_profiler === "true") {
      context.log.info("Custom profiler start!");
      if (context.options.browser === "firefox") {
        await commands.profiler.start();
      } else if (context.options.browser === "chrome") {
        await commands.trace.start();
      }
    }
    await commands.measure.start(url);

    context.log.info("Benchmark custom metric collection");

    let data = null;
    let starttime = await commands.js.run(`return performance.now();`);
    while (
      data == null &&
      (await commands.js.run(`return performance.now();`)) - starttime <
        page_timeout
    ) {
      let wait_time = 3000;
      context.log.info("Waiting %d ms for data from benchmark...", wait_time);
      await commands.wait.byTime(wait_time);
      data = await commands.js.run(
        "return window.sessionStorage.getItem('benchmark_results');"
      );
    }
    if (expose_profiler === "true") {
      context.log.info("Custom profiler stop!");
      if (context.options.browser === "firefox") {
        await commands.profiler.stop();
      } else if (context.options.browser === "chrome") {
        await commands.trace.stop();
      }
    }
    if (
      data == null &&
      (await commands.js.run(`return performance.now();`)) - starttime >=
        page_timeout
    ) {
      ret = false;
      context.log.error("Benchmark timed out. Aborting...");
    } else if (data) {
      // Reset benchmark results
      await commands.js.run(
        "return window.sessionStorage.removeItem('benchmark_results');"
      );

      context.log.info("Value of benchmark data: ", data);
      data = JSON.parse(data);

      if (!Array.isArray(data)) {
        commands.measure.addObject({ browsertime_benchmark: data });
      } else {
        commands.measure.addObject({
          browsertime_benchmark: {
            [data[1]]: data.slice(2),
          },
        });
      }
      ret = true;
    } else {
      context.log.error("No data collected from benchmark.");
      ret = false;
    }
  }
  context.log.info("Browsertime benchmark ended.");
  return ret;
};
