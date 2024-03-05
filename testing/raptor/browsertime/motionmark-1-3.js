/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

// index for the CSS selector in developer.html
const suiteSelectorNumber = {
  MotionMark: 1,
  "HTML suite": 2,
};

module.exports = async function (context, commands) {
  context.log.info("Starting MotionMark 1.3 test");
  let url = context.options.browsertime.url;
  let page_cycles = context.options.browsertime.page_cycles;
  let suite_name = context.options.browsertime.suite_name;
  let page_cycle_delay = context.options.browsertime.page_cycle_delay;
  let post_startup_delay = context.options.browsertime.post_startup_delay;
  let page_timeout = context.options.timeouts.pageLoad;
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

    let suite_selector = `#suites > ul > li:nth-child(${suiteSelectorNumber[suite_name]}) > label > input[type="checkbox"]`;

    await commands.mouse.singleClick.bySelector(suite_selector);
    await commands.js.runAndWait(`
      this.benchmarkController.startBenchmark()
    `);

    let data_exists = null;
    let starttime = await commands.js.run(`return performance.now();`);
    while (
      (data_exists == null || !Object.keys(data_exists).length) &&
      (await commands.js.run(`return performance.now();`)) - starttime <
        page_timeout
    ) {
      let wait_time = 3000;
      context.log.info(
        "Waiting %d ms for data from %s...",
        wait_time,
        suite_name
      );
      await commands.wait.byTime(wait_time);

      data_exists = await commands.js.run(`
        return window.benchmarkRunnerClient.results.data
      `);
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
      !data_exists &&
      (await commands.js.run(`return performance.now();`)) - starttime >=
        page_timeout
    ) {
      context.log.error("Benchmark timed out. Aborting...");
      return false;
    }

    let data = null;
    data = await commands.js.run(`
      const score = window.benchmarkRunnerClient.results.score;
      const results = window.benchmarkRunnerClient.results.results[0].testsResults;
      return {
        score,
        results,
      };
    `);
    data.suite_name = suite_name;

    commands.measure.addObject({ mm_res: data });
    context.log.info("Value of summarized benchmark data: ", data);
  }

  return true;
};
