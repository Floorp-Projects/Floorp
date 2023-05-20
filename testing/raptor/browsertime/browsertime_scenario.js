/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

module.exports = async function (context, commands) {
  context.log.info("Starting a browsertime scenario test");
  let page_cycles = context.options.browsertime.page_cycles;
  let page_cycle_delay = context.options.browsertime.page_cycle_delay;
  let post_startup_delay = context.options.browsertime.post_startup_delay;
  let scenario_time = context.options.browsertime.scenario_time;
  let background_app = context.options.browsertime.background_app == "true";
  let app = null;

  if (background_app) {
    if (!context.options.android) {
      throw new Error("Cannot background an application on desktop");
    }
    app = context.options.firefox.android.package;
  }

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

    if (background_app) {
      // Background the application and disable doze for it
      await commands.android.shell(`dumpsys deviceidle whitelist +${app}`);
      await commands.android.shell(`input keyevent 3`);
      await commands.wait.byTime(1000);
      const foreground = await commands.android.shell(
        "dumpsys window windows | grep mCurrentFocus"
      );
      if (foreground.includes(app)) {
        throw new Error("Application was not backgrounded successfully");
      } else {
        context.log.info("Application was backgrounded successfully");
      }
    }

    // Run the test
    await commands.measure.start("about:blank test");
    context.log.info("Waiting for %d ms for this scenario", scenario_time);
    await commands.wait.byTime(scenario_time);
    await commands.measure.stop();

    if (background_app) {
      // Foreground the application and enable doze again
      await commands.android.shell(`am start --activity-single-top ${app}`);
      await commands.android.shell(`dumpsys deviceidle enable`);
    }
  }
  context.log.info("Browsertime scenario test ended.");
  return true;
};
