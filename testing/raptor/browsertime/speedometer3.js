/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

module.exports = async function (context, commands) {
  context.log.info("Starting Speedometer 3 test");
  let url = context.options.browsertime.url;
  let page_cycles = context.options.browsertime.page_cycles;
  let speedometer_iterations =
    context.options.browsertime.speedometer_iterations;
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
      await commands.profiler.start();
    }
    await commands.measure.start(url);

    await commands.js.runAndWait(`
        window.testDone = false;
        window.suiteValues = [];
        const benchmarkClient = {
          didRunSuites(measuredValues) {
            window.suiteValues.push(measuredValues);
          },
          didFinishLastIteration() {
            window.testDone = true;
          }
        };
        window.Suites.forEach((el) => {
          el.disabled = false;
        });
        // BenchmarkRunner is overriden as the InteractiveBenchmarkRunner
        const runner = new BenchmarkRunner(window.Suites, ${speedometer_iterations});
        runner._client = benchmarkClient;

        runner.runSuites();
    `);

    let data_exists = false;
    let starttime = await commands.js.run(`return performance.now();`);
    while (
      !data_exists &&
      (await commands.js.run(`return performance.now();`)) - starttime <
        page_timeout
    ) {
      let wait_time = 3000;
      context.log.info("Waiting %d ms for data from speedometer...", wait_time);
      await commands.wait.byTime(wait_time);
      data_exists = await commands.js.run("return window.testDone;");
    }
    if (expose_profiler === "true") {
      context.log.info("Custom profiler stop!");
      await commands.profiler.stop();
    }
    if (
      !data_exists &&
      (await commands.js.run(`return performance.now();`)) - starttime >=
        page_timeout
    ) {
      context.log.error("Benchmark timed out. Aborting...");
      return false;
    }
    let data = await commands.js.run(`return window.suiteValues;`);
    context.log.info("Value of benchmark data: ", data);

    commands.measure.addObject({ browsertime_benchmark: { s3: data } });
  }

  return true;
};
