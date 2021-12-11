/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

module.exports = async function(context, commands) {
  context.log.info("Starting a process switch test");
  let urlstr = context.options.browsertime.url;
  let page_cycles = context.options.browsertime.page_cycles;
  let page_cycle_delay = context.options.browsertime.page_cycle_delay;
  let post_startup_delay = context.options.browsertime.post_startup_delay;

  // Get the two urls to use in the test (the second one will be measured)
  let urls = urlstr.split(",");
  if (urls.length != 2) {
    context.log.error(
      `Wrong number of urls given. Expecting: 2, Given: ${urls.length}`
    );
    return false;
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

    await commands.navigate(urls[0]);
    await commands.wait.byTime(3000);
    await commands.measure.start(urls[1]);
    await commands.wait.byTime(2000);
  }

  context.log.info("Process switch test ended.");
  return true;
};
