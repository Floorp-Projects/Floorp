/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

module.exports = async function(context, commands) {
  context.log.info("Starting a browsertime pageload");
  let test_url = context.options.browsertime.url;
  let secondary_url = context.options.browsertime.secondary_url;
  let page_cycles = context.options.browsertime.page_cycles;
  let page_cycle_delay = context.options.browsertime.page_cycle_delay;
  let post_startup_delay = context.options.browsertime.post_startup_delay;

  context.log.info(
    "Waiting for %d ms (post_startup_delay)",
    post_startup_delay
  );
  await commands.wait.byTime(post_startup_delay);

  for (let count = 0; count < page_cycles; count++) {
    if (count !== 0 && secondary_url !== undefined) {
      context.log.info("Navigating to secondary url:" + secondary_url);
      await commands.navigate(secondary_url);
    } else {
      context.log.info("Navigating to about:blank");
      await commands.navigate("about:blank");
    }

    context.log.info("Navigating to primary url:" + test_url);
    context.log.info("Cycle %d, waiting for %d ms", count, page_cycle_delay);
    await commands.wait.byTime(page_cycle_delay);

    context.log.info("Cycle %d, starting the measure", count);
    await commands.measure.start(test_url);
  }
  context.log.info("Browsertime pageload ended.");
  return true;
};
