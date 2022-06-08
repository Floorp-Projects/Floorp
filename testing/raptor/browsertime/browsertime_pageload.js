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
  let chimera_mode = context.options.browsertime.chimera;

  context.log.info(
    "Waiting for %d ms (post_startup_delay)",
    post_startup_delay
  );
  await commands.wait.byTime(post_startup_delay);
  let cached = false;

  for (let count = 0; count < page_cycles; count++) {
    if (count !== 0 && secondary_url !== undefined) {
      context.log.info("Navigating to secondary url:" + secondary_url);
      await commands.navigate(secondary_url);
      await commands.wait.byTime(1000);
      await commands.js.runAndWait(`
      (function() {
        const white = document.createElement('div');
        white.id = 'raptor-white';
        white.style.position = 'absolute';
        white.style.top = '0';
        white.style.left = '0';
        white.style.width = Math.max(document.documentElement.clientWidth, document.body.clientWidth) + 'px';
        white.style.height = Math.max(document.documentElement.clientHeight,document.body.clientHeight) + 'px';
        white.style.backgroundColor = 'white';
        white.style.zIndex = '2147483647';
        document.body.appendChild(white);
        document.body.style.display = '';
      })();`);
      await commands.wait.byTime(1000);
    } else {
      context.log.info("Navigating to about:blank, count: " + count);
      await commands.navigate("about:blank");
    }

    context.log.info("Navigating to primary url:" + test_url);
    context.log.info("Cycle %d, waiting for %d ms", count, page_cycle_delay);
    await commands.wait.byTime(page_cycle_delay);

    context.log.info("Cycle %d, starting the measure", count);
    await commands.measure.start(test_url);

    // Wait 20 seconds to populate bytecode cache
    if (chimera_mode && !cached) {
      await commands.wait.byTime(20000);
      cached = true;
    }
  }

  context.log.info("Browsertime pageload ended.");
  return true;
};
